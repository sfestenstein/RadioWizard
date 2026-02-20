#include <gtest/gtest.h>
#include "ChannelFilter.h"

#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

using SdrEngine::ChannelFilter;
using SdrEngine::IqSample;

// ============================================================================
// Initial state
// ============================================================================

TEST(ChannelFilterTest, DefaultState_NotConfiguredNotEnabled)
{
   ChannelFilter filter;
   EXPECT_FALSE(filter.isConfigured());
   EXPECT_FALSE(filter.isEnabled());
   EXPECT_DOUBLE_EQ(filter.getOutputSampleRate(), 0.0);
}

// ============================================================================
// Configuration
// ============================================================================

TEST(ChannelFilterTest, Configure_SetsParameters)
{
   ChannelFilter filter;
   filter.configure(100'000.0, 200'000.0, 2'400'000.0);

   EXPECT_TRUE(filter.isConfigured());
   EXPECT_DOUBLE_EQ(filter.getChannelBandwidth(), 200'000.0);
   EXPECT_DOUBLE_EQ(filter.getCenterOffset(), 100'000.0);
   EXPECT_GT(filter.getOutputSampleRate(), 0.0);
   EXPECT_GE(filter.getOutputSampleRate(), 200'000.0);
}

TEST(ChannelFilterTest, Configure_InvalidBandwidth_StaysUnconfigured)
{
   ChannelFilter filter;
   filter.configure(0.0, -100.0, 2'400'000.0);

   EXPECT_FALSE(filter.isConfigured());
}

TEST(ChannelFilterTest, Configure_InvalidSampleRate_StaysUnconfigured)
{
   ChannelFilter filter;
   filter.configure(0.0, 200'000.0, 0.0);

   EXPECT_FALSE(filter.isConfigured());
}

TEST(ChannelFilterTest, Configure_BandwidthClampedToSampleRate)
{
   ChannelFilter filter;
   filter.configure(0.0, 5'000'000.0, 2'400'000.0);

   EXPECT_TRUE(filter.isConfigured());
   EXPECT_DOUBLE_EQ(filter.getChannelBandwidth(), 2'400'000.0);
}

// ============================================================================
// Enable / disable
// ============================================================================

TEST(ChannelFilterTest, SetEnabled_TogglesState)
{
   ChannelFilter filter;
   filter.setEnabled(true);
   EXPECT_TRUE(filter.isEnabled());
   filter.setEnabled(false);
   EXPECT_FALSE(filter.isEnabled());
}

// ============================================================================
// Processing — disabled / unconfigured
// ============================================================================

TEST(ChannelFilterTest, Process_WhenDisabled_ReturnsEmpty)
{
   ChannelFilter filter;
   filter.configure(0.0, 200'000.0, 2'400'000.0);
   // Not enabled
   std::vector<IqSample> input(1024, {1.0F, 0.0F});
   auto result = filter.process(input);
   EXPECT_TRUE(result.empty());
}

TEST(ChannelFilterTest, Process_WhenNotConfigured_ReturnsEmpty)
{
   ChannelFilter filter;
   filter.setEnabled(true);
   // Not configured
   std::vector<IqSample> input(1024, {1.0F, 0.0F});
   auto result = filter.process(input);
   EXPECT_TRUE(result.empty());
}

TEST(ChannelFilterTest, Process_EmptyInput_ReturnsEmpty)
{
   ChannelFilter filter;
   filter.configure(0.0, 200'000.0, 2'400'000.0);
   filter.setEnabled(true);

   std::vector<IqSample> empty;
   auto result = filter.process(empty);
   EXPECT_TRUE(result.empty());
}

// ============================================================================
// Processing — decimation
// ============================================================================

TEST(ChannelFilterTest, Process_OutputIsShorterThanInput)
{
   ChannelFilter filter;
   // 200 kHz channel from 2.4 MHz → ~12x decimation
   filter.configure(0.0, 200'000.0, 2'400'000.0);
   filter.setEnabled(true);

   const std::size_t n = 4096;
   std::vector<IqSample> input(n, {0.5F, -0.3F});
   auto result = filter.process(input);

   // Output should be significantly smaller due to decimation.
   EXPECT_FALSE(result.empty());
   EXPECT_LT(result.size(), n);
}

TEST(ChannelFilterTest, Process_OutputSamplesAreFinite)
{
   ChannelFilter filter;
   filter.configure(0.0, 200'000.0, 2'400'000.0);
   filter.setEnabled(true);

   const std::size_t n = 2048;
   std::vector<IqSample> input(n);
   for (std::size_t i = 0; i < n; ++i)
   {
      const float phase =
         2.0F * std::numbers::pi_v<float> * 0.01F * static_cast<float>(i);
      input[i] = {std::cos(phase), std::sin(phase)};
   }

   auto result = filter.process(input);
   for (const auto& sample : result)
   {
      EXPECT_TRUE(std::isfinite(sample.real())) << "Non-finite real component";
      EXPECT_TRUE(std::isfinite(sample.imag())) << "Non-finite imag component";
   }
}

// ============================================================================
// Processing — DC signal at centre passes through
// ============================================================================

TEST(ChannelFilterTest, Process_DcSignal_ZeroOffset_PassesThrough)
{
   ChannelFilter filter;
   // Channel centred at DC, 200 kHz BW
   filter.configure(0.0, 200'000.0, 2'400'000.0);
   filter.setEnabled(true);

   // DC signal: constant I=1, Q=0
   const std::size_t n = 8192;
   std::vector<IqSample> dc(n, {1.0F, 0.0F});

   auto result = filter.process(dc);
   ASSERT_FALSE(result.empty());

   // After the filter settles, the output should have non-negligible energy.
   // Check the last quarter of the output for steady state.
   const std::size_t startIdx = result.size() * 3 / 4;
   float totalEnergy = 0.0F;
   for (std::size_t i = startIdx; i < result.size(); ++i)
   {
      totalEnergy += std::norm(result[i]);
   }
   const float avgEnergy = totalEnergy / static_cast<float>(result.size() - startIdx);
   EXPECT_GT(avgEnergy, 0.001F) << "DC signal should pass through zero-offset filter";
}

// ============================================================================
// Reset
// ============================================================================

TEST(ChannelFilterTest, Reset_DoesNotCrash)
{
   ChannelFilter filter;
   filter.configure(0.0, 200'000.0, 2'400'000.0);
   filter.setEnabled(true);

   std::vector<IqSample> input(1024, {1.0F, 0.0F});
   auto r1 = filter.process(input);

   filter.reset();

   auto r2 = filter.process(input);
   EXPECT_FALSE(r2.empty());
}

// ============================================================================
// Reconfigure
// ============================================================================

TEST(ChannelFilterTest, Reconfigure_ChangesOutputRate)
{
   ChannelFilter filter;

   filter.configure(0.0, 200'000.0, 2'400'000.0);
   const double rate1 = filter.getOutputSampleRate();

   filter.configure(0.0, 400'000.0, 2'400'000.0);
   const double rate2 = filter.getOutputSampleRate();

   // Wider channel → higher output rate (less decimation).
   EXPECT_GT(rate2, rate1);
}
