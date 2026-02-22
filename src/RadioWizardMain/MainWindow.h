#ifndef MAINWINDOW_H_
#define MAINWINDOW_H_

// Project headers
#include "SdrEngine.h"

// Third-party headers
#include <QMainWindow>

QT_BEGIN_NAMESPACE

namespace Ui
{
class MainWindow;
}

QT_END_NAMESPACE

/**
 * @class MainWindow
 * @brief Main application window for RadioWizard.
 *
 * Hosts the SDR engine controls, spectrum/waterfall display, and
 * constellation diagram.
 */
class MainWindow : public QMainWindow
{
   Q_OBJECT

public:
   /**
    * @brief Construct the main window.
    *
    * @param parent Parent widget.
    */
   MainWindow(QWidget* parent = nullptr);

   /**
    * @brief Destroy the main window.
    */
   ~MainWindow() override;

private slots:
   void onStartStopToggled(bool checked);
   void onAutoScaleClicked();
   void onCenterFreqChanged(double valueMhz);
   void onSampleRateChanged(int index);
   void onAutoGainToggled(bool checked);
   void onGainSliderChanged(int value);
   void onFftSizeChanged(int index);
   void onWindowFuncChanged(int index);
   void onFftAverageChanged(int value);
   void onDcSpikeRemovalToggled(bool checked);
   void onBwCursorToggled(bool checked);
   void onBwCursorLocked(double xData);
   void onBwCursorUnlocked();
   void onBwCursorHalfWidthChanged(double halfWidthHz);

private:
   // Register DataHandler listeners for spectrum and I/Q data.
   void connectDataHandlers();

   // Remove DataHandler listeners on shutdown.
   void disconnectDataHandlers();

   // Switch constellation plot to use unfiltered IQ data.
   void switchToUnfilteredIq();

   // Switch constellation plot to use filtered IQ data.
   void switchToFilteredIq();

   // Map the sample-rate combo index to Hz.
   static uint32_t sampleRateFromIndex(int index);

   Ui::MainWindow* _ui;
   SdrEngine::SdrEngine _engine;

   int _spectrumListenerId{-1};
   int _iqListenerId{-1};
   int _filteredIqListenerId{-1};

   // Cached bandwidth cursor state for channel filter configuration.
   double _bwCursorHalfWidthHz{100'000.0};
   double _bwCursorLockedX{0.5};

};

#endif // MAINWINDOW_H_
