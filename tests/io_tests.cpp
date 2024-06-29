import moderna.io;
import moderna.test_lib;
import moderna.file_lock;
#include <format>
#include <mutex>

auto reader_test =
  moderna::test_lib::make_tester("reader_tests")
    .add_test(
      "simple_read",
      []() {
        auto file_reader = moderna::io::readable_file::open(READ_FILE).value();
        moderna::test_lib::assert_equal(
          file_reader.read().value(), "HELLO WORLD 0\nHELLO WORLD 1\nHELLO WORLD 2"
        );
      }
    )
    .add_test(
      "simple_read_lines",
      []() {
        auto file_reader = moderna::io::readable_file::open(READ_FILE).value();
        auto read_content = file_reader.readlines().value();
        for (int i = 0; i < read_content.size(); i += 1) {
          moderna::test_lib::assert_equal(read_content[i], std::format("HELLO WORLD {}", i));
        }
      }
    )
    .add_test("simple_read_line", []() {
      auto file_reader = moderna::io::readable_file::open(READ_FILE).value();
      moderna::test_lib::assert_equal(file_reader.readline().value(), "HELLO WORLD 0\n");
    });
auto writer_test = moderna::test_lib::make_tester("writer_tests")
                     .add_test(
                       "simple_write",
                       []() {
                         auto mutex = moderna::file_lock::FileMutex(WRITE_FILE);
                         std::unique_lock l{mutex};
                         auto writer = moderna::io::writable_file::open(WRITE_FILE).value();
                         writer.write("HELLO WORLD");
                         writer.close();
                         l.unlock();
                         auto reader = moderna::io::readable_file::open(WRITE_FILE).value();
                         moderna::test_lib::assert_equal(reader.read().value(), "HELLO WORLD");
                       }
                     )
                     .add_test("simple_write_line", []() {
                       auto mutex = moderna::file_lock::FileMutex(WRITE_FILE);
                       std::unique_lock l{mutex};
                       auto writer = moderna::io::writable_file::open(WRITE_FILE).value();
                       writer.writeline("HELLO WORLD");
                       writer.close();
                       l.unlock();
                       auto reader = moderna::io::readable_file::open(WRITE_FILE).value();
                       moderna::test_lib::assert_equal(reader.read().value(), "HELLO WORLD\n");
                     });

int main() {
  reader_test.print_or_exit();
  writer_test.print_or_exit();
}