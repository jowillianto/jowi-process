module;
#include <format>
#include <string>
#include <type_traits>
#include <vector>
export module jowi.process:subprocess_argument;

namespace jowi::process {
  /**
   * @brief Builds an argv-style command line for spawning subprocesses.
   */
  export class SubprocessArgument {
    std::string __exec;
    std::vector<std::string> __args;
    mutable std::vector<const char *> __converted;

  public:
    /**
     * @brief Construct the argument list with an executable name and initial arguments.
     * @param exec Path to the executable to run.
     * @param args Additional arguments to pass to the process.
     */
    template <typename... Args>
      requires(std::is_constructible_v<std::string, Args> && ...)
    SubprocessArgument(std::string exec, Args &&...args) : __exec{exec} {
      __args.reserve(sizeof...(Args));
      (__args.emplace_back(std::string{std::forward<Args>(args)}), ...);
    }

    /**
     * @brief Append additional arguments to the command line.
     * @param args Arguments to append in order.
     */
    template <typename... Args>
      requires(std::is_constructible_v<std::string, Args> && ...)
    SubprocessArgument &add_argument(Args &&...args) {
      (__args.emplace_back(std::string{std::forward<Args>(args)}), ...);
      return *this;
    }
    /**
     * @brief Append each element of a range as an argument.
     * @param range Container whose elements will be appended.
     */
    template <std::ranges::range range_type>
      requires(std::is_constructible_v<std::string, std::ranges::range_value_t<range_type>>)
    SubprocessArgument &add_argument(range_type &&range) {
      for (const auto &value : range) {
        __args.emplace_back(std::string{value});
      }
      return *this;
    }
    /**
     * @brief Produce a null-terminated vector of C strings suitable for POSIX APIs.
     */
    const char *const *args() const noexcept {
      __converted.clear();
      __converted.reserve(__args.size() + 2);
      auto it = std::back_inserter(__converted);
      *it = __exec.c_str();
      std::ranges::transform(__args, it, &std::string::c_str);
      *it = nullptr;
      return __converted.data();
    }
    /**
     * @brief Retrieve the executable path.
     */
    const char *exec() const noexcept {
      return __exec.c_str();
    }

    /**
     * @brief Iterate over appended arguments.
     */
    constexpr auto begin() const noexcept {
      return __args.begin();
    }
    /**
     * @brief Sentinel for argument iteration.
     */
    constexpr auto end() const noexcept {
      return __args.end();
    }
  };
};

namespace proc = jowi::process;
/**
 * @brief Formatter to print subprocess arguments using `std::format`.
 */
template <class char_type> struct std::formatter<proc::SubprocessArgument, char_type> {
  /**
   * @brief Accept the default formatter behaviour.
   * @param ctx Parsing context provided by the formatter.
   */
  constexpr auto parse(auto &ctx) {
    return ctx.begin();
  }
  /**
   * @brief Write the executable and arguments into the target format context.
   * @param args Argument builder to serialise.
   * @param ctx Formatting context receiving the output.
   */
  constexpr auto format(const proc::SubprocessArgument &args, auto &ctx) const {
    std::format_to(ctx.out(), "{} ", args.exec());
    for (const auto &command : args) {
      std::format_to(ctx.out(), "{} ", command);
    }
    return ctx.out();
  }
};
