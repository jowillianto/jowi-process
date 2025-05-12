import moderna.test_lib;
import moderna.process;
import moderna.io;
#include <moderna/test_lib.hpp>
#include <string>

namespace test_lib = moderna::test_lib;
namespace io = moderna::io;
namespace proc = moderna::process;

MODERNA_SETUP(argc, argv) {
  test_lib::get_test_context().set_time_unit(test_lib::test_time_unit::MILLI_SECONDS);
}

MODERNA_ADD_TEST(execute_test_normal_exit) {
  auto null_writer = io::open_file<io::open_mode::write_truncate>("/dev/null").value();
  auto null_reader = io::open_file<io::open_mode::read>("/dev/null").value();
  auto completed_process = proc::run(
    proc::subprocess_argument{TEST_CHILD_EXEC, "arg1"},
    io::get_native_handle(null_writer),
    io::get_native_handle(null_reader),
    io::get_native_handle(null_writer)
  );
  test_lib::assert_equal(completed_process.has_value(), true);
  auto result = completed_process.value();
  test_lib::assert_equal(result.exit_code(), 0);
}

MODERNA_ADD_TEST(execute_test_abnormal_exit) {
  auto null_writer = io::open_file<io::open_mode::write_truncate>("/dev/null").value();
  auto null_reader = io::open_file<io::open_mode::read>("/dev/null").value();
  auto completed_process = proc::run(
    proc::subprocess_argument{TEST_CHILD_EXEC},
    false,
    io::get_native_handle(null_writer),
    io::get_native_handle(null_reader),
    io::get_native_handle(null_writer)
  );
  test_lib::assert_expected(completed_process);
  auto result = completed_process.value();
  test_lib::assert_equal(result.exit_code(), 1);
}

MODERNA_ADD_TEST(execute_pipe) {
  auto random_str = moderna::test_lib::random_string(20);
  auto pipe = io::open_pipe().value();
  auto completed_process = proc::run(
    proc::subprocess_argument{TEST_CHILD_EXEC, random_str}, true, io::get_native_handle(pipe.writer)
  );
  pipe.writer.close();
  test_lib::assert_equal(completed_process.value().exit_code(), 0);
  test_lib::assert_equal(pipe.reader.read().value(), random_str + "\n");
}

MODERNA_ADD_TEST(execute_kill) {
  auto delay_time = std::to_string(test_lib::random_integer(1000, 5000));
  auto process =
    proc::spawn(proc::subprocess_argument{TEST_CHILD_EXEC, "delay", delay_time}).value();
  test_lib::assert_equal(process.kill().has_value(), true);
  auto completed_process = process.wait(false);
  test_lib::assert_false(completed_process->exit_code() == 0);
}

MODERNA_ADD_TEST(wait_non_blocking_is_non_blocking) {
  auto null_pipe = fopen("/dev/null", "w+");
  auto fd = fileno(null_pipe);
  auto process =
    proc::spawn(proc::subprocess_argument(TEST_CHILD_EXEC, "delay", "1000"), fd, fd, fd).value();
  test_lib::assert_equal(process.wait_non_blocking().value().has_value(), false);
  auto completed_process = process.wait();
  test_lib::assert_equal(completed_process.has_value(), true);
  auto result = completed_process.value();
  test_lib::assert_equal(result.exit_code(), 0);
  fclose(null_pipe);
}