module;
#include <algorithm>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>
export module jowi.process:subprocess_env;

namespace jowi::process {
  /**
   * @brief Describes a single environment variable entry split into name and value.
   */
  export struct env_entry {
    std::string_view name;
    std::string_view value;

    /**
     * @brief Parse a `NAME=VALUE` string into an `env_entry` view.
     * @param v Environment string to split.
     */
    static env_entry make(std::string_view v) {
      auto eq_pos = std::ranges::find(v, '=');
      return env_entry{std::string_view{v.begin(), eq_pos}, std::string_view{eq_pos + 1, v.end()}};
    }
  };
  /**
   * @brief Manages the environment variables supplied to spawned subprocesses.
   */
  export class subprocess_env {
    /**
     * @brief Stores the process-wide environment snapshot configured during initialization.
     */
    static subprocess_env __global_env;
    std::vector<std::string> __env;
    mutable std::optional<std::vector<const char *>> __parsed_env;

    /**
     * @brief Lazily refresh the parsed environment cache with null-terminated strings.
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

    /**
     * @brief Look up an environment entry and return a mutable reference when present.
     * @param name Environment variable to search for.
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
    /**
     * @brief Creates an empty environment configuration.
     */
    subprocess_env() : __env{}, __parsed_env{std::nullopt} {}

    /**
     * @brief Set or update an environment variable value (does not modify the host environment).
     * @param name Environment variable to set.
     * @param value Value to assign to the variable.
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
    /**
     * @brief Retrieve an environment entry if it exists in this configuration.
     * @param name Environment variable to read.
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

    /**
     * @brief Provide a pointer suitable for POSIX spawn APIs.
     */
    const char *const *args() const noexcept {
      __update_parsed_env();
      return __parsed_env.value().data();
    }

    /**
     * @brief Populate the global environment from the host process environment once.
     * @param cur_env Null-terminated array of environment strings.
     */
    static void init(const char **cur_env) {
      uint64_t env_var_count = 0;
      for (auto i = cur_env; *i != nullptr; i += 1) {
        env_var_count += 1;
      }
      __global_env.__env.reserve(env_var_count);
      while (*cur_env != nullptr) {
        __global_env.__env.emplace_back(*cur_env);
        cur_env += 1;
      }
    }

    /**
     * @brief Create an environment either referencing the global snapshot or starting empty.
     * @param include_current When true, copy the global environment.
     */
    static subprocess_env make_env(bool include_current = true) {
      if (include_current) {
        return __global_env;
      } else {
        return subprocess_env{};
      }
    }

    /**
     * @brief Access the global environment snapshot.
     */
    static const subprocess_env &global_env() {
      return __global_env;
    }
  };

  /**
   * @brief Default initialize the global environment object.
   */
  subprocess_env subprocess_env::__global_env{};
}
