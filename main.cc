import moderna.process;
#include <print>

namespace proc = moderna::process;

int main(int argc, const char **argv, const char **env) {
  proc::subprocess_env::init(env);
  if (argc == 1) {
    std::println(stderr, "Provide an executable");
    return 1;
  }
  auto args = proc::dynamic_argument{argv[1]};
  for (auto i = 2; i < argc; i += 1) {
    args.add_argument(argv[i]);
  }
  auto res = proc::subprocess::run(args);
  if (!res) {
    std::println(stderr, "{}", res.error().what());
    return 1;
  }
  return 0;
}