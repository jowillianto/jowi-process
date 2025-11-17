module;
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cerrno>
#include <cstdlib>
#include <csignal>
#include <expected>
#include <functional>
#include <optional>
export module jowi.process:unique_pid;
import :subprocess_error;
import :subprocess_result;

namespace jowi::process {

  using native_pid_t = DWORD;
  using pid_t = native_pid_t;

  /**
   * @brief RAII owner for a Windows process handle with wait and signalling helpers.
   */
  export struct UniquePid {
  private:
    HANDLE __process;
    native_pid_t __pid;

    /**
     * @brief Release the process handle by terminating and closing it.
     */
    void __destroy() noexcept {
      if (__process != INVALID_HANDLE_VALUE) {
        TerminateProcess(__process, 1);
        WaitForSingleObject(__process, 100);
        CloseHandle(__process);
      }
      __process = INVALID_HANDLE_VALUE;
      __pid = static_cast<native_pid_t>(-1);
    }

    /**
     * @brief Read the exit code and convert it into a `SubprocessResult`.
     * @param check When true, non-zero exit codes surface as errors.
     */
    std::expected<SubprocessResult, SubprocessError> __finalise(bool check) noexcept {
      DWORD exit_code = 0;
      if (!GetExitCodeProcess(__process, &exit_code)) {
        DWORD err = GetLastError();
        _dosmaperr(err);
        return std::unexpected{SubprocessError::from_errcode(errno)};
      }
      CloseHandle(__process);
      __process = INVALID_HANDLE_VALUE;
      __pid = static_cast<native_pid_t>(-1);
      return SubprocessError::check_status(static_cast<int>(exit_code), check);
    }

  public:
    /**
     * @brief Adopt a new process handle and identifier pair.
     * @param process_handle Handle to the spawned process.
     * @param pid Process identifier reported by Windows.
     */
    explicit UniquePid(HANDLE process_handle, native_pid_t pid) :
      __process{process_handle}, __pid{pid} {}

    /**
     * @brief Move construct from another PID wrapper.
     */
    UniquePid(UniquePid &&other) noexcept : __process{other.__process}, __pid{other.__pid} {
      other.__process = INVALID_HANDLE_VALUE;
      other.__pid = static_cast<native_pid_t>(-1);
    }

    /**
     * @brief Copy construction is disabled.
     */
    UniquePid(const UniquePid &) = delete;

    /**
     * @brief Move assignment releases the previous handle and adopts the new one.
     * @param other Source PID wrapper losing ownership.
     */
    UniquePid &operator=(UniquePid &&other) noexcept {
      if (this != &other) {
        __destroy();
        __process = other.__process;
        __pid = other.__pid;
        other.__process = INVALID_HANDLE_VALUE;
        other.__pid = static_cast<native_pid_t>(-1);
      }
      return *this;
    }

    /**
     * @brief Copy assignment is disabled.
     */
    UniquePid &operator=(const UniquePid &) = delete;

    /**
     * @brief Wait until the process exits and optionally enforce zero exit codes.
     * @param check When true, non-zero exit codes become errors.
     */
    std::expected<SubprocessResult, SubprocessError> wait(bool check = true) noexcept {
      auto rv = WaitForSingleObject(__process, INFINITE);
      if (rv == WAIT_FAILED) {
        DWORD err = GetLastError();
        _dosmaperr(err);
        return std::unexpected{SubprocessError::from_errcode(errno)};
      }
      return __finalise(check);
    }

    /**
     * @brief Poll the process without blocking, returning completion when available.
     * @param check When true, non-zero exit codes become errors.
     */
    std::expected<std::optional<SubprocessResult>, SubprocessError> wait_non_blocking(
      bool check
    ) noexcept {
      auto rv = WaitForSingleObject(__process, 0);
      if (rv == WAIT_TIMEOUT) {
        return std::optional<SubprocessResult>{std::nullopt};
      }
      if (rv == WAIT_FAILED) {
        DWORD err = GetLastError();
        _dosmaperr(err);
        return std::unexpected{SubprocessError::from_errcode(errno)};
      }
      return __finalise(check).transform([](auto &&res) {
        return std::optional<SubprocessResult>{res};
      });
    }

    /**
     * @brief Send a signal to the process (limited support on Windows).
     * @param sig Signal number to deliver.
     */
    std::expected<std::reference_wrapper<UniquePid>, SubprocessError> send_signal(
      int sig
    ) noexcept {
      if (sig == SIGKILL || sig == SIGTERM) {
        if (!TerminateProcess(__process, 1)) {
          DWORD err = GetLastError();
          _dosmaperr(err);
          return std::unexpected{SubprocessError::from_errcode(errno)};
        }
        return std::ref(*this);
      }
      return std::unexpected{SubprocessError::from_errcode(ENOTSUP)};
    }

    /**
     * @brief Send a signal to the process from a const wrapper.
     * @param sig Signal number to deliver.
     */
    std::expected<std::reference_wrapper<const UniquePid>, SubprocessError> send_signal(
      int sig
    ) const noexcept {
      return const_cast<UniquePid *>(this)->send_signal(sig).transform([&](auto &) {
        return std::cref(*this);
      });
    }

    /**
     * @brief Retrieve the Windows-reported process identifier.
     */
    native_pid_t pid() const noexcept {
      return __pid;
    }

    /**
     * @brief Ensure handle is reclaimed on destruction.
     */
    ~UniquePid() noexcept {
      __destroy();
    }
  };
}
