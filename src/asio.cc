module;
#include <chrono>
#include <coroutine>
#include <expected>
#include <functional>
#include <optional>
#include <thread>
export module jowi.process:asio;
import jowi.asio;
import :subprocess_error;
import :subprocess_result;
import :unique_pid;

namespace jowi::process {
  /**
   * @brief Awaitable wrapper that co_awaits process completion using `unique_pid`.
   */
  struct process_wait {
  private:
    std::reference_wrapper<unique_pid> __p;
    bool __check;
    std::optional<std::expected<subprocess_result, subprocess_error>> __res;

  public:
    static constexpr bool is_defer_awaitable = true;

    /**
     * @brief Construct the awaitable for the provided process and exit checking policy.
     * @param p Process to monitor.
     * @param check When true, non-zero exit codes become errors.
     */
    process_wait(unique_pid &p, bool check = true) :
      __p{std::ref(p)}, __res{std::nullopt}, __check{check} {}

    /**
     * @brief Attempt a non-blocking wait to determine whether the coroutine should suspend.
     */
    bool await_ready() {
      __res.reset();
      auto res = __p.get().wait_non_blocking(__check);
      if (!res) {
        __res = std::unexpected{res.error()};
        return true;
      }
      if (res->has_value()) {
        __res = res->value();
        return true;
      }
      return false;
    }

    /**
     * @brief Suspend the coroutine while performing a blocking wait.
     * @param h Coroutine handle that will be resumed once waiting finishes.
     */
    std::coroutine_handle<void> await_suspend(std::coroutine_handle<void> h) {
      __res = __p.get().wait(__check);
      return h;
    }

    /**
     * @brief Produce the final wait result once resumed.
     */
    std::expected<subprocess_result, subprocess_error> await_resume() {
      return std::move(__res).value();
    }
  };

  /**
   * @brief Awaitable that waits for a process with a timeout before resuming.
   */
  struct process_wait_for {
  private:
    std::reference_wrapper<unique_pid> __p;
    std::chrono::system_clock::time_point __tp;
    std::chrono::milliseconds __dur;
    bool __check;
    std::optional<std::expected<std::optional<subprocess_result>, subprocess_error>> __res;

  public:
    static constexpr bool is_defer_awaitable = true;
    /**
     * @brief Construct the awaitable with the target process, timeout, and exit checking policy.
     * @param p Process to monitor.
     * @param dur Duration to wait before timing out.
     * @param check When true, non-zero exit codes become errors.
     */
    process_wait_for(unique_pid &p, std::chrono::milliseconds dur, bool check = true) :
      __p{std::ref(p)}, __tp{std::chrono::system_clock::now() + dur}, __dur{dur}, __check{check} {}

    /**
     * @brief Poll the process and decide whether to suspend or resume immediately.
     */
    bool await_ready() {
      __res = __p.get().wait_non_blocking(__check);
      if (!__res) {
        return false;
      }
      if (!__res->has_value()) {
        return true;
      }
      auto &optional_result = __res->value();
      if (optional_result.has_value()) {
        return true;
      }
      if (std::chrono::system_clock::now() >= __tp) {
        __res.emplace(
          std::expected<std::optional<subprocess_result>, subprocess_error>{std::nullopt}
        );
        return true;
      }
      return false;
    }

    /**
     * @brief Suspend execution while looping until the timeout expires or completion occurs.
     * @param h Coroutine handle that will resume after waiting.
     */
    std::coroutine_handle<void> await_suspend(std::coroutine_handle<void> h) {
      while (!await_ready()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
      }
      return h;
    }

    /**
     * @brief Return the non-blocking wait result when the coroutine resumes.
     */
    std::expected<std::optional<subprocess_result>, subprocess_error> await_resume() {
      return std::move(__res).value();
    }
  };
}
