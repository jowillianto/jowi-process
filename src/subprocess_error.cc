module;
#include <format>
#include <optional>
#include <string>
export module moderna.process:subprocess_error;
import :subprocess_result;

namespace moderna::process {
  export struct subprocess_error : public std::exception {
    std::optional<subprocess_result> run_result;
    std::string error_message;
    int error_code;

    subprocess_error(std::optional<subprocess_result> res, std::string msg, int error_code) :
      run_result{std::move(res)}, error_message{std::move(msg)}, error_code{error_code} {}

    const char *what() const noexcept {
      return error_message.c_str();
    }

    static subprocess_error from_errcode(int err_no) {
      return subprocess_error{std::nullopt, strerror(err_no), err_no};
    }
    static subprocess_error from_result(subprocess_result res) {
      int exit_code = res.exit_code();
      return subprocess_error{
        std::move(res),
        std::format("process exited with non zero exit code : {}", exit_code),
        exit_code
      };
    }
  };
}
