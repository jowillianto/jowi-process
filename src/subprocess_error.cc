module;
#include <expected>
#include <format>
#include <optional>
export module jowi.process:subprocess_error;
import jowi.generic;
import :subprocess_result;

namespace jowi::process {
  /**
   * @brief Exception type that encapsulates subprocess failures and exit status.
   */
  export struct SubprocessError : public std::exception {
  private:
    generic::Variant<SubprocessResult, int> __err;
    generic::FixedString<64> __msg;

    /**
     * @brief Create an error from an errno value and formatted message.
     * @param err_code POSIX error code captured from the failing call.
     * @param fmt Format string used to build the descriptive message.
     * @param args Arguments to interpolate into the format string.
     */
    template <class... Args>
    SubprocessError(int err_code, std::format_string<Args...> fmt, Args &&...args) :
      __err{err_code}, __msg{} {
      __msg.emplace_format(fmt, std::forward<Args>(args)...);
    }
    /**
     * @brief Create an error from a subprocess result and formatted message.
     * @param res Completed subprocess result describing the failure.
     * @param fmt Format string used to build the descriptive message.
     * @param args Arguments to interpolate into the format string.
     */
    template <class... Args>
    SubprocessError(SubprocessResult res, std::format_string<Args...> fmt, Args &&...args) :
      __err{res}, __msg{} {
      __msg.emplace_format(fmt, std::forward<Args>(args)...);
    }

  public:
    /**
     * @brief Retrieve the descriptive message describing the failure.
     */
    const char *what() const noexcept {
      return __msg.c_str();
    }
    /**
     * @brief Obtain the captured exit status when the subprocess completed.
     */
    std::optional<SubprocessResult> exit_result() const noexcept {
      return __err.as<SubprocessResult>();
    }
    /**
     * @brief Obtain the stored errno value for system call failures.
     */
    std::optional<int> error_code() const noexcept {
      return __err.as<int>();
    }
    /**
     * @brief Convenience accessor for the exit code when available.
     */
    std::optional<int> exit_code() const noexcept {
      return __err.as<SubprocessResult>().transform(&SubprocessResult::exit_code);
    }
    /**
     * @brief Build an error from an errno value.
     * @param err_no POSIX error code to wrap.
     */
    static SubprocessError from_errcode(int err_no) {
      return SubprocessError{err_no, "{}", strerror(err_no)};
    }
    /**
     * @brief Build an error from a subprocess result and populate a message.
     * @param res Result object describing the subprocess termination.
     */
    static SubprocessError from_result(SubprocessResult res) {
      return SubprocessError{res, "process exit: {}", res.exit_code()};
    }
    /**
     * @brief Return an `expected` that fails when `check` requires zero exit codes.
     * @param status_code Raw status obtained from `waitpid`.
     * @param check When true, non-zero exits become errors.
     */
    static std::expected<SubprocessResult, SubprocessError> check_status(
      int status_code, bool check
    ) {
      SubprocessResult status{status_code};
      if (!check || status.exit_code() == 0) {
        return status;
      } else {
        return std::unexpected{SubprocessError::from_result(status)};
      }
    }
  };
}
