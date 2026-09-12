#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

struct TestCase {
	const char *name;
	void (*fn)();
};

inline std::vector<TestCase> &test_registry() {
	static std::vector<TestCase> registry;
	return registry;
}

struct TestRegistration {
	TestRegistration(const char *name, void (*fn)()) {
		test_registry().push_back({name, fn});
	}
};

inline int g_checks_failed = 0;
inline int g_checks_passed = 0;

#define TEST(name)                                                                                 \
	static void test_##name();                                                                     \
	static TestRegistration registration_##name(#name, test_##name);                               \
	static void test_##name()

#define CHECK(cond)                                                                                \
	do {                                                                                           \
		if (!(cond)) {                                                                             \
			std::fprintf(stderr, "  FAIL %s (%s:%d)\n", #cond, __FILE__, __LINE__);                \
			++g_checks_failed;                                                                     \
		} else {                                                                                   \
			++g_checks_passed;                                                                     \
		}                                                                                          \
	} while (0)

#define CHECK_EQ(a, b)                                                                             \
	do {                                                                                           \
		const auto _va = (a);                                                                      \
		const auto _vb = (b);                                                                      \
		if (!(_va == _vb)) {                                                                       \
			std::fprintf(stderr, "  FAIL %s == %s (%s:%d)\n", #a, #b, __FILE__, __LINE__);         \
			++g_checks_failed;                                                                     \
		} else {                                                                                   \
			++g_checks_passed;                                                                     \
		}                                                                                          \
	} while (0)

#define CHECK_NEAR(a, b, eps)                                                                      \
	do {                                                                                           \
		const double _va = static_cast<double>(a);                                                 \
		const double _vb = static_cast<double>(b);                                                 \
		if (std::fabs(_va - _vb) > (eps)) {                                                        \
			std::fprintf(                                                                          \
				stderr,                                                                            \
				"  FAIL |%s - %s| = %.12f > %s (%s:%d)\n",                                         \
				#a,                                                                                \
				#b,                                                                                \
				std::fabs(_va - _vb),                                                              \
				#eps,                                                                              \
				__FILE__,                                                                          \
				__LINE__);                                                                         \
			++g_checks_failed;                                                                     \
		} else {                                                                                   \
			++g_checks_passed;                                                                     \
		}                                                                                          \
	} while (0)
