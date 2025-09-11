# Jowi Process
A C++23 Library written to handle spawning subprocesses synchronously. 

## 1. Running a subprocess
First, setup your toolchain to import the module. Then, spawning a subprocess is as easy as the following : 
```cpp
import jowi.process;
#include <expected>

namespace proc = jowi::process;

int main(int argc, const char** argv, const char** env) {
  // Optional : Initialize the current environment
  proc::subprocess_env::init(env);

  // Run a process, example echo something
  std::expected<
    proc::subprocess_result, proc::subprocess_error
  > res = proc::subprocess::run(
    proc::subprocess_argument(
      "echo", 
      "Hello World"
    )
  );
}
```
Conversely, a process can only be spawned without waiting for it : 
```cpp
import jowi.process;
#include <expected>

namespace proc = jowi::process;

int main(int argc, const char** argv, const char** env) {
  // Optional : Initialize the current environment
  proc::subprocess_env::init(env);

  // Run a process, example echo something
  std::expected<
    proc::subprocess, proc::subprocess_error
  > res = proc::subprocess::spawn(
    proc::subprocess_argument(
      "echo", 
      "Hello World"
    )
  );

  // Send Signal
  std::expected<
    std::reference_wrapper<proc::subprocess>, proc::subprocess_error
  > send_res = res -> send_signal(SIGTERM);

  // Kill
  std::expected<
    std::reference_wrapper<proc::subprocess>, proc::subprocess_error
  > kill_res = res -> kill();

  // get the pid
  pid_t pid = res -> pid();

  // wait non blockingly
  std::expected<
    std::optional<proc::subprocess_result>, proc::subprocess_error
  > opt_wait_res = res -> wait_non_blocking();

  std::expected<
    proc::subprocess_result, proc::subprocess_error
  > wait_res = res -> wait();
}
```

## 2. Configuring stdin, stdout, stderr. 
By default, stdin, stdout and stderr is configured to map to the current running process' stdin, stdout and stderr. However, this behaviour is configurable. The following is the call signature for subprocess::run
```
  subprocess:run(
    subprocess_argument args,
    bool check = true,
    int stdout = 0,
    int stdin = 1,
    int stderr = 2,
    const subprocess_env& e = subprocess_env::make_env()
  ) -> std::expected<subprocess_result, subprocess_error>
```
Another method for spawning processes are as follows :
```
  subprocess::spawn(
    subprocess_argument args,
    int stdout = 0,
    int stdin = 1,
    int stderr = 2,
    const subprocess_env& e = subprocess_env::make_env()
  ) -> std::expected<subprocess, subprocess_error>
```
