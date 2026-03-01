#include <gtest/gtest.h>
#include "SdrCommonUtils.h"

#include <cstddef>
#include <numeric>
#include <vector>

// ============================================================================
// decimateSpectrum
// ============================================================================

TEST(SdrCommonUtilsTest, DecimateSpectrum_SmallerThanMax_ReturnsUnchanged)
{
   const std::vector<float> input = {-10.0F, -20.0F, -30.0F};
   auto result = SdrEngine::decimateSpectrum(input, 2048);
   EXPECT_EQ(result, input);
}

TEST(SdrCommonUtilsTest, DecimateSpectrum_EqualToMax_ReturnsUnchanged)
{
   std::vector<float> input(2048);
   std::iota(input.begin(), input.end(), -100.0F);

   auto result = SdrEngine::decimateSpectrum(input, 2048);
   EXPECT_EQ(result.size(), 2048u);
   EXPECT_EQ(result, input);
}

TEST(SdrCommonUtilsTest, DecimateSpectrum_DoubleSize_PicksMaxOfPairs)
{
   // 4 bins → 2 bins: groups are {-10, -5} and {-20, -3}.
   const std::vector<float> input = {-10.0F, -5.0F, -20.0F, -3.0F};
   auto result = SdrEngine::decimateSpectrum(input, 2);
   ASSERT_EQ(result.size(), 2u);
   EXPECT_FLOAT_EQ(result[0], -5.0F);
   EXPECT_FLOAT_EQ(result[1], -3.0F);
}

TEST(SdrCommonUtilsTest, DecimateSpectrum_NonDivisible_HandlesRemainder)
{
   // 5 bins → 2 bins: first group gets 3 elements {1,2,3}, second gets 2 {4,5}.
   const std::vector<float> input = {1.0F, 2.0F, 3.0F, 4.0F, 5.0F};
   auto result = SdrEngine::decimateSpectrum(input, 2);
   ASSERT_EQ(result.size(), 2u);
   EXPECT_FLOAT_EQ(result[0], 3.0F);   // max(1, 2, 3)
   EXPECT_FLOAT_EQ(result[1], 5.0F);   // max(4, 5)
}

TEST(SdrCommonUtilsTest, DecimateSpectrum_LargeInput_OutputIsMaxBins)
{
   constexpr std::size_t INPUT_SIZE = 8192;
   constexpr std::size_t MAX_BINS = 2048;

   std::vector<float> input(INPUT_SIZE);
   std::iota(input.begin(), input.end(), -100.0F);

   auto result = SdrEngine::decimateSpectrum(input, MAX_BINS);
   EXPECT_EQ(result.size(), MAX_BINS);
}

TEST(SdrCommonUtilsTest, DecimateSpectrum_EmptyInput_ReturnsEmpty)
{
   const std::vector<float> input;
   auto result = SdrEngine::decimateSpectrum(input, 2048);
   EXPECT_TRUE(result.empty());
}

TEST(SdrCommonUtilsTest, DecimateSpectrum_MaxBinsZero_ReturnsOriginal)
{
   const std::vector<float> input = {1.0F, 2.0F, 3.0F};
   auto result = SdrEngine::decimateSpectrum(input, 0);
   EXPECT_EQ(result, input);
}

TEST(SdrCommonUtilsTest, DecimateSpectrum_DefaultMaxBins_Is2048)
{
   // Verify the default parameter works.
   std::vector<float> input(4096, -50.0F);
   auto result = SdrEngine::decimateSpectrum(input);
   EXPECT_EQ(result.size(), 2048u);
}

TEST(SdrCommonUtilsTest, DecimateSpectrum_PreservesMaxInEachGroup)
{
   // 6 bins → 3 bins: groups of 2.
   // Each group should return the maximum value.
   const std::vector<float> input = {
      -80.0F, -20.0F,   // group 0 → -20
      -60.0F, -40.0F,   // group 1 → -40
      -10.0F, -90.0F    // group 2 → -10
   };
   auto result = SdrEngine::decimateSpectrum(input, 3);
   ASSERT_EQ(result.size(), 3u);
   EXPECT_FLOAT_EQ(result[0], -20.0F);
   EXPECT_FLOAT_EQ(result[1], -40.0F);
   EXPECT_FLOAT_EQ(result[2], -10.0F);
}
