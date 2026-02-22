// Project headers
#include "Demodulator.h"
#include "GeneralLogger.h"

// Third-party headers (liquid-dsp)
#include <liquid/liquid.h>

// System headers
#include <cmath>
#include <cstddef>
#include <numbers>

namespace SdrEngine
{

// ============================================================================
// Construction / destruction
// ============================================================================

Demodulator::Demodulator() = default;

Demodulator::~Demodulator()
{
   destroyDspObjects();
}

// ============================================================================
// Configuration
// ============================================================================

void Demodulator::configure(DemodMode mode, double inputSampleRate,
                            double audioSampleRate)
{
   std::lock_guard lock(_mutex);

   if (inputSampleRate <= 0.0 || audioSampleRate <= 0.0)
   {
      GPERROR("Demodulator::configure: invalid rates ({}, {})",
              inputSampleRate, audioSampleRate);
      return;
   }

   _mode = mode;
   _inputSampleRate = inputSampleRate;
   _audioSampleRate = audioSampleRate;

   destroyDspObjects();

   switch (_mode)
   {
      case DemodMode::FmMono:
         createFmMonoObjects();
         break;
      case DemodMode::FmStereo:
         createFmStereoObjects();
         break;
      case DemodMode::AM:
         createAmObjects();
         break;
   }

   _configured = true;
   GPINFO("Demodulator configured: mode={}, input={:.0f} Hz, audio={:.0f} Hz",
          demodModeName(_mode), _inputSampleRate, _audioSampleRate);
}

bool Demodulator::isConfigured() const
{
   std::lock_guard lock(_mutex);
   return _configured;
}

DemodMode Demodulator::getMode() const
{
   std::lock_guard lock(_mutex);
   return _mode;
}

// ============================================================================
// Demodulation
// ============================================================================

DemodAudio Demodulator::demodulate(const std::vector<IqSample>& iqSamples)
{
   std::lock_guard lock(_mutex);

   if (!_configured || iqSamples.empty())
   {
      return {};
   }

   const auto numIn = static_cast<unsigned int>(iqSamples.size());

   switch (_mode)
   {
      // ---------------------------------------------------------------
      // FM Mono
      // ---------------------------------------------------------------
      case DemodMode::FmMono:
      {
         if (_fmDemod == nullptr)
         {
            return {};
         }

         // FM discriminator.
         std::vector<float> baseband(numIn);
         freqdem_demodulate_block(
            _fmDemod,
            const_cast<liquid_float_complex*>(
               reinterpret_cast<const liquid_float_complex*>(
                  iqSamples.data())),
            numIn, baseband.data());

         // De-emphasis.
         if (_deemphasisL != nullptr)
         {
            for (unsigned int i = 0; i < numIn; ++i)
            {
               float out = 0.0F;
               iirfilt_rrrf_execute(_deemphasisL, baseband[i], &out);
               baseband[i] = out;
            }
         }

         // Resample.
         auto audio = resample(baseband);

         DemodAudio result;
         result.right = audio;    // copy for stereo output
         result.left = std::move(audio);
         return result;
      }

      // ---------------------------------------------------------------
      // FM Stereo
      // ---------------------------------------------------------------
      case DemodMode::FmStereo:
      {
         if (_fmDemod == nullptr)
         {
            return {};
         }

         // FM discriminator → composite MPX signal.
         std::vector<float> composite(numIn);
         freqdem_demodulate_block(
            _fmDemod,
            const_cast<liquid_float_complex*>(
               reinterpret_cast<const liquid_float_complex*>(
                  iqSamples.data())),
            numIn, composite.data());

         // Stereo decode.
         std::vector<float> leftBaseband;
         std::vector<float> rightBaseband;
         stereoDecodeBlock(composite.data(), numIn,
                           leftBaseband, rightBaseband);

         // De-emphasis on each channel.
         if (_deemphasisL != nullptr)
         {
            for (size_t i = 0; i < leftBaseband.size(); ++i)
            {
               float out = 0.0F;
               iirfilt_rrrf_execute(_deemphasisL, leftBaseband[i], &out);
               leftBaseband[i] = out;
            }
         }
         if (_deemphasisR != nullptr)
         {
            for (size_t i = 0; i < rightBaseband.size(); ++i)
            {
               float out = 0.0F;
               iirfilt_rrrf_execute(_deemphasisR, rightBaseband[i], &out);
               rightBaseband[i] = out;
            }
         }

         // Resample each channel separately.
         DemodAudio result;
         result.left = resample(leftBaseband);
         // For the right channel, use _resamplerR directly.
         if (_resamplerR != nullptr)
         {
            const auto numR = static_cast<unsigned int>(rightBaseband.size());
            const float ratio =
               static_cast<float>(_audioSampleRate / _inputSampleRate);
            const auto maxOut =
               static_cast<size_t>(static_cast<float>(numR) * ratio + 64);
            result.right.resize(maxOut);
            unsigned int numWritten = 0;
            msresamp_rrrf_execute(_resamplerR,
                                 rightBaseband.data(), numR,
                                 result.right.data(), &numWritten);
            result.right.resize(numWritten);
         }
         else
         {
            result.right = std::move(rightBaseband);
         }
         return result;
      }

      // ---------------------------------------------------------------
      // AM
      // ---------------------------------------------------------------
      case DemodMode::AM:
      {
         if (_amDemod == nullptr)
         {
            return {};
         }

         // Envelope detection sample-by-sample.
         std::vector<float> baseband(numIn);
         for (unsigned int i = 0; i < numIn; ++i)
         {
            liquid_float_complex sample;
            sample.real(iqSamples[i].real());
            sample.imag(iqSamples[i].imag());
            ampmodem_demodulate(_amDemod, sample, &baseband[i]);
         }

         // DC-removal high-pass filter.
         if (_amDcBlock != nullptr)
         {
            for (unsigned int i = 0; i < numIn; ++i)
            {
               float out = 0.0F;
               iirfilt_rrrf_execute(_amDcBlock, baseband[i], &out);
               baseband[i] = out;
            }
         }

         // Resample.
         auto audio = resample(baseband);

         DemodAudio result;
         result.right = audio;
         result.left = std::move(audio);
         return result;
      }
   }

   return {};
}

double Demodulator::getAudioSampleRate() const
{
   std::lock_guard lock(_mutex);
   return _audioSampleRate;
}

// ============================================================================
// Reset
// ============================================================================

void Demodulator::reset()
{
   std::lock_guard lock(_mutex);
   if (_configured)
   {
      auto savedMode = _mode;
      auto savedInputRate = _inputSampleRate;
      auto savedAudioRate = _audioSampleRate;

      destroyDspObjects();

      _mode = savedMode;
      _inputSampleRate = savedInputRate;
      _audioSampleRate = savedAudioRate;

      switch (_mode)
      {
         case DemodMode::FmMono:   createFmMonoObjects();   break;
         case DemodMode::FmStereo: createFmStereoObjects(); break;
         case DemodMode::AM:       createAmObjects();        break;
      }
   }
}

// ============================================================================
// Internal — destroy all DSP objects
// ============================================================================

void Demodulator::destroyDspObjects()
{
   // FM
   if (_fmDemod != nullptr)
   {
      freqdem_destroy(_fmDemod);
      _fmDemod = nullptr;
   }

   // De-emphasis
   if (_deemphasisL != nullptr)
   {
      iirfilt_rrrf_destroy(_deemphasisL);
      _deemphasisL = nullptr;
   }
   if (_deemphasisR != nullptr)
   {
      iirfilt_rrrf_destroy(_deemphasisR);
      _deemphasisR = nullptr;
   }

   // Stereo
   if (_pilotBpf != nullptr)
   {
      firfilt_rrrf_destroy(_pilotBpf);
      _pilotBpf = nullptr;
   }
   if (_pilotPll != nullptr)
   {
      nco_crcf_destroy(_pilotPll);
      _pilotPll = nullptr;
   }
   if (_monoLpf != nullptr)
   {
      firfilt_rrrf_destroy(_monoLpf);
      _monoLpf = nullptr;
   }
   if (_diffLpf != nullptr)
   {
      firfilt_rrrf_destroy(_diffLpf);
      _diffLpf = nullptr;
   }

   // AM
   if (_amDemod != nullptr)
   {
      ampmodem_destroy(_amDemod);
      _amDemod = nullptr;
   }
   if (_amDcBlock != nullptr)
   {
      iirfilt_rrrf_destroy(_amDcBlock);
      _amDcBlock = nullptr;
   }

   // Resamplers
   if (_resamplerL != nullptr)
   {
      msresamp_rrrf_destroy(_resamplerL);
      _resamplerL = nullptr;
   }
   if (_resamplerR != nullptr)
   {
      msresamp_rrrf_destroy(_resamplerR);
      _resamplerR = nullptr;
   }
}

// ============================================================================
// Internal — create DSP objects for each mode
// ============================================================================

namespace
{
/// Build a 1st-order IIR de-emphasis filter with time constant τ (seconds).
iirfilt_rrrf_s* createDeemphasisFilter(double tau, double sampleRate)
{
   const float alpha =
      static_cast<float>(std::exp(-1.0 / (tau * sampleRate)));
   float b[2] = {1.0F - alpha, 0.0F};
   float a[2] = {1.0F, -alpha};
   return iirfilt_rrrf_create(b, 2, a, 2);
}

/// Build a 1st-order IIR DC-blocking high-pass filter.
/// H(z) = (1 - z^-1) / (1 - α z^-1)  with α close to 1.
iirfilt_rrrf_s* createDcBlockFilter(float alpha = 0.999F)
{
   float b[2] = {1.0F, -1.0F};
   float a[2] = {1.0F, -alpha};
   return iirfilt_rrrf_create(b, 2, a, 2);
}

/// Build an msresamp_rrrf resampler, or nullptr if ratio ≈ 1.
msresamp_rrrf_s* createResampler(double inRate, double outRate)
{
   const float ratio = static_cast<float>(outRate / inRate);
   if (std::fabs(ratio - 1.0F) < 0.001F)
   {
      return nullptr;
   }
   return msresamp_rrrf_create(ratio, 60.0F);
}
} // anonymous namespace

// ----- FM Mono -----

void Demodulator::createFmMonoObjects()
{
   constexpr double FM_DEVIATION_HZ = 75000.0;
   const float kf = static_cast<float>(FM_DEVIATION_HZ / _inputSampleRate);
   _fmDemod = freqdem_create(kf);

   constexpr double TAU = 75.0e-6;
   _deemphasisL = createDeemphasisFilter(TAU, _inputSampleRate);

   _resamplerL = createResampler(_inputSampleRate, _audioSampleRate);
   if (_resamplerL != nullptr)
   {
      GPINFO("Demodulator FM Mono: resampler ratio = {:.4f}",
             _audioSampleRate / _inputSampleRate);
   }
}

// ----- FM Stereo -----

void Demodulator::createFmStereoObjects()
{
   // FM demodulator — same as mono (gives composite MPX baseband).
   constexpr double FM_DEVIATION_HZ = 75000.0;
   const float kf = static_cast<float>(FM_DEVIATION_HZ / _inputSampleRate);
   _fmDemod = freqdem_create(kf);

   // --- Stereo MPX decode filters ---
   // All filters work at _inputSampleRate (the FM baseband rate).
   const auto fs = static_cast<float>(_inputSampleRate);

   // 1) Band-pass filter for 19 kHz pilot tone.
   //    Centre = 19 kHz, BW ≈ 500 Hz.
   //    Design a real-valued band-pass via liquid FIR.
   {
      constexpr float PILOT_FREQ = 19000.0F;
      constexpr float BPF_BW = 500.0F; // ±250 Hz around 19 kHz
      // Normalised frequencies for liquid firdespm method:
      // Use a relatively short filter (61 taps) for low latency.
      const unsigned int bpfLen = 127;
      const float f0 = (PILOT_FREQ - BPF_BW / 2.0F) / fs;
      const float f1 = (PILOT_FREQ + BPF_BW / 2.0F) / fs;
      // Design using parks-mcclellan (firdespm).
      // Bands: [0, f0-δ], [f0, f1], [f1+δ, 0.5]
      const float delta = 0.002F; // transition band width (normalised)
      constexpr unsigned int NUM_BANDS = 3;
      float bands[NUM_BANDS * 2] = {
         0.0F,          f0 - delta,   // stop-band
         f0,            f1,           // pass-band
         f1 + delta,    0.5F          // stop-band
      };
      float des[NUM_BANDS] = {0.0F, 1.0F, 0.0F};
      float weights[NUM_BANDS] = {1.0F, 1.0F, 1.0F};
      liquid_firdespm_btype btype = LIQUID_FIRDESPM_BANDPASS;
      std::vector<float> h(bpfLen);
      firdespm_run(bpfLen, NUM_BANDS, bands, des, weights, nullptr,
                   btype, h.data());
      _pilotBpf = firfilt_rrrf_create(h.data(), bpfLen);
   }

   // 2) Pilot PLL (NCO running at 19 kHz, we use it doubled for 38 kHz).
   {
      const float pilotOmega =
         2.0F * static_cast<float>(std::numbers::pi) * 19000.0F / fs;
      _pilotPll = nco_crcf_create(LIQUID_NCO);
      nco_crcf_set_frequency(_pilotPll, pilotOmega);
      nco_crcf_pll_set_bandwidth(_pilotPll, 0.002F);
   }

   // 3) Low-pass filter for L+R (mono): cutoff 15 kHz.
   {
      const unsigned int lpfLen = 65;
      const float fc = 15000.0F / fs; // normalised cutoff
      std::vector<float> h(lpfLen);
      liquid_firdes_kaiser(lpfLen, fc, 60.0F, 0.0F, h.data());
      _monoLpf = firfilt_rrrf_create(h.data(), lpfLen);
   }

   // 4) Low-pass filter for L-R (after mixing down from 38 kHz): cutoff 15 kHz.
   {
      const unsigned int lpfLen = 65;
      const float fc = 15000.0F / fs;
      std::vector<float> h(lpfLen);
      liquid_firdes_kaiser(lpfLen, fc, 60.0F, 0.0F, h.data());
      _diffLpf = firfilt_rrrf_create(h.data(), lpfLen);
   }

   // De-emphasis for both channels.
   constexpr double TAU = 75.0e-6;
   _deemphasisL = createDeemphasisFilter(TAU, _inputSampleRate);
   _deemphasisR = createDeemphasisFilter(TAU, _inputSampleRate);

   // Resampler — one per channel.
   _resamplerL = createResampler(_inputSampleRate, _audioSampleRate);
   _resamplerR = createResampler(_inputSampleRate, _audioSampleRate);

   if (_resamplerL != nullptr)
   {
      GPINFO("Demodulator FM Stereo: resampler ratio = {:.4f}",
             _audioSampleRate / _inputSampleRate);
   }
}

// ----- AM -----

void Demodulator::createAmObjects()
{
   // AM envelope detector: DSB mode, suppressed carrier = false,
   // modulation index guess of 1.0.
   _amDemod = ampmodem_create(1.0F, LIQUID_AMPMODEM_DSB, 0);

   // DC-blocking filter to remove carrier offset.
   _amDcBlock = createDcBlockFilter(0.999F);

   // Resampler.
   _resamplerL = createResampler(_inputSampleRate, _audioSampleRate);
   if (_resamplerL != nullptr)
   {
      GPINFO("Demodulator AM: resampler ratio = {:.4f}",
             _audioSampleRate / _inputSampleRate);
   }
}

// ============================================================================
// Internal — resample helper
// ============================================================================

std::vector<float> Demodulator::resample(const std::vector<float>& input) const
{
   if (input.empty())
   {
      return {};
   }

   // If no resampler needed, pass through.
   // Select the resampler for the "left" channel; for stereo right, caller
   // should resample separately using _resamplerR.  This helper always uses L.
   msresamp_rrrf_s* resampler = _resamplerL;
   if (resampler == nullptr)
   {
      return input;
   }

   const auto numIn = static_cast<unsigned int>(input.size());
   const float ratio =
      static_cast<float>(_audioSampleRate / _inputSampleRate);
   const auto maxOut =
      static_cast<size_t>(static_cast<float>(numIn) * ratio + 64);
   std::vector<float> output(maxOut);
   unsigned int numWritten = 0;

   msresamp_rrrf_execute(resampler,
                         const_cast<float*>(input.data()), numIn,
                         output.data(), &numWritten);
   output.resize(numWritten);
   return output;
}

// ============================================================================
// Internal — FM Stereo MPX decode
// ============================================================================

void Demodulator::stereoDecodeBlock(const float* composite, size_t numSamples,
                                    std::vector<float>& left,
                                    std::vector<float>& right)
{
   left.resize(numSamples);
   right.resize(numSamples);

   for (size_t i = 0; i < numSamples; ++i)
   {
      const float x = composite[i];

      // ---- Extract mono (L+R) via low-pass filter ----
      float mono = 0.0F;
      firfilt_rrrf_push(_monoLpf, x);
      firfilt_rrrf_execute(_monoLpf, &mono);

      // ---- Extract 19 kHz pilot ----
      float pilot = 0.0F;
      firfilt_rrrf_push(_pilotBpf, x);
      firfilt_rrrf_execute(_pilotBpf, &pilot);

      // ---- PLL: lock NCO to pilot ----
      // Compute phase error: pilot * sin(NCO) — pushes NCO towards lock.
      liquid_float_complex ncoOut;
      nco_crcf_cexpf(_pilotPll, &ncoOut);
      // Phase error = pilot * Im(nco) = pilot * sin(θ).
      float phaseError = pilot * ncoOut.imag();
      nco_crcf_pll_step(_pilotPll, phaseError);
      nco_crcf_step(_pilotPll);

      // ---- Generate 38 kHz carrier (double the NCO phase) ----
      // cos(2θ) = 2·cos²(θ) - 1
      float cos2theta = 2.0F * ncoOut.real() * ncoOut.real() - 1.0F;

      // ---- Mix composite with 38 kHz to recover L-R ----
      float diffRaw = x * cos2theta * 2.0F; // ×2 to compensate DSB-SC
      float diff = 0.0F;
      firfilt_rrrf_push(_diffLpf, diffRaw);
      firfilt_rrrf_execute(_diffLpf, &diff);

      // ---- Matrix: L = (mono + diff)/2, R = (mono - diff)/2 ----
      left[i] = (mono + diff) * 0.5F;
      right[i] = (mono - diff) * 0.5F;
   }
}

// ============================================================================
// Utility
// ============================================================================

const char* demodModeName(DemodMode mode)
{
   switch (mode)
   {
      case DemodMode::FmMono:   return "FM Mono";
      case DemodMode::FmStereo: return "FM Stereo";
      case DemodMode::AM:       return "AM";
   }
   return "Unknown";
}

} // namespace SdrEngine
