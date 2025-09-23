#include <chrono>
#include <csignal>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
import jowi.process;

namespace proc = jowi::process;

int main(int argc, char **argv, char **envp) {
  auto beg = std::chrono::steady_clock::now();
  proc::subprocess_env::init(const_cast<const char **>(envp));
  if (argc < 2) {
    std::cerr << "At least one argument have to be given" << std::endl;
    return 1;
  }

  if (argc == 2) {
    std::cout << argv[1] << std::endl;
    return 0;
  }

  std::string action = argv[1];
  std::string param = argv[2];
  if (action == "delay") {
    auto delay = std::stoi(param);
    std::this_thread::sleep_until(beg + std::chrono::milliseconds(delay));
    return 0;
  }
  if (action == "echo_stdin") {
    std::string buffer;
    std::cin >> buffer;
    std::cout << buffer << std::endl;
    return 0;
  }
  if (action == "env") {
    if (auto entry = proc::subprocess_env::global_env().get(param); entry) {
      std::cout << entry->value << std::endl;
      return 0;
    }
    return 2;
  }

  return 0;
}
