module;
#include <expected>
#include <format>
#include <optional>
#include <string>
export module moderna.process:subprocess_error;
import moderna.generic;
import :subprocess_result;

namespace moderna::process {
  export struct subprocess_error : public std::exception {
  private:
    std::optional<subprocess_result> __res;
    std::optional<int> __err;
    std::string __msg;

  public:
    template <class... Args>
    subprocess_error(int err_code, std::format_string<Args...> fmt, Args &&...args) :
      __res{std::nullopt}, __err{err_code}, __msg{std::format(fmt, std::forward<Args>(args)...)} {}
    template <class... Args>
    subprocess_error(subprocess_result res, std::format_string<Args...> fmt, Args &&...args) :
      __res{res}, __err{std::nullopt}, __msg{std::format(fmt, std::forward<Args>(args)...)} {}

    const char *what() const noexcept {
      return __msg.c_str();
    }

    std::optional<subprocess_result> exit_result() const noexcept {
      return __res;
    }

    std::optional<int> error_code() const noexcept {
      return __err;
    }

    std::optional<int> exit_code() const noexcept {
      return __res.transform(&subprocess_result::exit_code);
    }

    static subprocess_error from_errcode(int err_no) {
      return subprocess_error{err_no, "{}", strerror(err_no)};
    }
    static subprocess_error from_result(subprocess_result res) {
      return subprocess_error{res, "process exit: {}", res.exit_code()};
    }

    static std::expected<subprocess_result, subprocess_error> check_status(
      int status_code, bool check
    ) {
      subprocess_result status{status_code};
      if (!check || status.exit_code() == 0) {
        return status;
      } else {
        return std::unexpected{subprocess_error::from_result(status)};
      }
    }
  };
}
