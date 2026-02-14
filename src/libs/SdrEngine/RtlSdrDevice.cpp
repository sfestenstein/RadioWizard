// Project headers
#include "RtlSdrDevice.h"
#include "GeneralLogger.h"

// Third-party headers
#include <rtl-sdr.h>

// System headers
#include <algorithm>
#include <array>
#include <cstring>
#include <utility>

namespace SdrEngine
{

// ============================================================================
// Construction / destruction
// ============================================================================

RtlSdrDevice::RtlSdrDevice() = default;

RtlSdrDevice::~RtlSdrDevice()
{
   if (RtlSdrDevice::isStreaming())
   {
      RtlSdrDevice::stopStreaming();
   }
   if (RtlSdrDevice::isOpen())
   {
      RtlSdrDevice::close();
   }
}

// ============================================================================
// Lifecycle
// ============================================================================

bool RtlSdrDevice::open(int deviceIndex)
{
   if (_device != nullptr)
   {
      GPWARN("RtlSdrDevice::open() — device already open, closing first");
      RtlSdrDevice::close();
   }

   auto* dev = static_cast<rtlsdr_dev_t*>(nullptr);
   const int result = rtlsdr_open(&dev, static_cast<uint32_t>(deviceIndex));
   if (result < 0)
   {
      GPERROR("rtlsdr_open failed with error code {}", result);
      return false;
   }

   _device = dev;

   // Reset the endpoint buffer to flush stale data.
   if (rtlsdr_reset_buffer(static_cast<rtlsdr_dev_t*>(_device)) < 0)
   {
      GPWARN("rtlsdr_reset_buffer failed — streaming may produce initial junk");
   }

   GPINFO("Opened RTL-SDR device index {}", deviceIndex);
   return true;
}

void RtlSdrDevice::close()
{
   if (_device == nullptr)
   {
      return;
   }
   if (RtlSdrDevice::isStreaming())
   {
      RtlSdrDevice::stopStreaming();
   }
   rtlsdr_close(static_cast<rtlsdr_dev_t*>(_device));
   _device = nullptr;
   GPINFO("Closed RTL-SDR device");
}

bool RtlSdrDevice::isOpen() const
{
   return _device != nullptr;
}

// ============================================================================
// Tuning
// ============================================================================

bool RtlSdrDevice::setCenterFrequency(uint64_t frequencyHz)
{
   if (_device == nullptr)
   {
      return false;
   }
   const int result =
      rtlsdr_set_center_freq(static_cast<rtlsdr_dev_t*>(_device),
                             static_cast<uint32_t>(frequencyHz));
   if (result < 0)
   {
      GPERROR("rtlsdr_set_center_freq({}) failed: {}", frequencyHz, result);
      return false;
   }
   GPINFO("Set center frequency to {} Hz", frequencyHz);
   return true;
}

uint64_t RtlSdrDevice::getCenterFrequency() const
{
   if (_device == nullptr)
   {
      return 0;
   }
   return rtlsdr_get_center_freq(static_cast<rtlsdr_dev_t*>(_device));
}

// ============================================================================
// Sample rate
// ============================================================================

bool RtlSdrDevice::setSampleRate(uint32_t rateHz)
{
   if (_device == nullptr)
   {
      return false;
   }
   const int result =
      rtlsdr_set_sample_rate(static_cast<rtlsdr_dev_t*>(_device), rateHz);
   if (result < 0)
   {
      GPERROR("rtlsdr_set_sample_rate({}) failed: {}", rateHz, result);
      return false;
   }
   GPINFO("Set sample rate to {} Hz", rateHz);
   return true;
}

uint32_t RtlSdrDevice::getSampleRate() const
{
   if (_device == nullptr)
   {
      return 0;
   }
   return rtlsdr_get_sample_rate(static_cast<rtlsdr_dev_t*>(_device));
}

// ============================================================================
// Gain
// ============================================================================

bool RtlSdrDevice::setAutoGain(bool enabled)
{
   if (_device == nullptr)
   {
      return false;
   }
   auto* dev = static_cast<rtlsdr_dev_t*>(_device);

   // tuner_gain_mode: 0 = auto, 1 = manual
   const int mode = enabled ? 0 : 1;
   const int result = rtlsdr_set_tuner_gain_mode(dev, mode);
   if (result < 0)
   {
      GPERROR("rtlsdr_set_tuner_gain_mode({}) failed: {}", mode, result);
      return false;
   }

   // Also enable AGC at the RTL2832U level when auto is requested.
   rtlsdr_set_agc_mode(dev, enabled ? 1 : 0);

   GPINFO("Gain mode set to {}", enabled ? "auto" : "manual");
   return true;
}

bool RtlSdrDevice::setGain(int tenthsDb)
{
   if (_device == nullptr)
   {
      return false;
   }
   // Ensure manual mode first.
   rtlsdr_set_tuner_gain_mode(static_cast<rtlsdr_dev_t*>(_device), 1);

   const int result =
      rtlsdr_set_tuner_gain(static_cast<rtlsdr_dev_t*>(_device), tenthsDb);
   if (result < 0)
   {
      GPERROR("rtlsdr_set_tuner_gain({}) failed: {}", tenthsDb, result);
      return false;
   }
   GPINFO("Set gain to {} (tenths of dB)", tenthsDb);
   return true;
}

int RtlSdrDevice::getGain() const
{
   if (_device == nullptr)
   {
      return 0;
   }
   return rtlsdr_get_tuner_gain(static_cast<rtlsdr_dev_t*>(_device));
}

std::vector<int> RtlSdrDevice::getGainValues() const
{
   std::vector<int> gains;
   if (_device == nullptr)
   {
      return gains;
   }
   auto* dev = static_cast<rtlsdr_dev_t*>(_device);

   // First call: query how many gain values are available.
   const int count = rtlsdr_get_tuner_gains(dev, nullptr);
   if (count <= 0)
   {
      return gains;
   }

   gains.resize(static_cast<std::size_t>(count));
   rtlsdr_get_tuner_gains(dev, gains.data());
   std::ranges::sort(gains);
   return gains;
}

// ============================================================================
// Streaming
// ============================================================================

bool RtlSdrDevice::startStreaming(RawIqCallback callback, std::size_t bufferSize)
{
   if (_device == nullptr)
   {
      GPERROR("Cannot start streaming — device not open");
      return false;
   }
   if (_streaming)
   {
      GPWARN("Already streaming");
      return false;
   }

   _callback = std::move(callback);
   _streaming = true;
   _asyncThread = std::thread(&RtlSdrDevice::asyncReadThread, this, bufferSize);
   return true;
}

void RtlSdrDevice::stopStreaming()
{
   if (!_streaming)
   {
      return;
   }

   // Signal the async read loop to exit.
   _streaming = false;
   rtlsdr_cancel_async(static_cast<rtlsdr_dev_t*>(_device));

   if (_asyncThread.joinable())
   {
      _asyncThread.join();
   }
   GPINFO("Streaming stopped");
}

bool RtlSdrDevice::isStreaming() const
{
   return _streaming;
}

void RtlSdrDevice::asyncReadThread(std::size_t bufferSize)
{
   auto* dev = static_cast<rtlsdr_dev_t*>(_device);

   // Number of asynchronous buffers — 0 lets the library choose a default.
   constexpr uint32_t NUM_ASYNC_BUFFERS = 0;

   const int result = rtlsdr_read_async(
      dev, &RtlSdrDevice::rtlsdrCallback, this,
      NUM_ASYNC_BUFFERS, static_cast<uint32_t>(bufferSize));

   if (result < 0 && _streaming)
   {
      GPERROR("rtlsdr_read_async exited with error code {}", result);
   }
   _streaming = false;
}

// Static callback trampoline
void RtlSdrDevice::rtlsdrCallback(unsigned char* buf, uint32_t len, void* ctx)
{
   auto* self = static_cast<RtlSdrDevice*>(ctx);
   if (self != nullptr && self->_streaming && self->_callback)
   {
      self->_callback(buf, static_cast<std::size_t>(len));
   }
}

// ============================================================================
// Device info
// ============================================================================

std::string RtlSdrDevice::getName() const
{
   if (_device == nullptr)
   {
      return "RTL-SDR (not open)";
   }
   // Use index 0 for the currently open device.
   const char* name = rtlsdr_get_device_name(0);
   return (name != nullptr) ? std::string(name) : "RTL-SDR";
}

std::vector<DeviceInfo> RtlSdrDevice::enumerateDevices() const
{
   const auto count = static_cast<int>(rtlsdr_get_device_count());
   std::vector<DeviceInfo> devices;
   devices.reserve(static_cast<std::size_t>(count));

   for (int i = 0; i < count; ++i)
   {
      DeviceInfo info;
      info.index = i;

      const char* name = rtlsdr_get_device_name(static_cast<uint32_t>(i));
      info.name = (name != nullptr) ? name : "Unknown";

      std::array<char, 256> manufact = {};
      std::array<char, 256> product  = {};
      std::array<char, 256> serial   = {};
      rtlsdr_get_device_usb_strings(static_cast<uint32_t>(i),
                                    manufact.data(), product.data(),
                                    serial.data());
      info.manufacturer = manufact.data();
      info.product      = product.data();
      info.serial       = serial.data();

      devices.push_back(std::move(info));
   }
   return devices;
}

} // namespace SdrEngine
