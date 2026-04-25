#ifndef OSCILLOSCOPEWIDGET_H_
#define OSCILLOSCOPEWIDGET_H_

// Third-party headers
#include <QPointF>
#include <QPushButton>
#include <QTimer>
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
   void mousePressEvent(QMouseEvent* event) override;
   void mouseMoveEvent(QMouseEvent* event) override;
   void mouseReleaseEvent(QMouseEvent* event) override;

private:
   void drawGrid(QPainter& painter, const QRect& area) const;
   void drawTraces(QPainter& painter, const QRect& area);
   void drawLegend(QPainter& painter, const QRect& area);
   void drawTriggerLine(QPainter& painter, const QRect& area) const;

   // Render a single trace (I or Q) using envelope or polyline mode
   // depending on samples-per-pixel.  Caller must already hold _mutex.
   template <typename ComponentFn>
   void drawOneTrace(QPainter& painter, const QRect& area,
                     std::size_t viewBegin, std::size_t viewEnd,
                     ComponentFn component, const QColor& color);

   [[nodiscard]] qreal yFromAmp(float amp, const QRect& area) const;
   void repositionButtons();
   void clampViewOffsetLocked();  // caller must hold _mutex

   // Check whether any sample in an incoming batch exceeds the trigger level.
   [[nodiscard]] bool checkTrigger(
      const std::vector<std::complex<float>>& incoming) const;

   // Deep capture ring buffer.  Separates "what is stored" (capacity)
   // from "what is displayed" (_timeSpan + _viewOffset), so pausing and
   // zooming in reveals samples captured at full rate.
   static constexpr std::size_t CAPTURE_CAPACITY = 1U << 20;  // ~1M samples

   std::mutex _mutex;
   std::vector<std::complex<float>> _capture;  // ring, size == capacity
   std::size_t _captureHead{0};                // next write index
   std::size_t _captureSize{0};                // valid element count

   // Display view
   std::size_t _timeSpan{512};
   std::size_t _viewOffset{0};   // samples back from newest (pan)
   float _axisRange{1.0F};
   bool _iTraceEnabled{true};
   bool _qTraceEnabled{true};
   double _sampleRateHz{2'400'000.0};
   bool _gridEnabled{true};

   // State
   bool _paused{false};
   bool _triggerEnabled{false};
   float _triggerLevel{0.5F};

   // Panning (only active while paused)
   bool _dragging{false};
   int _dragStartX{0};
   std::size_t _dragStartOffset{0};

   // UI
   QPushButton* _pauseButton{nullptr};
   QPushButton* _triggerButton{nullptr};

   // Repaint throttling: setData() sets _dirty; timer at ~60 Hz repaints.
   QTimer* _repaintTimer{nullptr};
   bool _dirty{false};

   // Scratch buffer reused across paints to avoid per-frame allocation.
   std::vector<QPointF> _scratch;

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
