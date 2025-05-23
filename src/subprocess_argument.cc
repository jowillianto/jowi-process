module;
#include <format>
#include <string>
#include <type_traits>
#include <vector>
export module moderna.process:subprocess_argument;

namespace moderna::process {
  export class subprocess_argument {
    std::string __exec;
    std::vector<std::string> __args;
    mutable std::vector<const char *> __converted;

  public:
    template <typename... Args>
      requires(std::is_constructible_v<std::string, Args> && ...)
    subprocess_argument(std::string exec, Args &&...args) : __exec{exec} {
      __args.reserve(sizeof...(Args));
      (__args.emplace_back(std::string{std::forward<Args>(args)}), ...);
    }

    template <typename... Args>
      requires(std::is_constructible_v<std::string, Args> && ...)
    subprocess_argument &add_argument(Args &&...args) {
      (__args.emplace_back(std::string{std::forward<Args>(args)}), ...);
      return *this;
    }
    template <std::ranges::range range_type>
      requires(std::is_constructible_v<std::string, std::ranges::range_value_t<range_type>>)
    subprocess_argument &add_argument(range_type &&range) {
      for (const auto &value : range) {
        __args.emplace_back(std::string{value});
      }
      return *this;
    }
    const char *const *args() const noexcept {
      __converted.clear();
      __converted.reserve(__args.size() + 2);
      auto it = std::back_inserter(__converted);
      *it = __exec.c_str();
      std::ranges::transform(__args, it, &std::string::c_str);
      *it = nullptr;
      return __converted.data();
    }
    const char *exec() const noexcept {
      return __exec.c_str();
    }

    constexpr auto begin() const noexcept {
      return __args.begin();
    }
    constexpr auto end() const noexcept {
      return __args.end();
    }
  };
};

namespace proc = moderna::process;
template <class char_type> struct std::formatter<proc::subprocess_argument, char_type> {
  constexpr auto parse(auto &ctx) {
    return ctx.begin();
  }
  constexpr auto format(const proc::subprocess_argument &args, auto &ctx) const {
    std::format_to(ctx.out(), "{} ", args.exec());
    for (const auto &command : args) {
      std::format_to(ctx.out(), "{} ", command);
    }
    return ctx.out();
  }
};