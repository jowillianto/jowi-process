import moderna.test_lib;
import moderna.process;
import moderna.io;
#include <string>
#include <system_error>
#include <variant>

using namespace moderna;

auto process_tests =
  test_lib::make_tester("Process Test")
    .add_test(
      "execute_test_normal_exit",
      []() {
        auto null_writer = moderna::io::writable_file::open("/dev/null").value();
        auto null_reader = moderna::io::readable_file::open("/dev/null").value();
        auto completed_process =
          subprocess{
            process::static_argument{TEST_CHILD_EXEC, "arg1"},
            null_writer.fd(),
            null_reader.fd(),
            null_writer.fd()
          }
            .wait();
        test_lib::assert_equal(completed_process.has_value(), true);
        auto result = completed_process.value();
        test_lib::assert_equal(result.exit_code(), 0);
      }
    )
    .add_test(
      "execute_test_abnormal_exit",
      []() {
        auto null_writer = moderna::io::writable_file::open("/dev/null").value();
        auto null_reader = moderna::io::readable_file::open("/dev/null").value();
        auto completed_process =
          subprocess{
            process::static_argument{TEST_CHILD_EXEC},
            null_writer.fd(),
            null_reader.fd(),
            null_writer.fd()
          }
            .wait();
        test_lib::assert_equal(completed_process.has_value(), true);
        auto result = completed_process.value();
        test_lib::assert_equal(result.exit_code(), 1);
      }
    )
    .add_test(
      "execute_pipe",
      []() {
        auto random_str = moderna::test_lib::random_string(20);
        auto pipe = moderna::io::pipe::open().value();
        auto completed_process =
          moderna::subprocess{process::static_argument{TEST_CHILD_EXEC, random_str}, pipe.w.fd()}
            .wait();
        pipe.w.close();
        test_lib::assert_equal(completed_process.value().exit_code(), 0);
        test_lib::assert_equal(pipe.r.read().value(), random_str + "\n");
      }
    )
    .add_test(
      "execute_kill",
      []() {
        auto delay_time = std::to_string(test_lib::random_integer(1000, 5000));
        auto process =
          moderna::subprocess{process::static_argument{TEST_CHILD_EXEC, "delay", delay_time}};
        test_lib::assert_equal(process.kill().has_value(), true);
        auto completed_process = process.wait();
        test_lib::assert_equal(
          std::get<moderna::process_result_error>(completed_process.error().error).result.exit_code() != 0, true
        );
      }
    )
    .add_test(
      "wait_non_blocking_is_non_blocking",
      []() {
        auto null_pipe = fopen("/dev/null", "w+");
        auto fd = fileno(null_pipe);
        auto process = moderna::subprocess{
          process::static_argument{TEST_CHILD_EXEC, "delay", "1000"}, fd, fd, fd
        };
        test_lib::assert_equal(process.wait_non_blocking().value().has_value(), false);
        auto completed_process = process.wait();
        test_lib::assert_equal(completed_process.has_value(), true);
        auto result = completed_process.value();
        test_lib::assert_equal(result.exit_code(), 0);
        fclose(null_pipe);
      }
    )
    .add_test(
      "execute_send_sig",
      []() {

      }
    )
    .add_test("dyn_arg", []() {
      int arg_count = test_lib::random_integer(10, 50);
      std::vector<std::string> args;
      args.reserve(arg_count);
      for (size_t i = 0; i < arg_count; i += 1)
        args.push_back(test_lib::random_string(20));
      auto arg_pack = process::dyna_argument{TEST_CHILD_EXEC, args};
      test_lib::assert_equal(arg_pack.size(), arg_count + 1);
      auto null_pipe = fopen("/dev/null", "w+");
      auto fd = fileno(null_pipe);
      auto completed_process = moderna::subprocess{arg_pack, fd, fd, fd}.wait();
      test_lib::assert_equal(completed_process.has_value(), true);
      auto result = completed_process.value();
      test_lib::assert_equal(result.exit_code(), 0);
      fclose(null_pipe);
    });

int main(int argc, const char **argv, const char **env) {
  process::env::init_global(env);
  process_tests.print_or_exit();
}