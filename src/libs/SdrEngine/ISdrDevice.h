#ifndef ISDRDEVICE_H_
#define ISDRDEVICE_H_

// Project headers
#include "SdrTypes.h"

// System headers
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace SdrEngine
{

/**
 * @brief Callback invoked by the device when a block of raw I/Q bytes arrives.
 * @param data     Pointer to interleaved uint8_t I/Q pairs (RTL-SDR format).
 * @param length   Number of bytes (each pair is 2 bytes: I then Q).
 */
using RawIqCallback = std::function<void(const uint8_t* data, std::size_t length)>;

/**
 * @class ISdrDevice
 * @brief Abstract interface for a Software Defined Radio device.
 *
 * Implementations wrap a specific hardware API (e.g. librtlsdr) behind
 * a common surface so the rest of the application is device-agnostic.
 */
class ISdrDevice
{
public:
   virtual ~ISdrDevice() = default;

   // -- Lifecycle -----------------------------------------------------------

   /**
    * @brief Open the device at the given index (0-based).
    * @return true on success.
    */
   [[nodiscard]] virtual bool open(int deviceIndex = 0) = 0;

   /** @brief Close the device and release resources. */
   virtual void close() = 0;

   /**
    * @brief Check if the device is currently open.
    * @return true if the device is currently open.
    */
   [[nodiscard]] virtual bool isOpen() const = 0;

   // -- Tuning --------------------------------------------------------------

   /**
    * @brief Set the centre frequency in Hz.
    * @return true on success.
    */
   [[nodiscard]] virtual bool setCenterFrequency(uint64_t frequencyHz) = 0;

   /**
    * @brief Get the current centre frequency in Hz.
    * @return The current centre frequency in Hz.
    */
   [[nodiscard]] virtual uint64_t getCenterFrequency() const = 0;

   // -- Sample rate ---------------------------------------------------------

   /**
    * @brief Set the sample rate in samples per second.
    * @return true on success.
    */
   [[nodiscard]] virtual bool setSampleRate(uint32_t rateHz) = 0;

   /**
    * @brief Get the current sample rate in Hz.
    * @return The current sample rate in Hz.
    */
   [[nodiscard]] virtual uint32_t getSampleRate() const = 0;

   // -- Gain ----------------------------------------------------------------

   /**
    * @brief Enable or disable automatic gain control.
    * @return true on success.
    */
   [[nodiscard]] virtual bool setAutoGain(bool enabled) = 0;

   /**
    * @brief Set the manual gain in tenths of a dB (e.g. 496 = 49.6 dB).
    * @return true on success.
    */
   [[nodiscard]] virtual bool setGain(int tenthsDb) = 0;

   /**
    * @brief Get the current gain in tenths of a dB.
    * @return The current gain in tenths of a dB.
    */
   [[nodiscard]] virtual int getGain() const = 0;

   /**
    * @brief Get a sorted list of available gain values (tenths of dB).
    * @return A sorted list of available gain values (tenths of dB).
    */
   [[nodiscard]] virtual std::vector<int> getGainValues() const = 0;

   // -- Streaming -----------------------------------------------------------

   /**
    * @brief Start asynchronous streaming.  The callback will be invoked from a
    * device I/O thread with raw 8-bit unsigned I/Q pairs.
    * @param callback  Function to receive raw data blocks.
    * @param bufferSize  Requested buffer size per callback invocation (bytes).
    * @return true if streaming started successfully.
    */
   [[nodiscard]] virtual bool startStreaming(RawIqCallback callback,
                                            std::size_t bufferSize = 16384) = 0;

   /** @brief Stop asynchronous streaming. */
   virtual void stopStreaming() = 0;

   /**
    * @brief Check if the device is actively streaming.
    * @return true while the device is actively streaming.
    */
   [[nodiscard]] virtual bool isStreaming() const = 0;

   // -- Device info ---------------------------------------------------------

   /**
    * @brief Get the human-readable device name / description.
    * @return Human-readable device name / description.
    */
   [[nodiscard]] virtual std::string getName() const = 0;

   /** @brief Enumerate available devices of this type. */
   [[nodiscard]] virtual std::vector<DeviceInfo> enumerateDevices() const = 0;

   // -- Non-copyable / non-movable ------------------------------------------
   ISdrDevice() = default;
   ISdrDevice(const ISdrDevice&) = delete;
   ISdrDevice& operator=(const ISdrDevice&) = delete;
   ISdrDevice(ISdrDevice&&) = delete;
   ISdrDevice& operator=(ISdrDevice&&) = delete;
};

} // namespace SdrEngine

#endif // ISDRDEVICE_H_
