#include "OscilloscopeWidget.h"
#include "CommonGuiUtils.h"

#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>

#include <algorithm>

namespace RealTimeGraphs
{

// ============================================================================
// Construction
// ============================================================================

OscilloscopeWidget::OscilloscopeWidget(QWidget* parent)
   : QWidget(parent)
{
   setMinimumSize(OscilloscopeWidget::minimumSizeHint());
   setAttribute(Qt::WA_OpaquePaintEvent);
}

// ============================================================================
// Public API
// ============================================================================

void OscilloscopeWidget::setData(const std::vector<std::complex<float>>& samples)
{
   {
      const std::lock_guard<std::mutex> lock(_mutex);

      // Keep only the last _timeSpan samples.
      if (samples.size() <= _timeSpan)
      {
         _samples = samples;
      }
      else
      {
         const auto offset = samples.size() - _timeSpan;
         _samples.assign(samples.begin() + static_cast<std::ptrdiff_t>(offset),
                         samples.end());
      }
   }
   safeUpdate(this);
}

void OscilloscopeWidget::setAxisRange(float range)
{
   _axisRange = range;
   safeUpdate(this);
}

void OscilloscopeWidget::setTimeSpan(std::size_t sampleCount)
{
   _timeSpan = std::max<std::size_t>(16, sampleCount);
   safeUpdate(this);
}

void OscilloscopeWidget::setITraceEnabled(bool enable)
{
   _iTraceEnabled = enable;
   safeUpdate(this);
}

void OscilloscopeWidget::setQTraceEnabled(bool enable)
{
   _qTraceEnabled = enable;
   safeUpdate(this);
}

void OscilloscopeWidget::setSampleRate(double rateHz)
{
   _sampleRateHz = rateHz;
   safeUpdate(this);
}

void OscilloscopeWidget::setGridEnabled(bool enable)
{
   _gridEnabled = enable;
   safeUpdate(this);
}

QSize OscilloscopeWidget::minimumSizeHint() const
{
   return {250, 150};
}

// ============================================================================
// Paint
// ============================================================================

void OscilloscopeWidget::paintEvent(QPaintEvent* /*event*/)
{
   QPainter painter(this);
   painter.setRenderHint(QPainter::Antialiasing, true);

   const QRect plotArea(MARGIN_LEFT, MARGIN_TOP,
                        width() - MARGIN_LEFT - MARGIN_RIGHT,
                        height() - MARGIN_TOP - MARGIN_BOTTOM);
   if (plotArea.width() <= 0 || plotArea.height() <= 0)
   {
      return;
   }

   PlotUtils::drawPlotBackground(painter, this, plotArea);
   if (_gridEnabled)
   {
      drawGrid(painter, plotArea);
   }
   drawTraces(painter, plotArea);
   drawLegend(painter, plotArea);
}

void OscilloscopeWidget::wheelEvent(QWheelEvent* event)
{
   constexpr float ZOOM_FACTOR = 1.15F;
   constexpr float MIN_AXIS_RANGE = 0.01F;
   constexpr float MAX_AXIS_RANGE = 10.0F;

   const int deltaY = event->angleDelta().y();
   if (deltaY == 0)
   {
      QWidget::wheelEvent(event);
      return;
   }

   const float factor = (deltaY > 0) ? (1.0F / ZOOM_FACTOR) : ZOOM_FACTOR;
   _axisRange = std::clamp(_axisRange * factor, MIN_AXIS_RANGE, MAX_AXIS_RANGE);

   update();
   event->accept();
}

// ============================================================================
// Drawing helpers
// ============================================================================

void OscilloscopeWidget::drawGrid(QPainter& painter, const QRect& area) const
{
   constexpr int H_DIVS = 8;
   constexpr int V_DIVS = 8;

   PlotUtils::drawRectangularGrid(painter, area, H_DIVS, V_DIVS,
                                         true, false);

   PlotUtils::setupTickLabelPainter(painter);

   // Y-axis tick labels (amplitude)
   for (int i = 0; i <= H_DIVS; i += 2)
   {
      const float val = _axisRange -
                        (2.0F * _axisRange * static_cast<float>(i) /
                         static_cast<float>(H_DIVS));
      const int y = area.top() + ((i * area.height()) / H_DIVS);
      const QString label = QString::number(static_cast<double>(val), 'f', 2);
      PlotUtils::drawYTick(painter, area, MARGIN_LEFT, y, label);
   }

   // X-axis tick labels (time)
   const double totalTimeSec =
      static_cast<double>(_timeSpan) / _sampleRateHz;

   for (int i = 0; i <= V_DIVS; i += 2)
   {
      const double timeSec =
         totalTimeSec * static_cast<double>(i) / static_cast<double>(V_DIVS);

      QString label;
      if (totalTimeSec < 1.0e-3)
      {
         label = QString::number(timeSec * 1.0e6, 'f', 1) + " us";
      }
      else if (totalTimeSec < 1.0)
      {
         label = QString::number(timeSec * 1.0e3, 'f', 2) + " ms";
      }
      else
      {
         label = QString::number(timeSec, 'f', 3) + " s";
      }

      const int x = area.left() + ((i * area.width()) / V_DIVS);
      PlotUtils::drawXTick(painter, area, MARGIN_BOTTOM, x, label);
   }
}

void OscilloscopeWidget::drawTraces(QPainter& painter, const QRect& area)
{
   const std::lock_guard<std::mutex> lock(_mutex);

   const auto count = _samples.size();
   if (count < 2)
   {
      return;
   }

   // Build I and Q polylines
   const auto numPoints = std::min(count, _timeSpan);
   const auto startIdx = count - numPoints;

   auto buildPolyline = [&](auto componentFn) -> QVector<QPointF>
   {
      QVector<QPointF> points;
      points.reserve(static_cast<qsizetype>(numPoints));
      for (std::size_t i = 0; i < numPoints; ++i)
      {
         const auto sampleIdx = static_cast<float>(i);
         const float amp = componentFn(_samples[startIdx + i]);
         points.append(mapToPixel(sampleIdx, amp, area));
      }
      return points;
   };

   if (_iTraceEnabled)
   {
      const auto iPoints = buildPolyline(
         [](const std::complex<float>& s) { return s.real(); });
      painter.setPen(QPen(_iColor, 1.5));
      painter.drawPolyline(iPoints.data(), static_cast<int>(iPoints.size()));
   }

   if (_qTraceEnabled)
   {
      const auto qPoints = buildPolyline(
         [](const std::complex<float>& s) { return s.imag(); });
      painter.setPen(QPen(_qColor, 1.5));
      painter.drawPolyline(qPoints.data(), static_cast<int>(qPoints.size()));
   }
}

void OscilloscopeWidget::drawLegend(QPainter& painter, const QRect& area)
{
   QFont font = painter.font();
   font.setPointSize(9);
   font.setBold(true);
   painter.setFont(font);

   const int legendX = area.right() - 75;
   const int legendY = area.top() + 6;
   constexpr int SWATCH_SIZE = 8;
   constexpr int LINE_HEIGHT = 14;

   // Semi-transparent background for readability
   painter.fillRect(legendX - 4, legendY - 2, 72, (LINE_HEIGHT * 2) + 4,
                    QColor(10, 10, 15, 180));

   if (_iTraceEnabled)
   {
      painter.fillRect(legendX, legendY + 2, SWATCH_SIZE, SWATCH_SIZE, _iColor);
      painter.setPen(_iColor);
      painter.drawText(legendX + SWATCH_SIZE + 4, legendY,
                       60, LINE_HEIGHT, Qt::AlignLeft | Qt::AlignVCenter, "I");
   }

   if (_qTraceEnabled)
   {
      const int y2 = legendY + LINE_HEIGHT;
      painter.fillRect(legendX, y2 + 2, SWATCH_SIZE, SWATCH_SIZE, _qColor);
      painter.setPen(_qColor);
      painter.drawText(legendX + SWATCH_SIZE + 4, y2,
                       60, LINE_HEIGHT, Qt::AlignLeft | Qt::AlignVCenter, "Q");
   }
}

QPointF OscilloscopeWidget::mapToPixel(float sampleIdx, float amplitude,
                                       const QRect& area) const
{
   // X: map sample index [0, _timeSpan) to plot width
   const float normX = sampleIdx / static_cast<float>(_timeSpan);
   const float px = static_cast<float>(area.left()) +
                    (normX * static_cast<float>(area.width()));

   // Y: map amplitude [-_axisRange, +_axisRange] to plot height (inverted)
   const float normY = 0.5F - (amplitude / (2.0F * _axisRange));
   const float py = static_cast<float>(area.top()) +
                    (normY * static_cast<float>(area.height()));

   return {static_cast<qreal>(px), static_cast<qreal>(py)};
}

} // namespace RealTimeGraphs