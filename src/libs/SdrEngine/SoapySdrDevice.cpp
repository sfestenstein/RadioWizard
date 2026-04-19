// Project headers
#include "SoapySdrDevice.h"
#include "GeneralLogger.h"

// Third-party headers
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Types.hpp>
#include <SoapySDR/Version.hpp>

// System headers
#include <cmath>
#include <utility>

namespace SdrEngine
{

// ============================================================================
// Construction / destruction
// ============================================================================

SoapySdrDevice::SoapySdrDevice() = default;

SoapySdrDevice::~SoapySdrDevice()
{
   if (SoapySdrDevice::isStreaming())
   {
      SoapySdrDevice::stopStreaming();
   }
   if (SoapySdrDevice::isOpen())
   {
      SoapySdrDevice::close();
   }
}

// ============================================================================
// Lifecycle
// ============================================================================

bool SoapySdrDevice::open(int deviceIndex)
{
   if (_device != nullptr)
   {
      GPWARN("SoapySdrDevice::open() — device already open, closing first");
      SoapySdrDevice::close();
   }

   // Enumerate all available devices.
   const auto results = SoapySDR::Device::enumerate();
   if (results.empty())
   {
      GPERROR("No SoapySDR devices found");
      return false;
   }

   if (deviceIndex < 0 ||
       static_cast<std::size_t>(deviceIndex) >= results.size())
   {
      GPERROR("Device index {} out of range (found {})",
              deviceIndex, results.size());
      return false;
   }

   const auto& args = results[static_cast<std::size_t>(deviceIndex)];

   auto* dev = SoapySDR::Device::make(args);
   if (dev == nullptr)
   {
      GPERROR("SoapySDR::Device::make() returned nullptr");
      return false;
   }
   _device = dev;

   // Build a human-readable label from the device args.
   auto it = args.find("label");
   if (it != args.end())
   {
      _deviceLabel = it->second;
   }
   else
   {
      it = args.find("driver");
      _deviceLabel = (it != args.end()) ? it->second : "SoapySDR device";
   }

   GPINFO("Opened SoapySDR device: {}", _deviceLabel);

   // Ensure hardware calibration is in a known-good state.  Another
   // application (e.g. GQRX) may have disabled the AD9361's automatic
   // DC-offset tracking or IQ-balance correction, and those settings
   // persist on the device even after the previous application exits.
   resetCalibration();

   return true;
}

void SoapySdrDevice::close()
{
   if (_device == nullptr)
   {
      return;
   }
   if (SoapySdrDevice::isStreaming())
   {
      SoapySdrDevice::stopStreaming();
   }
   SoapySDR::Device::unmake(_device);
   _device = nullptr;
   GPINFO("Closed SoapySDR device");
}

// ============================================================================
// Hardware calibration reset
// ============================================================================

void SoapySdrDevice::resetCalibration()
{
   if (_device == nullptr)
   {
      return;
   }

   // Re-enable automatic DC offset correction if the hardware supports it.
   if (_device->hasDCOffsetMode(SOAPY_SDR_RX, 0))
   {
      _device->setDCOffsetMode(SOAPY_SDR_RX, 0, true);
      GPINFO("Enabled automatic DC offset correction");
   }

   // Re-enable automatic IQ balance correction if the hardware supports it.
   if (_device->hasIQBalanceMode(SOAPY_SDR_RX, 0))
   {
      _device->setIQBalanceMode(SOAPY_SDR_RX, 0, true);
      GPINFO("Enabled automatic IQ balance correction");
   }

   // Set RF analog bandwidth to match the sample rate.
   const double sampleRate = _device->getSampleRate(SOAPY_SDR_RX, 0);
   if (sampleRate > 0.0)
   {
      _device->setBandwidth(SOAPY_SDR_RX, 0, sampleRate);
      GPINFO("Set RF bandwidth to {} Hz (matches sample rate)", sampleRate);
   }
}

bool SoapySdrDevice::isOpen() const
{
   return _device != nullptr;
}

// ============================================================================
// Tuning
// ============================================================================

bool SoapySdrDevice::setCenterFrequency(uint64_t frequencyHz)
{
   if (_device == nullptr)
   {
      return false;
   }
   _device->setFrequency(SOAPY_SDR_RX, 0, static_cast<double>(frequencyHz));
   GPINFO("Set center frequency to {} Hz", frequencyHz);
   return true;
}

uint64_t SoapySdrDevice::getCenterFrequency() const
{
   if (_device == nullptr)
   {
      return 0;
   }
   return static_cast<uint64_t>(_device->getFrequency(SOAPY_SDR_RX, 0));
}

// ============================================================================
// Sample rate
// ============================================================================

bool SoapySdrDevice::setSampleRate(uint32_t rateHz)
{
   if (_device == nullptr)
   {
      return false;
   }
   _device->setSampleRate(SOAPY_SDR_RX, 0, static_cast<double>(rateHz));

   // Keep the RF bandwidth in sync with the sample rate so the analog
   // filter doesn't introduce an unexpected spectral shape.
   _device->setBandwidth(SOAPY_SDR_RX, 0, static_cast<double>(rateHz));

   GPINFO("Set sample rate to {} Hz", rateHz);
   return true;
}

uint32_t SoapySdrDevice::getSampleRate() const
{
   if (_device == nullptr)
   {
      return 0;
   }
   return static_cast<uint32_t>(_device->getSampleRate(SOAPY_SDR_RX, 0));
}

// ============================================================================
// Gain
// ============================================================================

bool SoapySdrDevice::setAutoGain(bool enabled)
{
   if (_device == nullptr)
   {
      return false;
   }
   _device->setGainMode(SOAPY_SDR_RX, 0, enabled);
   GPINFO("Gain mode set to {}", enabled ? "auto" : "manual");
   return true;
}

bool SoapySdrDevice::setGain(int tenthsDb)
{
   if (_device == nullptr)
   {
      return false;
   }
   // ISdrDevice uses tenths-of-dB; SoapySDR uses dB as double.
   const double gainDb = static_cast<double>(tenthsDb) / 10.0;
   _device->setGain(SOAPY_SDR_RX, 0, gainDb);
   GPINFO("Set gain to {} dB ({} tenths)", gainDb, tenthsDb);
   return true;
}

int SoapySdrDevice::getGain() const
{
   if (_device == nullptr)
   {
      return 0;
   }
   const double gainDb = _device->getGain(SOAPY_SDR_RX, 0);
   return static_cast<int>(std::round(gainDb * 10.0));
}

std::vector<int> SoapySdrDevice::getGainValues() const
{
   if (_device == nullptr)
   {
      return {};
   }

   // SoapySDR reports gain as a continuous range. Discretise into 1 dB steps.
   const auto range = _device->getGainRange(SOAPY_SDR_RX, 0);
   const int minDb = static_cast<int>(std::floor(range.minimum()));
   const int maxDb = static_cast<int>(std::ceil(range.maximum()));

   std::vector<int> gains;
   gains.reserve(static_cast<std::size_t>(maxDb - minDb + 1));
   for (int db = minDb; db <= maxDb; ++db)
   {
      gains.push_back(db * 10);   // tenths-of-dB
   }
   return gains;
}

// ============================================================================
// Streaming
// ============================================================================

bool SoapySdrDevice::startStreaming(IqCallback callback,
                                   std::size_t bufferSize)
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

   // Set up the RX stream in CF32 (complex float 32-bit) format.
   // This gives full-precision I/Q samples without any lossy conversion.
   _stream = _device->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32);
   if (_stream == nullptr)
   {
      GPERROR("SoapySDR::Device::setupStream() failed");
      return false;
   }

   if (_device->activateStream(_stream) != 0)
   {
      GPERROR("SoapySDR::Device::activateStream() failed");
      _device->closeStream(_stream);
      _stream = nullptr;
      return false;
   }

   _callback  = std::move(callback);
   _streaming = true;
   _streamThread =
      std::thread(&SoapySdrDevice::streamThread, this, bufferSize);
   return true;
}

void SoapySdrDevice::stopStreaming()
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
   if (_stream != nullptr && _device != nullptr)
   {
      _device->deactivateStream(_stream);
      _device->closeStream(_stream);
      _stream = nullptr;
   }
   GPINFO("Streaming stopped");
}

bool SoapySdrDevice::isStreaming() const
{
   return _streaming;
}

void SoapySdrDevice::streamThread(std::size_t samplesPerBuffer)
{
   // CF32 buffer: each sample is float I + float Q = 8 bytes.
   std::vector<float> cf32Buf(samplesPerBuffer * 2);
   void* buffs[] = {cf32Buf.data()};

   while (_streaming)
   {
      int flags = 0;
      long long timeNs = 0;
      const auto numRead = _device->readStream(
         _stream, buffs, samplesPerBuffer, flags, timeNs, 100000);

      if (numRead < 0)
      {
         if (_streaming)
         {
            GPERROR("SoapySDR readStream error: {}", numRead);
         }
         break;
      }
      if (numRead == 0)
      {
         continue;
      }

      // cf32Buf is interleaved float I/Q — reinterpret as IqSample.
      const auto* samples =
         reinterpret_cast<const IqSample*>(cf32Buf.data());

      if (_callback)
      {
         _callback(samples, static_cast<std::size_t>(numRead));
      }
   }
   _streaming = false;
}

// ============================================================================
// Device info
// ============================================================================

std::string SoapySdrDevice::getName() const
{
   if (_device == nullptr)
   {
      return "SoapySDR (not open)";
   }
   return _deviceLabel;
}

std::vector<DeviceInfo> SoapySdrDevice::enumerateDevices() const
{
   const auto results = SoapySDR::Device::enumerate();
   std::vector<DeviceInfo> devices;
   devices.reserve(results.size());

   for (std::size_t i = 0; i < results.size(); ++i)
   {
      const auto& args = results[i];
      DeviceInfo info;
      info.index = static_cast<int>(i);

      auto it = args.find("label");
      info.name = (it != args.end()) ? it->second : "Unknown";

      it = args.find("driver");
      info.manufacturer = (it != args.end()) ? it->second : "";

      it = args.find("product");
      if (it != args.end())
      {
         info.product = it->second;
      }

      it = args.find("serial");
      if (it != args.end())
      {
         info.serial = it->second;
      }

      devices.push_back(std::move(info));
   }
   return devices;
}

} // namespace SdrEngine
