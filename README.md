# Jowi Process

## Overview
`jowi::process` provides a C++23 module for launching and supervising POSIX subprocesses with modern C++ primitives. The public surface revolves around four pieces:

- `subprocess` – synchronous handles that let you spawn, wait, and signal processes.
- `subprocess_env` – captured environment snapshots that you pass to child processes.
- `subprocess_result` – exit status inspection helpers.
- `subprocess_error` – rich failure information returned through `std::expected`.

Import everything at once with:

```cpp
import jowi.process;
```

## Running Subprocesses
The library exposes factory helpers that return `std::expected` values. Successful calls yield either a `subprocess_result` (completed process) or a live `subprocess` handle you can manage.

### Synchronous helpers
Use `run` when you want to block until completion and capture stdio or exit information directly:

```cpp
import jowi.process;
#include <expected>
#include <print>

namespace proc = jowi::process;

int main(int, const char **, const char **envp) {
  proc::subprocess_env::init(envp); // initialise once near program start if you need the ambient environment

  auto status = proc::run(proc::subprocess_argument{"echo", "Hello", "Process"});
  if (!status) {
    std::print(stderr, "run failed: {}\n", status.error().what());
    return 1;
  }

  std::print("exit code: {}\n", status->exit_code());
  return 0;
}
```

When you need to retain control over the process, `spawn` gives you a `subprocess` handle:

```cpp
import jowi.process;

namespace proc = jowi::process;

auto child = proc::spawn(proc::subprocess_argument{"sleep", "10"});
if (!child) {
  throw child.error();
}

// do other work ...
auto result = child->wait();
```

`timed_run` blocks with a timeout, while `wait_non_blocking` lets you poll the state of a spawned process without blocking the caller thread.

### Asynchronous helpers
`async_run` and `async_timed_run` integrate with `asio::basic_task`, enabling cooperative multitasking:

```cpp
import jowi.asio;
import jowi.process;

namespace proc = jowi::process;

asio::basic_task<void> query_uname() {
  auto maybe_result = co_await proc::async_timed_run(
    proc::subprocess_argument{"uname", "-a"},
    true,
    std::chrono::seconds{2}
  );

  if (!maybe_result) {
    throw maybe_result.error();
  }

  co_return;
}
```

You can launch multiple asynchronous waits at the same time with `asio::gather` to confirm subprocesses progress in parallel.

## Signals and Lifecycle Management
Once you hold a `subprocess`, you can manipulate the child:

- `send_signal(int sig)` forwards POSIX signals and returns a reference to the handle.
- `terminate()` and `kill()` are convenience wrappers that send `SIGTERM`/`SIGKILL`.
- `wait()` blocks for completion and produces a `subprocess_result`.
- `wait_non_blocking()` polls the state and returns an `std::optional<subprocess_result>`.

These helpers also surface errors via `std::expected<…, subprocess_error>` so you can react to permission issues or missing processes.

## Interpreting Results and Errors
`subprocess_result` makes it easy to inspect how the process exited:

```cpp
if (result.is_normal()) {
  std::print("normal exit -> {}\n", result.exit_code());
} else if (result.is_terminated()) {
  std::print("terminated by signal -> {}\n", result.exit_code());
}
```

Remember that `exit_code()` reflects the exit status for normal completions and the signal number when termination is signal-driven.

When a helper fails, you receive a `subprocess_error` that records a descriptive message and (when applicable) the underlying `std::error_code`. Check `what()` for a formatted string or `error_code()` for the raw errno-style value so you can branch on failure modes.

## Working with `subprocess_env`
Call `subprocess_env::init(envp)` once near the top of `main` to capture the host environment. Afterwards you can clone and customise environments with `make_env`:

```cpp
import jowi.io;
import jowi.process;
#include <expected>
#include <print>

namespace proc = jowi::process;
namespace io = jowi::io;

int main(int, const char **, const char **envp) {
  proc::subprocess_env::init(envp); // capture the host environment once

  proc::subprocess_env env = proc::subprocess_env::make_env();
  env.set("MY_FLAG", "1");

  auto result = proc::run(
    proc::subprocess_argument{"printenv", "MY_FLAG"},
    true,
    io::BasicFile<int>{1},  // forward child stdout to parent stdout
    io::BasicFile<int>{0},  // use parent stdin
    io::BasicFile<int>{2},  // use parent stderr
    env
  );
  if (!result) {
    throw result.error();
  }

  std::print("env exit code: {}\n", result->exit_code());
}
```

Each subprocess helper accepts an environment parameter, letting you isolate test processes or override values without mutating globals. Reusing the cached environment avoids repeated parsing of `environ` and keeps your startup-time initialisation explicit.

## Requirements
- CMake 3.28+ with C++23 module support (`CMAKE_CXX_SCAN_FOR_MODULES` is enabled in the project).
- A POSIX platform that provides `posix_spawnp`, `waitpid`, and signals.
- Companion libraries `jowi::generic`, `jowi::io`, and `jowi::asio` are fetched automatically via the top-level CMake configuration.

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
- All operations surface errors through `std::expected`; the module never throws, apart from `std::bad_alloc`, which indicates allocation failure serious enough to justify letting the program terminate.

## Testing
Set `JOWI_PROCESS_BUILD_TESTS=ON` to compile the unit tests under `tests/`. They exercise spawning, waiting, signalling, and environment wiring.

## License
Distributed under the MIT License. See [`LICENSE`](LICENSE).
