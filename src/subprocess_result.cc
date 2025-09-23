module;
#include <sys/wait.h>
export module jowi.process:subprocess_result;

namespace jowi::process {
  /**
   * @brief Captures the raw status returned by `waitpid` and exposes helper queries.
   */
  export class subprocess_result {
    int _status;

  public:
    /**
     * @brief Construct the result wrapper from a POSIX wait status value.
     * @param status Raw status supplied by `waitpid` or similar calls.
     */
    subprocess_result(int status) : _status{status} {}
    /**
     * @brief Check whether the process exited normally via `exit` or `return`.
     */
    bool is_normal() const {
      return WIFEXITED(_status);
    }
    /**
     * @brief Check whether the process terminated because it was signalled.
     */
    bool is_terminated() const {
      return WIFSIGNALED(_status);
    }
    /**
     * @brief Check whether the process stopped due to an external signal.
     */
    bool is_stopped() const {
      return WIFSTOPPED(_status);
    }
    /**
     * @brief Retrieve the exit, termination, or stop code associated with the status.
     */
    int exit_code() const {
      if (is_normal()) return WEXITSTATUS(_status);
      else if (is_terminated())
        return WTERMSIG(_status);
      else
        return WSTOPSIG(_status);
    }
  };
}
