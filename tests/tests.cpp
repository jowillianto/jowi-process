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

JOWI_SETUP(argc, argv) {
  test_lib::get_test_context().set_time_unit(test_lib::test_time_unit::MILLI_SECONDS);
}

JOWI_ADD_TEST(spawn_returns_process_handle) {
  auto random_str = test_lib::random_string(16);
  auto [stdout_reader, stdout_writer] = test_lib::assert_expected_value(io::open_pipe());
  auto process = test_lib::assert_expected_value(
    proc::spawn(
      proc::subprocess_argument{TEST_CHILD_EXEC, random_str},
      stdout_writer,
      io::basic_file<int>{0},
      io::basic_file<int>{2}
    )
  );
  {
    auto _ = std::move(stdout_writer);
  }
  auto result = test_lib::assert_expected_value(process.wait());
  test_lib::assert_equal(result.exit_code(), 0);
  io::byte_reader stdout_reader_bytes{io::fixed_buffer<2048>{}, std::move(stdout_reader)};
  auto stdout_content = test_lib::assert_expected_value(stdout_reader_bytes.read());
  test_lib::assert_equal(stdout_content, random_str + "\n");
}

JOWI_ADD_TEST(execute_test_normal_exit) {
  auto null_writer = test_lib::assert_expected_value(io::open_options{}.write().open("/dev/null"));
  auto null_reader = test_lib::assert_expected_value(io::open_options{}.read().open("/dev/null"));
  auto result = test_lib::assert_expected_value(
    proc::run(
      proc::subprocess_argument{TEST_CHILD_EXEC, "arg1"},
      true,
      null_writer,
      null_reader,
      null_writer
    )
  );
  test_lib::assert_equal(result.exit_code(), 0);
}

JOWI_ADD_TEST(execute_test_abnormal_exit) {
  auto null_writer = test_lib::assert_expected_value(io::open_options{}.write().open("/dev/null"));
  auto null_reader = test_lib::assert_expected_value(io::open_options{}.read().open("/dev/null"));
  auto result = test_lib::assert_expected_value(
    proc::run(
      proc::subprocess_argument{TEST_CHILD_EXEC}, false, null_writer, null_reader, null_writer
    )
  );
  test_lib::assert_equal(result.exit_code(), 1);
}

JOWI_ADD_TEST(execute_pipe) {
  auto random_str = test_lib::random_string(20);
  auto [reader, writer] = test_lib::assert_expected_value(io::open_pipe());
  auto result = test_lib::assert_expected_value(
    proc::run(
      proc::subprocess_argument{TEST_CHILD_EXEC, random_str},
      true,
      writer,
      io::basic_file<int>{1},
      io::basic_file<int>{2}
    )
  );
  {
    auto _ = std::move(writer);
  } // close writer
  test_lib::assert_equal(result.exit_code(), 0);
  io::byte_reader pipe_reader{io::fixed_buffer<2048>{}, std::move(reader)};
  auto reader_content = test_lib::assert_expected_value(pipe_reader.read());
  test_lib::assert_equal(reader_content, random_str + "\n");
}

JOWI_ADD_TEST(stdin_stdout_piped_correctly) {
  auto payload = test_lib::random_string(24);
  auto [stdout_reader, stdout_writer] = test_lib::assert_expected_value(io::open_pipe());
  auto [stdin_reader, stdin_writer] = test_lib::assert_expected_value(io::open_pipe());
  auto process = test_lib::assert_expected_value(
    proc::spawn(
      proc::subprocess_argument{TEST_CHILD_EXEC, "echo_stdin", ""},
      stdout_writer,
      stdin_reader,
      io::basic_file<int>{2}
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
  io::byte_reader stdout_pipe_reader{io::fixed_buffer<2048>{}, std::move(stdout_reader)};
  auto stdout_payload = test_lib::assert_expected_value(stdout_pipe_reader.read());
  test_lib::assert_equal(stdout_payload, payload + "\n");
}

JOWI_ADD_TEST(environment_variable_forwarded_to_child) {
  constexpr std::string_view key = "JOWI_PROCESS_TEST_ENV";
  auto value = test_lib::random_string(18);
  auto env = proc::subprocess_env::make_env(false);
  env.set(key, value);
  auto [stdout_reader, stdout_writer] = test_lib::assert_expected_value(io::open_pipe());
  auto result = test_lib::assert_expected_value(
    proc::run(
      proc::subprocess_argument{TEST_CHILD_EXEC, "env", std::string{key}},
      true,
      stdout_writer,
      io::basic_file<int>{0},
      io::basic_file<int>{2},
      env
    )
  );
  {
    auto _ = std::move(stdout_writer);
  }
  test_lib::assert_equal(result.exit_code(), 0);
  io::byte_reader env_reader_bytes{io::fixed_buffer<2048>{}, std::move(stdout_reader)};
  auto env_output = test_lib::assert_expected_value(env_reader_bytes.read());
  test_lib::assert_equal(env_output, value + "\n");
}

JOWI_ADD_TEST(execute_kill) {
  auto delay_time = std::to_string(test_lib::random_integer(1000, 5000));
  auto process = test_lib::assert_expected_value(
    proc::spawn(proc::subprocess_argument{TEST_CHILD_EXEC, "delay", delay_time})
  );
  test_lib::assert_equal(process.kill().has_value(), true);
  auto result = test_lib::assert_expected_value(process.wait(false));
  test_lib::assert_false(result.exit_code() == 0);
}

JOWI_ADD_TEST(wait_non_blocking_is_non_blocking) {
  auto null_pipe =
    test_lib::assert_expected_value(io::open_options{}.read_write().open("/dev/null"));
  auto process = test_lib::assert_expected_value(
    proc::spawn(
      proc::subprocess_argument(TEST_CHILD_EXEC, "delay", "1000"), null_pipe, null_pipe, null_pipe
    )
  );
  test_lib::assert_equal(process.wait_non_blocking()->has_value(), false);
  auto result = test_lib::assert_expected_value(process.wait());
  test_lib::assert_equal(result.exit_code(), 0);
}

/*
  Async Tests
*/

asio::basic_task<proc::subprocess_result> run_child_delay(uint16_t delay) {
  auto null_pipe =
    test_lib::assert_expected_value(io::open_options{}.read_write().open("/dev/null"));
  auto proc = test_lib::assert_expected_value(
    proc::spawn({TEST_CHILD_EXEC, "delay", std::to_string(delay)}, null_pipe, null_pipe, null_pipe)
  );
  co_return test_lib::assert_expected_value(co_await proc.async_wait());
}

JOWI_ADD_TEST(parallel_wait) {
  auto beg = std::chrono::system_clock::now();
  auto [res1, res2] = asio::parallel(run_child_delay(500), run_child_delay(500));
  auto end = std::chrono::system_clock::now();
  test_lib::assert_lt(
    std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count(), 550
  );
}
