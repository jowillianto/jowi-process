module;
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <chrono>
#include <coroutine>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <functional>
#include <io.h>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
export module jowi.process:subprocess;
#ifdef JOWI_PROCESS_INTEGRATE_IO
import jowi.io;
#endif
import jowi.asio;
export import :subprocess_result;
export import :subprocess_argument;
export import :subprocess_env;
export import :subprocess_error;
export import :unique_pid;
export import :asio;

namespace jowi::process {
  namespace {
    SubprocessError win32_last_error() {
      DWORD err = GetLastError();
      _dosmaperr(err);
      return SubprocessError::from_errcode(errno);
    }

    std::string quote_argument(std::string_view arg) {
      if (arg.empty()) {
        return "\"\"";
      }
      bool needs_quotes = arg.find_first_of(" \t\"") != std::string_view::npos;
      if (!needs_quotes) {
        return std::string{arg};
      }
      std::string quoted;
      quoted.reserve(arg.size() + 2);
      quoted.push_back('"');
      size_t backslashes = 0;
      for (char c : arg) {
        if (c == '\\') {
          backslashes += 1;
        } else if (c == '"') {
          quoted.append(backslashes * 2 + 1, '\\');
          quoted.push_back('"');
          backslashes = 0;
        } else {
          if (backslashes) {
            quoted.append(backslashes, '\\');
            backslashes = 0;
          }
          quoted.push_back(c);
        }
      }
      if (backslashes) {
        quoted.append(backslashes * 2, '\\');
      }
      quoted.push_back('"');
      return quoted;
    }

    std::string build_command_line(const SubprocessArgument &args) {
      std::string command_line;
      const char *const *argv = args.args();
      if (!argv || !*argv) {
        return command_line;
      }
      command_line = quote_argument(*argv);
      for (auto current = argv + 1; *current != nullptr; ++current) {
        command_line.push_back(' ');
        command_line += quote_argument(*current);
      }
      return command_line;
    }

    std::vector<char> build_environment_block(const SubprocessEnv &env) {
      std::vector<char> block;
      const char *const *envp = env.args();
      if (envp == nullptr) {
        block.push_back('\0');
        block.push_back('\0');
        return block;
      }
      while (*envp != nullptr) {
        auto value = *envp;
        auto len = std::strlen(value);
        block.insert(block.end(), value, value + len);
        block.push_back('\0');
        envp += 1;
      }
      if (block.empty()) {
        block.push_back('\0');
      }
      block.push_back('\0');
      return block;
    }

    std::expected<HANDLE, SubprocessError> fd_to_handle(int fd) {
      intptr_t os_handle = _get_osfhandle(fd);
      if (os_handle == -1) {
        return std::unexpected{SubprocessError::from_errcode(errno)};
      }
      return reinterpret_cast<HANDLE>(os_handle);
    }

    struct HandleRestorer {
      std::vector<HANDLE> handles;
      ~HandleRestorer() {
        for (HANDLE handle : handles) {
          if (handle != INVALID_HANDLE_VALUE) {
            SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0);
          }
        }
      }
    };
  }

  /**
   * @brief RAII wrapper around a spawned Windows process with synchronous and async utilities.
   */
  export class Subprocess {
    UniquePid __p;

    explicit Subprocess(HANDLE handle, pid_t pid) : __p{handle, pid} {}

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
     * @param timeout Maximum time to poll before yielding control.
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
      return __p.pid() != static_cast<pid_t>(-1);
    }
    /**
     * @brief Retrieve the underlying process identifier.
     */
    pid_t pid() const noexcept {
      return __p.pid();
    }

    /**
     * @brief Convert optional POSIX-style file descriptors into inheritable Windows handles.
     * @param fd Descriptor to convert (negative values are ignored).
     * @param target Startup info field receiving the handle.
     * @param restorer Tracks handles to reset inheritance flags on exit.
     * @param inherits Flag toggled when any stream is redirected.
     */
    static std::expected<void, SubprocessError> configure_stream(
      const std::optional<int> &fd,
      HANDLE &target,
      HandleRestorer &restorer,
      bool &inherits
    ) {
      if (!fd || *fd == -1) {
        return {};
      }
      auto handle = fd_to_handle(*fd);
      if (!handle) {
        return std::unexpected{handle.error()};
      }
      DWORD flags = 0;
      if (!GetHandleInformation(*handle, &flags)) {
        return std::unexpected{win32_last_error()};
      }
      if ((flags & HANDLE_FLAG_INHERIT) == 0) {
        if (!SetHandleInformation(*handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
          return std::unexpected{win32_last_error()};
        }
        restorer.handles.push_back(*handle);
      }
      target = *handle;
      inherits = true;
      return {};
    }

    /**
     * @brief Spawn a new Subprocess using the Windows `CreateProcess` API.
     * @param args Executable and argument vector to launch.
     * @param out Optional file descriptor duplicated to the child stdout stream.
     * @param in Optional file descriptor duplicated to the child stdin stream.
     * @param err Optional file descriptor duplicated to the child stderr stream.
     * @param env Environment definition to expose to the child process.
     */
    static std::expected<Subprocess, SubprocessError> spawn(
      const SubprocessArgument &args,
      std::optional<int> out = std::nullopt,
      std::optional<int> in = std::nullopt,
      std::optional<int> err = std::nullopt,
      const SubprocessEnv &env = SubprocessEnv::global_env()
    ) {
      STARTUPINFOA startup_info{};
      startup_info.cb = sizeof(startup_info);
      startup_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
      startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
      startup_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
      HandleRestorer restorer{};
      bool inherit_handles = false;
      if (auto res = configure_stream(out, startup_info.hStdOutput, restorer, inherit_handles);
          !res) {
        return std::unexpected{res.error()};
      }
      if (auto res = configure_stream(in, startup_info.hStdInput, restorer, inherit_handles); !res) {
        return std::unexpected{res.error()};
      }
      if (auto res = configure_stream(err, startup_info.hStdError, restorer, inherit_handles); !res) {
        return std::unexpected{res.error()};
      }
      if (inherit_handles) {
        startup_info.dwFlags |= STARTF_USESTDHANDLES;
      }

      auto command_line = build_command_line(args);
      std::vector<char> command_buffer(command_line.begin(), command_line.end());
      command_buffer.push_back('\0');
      auto env_block = build_environment_block(env);

      PROCESS_INFORMATION proc_info{};
      BOOL created = CreateProcessA(
        args.exec(),
        command_buffer.data(),
        nullptr,
        nullptr,
        inherit_handles ? TRUE : FALSE,
        0,
        env_block.data(),
        nullptr,
        &startup_info,
        &proc_info
      );
      if (!created) {
        return std::unexpected{win32_last_error()};
      }
      CloseHandle(proc_info.hThread);
      return Subprocess{proc_info.hProcess, proc_info.dwProcessId};
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
      std::optional<int> out = std::nullopt,
      std::optional<int> in = std::nullopt,
      std::optional<int> err = std::nullopt,
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
      std::optional<int> out = std::nullopt,
      std::optional<int> in = std::nullopt,
      std::optional<int> err = std::nullopt,
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
      std::optional<int> out = std::nullopt,
      std::optional<int> in = std::nullopt,
      std::optional<int> err = std::nullopt,
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
      std::optional<int> out = std::nullopt,
      std::optional<int> in = std::nullopt,
      std::optional<int> err = std::nullopt,
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
    std::optional<int> out = std::nullopt,
    std::optional<int> in = std::nullopt,
    std::optional<int> err = std::nullopt,
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
    std::optional<int> out = std::nullopt,
    std::optional<int> in = std::nullopt,
    std::optional<int> err = std::nullopt,
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
    std::optional<int> out = std::nullopt,
    std::optional<int> in = std::nullopt,
    std::optional<int> err = std::nullopt,
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
    std::optional<int> out = std::nullopt,
    std::optional<int> in = std::nullopt,
    std::optional<int> err = std::nullopt,
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
    std::optional<int> out = std::nullopt,
    std::optional<int> in = std::nullopt,
    std::optional<int> err = std::nullopt,
    const SubprocessEnv &env = SubprocessEnv::global_env()
  ) {
    return Subprocess::async_timed_run(args, check, timeout, out, in, err, env);
  }

#ifdef JOWI_PROCESS_INTEGRATE_IO
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
    const io::IsOsFile auto &out = io::BasicOsFile{-1},
    const io::IsOsFile auto &in = io::BasicOsFile{-1},
    const io::IsOsFile auto &err = io::BasicOsFile{-1},
    const SubprocessEnv &env = SubprocessEnv::global_env()
  ) {
    return Subprocess::spawn(
      args, out.native_handle(), in.native_handle(), err.native_handle(), env
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
    const io::IsOsFile auto &out = io::BasicOsFile{-1},
    const io::IsOsFile auto &in = io::BasicOsFile{-1},
    const io::IsOsFile auto &err = io::BasicOsFile{-1},
    const SubprocessEnv &env = SubprocessEnv::global_env()
  ) {
    return Subprocess::run(
      args, check, out.native_handle(), in.native_handle(), err.native_handle(), env
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
    const io::IsOsFile auto &out = io::BasicOsFile{-1},
    const io::IsOsFile auto &in = io::BasicOsFile{-1},
    const io::IsOsFile auto &err = io::BasicOsFile{-1},
    const SubprocessEnv &env = SubprocessEnv::global_env()
  ) {
    return Subprocess::timed_run(
      args, check, timeout, out.native_handle(), in.native_handle(), err.native_handle(), env
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
    const io::IsOsFile auto &out = io::BasicOsFile{-1},
    const io::IsOsFile auto &in = io::BasicOsFile{-1},
    const io::IsOsFile auto &err = io::BasicOsFile{-1},
    const SubprocessEnv &env = SubprocessEnv::global_env()
  ) {
    return Subprocess::async_run(
      args, check, out.native_handle(), in.native_handle(), err.native_handle(), env
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
    const io::IsOsFile auto &out = io::BasicOsFile{-1},
    const io::IsOsFile auto &in = io::BasicOsFile{-1},
    const io::IsOsFile auto &err = io::BasicOsFile{-1},
    const SubprocessEnv &env = SubprocessEnv::global_env()
  ) {
    return Subprocess::async_timed_run(
      args, check, timeout, out.native_handle(), in.native_handle(), err.native_handle(), env
    );
  }
#endif

}
