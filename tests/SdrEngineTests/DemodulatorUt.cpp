#include <gtest/gtest.h>
#include "Demodulator.h"

#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

using SdrEngine::Demodulator;
using SdrEngine::DemodMode;
using SdrEngine::IqSample;

// ============================================================================
// Helper — generate an FM-modulated IQ carrier
// ============================================================================

namespace
{

/// Generate a block of FM-modulated complex samples.
/// @param numSamples  Number of samples to produce.
/// @param sampleRate  Sample rate (Hz).
/// @param deviation   Frequency deviation (Hz) for the modulating tone.
/// @param modFreq     Frequency of the single-tone modulator (Hz).
std::vector<IqSample> generateFmSignal(size_t numSamples, double sampleRate,
                                       double deviation, double modFreq)
{
   std::vector<IqSample> signal(numSamples);
   double phase = 0.0;
   const double dt = 1.0 / sampleRate;

   for (size_t i = 0; i < numSamples; ++i)
   {
      // Instantaneous frequency = deviation * sin(2π·modFreq·t)
      const double t = static_cast<double>(i) * dt;
      const double instFreq = deviation * std::sin(2.0 * std::numbers::pi * modFreq * t);

      phase += 2.0 * std::numbers::pi * instFreq * dt;
      signal[i] = {static_cast<float>(std::cos(phase)),
                    static_cast<float>(std::sin(phase))};
   }
   return signal;
}

/// Generate a block of AM-modulated complex samples.
/// carrier = (1 + m·cos(2π·modFreq·t)) · exp(j·0), i.e. carrier at DC.
std::vector<IqSample> generateAmSignal(size_t numSamples, double sampleRate,
                                       double modFreq,
                                       float modulationDepth = 0.5F)
{
   std::vector<IqSample> signal(numSamples);
   const double dt = 1.0 / sampleRate;

   for (size_t i = 0; i < numSamples; ++i)
   {
      const double t = static_cast<double>(i) * dt;
      const auto envelope =
         static_cast<float>(1.0 + (modulationDepth * std::cos(2.0 * std::numbers::pi * modFreq * t)));
      signal[i] = {envelope, 0.0F};
   }
   return signal;
}

/// Compute RMS energy of a float vector.
float rms(const std::vector<float>& v)
{
   if (v.empty())
   {
      return 0.0F;
   }
   double sum = 0.0;
   for (float s : v)
   {
      sum += static_cast<double>(s) * static_cast<double>(s);
   }
   return static_cast<float>(std::sqrt(sum / static_cast<double>(v.size())));
}

} // anonymous namespace

// Input rate for most tests — a realistic channel-filter output rate.
static constexpr double INPUT_RATE = 200'000.0;
static constexpr double AUDIO_RATE = 48'000.0;

// ============================================================================
// Default construction
// ============================================================================

TEST(DemodulatorTest, DefaultConstruction_NotConfigured)
{
   Demodulator demod;
   EXPECT_FALSE(demod.isConfigured());
   EXPECT_EQ(demod.getMode(), DemodMode::FmMono);
   EXPECT_DOUBLE_EQ(demod.getAudioSampleRate(), Demodulator::DEFAULT_AUDIO_RATE);
}

// ============================================================================
// demodModeName
// ============================================================================

TEST(DemodulatorTest, DemodModeName_ReturnsExpected)
{
   EXPECT_STREQ(SdrEngine::demodModeName(DemodMode::FmMono), "FM Mono");
   EXPECT_STREQ(SdrEngine::demodModeName(DemodMode::FmStereo), "FM Stereo");
   EXPECT_STREQ(SdrEngine::demodModeName(DemodMode::AM), "AM");
}

// ============================================================================
// Configuration
// ============================================================================

TEST(DemodulatorTest, Configure_FmMono_BecomesConfigured)
{
   Demodulator demod;
   demod.configure(DemodMode::FmMono, INPUT_RATE, AUDIO_RATE);

   EXPECT_TRUE(demod.isConfigured());
   EXPECT_EQ(demod.getMode(), DemodMode::FmMono);
   EXPECT_DOUBLE_EQ(demod.getAudioSampleRate(), AUDIO_RATE);
}

TEST(DemodulatorTest, Configure_FmStereo_BecomesConfigured)
{
   Demodulator demod;
   demod.configure(DemodMode::FmStereo, INPUT_RATE, AUDIO_RATE);

   EXPECT_TRUE(demod.isConfigured());
   EXPECT_EQ(demod.getMode(), DemodMode::FmStereo);
}

TEST(DemodulatorTest, Configure_Am_BecomesConfigured)
{
   Demodulator demod;
   demod.configure(DemodMode::AM, INPUT_RATE, AUDIO_RATE);

   EXPECT_TRUE(demod.isConfigured());
   EXPECT_EQ(demod.getMode(), DemodMode::AM);
}

TEST(DemodulatorTest, Configure_InvalidInputRate_StaysUnconfigured)
{
   Demodulator demod;
   demod.configure(DemodMode::FmMono, 0.0, AUDIO_RATE);

   EXPECT_FALSE(demod.isConfigured());
}

TEST(DemodulatorTest, Configure_NegativeInputRate_StaysUnconfigured)
{
   Demodulator demod;
   demod.configure(DemodMode::FmMono, -100.0, AUDIO_RATE);

   EXPECT_FALSE(demod.isConfigured());
}

TEST(DemodulatorTest, Configure_InvalidAudioRate_StaysUnconfigured)
{
   Demodulator demod;
   demod.configure(DemodMode::AM, INPUT_RATE, 0.0);

   EXPECT_FALSE(demod.isConfigured());
}

TEST(DemodulatorTest, Configure_DefaultAudioRate_Uses48kHz)
{
   Demodulator demod;
   demod.configure(DemodMode::FmMono, INPUT_RATE);

   EXPECT_DOUBLE_EQ(demod.getAudioSampleRate(), 48'000.0);
}

// ============================================================================
// Reconfiguration
// ============================================================================

TEST(DemodulatorTest, Reconfigure_ChangesMode)
{
   Demodulator demod;
   demod.configure(DemodMode::FmMono, INPUT_RATE, AUDIO_RATE);
   EXPECT_EQ(demod.getMode(), DemodMode::FmMono);

   demod.configure(DemodMode::AM, INPUT_RATE, AUDIO_RATE);
   EXPECT_EQ(demod.getMode(), DemodMode::AM);
   EXPECT_TRUE(demod.isConfigured());
}

TEST(DemodulatorTest, Reconfigure_AllModeTransitions_DoNotCrash)
{
   Demodulator demod;

   // Cycle through all mode transitions.
   for (auto from : {DemodMode::FmMono, DemodMode::FmStereo, DemodMode::AM})
   {
      for (auto to : {DemodMode::FmMono, DemodMode::FmStereo, DemodMode::AM})
      {
         demod.configure(from, INPUT_RATE, AUDIO_RATE);
         demod.configure(to, INPUT_RATE, AUDIO_RATE);
         EXPECT_TRUE(demod.isConfigured());
         EXPECT_EQ(demod.getMode(), to);
      }
   }
}

// ============================================================================
// Guard clauses — demodulate before configure / empty input
// ============================================================================

TEST(DemodulatorTest, Demodulate_WhenNotConfigured_ReturnsEmpty)
{
   Demodulator demod;
   std::vector<IqSample> input(1024, {1.0F, 0.0F});
   auto result = demod.demodulate(input);

   EXPECT_TRUE(result.left.empty());
   EXPECT_TRUE(result.right.empty());
}

TEST(DemodulatorTest, Demodulate_EmptyInput_ReturnsEmpty)
{
   Demodulator demod;
   demod.configure(DemodMode::FmMono, INPUT_RATE, AUDIO_RATE);

   std::vector<IqSample> empty;
   auto result = demod.demodulate(empty);

   EXPECT_TRUE(result.left.empty());
   EXPECT_TRUE(result.right.empty());
}

// ============================================================================
// FM Mono — signal processing
// ============================================================================

TEST(DemodulatorTest, FmMono_ProducesNonEmptyOutput)
{
   Demodulator demod;
   demod.configure(DemodMode::FmMono, INPUT_RATE, AUDIO_RATE);

   auto signal = generateFmSignal(4096, INPUT_RATE, 25'000.0, 1'000.0);
   auto result = demod.demodulate(signal);

   EXPECT_FALSE(result.left.empty());
   EXPECT_FALSE(result.right.empty());
}

TEST(DemodulatorTest, FmMono_OutputLengthMatchesResampleRatio)
{
   Demodulator demod;
   demod.configure(DemodMode::FmMono, INPUT_RATE, AUDIO_RATE);

   constexpr size_t N = 8192;
   auto signal = generateFmSignal(N, INPUT_RATE, 25'000.0, 1'000.0);
   auto result = demod.demodulate(signal);

   // Expected output count ≈ N × (audioRate / inputRate).
   const double ratio = AUDIO_RATE / INPUT_RATE;
   const auto expectedLen = static_cast<size_t>(static_cast<double>(N) * ratio);

   // Allow ±5% tolerance for resampler edge effects.
   EXPECT_GT(result.left.size(), static_cast<size_t>(expectedLen * 0.95));
   EXPECT_LT(result.left.size(), static_cast<size_t>(expectedLen * 1.05));
}

TEST(DemodulatorTest, FmMono_OutputSamplesAreFinite)
{
   Demodulator demod;
   demod.configure(DemodMode::FmMono, INPUT_RATE, AUDIO_RATE);

   auto signal = generateFmSignal(4096, INPUT_RATE, 25'000.0, 1'000.0);
   auto result = demod.demodulate(signal);

   for (size_t i = 0; i < result.left.size(); ++i)
   {
      EXPECT_TRUE(std::isfinite(result.left[i]))
         << "Non-finite left sample at index " << i;
      EXPECT_TRUE(std::isfinite(result.right[i]))
         << "Non-finite right sample at index " << i;
   }
}

TEST(DemodulatorTest, FmMono_LeftAndRightAreIdentical)
{
   Demodulator demod;
   demod.configure(DemodMode::FmMono, INPUT_RATE, AUDIO_RATE);

   auto signal = generateFmSignal(4096, INPUT_RATE, 25'000.0, 1'000.0);
   auto result = demod.demodulate(signal);

   ASSERT_EQ(result.left.size(), result.right.size());
   for (size_t i = 0; i < result.left.size(); ++i)
   {
      EXPECT_FLOAT_EQ(result.left[i], result.right[i])
         << "Mono output left != right at index " << i;
   }
}

TEST(DemodulatorTest, FmMono_ModulatedSignal_HasEnergy)
{
   Demodulator demod;
   demod.configure(DemodMode::FmMono, INPUT_RATE, AUDIO_RATE);

   auto signal = generateFmSignal(16384, INPUT_RATE, 50'000.0, 1'000.0);
   auto result = demod.demodulate(signal);

   // After filter settling, the output should have meaningful energy.
   ASSERT_GT(result.left.size(), 100u);
   // Check the last half of the output (after transient).
   const size_t start = result.left.size() / 2;
   std::vector<float> tail(result.left.begin() + static_cast<ptrdiff_t>(start),
                           result.left.end());
   EXPECT_GT(rms(tail), 0.001F)
      << "FM modulated signal should produce non-trivial audio energy";
}

TEST(DemodulatorTest, FmMono_SilentCarrier_LowEnergy)
{
   Demodulator demod;
   demod.configure(DemodMode::FmMono, INPUT_RATE, AUDIO_RATE);

   // Unmodulated carrier: constant frequency, zero deviation.
   // Generate a pure CW carrier — no frequency variation.
   const size_t n = 8192;
   std::vector<IqSample> carrier(n);
   for (size_t i = 0; i < n; ++i)
   {
      carrier[i] = {1.0F, 0.0F}; // DC, no modulation
   }

   auto result = demod.demodulate(carrier);
   ASSERT_FALSE(result.left.empty());

   // A DC carrier through an FM demod should produce near-zero audio.
   const size_t start = result.left.size() / 2;
   std::vector<float> tail(result.left.begin() + static_cast<ptrdiff_t>(start),
                           result.left.end());
   EXPECT_LT(rms(tail), 0.05F)
      << "Unmodulated carrier should produce very low audio energy";
}

// ============================================================================
// FM Stereo — signal processing
// ============================================================================

TEST(DemodulatorTest, FmStereo_ProducesNonEmptyOutput)
{
   Demodulator demod;
   demod.configure(DemodMode::FmStereo, INPUT_RATE, AUDIO_RATE);

   auto signal = generateFmSignal(8192, INPUT_RATE, 50'000.0, 1'000.0);
   auto result = demod.demodulate(signal);

   EXPECT_FALSE(result.left.empty());
   EXPECT_FALSE(result.right.empty());
}

TEST(DemodulatorTest, FmStereo_OutputLengthMatchesResampleRatio)
{
   Demodulator demod;
   demod.configure(DemodMode::FmStereo, INPUT_RATE, AUDIO_RATE);

   constexpr size_t N = 8192;
   auto signal = generateFmSignal(N, INPUT_RATE, 50'000.0, 1'000.0);
   auto result = demod.demodulate(signal);

   const double ratio = AUDIO_RATE / INPUT_RATE;
   const auto expectedLen = static_cast<size_t>(static_cast<double>(N) * ratio);

   EXPECT_GT(result.left.size(), static_cast<size_t>(expectedLen * 0.95));
   EXPECT_LT(result.left.size(), static_cast<size_t>(expectedLen * 1.05));
   EXPECT_GT(result.right.size(), static_cast<size_t>(expectedLen * 0.95));
   EXPECT_LT(result.right.size(), static_cast<size_t>(expectedLen * 1.05));
}

TEST(DemodulatorTest, FmStereo_OutputSamplesAreFinite)
{
   Demodulator demod;
   demod.configure(DemodMode::FmStereo, INPUT_RATE, AUDIO_RATE);

   auto signal = generateFmSignal(8192, INPUT_RATE, 50'000.0, 1'000.0);
   auto result = demod.demodulate(signal);

   for (size_t i = 0; i < result.left.size(); ++i)
   {
      EXPECT_TRUE(std::isfinite(result.left[i]))
         << "Non-finite left sample at index " << i;
   }
   for (size_t i = 0; i < result.right.size(); ++i)
   {
      EXPECT_TRUE(std::isfinite(result.right[i]))
         << "Non-finite right sample at index " << i;
   }
}

// ============================================================================
// AM — signal processing
// ============================================================================

TEST(DemodulatorTest, Am_ProducesNonEmptyOutput)
{
   Demodulator demod;
   demod.configure(DemodMode::AM, INPUT_RATE, AUDIO_RATE);

   auto signal = generateAmSignal(4096, INPUT_RATE, 1'000.0);
   auto result = demod.demodulate(signal);

   EXPECT_FALSE(result.left.empty());
   EXPECT_FALSE(result.right.empty());
}

TEST(DemodulatorTest, Am_OutputLengthMatchesResampleRatio)
{
   Demodulator demod;
   demod.configure(DemodMode::AM, INPUT_RATE, AUDIO_RATE);

   constexpr size_t N = 8192;
   auto signal = generateAmSignal(N, INPUT_RATE, 1'000.0);
   auto result = demod.demodulate(signal);

   const double ratio = AUDIO_RATE / INPUT_RATE;
   const auto expectedLen = static_cast<size_t>(static_cast<double>(N) * ratio);

   EXPECT_GT(result.left.size(), static_cast<size_t>(expectedLen * 0.95));
   EXPECT_LT(result.left.size(), static_cast<size_t>(expectedLen * 1.05));
}

TEST(DemodulatorTest, Am_OutputSamplesAreFinite)
{
   Demodulator demod;
   demod.configure(DemodMode::AM, INPUT_RATE, AUDIO_RATE);

   auto signal = generateAmSignal(4096, INPUT_RATE, 1'000.0);
   auto result = demod.demodulate(signal);

   for (size_t i = 0; i < result.left.size(); ++i)
   {
      EXPECT_TRUE(std::isfinite(result.left[i]))
         << "Non-finite left sample at index " << i;
      EXPECT_TRUE(std::isfinite(result.right[i]))
         << "Non-finite right sample at index " << i;
   }
}

TEST(DemodulatorTest, Am_LeftAndRightAreIdentical)
{
   Demodulator demod;
   demod.configure(DemodMode::AM, INPUT_RATE, AUDIO_RATE);

   auto signal = generateAmSignal(4096, INPUT_RATE, 1'000.0);
   auto result = demod.demodulate(signal);

   ASSERT_EQ(result.left.size(), result.right.size());
   for (size_t i = 0; i < result.left.size(); ++i)
   {
      EXPECT_FLOAT_EQ(result.left[i], result.right[i])
         << "AM mono output left != right at index " << i;
   }
}

TEST(DemodulatorTest, Am_ModulatedSignal_HasEnergy)
{
   Demodulator demod;
   demod.configure(DemodMode::AM, INPUT_RATE, AUDIO_RATE);

   auto signal = generateAmSignal(16384, INPUT_RATE, 1'000.0, 0.8F);
   auto result = demod.demodulate(signal);

   ASSERT_GT(result.left.size(), 100u);
   const size_t start = result.left.size() / 2;
   std::vector<float> tail(result.left.begin() + static_cast<ptrdiff_t>(start),
                           result.left.end());
   EXPECT_GT(rms(tail), 0.001F)
      << "AM modulated signal should produce non-trivial audio energy";
}

// ============================================================================
// Reset
// ============================================================================

TEST(DemodulatorTest, Reset_BeforeConfigure_DoesNotCrash)
{
   Demodulator demod;
   EXPECT_NO_THROW(demod.reset());
   EXPECT_FALSE(demod.isConfigured());
}

TEST(DemodulatorTest, Reset_AfterConfigure_PreservesConfiguration)
{
   Demodulator demod;
   demod.configure(DemodMode::FmMono, INPUT_RATE, AUDIO_RATE);
   demod.reset();

   EXPECT_TRUE(demod.isConfigured());
   EXPECT_EQ(demod.getMode(), DemodMode::FmMono);
   EXPECT_DOUBLE_EQ(demod.getAudioSampleRate(), AUDIO_RATE);
}

TEST(DemodulatorTest, Reset_StillProducesOutput)
{
   Demodulator demod;
   demod.configure(DemodMode::FmMono, INPUT_RATE, AUDIO_RATE);

   auto signal = generateFmSignal(4096, INPUT_RATE, 25'000.0, 1'000.0);

   // Demodulate, reset, demodulate again.
   auto r1 = demod.demodulate(signal);
   EXPECT_FALSE(r1.left.empty());

   demod.reset();

   auto r2 = demod.demodulate(signal);
   EXPECT_FALSE(r2.left.empty());
}

TEST(DemodulatorTest, Reset_AllModes_DoesNotCrash)
{
   Demodulator demod;
   for (auto mode : {DemodMode::FmMono, DemodMode::FmStereo, DemodMode::AM})
   {
      demod.configure(mode, INPUT_RATE, AUDIO_RATE);
      EXPECT_NO_THROW(demod.reset());
      EXPECT_TRUE(demod.isConfigured());
   }
}

// ============================================================================
// Consecutive blocks — no state corruption
// ============================================================================

TEST(DemodulatorTest, FmMono_MultipleBlocks_AllProduceOutput)
{
   Demodulator demod;
   demod.configure(DemodMode::FmMono, INPUT_RATE, AUDIO_RATE);

   auto signal = generateFmSignal(2048, INPUT_RATE, 25'000.0, 1'000.0);

   for (int block = 0; block < 5; ++block)
   {
      auto result = demod.demodulate(signal);
      EXPECT_FALSE(result.left.empty())
         << "Block " << block << " should produce output";
      for (size_t i = 0; i < result.left.size(); ++i)
      {
         EXPECT_TRUE(std::isfinite(result.left[i]))
            << "Block " << block << " sample " << i << " non-finite";
      }
   }
}

// ============================================================================
// No-resample path (audioRate == inputRate)
// ============================================================================

TEST(DemodulatorTest, FmMono_NoResample_WhenRatesEqual)
{
   Demodulator demod;
   // inputRate == audioRate → no resampler should be created.
   demod.configure(DemodMode::FmMono, 48'000.0, 48'000.0);

   constexpr size_t N = 4096;
   auto signal = generateFmSignal(N, 48'000.0, 10'000.0, 1'000.0);
   auto result = demod.demodulate(signal);

   // Without resampling, output count should match input count exactly.
   EXPECT_EQ(result.left.size(), N);
}
