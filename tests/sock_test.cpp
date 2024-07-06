import moderna.io;
#include <unistd.h>
#include <cstdlib>
#include <format>

int main(int argc, char **argv) {
  int port = std::atoi(argv[1]);
  moderna::io::readable_file::open(INDEX_HTML)
    .and_then([](auto &&file) { return file.read(); })
    .transform([&](auto &&line) {
      return moderna::io::listener_sock_file::create_tcp_listener(port, 50)
      .transform([&](auto && listener) {
        return listener.accept().transform([&](auto &&accepted) {
          accepted.io
            .write(std::format(
              "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n Content-Length: {}\r\n\r\n",
              line.length()
            ))
            .and_then([&]() { return accepted.io.write(line); })
            .value();
          return accepted.io.close();
        });
      });
    })
    .value();
}