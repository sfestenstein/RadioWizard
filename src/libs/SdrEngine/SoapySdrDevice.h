#ifndef SOAPYSDRDEVICE_H_
#define SOAPYSDRDEVICE_H_

// Project headers
#include "ISdrDevice.h"

// System headers
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

// Forward declarations for SoapySDR types.
namespace SoapySDR
{
class Device;
class Stream;
} // namespace SoapySDR

namespace SdrEngine
{

/**
 * @class SoapySdrDevice
 * @brief Vendor-neutral SDR device implementation using SoapySDR.
 *
 * Wraps the SoapySDR C++ API behind the ISdrDevice interface.
 * Supports any hardware with a SoapySDR module installed (RTL-SDR,
 * ADALM-PLUTO, HackRF, LimeSDR, Airspy, etc.).
 *
 * Streaming runs in a dedicated thread using readStream().
 * Samples are requested in CF32 format (native float) for full
 * precision — no lossy integer-to-float conversion is needed.
 */
class SoapySdrDevice : public ISdrDevice
{
public:
   SoapySdrDevice();
   ~SoapySdrDevice() override;

   // -- ISdrDevice ----------------------------------------------------------

   [[nodiscard]] bool open(int deviceIndex = 0) override;
   void close() override;
   [[nodiscard]] bool isOpen() const override;

   [[nodiscard]] bool setCenterFrequency(uint64_t frequencyHz) override;
   [[nodiscard]] uint64_t getCenterFrequency() const override;

   [[nodiscard]] bool setSampleRate(uint32_t rateHz) override;
   [[nodiscard]] uint32_t getSampleRate() const override;

   [[nodiscard]] bool setAutoGain(bool enabled) override;
   [[nodiscard]] bool setGain(int tenthsDb) override;
   [[nodiscard]] int getGain() const override;
   [[nodiscard]] std::vector<int> getGainValues() const override;

   [[nodiscard]] bool startStreaming(IqCallback callback,
                                    std::size_t bufferSize = 8192) override;
   void stopStreaming() override;
   [[nodiscard]] bool isStreaming() const override;

   [[nodiscard]] std::string getName() const override;
   [[nodiscard]] std::vector<DeviceInfo> enumerateDevices() const override;

private:
   // Thread body that runs the synchronous read loop.
   void streamThread(std::size_t samplesPerBuffer);

   // Re-enable hardware DC offset tracking, IQ balance, and set RF bandwidth.
   void resetCalibration();

   SoapySDR::Device* _device{nullptr};
   SoapySDR::Stream* _stream{nullptr};

   std::string _deviceLabel;   ///< Human-readable name of opened device.

   std::atomic<bool> _streaming{false};
   std::thread _streamThread;
   IqCallback _callback;
};

} // namespace SdrEngine

#endif // SOAPYSDRDEVICE_H_
