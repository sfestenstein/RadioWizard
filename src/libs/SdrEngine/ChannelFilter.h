#ifndef CHANNELFILTER_H_
#define CHANNELFILTER_H_

// Project headers
#include "SdrTypes.h"

// System headers
#include <mutex>
#include <vector>

// Forward declarations — avoids exposing liquid/liquid.h in the header.
struct nco_crcf_s;
struct msresamp_crcf_s;
struct firfilt_crcf_s;

namespace SdrEngine
{

/// Frequency-shifts and band-pass filters a selected channel from a
/// wideband I/Q stream, then decimates to match the selected bandwidth.
///
/// Uses liquid-dsp primitives:
///   - `nco_crcf`      — Numerically Controlled Oscillator for frequency shift
///   - `firfilt_crcf`  — FIR low-pass filter (Kaiser window design)
///   - `msresamp_crcf` — Multi-stage arbitrary-rate resampler for decimation
///
/// Thread-safety: all public methods are protected by an internal mutex.
class ChannelFilter
{
public:
   ChannelFilter();
   ~ChannelFilter();

   // Non-copyable.
   ChannelFilter(const ChannelFilter&) = delete;
   ChannelFilter& operator=(const ChannelFilter&) = delete;

   // Non-movable (liquid-dsp objects are opaque pointers).
   ChannelFilter(ChannelFilter&&) = delete;
   ChannelFilter& operator=(ChannelFilter&&) = delete;

   /// Configure the channel to extract.
   ///
   /// @param centerOffsetHz  Offset of the channel centre from the wideband
   ///                        centre frequency, in Hz.  Positive = above centre.
   /// @param bandwidthHz     Desired channel bandwidth in Hz.
   /// @param inputSampleRate Full wideband sample rate in Hz.
   void configure(double centerOffsetHz, double bandwidthHz,
                  double inputSampleRate);

   /// @return true if configure() has been called with valid parameters.
   [[nodiscard]] bool isConfigured() const;

   /// Enable or disable the filter.  When disabled, process() returns
   /// an empty vector (pass-through is handled by the caller).
   void setEnabled(bool enabled);

   /// @return true if the filter is enabled.
   [[nodiscard]] bool isEnabled() const;

   /// Filter a block of wideband I/Q samples.
   ///
   /// @param input  Wideband complex I/Q samples at the input sample rate.
   /// @return Filtered and decimated I/Q samples at the channel rate,
   ///         or an empty vector if the filter is disabled / not configured.
   [[nodiscard]] std::vector<IqSample> process(
      const std::vector<IqSample>& input);

   /// @return The output sample rate after decimation (Hz).
   [[nodiscard]] double getOutputSampleRate() const;

   /// @return The configured channel bandwidth (Hz).
   [[nodiscard]] double getChannelBandwidth() const;

   /// @return The configured centre-frequency offset (Hz).
   [[nodiscard]] double getCenterOffset() const;

   /// Reset internal filter state (e.g. after a frequency change).
   void reset();

private:
   void destroyDspObjects();
   void createDspObjects();

   mutable std::mutex _mutex;

   bool _enabled{false};
   bool _configured{false};

   // Configuration
   double _centerOffsetHz{0.0};
   double _bandwidthHz{0.0};
   double _inputSampleRate{0.0};
   double _outputSampleRate{0.0};
   double _decimationRate{1.0};

   // liquid-dsp objects
   nco_crcf_s* _nco{nullptr};
   firfilt_crcf_s* _filter{nullptr};
   msresamp_crcf_s* _resampler{nullptr};
};

} // namespace SdrEngine

#endif // CHANNELFILTER_H_
