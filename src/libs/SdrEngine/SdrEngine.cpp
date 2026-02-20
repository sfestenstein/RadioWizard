// Project headers
#include "SdrEngine.h"
#include "GeneralLogger.h"

// System headers
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <utility>

namespace SdrEngine
{

// ============================================================================
// Construction / destruction
// ============================================================================

SdrEngine::SdrEngine()
   : _fft{2048, WindowFunction::BlackmanHarris}
   , _spectrumHandler{std::make_unique<CommonUtils::DataHandler<std::shared_ptr<const SpectrumData>>>()}
   , _iqHandler{std::make_unique<CommonUtils::DataHandler<std::shared_ptr<const IqBuffer>>>()}
   , _filteredIqHandler{std::make_unique<CommonUtils::DataHandler<std::shared_ptr<const IqBuffer>>>()}
{
}

SdrEngine::~SdrEngine()
{
   stop();
   // Destroy handlers before the engine goes away so listener threads exit.
   _spectrumHandler.reset();
   _iqHandler.reset();
   _filteredIqHandler.reset();
}

// ============================================================================
// Device management
// ============================================================================

void SdrEngine::setDevice(std::unique_ptr<ISdrDevice> device)
{
   if (_running)
   {
      GPWARN("Cannot change device while running");
      return;
   }
   _device = std::move(device);
}

ISdrDevice* SdrEngine::getDevice() const
{
   return _device.get();
}

std::vector<DeviceInfo> SdrEngine::enumerateDevices() const
{
   if (_device)
   {
      return _device->enumerateDevices();
   }
   return {};
}

// ============================================================================
// Tuning controls
// ============================================================================

bool SdrEngine::setCenterFrequency(uint64_t frequencyHz)
{
   _centerFreqHz = frequencyHz;
   if (_device && _device->isOpen())
   {
      return _device->setCenterFrequency(frequencyHz);
   }
   return true;
}

uint64_t SdrEngine::getCenterFrequency() const
{
   return _centerFreqHz;
}

bool SdrEngine::setSampleRate(uint32_t rateHz)
{
   _sampleRateHz = rateHz;
   if (_device && _device->isOpen())
   {
      return _device->setSampleRate(rateHz);
   }
   return true;
}

uint32_t SdrEngine::getSampleRate() const
{
   return _sampleRateHz;
}

// ============================================================================
// Gain controls
// ============================================================================

bool SdrEngine::setAutoGain(bool enabled)
{
   if (_device && _device->isOpen())
   {
      return _device->setAutoGain(enabled);
   }
   return false;
}

bool SdrEngine::setGain(int tenthsDb)
{
   if (_device && _device->isOpen())
   {
      return _device->setGain(tenthsDb);
   }
   return false;
}

int SdrEngine::getGain() const
{
   if (_device && _device->isOpen())
   {
      return _device->getGain();
   }
   return 0;
}

std::vector<int> SdrEngine::getGainValues() const
{
   if (_device && _device->isOpen())
   {
      return _device->getGainValues();
   }
   return {};
}

// ============================================================================
// FFT controls
// ============================================================================

void SdrEngine::setFftSize(size_t fftSize)
{
   _fft.setFftSize(fftSize);
}

size_t SdrEngine::getFftSize() const
{
   return _fft.getFftSize();
}

void SdrEngine::setWindowFunction(WindowFunction windowFunc)
{
   _fft.setWindowFunction(windowFunc);
}

WindowFunction SdrEngine::getWindowFunction() const
{
   return _fft.getWindowFunction();
}

void SdrEngine::setFftAverageAlpha(float alpha)
{
   _fftAlpha = std::clamp(alpha, 0.0F, 1.0F);
}

float SdrEngine::getFftAverageAlpha() const
{
   return _fftAlpha;
}

void SdrEngine::setDcSpikeRemovalEnabled(bool enabled)
{
   _dcSpikeRemovalEnabled = enabled;
}

bool SdrEngine::isDcSpikeRemovalEnabled() const
{
   return _dcSpikeRemovalEnabled;
}

// ============================================================================
// Channel filter controls
// ============================================================================

void SdrEngine::configureChannelFilter(double centerOffsetHz, double bandwidthHz)
{
   _channelFilter.configure(centerOffsetHz, bandwidthHz,
                            static_cast<double>(_sampleRateHz.load()));
}

void SdrEngine::configureChannelFilterFromMinMax(double minFreqHz, double maxFreqHz)
{
   const uint64_t centerFreqHz = _centerFreqHz.load();
   _channelFilter.configureFromMinMax(minFreqHz, maxFreqHz,
                                      static_cast<double>(centerFreqHz),
                                      static_cast<double>(_sampleRateHz.load()));
}

void SdrEngine::setChannelFilterEnabled(bool enabled)
{
   _channelFilter.setEnabled(enabled);
}

bool SdrEngine::isChannelFilterEnabled() const
{
   return _channelFilter.isEnabled();
}

const ChannelFilter& SdrEngine::channelFilter() const
{
   return _channelFilter;
}

// ============================================================================
// Start / stop
// ============================================================================

bool SdrEngine::start(int deviceIndex)
{
   if (_running)
   {
      GPWARN("SdrEngine already running");
      return false;
   }
   if (!_device)
   {
      GPERROR("No SDR device set");
      return false;
   }

   // Open the device.
   if (!_device->open(deviceIndex))
   {
      GPERROR("Failed to open device at index {}", deviceIndex);
      return false;
   }

   // Apply cached tuning.
   std::ignore = _device->setCenterFrequency(_centerFreqHz);
   std::ignore = _device->setSampleRate(_sampleRateHz);

   // Start the processing thread.
   _running = true;
   _procThread = std::thread(&SdrEngine::processingLoop, this);

   // Start streaming — the raw callback feeds the accumulation buffer.
   const auto bufSize = static_cast<std::size_t>(_fft.getFftSize()) * 2;
   if (!_device->startStreaming(
          [this](const uint8_t* data, std::size_t len) { onRawIqData(data, len); },
          bufSize))
   {
      GPERROR("Failed to start streaming");
      _running = false;
      _bufCv.notify_all();
      if (_procThread.joinable())
      {
         _procThread.join();
      }
      _device->close();
      return false;
   }

   GPINFO("SdrEngine started (freq={} Hz, rate={} Hz, fft={})",
          _centerFreqHz.load(), _sampleRateHz.load(), _fft.getFftSize());
   return true;
}

void SdrEngine::stop()
{
   if (!_running)
   {
      return;
   }

   // Stop streaming first (blocks until device thread exits).
   if (_device && _device->isStreaming())
   {
      _device->stopStreaming();
   }

   // Signal processing thread to exit.
   _running = false;
   _bufCv.notify_all();

   if (_procThread.joinable())
   {
      _procThread.join();
   }

   if (_device && _device->isOpen())
   {
      _device->close();
   }

   GPINFO("SdrEngine stopped");
}

bool SdrEngine::isRunning() const
{
   return _running;
}

// ============================================================================
// Data handlers
// ============================================================================

CommonUtils::DataHandler<std::shared_ptr<const SpectrumData>>& SdrEngine::spectrumDataHandler()
{
   return *_spectrumHandler;
}

CommonUtils::DataHandler<std::shared_ptr<const IqBuffer>>& SdrEngine::iqDataHandler()
{
   return *_iqHandler;
}

CommonUtils::DataHandler<std::shared_ptr<const IqBuffer>>& SdrEngine::filteredIqDataHandler()
{
   return *_filteredIqHandler;
}

// ============================================================================
// Device callback → accumulation buffer
// ============================================================================

void SdrEngine::onRawIqData(const uint8_t* data, std::size_t length)
{
   // RTL-SDR delivers unsigned 8-bit I/Q pairs.  Convert to float [-1, 1].
   const std::size_t numSamples = length / 2;

   std::vector<std::complex<float>> chunk;
   chunk.reserve(numSamples);

   for (std::size_t i = 0; i < numSamples; ++i)
   {
      const auto iVal =
         (static_cast<float>(data[2 * i])     - 127.5F) / 127.5F;
      const auto qVal =
         (static_cast<float>(data[(2 * i) + 1]) - 127.5F) / 127.5F;
      chunk.emplace_back(iVal, qVal);
   }

   // Remove DC offset if enabled (suppresses LO leakage spike).
   if (_dcSpikeRemovalEnabled && !chunk.empty())
   {
      // Calculate mean I and Q.
      std::complex<float> mean{0.0F, 0.0F};
      for (const auto& sample : chunk)
      {
         mean += sample;
      }
      mean /= static_cast<float>(chunk.size());

      // Subtract mean from each sample.
      for (auto& sample : chunk)
      {
         sample -= mean;
      }
   }

   {
      const std::lock_guard<std::mutex> lock(_bufMutex);
      _accumBuf.insert(_accumBuf.end(), chunk.begin(), chunk.end());
   }
   _bufCv.notify_one();
}

// ============================================================================
// Processing thread
// ============================================================================

void SdrEngine::processingLoop()
{
   GPINFO("Processing thread started");

   std::vector<std::complex<float>> block;

   while (_running)
   {

      {
         std::unique_lock<std::mutex> lock(_bufMutex);

         // Wait until we have enough samples for one FFT frame.
         // Read size once per iteration — size may change between iterations
         // if the GUI thread calls setFftSize(), which is fine.
         const auto needed = static_cast<std::size_t>(_fft.getFftSize());
         _bufCv.wait(lock, [&] {
            return _accumBuf.size() >= needed || !_running;
         });

         if (!_running)
         {
            break;
         }

         // Take exactly one FFT frame from the front using the same
         // size we waited for.
         block.assign(_accumBuf.begin(),
                      _accumBuf.begin() + static_cast<std::ptrdiff_t>(needed));
         _accumBuf.erase(_accumBuf.begin(),
                         _accumBuf.begin() + static_cast<std::ptrdiff_t>(needed));
      }

      // Publish raw I/Q for constellation viewers.
      auto iqBuf = std::make_shared<IqBuffer>();
      iqBuf->samples      = block;
      iqBuf->centerFreqHz = static_cast<double>(_centerFreqHz.load());
      iqBuf->sampleRateHz = static_cast<double>(_sampleRateHz.load());
      iqBuf->timestamp    = std::chrono::steady_clock::now();
      _iqHandler->signalData(iqBuf);

      // Channel-filter the I/Q block and publish if enabled.
      if (_channelFilter.isEnabled())
      {
         auto filteredSamples = _channelFilter.process(block);
         if (!filteredSamples.empty())
         {
            auto filteredBuf = std::make_shared<IqBuffer>();
            filteredBuf->samples      = std::move(filteredSamples);
            filteredBuf->centerFreqHz = iqBuf->centerFreqHz +
                                       _channelFilter.getCenterOffset();
            filteredBuf->sampleRateHz = _channelFilter.getOutputSampleRate();
            filteredBuf->timestamp    = iqBuf->timestamp;
            _filteredIqHandler->signalData(filteredBuf);
         }
      }

      // Run the FFT.
      auto magnitudesDb = _fft.process(block);

      // Apply FFT averaging (exponential moving average).
      const float alpha = _fftAlpha.load();
      if (alpha > 0.0F)
      {
         const std::lock_guard<std::mutex> lock(_avgMutex);
         
         // Initialize averaging buffer on first run or size change.
         if (_fftAverage.size() != magnitudesDb.size())
         {
            _fftAverage = magnitudesDb;
         }
         else
         {
            // EMA: avg[n] = alpha * avg[n-1] + (1 - alpha) * new[n]
            for (std::size_t i = 0; i < magnitudesDb.size(); ++i)
            {
               _fftAverage[i] = (alpha * _fftAverage[i]) + ((1.0F - alpha) * magnitudesDb[i]);
            }
            magnitudesDb = _fftAverage;
         }
      }

      // Suppress center bin DC spike if enabled (interpolate from neighbors).
      if (_dcSpikeRemovalEnabled && magnitudesDb.size() > 2)
      {
         const std::size_t centerBin = magnitudesDb.size() / 2;
         // Interpolate center bin from immediate neighbors.
         if (centerBin > 0 && centerBin < magnitudesDb.size() - 1)
         {
            magnitudesDb[centerBin] = (magnitudesDb[centerBin - 1] + magnitudesDb[centerBin + 1]) / 2.0F;
         }
      }

      // Decimate to 2048 bins if needed (picking max of each bin).
      constexpr std::size_t MAX_PLOT_BINS = 2048;
      if (magnitudesDb.size() > MAX_PLOT_BINS)
      {
         std::vector<float> decimated;
         decimated.reserve(MAX_PLOT_BINS);

         const std::size_t binSize = magnitudesDb.size() / MAX_PLOT_BINS;
         const std::size_t remainder = magnitudesDb.size() % MAX_PLOT_BINS;

         std::size_t srcIdx = 0;
         for (std::size_t i = 0; i < MAX_PLOT_BINS; ++i)
         {
            // Distribute remainder across bins to handle non-divisible sizes.
            const std::size_t currentBinSize = binSize + (i < remainder ? 1 : 0);

            float maxVal = magnitudesDb[srcIdx];
            for (std::size_t j = 1; j < currentBinSize; ++j)
            {
               maxVal = std::max(maxVal, magnitudesDb[srcIdx + j]);
            }

            decimated.push_back(maxVal);
            srcIdx += currentBinSize;
         }

         magnitudesDb = std::move(decimated);
      }

      // Publish spectrum data.
      auto spectrum = std::make_shared<SpectrumData>();
      spectrum->magnitudesDb  = std::move(magnitudesDb);
      spectrum->centerFreqHz  = static_cast<double>(_centerFreqHz.load());
      spectrum->bandwidthHz   = static_cast<double>(_sampleRateHz.load());
      spectrum->fftSize       = _fft.getFftSize();
      _spectrumHandler->signalData(spectrum);
      block.clear();
   }

   GPINFO("Processing thread exiting");
}

} // namespace SdrEngine
