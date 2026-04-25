#include "OscilloscopeWidget.h"
#include "CommonGuiUtils.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

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

   _capture.resize(CAPTURE_CAPACITY);
   _scratch.reserve(4096);

   // Repaint at most ~60 Hz regardless of incoming data rate.
   _repaintTimer = new QTimer(this);
   _repaintTimer->setTimerType(Qt::PreciseTimer);
   _repaintTimer->setInterval(16);   // ~60 Hz
   connect(_repaintTimer, &QTimer::timeout, this, [this]()
   {
      if (_dirty)
      {
         _dirty = false;
         update();
      }
   });
   _repaintTimer->start();
}

// ============================================================================
// Public API
// ============================================================================

void OscilloscopeWidget::setData(const std::vector<std::complex<float>>& samples)
{
   // Trigger check scans the incoming batch directly (lock-free).
   if (!_paused && _triggerEnabled && checkTrigger(samples))
   {
      setPaused(true);
      emit triggerFired();
   }

   if (_paused || samples.empty())
   {
      return;
   }

   {
      const std::lock_guard<std::mutex> lock(_mutex);

      // Append into the ring.  If the incoming batch is larger than the
      // ring, keep only its tail.
      const auto* src = samples.data();
      auto n = samples.size();
      if (n >= CAPTURE_CAPACITY)
      {
         src += (n - CAPTURE_CAPACITY);
         n = CAPTURE_CAPACITY;
      }

      const auto tailSpace = CAPTURE_CAPACITY - _captureHead;
      const auto first = std::min(n, tailSpace);
      std::copy(src, src + first, _capture.begin()
                + static_cast<std::ptrdiff_t>(_captureHead));
      if (n > first)
      {
         std::copy(src + first, src + n, _capture.begin());
      }
      _captureHead = (_captureHead + n) % CAPTURE_CAPACITY;
      _captureSize = std::min(CAPTURE_CAPACITY, _captureSize + n);

      // Live view always anchors to the newest sample.
      _viewOffset = 0;
   }

   _dirty = true;  // coalesced by the 60 Hz repaint timer
}

void OscilloscopeWidget::setAxisRange(float range)
{
   _axisRange = range;
   safeUpdate(this);
}

void OscilloscopeWidget::setTimeSpan(std::size_t sampleCount)
{
   {
      const std::lock_guard<std::mutex> lock(_mutex);
      _timeSpan = std::clamp<std::size_t>(sampleCount, 16, CAPTURE_CAPACITY);
      clampViewOffsetLocked();
   }
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

   // When resuming, snap the view back to the newest sample.
   if (!paused)
   {
      const std::lock_guard<std::mutex> lock(_mutex);
      _viewOffset = 0;
   }

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
      constexpr std::size_t MAX_TIME_SPAN = CAPTURE_CAPACITY;
      constexpr float TIME_ZOOM_FACTOR = 1.15F;
      const float factor = (deltaY > 0) ? (1.0F / TIME_ZOOM_FACTOR)
                                         : TIME_ZOOM_FACTOR;
      auto newSpan = static_cast<std::size_t>(
         static_cast<float>(_timeSpan) * factor);
      newSpan = std::clamp(newSpan, MIN_TIME_SPAN, MAX_TIME_SPAN);

      {
         const std::lock_guard<std::mutex> lock(_mutex);
         const auto oldSpan = _timeSpan;
         _timeSpan = newSpan;
         // When paused, keep the center of the visible window fixed.
         if (_paused && _captureSize > 0 && oldSpan != newSpan)
         {
            const long long oldCenter =
               static_cast<long long>(_viewOffset) +
               static_cast<long long>(oldSpan) / 2;
            long long newOff = oldCenter -
               static_cast<long long>(newSpan) / 2;
            if (newOff < 0) newOff = 0;
            _viewOffset = static_cast<std::size_t>(newOff);
         }
         clampViewOffsetLocked();
      }
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

   if (_captureSize < 2)
   {
      return;
   }

   // Compute the visible window in logical indices.
   // Logical 0 = oldest stored sample, (_captureSize - 1) = newest.
   const auto viewEnd = _captureSize - std::min(_viewOffset, _captureSize);
   const auto numPoints = std::min(_timeSpan, viewEnd);
   if (numPoints < 2)
   {
      return;
   }
   const auto viewBegin = viewEnd - numPoints;

   if (_iTraceEnabled)
   {
      drawOneTrace(painter, area, viewBegin, viewEnd,
                   [](const std::complex<float>& s) { return s.real(); },
                   _iColor);
   }
   if (_qTraceEnabled)
   {
      drawOneTrace(painter, area, viewBegin, viewEnd,
                   [](const std::complex<float>& s) { return s.imag(); },
                   _qColor);
   }
}

template <typename ComponentFn>
void OscilloscopeWidget::drawOneTrace(QPainter& painter, const QRect& area,
                                      std::size_t viewBegin,
                                      std::size_t viewEnd,
                                      ComponentFn component,
                                      const QColor& color)
{
   const auto numPoints = viewEnd - viewBegin;
   const int W = area.width();
   if (W <= 0 || numPoints < 2)
   {
      return;
   }

   // Map logical index -> physical ring index.
   const auto oldest = (_captureSize < CAPTURE_CAPACITY) ? 0U : _captureHead;
   auto physIdx = [this, oldest](std::size_t logical)
   {
      return (oldest + logical) % CAPTURE_CAPACITY;
   };

   const double spp = static_cast<double>(numPoints) /
                      static_cast<double>(W);

   painter.setPen(QPen(color, 1.5));

   constexpr double ENVELOPE_SPP = 2.0;

   if (spp > ENVELOPE_SPP)
   {
      // Envelope: one min + one max per pixel column.
      _scratch.resize(static_cast<std::size_t>(W) * 2);
      std::size_t out = 0;
      for (int x = 0; x < W; ++x)
      {
         auto s0 = viewBegin + static_cast<std::size_t>(
                                  static_cast<double>(x) * spp);
         auto s1 = viewBegin + static_cast<std::size_t>(
                                  static_cast<double>(x + 1) * spp);
         if (s1 > viewEnd) s1 = viewEnd;
         if (s1 <= s0) continue;

         float mn = std::numeric_limits<float>::infinity();
         float mx = -std::numeric_limits<float>::infinity();
         for (auto i = s0; i < s1; ++i)
         {
            const float v = component(_capture[physIdx(i)]);
            if (v < mn) mn = v;
            if (v > mx) mx = v;
         }
         const qreal px = static_cast<qreal>(area.left() + x);
         _scratch[out++] = QPointF(px, yFromAmp(mx, area));
         _scratch[out++] = QPointF(px, yFromAmp(mn, area));
      }
      painter.drawPolyline(_scratch.data(), static_cast<int>(out));
   }
   else
   {
      // Full-resolution polyline.
      _scratch.resize(numPoints);
      const qreal xScale = static_cast<qreal>(W) /
                           static_cast<qreal>(numPoints - 1);
      for (std::size_t i = 0; i < numPoints; ++i)
      {
         const float v = component(_capture[physIdx(viewBegin + i)]);
         _scratch[i] = QPointF(
            static_cast<qreal>(area.left()) +
               static_cast<qreal>(i) * xScale,
            yFromAmp(v, area));
      }
      painter.drawPolyline(_scratch.data(), static_cast<int>(numPoints));

      // Dots at each sample when very zoomed in.
      if (spp < 0.25)
      {
         painter.setBrush(color);
         painter.setPen(Qt::NoPen);
         for (const auto& pt : _scratch)
         {
            painter.drawEllipse(pt, 2.0, 2.0);
         }
         painter.setBrush(Qt::NoBrush);
         painter.setPen(QPen(color, 1.5));
      }
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

qreal OscilloscopeWidget::yFromAmp(float amp, const QRect& area) const
{
   const float normY = 0.5F - (amp / (2.0F * _axisRange));
   return static_cast<qreal>(area.top()) +
          static_cast<qreal>(normY) * static_cast<qreal>(area.height());
}

void OscilloscopeWidget::drawTriggerLine(QPainter& painter,
                                         const QRect& area) const
{
   // Draw horizontal dashed lines at +threshold and -threshold.
   const QPen pen(QColor(68, 255, 68, 180), 1.5, Qt::DashLine);
   painter.setPen(pen);

   const int yPos = static_cast<int>(yFromAmp(_triggerLevel, area));
   const int yNeg = static_cast<int>(yFromAmp(-_triggerLevel, area));
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

bool OscilloscopeWidget::checkTrigger(
   const std::vector<std::complex<float>>& incoming) const
{
   return std::ranges::any_of(incoming, [this](const std::complex<float>& s)
   {
      return std::abs(s.real()) > _triggerLevel
             || std::abs(s.imag()) > _triggerLevel;
   });
}

void OscilloscopeWidget::clampViewOffsetLocked()
{
   const auto maxOff = (_captureSize > _timeSpan)
                          ? (_captureSize - _timeSpan)
                          : 0U;
   if (_viewOffset > maxOff)
   {
      _viewOffset = maxOff;
   }
}

// ============================================================================
// Mouse: drag-to-pan while paused
// ============================================================================

void OscilloscopeWidget::mousePressEvent(QMouseEvent* event)
{
   if (_paused && event->button() == Qt::LeftButton)
   {
      _dragging = true;
      _dragStartX = static_cast<int>(event->position().x());
      {
         const std::lock_guard<std::mutex> lock(_mutex);
         _dragStartOffset = _viewOffset;
      }
      setCursor(Qt::ClosedHandCursor);
      event->accept();
      return;
   }
   QWidget::mousePressEvent(event);
}

void OscilloscopeWidget::mouseMoveEvent(QMouseEvent* event)
{
   if (_dragging)
   {
      const int plotW = width() - MARGIN_LEFT - MARGIN_RIGHT;
      if (plotW > 0)
      {
         const std::lock_guard<std::mutex> lock(_mutex);
         const auto visible = std::min(_timeSpan, _captureSize);
         const double spp = static_cast<double>(visible) /
                            static_cast<double>(plotW);
         const int dx = static_cast<int>(event->position().x())
                      - _dragStartX;
         // Dragging right reveals earlier data, so offset increases.
         const long long deltaSamples = static_cast<long long>(
            static_cast<double>(dx) * spp);
         long long newOff = static_cast<long long>(_dragStartOffset)
                          + deltaSamples;
         if (newOff < 0) newOff = 0;
         _viewOffset = static_cast<std::size_t>(newOff);
         clampViewOffsetLocked();
      }
      update();
      event->accept();
      return;
   }
   QWidget::mouseMoveEvent(event);
}

void OscilloscopeWidget::mouseReleaseEvent(QMouseEvent* event)
{
   if (_dragging && event->button() == Qt::LeftButton)
   {
      _dragging = false;
      unsetCursor();
      event->accept();
      return;
   }
   QWidget::mouseReleaseEvent(event);
}

} // namespace RealTimeGraphs