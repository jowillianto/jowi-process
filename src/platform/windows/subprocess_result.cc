module;
#include <cstdint>
export module jowi.process:subprocess_result;

namespace jowi::process {
  /**
   * @brief Captures the raw exit code returned by Windows process APIs.
   */
  export class SubprocessResult {
    int _exit_code;

  public:
    /**
     * @brief Construct the result wrapper from a Windows exit code.
     * @param status Code reported by `GetExitCodeProcess`.
     */
    explicit SubprocessResult(int status) : _exit_code{status} {}
    /**
     * @brief Windows processes report a single exit code, so assume normal termination.
     */
    bool is_normal() const {
      return true;
    }
    /**
     * @brief Windows does not expose signal-style termination.
     */
    bool is_terminated() const {
      return false;
    }
    /**
     * @brief Windows does not expose stop semantics.
     */
    bool is_stopped() const {
      return false;
    }
    /**
     * @brief Retrieve the exit code reported by Windows.
     */
    int exit_code() const {
      return _exit_code;
    }
  };
}
