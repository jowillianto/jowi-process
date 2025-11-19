# Jowi Process

## Overview
`jowi::process` is a C++23 module that exposes a small set of types for building argument lists, preparing environments, spawning child processes, and inspecting their results. Everything is surfaced through `std::expected`, so subprocess failures travel through normal value channels rather than exceptions.

Import the whole surface from any translation unit that needs to launch work:

```cpp
import jowi.process;
```

The module exports the following pieces:

- `SubprocessArgument` - builds argv-style command lines.
- `SubprocessEnv` - stores the environment that will be shown to children.
- `Subprocess` - RAII handle around a live child process.
- `SubprocessResult` - wrapper over the raw wait status.
- `SubprocessError` - describes failures when spawning or waiting.
- `spawn`, `run`, `timed_run`, and their asynchronous equivalents - helper functions that manufacture `Subprocess` objects for you.

`SubprocessEnv::global_env()` defaults to an empty environment. Call `SubprocessEnv::init(envp)` once near `main` if you want the host environment copied into the global snapshot before you start spawning processes.

## Quick Start
```cpp
import jowi.process;
#include <expected>
#include <print>

namespace proc = jowi::process;

std::expected<void, proc::SubprocessError> run_echo() {
  auto echo = proc::run(proc::SubprocessArgument{"/bin/echo", "Jowi Process"}, true);
  if (!echo) {
    return std::unexpected{echo.error()};
  }
  std::print("child exited with {}\n", echo->exit_code());
  return {};
}

int main(int, const char **, const char **envp) {
  proc::SubprocessEnv::init(envp); // optional, but ensures spawn() sees the host env

  auto done = run_echo();
  if (!done) {
    std::print(stderr, "run failed: {}\n", done.error().what());
    return 1;
  }

  return 0;
}
```

When you need to retain control over the child (to stream data, poll, or signal it), use `spawn` to obtain a `Subprocess` handle and then call the member functions described below.

## API Reference

### SubprocessArgument
`SubprocessArgument` owns the executable path and all arguments passed to `posix_spawnp`/`CreateProcess`. It always exposes a null-terminated view so it can be forwarded directly to OS APIs.

- `SubprocessArgument(std::string exec, Args &&...args)` constructs the command line with any number of additional arguments.
- `add_argument(Args &&...args)` appends more arguments lazily, so you can conditionally build complex commands.
- `add_argument(range)` takes any range whose values are convertible to `std::string`.
- `args()` returns `const char *const *` suitable for `posix_spawnp`.
- `exec()` returns the executable path (`argv[0]`).
- The type is iterable, so you can log or inspect the arguments with range-for loops or formatters.

Example:

```cpp
proc::SubprocessArgument cmd{"/usr/bin/env", "-i"};
if (!extra_path.empty()) {
  cmd.add_argument("--path", extra_path);
}
cmd.add_argument(std::array{"VAR1=1", "VAR2=2"}); // append a range
auto raw = cmd.args(); // safe to pass to posix_spawnp
```

### SubprocessEnv
`SubprocessEnv` manages the environment shown to a child. The class stores a vector of `"NAME=VALUE"` strings and lazily builds a null-terminated list when the OS needs it.

- `static void init(const char **envp)` copies the host environment into the global snapshot. Call it once before you rely on `SubprocessEnv::global_env()`.
- `static SubprocessEnv make_env(bool include_current = true)` returns either a copy of the global snapshot or an empty definition if `include_current == false`.
- `set(name, value)` writes or updates a variable inside the snapshot without touching the host process environment.
- `std::optional<EnvEntry> get(name) const` reads a single entry. `EnvEntry` is a trivial `{name, value}` view over the stored string.
- `const char *const *args() const` exposes the memory in a format compatible with `posix_spawnp`.
- `static const SubprocessEnv &global_env()` grants readonly access to the process-wide snapshot used as the default for all helper functions.

Example:

```cpp
std::expected<void, proc::SubprocessError> print_clean_env(const char **envp) {
  proc::SubprocessEnv::init(envp); // grab host env once
  auto clean_env = proc::SubprocessEnv::make_env(false);
  clean_env.set("JOWI_PROCESS_TEST", "1");
  if (!clean_env.get("PATH")) {
    std::print("PATH intentionally unset\n");
  }

  auto result = proc::run(
    proc::SubprocessArgument{"/usr/bin/env"},
    true,
    std::nullopt,
    std::nullopt,
    std::nullopt,
    clean_env
  );
  if (!result) {
    return std::unexpected{result.error()};
  }

  std::print("env child exited with {}\n", result->exit_code());
  return {};
}
```

### Spawning Helpers
The library exposes a consistent family of helpers for starting processes. Every helper returns `std::expected<..., SubprocessError>` so you can branch on failure without throwing.

- `Subprocess::spawn` and the convenience wrapper `jowi::process::spawn` launch the child and return a `Subprocess` handle immediately so you can manage IO, polling, or signalling.
- `Subprocess::run` / `jowi::process::run` spawn a child and block until it finishes, returning a `SubprocessResult` on success.
- `Subprocess::timed_run` / `jowi::process::timed_run` behave like `run`, but they wait only up to `timeout` (10 s default) before killing a stubborn child with `SIGKILL`.
- `Subprocess::async_run` / `jowi::process::async_run` are available when `JOWI_PROCESS_INTEGRATE_ASIO` is enabled. They return `asio::BasicTask<std::expected<SubprocessResult, SubprocessError>>` so you can `co_await` process completion inside cooperative tasks.
- `Subprocess::async_timed_run` / `jowi::process::async_timed_run` combine coroutine waits with a timeout, cancelling the child by killing it if it stays alive past the deadline.

Common parameters:

- `const SubprocessArgument &args` - executable and argv.
- `bool check` - when true (default), non-zero exit codes become `std::unexpected(SubprocessError)`.
- `std::optional<int> out/in/err` - optional file descriptors duplicated into the child's stdout, stdin, and stderr. When `JOWI_PROCESS_INTEGRATE_IO` is enabled you also get overloads that accept `io::IsOsFile` wrappers instead of bare descriptors.
- `const SubprocessEnv &env` - environment definition, defaulting to `SubprocessEnv::global_env()`. Pass `SubprocessEnv::make_env(false)` when you want a clean slate.
- `std::chrono::milliseconds timeout` - how long to wait before the child is killed (`timed_run` / `async_timed_run`).

Prefer the free functions (`jowi::process::spawn`, etc.) when you simply want to use the global defaults. Reach for the static member versions when calling from generic code where you already hold a `Subprocess` and want to forward parameters through.

### Subprocess
`Subprocess` owns a single child process. The type is move-only, so ownership cannot be duplicated.

- `std::expected<SubprocessResult, SubprocessError> wait(bool check = true)` blocks until the child exits.
- `std::expected<std::optional<SubprocessResult>, SubprocessError> wait_non_blocking(bool check = true)` polls once and returns `std::nullopt` if the child is still running.
- `wait_for(timeout, check)` repeatedly polls (sleeping 10 ms between attempts) until either the process finishes or the timeout expires.
- `wait_or_kill(timeout, check)` tries `wait_for` and, if the process is still alive afterwards, calls `kill_and_wait`.
- `std::expected<std::reference_wrapper<Subprocess>, SubprocessError> send_signal(int sig)` delivers arbitrary POSIX signals and returns a reference to the handle so you can chain calls.
- `std::expected<std::reference_wrapper<Subprocess>, SubprocessError> kill()` sends `SIGKILL`, while `kill_and_wait` combines `kill` and `wait`.
- `bool waitable() const` reports whether the underlying PID is still owned, and `pid_t pid() const` exposes it for logging.
- When `JOWI_PROCESS_INTEGRATE_ASIO` is enabled, `async_wait(check)` and `async_wait_for(timeout, check)` return awaitables that you can `co_await` inside `asio::BasicTask`.

Destroying a `Subprocess` automatically attempts to kill and reap the child if it is still running.

Example:

```cpp
std::expected<void, proc::SubprocessError> wait_for_sleep() {
  auto child = proc::spawn(proc::SubprocessArgument{"/bin/sleep", "2"});
  if (!child) {
    return std::unexpected{child.error()};
  }

  auto maybe_done = child->wait_non_blocking();
  if (!maybe_done) {
    return std::unexpected{maybe_done.error()};
  }
  if (maybe_done->has_value()) {
    std::print("sleep already exited with {}\n", maybe_done->value().exit_code());
    return {};
  }

  auto done = child->wait();
  if (!done) {
    return std::unexpected{done.error()};
  }

  std::print("sleep exited with {}\n", done->exit_code());
  return {};
}
```

### SubprocessResult
`SubprocessResult` wraps the raw status returned by `waitpid`/`GetExitCodeProcess`.

- `is_normal()` reports whether the process exited via `exit`/`return`.
- `is_terminated()` tells you if the child died because it was signalled.
- `is_stopped()` reports whether it is suspended by a signal.
- `int exit_code() const` returns the exit status for normal completions, the signal number for terminated processes, and the stop signal for suspended ones.

Example:

```cpp
std::expected<void, proc::SubprocessError> inspect_status() {
  auto status = proc::run(proc::SubprocessArgument{"/bin/sh", "-c", "kill -TERM $$"}, false);
  if (!status) {
    return std::unexpected{status.error()};
  }

  if (status->is_terminated()) {
    std::print("killed by signal {}\n", status->exit_code());
  } else if (status->is_normal()) {
    std::print("exited with {}\n", status->exit_code());
  }
  return {};
}
```

### SubprocessError
`SubprocessError` records the context for any failure surfaced through the helpers.

- `const char *what() const` formats a short human-readable message.
- `std::optional<int> error_code() const` holds the errno value when a system call fails.
- `std::optional<SubprocessResult> exit_result() const` (and `exit_code() const`) carry information about completed children when `check == true` rejects a non-zero exit status.
- Static helpers `from_errcode` and `from_result` are used internally but are also available if you need to create your own `std::unexpected` values.

Example:

```cpp
std::expected<void, proc::SubprocessError> ensure_failure_reports() {
  auto status = proc::run(proc::SubprocessArgument{"/bin/false"}, true);
  if (!status) {
    const auto &err = status.error();
    std::print("run failed: {}\n", err.what());
    if (auto exit = err.exit_result()) {
      std::print("child exit code: {}\n", exit->exit_code());
    }
    return std::unexpected{err};
  }

  std::print("unexpected success: {}\n", status->exit_code());
  return {};
}
```

## Requirements
- CMake 3.28+ with C++23 module support (`CMAKE_CXX_SCAN_FOR_MODULES` must be enabled).
- POSIX platforms must provide `posix_spawnp`, `waitpid`, and standard signal facilities. Windows builds rely on the equivalent Win32 APIs contained in the platform-specific sources.
- `jowi::generic` is always required. `JOWI_PROCESS_INTEGRATE_IO` and `JOWI_PROCESS_INTEGRATE_ASIO` pull in `jowi::io` and `jowi::asio` respectively when you enable those options.

## Build & Link
```bash
cmake -S . -B build -DJOWI_PROCESS_BUILD_TESTS=ON
cmake --build build
```

From another project:

```cmake
find_package(jowi CONFIG REQUIRED)

add_executable(example main.cpp)
target_link_libraries(example PRIVATE jowi::process)
```

## Worth Noting
- Every helper returns `std::expected`, so you control failure paths explicitly. The module only throws on allocation failure (`std::bad_alloc`).
- `SubprocessEnv::init` is optional; if you skip it, global spawns run with a blank environment by design.

## Testing
Set `JOWI_PROCESS_BUILD_TESTS=ON`, `JOWI_PROCESS_INTEGRATE_IO=ON`, and `JOWI_PROCESS_INTEGRATE_ASIO=ON` to build the unit tests under `tests/`. They cover spawning, IO plumbing, signalling, waiting APIs, and async waits.

## License
Distributed under the MIT License. See [`LICENSE`](LICENSE).
