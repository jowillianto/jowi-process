#include <jowi/test_lib.hpp>
#include <string>
import jowi.test_lib;
import jowi.process;
import jowi.io;

namespace test_lib = jowi::test_lib;
namespace io = jowi::io;
namespace proc = jowi::process;

JOWI_SETUP(argc, argv) {
  test_lib::get_test_context().set_time_unit(test_lib::test_time_unit::MILLI_SECONDS);
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
  test_lib::assert_equal(io::make_byte_reader<2048>(std::move(reader)).read(), random_str + "\n");
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