module;
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
  };
};