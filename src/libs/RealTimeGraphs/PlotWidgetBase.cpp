#include "PlotWidgetBase.h"
#include "CommonGuiUtils.h"

#include <algorithm>
#include <cmath>

namespace RealTimeGraphs
{

// ============================================================================
// Construction
// ============================================================================

PlotWidgetBase::PlotWidgetBase(QWidget* parent)
   : QWidget(parent)
{
   setAttribute(Qt::WA_OpaquePaintEvent);
   setMouseTracking(true);
   setMinimumSize(320, 200);

   _cursorOverlay.setMargins(MARGIN_LEFT, MARGIN_RIGHT, MARGIN_TOP, MARGIN_BOTTOM);
}

// ============================================================================
// Public API
// ============================================================================

void PlotWidgetBase::setFrequencyRange(double centerFreqHz, double bandwidthHz)
{
   _centerFreqHz = centerFreqHz;
   _bandwidthHz  = bandwidthHz;
   _viewXStart   = 0.0;
   _viewXEnd     = 1.0;

   if (_bwSelector.isEnabled())
   {
      syncBandwidthOverlay();
   }

   emit xViewChanged(_viewXStart, _viewXEnd);
   update();
}

QSize PlotWidgetBase::minimumSizeHint() const
{
   return {320, 200};
}

// ============================================================================
// Slots
// ============================================================================

void PlotWidgetBase::setXViewRange(double xStart, double xEnd)
{
   if (_viewXStart == xStart && _viewXEnd == xEnd)
   {
      return;
   }
   _viewXStart = xStart;
   _viewXEnd   = xEnd;
   onViewRangeChanged();
   update();
}

void PlotWidgetBase::setLinkedCursorX(double xData)
{
   _cursorOverlay.setLinkedTrackingX(xData);
   update();
}

void PlotWidgetBase::clearLinkedCursorX()
{
   _cursorOverlay.clearLinkedTrackingX();
   update();
}

void PlotWidgetBase::setLinkedMeasCursors(double x1Valid, double x1,
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

void PlotWidgetBase::clearMeasCursors()
{
   if (_cursorOverlay.clearCursors())
   {
      emitMeasCursorsChanged();
      update();
   }
   _cursorOverlay.setLinkedMeasCursors(std::nullopt, std::nullopt);
   update();
}

void PlotWidgetBase::setBandwidthCursorEnabled(bool enabled)
{
   _bwSelector.setEnabled(enabled);
   _cursorOverlay.setBandwidthCursorEnabled(enabled);
   if (enabled)
   {
      syncBandwidthOverlay();
   }
   update();
}

void PlotWidgetBase::setBandwidthCursorHalfWidthHz(double hz)
{
   _bwSelector.setHalfWidthHz(hz);
   syncBandwidthOverlay();
   update();
}

void PlotWidgetBase::lockBandwidthCursorAt(double xData)
{
   _cursorOverlay.lockBandwidthCursorAt(xData);
   update();
}

void PlotWidgetBase::unlockBandwidthCursor()
{
   _cursorOverlay.unlockBandwidthCursor();
   update();
}

// ============================================================================
// Protected helpers
// ============================================================================

void PlotWidgetBase::onViewRangeChanged()
{
   // Default: nothing.  Subclasses override (e.g. for color-bar sync).
}

void PlotWidgetBase::emitMeasCursorsChanged()
{
   const auto& c1 = _cursorOverlay.measCursor1();
   const auto& c2 = _cursorOverlay.measCursor2();
   emit measCursorsChanged(
      c1.has_value() ? 1.0 : 0.0, c1.has_value() ? c1->x : 0.0,
      c2.has_value() ? 1.0 : 0.0, c2.has_value() ? c2->x : 0.0);
}

QString PlotWidgetBase::formatXValue(double dataFrac) const
{
   if (_bandwidthHz > 0.0)
   {
      const double startFreq = _centerFreqHz - (_bandwidthHz / 2.0);
      const double freq = startFreq + (dataFrac * _bandwidthHz);
      return QString::fromStdString(formatFrequency(freq));
   }
   return QString::number(dataFrac, 'f', 3);
}

QString PlotWidgetBase::formatDeltaXValue(double x1, double x2) const
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
}

void PlotWidgetBase::syncBandwidthOverlay()
{
   _cursorOverlay.setBandwidthCursorHalfWidth(
      _bwSelector.halfWidthFraction(_bandwidthHz));
}

bool PlotWidgetBase::processCursorMove(const QPoint& pos)
{
   const QRect area = plotArea();
   if (_cursorOverlay.handleMouseMove(pos, area))
   {
      const double xFrac = static_cast<double>(pos.x() - area.left())
                         / static_cast<double>(area.width());
      const double dataX = _viewXStart +
                         (std::clamp(xFrac, 0.0, 1.0) * (_viewXEnd - _viewXStart));
      emit trackingCursorXChanged(dataX);
      update();
      return true;
   }
   return false;
}

void PlotWidgetBase::processLeaveEvent(QEvent* event)
{
   if (_cursorOverlay.handleLeave())
   {
      emit trackingCursorLeft();
      update();
   }
   QWidget::leaveEvent(event);
}

bool PlotWidgetBase::processBandwidthWheel(int angleDelta)
{
   if (!_bwSelector.isEnabled())
   {
      return false;
   }
   const double newHz = _bwSelector.adjustHalfWidth(angleDelta);
   syncBandwidthOverlay();
   emit bandwidthCursorHalfWidthChanged(newHz);
   update();
   return true;
}

bool PlotWidgetBase::processBandwidthClick(const QPoint& pos)
{
   if (!_bwSelector.isEnabled())
   {
      return false;
   }
   const QRect area = plotArea();
   if (!area.contains(pos))
   {
      return false;
   }

   if (_cursorOverlay.isBandwidthCursorLocked())
   {
      _cursorOverlay.unlockBandwidthCursor();
      emit bandwidthCursorUnlocked();
   }
   else if (_cursorOverlay.lockBandwidthCursor(pos, area))
   {
      emit bandwidthCursorLocked(_cursorOverlay.lockedBandwidthCursorX());
   }
   update();
   return true;
}

bool PlotWidgetBase::processMiddleButtonPress()
{
   if (_cursorOverlay.clearCursors())
   {
      emitMeasCursorsChanged();
      update();
   }
   if (_bwSelector.isEnabled() && _cursorOverlay.isBandwidthCursorLocked())
   {
      _cursorOverlay.unlockBandwidthCursor();
      emit bandwidthCursorUnlocked();
      update();
   }
   emit requestPeerCursorClear();
   return true;
}

bool PlotWidgetBase::processMeasCursorDoubleClick(QMouseEvent* event)
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
      return true;
   }
   if (event->button() == Qt::RightButton)
   {
      if (_cursorOverlay.placeCursor2(event->pos(), area))
      {
         emit requestPeerCursorClear();
         emitMeasCursorsChanged();
         update();
      }
      return true;
   }
   return false;
}

} // namespace RealTimeGraphs
