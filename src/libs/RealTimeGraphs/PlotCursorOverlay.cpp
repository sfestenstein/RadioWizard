#include "PlotCursorOverlay.h"

#include <algorithm>

namespace RealTimeGraphs
{

// ============================================================================
// Configuration
// ============================================================================

void PlotCursorOverlay::setMargins(int left, int /*right*/, int /*top*/, int bottom)
{
   _marginLeft   = left;
   _marginBottom = bottom;
}

// ============================================================================
// Mouse handling
// ============================================================================

bool PlotCursorOverlay::handleMouseMove(const QPoint& pos, const QRect& plotArea)
{
   const bool inPlot = plotArea.contains(pos);

   // If the mouse is in this plot, any linked tracking line from the other
   // widget is stale — clear it to avoid ghosting.
   if (inPlot && _linkedTrackingX.has_value())
   {
      _linkedTrackingX.reset();
   }

   if (inPlot != _cursorInPlot || (inPlot && pos != _cursorPos))
   {
      _cursorInPlot = inPlot;
      _cursorPos    = pos;
      return true;
   }
   return false;
}

bool PlotCursorOverlay::handleLeave()
{
   if (_cursorInPlot)
   {
      _cursorInPlot = false;
      return true;
   }
   return false;
}

bool PlotCursorOverlay::placeCursor1(const QPoint& pos, const QRect& plotArea)
{
   if (!plotArea.contains(pos) || !_pixelToData)
   {
      return false;
   }
   _measCursor1 = _pixelToData(pos, plotArea);
   return true;
}

bool PlotCursorOverlay::placeCursor2(const QPoint& pos, const QRect& plotArea)
{
   if (!plotArea.contains(pos) || !_pixelToData)
   {
      return false;
   }
   _measCursor2 = _pixelToData(pos, plotArea);
   return true;
}

bool PlotCursorOverlay::clearCursors()
{
   if (_measCursor1.has_value() || _measCursor2.has_value())
   {
      _measCursor1.reset();
      _measCursor2.reset();
      return true;
   }
   return false;
}

// ============================================================================
// Drawing
// ============================================================================

void PlotCursorOverlay::draw(QPainter& painter, const QRect& plotArea) const
{
   drawBandwidthCursor(painter, plotArea);
   drawLinkedCursors(painter, plotArea);
   drawTrackingCrosshair(painter, plotArea);
   drawMeasurementCursors(painter, plotArea);
   drawDeltaReadout(painter, plotArea);
}

void PlotCursorOverlay::drawTrackingCrosshair(QPainter& painter,
                                               const QRect& area) const
{
   if (!_cursorInPlot)
   {
      return;
   }

   const int cx = _cursorPos.x();
   const int cy = _cursorPos.y();

   // Off-white dashed crosshair lines
   const QColor crossColor(220, 220, 215, 160);
   painter.setPen(QPen(crossColor, 1, Qt::DashLine));
   painter.drawLine(area.left(), cy, area.right(), cy);
   painter.drawLine(cx, area.top(), cx, area.bottom());

   if (!_pixelToData)
   {
      return;
   }

   const auto data = _pixelToData(_cursorPos, area);

   QFont font = painter.font();
   font.setPointSize(10);
   painter.setFont(font);

   const QColor labelBg(30, 30, 35, 200);
   const QColor labelFg(220, 220, 215);

   // Y label in left margin
   if (_formatY)
   {
      const QString yLabel = _formatY(data.y);
      const QRect yRect(0, cy - 8, _marginLeft - 5, 16);
      painter.fillRect(yRect, labelBg);
      painter.setPen(labelFg);
      painter.drawText(yRect, Qt::AlignRight | Qt::AlignVCenter, yLabel);
   }

   // X label in bottom margin
   if (_formatX && _showXLabels)
   {
      const QString xLabel = _formatX(data.x);
      constexpr int LABEL_WIDTH = 80;
      const QRect xRect(cx - (LABEL_WIDTH / 2), area.bottom() + 3,
                        LABEL_WIDTH, _marginBottom - 5);
      painter.fillRect(xRect, labelBg);
      painter.setPen(labelFg);
      painter.drawText(xRect, Qt::AlignCenter, xLabel);
   }
}

void PlotCursorOverlay::drawMeasurementCursors(QPainter& painter,
                                                const QRect& area) const
{
   if (!_dataToPixel)
   {
      return;
   }

   auto drawOneCursor = [&](const DataPoint& dp, const QColor& color)
   {
      const QPoint pt = _dataToPixel(dp, area);
      painter.setPen(QPen(color, 1, Qt::SolidLine));

      const int cx = std::clamp(pt.x(), area.left(), area.right());
      const int cy = std::clamp(pt.y(), area.top(), area.bottom());

      painter.drawLine(area.left(), cy, area.right(), cy);
      painter.drawLine(cx, area.top(), cx, area.bottom());

      // Axis labels
      QFont font = painter.font();
      font.setPointSize(10);
      painter.setFont(font);

      const QColor labelBg(color.red(), color.green(), color.blue(), 180);
      const QColor labelFg(255, 255, 255);

      // Y-axis label in left margin
      if (_formatY)
      {
         const QString yLabel = _formatY(dp.y);
         const QRect yRect(0, cy - 8, _marginLeft - 5, 16);
         painter.fillRect(yRect, labelBg);
         painter.setPen(labelFg);
         painter.drawText(yRect, Qt::AlignRight | Qt::AlignVCenter, yLabel);
      }

      // X-axis label in bottom margin
      if (_formatX && _showXLabels)
      {
         const QString xLabel = _formatX(dp.x);
         constexpr int LABEL_WIDTH = 80;
         const QRect xRect(cx - (LABEL_WIDTH / 2), area.bottom() + 3,
                           LABEL_WIDTH, _marginBottom - 5);
         painter.fillRect(xRect, labelBg);
         painter.setPen(labelFg);
         painter.drawText(xRect, Qt::AlignCenter, xLabel);
      }
   };

   if (_measCursor1.has_value())
   {
      drawOneCursor(*_measCursor1, QColor(230, 50, 50));
   }
   if (_measCursor2.has_value())
   {
      drawOneCursor(*_measCursor2, QColor(140, 30, 30));
   }
}

void PlotCursorOverlay::drawDeltaReadout(QPainter& painter,
                                          const QRect& area) const
{
   if (!_measCursor1.has_value() || !_measCursor2.has_value())
   {
      return;
   }

   QString deltaXLabel;
   if (_formatDeltaX)
   {
      deltaXLabel = _formatDeltaX(_measCursor1->x, _measCursor2->x);
   }

   QString deltaYLabel;
   if (_formatDeltaY)
   {
      deltaYLabel = _formatDeltaY(_measCursor1->y, _measCursor2->y);
   }

   QFont font = painter.font();
   font.setPointSize(11);
   font.setBold(true);
   painter.setFont(font);

   const QColor bg(30, 30, 35, 210);
   const QColor fg(220, 200, 180);

   constexpr int PAD    = 5;
   constexpr int LINE_H = 16;
   constexpr int BOX_W  = 180;
   constexpr int BOX_H  = (LINE_H * 2) + (PAD * 2);
   const int bx = area.left() + 6;
   const int by = area.bottom() - BOX_H - 6;

   const QRect box(bx, by, BOX_W, BOX_H);
   painter.fillRect(box, bg);
   painter.setPen(QColor(80, 80, 90));
   painter.drawRect(box);

   painter.setPen(fg);
   painter.drawText(bx + PAD, by + PAD,
                    BOX_W - (2 * PAD), LINE_H,
                    Qt::AlignLeft | Qt::AlignVCenter, deltaXLabel);
   painter.drawText(bx + PAD, by + PAD + LINE_H,
                    BOX_W - (2 * PAD), LINE_H,
                    Qt::AlignLeft | Qt::AlignVCenter, deltaYLabel);

   font.setBold(false);
   painter.setFont(font);
}

void PlotCursorOverlay::setLinkedTrackingX(double xData)
{
   _linkedTrackingX = xData;
}

void PlotCursorOverlay::clearLinkedTrackingX()
{
   _linkedTrackingX.reset();
}

void PlotCursorOverlay::setLinkedMeasCursors(std::optional<double> x1,
                                              std::optional<double> x2)
{
   _linkedMeas1X = x1;
   _linkedMeas2X = x2;
}

void PlotCursorOverlay::drawLinkedCursors(QPainter& painter,
                                           const QRect& area) const
{
   if (!_dataToPixel)
   {
      return;
   }

   auto drawVerticalLine = [&](double xData, const QColor& color, Qt::PenStyle style)
   {
      const DataPoint dp{xData, 0.0};
      const QPoint pt = _dataToPixel(dp, area);
      const int cx = std::clamp(pt.x(), area.left(), area.right());
      painter.setPen(QPen(color, 1, style));
      painter.drawLine(cx, area.top(), cx, area.bottom());

      // X-axis label in bottom margin (only when axis is visible)
      if (_formatX && _showXLabels)
      {
         QFont font = painter.font();
         font.setPointSize(10);
         painter.setFont(font);

         const QColor labelBg(color.red(), color.green(), color.blue(), 180);
         const QString xLabel = _formatX(xData);
         constexpr int LABEL_WIDTH = 80;
         const QRect xRect(cx - (LABEL_WIDTH / 2), area.bottom() + 3,
                           LABEL_WIDTH, _marginBottom - 5);
         painter.fillRect(xRect, labelBg);
         painter.setPen(QColor(255, 255, 255));
         painter.drawText(xRect, Qt::AlignCenter, xLabel);
      }
   };

   if (_linkedTrackingX.has_value())
   {
      drawVerticalLine(*_linkedTrackingX, QColor(220, 220, 215, 160), Qt::DashLine);
   }
   if (_linkedMeas1X.has_value())
   {
      drawVerticalLine(*_linkedMeas1X, QColor(230, 50, 50), Qt::SolidLine);
   }
   if (_linkedMeas2X.has_value())
   {
      drawVerticalLine(*_linkedMeas2X, QColor(140, 30, 30), Qt::SolidLine);
   }
}

// ============================================================================
// Bandwidth cursor
// ============================================================================

void PlotCursorOverlay::setBandwidthCursorEnabled(bool enabled)
{
   _bwCursorEnabled = enabled;
   if (!enabled)
   {
      _bwCursorLocked = false;
      _linkedBwLockActive = false;
   }
}

void PlotCursorOverlay::setBandwidthCursorHalfWidth(double dataFrac)
{
   _bwCursorHalfWidthFrac = dataFrac;
}

bool PlotCursorOverlay::lockBandwidthCursor(const QPoint& pos,
                                             const QRect& plotArea)
{
   if (!_bwCursorEnabled || !plotArea.contains(pos) || !_pixelToData)
   {
      return false;
   }
   const auto data = _pixelToData(pos, plotArea);
   _bwCursorLocked  = true;
   _bwCursorLockedX = data.x;
   return true;
}

void PlotCursorOverlay::lockBandwidthCursorAt(double xData)
{
   _bwCursorLocked  = true;
   _bwCursorLockedX = xData;
}

void PlotCursorOverlay::unlockBandwidthCursor()
{
   _bwCursorLocked = false;
}

void PlotCursorOverlay::setLinkedBandwidthLock(double xData,
                                                double halfWidthFrac)
{
   _linkedBwLockActive    = true;
   _linkedBwLockX         = xData;
   _linkedBwLockHalfWidth = halfWidthFrac;
}

void PlotCursorOverlay::clearLinkedBandwidthLock()
{
   _linkedBwLockActive = false;
}

void PlotCursorOverlay::drawBandwidthCursor(QPainter& painter,
                                             const QRect& area) const
{
   if (!_bwCursorEnabled || !_dataToPixel)
   {
      return;
   }

   // Determine the X data centre and half-width for the band.
   double centerX     = 0.0;
   double halfWidth   = _bwCursorHalfWidthFrac;
   bool   shouldDraw  = false;

   if (_bwCursorLocked)
   {
      centerX    = _bwCursorLockedX;
      shouldDraw = true;
   }
   else if (_cursorInPlot && _pixelToData)
   {
      const auto data = _pixelToData(_cursorPos, area);
      centerX    = data.x;
      shouldDraw = true;
   }
   else if (_linkedTrackingX.has_value())
   {
      centerX    = *_linkedTrackingX;
      shouldDraw = true;
   }

   // Also draw linked bandwidth lock from peer widget
   if (_linkedBwLockActive)
   {
      centerX    = _linkedBwLockX;
      halfWidth  = _linkedBwLockHalfWidth;
      shouldDraw = true;
   }

   if (!shouldDraw)
   {
      return;
   }

   // Convert data-space boundaries to pixel positions.
   const DataPoint leftPt{centerX - halfWidth, 0.0};
   const DataPoint rightPt{centerX + halfWidth, 0.0};
   const QPoint leftPx  = _dataToPixel(leftPt, area);
   const QPoint rightPx = _dataToPixel(rightPt, area);

   const int x1 = std::clamp(leftPx.x(), area.left(), area.right());
   const int x2 = std::clamp(rightPx.x(), area.left(), area.right());

   if (x2 > x1)
   {
      const QColor bandColor(160, 160, 160, 50);
      painter.fillRect(QRect(x1, area.top(), x2 - x1, area.height()), bandColor);

      // Draw thin border lines at the edges
      const QColor edgeColor(180, 180, 180, 120);
      painter.setPen(QPen(edgeColor, 1, Qt::SolidLine));
      if (leftPx.x() >= area.left() && leftPx.x() <= area.right())
      {
         painter.drawLine(leftPx.x(), area.top(), leftPx.x(), area.bottom());
      }
      if (rightPx.x() >= area.left() && rightPx.x() <= area.right())
      {
         painter.drawLine(rightPx.x(), area.top(), rightPx.x(), area.bottom());
      }
   }
}

} // namespace RealTimeGraphs
