#ifndef DEMODULATOR_H_
#define DEMODULATOR_H_

// Project headers
#include "SdrTypes.h"

// System headers
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

// Forward declarations for liquid-dsp types.
struct freqdem_s;
struct ampmodem_s;
struct msresamp_rrrf_s;
struct iirfilt_rrrf_s;
struct firfilt_rrrf_s;
struct nco_crcf_s;

namespace SdrEngine
{

/**
 * @brief Demodulation mode.
 */
enum class DemodMode : uint8_t
{
   FmMono,    ///< FM broadcast — mono (L+R only)
   FmStereo,  ///< FM broadcast — stereo (L+R, 19 kHz pilot, L-R sub-carrier)
   AM         ///< Amplitude modulation — envelope detection
};

/**
 * @brief Audio output from the demodulator.
 *
 * For mono modes (FmMono, AM) left and right contain the same data.
 * For FmStereo, left and right hold the decoded L and R channels.
 */
struct DemodAudio
{
   std::vector<float> left;
   std::vector<float> right;
};

/**
 * @class Demodulator
 * @brief Demodulates complex I/Q samples to audio using FM or AM.
 *
 * Supports three modes:
 *   - **FM Mono**:   freqdem → de-emphasis → resample
 *   - **FM Stereo**: freqdem → stereo MPX decode (19 kHz pilot PLL,
 *                    38 kHz L-R extraction) → de-emphasis → resample
 *   - **AM**:        envelope detector (ampmodem) → DC-removal → resample
 *
 * Uses liquid-dsp primitives throughout.
 * Designed to consume the output of ChannelFilter.
 *
 * Thread-safety: all public methods are protected by an internal mutex.
 */
class Demodulator
{
public:
   /// Default audio output sample rate.
   static constexpr double DEFAULT_AUDIO_RATE = 48000.0;

   /**
    * @brief Construct an unconfigured Demodulator.
    */
   Demodulator();

   /**
    * @brief Destroy the Demodulator and release resources.
    */
   ~Demodulator();

   // Non-copyable, non-movable.
   Demodulator(const Demodulator&) = delete;
   Demodulator& operator=(const Demodulator&) = delete;
   Demodulator(Demodulator&&) = delete;
   Demodulator& operator=(Demodulator&&) = delete;

   /**
    * @brief Configure the demodulator.
    *
    * @param mode             Demodulation mode.
    * @param inputSampleRate  Sample rate of the incoming IQ data (Hz).
    *                         This should be the ChannelFilter output rate.
    * @param audioSampleRate  Desired audio output rate (Hz). Default 48 kHz.
    */
   void configure(DemodMode mode, double inputSampleRate,
                  double audioSampleRate = DEFAULT_AUDIO_RATE);

   /**
    * @brief Check if the demodulator has been configured.
    * @return true if configure() has been called with valid parameters.
    */
   [[nodiscard]] bool isConfigured() const;

   /**
    * @brief Get the current demodulation mode.
    * @return Current demod mode.
    */
   [[nodiscard]] DemodMode getMode() const;

   /**
    * @brief Demodulate a block of I/Q samples to audio.
    *
    * @param iqSamples  Filtered complex I/Q samples from ChannelFilter.
    * @return Stereo audio (left + right channels at the audio sample rate).
    */
   [[nodiscard]] DemodAudio demodulate(
      const std::vector<IqSample>& iqSamples);

   /**
    * @brief Get the audio output sample rate.
    * @return Audio output sample rate in Hz.
    */
   [[nodiscard]] double getAudioSampleRate() const;

   /**
    * @brief Reset internal state (e.g. after a frequency change).
    */
   void reset();

private:
   void destroyDspObjects();
   void createFmMonoObjects();
   void createFmStereoObjects();
   void createAmObjects();

   // Common resample step (mono input → mono output).
   [[nodiscard]] std::vector<float> resample(
      const std::vector<float>& input) const;

   // FM composite → stereo decode.
   void stereoDecodeBlock(const float* composite, size_t numSamples,
                          std::vector<float>& left,
                          std::vector<float>& right);

   mutable std::mutex _mutex;

   bool _configured{false};
   DemodMode _mode{DemodMode::FmMono};

   // Configuration
   double _inputSampleRate{0.0};
   double _audioSampleRate{DEFAULT_AUDIO_RATE};

   // === FM objects ===
   freqdem_s* _fmDemod{nullptr};

   // De-emphasis (one per channel for stereo).
   iirfilt_rrrf_s* _deemphasisL{nullptr};
   iirfilt_rrrf_s* _deemphasisR{nullptr};

   // === FM Stereo objects ===
   // Band-pass filter to isolate 19 kHz pilot.
   firfilt_rrrf_s* _pilotBpf{nullptr};
   // PLL NCO locked to the pilot, running at 2× (38 kHz).
   nco_crcf_s* _pilotPll{nullptr};
   // Low-pass filter to isolate L+R (mono sum) from composite.
   firfilt_rrrf_s* _monoLpf{nullptr};
   // Low-pass filter to isolate L-R after mixing with 38 kHz.
   firfilt_rrrf_s* _diffLpf{nullptr};

   // === AM objects ===
   ampmodem_s* _amDemod{nullptr};
   // DC-removal high-pass filter for AM audio.
   iirfilt_rrrf_s* _amDcBlock{nullptr};

   // === Common resample ===
   msresamp_rrrf_s* _resamplerL{nullptr};
   msresamp_rrrf_s* _resamplerR{nullptr};
};

/**
 * @brief Convert DemodMode to a human-readable string.
 */
[[nodiscard]] const char* demodModeName(DemodMode mode);

} // namespace SdrEngine

#endif // DEMODULATOR_H_
