module;
#include <sys/wait.h>
export module moderna.process:subprocess_result;

namespace moderna::process {
  export class subprocess_result {
    int _status;

  public:
    subprocess_result(int status) : _status{status} {}
    /*
      Checks if the process exits normally.
    */
    bool is_normal() const {
      return WIFEXITED(_status);
    }
    /*
      Checks if the process is terminated by an external signal.
    */
    bool is_terminated() const {
      return WIFSIGNALED(_status);
    }
    /*
      Checks if the process is stopped by an external signal.
    */
    bool is_stopped() const {
      return WIFSTOPPED(_status);
    }
    /*
      Get the process exit code.
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