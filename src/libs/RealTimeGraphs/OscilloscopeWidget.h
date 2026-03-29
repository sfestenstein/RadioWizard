#ifndef OSCILLOSCOPEWIDGET_H_
#define OSCILLOSCOPEWIDGET_H_

// Project headers
#include "CircularBuffer.h"

// Third-party headers
#include <QPushButton>
#include <QWidget>

// System headers
#include <complex>
#include <mutex>
#include <vector>

namespace RealTimeGraphs
{

/**
 * @class OscilloscopeWidget
 * @brief Custom QPainter-based time-domain I/Q oscilloscope widget.
 *
 * Renders two traces (In-Phase and Quadrature) vs. sample index, giving
 * a real-time time-domain view of the received I/Q signal.
 */
class OscilloscopeWidget : public QWidget
{
   Q_OBJECT

public:
   explicit OscilloscopeWidget(QWidget* parent = nullptr);

   /**
    * @brief Replace the displayed I/Q waveform.  Thread-safe.
    * @param samples  Complex I/Q samples.
    */
   void setData(const std::vector<std::complex<float>>& samples);

   /** @brief Set the vertical axis range (symmetric about zero). */
   void setAxisRange(float range);

   /** @brief Set the number of samples visible on the time axis. */
   void setTimeSpan(std::size_t sampleCount);

   /** @brief Enable or disable the I (in-phase) trace. */
   void setITraceEnabled(bool enable);

   /** @brief Enable or disable the Q (quadrature) trace. */
   void setQTraceEnabled(bool enable);

   /** @brief Set the sample rate in Hz (used for time-axis labelling). */
   void setSampleRate(double rateHz);

   /** @brief Enable or disable grid drawing. */
   void setGridEnabled(bool enable);

   /** @brief Pause or resume live data updates. */
   void setPaused(bool paused);

   /** @brief Returns true if the display is currently paused. */
   [[nodiscard]] bool isPaused() const;

   /**
    * @brief Enable or disable trigger mode.
    * When armed, the display auto-pauses when any sample amplitude
    * exceeds the trigger threshold.
    */
   void setTriggerEnabled(bool enabled);

   /** @brief Set the trigger threshold (Y-axis amplitude). */
   void setTriggerLevel(float level);

   /** @brief Returns true if trigger mode is currently armed. */
   [[nodiscard]] bool isTriggerEnabled() const;

   /** @brief Returns the current trigger threshold. */
   [[nodiscard]] float triggerLevel() const;

   /** @brief Minimum size hint for layout. */
   [[nodiscard]] QSize minimumSizeHint() const override;

signals:
   /** @brief Emitted when the trigger fires (signal exceeded threshold). */
   void triggerFired();

   /** @brief Emitted when trigger mode is armed (enabled). */
   void triggerArmed();

   /** @brief Emitted when trigger mode is disarmed (disabled or after resume). */
   void triggerDisarmed();

protected:
   void paintEvent(QPaintEvent* event) override;
   void wheelEvent(QWheelEvent* event) override;
   void resizeEvent(QResizeEvent* event) override;

private:
   void drawGrid(QPainter& painter, const QRect& area) const;
   void drawTraces(QPainter& painter, const QRect& area);
   void drawLegend(QPainter& painter, const QRect& area);
   void drawTriggerLine(QPainter& painter, const QRect& area) const;

   // Map a (sampleIndex, amplitude) to pixel within the plot area.
   [[nodiscard]] QPointF mapToPixel(float sampleIdx, float amplitude,
                                    const QRect& area) const;
   void repositionButtons();

   // Check whether any sample in the current data exceeds the trigger level.
   [[nodiscard]] bool checkTrigger();

   std::mutex _mutex;
   bool _paused{false};
   bool _triggerEnabled{false};
   float _triggerLevel{0.5F};
   QPushButton* _pauseButton{nullptr};
   QPushButton* _triggerButton{nullptr};
   std::vector<std::complex<float>> _samples;

   float _axisRange{1.0F};
   std::size_t _timeSpan{512};
   bool _iTraceEnabled{true};
   bool _qTraceEnabled{true};
   double _sampleRateHz{2'400'000.0};
   bool _gridEnabled{true};

   QColor _iColor{0, 200, 255};    // Cyan for I
   QColor _qColor{255, 100, 50};   // Orange-red for Q

   // Layout margins (matches SpectrumWidget vertical alignment)
   static constexpr int MARGIN_LEFT   = 45;
   static constexpr int MARGIN_RIGHT  = 10;
   static constexpr int MARGIN_TOP    = 10;
   static constexpr int MARGIN_BOTTOM = 25;
};

} // namespace RealTimeGraphs

#endif // OSCILLOSCOPEWIDGET_H_
