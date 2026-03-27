#ifndef PLUTOSDRDEVICE_H_
#define PLUTOSDRDEVICE_H_

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

/**
 * @class PlutoSdrDevice
 * @brief ADALM-PLUTO (Pluto+) SDR device implementation using libiio.
 *
 * Wraps the libiio / libad9361 C API behind the ISdrDevice interface.
 * Streaming runs in a dedicated thread using iio_buffer_refill().
 *
 * The AD9361 delivers 12-bit signed I/Q in int16_t pairs.  This class
 * converts them to unsigned 8-bit pairs before invoking the RawIqCallback
 * so the rest of the pipeline (SdrEngine::onRawIqData) works unchanged.
 */
class PlutoSdrDevice : public ISdrDevice
{
public:
   PlutoSdrDevice();
   ~PlutoSdrDevice() override;

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
   // Thread body that runs the synchronous buffer-refill loop.
   void streamThread(std::size_t samplesPerBuffer);

   // Helper: write a string attribute to an IIO channel.
   static bool writeChannelAttr(void* channel, const char* attr,
                                const char* value);
   // Helper: write a long-long attribute to an IIO channel.
   static bool writeChannelAttrLongLong(void* channel, const char* attr,
                                        long long value);
   // Helper: read a long-long attribute from an IIO channel.
   static long long readChannelAttrLongLong(void* channel, const char* attr);

   void* _ctx{nullptr};           ///< iio_context*
   void* _phyDev{nullptr};        ///< iio_device* for ad9361-phy (control)
   void* _rxDev{nullptr};         ///< iio_device* for cf-ad9361-lpc (RX capture)
   void* _rxI{nullptr};           ///< iio_channel* for RX I
   void* _rxQ{nullptr};           ///< iio_channel* for RX Q
   void* _rxBuf{nullptr};         ///< iio_buffer*

   std::atomic<bool> _streaming{false};
   std::thread _streamThread;
   RawIqCallback _callback;
};

} // namespace SdrEngine

#endif // PLUTOSDRDEVICE_H_
