module;
#include <expected>
#include <optional>
export module jowi.process:asio;
import jowi.asio;
import :subprocess_error;
import :subprocess_result;
import :unique_pid;

namespace jowi::process {
  struct ProcessWaitPoller {
  private:
    UniquePid &__p;
    bool __check;

  public:
    using ValueType = std::expected<SubprocessResult, SubprocessError>;
    ProcessWaitPoller(UniquePid &p, bool check = true) : __p{p}, __check{check} {}

    std::optional<ValueType> poll() {
      auto res = __p.wait_non_blocking(__check);
      if (!res) {
        return std::unexpected{res.error()};
      }
      if (res->has_value()) {
        return res->value();
      }
      return std::nullopt;
    }

    ValueType poll_block() {
      return __p.wait(__check);
    }
  };
}
