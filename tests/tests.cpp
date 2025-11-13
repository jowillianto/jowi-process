#include <jowi/test_lib.hpp>
#include <chrono>
#include <coroutine>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
import jowi.test_lib;
import jowi.process;
import jowi.io;
import jowi.asio;

namespace test_lib = jowi::test_lib;
namespace io = jowi::io;
namespace proc = jowi::process;
namespace asio = jowi::asio;

std::optional<io::LocalFile> null_file;

JOWI_SETUP(argc, argv) {
  test_lib::get_test_context().set_time_unit(test_lib::TestTimeUnit::MILLI_SECONDS);
  null_file = test_lib::assert_expected_value(io::OpenOptions{}.read_write().open("/dev/null"));
}

JOWI_ADD_TEST(spawn_returns_process_handle) {
  auto random_str = test_lib::random_string(16);
  auto [stdout_reader, stdout_writer] = test_lib::assert_expected_value(io::open_pipe());
  auto process = test_lib::assert_expected_value(
    proc::spawn(
      proc::SubprocessArgument{TEST_CHILD_EXEC, random_str}, stdout_writer, *null_file, *null_file
    )
  );
  {
    auto _ = std::move(stdout_writer);
  }
  auto result = test_lib::assert_expected_value(process.wait());
  test_lib::assert_equal(result.exit_code(), 0);
  auto buf = io::DynBuffer{2048};
  test_lib::assert_expected_value(stdout_reader.read(buf));
  test_lib::assert_equal(buf.read(), random_str + "\n");
}

JOWI_ADD_TEST(execute_test_normal_exit) {
  auto result = test_lib::assert_expected_value(
    proc::run(
      proc::SubprocessArgument{TEST_CHILD_EXEC, "arg1"}, true, *null_file, *null_file, *null_file
    )
  );
  test_lib::assert_equal(result.exit_code(), 0);
}

JOWI_ADD_TEST(execute_test_abnormal_exit) {
  auto result = test_lib::assert_expected_value(
    proc::run(proc::SubprocessArgument{TEST_CHILD_EXEC}, false, *null_file, *null_file, *null_file)
  );
  test_lib::assert_equal(result.exit_code(), 1);
}

JOWI_ADD_TEST(execute_pipe) {
  auto random_str = test_lib::random_string(20);
  auto [reader, writer] = test_lib::assert_expected_value(io::open_pipe());
  auto result = test_lib::assert_expected_value(
    proc::run(
      proc::SubprocessArgument{TEST_CHILD_EXEC, random_str}, true, writer, *null_file, *null_file
    )
  );
  {
    auto _ = std::move(writer);
  } // close writer
  test_lib::assert_equal(result.exit_code(), 0);
  auto buf = io::DynBuffer{2048};
  test_lib::assert_expected_value(reader.read(buf));
  test_lib::assert_equal(buf.read(), random_str + "\n");
}

JOWI_ADD_TEST(stdin_stdout_piped_correctly) {
  auto payload = test_lib::random_string(24);
  auto [stdout_reader, stdout_writer] = test_lib::assert_expected_value(io::open_pipe());
  auto [stdin_reader, stdin_writer] = test_lib::assert_expected_value(io::open_pipe());
  auto process = test_lib::assert_expected_value(
    proc::spawn(
      proc::SubprocessArgument{TEST_CHILD_EXEC, "echo_stdin", ""},
      stdout_writer,
      stdin_reader,
      *null_file
    )
  );
  {
    auto _ = std::move(stdout_writer);
  }
  {
    auto _ = std::move(stdin_reader);
  }
  test_lib::assert_expected_value(stdin_writer.write(payload + "\n"));
  {
    auto _ = std::move(stdin_writer);
  }
  auto wait_result = test_lib::assert_expected_value(process.wait());
  test_lib::assert_equal(wait_result.exit_code(), 0);
  auto buf = io::DynBuffer{2048};
  test_lib::assert_expected_value(stdout_reader.read(buf));
  test_lib::assert_equal(buf.read(), payload + "\n");
}

JOWI_ADD_TEST(environment_variable_forwarded_to_child) {
  constexpr std::string_view key = "JOWI_PROCESS_TEST_ENV";
  auto value = test_lib::random_string(18);
  auto env = proc::SubprocessEnv::make_env(false);
  env.set(key, value);
  auto [stdout_reader, stdout_writer] = test_lib::assert_expected_value(io::open_pipe());
  auto result = test_lib::assert_expected_value(
    proc::run(
      proc::SubprocessArgument{TEST_CHILD_EXEC, "env", std::string{key}},
      true,
      stdout_writer,
      *null_file,
      *null_file,
      env
    )
  );
  {
    auto _ = std::move(stdout_writer);
  }
  test_lib::assert_equal(result.exit_code(), 0);
  auto buf = io::DynBuffer{2048};
  test_lib::assert_expected_value(stdout_reader.read(buf));
  test_lib::assert_equal(buf.read(), value + "\n");
}

JOWI_ADD_TEST(execute_kill) {
  auto delay_time = std::to_string(test_lib::random_integer(1000, 5000));
  auto process = test_lib::assert_expected_value(
    proc::spawn(
      proc::SubprocessArgument{TEST_CHILD_EXEC, "delay", delay_time},
      *null_file,
      *null_file,
      *null_file
    )
  );
  test_lib::assert_equal(process.kill().has_value(), true);
  auto result = test_lib::assert_expected_value(process.wait(false));
  test_lib::assert_false(result.exit_code() == 0);
}

JOWI_ADD_TEST(wait_non_blocking_is_non_blocking) {
  auto process = test_lib::assert_expected_value(
    proc::spawn(
      proc::SubprocessArgument(TEST_CHILD_EXEC, "delay", "1000"), *null_file, *null_file, *null_file
    )
  );
  test_lib::assert_equal(process.wait_non_blocking()->has_value(), false);
  auto result = test_lib::assert_expected_value(process.wait());
  test_lib::assert_equal(result.exit_code(), 0);
}

/*
  Async Tests
*/

asio::BasicTask<proc::SubprocessResult> run_child_delay(uint16_t delay) {
  auto proc = test_lib::assert_expected_value(
    proc::spawn(
      {TEST_CHILD_EXEC, "delay", std::to_string(delay)}, *null_file, *null_file, *null_file
    )
  );
  co_return test_lib::assert_expected_value(co_await proc.async_wait());
}

JOWI_ADD_TEST(parallel_wait) {
  auto beg = std::chrono::system_clock::now();
  auto _ = asio::parallel(
    run_child_delay(500),
    run_child_delay(500),
    run_child_delay(500),
    run_child_delay(500),
    run_child_delay(500)
  );
  auto end = std::chrono::system_clock::now();
  test_lib::assert_lt(
    std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count(), 550
  );
}
