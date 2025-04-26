module;
#include <sys/mman.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <csignal>
#include <expected>
#include <optional>
#include <print>
#include <spawn.h>
#include <unistd.h>
export module moderna.process;
export import :subprocess_result;
export import :argument;
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

    std::expected<subprocess_result, subprocess_error> __check_result(subprocess_result res) {
      if (res.exit_code() == 0) {
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

    // Subprocess operation
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
            return __check_result(subprocess_result{status});
          }
        });
    }
    std::expected<subprocess_result, subprocess_error> wait(bool check = true) {
      int status = 0;
      return invoke_syscall(waitpid, __pid, &status, 0).and_then([&](int wait_status) {
        __pid = -1;
        return __check_result(subprocess_result{status});
      });
    }
    std::expected<std::reference_wrapper<subprocess>, subprocess_error> send_signal(int sig) {
      return invoke_syscall(::kill, __pid, sig).transform([&](int res) { return std::ref(*this); });
    }
    auto kill() {
      return send_signal(SIGKILL);
    }
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
      Friend Functions
    */
    static std::expected<subprocess, subprocess_error> spawn(
      const is_argument auto &args,
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

    static std::expected<subprocess_result, subprocess_error> run(
      const is_argument auto &args,
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
}