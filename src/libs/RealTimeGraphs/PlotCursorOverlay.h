#ifndef PLOTCURSOROVERLAY_H_
#define PLOTCURSOROVERLAY_H_

// Third-party headers
#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QString>

// System headers
#include <functional>
#include <optional>

namespace RealTimeGraphs
{

/**
 * @class PlotCursorOverlay
 * @brief Reusable cursor overlay for 2-D plot widgets.
 *
 * Provides:
 *   - Moving dashed off-white crosshair that tracks the mouse.
 *   - Two stationary measurement cursors (red / dark-red) placed by
 *     double-click, with axis labels.
 *   - Delta readout box when both measurement cursors are placed.
 *
 * The overlay is axis-agnostic: the owning widget supplies callbacks
 * that convert between pixel positions and human-readable axis values.
 */
class PlotCursorOverlay
{
public:
   /**
    * @class DataPoint
    * @brief A measurement cursor stored in data-space coordinates.
    */
   struct DataPoint
   {
      double x{0.0};   ///< X-axis data value
      double y{0.0};   ///< Y-axis data value
   };

   /** @brief Callback that converts a pixel position to data-space coordinates. */
   using PixelToDataFn = std::function<DataPoint(const QPoint& pixel,
                                                  const QRect& plotArea)>;

   /** @brief Callback that converts data-space coordinates to a pixel position. */
   using DataToPixelFn = std::function<QPoint(const DataPoint& data,
                                               const QRect& plotArea)>;

   /** @brief Callback that formats an X data value as a display string. */
   using FormatXFn = std::function<QString(double xValue)>;

   /** @brief Callback that formats a Y data value as a display string. */
   using FormatYFn = std::function<QString(double yValue)>;

   /** @brief Callback that formats the delta between two X data values. */
   using FormatDeltaXFn = std::function<QString(double x1, double x2)>;

   /** @brief Callback that formats the delta between two Y data values. */
   using FormatDeltaYFn = std::function<QString(double y1, double y2)>;

   PlotCursorOverlay() = default;

   /** @brief Configure the coordinate conversion callbacks. */
   void setPixelToData(PixelToDataFn fn) { _pixelToData = std::move(fn); }
   void setDataToPixel(DataToPixelFn fn) { _dataToPixel = std::move(fn); }
   void setFormatX(FormatXFn fn) { _formatX = std::move(fn); }
   void setFormatY(FormatYFn fn) { _formatY = std::move(fn); }
   void setFormatDeltaX(FormatDeltaXFn fn) { _formatDeltaX = std::move(fn); }
   void setFormatDeltaY(FormatDeltaYFn fn) { _formatDeltaY = std::move(fn); }

   /** @brief Set the layout margins so axis labels are positioned correctly. */
   void setMargins(int left, int right, int top, int bottom);

   /**
    * @brief Control whether X-axis labels are drawn by cursors.
    * Set to false when the X axis is hidden (e.g. stacked layout).
    */
   void setShowXLabels(bool show) { _showXLabels = show; }

   // ----- Mouse event handlers (call from the owning widget) -----

   /** @brief Call from mouseMoveEvent.  Returns true if repaint is needed. */
   bool handleMouseMove(const QPoint& pos, const QRect& plotArea);

   /** @brief Call from leaveEvent.  Returns true if repaint is needed. */
   bool handleLeave();

   /**
    * @brief Call from mouseDoubleClickEvent for left button.
    * Places measurement cursor 1 (red).  Returns true if repaint needed.
    */
   bool placeCursor1(const QPoint& pos, const QRect& plotArea);

   /**
    * @brief Call from mouseDoubleClickEvent for right button.
    * Places measurement cursor 2 (dark-red).  Returns true if repaint needed.
    */
   bool placeCursor2(const QPoint& pos, const QRect& plotArea);

   /** @brief Clear both measurement cursors.  Returns true if any were active. */
   bool clearCursors();

   /** @brief Get measurement cursor data (for signal emission). */
   [[nodiscard]] const std::optional<DataPoint>& measCursor1() const { return _measCursor1; }
   [[nodiscard]] const std::optional<DataPoint>& measCursor2() const { return _measCursor2; }

   /**
    * @brief Set or clear a linked vertical cursor line from another widget.
    * Only draws a vertical line — no horizontal, no Y label.
    */
   void setLinkedTrackingX(double xData);
   void clearLinkedTrackingX();

   /** @brief Set or clear linked measurement-cursor vertical lines from another widget. */
   void setLinkedMeasCursors(std::optional<double> x1, std::optional<double> x2);

   // ----- Bandwidth cursor -----

   /** @brief Enable or disable bandwidth cursor overlay. */
   void setBandwidthCursorEnabled(bool enabled);

   /** @brief Whether the bandwidth cursor is currently enabled. */
   [[nodiscard]] bool isBandwidthCursorEnabled() const { return _bwCursorEnabled; }

   /** @brief Set the half-width of the bandwidth cursor as a fraction of the full data range. */
   void setBandwidthCursorHalfWidth(double dataFrac);

   /** @brief Get the current bandwidth cursor half-width fraction. */
   [[nodiscard]] double bandwidthCursorHalfWidth() const { return _bwCursorHalfWidthFrac; }

   /**
    * @brief Lock the bandwidth cursor at the current tracking position.
    * Returns true if successfully locked.
    */
   bool lockBandwidthCursor(const QPoint& pos, const QRect& plotArea);

   /** @brief Lock the bandwidth cursor at a specific data-space X value. */
   void lockBandwidthCursorAt(double xData);

   /** @brief Unlock the bandwidth cursor so it follows the mouse again. */
   void unlockBandwidthCursor();

   /** @brief Whether the bandwidth cursor is locked in place. */
   [[nodiscard]] bool isBandwidthCursorLocked() const { return _bwCursorLocked; }

   /** @brief Get the locked bandwidth cursor X data value. */
   [[nodiscard]] double lockedBandwidthCursorX() const { return _bwCursorLockedX; }

   /** @brief Set or clear a linked bandwidth cursor lock from another widget. */
   void setLinkedBandwidthLock(double xData, double halfWidthFrac);

   /** @brief Clear the linked bandwidth cursor lock. */
   void clearLinkedBandwidthLock();

   // ----- Drawing (call from paintEvent after other content) -----

   /** @brief Draw the tracking crosshair, measurement cursors, and delta readout. */
   void draw(QPainter& painter, const QRect& plotArea) const;

private:
   void drawTrackingCrosshair(QPainter& painter, const QRect& area) const;
   void drawMeasurementCursors(QPainter& painter, const QRect& area) const;
   void drawDeltaReadout(QPainter& painter, const QRect& area) const;
   void drawLinkedCursors(QPainter& painter, const QRect& area) const;
   void drawBandwidthCursor(QPainter& painter, const QRect& area) const;

   // Coordinate callbacks
   PixelToDataFn _pixelToData;
   DataToPixelFn _dataToPixel;
   FormatXFn _formatX;
   FormatYFn _formatY;
   FormatDeltaXFn _formatDeltaX;
   FormatDeltaYFn _formatDeltaY;

   // Margins
   int _marginLeft{55};
   int _marginBottom{25};
   bool _showXLabels{true};

   // Tracking crosshair state
   bool _cursorInPlot{false};
   QPoint _cursorPos;

   // Measurement cursors
   std::optional<DataPoint> _measCursor1;
   std::optional<DataPoint> _measCursor2;

   // Linked vertical cursor lines from another widget
   std::optional<double> _linkedTrackingX;
   std::optional<double> _linkedMeas1X;
   std::optional<double> _linkedMeas2X;

   // Bandwidth cursor state
   bool _bwCursorEnabled{false};
   double _bwCursorHalfWidthFrac{0.0};
   bool _bwCursorLocked{false};
   double _bwCursorLockedX{0.0};

   // Linked bandwidth cursor lock from another widget
   bool _linkedBwLockActive{false};
   double _linkedBwLockX{0.0};
   double _linkedBwLockHalfWidth{0.0};
};

} // namespace RealTimeGraphs

#endif // PLOTCURSOROVERLAY_H_
