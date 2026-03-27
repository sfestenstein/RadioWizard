// Project headers
#include <MainWindow.h>
#include "AudioOutput.h"
#include "Demodulator.h"
#include "GeneralLogger.h"
#include "PlutoSdrDevice.h"
#include "RtlSdrDevice.h"
#include "SdrCommonUtils.h"

// Generated UI header
#include "./ui_MainWindow.h"

// Third-party headers
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QSlider>

// System headers
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>

namespace
{

/// Sample rates matching the combo-box order in the .ui file.
constexpr size_t NUM_SAMPLE_RATES = 13;
constexpr std::array<uint32_t, NUM_SAMPLE_RATES> SAMPLE_RATES = {
    250'000, 1'024'000, 1'400'000, 1'800'000, 2'048'000, 2'400'000, 2'800'000, 3'200'000,
    6'000'000, 8'000'000,10'000'000, 15'000'000, 20'000'000};

constexpr size_t NUM_FFT_SIZES = 8;
constexpr std::array<size_t, NUM_FFT_SIZES> FFT_SIZES = {2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144};

} // anonymous namespace

// ============================================================================
// Construction / destruction
// ============================================================================

MainWindow::MainWindow(QWidget* parent)
   : QMainWindow(parent)
   , _ui(new Ui::MainWindow)
{
   _ui->setupUi(this);

   // Add a device selector + refresh button into the control panel grid.
   auto* gridLayout = _ui->_startStopButton->parentWidget()
                          ->findChild<QGridLayout*>("gridLayout");
   if (gridLayout != nullptr)
   {
      auto* deviceLabel = new QLabel("Device", this);
      _deviceCombo = new QComboBox(this);
      _deviceCombo->setSizeAdjustPolicy(
         QComboBox::AdjustToMinimumContentsLengthWithIcon);
      _deviceCombo->setMinimumContentsLength(12);
      _refreshDevicesBtn = new QPushButton("Refresh", this);
      _refreshDevicesBtn->setToolTip("Re-scan for connected SDR devices");

      gridLayout->addWidget(deviceLabel, 2, 0);
      gridLayout->addWidget(_deviceCombo, 3, 0, 1, 2);
      gridLayout->addWidget(_refreshDevicesBtn, 2, 1);

      connect(_deviceCombo, &QComboBox::currentIndexChanged, this,
              [this](int index) { applyDeviceSelection(index); });
      connect(_refreshDevicesBtn, &QPushButton::clicked, this,
              [this]() { refreshDevices(); });
   }

   // Auto-detect connected devices and select the first one.
   refreshDevices();

   // Populate sample rate combo box.
   for (const auto sampleRate : SAMPLE_RATES)
   {
      const auto rateMs = static_cast<double>(sampleRate) / 1.0e6;
      _ui->_sampleRateCombo->addItem(QString::number(rateMs, 'g', 4) + " MS/s");
   }

   // Populate FFT size combo box.
   for (const auto fftSize : FFT_SIZES)
   {
      _ui->_fftSizeCombo->addItem(QString::number(fftSize));
   }

   // Set defaults that match the UI initial values.
   _engine.setCenterFrequency(92'100'000);
   _engine.setSampleRate(2'400'000);
   _engine.setFftSize(65536);
   onCenterFreqChanged(_engine.getCenterFrequencyMHz());

   _ui->_centerFreqSpinBox->setFrequencyMhz(92.1);

   // Default combo selections.
   _ui->_sampleRateCombo->setCurrentIndex(5);
   _ui->_fftSizeCombo->setCurrentIndex(5);

   // Configure the plot widgets for dB input and frequency display.
   _ui->_spectrurmWidget->setInputIsDb(true);
   _ui->_spectrurmWidget->setDbRange(-120.0F, 0.0F);
   _ui->_waterfallWidget->setInputIsDb(true);
   _ui->_waterfallWidget->setDbRange(-120.0F, 0.0F);
   _ui->_detailedSpectrumWidget->setInputIsDb(true);
   _ui->_detailedSpectrumWidget->setDbRange(-120.0F, 0.0F);
   _ui->_detailedSpectrumWidget->setGridLines(6, 4);

   // Set up the Color Theme combo Box
   for (std::size_t i = 0; i < RealTimeGraphs::ColorMap::paletteCount(); ++i)
   {
      auto pal = RealTimeGraphs::ColorMap::paletteAt(i);
      _ui->_colorThemeCombo->addItem(
          QString::fromStdString(RealTimeGraphs::ColorMap::paletteName(pal)),
          static_cast<int>(i));
   }
   connect(_ui->_colorThemeCombo, &QComboBox::currentIndexChanged, this, [this](int idx)
   {
      auto pal = RealTimeGraphs::ColorMap::paletteAt(static_cast<std::size_t>(idx));
      _ui->_spectrurmWidget->setColorMap(pal);
      _ui->_waterfallWidget->setColorMap(pal);
      _ui->_detailedSpectrumWidget->setColorMap(pal);
    });
   _ui->_colorThemeCombo->setCurrentIndex(2);
   _ui->_spectrurmWidget->setColorMap(RealTimeGraphs::ColorMap::paletteAt(2));
   _ui->_waterfallWidget->setColorMap(RealTimeGraphs::ColorMap::paletteAt(2));
   _ui->_detailedSpectrumWidget->setColorMap(RealTimeGraphs::ColorMap::paletteAt(2));

    // Wire UI signals → slots.
   connect(_ui->_startStopButton, &QPushButton::toggled,
           this, &MainWindow::onStartStopToggled);
   connect(_ui->_autoScaleBtn, &QPushButton::clicked,
           this, &MainWindow::onAutoScaleClicked);
   connect(_ui->_centerFreqSpinBox, &FrequencyInputWidget::frequencyChanged,
           this, &MainWindow::onCenterFreqChanged);
   connect(_ui->_sampleRateCombo, &QComboBox::currentIndexChanged,
           this, &MainWindow::onSampleRateChanged);
   connect(_ui->_autoGainCheckBox, &QCheckBox::toggled,
           this, &MainWindow::onAutoGainToggled);
   connect(_ui->_gainSlider, &QSlider::valueChanged,
           this, &MainWindow::onGainSliderChanged);
   connect(_ui->_fftSizeCombo, &QComboBox::currentIndexChanged,
           this, &MainWindow::onFftSizeChanged);
   connect(_ui->_windowFuncCombo, &QComboBox::currentIndexChanged,
           this, &MainWindow::onWindowFuncChanged);
   connect(_ui->_fftAverageSlider, &QSlider::valueChanged,
           this, &MainWindow::onFftAverageChanged);
   connect(_ui->_dcSpikeRemovalCheckBox, &QCheckBox::toggled,
           this, &MainWindow::onDcSpikeRemovalToggled);
   connect(_ui->_maxHoldCheckBox, &QCheckBox::toggled,
           _ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::setMaxHoldEnabled);
   connect(_ui->_bwCursorButton, &QPushButton::toggled,
           this, &MainWindow::onBwCursorToggled);
   connect(_ui->_demodButton, &QPushButton::toggled,
           this, &MainWindow::onDemodToggled);
   connect(_ui->_demodModeCombo, &QComboBox::currentIndexChanged,
           this, &MainWindow::onDemodModeChanged);
   connect(_ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::requestPeerCursorClear,
           _ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::clearMeasCursors);
   connect(_ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::requestPeerCursorClear,
           _ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::clearMeasCursors);
   onFftAverageChanged(33);

   // Bandwidth cursor synchronisation between Spectrum and Waterfall
   connect(_ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::bandwidthCursorHalfWidthChanged,
           _ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::setBandwidthCursorHalfWidthHz);
   connect(_ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::bandwidthCursorHalfWidthChanged,
           _ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::setBandwidthCursorHalfWidthHz);
   connect(_ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::bandwidthCursorLocked,
           _ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::lockBandwidthCursorAt);
   connect(_ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::bandwidthCursorLocked,
           _ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::lockBandwidthCursorAt);
   connect(_ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::bandwidthCursorUnlocked,
           _ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::unlockBandwidthCursor);
   connect(_ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::bandwidthCursorUnlocked,
           _ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::unlockBandwidthCursor);

   // Bandwidth cursor → MainWindow (channel filter + constellation switching)
   connect(_ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::bandwidthCursorLocked,
           this, &MainWindow::onBwCursorLocked);
   connect(_ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::bandwidthCursorLocked,
           this, &MainWindow::onBwCursorLocked);
   connect(_ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::bandwidthCursorUnlocked,
           this, &MainWindow::onBwCursorUnlocked);
   connect(_ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::bandwidthCursorUnlocked,
           this, &MainWindow::onBwCursorUnlocked);
   connect(_ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::bandwidthCursorHalfWidthChanged,
           this, &MainWindow::onBwCursorHalfWidthChanged);
   connect(_ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::bandwidthCursorHalfWidthChanged,
           this, &MainWindow::onBwCursorHalfWidthChanged);

   // Tie the Spectrum and Waterfall widgets' color maps together.
   _ui->_spectrurmWidget->setXAxisVisible(false); // Waterfall below shows the shared X axis

   // Bidirectional X-axis linking
   connect(_ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::xViewChanged,
           _ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::setXViewRange);
   connect(_ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::xViewChanged,
           _ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::setXViewRange);

   // Bidirectional tracking-cursor linking (vertical line sync)
   connect(_ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::trackingCursorXChanged,
           _ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::setLinkedCursorX);
   connect(_ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::trackingCursorLeft,
           _ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::clearLinkedCursorX);
   connect(_ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::trackingCursorXChanged,
           _ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::setLinkedCursorX);
   connect(_ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::trackingCursorLeft,
           _ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::clearLinkedCursorX);

   // Bidirectional measurement-cursor linking (vertical lines only)
   connect(_ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::measCursorsChanged,
           _ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::setLinkedMeasCursors);
   connect(_ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::measCursorsChanged,
           _ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::setLinkedMeasCursors);

   // Sync waterfall color bar range with spectrum color bar changes.
   connect(_ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::dbRangeChanged,
           _ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::setDbRange);

}

MainWindow::~MainWindow()
{
   stopDemod();
   disconnectDataHandlers();
   if (_engine.isRunning())
   {
      _engine.stop();
   }
   delete _ui;
}

// ============================================================================
// Slots
// ============================================================================

void MainWindow::onStartStopToggled(bool checked)
{
   if (checked)
   {
      connectDataHandlers();
      if (_engine.start())
      {
         _ui->_startStopButton->setText("Stop");
         if (_deviceCombo != nullptr)
         {
            _deviceCombo->setEnabled(false);
         }
         if (_refreshDevicesBtn != nullptr)
         {
            _refreshDevicesBtn->setEnabled(false);
         }
      }
      else
      {
         _ui->_startStopButton->setChecked(false);
         disconnectDataHandlers();
      }
   }
   else
   {
      _engine.stop();

      // Stop demod if active.
      if (_ui->_demodButton->isChecked())
      {
         _ui->_demodButton->setChecked(false);
      }
      stopDemod();

      disconnectDataHandlers();
      _ui->_startStopButton->setText("Start");
      if (_deviceCombo != nullptr)
      {
         _deviceCombo->setEnabled(true);
      }
      if (_refreshDevicesBtn != nullptr)
      {
         _refreshDevicesBtn->setEnabled(true);
      }
   }
}

void MainWindow::onAutoScaleClicked()
{
   // Get the amplitude range from the waterfall widget.
   const auto range = _ui->_waterfallWidget->getAmplitudeRange();
   if (!range.has_value())
   {
      GPWARN("No data available for auto-scaling");
      return;
   }

   auto [minDb, maxDb] = range.value();

   // Add some margin (5 dB on each side).
   constexpr float MARGIN = 5.0F;
   minDb -= MARGIN;
   maxDb += MARGIN;

   GPINFO("Auto-scaling to range: {} dB to {} dB", minDb, maxDb);

   // Apply to both spectrum widgets.
   _ui->_spectrurmWidget->setDbRange(minDb, maxDb);
   _ui->_detailedSpectrumWidget->setDbRange(minDb, maxDb);

   // Apply to both waterfall widgets.
   _ui->_waterfallWidget->setDbRange(minDb, maxDb);
}

void MainWindow::onCenterFreqChanged(double valueMhz)
{
   const auto hz = static_cast<uint64_t>(valueMhz * 1.0e6);
   _engine.setCenterFrequency(hz);
   _ui->_spectrurmWidget->setFrequencyRange(
       static_cast<double>(_engine.getCenterFrequency()),_engine.getSampleRate());
   _ui->_waterfallWidget->setFrequencyRange(
       static_cast<double>(_engine.getCenterFrequency()),_engine.getSampleRate());
}

void MainWindow::onSampleRateChanged(int index)
{
   _engine.setSampleRate(sampleRateFromIndex(index));

   // Update the plot widgets so their frequency range (and thus the bandwidth
   // cursor visual width) reflects the new sample rate.
   const auto center = static_cast<double>(_engine.getCenterFrequency());
   const auto bw     = static_cast<double>(_engine.getSampleRate());
   _ui->_spectrurmWidget->setFrequencyRange(center, bw);
   _ui->_waterfallWidget->setFrequencyRange(center, bw);
}

void MainWindow::onAutoGainToggled(bool checked)
{
   _ui->_gainSlider->setEnabled(!checked);
   if (checked)
   {
      _ui->_gainValueLabel->setText("Auto");
   }
   else
   {
      const int val = _ui->_gainSlider->value();
      _ui->_gainValueLabel->setText(
         QString::number(static_cast<double>(val) / 10.0, 'f', 1) + " dB");
   }
   _engine.setAutoGain(checked);
}

void MainWindow::onGainSliderChanged(int value)
{
   _ui->_gainValueLabel->setText(
      QString::number(static_cast<double>(value) / 10.0, 'f', 1) + " dB");
   _engine.setGain(value);
}

void MainWindow::onFftSizeChanged(int index)
{
   auto indexSize = static_cast<size_t>(index);
   if (indexSize < NUM_FFT_SIZES)
   {
      _engine.setFftSize(FFT_SIZES[indexSize]);
   }
}

void MainWindow::onWindowFuncChanged(int index)
{
   // Combo order: Blackman-Harris, Hanning, Flat-Top, Rectangular.
   static constexpr std::array<SdrEngine::WindowFunction, 4> MAP =
   {
      SdrEngine::WindowFunction::BlackmanHarris,
      SdrEngine::WindowFunction::Hanning,
      SdrEngine::WindowFunction::FlatTop,
      SdrEngine::WindowFunction::Rectangular,
   };
   if (index < std::ssize(MAP))
   {
      _engine.setWindowFunction(MAP[static_cast<size_t>(index)]);
   }
}

void MainWindow::onFftAverageChanged(int value)
{
   // Map slider value [0, 100] to alpha [0.0, 1.0]
   const auto alpha = static_cast<float>(value) / 100.0F;
   _engine.setFftAverageAlpha(alpha);

   // Update the label to show current value
   _ui->_fftAverageValueLabel->setText(QString::number(alpha, 'f', 2));
}

void MainWindow::onDcSpikeRemovalToggled(bool checked)
{
   _engine.setDcSpikeRemovalEnabled(checked);
}

void MainWindow::onBwCursorToggled(bool checked)
{
   _ui->_spectrurmWidget->setBandwidthCursorEnabled(checked);
   _ui->_waterfallWidget->setBandwidthCursorEnabled(checked);

   // Disable channel filter when BW cursor is turned off.
   if (!checked)
   {
      // Stop demod if active.
      if (_ui->_demodButton->isChecked())
      {
         _ui->_demodButton->setChecked(false);
      }
      stopDemod();
      _bwCursorLocked = false;
      updateDemodButtonState();

      _engine.setChannelFilterEnabled(false);

      // Clear the detailed spectrum widget.
      _ui->_detailedSpectrumWidget->setData({});

      // Disconnect all IQ feeds and clear the constellation.
      if (_filteredIqListenerId >= 0)
      {
         _engine.filteredIqDataHandler().unregisterListener(_filteredIqListenerId);
         _filteredIqListenerId = -1;
      }
      if (_iqListenerId >= 0)
      {
         _engine.iqDataHandler().unregisterListener(_iqListenerId);
         _iqListenerId = -1;
      }
   }
}

void MainWindow::onBwCursorLocked(double xData)
{
   // xData is a fraction [0, 1] of the total bandwidth.
   // 0.5 = centre frequency.  Convert to Hz offset.
   _bwCursorLockedX = xData;  // Store for use when half-width changes.
   _bwCursorLocked = true;

   const uint64_t centerFreqHz = _engine.getCenterFrequency();
   auto sampleRate = static_cast<double>(_engine.getSampleRate());
   const double offsetHz = (xData - 0.5) * sampleRate;
   const double bwHz = _bwCursorHalfWidthHz * 2.0;

   // Calculate min and max frequencies for the channel filter.
   const double channelCenterHz = static_cast<double>(centerFreqHz) + offsetHz;
   const double minFreqHz = channelCenterHz - (bwHz / 2.0);
   const double maxFreqHz = channelCenterHz + (bwHz / 2.0);

   _engine.configureChannelFilterFromMinMax(minFreqHz, maxFreqHz);
   _engine.setChannelFilterEnabled(true);

   // Switch constellation to display filtered IQ data.
   switchToFilteredIq();

   // Configure the detailed spectrum widget for the cursor region.
   _ui->_detailedSpectrumWidget->setFrequencyRange(0.0, bwHz);
   if (_lastSpectrumData)
   {
      updateDetailedSpectrum();
   }

   updateDemodButtonState();

   // If demod is active, reconfigure for new channel.
   if (_ui->_demodButton->isChecked())
   {
      startDemod();
   }
}

void MainWindow::onBwCursorUnlocked()
{
   _bwCursorLocked = false;
   _engine.setChannelFilterEnabled(false);

   // Clear the detailed spectrum widget.
   _ui->_detailedSpectrumWidget->setData({});

   // Stop demod if active.
   if (_ui->_demodButton->isChecked())
   {
      _ui->_demodButton->setChecked(false);
   }
   stopDemod();
   updateDemodButtonState();

   // Disconnect all IQ feeds and clear the constellation.
   // The plot is not useful without a locked bandwidth selection.
   if (_filteredIqListenerId >= 0)
   {
      _engine.filteredIqDataHandler().unregisterListener(_filteredIqListenerId);
      _filteredIqListenerId = -1;
   }
   if (_iqListenerId >= 0)
   {
      _engine.iqDataHandler().unregisterListener(_iqListenerId);
      _iqListenerId = -1;
   }
}

void MainWindow::onBwCursorHalfWidthChanged(double halfWidthHz)
{
   _bwCursorHalfWidthHz = halfWidthHz;

   // If the channel filter is already enabled (cursor locked),
   // update its bandwidth in real time.
   if (_engine.isChannelFilterEnabled())
   {
      const uint64_t centerFreqHz = _engine.getCenterFrequency();
      auto sampleRate = static_cast<double>(_engine.getSampleRate());
      const double offsetHz = (_bwCursorLockedX - 0.5) * sampleRate;
      const double bwHz = halfWidthHz * 2.0;

      // Calculate min and max frequencies for the channel filter.
      const double channelCenterHz = static_cast<double>(centerFreqHz) + offsetHz;
      const double minFreqHz = channelCenterHz - (bwHz / 2.0);
      const double maxFreqHz = channelCenterHz + (bwHz / 2.0);

      _engine.configureChannelFilterFromMinMax(minFreqHz, maxFreqHz);

      // Update the detailed spectrum widget's frequency range and data.
      _ui->_detailedSpectrumWidget->setFrequencyRange(0.0, halfWidthHz * 2.0);
      if (_lastSpectrumData)
      {
         updateDetailedSpectrum();
      }

      // Reconfigure demod if active (bandwidth changed).
      if (_ui->_demodButton->isChecked())
      {
         startDemod();
      }
   }
}

void MainWindow::onDemodToggled(bool checked)
{
   if (checked)
   {
      startDemod();
   }
   else
   {
      stopDemod();
   }
}

void MainWindow::onDemodModeChanged(int /*index*/)
{
   // If demod is currently active, restart with the new mode.
   if (_ui->_demodButton->isChecked())
   {
      startDemod();
   }
}

// ============================================================================
// Demodulation helpers
// ============================================================================

void MainWindow::startDemod()
{
   // Stop any existing demod session.
   stopDemod();

   // Ensure the channel filter is configured and enabled.
   if (!_engine.isChannelFilterEnabled())
   {
      GPWARN("Demod: channel filter not active — lock a BW cursor first");
      _ui->_demodButton->setChecked(false);
      return;
   }

   // Configure Demodulator with the channel filter output rate.
   const double channelRate = _engine.channelFilter().getOutputSampleRate();
   if (channelRate <= 0.0)
   {
      GPWARN("Demod: channel filter output rate invalid");
      _ui->_demodButton->setChecked(false);
      return;
   }

   const auto mode = selectedDemodMode();
   constexpr double AUDIO_RATE = SdrEngine::Demodulator::DEFAULT_AUDIO_RATE;
   _demod.configure(mode, channelRate, AUDIO_RATE);

   // Create and start stereo audio output.
   _audioOutput = std::make_unique<AudioOutput>(AUDIO_RATE, 2);
   if (!_audioOutput->start())
   {
      GPERROR("Demod: failed to start audio output");
      _audioOutput.reset();
      _ui->_demodButton->setChecked(false);
      return;
   }

   // Register a listener on the filtered IQ data handler to feed the demod.
   _demodListenerId = _engine.filteredIqDataHandler().registerListener(
      [this](const std::shared_ptr<const SdrEngine::IqBuffer>& iqData)
      {
         try
         {
            auto audio = _demod.demodulate(iqData->samples);
            if (audio.left.empty() || !_audioOutput ||
                !_audioOutput->isPlaying())
            {
               return;
            }

            // Interleave L and R into a single buffer.
            const size_t frames = std::min(audio.left.size(),
                                           audio.right.size());
            std::vector<float> interleaved(frames * 2);
            for (size_t i = 0; i < frames; ++i)
            {
               interleaved[(i * 2)] = audio.left[i];
               interleaved[((i * 2) + 1)] = audio.right[i];
            }
            _audioOutput->pushSamples(interleaved);
         }
         catch (std::exception& ex)
         {
            GPERROR("Demod exception: {}", ex.what());
         }
      });

   GPINFO("Demod started: mode={}, channel={:.0f} Hz → audio={:.0f} Hz",
          SdrEngine::demodModeName(mode), channelRate, AUDIO_RATE);
}

void MainWindow::stopDemod()
{
   // Unregister the filtered IQ listener for demod.
   if (_demodListenerId >= 0)
   {
      _engine.filteredIqDataHandler().unregisterListener(_demodListenerId);
      _demodListenerId = -1;
   }

   // Stop and destroy audio output.
   if (_audioOutput)
   {
      _audioOutput->stop();
      _audioOutput.reset();
   }

   _demod.reset();
}

void MainWindow::updateDemodButtonState()
{
   // Demod is only available when a bandwidth cursor is locked.
   _ui->_demodButton->setEnabled(_bwCursorLocked);
   _ui->_demodModeCombo->setEnabled(_bwCursorLocked);
}

SdrEngine::DemodMode MainWindow::selectedDemodMode() const
{
   // Combo order: FM Mono (0), FM Stereo (1), AM (2).
   static constexpr std::array<SdrEngine::DemodMode, 3> MODES =
   {
      SdrEngine::DemodMode::FmMono,
      SdrEngine::DemodMode::FmStereo,
      SdrEngine::DemodMode::AM,
   };
   const int idx = _ui->_demodModeCombo->currentIndex();
   if (idx >= 0 && static_cast<size_t>(idx) < MODES.size())
   {
      return MODES[static_cast<size_t>(idx)];
   }
   return SdrEngine::DemodMode::FmMono;
}

// ============================================================================
// DataHandler wiring
// ============================================================================
void MainWindow::connectDataHandlers()
{
   // --- Spectrum data → SpectrumWidget + WaterfallWidget ---
   _spectrumListenerId = _engine.spectrumDataHandler().registerListener(
      [this](const std::shared_ptr<const SdrEngine::SpectrumData>& specData)
      {
         try
         {
            const auto decimated =
               SdrEngine::decimateSpectrum(specData->magnitudesDb);
            _ui->_spectrurmWidget->setData(decimated);
            _ui->_waterfallWidget->addRow(decimated);

            // Cache the full spectrum for the detailed widget.
            _lastSpectrumData = specData;
            if (_bwCursorLocked)
            {
               updateDetailedSpectrum();
            }
         }
         catch (std::exception &exception)
         {
            GPERROR("Caught Data Handler Exception: {}", exception.what());
         }
         catch(...)
         {
            GPERROR("Caught unknown Data Handler Exception");
         }
      });

}

void MainWindow::disconnectDataHandlers()
{
   if (_spectrumListenerId >= 0)
   {
      _engine.spectrumDataHandler().unregisterListener(_spectrumListenerId);
      _spectrumListenerId = -1;
   }
   if (_iqListenerId >= 0)
   {
      _engine.iqDataHandler().unregisterListener(_iqListenerId);
      _iqListenerId = -1;
   }
   if (_filteredIqListenerId >= 0)
   {
      _engine.filteredIqDataHandler().unregisterListener(_filteredIqListenerId);
      _filteredIqListenerId = -1;
   }
}

// ============================================================================
// Constellation data source switching
// ============================================================================
void MainWindow::switchToUnfilteredIq()
{
   // Unregister filtered listener if active.
   if (_filteredIqListenerId >= 0)
   {
      _engine.filteredIqDataHandler().unregisterListener(_filteredIqListenerId);
      _filteredIqListenerId = -1;
   }

   // Register unfiltered listener if not already active.
   if (_iqListenerId < 0)
   {
      _iqListenerId = _engine.iqDataHandler().registerListener(
         [this](const std::shared_ptr<const SdrEngine::IqBuffer>& iqData)
         {
            _ui->_constellationWidget->setData(iqData->samples);
         });
      GPINFO("Switched constellation to unfiltered I/Q data");
   }
}

void MainWindow::switchToFilteredIq()
{
   // Unregister unfiltered listener if active.
   if (_iqListenerId >= 0)
   {
      _engine.iqDataHandler().unregisterListener(_iqListenerId);
      _iqListenerId = -1;
   }

   // Register filtered listener if not already active.
   if (_filteredIqListenerId < 0)
   {
      _filteredIqListenerId = _engine.filteredIqDataHandler().registerListener(
         [this](const std::shared_ptr<const SdrEngine::IqBuffer>& iqData)
         {
            _ui->_constellationWidget->setData(iqData->samples);
         });
      GPINFO("Switched constellation to filtered I/Q data");
   }
}

// ============================================================================
// Detailed spectrum (BW-cursor zoom)
// ============================================================================
void MainWindow::updateDetailedSpectrum()
{
   if (!_lastSpectrumData || !_bwCursorLocked)
   {
      return;
   }

   const auto& bins = _lastSpectrumData->magnitudesDb;
   const auto totalBins = bins.size();
   if (totalBins == 0)
   {
      return;
   }

   // The cursor position is a [0, 1] fraction of the bandwidth.
   // Convert to a bin index range.  Use the spectrum data's own bandwidth
   // so the extraction is always consistent with the cached FFT frame.
   const double sampleRate = _lastSpectrumData->bandwidthHz;
   const double hzPerBin = sampleRate / static_cast<double>(totalBins);

   const double cursorCenterBin =
      _bwCursorLockedX * static_cast<double>(totalBins);
   const double halfWidthBins = _bwCursorHalfWidthHz / hzPerBin;

   auto startBin = static_cast<size_t>(
      std::max(0.0, cursorCenterBin - halfWidthBins));
   auto endBin = static_cast<size_t>(
      std::min(static_cast<double>(totalBins),
               cursorCenterBin + halfWidthBins));

   if (endBin <= startBin)
   {
      return;
   }

   // Extract only the bins within the cursor region.
   const std::vector<float> region(bins.begin() + static_cast<ptrdiff_t>(startBin),
                                   bins.begin() + static_cast<ptrdiff_t>(endBin));

   // Decimate the extracted region for display.
   const auto decimated = SdrEngine::decimateSpectrum(region);
   _ui->_detailedSpectrumWidget->setData(decimated);
}

// ============================================================================
// Helpers
// ============================================================================
uint32_t MainWindow::sampleRateFromIndex(int index)
{
   if (index < std::ssize(SAMPLE_RATES))
   {
      return SAMPLE_RATES[static_cast<size_t>(index)];
   }
   return 2'400'000;
}

// ============================================================================
// Device auto-detection
// ============================================================================

void MainWindow::refreshDevices()
{
   if (_engine.isRunning())
   {
      return;
   }
   if (_deviceCombo == nullptr)
   {
      return;
   }

   // Block signals while repopulating so we don't trigger applyDeviceSelection
   // for every intermediate state.
   _deviceCombo->blockSignals(true);
   _deviceCombo->clear();
   _detectedDevices.clear();

   // Scan RTL-SDR devices.
   {
      const SdrEngine::RtlSdrDevice probe;
      for (auto& info : probe.enumerateDevices())
      {
         DetectedDevice entry;
         entry.backend = DeviceBackend::RtlSdr;
         entry.info    = std::move(info);
         _detectedDevices.push_back(std::move(entry));
      }
   }

   // Scan ADALM-PLUTO devices.
   {
      const SdrEngine::PlutoSdrDevice probe;
      for (auto& info : probe.enumerateDevices())
      {
         DetectedDevice entry;
         entry.backend = DeviceBackend::PlutoSdr;
         entry.info    = std::move(info);
         _detectedDevices.push_back(std::move(entry));
      }
   }

   // Populate the combo box.
   for (const auto& dev : _detectedDevices)
   {
      QString label;
      if (dev.backend == DeviceBackend::RtlSdr)
      {
         label = QString("RTL-SDR: %1")
                    .arg(QString::fromStdString(dev.info.name));
      }
      else
      {
         label = QString("Pluto: %1")
                    .arg(QString::fromStdString(dev.info.name));
      }
      if (!dev.info.serial.empty())
      {
         label += QString(" [%1]").arg(
            QString::fromStdString(dev.info.serial));
      }
      _deviceCombo->addItem(label);
   }

   if (_detectedDevices.empty())
   {
      _deviceCombo->addItem("(no devices found)");
   }

   _deviceCombo->blockSignals(false);

   // Auto-select the first device.
   if (!_detectedDevices.empty())
   {
      _deviceCombo->setCurrentIndex(0);
      applyDeviceSelection(0);
   }

   GPINFO("Device scan found {} device(s)", _detectedDevices.size());
}

void MainWindow::applyDeviceSelection(int comboIndex)
{
   if (_engine.isRunning())
   {
      return;
   }
   if (comboIndex < 0 ||
       static_cast<std::size_t>(comboIndex) >= _detectedDevices.size())
   {
      return;
   }

   const auto& dev = _detectedDevices[static_cast<std::size_t>(comboIndex)];
   if (dev.backend == DeviceBackend::RtlSdr)
   {
      _engine.setDevice(std::make_unique<SdrEngine::RtlSdrDevice>());
   }
   else
   {
      _engine.setDevice(std::make_unique<SdrEngine::PlutoSdrDevice>());
   }
}
