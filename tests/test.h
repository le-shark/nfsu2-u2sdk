#pragma once
// Minimal test harness: TEST(name) { CHECK(cond); CHECK_NEAR(a, b, eps); }
#include <cmath>
#include <cstdio>
#include <vector>

struct TestCase { const char* name; void (*fn)(); };
std::vector<TestCase>& Tests();
extern int g_failures;

struct TestReg { TestReg(const char* n, void (*f)()) { Tests().push_back({n, f}); } };
#define TEST(name) static void name(); static TestReg reg_##name(#name, name); static void name()
#define CHECK(c) do { if (!(c)) { ++g_failures; std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_NEAR(a, b, eps) do { double _a = (a), _b = (b); if (std::fabs(_a - _b) > (eps)) { ++g_failures; \
  std::printf("  FAIL %s:%d: %s = %g, expected %g\n", __FILE__, __LINE__, #a, _a, _b); } } while (0)
