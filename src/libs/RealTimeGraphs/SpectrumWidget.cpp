#include "SpectrumWidget.h"
#include "ColorBarWidget.h"
#include "CommonGuiUtils.h"

#include <GeneralLogger.h>

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>

namespace RealTimeGraphs
{

// ============================================================================
// Construction
// ============================================================================

SpectrumWidget::SpectrumWidget(QWidget* parent)
   : PlotWidgetBase(parent)
{
   _colorBar = new ColorBarStrip(this);
   _colorBar->setDbRange(_minDb, _maxDb);
   _colorBar->setColorMap(_colorMap);

   // Configure cursor overlay callbacks (Y-axis is dB-specific)
   _cursorOverlay.setPixelToData(
      [this](const QPoint& pos, const QRect& area) -> PlotCursorOverlay::DataPoint
      {
         const double xFrac = static_cast<double>(pos.x() - area.left())
                            / static_cast<double>(area.width());
         const double dataFrac = _viewXStart + (xFrac * (_viewXEnd - _viewXStart));
         const double yFrac = static_cast<double>(pos.y() - area.top())
                            / static_cast<double>(area.height());
         const double db = _viewMaxDb - (yFrac * (_viewMaxDb - _viewMinDb));
         return {dataFrac, db};
      });

   _cursorOverlay.setDataToPixel(
      [this](const PlotCursorOverlay::DataPoint& dp,
             const QRect& area) -> QPoint
      {
         const double xFrac = (dp.x - _viewXStart) / (_viewXEnd - _viewXStart);
         const int px = area.left() + static_cast<int>(xFrac * area.width());
         const double yFrac = (_viewMaxDb - dp.y) / (_viewMaxDb - _viewMinDb);
         const int py = area.top() + static_cast<int>(yFrac * area.height());
         return {px, py};
      });

   _cursorOverlay.setFormatX(
      [this](double xVal) -> QString { return formatXValue(xVal); });

   _cursorOverlay.setFormatY(
      [](double yVal) -> QString
      { return QString::number(yVal, 'f', 1) + " dB"; });

   _cursorOverlay.setFormatDeltaX(
      [this](double x1, double x2) -> QString
      { return formatDeltaXValue(x1, x2); });

   _cursorOverlay.setFormatDeltaY(
      [](double y1, double y2) -> QString
      {
         const double dy = y1 - y2;
         return QString::fromUtf8("\u0394: ") + QString::number(dy, 'f', 1) + " dB";
      });
}

// ============================================================================
// Public API
// ============================================================================

void SpectrumWidget::setData(const std::vector<float>& magnitudes)
{
   {
      const std::lock_guard<std::mutex> lock(_mutex);
      _data = magnitudes;

      // Update max-hold envelope
      if (_maxHoldEnabled)
      {
         auto sz = magnitudes.size();
         if (_maxHoldData.size() != sz)
         {
            _maxHoldData = magnitudes;
         }
         else
         {
            // Decay rate: convert dB/s to a per-frame linear factor.
            // Assuming ~30 FPS; decay is applied in dB domain.
            constexpr float FPS_ESTIMATE = 30.0F;
            constexpr float EPSILON = 1.0e-10F;
            const float decayDb = _maxHoldDecayRate / FPS_ESTIMATE;

            for (std::size_t i = 0; i < sz; ++i)
            {
               float inDb = magnitudes[i];
               float holdDb = _maxHoldData[i];
               if (!_inputIsDb)
               {
                  inDb = 20.0F * std::log10(std::max(magnitudes[i], EPSILON));
                  holdDb = 20.0F * std::log10(std::max(_maxHoldData[i], EPSILON));
               }
               // Decay the hold value, then take the max
               holdDb -= decayDb;
               holdDb = std::max(holdDb, inDb);

               if (!_inputIsDb)
               {
                  _maxHoldData[i] = std::pow(10.0F, holdDb / 20.0F);
               }
               else
               {
                  _maxHoldData[i] = holdDb;
               }
            }
         }
      }
   }
   update(); // schedule repaint on the GUI thread
}

void SpectrumWidget::setDbRange(float minDb, float maxDb)
{
   _minDb = minDb;
   _maxDb = maxDb;
   _viewMinDb = static_cast<double>(minDb);
   _viewMaxDb = static_cast<double>(maxDb);
   syncColorBar();
   update();
}

void SpectrumWidget::setInputIsDb(bool isDb)
{
   _inputIsDb = isDb;
   update();
}

void SpectrumWidget::setColorMap(ColorMap::Palette palette)
{
   _colorMap = ColorMap(palette);
   syncColorBar();
   update();
}

void SpectrumWidget::setGridLines(int count)
{
   _gridLines = count;
   update();
}

void SpectrumWidget::resetView()
{
   _viewMinDb  = static_cast<double>(_minDb);
   _viewMaxDb  = static_cast<double>(_maxDb);
   _viewXStart = 0.0;
   _viewXEnd   = 1.0;
   syncColorBar();
   emit xViewChanged(_viewXStart, _viewXEnd);
   update();
}

void SpectrumWidget::setColorBarVisible(bool visible)
{
   _colorBar->setVisible(visible);
   update();
}

void SpectrumWidget::setXAxisVisible(bool visible)
{
   _xAxisVisible = visible;
   _cursorOverlay.setMargins(MARGIN_LEFT, MARGIN_RIGHT, MARGIN_TOP,
                             visible ? MARGIN_BOTTOM : MARGIN_BOTTOM_HIDDEN);
   _cursorOverlay.setShowXLabels(visible);
   update();
}

void SpectrumWidget::setMaxHoldEnabled(bool enabled)
{
   _maxHoldEnabled = enabled;
   if (!enabled)
   {
      const std::lock_guard<std::mutex> lock(_mutex);
      _maxHoldData.clear();
   }
   update();
}

void SpectrumWidget::setMaxHoldDecayRate(float dbPerSecond)
{
   _maxHoldDecayRate = dbPerSecond;
}

QRect SpectrumWidget::plotArea() const
{
   const int bot = _xAxisVisible ? MARGIN_BOTTOM : MARGIN_BOTTOM_HIDDEN;
   return {MARGIN_LEFT, MARGIN_TOP,
           width() - MARGIN_LEFT - MARGIN_RIGHT,
           height() - MARGIN_TOP - bot};
}

// ============================================================================
// Paint
// ============================================================================

void SpectrumWidget::resizeEvent(QResizeEvent* event)
{
   QWidget::resizeEvent(event);
   const QRect area = plotArea();
   _colorBar->setGeometry(
      area.right() + 5,
      area.top(),
      COLOR_BAR_WIDTH,
      area.height());
}

void SpectrumWidget::paintEvent(QPaintEvent* /*event*/)
{
   QPainter painter(this);
   painter.setRenderHint(QPainter::Antialiasing, false);

   const QRect area = plotArea();

   drawBackground(painter, area);
   drawGrid(painter, area);

   // Clip spectrum to the plot area so zoomed/panned data
   // does not overflow into the label margins.
   painter.save();
   painter.setClipRect(area);
   drawSpectrum(painter, area);
   drawMaxHold(painter, area);
   painter.restore();

   drawLabels(painter, area);
   drawFps(painter, area);
   _cursorOverlay.draw(painter, area);

   // Update FPS counter.
   ++_fpsFrameCount;
   const auto now = std::chrono::steady_clock::now();
   const auto elapsed = std::chrono::duration<float>(now - _fpsLastTime).count();
   if (elapsed >= 1.0F)
   {
      _fpsDisplay = static_cast<float>(_fpsFrameCount) / elapsed;
      _fpsFrameCount = 0;
      _fpsLastTime = now;
   }
}

// ============================================================================
// Drawing helpers
// ============================================================================

void SpectrumWidget::drawBackground(QPainter& painter, const QRect& area) const
{
   painter.fillRect(rect(), QColor(25, 25, 30));
   painter.fillRect(area, QColor(15, 15, 20));
}

void SpectrumWidget::drawGrid(QPainter& painter, const QRect& area) const
{
   painter.setPen(QPen(QColor(60, 60, 70), 1, Qt::DotLine));

   // Horizontal grid lines (amplitude)
   for (int i = 0; i <= _gridLines; ++i)
   {
      const float frac = static_cast<float>(i) / static_cast<float>(_gridLines);
      const int yPos = area.top() + static_cast<int>(frac * static_cast<float>(area.height()));
      painter.drawLine(area.left(), yPos, area.right(), yPos);
   }

   // Vertical grid lines (bins / frequency)
   constexpr int V_LINES = 8;
   for (int i = 0; i <= V_LINES; ++i)
   {
      const float frac = static_cast<float>(i) / static_cast<float>(V_LINES);
      const int xPos = area.left() + static_cast<int>(frac * static_cast<float>(area.width()));
      painter.drawLine(xPos, area.top(), xPos, area.bottom());
   }
}

void SpectrumWidget::drawSpectrum(QPainter& painter, const QRect& area) const
{
   std::vector<float> snapshot;
   {
      const std::lock_guard<std::mutex> lock(_mutex);
      snapshot = _data;
   }

   if (snapshot.empty())
   {
      return;
   }

   auto binCount = static_cast<int>(snapshot.size());
   double viewWidth = _viewXEnd - _viewXStart;
   if (viewWidth <= 0.0)
   {
      viewWidth = 1.0;
   }

   // Determine visible bin range (with 1-bin margin for line continuity)
   const int firstBin = std::max(0,
      static_cast<int>(std::floor(_viewXStart * static_cast<double>(binCount))) - 1);
   const int lastBin = std::min(binCount - 1,
      static_cast<int>(std::ceil(_viewXEnd * static_cast<double>(binCount))));

   // Pre-compute normalised values and positions for visible bins
   std::vector<float> norms(snapshot.size());
   std::vector<float> xPts(snapshot.size());
   std::vector<float> yPts(snapshot.size());

   for (int i = firstBin; i <= lastBin; ++i)
   {
      auto si = static_cast<std::size_t>(i);
      const float norm = toNormalised(snapshot[si]);
      norms[si] = norm;

      const double binFrac = (static_cast<double>(i) + 0.5) / static_cast<double>(binCount);
      const double screenFrac = (binFrac - _viewXStart) / viewWidth;

      xPts[si] = static_cast<float>(area.left()) +
                 (static_cast<float>(screenFrac) * static_cast<float>(area.width()));
      yPts[si] = static_cast<float>(area.bottom()) -
                 (norm * static_cast<float>(area.height()));
   }

   // Build the fill path
   QPainterPath fillPath;
   fillPath.moveTo(static_cast<double>(xPts[static_cast<std::size_t>(firstBin)]),
                   static_cast<double>(area.bottom()));
   for (int i = firstBin; i <= lastBin; ++i)
   {
      fillPath.lineTo(static_cast<double>(xPts[static_cast<std::size_t>(i)]),
                      static_cast<double>(yPts[static_cast<std::size_t>(i)]));
   }
   fillPath.lineTo(static_cast<double>(xPts[static_cast<std::size_t>(lastBin)]),
                   static_cast<double>(area.bottom()));
   fillPath.closeSubpath();

   // Draw gradient fill under the curve matching the color bar
   QLinearGradient gradient(0, area.top(), 0, area.bottom());
   constexpr int GRADIENT_STOPS = 16;
   for (int s = 0; s <= GRADIENT_STOPS; ++s)
   {
      const float frac = static_cast<float>(s) / static_cast<float>(GRADIENT_STOPS);
      // frac 0 = top of plot = high value, frac 1 = bottom = low value
      const float norm = 1.0F - frac;
      const Color c = _colorMap.map(norm);
      gradient.setColorAt(static_cast<double>(frac),
                          QColor(c.r, c.g, c.b, 70));
   }

   painter.setRenderHint(QPainter::Antialiasing, true);
   painter.fillPath(fillPath, gradient);

   // Draw per-segment colored line on top — each segment uses the
   // average normalised value of its two endpoints to pick a color.
   for (int i = firstBin + 1; i <= lastBin; ++i)
   {
      auto si = static_cast<std::size_t>(i);
      const float avgNorm = (norms[si - 1] + norms[si]) * 0.5F;
      const Color c = _colorMap.map(avgNorm);
      painter.setPen(QPen(QColor(c.r, c.g, c.b, 220), 1.5));
      painter.drawLine(QPointF(static_cast<double>(xPts[si - 1]),
                               static_cast<double>(yPts[si - 1])),
                       QPointF(static_cast<double>(xPts[si]),
                               static_cast<double>(yPts[si])));
   }
   painter.setRenderHint(QPainter::Antialiasing, false);
}

void SpectrumWidget::drawMaxHold(QPainter& painter, const QRect& area) const
{
   if (!_maxHoldEnabled)
   {
      return;
   }

   std::vector<float> holdSnapshot;
   {
      const std::lock_guard<std::mutex> lock(_mutex);
      holdSnapshot = _maxHoldData;
   }

   if (holdSnapshot.empty())
   {
      return;
   }

   auto binCount = static_cast<int>(holdSnapshot.size());
   double viewWidth = _viewXEnd - _viewXStart;
   if (viewWidth <= 0.0)
   {
      viewWidth = 1.0;
   }

   // Use int for firstBin/lastBin to avoid UB when the subtraction goes negative.
   const int firstBin = std::max(0,
      static_cast<int>(std::floor(_viewXStart * static_cast<double>(binCount))) - 1);
   const int lastBin = std::min(binCount - 1,
      static_cast<int>(std::ceil(_viewXEnd * static_cast<double>(binCount))));

   if (firstBin >= lastBin)
   {
      return;
   }

   // Compute screen positions for max-hold bins
   std::vector<QPointF> pts;
   pts.reserve(static_cast<std::size_t>(lastBin - firstBin) + 1);

   for (int i = firstBin; i <= lastBin; ++i)
   {
      auto si = static_cast<std::size_t>(i);
      const float norm = toNormalised(holdSnapshot[si]);

      const double binFrac = (static_cast<double>(i) + 0.5) / static_cast<double>(binCount);
      const double screenFrac = (binFrac - _viewXStart) / viewWidth;

      const double xPos = static_cast<double>(area.left()) +
                          (screenFrac * static_cast<double>(area.width()));
      const double yPos = static_cast<double>(area.bottom()) -
                          (static_cast<double>(norm) * static_cast<double>(area.height()));
      pts.emplace_back(xPos, yPos);
   }

   // Draw white max-hold line
   painter.setRenderHint(QPainter::Antialiasing, true);
   painter.setPen(QPen(QColor(255, 255, 255, 200), 1.0));
   for (std::size_t i = 1; i < pts.size(); ++i)
   {
      painter.drawLine(pts[i - 1], pts[i]);
   }
   painter.setRenderHint(QPainter::Antialiasing, false);
}

void SpectrumWidget::drawFps(QPainter& painter, const QRect& area) const
{
   QFont font = painter.font();
   font.setPointSize(9);
   painter.setFont(font);
   painter.setPen(QColor(160, 160, 170));

   const QString fpsText = QString::number(static_cast<int>(_fpsDisplay)) + " FPS";
   constexpr int PADDING = 4;
   const QRect fpsRect(area.right() - 60 - PADDING, area.top() + PADDING,
                       60, 16);
   painter.drawText(fpsRect, Qt::AlignRight | Qt::AlignVCenter, fpsText);
}

void SpectrumWidget::drawLabels(QPainter& painter, const QRect& area) const
{
   painter.setPen(QColor(180, 180, 190));
   QFont font = painter.font();
   font.setPointSize(10);
   painter.setFont(font);

   // Y-axis dB labels + tick marks
   for (int i = 0; i <= _gridLines; ++i)
   {
      const auto frac = static_cast<float>(i) / static_cast<float>(_gridLines);
      const auto db = static_cast<float>(_viewMaxDb -
                (static_cast<double>(frac) * (_viewMaxDb - _viewMinDb)));
      const int yPos = area.top() + static_cast<int>(frac * static_cast<float>(area.height()));

      // Tick mark on left edge of plot
      painter.setPen(QColor(180, 180, 190));
      painter.drawLine(area.left() - TICK_LENGTH, yPos, area.left(), yPos);

      const QString label = QString::number(static_cast<int>(db)) + " dB";
      painter.drawText(0, yPos - 6, MARGIN_LEFT - TICK_LENGTH - 2, 12,
                       Qt::AlignRight | Qt::AlignVCenter, label);
   }

   // X-axis: frequency tick labels + tick marks
   if (!_xAxisVisible)
   {
      return;
   }

   constexpr int X_TICKS = 8;
   const bool hasFreq = (_bandwidthHz > 0.0);
   const double startFreq = _centerFreqHz - (_bandwidthHz / 2.0);

   for (int i = 0; i <= X_TICKS; ++i)
   {
      const float frac = static_cast<float>(i) / static_cast<float>(X_TICKS);
      const int xPos = area.left() + static_cast<int>(frac * static_cast<float>(area.width()));

      // Tick mark on bottom edge of plot
      painter.setPen(QColor(180, 180, 190));
      painter.drawLine(xPos, area.bottom(), xPos, area.bottom() + TICK_LENGTH);

      QString label;
      if (hasFreq)
      {
         const double dataFrac = _viewXStart +
                           (static_cast<double>(frac) * (_viewXEnd - _viewXStart));
         const double freq = startFreq + (dataFrac * _bandwidthHz);
         label = QString::fromStdString(formatFrequency(freq));
      }
      else
      {
         const double dataFrac = _viewXStart +
                           (static_cast<double>(frac) * (_viewXEnd - _viewXStart));
         label = QString::number(dataFrac, 'f', 2);
      }

      // Centre the label on the tick position
      constexpr int LABEL_WIDTH = 80;
      const int labelTop = area.bottom() + TICK_LENGTH + 3;
      painter.drawText(xPos - (LABEL_WIDTH / 2), labelTop,
                       LABEL_WIDTH, MARGIN_BOTTOM - TICK_LENGTH - 6,
                       Qt::AlignCenter, label);
   }
}

// ============================================================================
// Mouse interaction
// ============================================================================

void SpectrumWidget::wheelEvent(QWheelEvent* event)
{
   const QRect area = plotArea();
   const QPointF pos = event->position();

   const bool inPlot   = area.contains(pos.toPoint());
   const bool inYMargin = ((pos.x() < area.left() || pos.x() > area.right()) &&
                            pos.y() >= area.top() && pos.y() <= area.bottom());
   const bool inXMargin = (pos.y() > area.bottom() &&
                           pos.x() >= area.left() && pos.x() <= area.right());

   if (!inPlot && !inYMargin && !inXMargin)
   {
      QWidget::wheelEvent(event);
      return;
   }

   // Bandwidth cursor: wheel adjusts the half-width when in the plot area.
   if (inPlot && processBandwidthWheel(event->angleDelta().y()))
   {
      event->accept();
      return;
   }

   constexpr double ZOOM_FACTOR = 1.15;
   const double factor = (event->angleDelta().y() > 0)
                           ? (1.0 / ZOOM_FACTOR)
                           : ZOOM_FACTOR;

   const bool zoomY = inPlot || inYMargin;
   const bool zoomX = inPlot || inXMargin;

   if (zoomY)
   {
      // Y-axis zoom centred on the dB value under the mouse
      double yFrac = (pos.y() - area.top()) / area.height();
      yFrac = std::clamp(yFrac, 0.0, 1.0);
      const double dbAtMouse = _viewMaxDb - (yFrac * (_viewMaxDb - _viewMinDb));
      _viewMinDb = dbAtMouse - ((dbAtMouse - _viewMinDb) * factor);
      _viewMaxDb = dbAtMouse + ((_viewMaxDb - dbAtMouse) * factor);
   }

   if (zoomX)
   {
      // X-axis zoom centred on the data fraction under the mouse
      double xFrac = (pos.x() - area.left()) / area.width();
      xFrac = std::clamp(xFrac, 0.0, 1.0);
      const double dataFracAtMouse = _viewXStart + (xFrac * (_viewXEnd - _viewXStart));
      const double newXStart = dataFracAtMouse - ((dataFracAtMouse - _viewXStart) * factor);
      const double newXEnd   = dataFracAtMouse + ((_viewXEnd - dataFracAtMouse) * factor);
      _viewXStart = std::max(0.0, newXStart);
      _viewXEnd   = std::min(1.0, newXEnd);
   }

   syncColorBar();
   emit xViewChanged(_viewXStart, _viewXEnd);
   update();
   event->accept();
}

void SpectrumWidget::mousePressEvent(QMouseEvent* event)
{
   if (event->button() == Qt::MiddleButton)
   {
      processMiddleButtonPress();
      event->accept();
      return;
   }

   if (event->button() == Qt::LeftButton)
   {
      // Bandwidth cursor: left-click toggles lock.
      if (processBandwidthClick(event->pos()))
      {
         event->accept();
         return;
      }

      const QRect area = plotArea();
      const QPoint pos = event->pos();

      const bool inPlot   = area.contains(pos);
      const bool inYMargin = ((pos.x() < area.left() || pos.x() > area.right()) &&
                               pos.y() >= area.top() && pos.y() <= area.bottom());
      const bool inXMargin = (pos.y() > area.bottom() &&
                              pos.x() >= area.left() && pos.x() <= area.right());

      if (inPlot || inYMargin || inXMargin)
      {
         _panning        = true;
         _panStartPos    = pos;
         _panStartMinDb  = _viewMinDb;
         _panStartMaxDb  = _viewMaxDb;
         _panStartXStart = _viewXStart;
         _panStartXEnd   = _viewXEnd;

         if (inYMargin)
         {
            _panAxis = PanAxis::YOnly;
         }
         else if (inXMargin)
         {
            _panAxis = PanAxis::XOnly;
         }
         else
         {
            _panAxis = PanAxis::Both;
         }

         setCursor(Qt::ClosedHandCursor);
         event->accept();
         return;
      }
   }
   QWidget::mousePressEvent(event);
}

void SpectrumWidget::mouseMoveEvent(QMouseEvent* event)
{
   if (_panning)
   {
      const QRect area = plotArea();
      const double dxPixels = event->pos().x() - _panStartPos.x();
      const double dyPixels = event->pos().y() - _panStartPos.y();

      const bool panX = (_panAxis == PanAxis::Both || _panAxis == PanAxis::XOnly);
      const bool panY = (_panAxis == PanAxis::Both || _panAxis == PanAxis::YOnly);

      if (panX)
      {
         // X pan: drag right -> content moves right -> view shifts left
         const double xRange = _panStartXEnd - _panStartXStart;
         const double dxData = -dxPixels / area.width() * xRange;
         double newXStart = _panStartXStart + dxData;
         double newXEnd   = _panStartXEnd   + dxData;

         if (newXStart < 0.0)
         {
            newXEnd  -= newXStart;
            newXStart = 0.0;
         }
         if (newXEnd > 1.0)
         {
            newXStart -= (newXEnd - 1.0);
            newXEnd    = 1.0;
         }
         _viewXStart = std::max(0.0, newXStart);
         _viewXEnd   = std::min(1.0, newXEnd);
      }

      if (panY)
      {
         // Y pan: drag down -> content moves down -> view shifts up in dB
         const double dbRange = _panStartMaxDb - _panStartMinDb;
         const double dyDb    = dyPixels / area.height() * dbRange;
         _viewMinDb = _panStartMinDb + dyDb;
         _viewMaxDb = _panStartMaxDb + dyDb;
      }

      syncColorBar();
      emit xViewChanged(_viewXStart, _viewXEnd);
      update();
      event->accept();
   }
   else
   {
      processCursorMove(event->pos());
      QWidget::mouseMoveEvent(event);
   }
}

void SpectrumWidget::mouseReleaseEvent(QMouseEvent* event)
{
   if (event->button() == Qt::LeftButton && _panning)
   {
      _panning = false;
      setCursor(Qt::ArrowCursor);
      event->accept();
   }
   else
   {
      QWidget::mouseReleaseEvent(event);
   }
}

void SpectrumWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
   if (processMeasCursorDoubleClick(event))
   {
      event->accept();
      return;
   }

   if (event->button() == Qt::MiddleButton)
   {
      resetView();
      event->accept();
   }
   else
   {
      QWidget::mouseDoubleClickEvent(event);
   }
}

void SpectrumWidget::leaveEvent(QEvent* event)
{
   processLeaveEvent(event);
}

// ============================================================================
// Utilities
// ============================================================================

void SpectrumWidget::syncColorBar()
{
   _colorBar->setDbRange(static_cast<float>(_viewMinDb),
                         static_cast<float>(_viewMaxDb));
   _colorBar->setColorMap(_colorMap);
}

void SpectrumWidget::onViewRangeChanged()
{
   syncColorBar();
}

float SpectrumWidget::toNormalised(float value) const
{
   float db = value;
   if (!_inputIsDb)
   {
      // Convert linear magnitude to dB
      constexpr float EPSILON = 1.0e-10F;
      db = 20.0F * std::log10(std::max(value, EPSILON));
   }

   // Map dB range to [0, 1] using the current view range
   auto viewMin = static_cast<float>(_viewMinDb);
   auto viewMax = static_cast<float>(_viewMaxDb);
   const float norm = (db - viewMin) / (viewMax - viewMin);
   return std::clamp(norm, 0.0F, 1.0F);
}

} // namespace RealTimeGraphs
