// Project headers
#include "MainWindow.h"
#include "GeneralLogger.h"
#include "RtlSdrDevice.h"

// Generated UI header
#include "./ui_MainWindow.h"

// Third-party headers
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QMetaObject>
#include <QPushButton>
#include <QSlider>

// System headers
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>

namespace
{

/// Sample rates matching the combo-box order in the .ui file.
constexpr size_t NUM_SAMPLE_RATES = 8;
constexpr std::array<uint32_t, NUM_SAMPLE_RATES> SAMPLE_RATES =
{
   250'000,
   1'024'000,
   1'400'000,
   1'800'000,
   2'048'000,
   2'400'000,
   2'800'000,
   3'200'000,
};

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

   // Inject an RTL-SDR device into the engine.
   _engine.setDevice(std::make_unique<SdrEngine::RtlSdrDevice>());

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

   // Set up the Color Theme combo Box
   for (std::size_t i = 0; i < RealTimeGraphs::ColorMap::paletteCount(); ++i)
   {
      auto pal = RealTimeGraphs::ColorMap::paletteAt(i);
      _ui->_colorThemeCombo->addItem(
          QString::fromStdString(RealTimeGraphs::ColorMap::paletteName(pal)),
          static_cast<int>(i));
   }
   QObject::connect(_ui->_colorThemeCombo, &QComboBox::currentIndexChanged, this, [this](int idx)
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
   connect(_ui->_centerFreqSpinBox, &QDoubleSpinBox::valueChanged,
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
   onFftAverageChanged(33); 

   // Bandwidth cursor synchronisation between Spectrum and Waterfall
   QObject::connect(_ui->_spectrurmWidget,
                    &RealTimeGraphs::SpectrumWidget::bandwidthCursorHalfWidthChanged,
                    _ui->_waterfallWidget,
                    &RealTimeGraphs::WaterfallWidget::setBandwidthCursorHalfWidthHz);
   QObject::connect(_ui->_waterfallWidget,
                    &RealTimeGraphs::WaterfallWidget::bandwidthCursorHalfWidthChanged,
                    _ui->_spectrurmWidget,
                    &RealTimeGraphs::SpectrumWidget::setBandwidthCursorHalfWidthHz);
   QObject::connect(_ui->_spectrurmWidget,
                    &RealTimeGraphs::SpectrumWidget::bandwidthCursorLocked,
                    _ui->_waterfallWidget,
                    &RealTimeGraphs::WaterfallWidget::lockBandwidthCursorAt);
   QObject::connect(_ui->_waterfallWidget,
                    &RealTimeGraphs::WaterfallWidget::bandwidthCursorLocked,
                    _ui->_spectrurmWidget,
                    &RealTimeGraphs::SpectrumWidget::lockBandwidthCursorAt);
   QObject::connect(_ui->_spectrurmWidget,
                    &RealTimeGraphs::SpectrumWidget::bandwidthCursorUnlocked,
                    _ui->_waterfallWidget,
                    &RealTimeGraphs::WaterfallWidget::unlockBandwidthCursor);
   QObject::connect(_ui->_waterfallWidget,
                    &RealTimeGraphs::WaterfallWidget::bandwidthCursorUnlocked,
                    _ui->_spectrurmWidget,
                    &RealTimeGraphs::SpectrumWidget::unlockBandwidthCursor);

   // Bandwidth cursor → channel filter wiring
   QObject::connect(_ui->_spectrurmWidget,
                    &RealTimeGraphs::SpectrumWidget::bandwidthCursorLocked,
                    this, &MainWindow::onBwCursorLocked);
   QObject::connect(_ui->_waterfallWidget,
                    &RealTimeGraphs::WaterfallWidget::bandwidthCursorLocked,
                    this, &MainWindow::onBwCursorLocked);
   QObject::connect(_ui->_spectrurmWidget,
                    &RealTimeGraphs::SpectrumWidget::bandwidthCursorUnlocked,
                    this, &MainWindow::onBwCursorUnlocked);
   QObject::connect(_ui->_waterfallWidget,
                    &RealTimeGraphs::WaterfallWidget::bandwidthCursorUnlocked,
                    this, &MainWindow::onBwCursorUnlocked);
   QObject::connect(_ui->_spectrurmWidget,
                    &RealTimeGraphs::SpectrumWidget::bandwidthCursorHalfWidthChanged,
                    this, &MainWindow::onBwCursorHalfWidthChanged);
   QObject::connect(_ui->_waterfallWidget,
                    &RealTimeGraphs::WaterfallWidget::bandwidthCursorHalfWidthChanged,
                    this, &MainWindow::onBwCursorHalfWidthChanged);

   // Tie the Spectrum and Waterfall widgets' color maps together.
   _ui->_spectrurmWidget->setXAxisVisible(false); // Waterfall below shows the shared X axis

   // Bidirectional X-axis linking
   QObject::connect(_ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::xViewChanged, _ui->_waterfallWidget,
                    &RealTimeGraphs::WaterfallWidget::setXViewRange);
   QObject::connect(_ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::xViewChanged, _ui->_spectrurmWidget,
                    &RealTimeGraphs::SpectrumWidget::setXViewRange);

   // Bidirectional tracking-cursor linking (vertical line sync)
   QObject::connect(_ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::trackingCursorXChanged, _ui->_waterfallWidget,
                    &RealTimeGraphs::WaterfallWidget::setLinkedCursorX);
   QObject::connect(_ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::trackingCursorLeft, _ui->_waterfallWidget,
                    &RealTimeGraphs::WaterfallWidget::clearLinkedCursorX);
   QObject::connect(_ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::trackingCursorXChanged, _ui->_spectrurmWidget,
                    &RealTimeGraphs::SpectrumWidget::setLinkedCursorX);
   QObject::connect(_ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::trackingCursorLeft, _ui->_spectrurmWidget,
                    &RealTimeGraphs::SpectrumWidget::clearLinkedCursorX);

   // Bidirectional measurement-cursor linking (vertical lines only)
   QObject::connect(_ui->_spectrurmWidget, &RealTimeGraphs::SpectrumWidget::measCursorsChanged, _ui->_waterfallWidget,
                    &RealTimeGraphs::WaterfallWidget::setLinkedMeasCursors);
   QObject::connect(_ui->_waterfallWidget, &RealTimeGraphs::WaterfallWidget::measCursorsChanged, _ui->_spectrurmWidget,
                    &RealTimeGraphs::SpectrumWidget::setLinkedMeasCursors);

}

MainWindow::~MainWindow()
{
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
      disconnectDataHandlers();
      _ui->_startStopButton->setText("Start");
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
}

void MainWindow::onSampleRateChanged(int index)
{
   _engine.setSampleRate(sampleRateFromIndex(index));
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
      _engine.setChannelFilterEnabled(false);
   }
}

void MainWindow::onBwCursorLocked(double xData)
{
   // xData is a fraction [0, 1] of the total bandwidth.
   // 0.5 = centre frequency.  Convert to Hz offset.
   auto sampleRate = static_cast<double>(_engine.getSampleRate());
   const double offsetHz = (xData - 0.5) * sampleRate;
   const double bwHz = _bwCursorHalfWidthHz * 2.0;

   _engine.configureChannelFilter(offsetHz, bwHz);
   _engine.setChannelFilterEnabled(true);
}

void MainWindow::onBwCursorUnlocked()
{
   _engine.setChannelFilterEnabled(false);
}

void MainWindow::onBwCursorHalfWidthChanged(double halfWidthHz)
{
   _bwCursorHalfWidthHz = halfWidthHz;

   // If the channel filter is already enabled (cursor locked),
   // update its bandwidth in real time.
   if (_engine.isChannelFilterEnabled())
   {
      const double offsetHz = _engine.channelFilter().getCenterOffset();
      const double bwHz = halfWidthHz * 2.0;
      _engine.configureChannelFilter(offsetHz, bwHz);
   }
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
         // DataHandler invokes listeners on its own thread.
         // We need to marshal updates to the GUI thread.
         QMetaObject::invokeMethod(this, [this, specData]()
         {
            try
            {
               _ui->_spectrurmWidget->setFrequencyRange(
                  specData->centerFreqHz, specData->bandwidthHz);
               _ui->_spectrurmWidget->setData(specData->magnitudesDb);

               _ui->_detailedSpectrumWidget->setFrequencyRange(
                  specData->centerFreqHz, specData->bandwidthHz);
               _ui->_detailedSpectrumWidget->setData(specData->magnitudesDb);

               _ui->_waterfallWidget->setFrequencyRange(
                  specData->centerFreqHz, specData->bandwidthHz);
               _ui->_waterfallWidget->addRow(specData->magnitudesDb);
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
      });

   // --- I/Q data → ConstellationWidget ---
   _iqListenerId = _engine.iqDataHandler().registerListener(
      [this](const std::shared_ptr<const SdrEngine::IqBuffer>& iqData)
      {
         QMetaObject::invokeMethod(this, [this, iqData]()
         {
            // Only use unfiltered IQ when no channel filter is active.
            if (!_engine.isChannelFilterEnabled())
            {
               _ui->_constellationWidget->setData(iqData->samples);
            }
         });
      });

   // --- Filtered I/Q data → ConstellationWidget ---
   _filteredIqListenerId = _engine.filteredIqDataHandler().registerListener(
      [this](const std::shared_ptr<const SdrEngine::IqBuffer>& iqData)
      {
         QMetaObject::invokeMethod(this, [this, iqData]()
         {
            _ui->_constellationWidget->setData(iqData->samples);
         });
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
