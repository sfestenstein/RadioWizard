#ifndef RTLSDRDEVICE_H_
#define RTLSDRDEVICE_H_

// Project headers
#include "ISdrDevice.h"

// System headers
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace SdrEngine
{

/// RTL-SDR device implementation using librtlsdr.
///
/// Wraps the librtlsdr C API behind the ISdrDevice interface.
/// Async streaming runs in a dedicated thread via `rtlsdr_read_async`.
class RtlSdrDevice : public ISdrDevice
{
public:
   RtlSdrDevice();
   ~RtlSdrDevice() override;

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

   [[nodiscard]] bool startStreaming(RawIqCallback callback,
                                    std::size_t bufferSize = 16384) override;
   void stopStreaming() override;
   [[nodiscard]] bool isStreaming() const override;

   [[nodiscard]] std::string getName() const override;
   [[nodiscard]] std::vector<DeviceInfo> enumerateDevices() const override;

private:
   /// librtlsdr callback trampoline (static → member).
   static void rtlsdrCallback(unsigned char* buf, uint32_t len, void* ctx);

   /// Thread body that runs rtlsdr_read_async.
   void asyncReadThread(std::size_t bufferSize);

   void* _device{nullptr};   ///< Opaque rtlsdr_dev_t* (avoids exposing rtl-sdr.h)
   std::atomic<bool> _streaming{false};
   std::thread _asyncThread;
   RawIqCallback _callback;
};

} // namespace SdrEngine

#endif // RTLSDRDEVICE_H_
