#include "WaterfallWidget.h"

#include "ColorBarWidget.h"
#include "CommonGuiUtils.h"

#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace RealTimeGraphs
{

// ============================================================================
// Construction
// ============================================================================

WaterfallWidget::WaterfallWidget(int historyRows, QWidget* parent)
   : QWidget(parent)
   , _historyRows{historyRows}
   , _rows(static_cast<std::size_t>(historyRows))
   , _timestamps(static_cast<std::size_t>(historyRows))
{
   setAttribute(Qt::WA_OpaquePaintEvent);
   setMinimumSize(320, 200);
   setMouseTracking(true);

   _colorBar = new ColorBarStrip(this);
   _colorBar->setDbRange(_minDb, _maxDb);
   _colorBar->setColorMap(_colorMap);

   // Configure cursor overlay
   _cursorOverlay.setMargins(MARGIN_LEFT, MARGIN_RIGHT, MARGIN_TOP, MARGIN_BOTTOM);

   _cursorOverlay.setPixelToData(
      [this](const QPoint& pos, const QRect& area) -> PlotCursorOverlay::DataPoint
      {
         // X = screen fraction mapped through view range to data fraction
         const double xFrac = static_cast<double>(pos.x() - area.left())
                            / static_cast<double>(area.width());
         const double dataX = _viewXStart + (std::clamp(xFrac, 0.0, 1.0)
                            * (_viewXEnd - _viewXStart));
         // Y = row fraction (0 = top/newest, 1 = bottom/oldest)
         const double yFrac = static_cast<double>(pos.y() - area.top())
                            / static_cast<double>(area.height());
         return {std::clamp(dataX, 0.0, 1.0), std::clamp(yFrac, 0.0, 1.0)};
      });

   _cursorOverlay.setDataToPixel(
      [this](const PlotCursorOverlay::DataPoint& dp,
             const QRect& area) -> QPoint
      {
         const double viewWidth = _viewXEnd - _viewXStart;
         const double xFrac = (viewWidth > 0.0)
                            ? (dp.x - _viewXStart) / viewWidth : 0.0;
         const int px = area.left() + static_cast<int>(xFrac * area.width());
         const int py = area.top()  + static_cast<int>(dp.y * area.height());
         return {px, py};
      });

   _cursorOverlay.setFormatX(
      [this](double xVal) -> QString { return formatXValue(xVal); });

   _cursorOverlay.setFormatY(
      [this](double yVal) -> QString
      {
         // yVal is row fraction: 0 = newest, 1 = oldest
         // Map to a row index and get its timestamp
         const std::lock_guard<std::mutex> lock(_mutex);
         const auto rowCount = static_cast<int>(_rows.size());
         if (rowCount == 0)
         {
            return QString();
         }
         // frac 0 (top) -> newest = index rowCount-1
         // frac 1 (bottom) -> oldest = index 0
         const auto rowIdx = static_cast<std::size_t>(
            std::clamp(static_cast<int>(std::lround(
                          (1.0 - yVal) * static_cast<double>(rowCount - 1))),
                       0, rowCount - 1));
         const auto& ts = _timestamps[rowIdx];
         const auto now = std::chrono::steady_clock::now();
         const double ageSec = std::chrono::duration<double>(now - ts).count();
         return QString::fromStdString(formatAge(ageSec));
      });

   _cursorOverlay.setFormatDeltaX(
      [this](double x1, double x2) -> QString
      {
         if (_bandwidthHz > 0.0)
         {
            const double startFreq = _centerFreqHz - (_bandwidthHz / 2.0);
            const double freq1 = startFreq + (x1 * _bandwidthHz);
            const double freq2 = startFreq + (x2 * _bandwidthHz);
            const double deltaFreq = freq1 - freq2;
            const QString sign = (deltaFreq < 0.0) ? "-" : "";
            return QString::fromUtf8("\u0394f: ") + sign
                 + QString::fromStdString(formatFrequency(std::abs(deltaFreq)));
         }
         const double dx = x1 - x2;
         return QString::fromUtf8("\u0394x: ") + QString::number(dx, 'f', 4);
      });

   _cursorOverlay.setFormatDeltaY(
      [this](double y1, double y2) -> QString
      {
         // Compute time delta between the two row fractions
         const std::lock_guard<std::mutex> lock(_mutex);
         const auto rowCount = static_cast<int>(_rows.size());
         if (rowCount == 0)
         {
            return QString();
         }
         auto toRowIdx = [&](double frac) -> std::size_t
         {
            return static_cast<std::size_t>(
               std::clamp(static_cast<int>(std::lround(
                             (1.0 - frac) * static_cast<double>(rowCount - 1))),
                          0, rowCount - 1));
         };
         const auto& ts1 = _timestamps[toRowIdx(y1)];
         const auto& ts2 = _timestamps[toRowIdx(y2)];
         const double deltaSec = std::chrono::duration<double>(ts1 - ts2).count();
         const QString sign = (deltaSec < 0.0) ? "-" : "";
         return QString::fromUtf8("\u0394t: ") + sign
              + QString::fromStdString(formatAge(std::abs(deltaSec)));
      });
}

// ============================================================================
// Public API
// ============================================================================

void WaterfallWidget::addRow(const std::vector<float>& magnitudes)
{
   // Normalise the incoming row
   std::vector<float> normRow;
   normRow.reserve(magnitudes.size());
   for (const float val : magnitudes)
   {
      normRow.push_back(toNormalised(val));
   }

   {
      const std::lock_guard<std::mutex> lock(_mutex);
      _binCount = static_cast<int>(magnitudes.size());
      _rows.push(normRow);
      _timestamps.push(std::chrono::steady_clock::now());
   }
   update();
}

void WaterfallWidget::setDbRange(float minDb, float maxDb)
{
   _minDb = minDb;
   _maxDb = maxDb;
   _colorBar->setDbRange(minDb, maxDb);
   update();
}

void WaterfallWidget::setInputIsDb(bool isDb)
{
   _inputIsDb = isDb;
   update();
}

void WaterfallWidget::setColorMap(ColorMap::Palette palette)
{
   _colorMap = ColorMap(palette);
   _colorBar->setColorMap(_colorMap);
   update();
}

void WaterfallWidget::setFrequencyRange(double centerFreqHz, double bandwidthHz)
{
   _centerFreqHz = centerFreqHz;
   _bandwidthHz  = bandwidthHz;
   _viewXStart   = 0.0;
   _viewXEnd     = 1.0;
   emit xViewChanged(_viewXStart, _viewXEnd);
   update();
}

QSize WaterfallWidget::minimumSizeHint() const
{
   return {320, 200};
}

QRect WaterfallWidget::plotArea() const
{
   return {MARGIN_LEFT, MARGIN_TOP,
           width() - MARGIN_LEFT - MARGIN_RIGHT,
           height() - MARGIN_TOP - MARGIN_BOTTOM};
}

QString WaterfallWidget::formatXValue(double dataFrac) const
{
   if (_bandwidthHz > 0.0)
   {
      const double startFreq = _centerFreqHz - (_bandwidthHz / 2.0);
      const double freq = startFreq + (dataFrac * _bandwidthHz);
      return QString::fromStdString(formatFrequency(freq));
   }
   return QString::number(dataFrac, 'f', 3);
}

void WaterfallWidget::setColorBarVisible(bool visible)
{
   _colorBar->setVisible(visible);
   update();
}

void WaterfallWidget::setXViewRange(double xStart, double xEnd)
{
   if (_viewXStart == xStart && _viewXEnd == xEnd)
   {
      return;
   }
   _viewXStart = xStart;
   _viewXEnd   = xEnd;
   update();
}

// ============================================================================
// Events
// ============================================================================

void WaterfallWidget::paintEvent(QPaintEvent* /*event*/)
{
   QPainter painter(this);
   painter.setRenderHint(QPainter::Antialiasing, false);

   const QRect pArea = plotArea();

   // Background
   painter.fillRect(rect(), QColor(25, 25, 30));

   // Build and draw the spectrogram image
   rebuildImage();

   if (!_image.isNull())
   {
      // Draw only the visible portion of the full image
      const int srcX = static_cast<int>(_viewXStart * _image.width());
      const int srcW = static_cast<int>((_viewXEnd - _viewXStart) * _image.width());
      const QRect srcRect(srcX, 0, std::max(1, srcW), _image.height());
      painter.drawImage(pArea, _image, srcRect);
   }
   else
   {
      painter.fillRect(pArea, QColor(15, 15, 20));
   }

   // Draw border around plot area
   painter.setPen(QColor(60, 60, 70));
   painter.drawRect(pArea);

   // Labels
   painter.setPen(QColor(180, 180, 190));
   QFont font = painter.font();
   font.setPointSize(10);
   painter.setFont(font);

   // Y-axis: time labels
   drawTimeLabels(painter, pArea);

   // X-axis: frequency tick labels
   drawFrequencyLabels(painter, pArea);

   // Cursor overlay
   _cursorOverlay.draw(painter, pArea);
}

void WaterfallWidget::resizeEvent(QResizeEvent* event)
{
   QWidget::resizeEvent(event);

   const QRect area = plotArea();
   _colorBar->setGeometry(
      area.right() + 5,
      area.top(),
      COLOR_BAR_WIDTH,
      area.height());

   update();
}

void WaterfallWidget::wheelEvent(QWheelEvent* event)
{
   const QRect area = plotArea();
   const QPointF pos = event->position();

   const bool inPlot   = area.contains(pos.toPoint());
   const bool inXMargin = (pos.y() > area.bottom() &&
                           pos.x() >= area.left() && pos.x() <= area.right());

   if (!inPlot && !inXMargin)
   {
      QWidget::wheelEvent(event);
      return;
   }

   constexpr double ZOOM_FACTOR = 1.15;
   const double factor = (event->angleDelta().y() > 0)
                           ? (1.0 / ZOOM_FACTOR)
                           : ZOOM_FACTOR;

   // X-axis zoom centred on the data fraction under the mouse
   double xFrac = (pos.x() - area.left()) / area.width();
   xFrac = std::clamp(xFrac, 0.0, 1.0);
   const double dataFracAtMouse = _viewXStart + (xFrac * (_viewXEnd - _viewXStart));
   const double newXStart = dataFracAtMouse - ((dataFracAtMouse - _viewXStart) * factor);
   const double newXEnd   = dataFracAtMouse + ((_viewXEnd - dataFracAtMouse) * factor);
   _viewXStart = std::max(0.0, newXStart);
   _viewXEnd   = std::min(1.0, newXEnd);

   emit xViewChanged(_viewXStart, _viewXEnd);
   update();
   event->accept();
}

void WaterfallWidget::mousePressEvent(QMouseEvent* event)
{
   if (event->button() == Qt::MiddleButton)
   {
      if (_cursorOverlay.clearCursors())
      {
         emitMeasCursorsChanged();
         update();
      }
      emit requestPeerCursorClear();
      event->accept();
      return;
   }

   if (event->button() == Qt::LeftButton)
   {
      const QRect area = plotArea();
      const QPoint pos = event->pos();
      const bool inPlot   = area.contains(pos);
      const bool inXMargin = (pos.y() > area.bottom() &&
                              pos.x() >= area.left() && pos.x() <= area.right());
      if (inPlot || inXMargin)
      {
         _panning        = true;
         _panStartPos    = pos;
         _panStartXStart = _viewXStart;
         _panStartXEnd   = _viewXEnd;
         setCursor(Qt::ClosedHandCursor);
         event->accept();
         return;
      }
   }
   QWidget::mousePressEvent(event);
}

void WaterfallWidget::mouseMoveEvent(QMouseEvent* event)
{
   if (_panning)
   {
      const QRect area = plotArea();
      const double dxPixels = event->pos().x() - _panStartPos.x();
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
      emit xViewChanged(_viewXStart, _viewXEnd);
      update();
      event->accept();
   }
   else
   {
      const QRect area = plotArea();
      if (_cursorOverlay.handleMouseMove(event->pos(), area))
      {
         // Emit tracking X in data-space
         const double xFrac = static_cast<double>(event->pos().x() - area.left())
                            / static_cast<double>(area.width());
         const double dataX = _viewXStart +
                            (std::clamp(xFrac, 0.0, 1.0) * (_viewXEnd - _viewXStart));
         emit trackingCursorXChanged(dataX);
         update();
      }
      QWidget::mouseMoveEvent(event);
   }
}

void WaterfallWidget::mouseReleaseEvent(QMouseEvent* event)
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

void WaterfallWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
   const QRect area = plotArea();

   if (event->button() == Qt::LeftButton)
   {
      if (_cursorOverlay.placeCursor1(event->pos(), area))
      {
         emit requestPeerCursorClear();
         emitMeasCursorsChanged();
         update();
      }
      event->accept();
   }
   else if (event->button() == Qt::RightButton)
   {
      if (_cursorOverlay.placeCursor2(event->pos(), area))
      {
         emit requestPeerCursorClear();
         emitMeasCursorsChanged();
         update();
      }
      event->accept();
   }
   else if (event->button() == Qt::MiddleButton)
   {
      // Reset X view to full range
      _viewXStart = 0.0;
      _viewXEnd   = 1.0;
      emit xViewChanged(_viewXStart, _viewXEnd);
      update();
      event->accept();
   }
   else
   {
      QWidget::mouseDoubleClickEvent(event);
   }
}

void WaterfallWidget::leaveEvent(QEvent* event)
{
   if (_cursorOverlay.handleLeave())
   {
      emit trackingCursorLeft();
      update();
   }
   QWidget::leaveEvent(event);
}

// ============================================================================
// Internals
// ============================================================================

void WaterfallWidget::rebuildImage()
{
   const std::lock_guard<std::mutex> lock(_mutex);

   auto rowCount = static_cast<int>(_rows.size());
   if (rowCount == 0 || _binCount == 0)
   {
      _image = QImage();
      return;
   }

   // Create an image with one pixel per bin horizontally, one per row vertically.
   // Most recent row is at top (row index 0 in image), oldest at bottom.
   _image = QImage(_binCount, rowCount, QImage::Format_RGBA8888);

   for (int r = 0; r < rowCount; ++r)
   {
      // Logical index: 0 = oldest.  We want newest at image top.
      const int logicalIdx = rowCount - 1 - r;
      const auto& row = _rows[static_cast<std::size_t>(logicalIdx)];

      auto* scanLine = reinterpret_cast<uint8_t*>(_image.scanLine(r));
      const int cols = std::min(_binCount, static_cast<int>(row.size()));

      for (int c = 0; c < cols; ++c)
      {
         const Color clr = _colorMap.map(row[static_cast<std::size_t>(c)]);
         const int offset = c * 4;
         scanLine[offset + 0] = clr.r;
         scanLine[offset + 1] = clr.g;
         scanLine[offset + 2] = clr.b;
         scanLine[offset + 3] = clr.a;
      }
   }
}

float WaterfallWidget::toNormalised(float value) const
{
   float db = value;
   if (!_inputIsDb)
   {
      constexpr float EPSILON = 1.0e-10F;
      db = 20.0F * std::log10(std::max(value, EPSILON));
   }

   const float norm = (db - _minDb) / (_maxDb - _minDb);
   return std::clamp(norm, 0.0F, 1.0F);
}

void WaterfallWidget::drawFrequencyLabels(QPainter& painter, const QRect& area) const
{
   painter.setPen(QColor(180, 180, 190));
   QFont font = painter.font();
   font.setPointSize(10);
   painter.setFont(font);

   constexpr int X_TICKS = 8;
   const bool hasFreq = (_bandwidthHz > 0.0);
   const double startFreq = _centerFreqHz - (_bandwidthHz / 2.0);

   for (int i = 0; i <= X_TICKS; ++i)
   {
      const float frac = static_cast<float>(i) / static_cast<float>(X_TICKS);
      const int xPos = area.left() + static_cast<int>(frac * static_cast<float>(area.width()));

      // Map screen fraction through view range to data fraction
      const double dataFrac = _viewXStart +
                        (static_cast<double>(frac) * (_viewXEnd - _viewXStart));

      QString label;
      if (hasFreq)
      {
         const double freq = startFreq + (dataFrac * _bandwidthHz);
         label = QString::fromStdString(formatFrequency(freq));
      }
      else
      {
         label = QString::number(dataFrac, 'f', 2);
      }

      constexpr int LABEL_WIDTH = 80;
      painter.drawText(xPos - (LABEL_WIDTH / 2), area.bottom() + 3,
                       LABEL_WIDTH, MARGIN_BOTTOM - 5,
                       Qt::AlignCenter, label);
   }
}

void WaterfallWidget::drawTimeLabels(QPainter& painter, const QRect& area) const
{
   painter.setPen(QColor(180, 180, 190));
   QFont font = painter.font();
   font.setPointSize(10);
   painter.setFont(font);

   const std::lock_guard<std::mutex> lock(_mutex);

   const auto rowCount = static_cast<int>(_rows.size());
   if (rowCount == 0)
   {
      return;
   }

   constexpr int Y_TICKS = 6;
   const auto now = std::chrono::steady_clock::now();

   // Newest row is logical index (rowCount - 1), oldest is index 0.
   for (int i = 0; i <= Y_TICKS; ++i)
   {
      const float frac = static_cast<float>(i) / static_cast<float>(Y_TICKS);
      // frac 0 = top of plot (newest), frac 1 = bottom (oldest)
      const int yPos = area.top() + static_cast<int>(frac * static_cast<float>(area.height()));

      // Interpolate which logical row this tick corresponds to
      // frac 0 -> newest (index rowCount-1), frac 1 -> oldest (index 0)
      const auto rowIdx = static_cast<std::size_t>(
         std::clamp(static_cast<int>(std::lround(
                       (1.0F - frac) * static_cast<float>(rowCount - 1))),
                    0, rowCount - 1));
      const auto& ts = _timestamps[rowIdx];

      const double ageSec = std::chrono::duration<double>(now - ts).count();
      const QString label = QString::fromStdString(formatAge(ageSec));

      constexpr int LABEL_HEIGHT = 12;
      painter.drawText(0, yPos - (LABEL_HEIGHT / 2), MARGIN_LEFT - 5, LABEL_HEIGHT,
                       Qt::AlignRight | Qt::AlignVCenter, label);
   }
}

std::string WaterfallWidget::formatAge(double seconds)
{
   if (seconds < 0.001)
   {
      return "now";
   }
   if (seconds < 1.0)
   {
      const int ms = static_cast<int>(std::lround(seconds * 1000.0));
      return std::to_string(ms) + " ms";
   }
   if (seconds < 60.0)
   {
      // Show one decimal place for < 10s, integer for >= 10s
      if (seconds < 10.0)
      {
         std::array<char, 16> buf{};
         (void)std::snprintf(buf.data(), buf.size(), "%.1f s", seconds);
         return buf.data();
      }
      return std::to_string(static_cast<int>(std::lround(seconds))) + " s";
   }
   if (seconds < 3600.0)
   {
      const int mins = static_cast<int>(seconds / 60.0);
      const int secs = static_cast<int>(seconds) % 60;
      return std::to_string(mins) + "m " + std::to_string(secs) + "s";
   }
   const int hours = static_cast<int>(seconds / 3600.0);
   const int mins  = (static_cast<int>(seconds) % 3600) / 60;
   return std::to_string(hours) + "h " + std::to_string(mins) + "m";
}

void WaterfallWidget::setLinkedCursorX(double xData)
{
   _cursorOverlay.setLinkedTrackingX(xData);
   update();
}

void WaterfallWidget::clearLinkedCursorX()
{
   _cursorOverlay.clearLinkedTrackingX();
   update();
}

void WaterfallWidget::setLinkedMeasCursors(double x1Valid, double x1,
                                           double x2Valid, double x2)
{
   std::optional<double> opt1;
   std::optional<double> opt2;
   if (x1Valid > 0.5)
   {
      opt1 = x1;
   }
   if (x2Valid > 0.5)
   {
      opt2 = x2;
   }
   _cursorOverlay.setLinkedMeasCursors(opt1, opt2);
   update();
}

void WaterfallWidget::emitMeasCursorsChanged()
{
   const auto& c1 = _cursorOverlay.measCursor1();
   const auto& c2 = _cursorOverlay.measCursor2();
   emit measCursorsChanged(
      c1.has_value() ? 1.0 : 0.0, c1.has_value() ? c1->x : 0.0,
      c2.has_value() ? 1.0 : 0.0, c2.has_value() ? c2->x : 0.0);
}

void WaterfallWidget::clearMeasCursors()
{
   if (_cursorOverlay.clearCursors())
   {
      emitMeasCursorsChanged();
      update();
   }
   _cursorOverlay.setLinkedMeasCursors(std::nullopt, std::nullopt);
   update();
}

} // namespace RealTimeGraphs
