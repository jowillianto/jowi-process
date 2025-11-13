module;
#include <sys/wait.h>
#include <cerrno>
#include <concepts>
#include <csignal>
#include <expected>
#include <functional>
#include <optional>
export module jowi.process:unique_pid;
import :subprocess_error;
import :subprocess_result;

namespace jowi::process {
  /**
   * @brief Invoke a POSIX call and return errno failures as `SubprocessError`.
   * @param f Callable object invoking the POSIX API.
   * @param args Arguments forwarded to the callable.
   */
  template <class F, class... Args>
    requires(std::invocable<F, Args...>)
  std::expected<std::invoke_result_t<F, Args...>, SubprocessError> sys_call(F &&f, Args &&...args) {
    int res = std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
    int err_no = errno;
    if (res == -1) {
      return std::unexpected{SubprocessError::from_errcode(err_no)};
    }
    return res;
  }

  /**
   * @brief Invoke a POSIX call that returns errno directly and map failures.
   * @param f Callable object invoking the POSIX API.
   * @param args Arguments forwarded to the callable.
   */
  template <class F, class... Args>
    requires(std::is_invocable_r_v<int, F, Args...>)
  std::expected<std::invoke_result_t<F, Args...>, SubprocessError> sys_call_return_err(
    F &&f, Args &&...args
  ) {
    int res = std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
    if (res != 0) {
      return std::unexpected{SubprocessError::from_errcode(res)};
    }
    return res;
  }

  /**
   * @brief Invoke a POSIX call that returns errno directly and map failures.
   * @param f Callable object invoking the POSIX API.
   * @param args Arguments forwarded to the callable.
   */
  template <class F, class... Args>
    requires(std::is_invocable_r_v<int, F, Args...>)
  std::expected<void, SubprocessError> sys_call_return_err_void(F &&f, Args &&...args) {
    int res = std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
    if (res != 0) {
      return std::unexpected{SubprocessError::from_errcode(res)};
    }
    return {};
  }

  /**
   * @brief RAII owner for a process identifier with wait and signalling helpers.
   */
  export struct UniquePid {
  private:
    pid_t __pid;

    /**
     * @brief Release ownership by attempting to kill and reap the process.
     */
    void __destroy() {
      if (__pid != -1) {
        auto kill_res = send_signal(__pid);
        auto wait_res = wait();
      }
    }

  public:
    /**
     * @brief Adopt a POSIX process identifier.
     * @param pid Newly spawned process identifier to manage.
     */
    explicit UniquePid(pid_t pid) : __pid{pid} {}
    /**
     * @brief Transfer ownership from another `UniquePid`.
     * @param p Source PID wrapper losing ownership.
     */
    UniquePid(UniquePid &&p) : __pid{p.__pid} {
      p.__pid = -1;
    }
    /**
     * @brief Copy construction is disabled because PID ownership cannot be duplicated.
     */
    UniquePid(const UniquePid &) = delete;
    /**
     * @brief Copy assignment is disabled because PID ownership cannot be duplicated.
     */
    UniquePid &operator=(const UniquePid &) = delete;
    /**
     * @brief Move-assign, releasing any currently owned process.
     * @param p Source PID wrapper losing ownership.
     */
    UniquePid &operator=(UniquePid &&p) {
      __destroy();
      __pid = p.__pid;
      p.__pid = -1;
      return *this;
    }

    /**
     * @brief Block until the process terminates and optionally validate the exit code.
     * @param check When true, non-zero exits surface as errors.
     */
    std::expected<SubprocessResult, SubprocessError> wait(bool check = true) noexcept {
      int status = 0;
      return sys_call(waitpid, __pid, &status, 0).and_then([&](int) {
        __pid = -1;
        return SubprocessError::check_status(status, check);
      });
    }
    /**
     * @brief Poll the process without blocking and return completion if available.
     * @param check When true, non-zero exits surface as errors.
     */
    std::expected<std::optional<SubprocessResult>, SubprocessError> wait_non_blocking(
      bool check
    ) noexcept {
      using expected_type = std::expected<std::optional<SubprocessResult>, SubprocessError>;
      int status = 0;
      return sys_call(waitpid, __pid, &status, WNOHANG).and_then([&](int res) -> expected_type {
        if (res == 0) {
          return expected_type{std::optional<SubprocessResult>{std::nullopt}};
        } else {
          __pid = -1;
          return SubprocessError::check_status(status, check);
        }
      });
    }
    /**
     * @brief Send a signal to the process while keeping the wrapper const.
     * @param sig Signal number to deliver.
     */
    std::expected<std::reference_wrapper<const UniquePid>, SubprocessError> send_signal(
      int sig
    ) const noexcept {
      return sys_call(kill, __pid, sig).transform([&](int res) { return std::ref(*this); });
    }
    /**
     * @brief Send a signal to the process.
     * @param sig Signal number to deliver.
     */
    std::expected<std::reference_wrapper<UniquePid>, SubprocessError> send_signal(
      int sig
    ) noexcept {
      return sys_call(kill, __pid, sig).transform([&](int res) { return std::ref(*this); });
    }

    /**
     * @brief Expose the underlying process identifier.
     */
    pid_t pid() const noexcept {
      return __pid;
    }

    /**
     * @brief Ensure the process is cleaned up when the owner goes out of scope.
     */
    ~UniquePid() noexcept {
      __destroy();
    }
  };
}
