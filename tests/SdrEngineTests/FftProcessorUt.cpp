#include <gtest/gtest.h>
#include "FftProcessor.h"

#include <cmath>
#include <complex>
#include <numbers>
#include <numeric>
#include <vector>

using SdrEngine::FftProcessor;
using SdrEngine::WindowFunction;

// ============================================================================
// Construction
// ============================================================================

TEST(FftProcessorTest, DefaultConstruction_SizeIs2048)
{
   FftProcessor proc;
   EXPECT_EQ(proc.getFftSize(), 2048);
}

TEST(FftProcessorTest, CustomSize_ReportsCorrectSize)
{
   FftProcessor proc(1024, WindowFunction::Hanning);
   EXPECT_EQ(proc.getFftSize(), 1024);
   EXPECT_EQ(proc.getWindowFunction(), WindowFunction::Hanning);
}

// ============================================================================
// Configuration changes
// ============================================================================

TEST(FftProcessorTest, SetFftSize_ChangesSize)
{
   FftProcessor proc(512);
   proc.setFftSize(4096);
   EXPECT_EQ(proc.getFftSize(), 4096);
}

TEST(FftProcessorTest, SetWindowFunction_ChangesFunction)
{
   FftProcessor proc;
   proc.setWindowFunction(WindowFunction::FlatTop);
   EXPECT_EQ(proc.getWindowFunction(), WindowFunction::FlatTop);
}

// ============================================================================
// Processing — DC tone
// ============================================================================

TEST(FftProcessorTest, Process_DcTone_PeakAtCenter)
{
   constexpr size_t N = 1024;
   FftProcessor proc(N, WindowFunction::Rectangular);

   // All-ones signal → DC tone.  After FFT-shift the DC bin should be
   // at index N/2 and be the largest bin.
   std::vector<std::complex<float>> dc(N, {1.0F, 0.0F});
   auto result = proc.process(dc);

   ASSERT_EQ(result.size(), N);

   // DC bin after fftshift is at N/2.
   const size_t dcBin = N / 2;
   const float dcMag = result[dcBin];

   // Every other bin should be lower than DC.
   for (size_t i = 0; i < N; ++i)
   {
      if (i != dcBin)
      {
         EXPECT_LT(result[i], dcMag)  << "Bin " << i << " should be less than DC bin";
      }
   }
}

// ============================================================================
// Processing — pure tone peak location
// ============================================================================

TEST(FftProcessorTest, Process_PureTone_PeakAtExpectedBin)
{
   constexpr size_t N = 1024;
   FftProcessor proc(N, WindowFunction::Rectangular);

   // Generate a tone at bin +64 (i.e. 64/1024 of the sample rate).
   constexpr size_t TONE_BIN = 64;
   const float freqNorm =
      static_cast<float>(TONE_BIN) / static_cast<float>(N);

   std::vector<std::complex<float>> signal(static_cast<std::size_t>(N));
   for (size_t i = 0; i < N; ++i)
   {
      const float phase =
         2.0F * std::numbers::pi_v<float> * freqNorm * static_cast<float>(i);
      signal[static_cast<std::size_t>(i)] = {std::cos(phase), std::sin(phase)};
   }

   auto result = proc.process(signal);

   // After fftshift, positive frequency bin k maps to index N/2 + k.
   const auto peakIdx = (N / 2) + TONE_BIN;
   float peakVal = result[peakIdx];

   // Peak should be the maximum in the entire spectrum.
   for (std::size_t i = 0; i < result.size(); ++i)
   {
      if (i != peakIdx)
      {
         EXPECT_LT(result[i], peakVal)
            << "Bin " << i << " should be less than tone bin " << peakIdx;
      }
   }
}

// ============================================================================
// Processing — zero input
// ============================================================================

TEST(FftProcessorTest, Process_ZeroInput_AllBinsVeryLow)
{
   constexpr size_t N = 512;
   FftProcessor proc(N, WindowFunction::BlackmanHarris);

   std::vector<std::complex<float>> zeros(N, {0.0F, 0.0F});
   auto result = proc.process(zeros);

   ASSERT_EQ(result.size(), N);

   // All bins should be very low (no energy).
   for (const auto& val : result)
   {
      EXPECT_LT(val, -200.0F);
   }
}

// ============================================================================
// Output size
// ============================================================================

TEST(FftProcessorTest, Process_OutputSizeMatchesFftSize)
{
   constexpr int N = 2048;
   FftProcessor proc(N);

   // Pass fewer samples than FFT size (zero-padded).
   std::vector<std::complex<float>> small(256, {1.0F, 0.0F});
   auto result = proc.process(small);
   EXPECT_EQ(result.size(), static_cast<std::size_t>(N));
}

// ============================================================================
// Move semantics
// ============================================================================

TEST(FftProcessorTest, MoveConstruction_Works)
{
   FftProcessor a(1024, WindowFunction::Hanning);
   FftProcessor b(std::move(a));

   EXPECT_EQ(b.getFftSize(), 1024);
   EXPECT_EQ(b.getWindowFunction(), WindowFunction::Hanning);

   // Moved-from object should still be usable (but empty plan).
   std::vector<std::complex<float>> dc(1024, {1.0F, 0.0F});
   auto result = b.process(dc);
   EXPECT_EQ(result.size(), 1024u);
}

// ============================================================================
// Window function processing (smoke tests)
// ============================================================================

TEST(FftProcessorTest, AllWindowFunctions_ProduceValidOutput)
{
   constexpr int N = 512;
   const std::vector<std::complex<float>> signal(
      static_cast<std::size_t>(N), {0.5F, -0.3F});

   for (auto wf : {WindowFunction::Rectangular,
                    WindowFunction::Hanning,
                    WindowFunction::BlackmanHarris,
                    WindowFunction::FlatTop})
   {
      FftProcessor proc(N, wf);
      auto result = proc.process(signal);
      ASSERT_EQ(result.size(), static_cast<std::size_t>(N));

      // All values should be finite.
      for (const auto& v : result)
      {
         EXPECT_TRUE(std::isfinite(v)) << "Window function produced non-finite dB value";
      }
   }
}
