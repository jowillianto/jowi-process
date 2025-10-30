module;
#include <expected>
#include <optional>
export module jowi.process:asio;
import jowi.asio;
import :subprocess_error;
import :subprocess_result;
import :unique_pid;

namespace jowi::process {
  struct process_wait_poller {
  private:
    unique_pid &__p;
    bool __check;

  public:
    using value_type = std::expected<subprocess_result, subprocess_error>;
    process_wait_poller(unique_pid &p, bool check = true) : __p{p}, __check{check} {}

    std::optional<value_type> poll() {
      auto res = __p.wait_non_blocking(__check);
      if (!res) {
        return std::unexpected{res.error()};
      }
      if (res->has_value()) {
        return res->value();
      }
      return std::nullopt;
    }

    value_type poll_block() {
      return __p.wait(__check);
    }
  };
}
