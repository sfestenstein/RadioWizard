#ifndef PLOTWIDGETBASE_H_
#define PLOTWIDGETBASE_H_

// Project headers
#include "BandwidthSelector.h"
#include "PlotCursorOverlay.h"

// Third-party headers
#include <QMouseEvent>
#include <QWheelEvent>
#include <QWidget>

namespace RealTimeGraphs
{

/**
 * @class PlotWidgetBase
 * @brief Abstract base class for frequency-domain plot widgets.
 *
 * Centralises the cursor overlay, bandwidth selector, and the shared
 * mouse-interaction logic that is common to SpectrumWidget and
 * WaterfallWidget.  Subclasses implement `plotArea()` and provide
 * their own painting, zooming, and panning.
 */
class PlotWidgetBase : public QWidget
{
   Q_OBJECT

public:
   explicit PlotWidgetBase(QWidget* parent = nullptr);

   /**
    * @brief Set the frequency range so the x-axis shows real frequencies.
    * @param centerFreqHz  Centre frequency in Hz.
    * @param bandwidthHz   Total bandwidth in Hz.
    */
   void setFrequencyRange(double centerFreqHz, double bandwidthHz);

   /** @brief Minimum size hint for layout. */
   [[nodiscard]] QSize minimumSizeHint() const override;

public slots:
   /**
    * @brief Set the visible X range (as fraction of total bin range [0, 1]).
    * Used for linked-axis synchronisation — does not re-emit xViewChanged.
    */
   void setXViewRange(double xStart, double xEnd);

   /** @brief Show a linked vertical cursor line from another widget. */
   void setLinkedCursorX(double xData);

   /** @brief Hide the linked vertical cursor line. */
   void clearLinkedCursorX();

   /** @brief Show linked measurement-cursor vertical lines from another widget. */
   void setLinkedMeasCursors(double x1Valid, double x1,
                             double x2Valid, double x2);

   /** @brief Clear local measurement cursors (called by linked peer). */
   void clearMeasCursors();

   /** @brief Enable or disable bandwidth cursor mode. */
   void setBandwidthCursorEnabled(bool enabled);

   /** @brief Set the bandwidth cursor half-width in Hz (for sync from peer widget). */
   void setBandwidthCursorHalfWidthHz(double hz);

   /** @brief Lock bandwidth cursor at a specific data-space X value (from peer). */
   void lockBandwidthCursorAt(double xData);

   /** @brief Unlock bandwidth cursor (from peer). */
   void unlockBandwidthCursor();

signals:
   /** @brief Emitted when the user zooms or pans the X axis. */
   void xViewChanged(double xStart, double xEnd);

   /** @brief Emitted when the tracking crosshair X position changes. */
   void trackingCursorXChanged(double xData);

   /** @brief Emitted when the tracking crosshair leaves the plot. */
   void trackingCursorLeft();

   /**
    * @brief Emitted when measurement cursors change.
    * Valid flags are 1.0 if present, 0.0 if absent.
    */
   void measCursorsChanged(double x1Valid, double x1,
                           double x2Valid, double x2);

   /** @brief Emitted when the peer widget should clear its measurement cursors. */
   void requestPeerCursorClear();

   /** @brief Emitted when the bandwidth cursor half-width changes (via mouse wheel). */
   void bandwidthCursorHalfWidthChanged(double halfWidthHz);

   /** @brief Emitted when the bandwidth cursor is locked by a click. */
   void bandwidthCursorLocked(double xData);

   /** @brief Emitted when the bandwidth cursor is unlocked. */
   void bandwidthCursorUnlocked();

protected:
   /** @brief Compute the plot area rectangle.  Subclasses must implement this. */
   [[nodiscard]] virtual QRect plotArea() const = 0;

   /**
    * @brief Virtual hook called when the view X range changes.
    * Override to perform additional synchronisation (e.g. color-bar update).
    */
   virtual void onViewRangeChanged();

   /** @brief Emit the measCursorsChanged signal with current overlay state. */
   void emitMeasCursorsChanged();

   /** @brief Format the X-axis value at a given data fraction. */
   [[nodiscard]] QString formatXValue(double dataFrac) const;

   /** @brief Format the delta between two X data values. */
   [[nodiscard]] QString formatDeltaXValue(double x1, double x2) const;

   /** @brief Push the current BandwidthSelector half-width to the overlay. */
   void syncBandwidthOverlay();

   // ----- Event-handling helpers for subclasses -----

   /**
    * @brief Process cursor tracking on mouse move (non-panning case).
    * Returns true if a repaint was scheduled.
    */
   bool processCursorMove(const QPoint& pos);

   /** @brief Process leave event for cursor tracking. */
   void processLeaveEvent(QEvent* event);

   /**
    * @brief Process bandwidth-cursor wheel adjustment.
    * Returns true if the event was consumed (caller should accept).
    */
   bool processBandwidthWheel(int angleDelta);

   /**
    * @brief Process bandwidth-cursor click (set/update locked position).
    * Returns true if the event was consumed.
    */
   bool processBandwidthClick(const QPoint& pos);

   /**
    * @brief Process middle-button press (clear cursors + BW unlock).
    * Returns true (always consumes).
    */
   bool processMiddleButtonPress();

   /**
    * @brief Process measurement-cursor placement on double-click.
    * Handles left (cursor 1) and right (cursor 2) buttons.
    * Returns true if the event was consumed.
    */
   bool processMeasCursorDoubleClick(QMouseEvent* event);

   // ----- Shared state -----

   PlotCursorOverlay _cursorOverlay;
   BandwidthSelector _bwSelector;

   double _centerFreqHz{0.0};
   double _bandwidthHz{0.0};

   double _viewXStart{0.0};   ///< visible start as fraction of bin range [0, 1]
   double _viewXEnd{1.0};     ///< visible end as fraction of bin range [0, 1]

   // Layout margin constants (shared by Spectrum and Waterfall)
   static constexpr int MARGIN_LEFT      = 55;
   static constexpr int MARGIN_RIGHT     = 83;  // COLOR_BAR_WIDTH + 15
   static constexpr int MARGIN_TOP       = 10;
   static constexpr int MARGIN_BOTTOM    = 25;
   static constexpr int COLOR_BAR_WIDTH  = 68;
};

} // namespace RealTimeGraphs

#endif // PLOTWIDGETBASE_H_
