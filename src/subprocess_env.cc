module;
#include <algorithm>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>
export module moderna.process:subprocess_env;

namespace moderna::process {
  export struct env_entry {
    std::string_view name;
    std::string_view value;

    static env_entry make(std::string_view v) {
      auto eq_pos = std::ranges::find(v, '=');
      return env_entry{std::string_view{v.begin(), eq_pos}, std::string_view{eq_pos, v.end()}};
    }
  };
  export class subprocess_env {
    /*
      This is the global environment variable that should be set with a call when the program
      starts.
    */
    static subprocess_env __global_env;
    std::vector<std::string> __env;
    mutable std::optional<std::vector<const char *>> __parsed_env;

    /*
      This updates the parsed environment with the unparsed environment.
    */
    void __update_parsed_env() const {
      if (!__parsed_env) {
        std::vector<const char *> v;
        v.reserve(__env.size() + 1);
        auto it = std::back_inserter(v);
        std::ranges::transform(__env, it, [](const auto &s) { return s.c_str(); });
        *it = nullptr;
        __parsed_env = std::move(v);
      }
    }

    /*
      Returns a reference to the environment variable string.
    */
    std::optional<std::reference_wrapper<std::string>> __get(std::string_view name) noexcept {
      auto it = std::ranges::find_if(__env, [&](const std::string &v) {
        return env_entry::make(v).name == name;
      });
      if (it == __env.end()) {
        return std::nullopt;
      } else {
        return std::ref(*it);
      }
    }

  public:
    /*
      Creates an empty environment.
    */
    subprocess_env() : __env{}, __parsed_env{std::nullopt} {}

    /*
      Sets a value into the environment. If the value exists, it will be mutated. If the value
      does not exist, it will be added into the current environment. Note that, this does not
      necessarily sync with the OS environment.
    */
    subprocess_env &set(std::string_view name, std::string_view value) {
      return __get(name)
        .transform([&](std::string &v) {
          v = std::format("{}={}", name, value);
          return std::ref(*this);
        })
        .or_else([&]() {
          __env.emplace_back(std::format("{}={}", name, value));
          return std::optional{std::ref(*this)};
        })
        .value()
        .get();
    }
    /*
      gets the value of an environment variable.
    */
    std::optional<env_entry> get(std::string_view name) const noexcept {
      for (const auto &env_string : __env) {
        auto entry = env_entry::make(env_string);
        if (entry.name == name) {
          return entry;
        }
      }
      return std::nullopt;
    }

    /*
      Returns a value that can be used for system calls.
    */
    const char *const *args() const noexcept {
      __update_parsed_env();
      return __parsed_env.value().data();
    }

    /*
      Initialize the global environment. Ideally, this is called only once at the start of the
      program.
    */
    static void init(const char **cur_env) {
      while (*cur_env != nullptr) {
        __global_env.__env.emplace_back(std::string(*cur_env));
        cur_env += 1;
      }
    }

    static subprocess_env make_env(bool include_current = true) {
      if (include_current) {
        return __global_env;
      } else {
        return subprocess_env{};
      }
    }

    static const subprocess_env &global_env() {
      return __global_env;
    }
  };

  /*
    Default initialize the global environment
  */
  subprocess_env subprocess_env::__global_env{};
}