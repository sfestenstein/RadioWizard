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
constexpr size_t NUM_SAMPLE_RATES = 9;
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

constexpr size_t NUM_FFT_SIZES = 7;
constexpr std::array<int, NUM_FFT_SIZES> FFT_SIZES = {512, 1024, 2048, 4096, 8192, 16384, 32768};

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

   // Set defaults that match the UI initial values.
   _engine.setCenterFrequency(100'000'000);   // 100 MHz
   _engine.setSampleRate(2'400'000);           // 2.4 MS/s
   _engine.setFftSize(2048);

   // Default combo selections.
   _ui->_sampleRateCombo->setCurrentIndex(5);  // 2.4 MS/s
   _ui->_fftSizeCombo->setCurrentIndex(2);     // 2048

   // Configure the plot widgets for dB input and frequency display.
   _ui->_spectrurmWidget->setInputIsDb(true);
   _ui->_spectrurmWidget->setDbRange(-120.0F, 0.0F);
   _ui->_waterfallWidget->setInputIsDb(true);
   _ui->_waterfallWidget->setDbRange(-120.0F, 0.0F);
   _ui->_detailedSpectrumWidget->setInputIsDb(true);
   _ui->_detailedSpectrumWidget->setDbRange(-120.0F, 0.0F);

   // Wire UI signals → slots.
   connect(_ui->_startStopButton, &QPushButton::toggled,
           this, &MainWindow::onStartStopToggled);
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
   if (index < static_cast<int>(std::ssize(FFT_SIZES)))
   {
      _engine.setFftSize(FFT_SIZES[index]);
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
   if (index < static_cast<int>(std::ssize(MAP)))
   {
      _engine.setWindowFunction(MAP[index]);
   }
}

// ============================================================================
// DataHandler wiring
// ============================================================================

void MainWindow::connectDataHandlers()
{
   // --- Spectrum data → SpectrumWidget + WaterfallWidget ---
   _spectrumListenerId = _engine.spectrumDataHandler().registerListener(
      [this](const std::shared_ptr<const SdrEngine::SpectrumData>& data)
      {
         // DataHandler invokes listeners on its own thread.
         // We need to marshal updates to the GUI thread.
         QMetaObject::invokeMethod(this, [this, data]()
         {
            try 
            {
               _ui->_spectrurmWidget->setFrequencyRange(
                  data->centerFreqHz, data->bandwidthHz);
               _ui->_spectrurmWidget->setData(data->magnitudesDb);

               _ui->_detailedSpectrumWidget->setFrequencyRange(
                  data->centerFreqHz, data->bandwidthHz);
               _ui->_detailedSpectrumWidget->setData(data->magnitudesDb);

               _ui->_waterfallWidget->setFrequencyRange(
                  data->centerFreqHz, data->bandwidthHz);
               _ui->_waterfallWidget->addRow(data->magnitudesDb);
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
      [this](const std::shared_ptr<const SdrEngine::IqBuffer>& data)
      {
         QMetaObject::invokeMethod(this, [this, data]()
         {
            _ui->_constellationWidget->setData(data->samples);
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
}

// ============================================================================
// Helpers
// ============================================================================

uint32_t MainWindow::sampleRateFromIndex(int index)
{
   if (index < static_cast<int>(std::ssize(SAMPLE_RATES)))
   {
      return SAMPLE_RATES[index];
   }
   return 2'400'000;
}
