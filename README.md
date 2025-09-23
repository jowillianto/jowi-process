# Jowi Process

## Overview
`jowi::process` delivers a C++23 module for launching and supervising POSIX subprocesses with modern C++ primitives. The public surface focuses on four components:

- `subprocess` – synchronous and coroutine-based process management.
- `subprocess_env` – environment snapshots and overrides passed to child processes.
- `subprocess_result` – inspection utilities for exit statuses.
- `subprocess_error` – rich error information surfaced through `std::expected`.

Each is exported through the umbrella module:

```cpp
import jowi.process;
```

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

## API Highlights
### subprocess
`subprocess` wraps process creation and waiting. The helpers `spawn`, `run`, `timed_run`, `async_run`, and `async_timed_run` all return `std::expected` objects that either hold a result or a `subprocess_error`.

```cpp
import jowi.process;
#include <expected>
#include <print>

using namespace jowi::process;

int main(int, const char **, const char **envp) {
  subprocess_env::init(envp); // call once near program start if you need the ambient environment

  auto status = run(subprocess_argument{"echo", "Hello", "Process"});
  if (!status) {
    std::print(stderr, "spawn failed: {}\n", status.error().what());
    return 1;
  }

  std::print("exit code: {}\n", status->exit_code());
  return 0;
}
```

Coroutines integrate through the `async_run` and `async_timed_run` factories:

```cpp
import jowi.process;
import jowi.asio; // provides asio::basic_task
#include <print>

asio::basic_task<void> gather_info() {
  using namespace jowi::process;

  auto result = co_await async_timed_run(
    subprocess_argument{"uname", "-a"},
    true,
    std::chrono::seconds{2}
  );

  if (!result) {
    throw result.error();
  }
}
```

### subprocess_env
`subprocess_env` captures environment variables passed to a child. Build an instance with `make_env`, adjust values with `set`, and pass it into any `subprocess` factory.

```cpp
using namespace jowi::process;

subprocess_env env = subprocess_env::make_env();
env.set("MY_FLAG", "1");

auto res = run(subprocess_argument{"printenv", "MY_FLAG"}, true, 0, 1, 2, env);
```

### subprocess_result
When a process completes successfully, the helper exposes queries about the exit reason.

```cpp
if (status->is_normal()) {
  std::print("normal exit -> {}\n", status->exit_code());
} else if (status->is_terminated()) {
  std::print("terminated by signal -> {}\n", status->exit_code());
}
```

### subprocess_error
Failures are represented as `subprocess_error`. You can either read the message via `what()` or inspect the stored data.

```cpp
auto maybe_proc = spawn(subprocess_argument{"/bin/false"});
if (!maybe_proc) {
  if (auto code = maybe_proc.error().error_code()) {
    std::print(stderr, "errno: {}\n", *code);
  }
}
```

## Worth Noting
- Library functions report failures through `std::expected`; they do not throw exceptions except for `std::bad_alloc`, which would typically terminate the program.

## Testing
Set `JOWI_PROCESS_BUILD_TESTS=ON` to compile the unit tests under `tests/`. They exercise spawning, waiting, signalling, and environment wiring.

## License
Distributed under the MIT License. See [`LICENSE`](LICENSE).
