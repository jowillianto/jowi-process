module;
#include <sys/mman.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <chrono>
#include <csignal>
#include <expected>
#include <functional>
#include <future>
#include <optional>
#include <print>
#include <spawn.h>
#include <thread>
#include <unistd.h>
export module moderna.process:subprocess;
export import :subprocess_result;
export import :subprocess_argument;
export import :subprocess_env;
export import :subprocess_error;

namespace moderna::process {
  template <typename F, typename... Args>
    requires(std::invocable<F, Args...>)
  std::expected<int, subprocess_error> invoke_syscall(F &&sys_call, Args &&...args) {
    int res = std::invoke(std::forward<F>(sys_call), std::forward<Args>(args)...);
    int err_no = errno;
    if (res == -1) {
      return std::unexpected{subprocess_error::from_errcode(err_no)};
    }
    return res;
  }
  template <typename F, typename... Args>
    requires(std::invocable<F, Args...>)
  std::expected<int, subprocess_error> invoke_zero_syscall(F &&sys_call, Args &&...args) {
    int err_no = std::invoke(std::forward<F>(sys_call), std::forward<Args>(args)...);
    if (err_no == 0) {
      return err_no;
    }
    return std::unexpected{subprocess_error::from_errcode(err_no)};
  };

  export class subprocess {
    pid_t __pid;
    subprocess(pid_t pid) : __pid{pid} {}

    std::expected<subprocess_result, subprocess_error> __check_result(
      subprocess_result res, bool check
    ) {
      if (!check || res.exit_code() == 0) {
        return res;
      } else {
        return std::unexpected{subprocess_error::from_result(std::move(res))};
      }
    }

  public:
    subprocess(const subprocess &) = delete;

    // Delete copy constructor
    subprocess(subprocess &&o) : __pid{o.__pid} {
      o.__pid = -1;
    }
    subprocess &operator=(subprocess &&o) {
      __pid = o.__pid;
      o.__pid = -1;
      return *this;
    }
    subprocess &operator=(const subprocess &o) = delete;

    /*
      wait non blockingly for a process. This will return a result if it is successful or
      an empty optional otherwise
    */
    std::expected<std::optional<subprocess_result>, subprocess_error> wait_non_blocking(
      bool check = true
    ) {
      using expected_type = std::expected<std::optional<subprocess_result>, subprocess_error>;
      int status = 0;
      return invoke_syscall(waitpid, __pid, &status, WNOHANG)
        .and_then([&](int wait_status) -> expected_type {
          if (wait_status == 0) {
            return expected_type{std::optional<subprocess_result>{std::nullopt}};
          } else {
            __pid = -1;
            return __check_result(subprocess_result{status}, check);
          }
        });
    }

    /*
      First, checks if the process have finished executing, and then waits for the timeout to pass
      and checks the process again.
    */
    std::expected<std::optional<subprocess_result>, subprocess_error> wait_for(
      std::chrono::milliseconds timeout, bool check = true
    ) {
      using result_type = std::expected<std::optional<subprocess_result>, subprocess_error>;
      return wait_non_blocking(check).and_then([&](std::optional<subprocess_result> &&res) {
        if (res) {
          return result_type{res};
        } else {
          std::this_thread::sleep_for(timeout);
          return wait_non_blocking(check);
        }
      });
    }
    /*
      Waits for the timeout to pass with wait_for and then kills the process if it has not finished
      executing.
    */
    std::expected<subprocess_result, subprocess_error> wait_or_kill(
      std::chrono::milliseconds timeout, bool check
    ) {
      using result_type = std::expected<subprocess_result, subprocess_error>;
      return wait_for(timeout, check).and_then([&](std::optional<subprocess_result> &&res) {
        if (res) {
          return result_type{res.value()};
        } else {
          return kill_and_wait(check);
        }
      });
    }
    /*
      wait blockingly for the process to finish.
    */
    std::expected<subprocess_result, subprocess_error> wait(bool check = true) {
      int status = 0;
      return invoke_syscall(waitpid, __pid, &status, 0).and_then([&](int wait_status) {
        __pid = -1;
        return __check_result(subprocess_result{status}, check);
      });
    }
    /*
      sends a signal to the process
    */
    std::expected<std::reference_wrapper<subprocess>, subprocess_error> send_signal(int sig) {
      return invoke_syscall(::kill, __pid, sig).transform([&](int res) { return std::ref(*this); });
    }

    /*
      kills the process
    */
    std::expected<std::reference_wrapper<subprocess>, subprocess_error> kill() {
      return send_signal(SIGKILL);
    }

    /*
      kills and waits for the process
    */
    std::expected<subprocess_result, subprocess_error> kill_and_wait(bool check = true) {
      return send_signal(SIGKILL).and_then([&](subprocess &process) { return process.wait(check); }
      );
    }

    /*
      checks if the current process is waitable. i.e. check if it has been waited before.
    */
    bool waitable() const {
      return __pid != -1;
    }
    ~subprocess() {
      if (waitable()) {
        auto res = send_signal(SIGKILL).and_then([](subprocess &p) { return p.wait(); });
        if (!res) std::println("WARNING: subprocess destruction error : {}", res.error().what());
      }
    }
    pid_t pid() const noexcept {
      return __pid;
    }

    /*
      spawns a new process.
    */
    static std::expected<subprocess, subprocess_error> spawn(
      const subprocess_argument &args,
      int stdout = 0,
      int stdin = 1,
      int stderr = 2,
      const subprocess_env &env = subprocess_env::global_env()
    ) {
      posix_spawn_file_actions_t spawn_action;
      posix_spawnattr_t attributes;
      pid_t pid;
      return invoke_zero_syscall(posix_spawn_file_actions_init, &spawn_action)
        .and_then([&](auto &&) {
          return invoke_zero_syscall(
            posix_spawn_file_actions_adddup2, &spawn_action, stdout, STDOUT_FILENO
          );
        })
        .and_then([&](auto &&) {
          return invoke_zero_syscall(
            posix_spawn_file_actions_adddup2, &spawn_action, stdin, STDIN_FILENO
          );
        })
        .and_then([&](auto &&) {
          return invoke_zero_syscall(
            posix_spawn_file_actions_adddup2, &spawn_action, stderr, STDERR_FILENO
          );
        })
        .and_then([&](auto &&) { return invoke_zero_syscall(posix_spawnattr_init, &attributes); })
        .and_then([&](auto &&) {
          return invoke_zero_syscall(
            posix_spawnp,
            &pid,
            args.exec(),
            &spawn_action,
            &attributes,
            const_cast<char *const *>(args.args()),
            const_cast<char *const *>(env.args())
          );
        })
        .transform([&](auto &&) { return subprocess{pid}; });
    }

    /*
      Runs a process for timeout and then kills if it does not finish executing.
    */
    static std::expected<subprocess_result, subprocess_error> timed_run(
      const subprocess_argument &args,
      bool check = true,
      std::chrono::milliseconds timeout = std::chrono::seconds{10},
      int stdout = 0,
      int stdin = 1,
      int stderr = 2,
      const subprocess_env &e = subprocess_env::make_env()
    ) {
      return spawn(args, stdout, stdin, stderr, e).and_then([&](subprocess &&proc) {
        return proc.wait_or_kill(timeout, check);
      });
    }

    /*
      Runs a process and wait for it to finish
    */
    static std::expected<subprocess_result, subprocess_error> run(
      const subprocess_argument &args,
      bool check = true,
      int stdout = 0,
      int stdin = 1,
      int stderr = 2,
      const subprocess_env &e = subprocess_env::make_env()
    ) {
      return spawn(args, stdout, stdin, stderr, e).and_then([&](subprocess &&proc) {
        return proc.wait(check);
      });
    }
  };

  export std::expected<subprocess, subprocess_error> spawn(
    const subprocess_argument &args,
    int stdout = 0,
    int stdin = 1,
    int stderr = 2,
    const subprocess_env &env = subprocess_env::global_env()
  ) {
    return subprocess::spawn(args, stdout, stdin, stderr, env);
  }

  export std::expected<subprocess_result, subprocess_error> run(
    const subprocess_argument &args,
    bool check = true,
    int stdout = 0,
    int stdin = 1,
    int stderr = 2,
    const subprocess_env &env = subprocess_env::global_env()
  ) {
    return subprocess::run(args, check, stdout, stdin, stderr, env);
  }
  export std::expected<subprocess_result, subprocess_error> timed_run(
    const subprocess_argument &args,
    bool check = true,
    std::chrono::milliseconds timeout = std::chrono::seconds{10},
    int stdout = 0,
    int stdin = 1,
    int stderr = 2,
    const subprocess_env &env = subprocess_env::global_env()
  ) {
    return subprocess::run(args, check, stdout, stdin, stderr, env);
  }
}