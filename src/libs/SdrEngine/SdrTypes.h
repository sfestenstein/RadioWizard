#ifndef SDRTYPES_H_
#define SDRTYPES_H_

// System headers
#include <chrono>
#include <complex>
#include <cstdint>
#include <string>
#include <vector>

namespace SdrEngine
{

/// Single I/Q sample as interleaved float.
using IqSample = std::complex<float>;

/// A chunk of I/Q samples with metadata.
struct IqBuffer
{
   std::vector<IqSample> samples;
   double centerFreqHz{0.0};
   double sampleRateHz{0.0};
   std::chrono::steady_clock::time_point timestamp;
};

/// FFT magnitude spectrum with metadata.
struct SpectrumData
{
   std::vector<float> magnitudesDb;   ///< Power in dB (typically negative).
   double centerFreqHz{0.0};
   double bandwidthHz{0.0};
   size_t fftSize{0};
};

/// FFT windowing function choices.
enum class WindowFunction : uint8_t
{
   Rectangular,
   Hanning,
   BlackmanHarris,
   FlatTop
};

/// SDR gain mode.
enum class GainMode : uint8_t
{
   Automatic,
   Manual
};

/// Information about a connected SDR device.
struct DeviceInfo
{
   int index{-1};
   std::string name;
   std::string manufacturer;
   std::string product;
   std::string serial;
};

} // namespace SdrEngine

#endif // SDRTYPES_H_
