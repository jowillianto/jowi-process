module;
#include <sys/mman.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <chrono>
#include <coroutine>
#include <csignal>
#include <expected>
#include <functional>
#include <optional>
#include <spawn.h>
#include <thread>
#include <unistd.h>
export module jowi.process:subprocess;
import jowi.io;
import jowi.asio;
export import :subprocess_result;
export import :subprocess_argument;
export import :subprocess_env;
export import :subprocess_error;
export import :unique_pid;
export import :asio;

namespace jowi::process {
  std::optional<int> as_optional_fd(const io::IsFile auto &file) noexcept {
    auto fd = file.handle().fd();
    if (fd == -1) {
      return std::nullopt;
    }
    return fd;
  }
  /**
   * @brief RAII wrapper around a spawned POSIX process with synchronous and async utilities.
   */
  export class Subprocess {
    UniquePid __p;
    /**
     * @brief Adopt an existing process identifier produced by `posix_spawn`.
     * @param pid Process identifier to manage.
     */
    explicit Subprocess(pid_t pid) : __p{pid} {}

  public:
    /**
     * @brief Wait non-blockingly for process completion, returning `std::nullopt` if still running.
     * @param check When true, non-zero exits surface as errors.
     */
    std::expected<std::optional<SubprocessResult>, SubprocessError> wait_non_blocking(
      bool check = true
    ) {
      return __p.wait_non_blocking(check);
    }

    /**
     * @brief Poll the process until the timeout elapses, sleeping briefly between checks.
     * @param timeout Duration to keep polling before giving up.
     * @param check When true, non-zero exits surface as errors.
     */
    std::expected<std::optional<SubprocessResult>, SubprocessError> wait_for(
      std::chrono::milliseconds timeout, bool check = true
    ) noexcept {
      auto tp = std::chrono::system_clock::now() + timeout;
      auto res = __p.wait_non_blocking(check);
      while (res && !res->has_value() && std::chrono::system_clock::now() <= tp) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        res = __p.wait_non_blocking(check);
      }
      return res;
    }
    /**
     * @brief Wait for the timeout and kill the process if it is still running afterwards.
     * @param timeout Maximum duration to wait before killing the process.
     * @param check When true, non-zero exits surface as errors.
     */
    std::expected<SubprocessResult, SubprocessError> wait_or_kill(
      std::chrono::milliseconds timeout, bool check
    ) noexcept {
      using result_type = std::expected<SubprocessResult, SubprocessError>;
      return wait_for(timeout, check).and_then([&](std::optional<SubprocessResult> &&res) {
        if (res) {
          return result_type{res.value()};
        } else {
          return kill_and_wait(check);
        }
      });
    }
    /**
     * @brief Wait blockingly for the process to finish.
     * @param check When true, non-zero exits surface as errors.
     */
    std::expected<SubprocessResult, SubprocessError> wait(bool check = true) noexcept {
      return __p.wait(check);
    }

    /**
     * @brief Create an awaitable that resolves when the process completes.
     * @param check When true, non-zero exits surface as errors.
     */
    asio::InfiniteAwaiter<ProcessWaitPoller> async_wait(bool check = true) noexcept {
      return asio::InfiniteAwaiter<ProcessWaitPoller>{__p, check};
    }

    /**
     * @brief Create an awaitable that polls until timeout before forcing completion.
     * @param timeout Maximum duration to poll before yielding control.
     * @param check When true, non-zero exits surface as errors.
     */
    asio::TimedAwaiter<ProcessWaitPoller> async_wait_for(
      std::chrono::milliseconds timeout, bool check = true
    ) noexcept {
      return asio::TimedAwaiter<ProcessWaitPoller>{timeout, __p, check};
    }
    /**
     * @brief Send a signal to the process and return a reference to this wrapper.
     * @param sig Signal number to deliver.
     */
    std::expected<std::reference_wrapper<Subprocess>, SubprocessError> send_signal(
      int sig
    ) noexcept {
      return __p.send_signal(sig).transform([&](auto &&) { return std::ref(*this); });
    }

    /**
     * @brief Send a signal to the process on a const wrapper and return a reference.
     * @param sig Signal number to deliver.
     */
    std::expected<std::reference_wrapper<const Subprocess>, SubprocessError> send_signal(
      int sig
    ) const noexcept {
      return __p.send_signal(sig).transform([&](auto &&) { return std::ref(*this); });
    }

    /**
     * @brief Send `SIGKILL` to terminate the process.
     */
    std::expected<std::reference_wrapper<Subprocess>, SubprocessError> kill() {
      return send_signal(SIGKILL);
    }

    /**
     * @brief Kill the process and then wait for it to exit.
     * @param check When true, non-zero exits surface as errors.
     */
    std::expected<SubprocessResult, SubprocessError> kill_and_wait(bool check = true) {
      return send_signal(SIGKILL).and_then([&](Subprocess &process) {
        return process.wait(check);
      });
    }

    /**
     * @brief Determine whether the wrapped process can still be waited on.
     */
    bool waitable() const {
      return __p.pid() != -1;
    }
    /**
     * @brief Retrieve the underlying process identifier.
     */
    pid_t pid() const noexcept {
      return __p.pid();
    }

    /**
     * @brief Spawn a new Subprocess using `posix_spawnp` with optional file descriptor overrides.
     * @param args Executable and argument vector to launch.
     * @param out Optional file descriptor duplicated to the child stdout stream.
     * @param in Optional file descriptor duplicated to the child stdin stream.
     * @param err Optional file descriptor duplicated to the child stderr stream.
     * @param env Environment definition to expose to the child process.
     */
    static std::expected<Subprocess, SubprocessError> spawn(
      const SubprocessArgument &args,
      std::optional<int> out = STDOUT_FILENO,
      std::optional<int> in = std::nullopt,
      std::optional<int> err = STDERR_FILENO,
      const SubprocessEnv &env = SubprocessEnv::global_env()
    ) {
      posix_spawn_file_actions_t spawn_action;
      posix_spawnattr_t attributes;
      pid_t pid;
      return sys_call_return_err(posix_spawn_file_actions_init, &spawn_action)
        .and_then([&](auto &&) {
          return out
            .transform([&](int out) {
              return sys_call_return_err(
                posix_spawn_file_actions_adddup2, &spawn_action, out, STDOUT_FILENO
              );
            })
            .value_or(0);
        })
        .and_then([&](auto &&) {
          return in
            .transform([&](int in) {
              return sys_call_return_err(
                posix_spawn_file_actions_adddup2, &spawn_action, in, STDIN_FILENO
              );
            })
            .value_or(0);
        })
        .and_then([&](auto &&) {
          return err
            .transform([&](int out) {
              return sys_call_return_err(
                posix_spawn_file_actions_adddup2, &spawn_action, out, STDERR_FILENO
              );
            })
            .value_or(0);
        })
        .and_then([&](auto &&) { return sys_call_return_err(posix_spawnattr_init, &attributes); })
        .and_then([&](auto &&) {
          return sys_call_return_err(
            posix_spawnp,
            &pid,
            args.exec(),
            &spawn_action,
            &attributes,
            const_cast<char *const *>(args.args()),
            const_cast<char *const *>(env.args())
          );
        })
        .transform([&](auto &&) { return Subprocess{pid}; });
    }

    /**
     * @brief Run a Subprocess, wait for the timeout, and kill if it outlives the deadline.
     * @param args Executable and arguments to launch.
     * @param check When true, non-zero exits surface as errors.
     * @param timeout Maximum time the Subprocess is allowed to run.
     * @param out Optional file descriptor duplicated to the child stdout stream.
     * @param in Optional file descriptor duplicated to the child stdin stream.
     * @param err Optional file descriptor duplicated to the child stderr stream.
     * @param e Environment definition to expose to the child process.
     */
    static std::expected<SubprocessResult, SubprocessError> timed_run(
      const SubprocessArgument &args,
      bool check = true,
      std::chrono::milliseconds timeout = std::chrono::seconds{10},
      std::optional<int> out = STDOUT_FILENO,
      std::optional<int> in = std::nullopt,
      std::optional<int> err = STDERR_FILENO,
      const SubprocessEnv &e = SubprocessEnv::make_env()
    ) {
      return spawn(args, out, in, err, e).and_then([&](Subprocess &&proc) {
        return proc.wait_or_kill(timeout, check);
      });
    }

    /**
     * @brief Run a Subprocess synchronously until completion.
     * @param args Executable and arguments to launch.
     * @param check When true, non-zero exits surface as errors.
     * @param out Optional file descriptor duplicated to the child stdout stream.
     * @param in Optional file descriptor duplicated to the child stdin stream.
     * @param err Optional file descriptor duplicated to the child stderr stream.
     * @param e Environment definition to expose to the child process.
     */
    static std::expected<SubprocessResult, SubprocessError> run(
      const SubprocessArgument &args,
      bool check = true,
      std::optional<int> out = STDOUT_FILENO,
      std::optional<int> in = std::nullopt,
      std::optional<int> err = STDERR_FILENO,
      const SubprocessEnv &e = SubprocessEnv::make_env()
    ) {
      return spawn(args, out, in, err, e).and_then([&](Subprocess &&proc) {
        return proc.wait(check);
      });
    }

    /**
     * @brief Spawn and await a Subprocess using coroutines.
     * @param args Executable and arguments to launch.
     * @param check When true, non-zero exits surface as errors.
     * @param out Optional file descriptor duplicated to the child stdout stream.
     * @param in Optional file descriptor duplicated to the child stdin stream.
     * @param err Optional file descriptor duplicated to the child stderr stream.
     * @param e Environment definition to expose to the child process.
     */
    static asio::BasicTask<std::expected<SubprocessResult, SubprocessError>> async_run(
      const SubprocessArgument &args,
      bool check = true,
      std::optional<int> out = STDOUT_FILENO,
      std::optional<int> in = std::nullopt,
      std::optional<int> err = STDERR_FILENO,
      const SubprocessEnv &e = SubprocessEnv::make_env()
    ) {
      auto proc = spawn(args, out, in, err, e);
      if (!proc) {
        co_return std::unexpected{proc.error()};
      }
      co_return co_await proc->async_wait(check);
    }

    /**
     * @brief Spawn and await a Subprocess with a timeout, killing if it exceeds the limit.
     * @param args Executable and arguments to launch.
     * @param check When true, non-zero exits surface as errors.
     * @param timeout Maximum time the Subprocess is allowed to run.
     * @param out Optional file descriptor duplicated to the child stdout stream.
     * @param in Optional file descriptor duplicated to the child stdin stream.
     * @param err Optional file descriptor duplicated to the child stderr stream.
     * @param e Environment definition to expose to the child process.
     */
    static asio::BasicTask<std::expected<SubprocessResult, SubprocessError>> async_timed_run(
      const SubprocessArgument &args,
      bool check = true,
      std::chrono::milliseconds timeout = std::chrono::seconds{10},
      std::optional<int> out = STDOUT_FILENO,
      std::optional<int> in = std::nullopt,
      std::optional<int> err = STDERR_FILENO,
      const SubprocessEnv &e = SubprocessEnv::make_env()
    ) {
      auto proc = spawn(args, out, in, err, e);
      if (!proc) {
        co_return std::unexpected{proc.error()};
      }
      auto res = co_await proc->async_wait_for(timeout, check);
      if (!res) {
        co_return std::unexpected{res->error()};
      } else if (res->has_value()) {
        co_return res->value();
      }
      co_return proc->kill_and_wait(check);
    }
  };

  /**
   * @brief Convenience wrapper to spawn a Subprocess returning the `Subprocess` handle.
   * @param args Executable and arguments to launch.
   * @param out Optional file descriptor duplicated to the child stdout stream.
   * @param in Optional file descriptor duplicated to the child stdin stream.
   * @param err Optional file descriptor duplicated to the child stderr stream.
   * @param env Environment definition to expose to the child process.
   */
  export std::expected<Subprocess, SubprocessError> spawn(
    const SubprocessArgument &args,
    std::optional<int> out = STDOUT_FILENO,
    std::optional<int> in = std::nullopt,
    std::optional<int> err = STDERR_FILENO,
    const SubprocessEnv &env = SubprocessEnv::global_env()
  ) {
    return Subprocess::spawn(args, out, in, err, env);
  }

  /**
   * @brief Spawn and synchronously wait for a Subprocess using the global environment.
   * @param args Executable and arguments to launch.
   * @param check When true, non-zero exits surface as errors.
   * @param out Optional file descriptor duplicated to the child stdout stream.
   * @param in Optional file descriptor duplicated to the child stdin stream.
   * @param err Optional file descriptor duplicated to the child stderr stream.
   * @param env Environment definition to expose to the child process.
   */
  export std::expected<SubprocessResult, SubprocessError> run(
    const SubprocessArgument &args,
    bool check = true,
    std::optional<int> out = STDOUT_FILENO,
    std::optional<int> in = std::nullopt,
    std::optional<int> err = STDERR_FILENO,
    const SubprocessEnv &env = SubprocessEnv::global_env()
  ) {
    return Subprocess::run(args, check, out, in, err, env);
  }
  /**
   * @brief Spawn and wait with a timeout, killing the process if it exceeds the deadline.
   * @param args Executable and arguments to launch.
   * @param check When true, non-zero exits surface as errors.
   * @param timeout Maximum time the Subprocess is allowed to run.
   * @param out Optional file descriptor duplicated to the child stdout stream.
   * @param in Optional file descriptor duplicated to the child stdin stream.
   * @param err Optional file descriptor duplicated to the child stderr stream.
   * @param env Environment definition to expose to the child process.
   */
  export std::expected<SubprocessResult, SubprocessError> timed_run(
    const SubprocessArgument &args,
    bool check = true,
    std::chrono::milliseconds timeout = std::chrono::seconds{10},
    std::optional<int> out = STDOUT_FILENO,
    std::optional<int> in = std::nullopt,
    std::optional<int> err = STDERR_FILENO,
    const SubprocessEnv &env = SubprocessEnv::global_env()
  ) {
    return Subprocess::timed_run(args, check, timeout, out, in, err, env);
  }
  /**
   * @brief Coroutine helper to spawn and await a Subprocess completion.
   * @param args Executable and arguments to launch.
   * @param check When true, non-zero exits surface as errors.
   * @param out Optional file descriptor duplicated to the child stdout stream.
   * @param in Optional file descriptor duplicated to the child stdin stream.
   * @param err Optional file descriptor duplicated to the child stderr stream.
   * @param env Environment definition to expose to the child process.
   */
  export asio::BasicTask<std::expected<SubprocessResult, SubprocessError>> async_run(
    const SubprocessArgument &args,
    bool check = true,
    std::optional<int> out = STDOUT_FILENO,
    std::optional<int> in = std::nullopt,
    std::optional<int> err = STDERR_FILENO,
    const SubprocessEnv &env = SubprocessEnv::global_env()
  ) {
    return Subprocess::async_run(args, check, out, in, err, env);
  }
  /**
   * @brief Coroutine helper to spawn, await with timeout, and kill lingering processes.
   * @param args Executable and arguments to launch.
   * @param check When true, non-zero exits surface as errors.
   * @param timeout Maximum time the Subprocess is allowed to run.
   * @param out Optional file descriptor duplicated to the child stdout stream.
   * @param in Optional file descriptor duplicated to the child stdin stream.
   * @param err Optional file descriptor duplicated to the child stderr stream.
   * @param env Environment definition to expose to the child process.
   */
  export asio::BasicTask<std::expected<SubprocessResult, SubprocessError>> async_timed_run(
    const SubprocessArgument &args,
    bool check = true,
    std::chrono::milliseconds timeout = std::chrono::seconds{10},
    std::optional<int> out = STDOUT_FILENO,
    std::optional<int> in = std::nullopt,
    std::optional<int> err = STDERR_FILENO,
    const SubprocessEnv &env = SubprocessEnv::global_env()
  ) {
    return Subprocess::async_timed_run(args, check, timeout, out, in, err, env);
  }
  /**
   * @brief Spawn a Subprocess using file wrapper handles for standard streams.
   * @param args Executable and arguments to launch.
   * @param out Optional file wrapper duplicated to the child stdout stream.
   * @param in Optional file wrapper duplicated to the child stdin stream (`fd == -1` disables
   * duplication).
   * @param err Optional file wrapper duplicated to the child stderr stream.
   * @param env Environment definition to expose to the child process.
   */
  export std::expected<Subprocess, SubprocessError> spawn(
    const SubprocessArgument &args,
    const io::IsFile auto &out = io::BasicFile<int>{STDOUT_FILENO},
    const io::IsFile auto &in = io::BasicFile<int>{-1},
    const io::IsFile auto &err = io::BasicFile<int>{STDERR_FILENO},
    const SubprocessEnv &env = SubprocessEnv::global_env()
  ) {
    return Subprocess::spawn(
      args, as_optional_fd(out), as_optional_fd(in), as_optional_fd(err), env
    );
  }

  /**
   * @brief Run a Subprocess synchronously while configuring standard streams via file wrappers.
   * @param args Executable and arguments to launch.
   * @param check When true, non-zero exits surface as errors.
   * @param out Optional file wrapper duplicated to the child stdout stream.
   * @param in Optional file wrapper duplicated to the child stdin stream (`fd == -1` disables
   * duplication).
   * @param err Optional file wrapper duplicated to the child stderr stream.
   * @param env Environment definition to expose to the child process.
   */
  export std::expected<SubprocessResult, SubprocessError> run(
    const SubprocessArgument &args,
    bool check = true,
    const io::IsFile auto &out = io::BasicFile<int>{STDOUT_FILENO},
    const io::IsFile auto &in = io::BasicFile<int>::invalid_file(),
    const io::IsFile auto &err = io::BasicFile<int>{STDERR_FILENO},
    const SubprocessEnv &env = SubprocessEnv::global_env()
  ) {
    return Subprocess::run(
      args, check, as_optional_fd(out), as_optional_fd(in), as_optional_fd(err), env
    );
  }
  /**
   * @brief Run a Subprocess with a timeout while providing file wrappers for standard streams.
   * @param args Executable and arguments to launch.
   * @param check When true, non-zero exits surface as errors.
   * @param timeout Maximum time the Subprocess is allowed to run.
   * @param out Optional file wrapper duplicated to the child stdout stream.
   * @param in Optional file wrapper duplicated to the child stdin stream (`fd == -1` disables
   * duplication).
   * @param err Optional file wrapper duplicated to the child stderr stream.
   * @param env Environment definition to expose to the child process.
   */
  export std::expected<SubprocessResult, SubprocessError> timed_run(
    const SubprocessArgument &args,
    bool check = true,
    std::chrono::milliseconds timeout = std::chrono::seconds{10},
    const io::IsFile auto &out = io::BasicFile<int>{STDOUT_FILENO},
    const io::IsFile auto &in = io::BasicFile<int>::invalid_file(),
    const io::IsFile auto &err = io::BasicFile<int>{STDERR_FILENO},
    const SubprocessEnv &env = SubprocessEnv::global_env()
  ) {
    return Subprocess::timed_run(
      args, check, timeout, as_optional_fd(out), as_optional_fd(in), as_optional_fd(err), env
    );
  }
  /**
   * @brief Coroutine wrapper combining file handles with asynchronous Subprocess execution.
   * @param args Executable and arguments to launch.
   * @param check When true, non-zero exits surface as errors.
   * @param out Optional file wrapper duplicated to the child stdout stream.
   * @param in Optional file wrapper duplicated to the child stdin stream (`fd == -1` disables
   * duplication).
   * @param err Optional file wrapper duplicated to the child stderr stream.
   * @param env Environment definition to expose to the child process.
   */
  export asio::BasicTask<std::expected<SubprocessResult, SubprocessError>> async_run(
    const SubprocessArgument &args,
    bool check = true,
    const io::IsFile auto &out = io::BasicFile<int>{STDOUT_FILENO},
    const io::IsFile auto &in = io::BasicFile<int>::invalid_file(),
    const io::IsFile auto &err = io::BasicFile<int>{STDERR_FILENO},
    const SubprocessEnv &env = SubprocessEnv::global_env()
  ) {
    return Subprocess::async_run(
      args, check, as_optional_fd(out), as_optional_fd(in), as_optional_fd(err), env
    );
  }
  /**
   * @brief Coroutine wrapper that enforces a timeout while using file handles for streams.
   * @param args Executable and arguments to launch.
   * @param check When true, non-zero exits surface as errors.
   * @param timeout Maximum time the Subprocess is allowed to run.
   * @param out Optional file wrapper duplicated to the child stdout stream.
   * @param in Optional file wrapper duplicated to the child stdin stream (`fd == -1` disables
   * duplication).
   * @param err Optional file wrapper duplicated to the child stderr stream.
   * @param env Environment definition to expose to the child process.
   */
  export asio::BasicTask<std::expected<SubprocessResult, SubprocessError>> async_timed_run(
    const SubprocessArgument &args,
    bool check = true,
    std::chrono::milliseconds timeout = std::chrono::seconds{10},
    const io::IsFile auto &out = io::BasicFile<int>{STDOUT_FILENO},
    const io::IsFile auto &in = io::BasicFile<int>::invalid_file(),
    const io::IsFile auto &err = io::BasicFile<int>{STDERR_FILENO},
    const SubprocessEnv &env = SubprocessEnv::global_env()
  ) {
    return Subprocess::async_timed_run(
      args, check, timeout, as_optional_fd(out), as_optional_fd(in), as_optional_fd(err), env
    );
  }

}
