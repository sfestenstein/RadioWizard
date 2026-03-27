// Project headers
#include "PlutoSdrDevice.h"
#include "GeneralLogger.h"

// Third-party headers
#include <ad9361.h>
#include <iio.h>

// System headers
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <utility>

namespace SdrEngine
{

// ============================================================================
// Construction / destruction
// ============================================================================

PlutoSdrDevice::PlutoSdrDevice() = default;

PlutoSdrDevice::~PlutoSdrDevice()
{
   if (PlutoSdrDevice::isStreaming())
   {
      PlutoSdrDevice::stopStreaming();
   }
   if (PlutoSdrDevice::isOpen())
   {
      PlutoSdrDevice::close();
   }
}

// ============================================================================
// Lifecycle
// ============================================================================

bool PlutoSdrDevice::open(int deviceIndex)
{
   if (_ctx != nullptr)
   {
      GPWARN("PlutoSdrDevice::open() — device already open, closing first");
      PlutoSdrDevice::close();
   }

   // Enumerate available Pluto devices and pick the requested index.
   auto* scanCtx = iio_create_scan_context(nullptr, 0);
   if (scanCtx == nullptr)
   {
      GPERROR("Failed to create IIO scan context");
      return false;
   }

   struct iio_context_info** infoList = nullptr;
   const auto count = iio_scan_context_get_info_list(scanCtx, &infoList);
   if (count < 0)
   {
      GPERROR("IIO scan failed with error {}", static_cast<int>(count));
      iio_scan_context_destroy(scanCtx);
      return false;
   }

   // Find Pluto devices only (filter by description containing "PlutoSDR").
   std::vector<std::string> plutoUris;
   for (ssize_t i = 0; i < count; ++i)
   {
      const char* desc = iio_context_info_get_description(infoList[i]);
      const char* uri  = iio_context_info_get_uri(infoList[i]);
      if (desc != nullptr && uri != nullptr)
      {
         std::string descStr(desc);
         if (descStr.find("PlutoSDR") != std::string::npos ||
             descStr.find("ADALM-PLUTO") != std::string::npos)
         {
            plutoUris.emplace_back(uri);
         }
      }
   }
   iio_context_info_list_free(infoList);
   iio_scan_context_destroy(scanCtx);

   if (plutoUris.empty())
   {
      GPERROR("No ADALM-PLUTO devices found");
      return false;
   }
   if (deviceIndex < 0 ||
       static_cast<std::size_t>(deviceIndex) >= plutoUris.size())
   {
      GPERROR("Pluto device index {} out of range (found {})",
              deviceIndex, plutoUris.size());
      return false;
   }

   // Connect to the selected Pluto.
   const auto& uri = plutoUris[static_cast<std::size_t>(deviceIndex)];
   auto* ctx = iio_create_context_from_uri(uri.c_str());
   if (ctx == nullptr)
   {
      GPERROR("Failed to create IIO context from URI '{}'", uri);
      return false;
   }
   _ctx = ctx;

   // Locate the AD9361 physical device (control plane: frequency, gain, etc.)
   auto* phyDev = iio_context_find_device(ctx, "ad9361-phy");
   if (phyDev == nullptr)
   {
      GPERROR("ad9361-phy device not found in IIO context");
      PlutoSdrDevice::close();
      return false;
   }
   _phyDev = phyDev;

   // Locate the RX capture device (data plane: I/Q samples).
   auto* rxDev = iio_context_find_device(ctx, "cf-ad9361-lpc");
   if (rxDev == nullptr)
   {
      GPERROR("cf-ad9361-lpc device not found in IIO context");
      PlutoSdrDevice::close();
      return false;
   }
   _rxDev = rxDev;

   // Acquire the I and Q streaming channels.
   auto* rxI = iio_device_find_channel(
      static_cast<iio_device*>(_rxDev), "voltage0", false);
   auto* rxQ = iio_device_find_channel(
      static_cast<iio_device*>(_rxDev), "voltage1", false);
   if (rxI == nullptr || rxQ == nullptr)
   {
      GPERROR("Could not find RX I/Q channels on cf-ad9361-lpc");
      PlutoSdrDevice::close();
      return false;
   }
   _rxI = rxI;
   _rxQ = rxQ;

   // Enable both channels for streaming.
   iio_channel_enable(static_cast<iio_channel*>(_rxI));
   iio_channel_enable(static_cast<iio_channel*>(_rxQ));

   // Set a sensible default: RX RF port to "A_BALANCED" (wideband RX input).
   auto* phyRxCh = iio_device_find_channel(phyDev, "voltage0", false);
   if (phyRxCh != nullptr)
   {
      writeChannelAttr(phyRxCh, "rf_port_select", "A_BALANCED");
   }

   GPINFO("Opened ADALM-PLUTO via '{}'", uri);
   return true;
}

void PlutoSdrDevice::close()
{
   if (_ctx == nullptr)
   {
      return;
   }
   if (PlutoSdrDevice::isStreaming())
   {
      PlutoSdrDevice::stopStreaming();
   }
   // Buffer must be destroyed before the context.
   if (_rxBuf != nullptr)
   {
      iio_buffer_destroy(static_cast<iio_buffer*>(_rxBuf));
      _rxBuf = nullptr;
   }
   iio_context_destroy(static_cast<iio_context*>(_ctx));
   _ctx    = nullptr;
   _phyDev = nullptr;
   _rxDev  = nullptr;
   _rxI    = nullptr;
   _rxQ    = nullptr;
   GPINFO("Closed ADALM-PLUTO device");
}

bool PlutoSdrDevice::isOpen() const
{
   return _ctx != nullptr;
}

// ============================================================================
// Tuning
// ============================================================================

bool PlutoSdrDevice::setCenterFrequency(uint64_t frequencyHz)
{
   if (_phyDev == nullptr)
   {
      return false;
   }
   auto* rxLoCh = iio_device_find_channel(
      static_cast<iio_device*>(_phyDev), "altvoltage0", true);
   if (rxLoCh == nullptr)
   {
      GPERROR("Could not find RX LO channel");
      return false;
   }
   if (!writeChannelAttrLongLong(rxLoCh, "frequency",
                                 static_cast<long long>(frequencyHz)))
   {
      GPERROR("Failed to set RX LO frequency to {} Hz", frequencyHz);
      return false;
   }
   GPINFO("Set center frequency to {} Hz", frequencyHz);
   return true;
}

uint64_t PlutoSdrDevice::getCenterFrequency() const
{
   if (_phyDev == nullptr)
   {
      return 0;
   }
   auto* rxLoCh = iio_device_find_channel(
      static_cast<iio_device*>(_phyDev), "altvoltage0", true);
   if (rxLoCh == nullptr)
   {
      return 0;
   }
   const auto val = readChannelAttrLongLong(rxLoCh, "frequency");
   return (val >= 0) ? static_cast<uint64_t>(val) : 0;
}

// ============================================================================
// Sample rate
// ============================================================================

bool PlutoSdrDevice::setSampleRate(uint32_t rateHz)
{
   if (_phyDev == nullptr)
   {
      return false;
   }
   // ad9361_set_bb_rate configures the sample rate AND the FIR filter chain.
   const int ret = ad9361_set_bb_rate(
      static_cast<iio_device*>(_phyDev), static_cast<unsigned long>(rateHz));
   if (ret < 0)
   {
      GPERROR("ad9361_set_bb_rate({}) failed: {}", rateHz, ret);
      return false;
   }
   GPINFO("Set sample rate to {} Hz", rateHz);
   return true;
}

uint32_t PlutoSdrDevice::getSampleRate() const
{
   if (_phyDev == nullptr)
   {
      return 0;
   }
   auto* rxCh = iio_device_find_channel(
      static_cast<iio_device*>(_phyDev), "voltage0", false);
   if (rxCh == nullptr)
   {
      return 0;
   }
   const auto val = readChannelAttrLongLong(rxCh, "sampling_frequency");
   return (val > 0) ? static_cast<uint32_t>(val) : 0;
}

// ============================================================================
// Gain
// ============================================================================

bool PlutoSdrDevice::setAutoGain(bool enabled)
{
   if (_phyDev == nullptr)
   {
      return false;
   }
   auto* rxCh = iio_device_find_channel(
      static_cast<iio_device*>(_phyDev), "voltage0", false);
   if (rxCh == nullptr)
   {
      return false;
   }
   const char* mode = enabled ? "slow_attack" : "manual";
   if (!writeChannelAttr(rxCh, "gain_control_mode", mode))
   {
      GPERROR("Failed to set gain_control_mode to '{}'", mode);
      return false;
   }
   GPINFO("Gain mode set to {}", mode);
   return true;
}

bool PlutoSdrDevice::setGain(int tenthsDb)
{
   if (_phyDev == nullptr)
   {
      return false;
   }
   auto* rxCh = iio_device_find_channel(
      static_cast<iio_device*>(_phyDev), "voltage0", false);
   if (rxCh == nullptr)
   {
      return false;
   }
   // Switch to manual mode first.
   writeChannelAttr(rxCh, "gain_control_mode", "manual");

   // AD9361 gain attribute is in dB (integer).  ISdrDevice uses tenths-of-dB.
   const auto gainDb = static_cast<long long>(tenthsDb / 10);
   if (!writeChannelAttrLongLong(rxCh, "hardwaregain", gainDb))
   {
      GPERROR("Failed to set hardware gain to {} dB", gainDb);
      return false;
   }
   GPINFO("Set gain to {} dB ({} tenths)", gainDb, tenthsDb);
   return true;
}

int PlutoSdrDevice::getGain() const
{
   if (_phyDev == nullptr)
   {
      return 0;
   }
   auto* rxCh = iio_device_find_channel(
      static_cast<iio_device*>(_phyDev), "voltage0", false);
   if (rxCh == nullptr)
   {
      return 0;
   }
   const auto gainDb = readChannelAttrLongLong(rxCh, "hardwaregain");
   // Return in tenths of dB to match ISdrDevice convention.
   return static_cast<int>(gainDb * 10);
}

std::vector<int> PlutoSdrDevice::getGainValues() const
{
   // AD9361 RX gain range: -1 dB to +73 dB in 1 dB steps.
   // Return in tenths-of-dB to match ISdrDevice convention.
   std::vector<int> gains;
   gains.reserve(75);
   for (int db = -1; db <= 73; ++db)
   {
      gains.push_back(db * 10);
   }
   return gains;
}

// ============================================================================
// Streaming
// ============================================================================

bool PlutoSdrDevice::startStreaming(RawIqCallback callback,
                                   std::size_t bufferSize)
{
   if (_rxDev == nullptr)
   {
      GPERROR("Cannot start streaming — device not open");
      return false;
   }
   if (_streaming)
   {
      GPWARN("Already streaming");
      return false;
   }

   // bufferSize is in bytes of uint8_t I/Q pairs (2 bytes per sample).
   // libiio works in samples, and each sample is 2×int16_t = 4 bytes.
   // Convert so the callback delivers a comparable amount of data.
   const std::size_t samplesPerBuffer = bufferSize / 2;

   // Create the IIO buffer for the RX device.
   auto* buf = iio_device_create_buffer(
      static_cast<iio_device*>(_rxDev), samplesPerBuffer, false);
   if (buf == nullptr)
   {
      GPERROR("iio_device_create_buffer failed");
      return false;
   }
   _rxBuf = buf;

   _callback  = std::move(callback);
   _streaming = true;
   _streamThread =
      std::thread(&PlutoSdrDevice::streamThread, this, samplesPerBuffer);
   return true;
}

void PlutoSdrDevice::stopStreaming()
{
   if (!_streaming)
   {
      return;
   }
   _streaming = false;

   if (_streamThread.joinable())
   {
      _streamThread.join();
   }
   if (_rxBuf != nullptr)
   {
      iio_buffer_destroy(static_cast<iio_buffer*>(_rxBuf));
      _rxBuf = nullptr;
   }
   GPINFO("Streaming stopped");
}

bool PlutoSdrDevice::isStreaming() const
{
   return _streaming;
}

void PlutoSdrDevice::streamThread(std::size_t samplesPerBuffer)
{
   auto* buf = static_cast<iio_buffer*>(_rxBuf);

   // Pre-allocate the uint8_t conversion buffer.
   // Each I/Q sample becomes 2 uint8_t values (matching RTL-SDR format).
   std::vector<uint8_t> convBuf(samplesPerBuffer * 2);

   while (_streaming)
   {
      const auto bytesRead = iio_buffer_refill(buf);
      if (bytesRead < 0)
      {
         if (_streaming)
         {
            GPERROR("iio_buffer_refill failed: {}", static_cast<int>(bytesRead));
         }
         break;
      }

      // Walk the buffer using iio_buffer_step / iio_buffer_first / iio_buffer_end.
      const auto step =
         static_cast<std::size_t>(iio_buffer_step(buf));
      const auto* startPtr =
         static_cast<const uint8_t*>(iio_buffer_first(buf,
            static_cast<iio_channel*>(_rxI)));
      const auto* endPtr =
         static_cast<const uint8_t*>(iio_buffer_end(buf));

      std::size_t outIdx = 0;
      for (const uint8_t* ptr = startPtr;
           ptr < endPtr && outIdx + 1 < convBuf.size();
           ptr += step)
      {
         // Each sample frame contains int16_t I followed by int16_t Q.
         int16_t iRaw = 0;
         int16_t qRaw = 0;
         std::memcpy(&iRaw, ptr, sizeof(int16_t));
         std::memcpy(&qRaw, ptr + sizeof(int16_t), sizeof(int16_t));

         // AD9361 delivers 12-bit signed data in the upper bits of int16_t.
         // Shift right by 4 to get 12-bit range [-2048, 2047], then map
         // to uint8_t [0, 255] to match the RTL-SDR convention.
         convBuf[outIdx++] =
            static_cast<uint8_t>(std::clamp((iRaw >> 4) + 128, 0, 255));
         convBuf[outIdx++] =
            static_cast<uint8_t>(std::clamp((qRaw >> 4) + 128, 0, 255));
      }

      if (outIdx > 0 && _callback)
      {
         _callback(convBuf.data(), outIdx);
      }
   }
   _streaming = false;
}

// ============================================================================
// Device info
// ============================================================================

std::string PlutoSdrDevice::getName() const
{
   if (_ctx == nullptr)
   {
      return "ADALM-PLUTO (not open)";
   }
   const char* name =
      iio_context_get_attr_value(static_cast<iio_context*>(_ctx),
                                 "hw_model");
   return (name != nullptr) ? std::string(name) : "ADALM-PLUTO";
}

std::vector<DeviceInfo> PlutoSdrDevice::enumerateDevices() const
{
   std::vector<DeviceInfo> devices;

   auto* scanCtx = iio_create_scan_context(nullptr, 0);
   if (scanCtx == nullptr)
   {
      return devices;
   }

   struct iio_context_info** infoList = nullptr;
   const auto count = iio_scan_context_get_info_list(scanCtx, &infoList);
   if (count < 0)
   {
      iio_scan_context_destroy(scanCtx);
      return devices;
   }

   int plutoIdx = 0;
   for (ssize_t i = 0; i < count; ++i)
   {
      const char* desc = iio_context_info_get_description(infoList[i]);
      const char* uri  = iio_context_info_get_uri(infoList[i]);
      if (desc == nullptr || uri == nullptr)
      {
         continue;
      }
      std::string descStr(desc);
      if (descStr.find("PlutoSDR") == std::string::npos &&
          descStr.find("ADALM-PLUTO") == std::string::npos)
      {
         continue;
      }

      DeviceInfo info;
      info.index        = plutoIdx++;
      info.name         = descStr;
      info.manufacturer = "Analog Devices";
      info.product      = "ADALM-PLUTO";
      info.serial       = uri;   // URI doubles as a unique identifier.
      devices.push_back(std::move(info));
   }

   iio_context_info_list_free(infoList);
   iio_scan_context_destroy(scanCtx);
   return devices;
}

// ============================================================================
// Helpers
// ============================================================================

bool PlutoSdrDevice::writeChannelAttr(void* channel, const char* attr,
                                      const char* value)
{
   const auto ret = iio_channel_attr_write(
      static_cast<iio_channel*>(channel), attr, value);
   return ret >= 0;
}

bool PlutoSdrDevice::writeChannelAttrLongLong(void* channel, const char* attr,
                                              long long value)
{
   const auto ret = iio_channel_attr_write_longlong(
      static_cast<iio_channel*>(channel), attr, value);
   return ret >= 0;
}

long long PlutoSdrDevice::readChannelAttrLongLong(void* channel,
                                                  const char* attr)
{
   long long val = 0;
   const auto ret = iio_channel_attr_read_longlong(
      static_cast<iio_channel*>(channel), attr, &val);
   return (ret >= 0) ? val : -1;
}

} // namespace SdrEngine
