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

/** @brief Single I/Q sample as interleaved float. */
using IqSample = std::complex<float>;

/**
 * @class IqBuffer
 * @brief A chunk of I/Q samples with metadata.
 */
struct IqBuffer
{
   std::vector<IqSample> samples;
   double centerFreqHz{0.0};
   double sampleRateHz{0.0};
   std::chrono::steady_clock::time_point timestamp;
};

/**
 * @class SpectrumData
 * @brief FFT magnitude spectrum with metadata.
 */
struct SpectrumData
{
   std::vector<float> magnitudesDb;   ///< Power in dB (typically negative).
   double centerFreqHz{0.0};
   double bandwidthHz{0.0};
   size_t fftSize{0};
};

/** @brief FFT windowing function choices. */
enum class WindowFunction : uint8_t
{
   Rectangular,
   Hanning,
   BlackmanHarris,
   FlatTop
};

/** @brief SDR gain mode. */
enum class GainMode : uint8_t
{
   Automatic,
   Manual
};

/**
 * @class DeviceInfo
 * @brief Information about a connected SDR device.
 */
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
