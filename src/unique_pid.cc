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
   * @brief Invoke a POSIX call and return errno failures as `subprocess_error`.
   * @param f Callable object invoking the POSIX API.
   * @param args Arguments forwarded to the callable.
   */
  template <class F, class... Args>
    requires(std::invocable<F, Args...>)
  std::expected<std::invoke_result_t<F, Args...>, subprocess_error> sys_call(
    F &&f, Args &&...args
  ) {
    int res = std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
    int err_no = errno;
    if (res == -1) {
      return std::unexpected{subprocess_error::from_errcode(err_no)};
    }
    return res;
  }

  /**
   * @brief Invoke a POSIX call that returns errno directly and map failures.
   * @param f Callable object invoking the POSIX API.
   * @param args Arguments forwarded to the callable.
   */
  template <class F, class... Args>
    requires(std::invocable<F, Args...>)
  std::expected<std::invoke_result_t<F, Args...>, subprocess_error> sys_call_return_err(
    F &&f, Args &&...args
  ) {
    int res = std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
    if (res != 0) {
      return std::unexpected{subprocess_error::from_errcode(res)};
    }
    return res;
  }

  /**
   * @brief RAII owner for a process identifier with wait and signalling helpers.
   */
  export struct unique_pid {
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
    explicit unique_pid(pid_t pid) : __pid{pid} {}
    /**
     * @brief Transfer ownership from another `unique_pid`.
     * @param p Source PID wrapper losing ownership.
     */
    unique_pid(unique_pid &&p) : __pid{p.__pid} {
      p.__pid = -1;
    }
    /**
     * @brief Copy construction is disabled because PID ownership cannot be duplicated.
     */
    unique_pid(const unique_pid &) = delete;
    /**
     * @brief Copy assignment is disabled because PID ownership cannot be duplicated.
     */
    unique_pid &operator=(const unique_pid &) = delete;
    /**
     * @brief Move-assign, releasing any currently owned process.
     * @param p Source PID wrapper losing ownership.
     */
    unique_pid &operator=(unique_pid &&p) {
      __destroy();
      __pid = p.__pid;
      p.__pid = -1;
      return *this;
    }

    /**
     * @brief Block until the process terminates and optionally validate the exit code.
     * @param check When true, non-zero exits surface as errors.
     */
    std::expected<subprocess_result, subprocess_error> wait(bool check = true) noexcept {
      int status = 0;
      return sys_call(waitpid, __pid, &status, 0).and_then([&](int) {
        __pid = -1;
        return subprocess_error::check_status(status, check);
      });
    }
    /**
     * @brief Poll the process without blocking and return completion if available.
     * @param check When true, non-zero exits surface as errors.
     */
    std::expected<std::optional<subprocess_result>, subprocess_error> wait_non_blocking(
      bool check
    ) noexcept {
      using expected_type = std::expected<std::optional<subprocess_result>, subprocess_error>;
      int status = 0;
      return sys_call(waitpid, __pid, &status, WNOHANG).and_then([&](int res) -> expected_type {
        if (res == 0) {
          return expected_type{std::optional<subprocess_result>{std::nullopt}};
        } else {
          __pid = -1;
          return subprocess_error::check_status(status, check);
        }
      });
    }
    /**
     * @brief Send a signal to the process while keeping the wrapper const.
     * @param sig Signal number to deliver.
     */
    std::expected<std::reference_wrapper<const unique_pid>, subprocess_error> send_signal(
      int sig
    ) const noexcept {
      return sys_call(kill, __pid, sig).transform([&](int res) { return std::ref(*this); });
    }
    /**
     * @brief Send a signal to the process.
     * @param sig Signal number to deliver.
     */
    std::expected<std::reference_wrapper<unique_pid>, subprocess_error> send_signal(
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
    ~unique_pid() noexcept {
      __destroy();
    }
  };
}
