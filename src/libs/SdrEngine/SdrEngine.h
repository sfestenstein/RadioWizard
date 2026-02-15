#ifndef SDRENGINE_H_
#define SDRENGINE_H_

// Project headers
#include "DataHandler.h"
#include "FftProcessor.h"
#include "ISdrDevice.h"
#include "SdrTypes.h"

// System headers
#include <atomic>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace SdrEngine
{

/// High-level SDR controller.
///
/// Owns an ISdrDevice, an FftProcessor, and two DataHandlers:
///   - `spectrumDataHandler()`  — publishes SpectrumData after each FFT.
///   - `iqDataHandler()`        — publishes IqBuffer (raw complex I/Q chunks).
///
/// The data pipeline is entirely Qt-free.  The MainWindow (or any other
/// consumer) registers listeners on the DataHandlers to receive results.
///
/// Threading model:
///   Device callback thread → lock-free append to internal accumulation buffer
///   Processing thread      → takes accumulated I/Q, runs FFT, publishes
class SdrEngine
{
public:
   SdrEngine();
   ~SdrEngine();

   // Non-copyable, non-movable.
   SdrEngine(const SdrEngine&) = delete;
   SdrEngine& operator=(const SdrEngine&) = delete;
   SdrEngine(SdrEngine&&) = delete;
   SdrEngine& operator=(SdrEngine&&) = delete;

   // -- Device management ---------------------------------------------------

   /// Set the concrete SDR device to use (e.g. RtlSdrDevice).
   /// Must be called before start().
   void setDevice(std::unique_ptr<ISdrDevice> device);

   /// @return Pointer to the current device (may be null).
   [[nodiscard]] ISdrDevice* getDevice() const;

   /// Enumerate devices via the current device implementation.
   [[nodiscard]] std::vector<DeviceInfo> enumerateDevices() const;

   // -- Tuning controls -----------------------------------------------------

   /// Set centre frequency in Hz.
   bool setCenterFrequency(uint64_t frequencyHz);

   /// @return Current centre frequency in Hz.
   [[nodiscard]] uint64_t getCenterFrequency() const;

   /// Set sample rate in Hz.
   bool setSampleRate(uint32_t rateHz);

   /// @return Current sample rate in Hz.
   [[nodiscard]] uint32_t getSampleRate() const;

   // -- Gain controls -------------------------------------------------------

   /// Enable / disable automatic gain control.
   bool setAutoGain(bool enabled);

   /// Set manual gain in tenths of dB.
   bool setGain(int tenthsDb);

   /// @return Current gain in tenths of dB.
   [[nodiscard]] int getGain() const;

   /// @return Available gain steps (tenths of dB).
   [[nodiscard]] std::vector<int> getGainValues() const;

   // -- FFT controls --------------------------------------------------------

   /// Change the FFT size (takes effect on the next processing cycle).
   void setFftSize(size_t fftSize);

   /// @return Current FFT size.
   [[nodiscard]] size_t getFftSize() const;

   /// Change the windowing function.
   void setWindowFunction(WindowFunction windowFunc);

   /// @return Current windowing function.
   [[nodiscard]] WindowFunction getWindowFunction() const;

   /// Set the FFT averaging alpha (0.0 = no averaging, 1.0 = full smoothing).
   /// Alpha is the coefficient for the exponential moving average:
   ///   avg[n] = alpha * avg[n-1] + (1 - alpha) * new[n]
   /// @param alpha  Averaging coefficient [0.0, 1.0].
   void setFftAverageAlpha(float alpha);

   /// @return Current FFT averaging alpha coefficient.
   [[nodiscard]] float getFftAverageAlpha() const;

   /// Enable or disable DC spike removal (local oscillator leakage suppression).
   /// When enabled, removes DC offset from IQ samples and interpolates the center FFT bin.
   /// @param enabled  true to remove DC spike, false to disable.
   void setDcSpikeRemovalEnabled(bool enabled);

   /// @return true if DC spike removal is enabled.
   [[nodiscard]] bool isDcSpikeRemovalEnabled() const;

   // -- Start / stop --------------------------------------------------------

   /// Open the device and begin streaming + processing.
   /// @param deviceIndex  Device to open (default 0).
   /// @return true on success.
   [[nodiscard]] bool start(int deviceIndex = 0);

   /// Stop streaming and processing; close the device.
   void stop();

   /// @return true when the engine is actively running.
   [[nodiscard]] bool isRunning() const;

   // -- Data outputs --------------------------------------------------------

   /// DataHandler that publishes SpectrumData after each FFT frame.
   [[nodiscard]] CommonUtils::DataHandler<std::shared_ptr<const SpectrumData>>& spectrumDataHandler();

   /// DataHandler that publishes IqBuffer chunks for constellation display.
   [[nodiscard]] CommonUtils::DataHandler<std::shared_ptr<const IqBuffer>>& iqDataHandler();

private:
   /// Called from the device's async callback thread.
   void onRawIqData(const uint8_t* data, std::size_t length);

   /// Processing thread body.
   void processingLoop();

   // -- Device & DSP --------------------------------------------------------
   std::unique_ptr<ISdrDevice> _device;
   FftProcessor _fft;

   // -- Data handlers -------------------------------------------------------
   std::unique_ptr<CommonUtils::DataHandler<std::shared_ptr<const SpectrumData>>> _spectrumHandler;
   std::unique_ptr<CommonUtils::DataHandler<std::shared_ptr<const IqBuffer>>> _iqHandler;

   // -- Accumulation buffer (device callback → processing thread) -----------
   std::mutex _bufMutex;
   std::vector<std::complex<float>> _accumBuf;
   std::condition_variable _bufCv;

   // -- Processing thread ---------------------------------------------------
   std::thread _procThread;
   std::atomic<bool> _running{false};

   // -- Cached tuning info --------------------------------------------------
   std::atomic<uint64_t> _centerFreqHz{100'000'000};   // 100 MHz default
   std::atomic<uint32_t> _sampleRateHz{2'400'000};     // 2.4 MS/s default

   // -- FFT averaging -------------------------------------------------------
   std::atomic<float> _fftAlpha{0.0F};                 // 0.0 = no averaging
   std::mutex _avgMutex;
   std::vector<float> _fftAverage;                     // Running average buffer

   // -- DC spike removal ----------------------------------------------------
   std::atomic<bool> _dcSpikeRemovalEnabled{true};     // Default: enabled
};

} // namespace SdrEngine

#endif // SDRENGINE_H_
