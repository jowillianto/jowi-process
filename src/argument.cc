module;
#include <algorithm>
#include <array>
#include <concepts>
#include <string>
#include <vector>
export module moderna.process:argument;

namespace moderna::process {
  export template <class argument_type>
  concept is_argument = requires(const argument_type arg) {
    { arg.args() } -> std::same_as<const char *const *>;
    { arg.exec() } -> std::same_as<const char *>;
  };

  export template <size_t N> class static_argument {
    std::array<std::string, N> __args;
    std::array<const char *, N + 1> __converted_args;

    void __make_converted() {
      std::ranges::transform(__args, __converted_args.begin(), [](const auto &s) {
        return s.c_str();
      });
      __converted_args[N] = nullptr;
    }

  public:
    template <typename... Args>
      requires(sizeof...(Args) == N && (std::is_constructible_v<std::string, Args> && ...))
    static_argument(Args &&...args) : __args{{std::string{std::forward<Args>(args)}...}} {
      __make_converted();
    }
    static_argument(const char *const (&args)[N]) {
      for (auto i = 0; i != N; i += 1) {
        __args[i] = args[i];
      }
      __make_converted();
    }
    const char *const *args() const noexcept {
      return __converted_args.data();
    }
    const char *exec() const noexcept {
      return __converted_args[0];
    }
  };

  export class dynamic_argument {
    std::string __exec;
    std::vector<std::string> __args;
    mutable std::vector<const char *> __converted_args;

    void __make_converted() const {
      __converted_args.clear();
      __converted_args.reserve(__args.size() + 2);
      auto it = std::back_inserter(__converted_args);
      *it = __exec.c_str();
      std::ranges::transform(__args, it, &std::string::c_str);
      *it = nullptr;
    }

  public:
    dynamic_argument(std::string exec) : __exec{std::move(exec)}, __args{}, __converted_args{} {}
    dynamic_argument(std::string exec, std::vector<std::string> args) :
      __exec{std::move(exec)}, __args{std::move(args)} {}

    dynamic_argument &add_argument(std::string v) {
      __args.emplace_back(std::move(v));
      return *this;
    }

    const char *const *args() const {
      __make_converted();
      return __converted_args.data();
    }
    const char *exec() const noexcept {
      return __exec.c_str();
    }
  };

  export template <class... Args>
    requires(std::is_constructible_v<std::string, Args> && ...)
  static_argument<sizeof...(Args)> make_argument(Args &&...args) {
    return static_argument<sizeof...(Args)>{std::string{std::forward<Args>(args)}...};
  }

  static_assert(is_argument<static_argument<10>>);
  static_assert(is_argument<dynamic_argument>);
}