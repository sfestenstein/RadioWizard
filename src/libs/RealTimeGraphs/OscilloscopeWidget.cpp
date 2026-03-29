#include "OscilloscopeWidget.h"
#include "CommonGuiUtils.h"

#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

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

   _pauseButton = new QPushButton(QStringLiteral("\u23F8"), this);
   _pauseButton->setToolTip("Pause/Resume");
   _pauseButton->setFixedSize(28, 28);
   _pauseButton->setFocusPolicy(Qt::NoFocus);
   _pauseButton->setStyleSheet(
      "QPushButton { background: rgba(30,30,40,180); color: #cccccc;"
      " border: 1px solid #555; border-radius: 4px; font-size: 14px; }"
      "QPushButton:hover { background: rgba(60,60,80,200); }"
      "QPushButton:checked { color: #ff6644; }");
   _pauseButton->setCheckable(true);
   connect(_pauseButton, &QPushButton::toggled, this,
           &OscilloscopeWidget::setPaused);

   _triggerButton = new QPushButton(QStringLiteral("T"), this);
   _triggerButton->setToolTip("Trigger: pause when signal exceeds threshold");
   _triggerButton->setFixedSize(28, 28);
   _triggerButton->setFocusPolicy(Qt::NoFocus);
   _triggerButton->setStyleSheet(
      "QPushButton { background: rgba(30,30,40,180); color: #cccccc;"
      " border: 1px solid #555; border-radius: 4px; font-size: 12px;"
      " font-weight: bold; }"
      "QPushButton:hover { background: rgba(60,60,80,200); }"
      "QPushButton:checked { color: #44ff44; border-color: #44ff44; }");
   _triggerButton->setCheckable(true);
   connect(_triggerButton, &QPushButton::toggled, this,
           &OscilloscopeWidget::setTriggerEnabled);
}

// ============================================================================
// Public API
// ============================================================================

void OscilloscopeWidget::setData(const std::vector<std::complex<float>>& samples)
{
   if (_paused)
   {
      return;
   }
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

   // Trigger check: if armed, test whether any sample exceeds the threshold.
   if (_triggerEnabled && checkTrigger())
   {
      setPaused(true);
      emit triggerFired();
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

void OscilloscopeWidget::setPaused(bool paused)
{
   _paused = paused;
   if (_pauseButton->isChecked() != paused)
   {
      _pauseButton->setChecked(paused);
   }
   _pauseButton->setText(paused ? QStringLiteral("\u25B6")
                                : QStringLiteral("\u23F8"));

   // When resuming from a trigger-fired pause, disarm the trigger.
   if (!paused && _triggerEnabled)
   {
      _triggerEnabled = false;
      _triggerButton->setChecked(false);
      emit triggerDisarmed();
   }

   safeUpdate(this);
}

bool OscilloscopeWidget::isPaused() const
{
   return _paused;
}

void OscilloscopeWidget::setTriggerEnabled(bool enabled)
{
   // Cannot arm trigger while paused.
   if (enabled && _paused)
   {
      _triggerButton->setChecked(false);
      return;
   }

   _triggerEnabled = enabled;
   if (_triggerButton->isChecked() != enabled)
   {
      _triggerButton->setChecked(enabled);
   }

   if (enabled)
   {
      emit triggerArmed();
   }
   else
   {
      emit triggerDisarmed();
   }
   safeUpdate(this);
}

void OscilloscopeWidget::setTriggerLevel(float level)
{
   _triggerLevel = level;
   safeUpdate(this);
}

bool OscilloscopeWidget::isTriggerEnabled() const
{
   return _triggerEnabled;
}

float OscilloscopeWidget::triggerLevel() const
{
   return _triggerLevel;
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

   if (_triggerEnabled || _paused)
   {
      drawTriggerLine(painter, plotArea);
   }

   if (_paused)
   {
      painter.setPen(QColor(255, 100, 50, 200));
      QFont font = painter.font();
      font.setPointSize(9);
      font.setBold(true);
      painter.setFont(font);
      painter.drawText(plotArea.left() + 6, plotArea.top() + 16, "PAUSED");
   }
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

   const bool overYAxis = event->position().x() < MARGIN_LEFT;
   const bool overXAxis = event->position().y() > (height() - MARGIN_BOTTOM);

   // Over the time axis: zoom the time span.
   if (overXAxis)
   {
      constexpr std::size_t MIN_TIME_SPAN = 16;
      constexpr std::size_t MAX_TIME_SPAN = 65536;
      constexpr float TIME_ZOOM_FACTOR = 1.15F;
      const float factor = (deltaY > 0) ? (1.0F / TIME_ZOOM_FACTOR)
                                         : TIME_ZOOM_FACTOR;
      auto newSpan = static_cast<std::size_t>(
         static_cast<float>(_timeSpan) * factor);
      _timeSpan = std::clamp(newSpan, MIN_TIME_SPAN, MAX_TIME_SPAN);
      update();
      event->accept();
      return;
   }

   // When trigger is armed, scroll adjusts the trigger level instead
   // (unless the mouse is over the Y axis, where scroll always zooms).
   if (_triggerEnabled && !overYAxis)
   {
      constexpr float STEP = 0.02F;
      const float delta = (deltaY > 0) ? STEP : -STEP;
      _triggerLevel = std::clamp(_triggerLevel + delta, 0.01F, _axisRange);
      update();
      event->accept();
      return;
   }

   const float factor = (deltaY > 0) ? (1.0F / ZOOM_FACTOR) : ZOOM_FACTOR;
   _axisRange = std::clamp(_axisRange * factor, MIN_AXIS_RANGE, MAX_AXIS_RANGE);

   update();
   event->accept();
}

void OscilloscopeWidget::resizeEvent(QResizeEvent* event)
{
   QWidget::resizeEvent(event);
   repositionButtons();
}

void OscilloscopeWidget::repositionButtons()
{
   const int rightEdge = width() - MARGIN_RIGHT - 2;
   _pauseButton->move(rightEdge - _pauseButton->width(), MARGIN_TOP + 4);
   _triggerButton->move(rightEdge - _pauseButton->width(),
                        MARGIN_TOP + 4 + 4 + _pauseButton->height());
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

void OscilloscopeWidget::drawTriggerLine(QPainter& painter,
                                         const QRect& area) const
{
   // Draw horizontal dashed lines at +threshold and -threshold.
   QPen pen(QColor(68, 255, 68, 180), 1.5, Qt::DashLine);
   painter.setPen(pen);

   auto yFromAmp = [&](float amp) -> int
   {
      const float normY = 0.5F - (amp / (2.0F * _axisRange));
      return area.top() + static_cast<int>(normY * static_cast<float>(area.height()));
   };

   const int yPos = yFromAmp(_triggerLevel);
   const int yNeg = yFromAmp(-_triggerLevel);
   painter.drawLine(area.left(), yPos, area.right(), yPos);
   painter.drawLine(area.left(), yNeg, area.right(), yNeg);

   // Label
   QFont font = painter.font();
   font.setPointSize(8);
   painter.setFont(font);
   painter.setPen(QColor(68, 255, 68, 220));
   const QString label = QStringLiteral("T: ")
                       + QString::number(static_cast<double>(_triggerLevel), 'f', 2);
   painter.drawText(area.left() + 4, yPos - 3, label);
}

bool OscilloscopeWidget::checkTrigger()
{
   const std::lock_guard<std::mutex> lock(_mutex);
   for (const auto& s : _samples)
   {
      if (std::abs(s.real()) > _triggerLevel
          || std::abs(s.imag()) > _triggerLevel)
      {
         return true;
      }
   }
   return false;
}

} // namespace RealTimeGraphs