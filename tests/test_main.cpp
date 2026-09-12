#include "check.hpp"

#include <cstdio>

int main() {
	int failed_tests = 0;
	for (const TestCase &test : test_registry()) {
		const int before = g_checks_failed;
		std::printf("TEST %s\n", test.name);
		test.fn();
		if (g_checks_failed > before) {
			++failed_tests;
			std::printf("  ... failed\n");
		} else {
			std::printf("  ... ok\n");
		}
	}
	std::printf(
		"\n%d passed checks, %d failed checks, %d failed tests\n",
		g_checks_passed,
		g_checks_failed,
		failed_tests);
	return failed_tests == 0 && g_checks_failed == 0 ? 0 : 1;
}
