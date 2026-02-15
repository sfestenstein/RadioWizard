#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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

class MainWindow : public QMainWindow
{
   Q_OBJECT

public:
   MainWindow(QWidget* parent = nullptr);
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

private:
   /// Register DataHandler listeners for spectrum and I/Q data.
   void connectDataHandlers();

   /// Remove DataHandler listeners on shutdown.
   void disconnectDataHandlers();

   /// Map the sample-rate combo index to Hz.
   static uint32_t sampleRateFromIndex(int index);

   Ui::MainWindow* _ui;
   SdrEngine::SdrEngine _engine;

   int _spectrumListenerId{-1};
   int _iqListenerId{-1};
};

#endif // MAINWINDOW_H
