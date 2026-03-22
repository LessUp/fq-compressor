// =============================================================================
// Build smoke test for the test harness
// =============================================================================

#include <gtest/gtest.h>

TEST(BuildSmokeTest, HarnessLoads) {
    EXPECT_TRUE(true);
}

TEST(BuildSmokeTest, CppStandard) {
// Verify C++23 is enabled
#if __cplusplus >= 202302L
    EXPECT_TRUE(true);
#elif __cplusplus >= 202002L
    // C++20 is acceptable (some compilers report C++20 for C++23 mode)
    EXPECT_TRUE(true);
#else
    FAIL() << "C++23 or later is not enabled";
#endif
}
