#include "test.h"

std::vector<TestCase>& Tests() {
  static std::vector<TestCase> t;
  return t;
}
int g_failures = 0;

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);  // a crashing test still shows the ones before it
  for (auto& t : Tests()) {
    int before = g_failures;
    t.fn();
    std::printf("%s %s\n", g_failures == before ? "[ ok ]" : "[FAIL]", t.name);
  }
  std::printf("%zu tests, %d failed checks\n", Tests().size(), g_failures);
  return g_failures ? 1 : 0;
}
