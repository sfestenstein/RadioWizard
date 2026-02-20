// Project headers
#include "ChannelFilter.h"
#include "GeneralLogger.h"

// Third-party headers
#include <liquid/liquid.h>

// System headers
#include <algorithm>
#include <cmath>
#include <numbers>

namespace SdrEngine
{

// ============================================================================
// Construction / destruction
// ============================================================================

ChannelFilter::ChannelFilter() = default;

ChannelFilter::~ChannelFilter()
{
   destroyDspObjects();
}

// ============================================================================
// Configuration
// ============================================================================

void ChannelFilter::configure(double centerOffsetHz, double bandwidthHz,
                              double inputSampleRate)
{
   const std::lock_guard<std::mutex> lock(_mutex);

   if (bandwidthHz <= 0.0 || inputSampleRate <= 0.0)
   {
      GPWARN("ChannelFilter::configure: invalid parameters "
             "(bw={}, rate={})", bandwidthHz, inputSampleRate);
      return;
   }

   // Clamp bandwidth to the input sample rate.
   const double clampedBw = std::min(bandwidthHz, inputSampleRate);

   // Check if configuration actually changed.
   if (_configured &&
       _centerOffsetHz == centerOffsetHz &&
       _bandwidthHz == clampedBw &&
       _inputSampleRate == inputSampleRate)
   {
      return;
   }

   _centerOffsetHz  = centerOffsetHz;
   _bandwidthHz     = clampedBw;
   _inputSampleRate = inputSampleRate;

   // Compute decimation: output rate should be at least the channel BW.
   // We pick the largest integer decimation that keeps the output rate ≥ BW.
   const double idealDecimation = inputSampleRate / clampedBw;
   _decimationRate  = std::max(1.0, std::floor(idealDecimation));
   _outputSampleRate = inputSampleRate / _decimationRate;

   destroyDspObjects();
   createDspObjects();
   _configured = true;

   GPINFO("ChannelFilter configured: offset={:.0f} Hz, bw={:.0f} Hz, "
          "decim={:.0f}x, output rate={:.0f} Hz",
          _centerOffsetHz, _bandwidthHz, _decimationRate, _outputSampleRate);
}

void ChannelFilter::configureFromMinMax(double minFreqHz, double maxFreqHz,
                                        double centerFreqHz, double inputSampleRate)
{
   // Convert min/max frequencies to center offset and bandwidth.
   const double bandwidthHz = maxFreqHz - minFreqHz;
   const double channelCenterHz = (minFreqHz + maxFreqHz) / 2.0;
   const double centerOffsetHz = channelCenterHz - centerFreqHz;

   configure(centerOffsetHz, bandwidthHz, inputSampleRate);
}

bool ChannelFilter::isConfigured() const
{
   const std::lock_guard<std::mutex> lock(_mutex);
   return _configured;
}

void ChannelFilter::setEnabled(bool enabled)
{
   const std::lock_guard<std::mutex> lock(_mutex);
   _enabled = enabled;
}

bool ChannelFilter::isEnabled() const
{
   const std::lock_guard<std::mutex> lock(_mutex);
   return _enabled;
}

// ============================================================================
// Processing
// ============================================================================

std::vector<IqSample> ChannelFilter::process(
   const std::vector<IqSample>& input)
{
   const std::lock_guard<std::mutex> lock(_mutex);

   if (!_enabled || !_configured || input.empty())
   {
      return {};
   }

   // 1. Frequency-shift: mix down the selected channel to baseband.
   std::vector<IqSample> shifted(input.size());
   for (std::size_t i = 0; i < input.size(); ++i)
   {
      liquid_float_complex out;
      nco_crcf_mix_down(_nco, input[i], &out);
      nco_crcf_step(_nco);
      shifted[i] = out;
   }

   // 2. Low-pass filter to the channel bandwidth.
   std::vector<IqSample> filtered(shifted.size());
   for (std::size_t i = 0; i < shifted.size(); ++i)
   {
      liquid_float_complex out;
      firfilt_crcf_push(_filter, shifted[i]);
      firfilt_crcf_execute(_filter, &out);
      filtered[i] = out;
   }

   // 3. Decimate (arbitrary-rate resampler).
   // Maximum possible output length.
   const auto maxOut = static_cast<std::size_t>(
      std::ceil(static_cast<double>(filtered.size()) / _decimationRate) + 16);
   std::vector<IqSample> decimated(maxOut);

   unsigned int numWritten = 0;
   msresamp_crcf_execute(
      _resampler,
      reinterpret_cast<liquid_float_complex*>(filtered.data()),
      static_cast<unsigned int>(filtered.size()),
      reinterpret_cast<liquid_float_complex*>(decimated.data()),
      &numWritten);

   decimated.resize(numWritten);
   return decimated;
}

// ============================================================================
// Getters
// ============================================================================

double ChannelFilter::getOutputSampleRate() const
{
   const std::lock_guard<std::mutex> lock(_mutex);
   return _outputSampleRate;
}

double ChannelFilter::getChannelBandwidth() const
{
   const std::lock_guard<std::mutex> lock(_mutex);
   return _bandwidthHz;
}

double ChannelFilter::getCenterOffset() const
{
   const std::lock_guard<std::mutex> lock(_mutex);
   return _centerOffsetHz;
}

void ChannelFilter::reset()
{
   const std::lock_guard<std::mutex> lock(_mutex);
   if (_nco != nullptr)
   {
      nco_crcf_reset(_nco);
   }
   if (_filter != nullptr)
   {
      firfilt_crcf_reset(_filter);
   }
   if (_resampler != nullptr)
   {
      msresamp_crcf_reset(_resampler);
   }
}

// ============================================================================
// Internal helpers
// ============================================================================

void ChannelFilter::destroyDspObjects()
{
   if (_nco != nullptr)
   {
      nco_crcf_destroy(_nco);
      _nco = nullptr;
   }
   if (_filter != nullptr)
   {
      firfilt_crcf_destroy(_filter);
      _filter = nullptr;
   }
   if (_resampler != nullptr)
   {
      msresamp_crcf_destroy(_resampler);
      _resampler = nullptr;
   }
}

void ChannelFilter::createDspObjects()
{
   // --- NCO: frequency shift ---
   // Convert offset in Hz to normalised angular frequency (radians/sample).
   const double normFreq =
      2.0 * std::numbers::pi * _centerOffsetHz / _inputSampleRate;
   _nco = nco_crcf_create(LIQUID_NCO);
   nco_crcf_set_frequency(_nco, static_cast<float>(normFreq));

   // --- FIR low-pass filter ---
   // Cutoff = half the channel bandwidth, normalised to the input rate.
   // Use a Kaiser-windowed FIR with 60 dB stop-band attenuation.
   auto cutoffNorm = static_cast<float>(_bandwidthHz / (2.0 * _inputSampleRate));
   constexpr unsigned int FILTER_SEMI_LEN = 25;  // filter length = 2*m+1
   constexpr float STOP_BAND_ATTEN = 60.0F;      // dB

   _filter = firfilt_crcf_create_kaiser(
      (2 * FILTER_SEMI_LEN) + 1,
      cutoffNorm,
      STOP_BAND_ATTEN,
      0.0F);   // fractional sample offset
   firfilt_crcf_set_scale(_filter, 2.0F * cutoffNorm);  // unity passband gain

   // --- Arbitrary-rate resampler (decimator) ---
   const float resampRate = 1.0F / static_cast<float>(_decimationRate);
   constexpr float RESAMP_ATTEN = 60.0F;  // stop-band attenuation dB
   _resampler = msresamp_crcf_create(resampRate, RESAMP_ATTEN);
}

} // namespace SdrEngine
