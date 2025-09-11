#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "At least one argument have to be given" << std::endl;
    return 1;
  } else if (argc == 2) {
    std::cout << argv[1] << std::endl;
  } else if (argc == 3) {
    std::string action = argv[1];
    std::string param = argv[2];
    if (action == "delay") {
      std::this_thread::sleep_for(std::chrono::milliseconds(std::atoi(param.c_str())));
    } else if (action == "echo_stdin") {
      std::string buffer;
      std::cin >> buffer;
      std::cout << buffer << std::endl;
    }
  }
}