module;
#include <expected>
#include <format>
#include <optional>
export module jowi.process:subprocess_error;
import jowi.generic;
import :subprocess_result;

namespace jowi::process {
  export struct subprocess_error : public std::exception {
  private:
    generic::variant<subprocess_result, int> __err;
    generic::fixed_string<64> __msg;

    template <class... Args>
    subprocess_error(int err_code, std::format_string<Args...> fmt, Args &&...args) :
      __err{err_code}, __msg{} {
      __msg.emplace_format(fmt, std::forward<Args>(args)...);
    }
    template <class... Args>
    subprocess_error(subprocess_result res, std::format_string<Args...> fmt, Args &&...args) :
      __err{res}, __msg{} {
      __msg.emplace_format(fmt, std::forward<Args>(args)...);
    }

  public:
    const char *what() const noexcept {
      return __msg.c_str();
    }
    std::optional<subprocess_result> exit_result() const noexcept {
      return __err.as<subprocess_result>();
    }
    std::optional<int> error_code() const noexcept {
      return __err.as<int>();
    }
    std::optional<int> exit_code() const noexcept {
      return __err.as<subprocess_result>().transform(&subprocess_result::exit_code);
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
