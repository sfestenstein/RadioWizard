#ifndef CONSTELLATIONWIDGET_H_
#define CONSTELLATIONWIDGET_H_

// Project headers
#include "CircularBuffer.h"

// Third-party headers
#include <QWidget>

// System headers
#include <chrono>
#include <complex>
#include <mutex>
#include <utility>
#include <vector>

namespace RealTimeGraphs
{

/// Custom QPainter-based I/Q constellation diagram widget.
///
/// Renders a 2-D scatter plot of complex samples: the real part (I) on the
/// X-axis and the imaginary part (Q) on the Y-axis.  Recent points are
/// drawn brighter; older points fade toward the background.
class ConstellationWidget : public QWidget
{
   Q_OBJECT

public:
   /// @param historySize  Number of recent I/Q samples to display.
   /// @param parent       Parent widget.
   explicit ConstellationWidget(QWidget* parent = nullptr, int historySize = 4096);

   /// Replace the current constellation data.  Samples are appended to the
   /// persistence buffer.  Thread-safe.
   /// @param samples  Complex I/Q samples.
   void setData(const std::vector<std::complex<float>>& samples);

   /// Set the axis range (symmetric about the origin).
   void setAxisRange(float range);

   /// Set the point size in pixels.
   void setPointSize(int size);

   /// Enable or disable persistence (fading trail effect).
   void setPersistence(bool enable);

   /// Set persistence depth (how many samples to keep).
   void setPersistenceDepth(int depth);

   /// Set dot color (recent samples).
   void setDotColor(const QColor& color);

   /// Enable or disable grid drawing.
   void setGridEnabled(bool enable);

   /// Set the fade amount for persistence mode (0 = no fade, 255 = full fade).
   /// Controls how transparent the oldest points become.
   void setFadeAmount(int amount);

   /// Set the persistence fade time in seconds.
   /// Points fade linearly from full opacity to zero over this duration.
   /// @param seconds  Fade time (clamped to 0.5 – 30 s, default 5 s).
   void setFadeTime(float seconds);

   /// Minimum size hint for layout.
   [[nodiscard]] QSize minimumSizeHint() const override;

protected:
   void paintEvent(QPaintEvent* event) override;

private:
   void drawBackground(QPainter& painter, const QRect& area);
   void drawGrid(QPainter& painter, const QRect& area);
   void drawPoints(QPainter& painter, const QRect& area);

   /// Map an I/Q value to a pixel position within the plot area.
   [[nodiscard]] QPoint mapToPixel(float i, float q, const QRect& area) const;

   using Clock     = std::chrono::steady_clock;
   using TimePoint  = Clock::time_point;
   using TimedPoint = std::pair<TimePoint, std::complex<float>>;

   std::mutex _mutex;
   CommonUtils::CircularBuffer<TimedPoint> _points;

   float _axisRange{1.5F};
   int _pointSize{3};
   bool _persistence{true};
   QColor _dotColor{0, 255, 128};
   bool _gridEnabled{true};
   int _fadeAmount{255};  ///< 0 = no fade (all opaque), 255 = full fade (oldest invisible)
   float _fadeTimeSec{5.0F};  ///< Seconds for a point to fade from full opacity to zero

   // Layout margins
   static constexpr int MARGIN_LEFT   = 40;
   static constexpr int MARGIN_RIGHT  = 10;
   static constexpr int MARGIN_TOP    = 10;
   static constexpr int MARGIN_BOTTOM = 30;
};

} // namespace RealTimeGraphs

#endif // CONSTELLATIONWIDGET_H_
