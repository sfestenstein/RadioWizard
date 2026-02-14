#ifndef WATERFALLWIDGET_H_
#define WATERFALLWIDGET_H_

// Project headers
#include "CircularBuffer.h"
#include "PlotCursorOverlay.h"
#include "ColorMap.h"

// Third-party headers
#include <QMouseEvent>
#include <QWheelEvent>
#include <QImage>
#include <QWidget>

// System headers
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace RealTimeGraphs
{

class ColorBarStrip; // forward declaration

/// Custom QPainter-based waterfall / spectrogram widget.
///
/// Each call to `addRow()` pushes a new frequency-domain row onto the
/// display.  The most recent row appears at the bottom; older rows scroll
/// upward and are eventually discarded.  Internally uses a `CircularBuffer`
/// of `QImage` scan lines.
class WaterfallWidget : public QWidget
{
   Q_OBJECT

public:
   /// @param historyRows  Number of time rows to keep in history.
   /// @param parent       Parent widget.
   explicit WaterfallWidget(QWidget* parent = nullptr, int historyRows = 256);

   /// Append a new spectrum row.  The vector is copied.
   /// Thread-safe — can be called from a producer thread.
   /// @param magnitudes  Linear magnitudes (0 … 1 normalised) or dB values.
   void addRow(const std::vector<float>& magnitudes);

   /// Set the dB display range (e.g., -120 to 0).
   void setDbRange(float minDb, float maxDb);

   /// Set whether data is already in dB (true) or linear (false).
   void setInputIsDb(bool isDb);

   /// Change the color palette.
   void setColorMap(ColorMap::Palette palette);

   /// Set the frequency range so the x-axis shows real frequencies.
   /// @param centerFreqHz  Centre frequency in Hz.
   /// @param bandwidthHz   Total bandwidth in Hz.
   void setFrequencyRange(double centerFreqHz, double bandwidthHz);

   /// Show or hide the built-in color-bar legend.
   void setColorBarVisible(bool visible);

   /// Get the minimum and maximum amplitude values (in dB) from all rows in history.
   /// Returns std::nullopt if no data is available.
   /// @return std::optional containing {minDb, maxDb} if data exists, std::nullopt otherwise.
   [[nodiscard]] std::optional<std::pair<float, float>> getAmplitudeRange() const;

   /// Minimum size hint for layout.
   [[nodiscard]] QSize minimumSizeHint() const override;

public slots:
   /// Set the visible X range (as fraction of total bin range [0, 1]).
   /// Used for linked-axis synchronisation — does not re-emit xViewChanged.
   void setXViewRange(double xStart, double xEnd);

   /// Show a linked vertical cursor line from another widget.
   void setLinkedCursorX(double xData);

   /// Hide the linked vertical cursor line.
   void clearLinkedCursorX();

   /// Show linked measurement-cursor vertical lines from another widget.
   void setLinkedMeasCursors(double x1Valid, double x1,
                             double x2Valid, double x2);

   /// Clear local measurement cursors (called by linked peer).
   void clearMeasCursors();

signals:
   /// Emitted when the user zooms or pans the X axis.
   void xViewChanged(double xStart, double xEnd);

   /// Emitted when the tracking crosshair X position changes.
   void trackingCursorXChanged(double xData);

   /// Emitted when the tracking crosshair leaves the plot.
   void trackingCursorLeft();

   /// Emitted when measurement cursors change.
   /// Valid flags are 1.0 if present, 0.0 if absent.
   void measCursorsChanged(double x1Valid, double x1,
                           double x2Valid, double x2);

   /// Emitted when the peer widget should clear its measurement cursors.
   void requestPeerCursorClear();

protected:
   void paintEvent(QPaintEvent* event) override;
   void resizeEvent(QResizeEvent* event) override;
   void wheelEvent(QWheelEvent* event) override;
   void mousePressEvent(QMouseEvent* event) override;
   void mouseMoveEvent(QMouseEvent* event) override;
   void mouseReleaseEvent(QMouseEvent* event) override;
   void mouseDoubleClickEvent(QMouseEvent* event) override;
   void leaveEvent(QEvent* event) override;

private:
   /// Rebuild the off-screen image from circular buffer contents.
   void rebuildImage();

   /// Draw frequency tick labels along the x-axis.
   void drawFrequencyLabels(QPainter& painter, const QRect& area) const;

   /// Draw time-age labels along the y-axis.
   void drawTimeLabels(QPainter& painter, const QRect& area) const;

   /// Emit the measCursorsChanged signal with current overlay state.
   void emitMeasCursorsChanged();

   /// Format a duration as a human-readable age string.
   [[nodiscard]] static std::string formatAge(double seconds);

   /// Convert a linear magnitude to normalised [0, 1] within the dB range.
   [[nodiscard]] float toNormalised(float value) const;

   /// Format the X-axis value at a given data fraction.
   [[nodiscard]] QString formatXValue(double dataFrac) const;

   /// Compute the plot area from the current widget size.
   [[nodiscard]] QRect plotArea() const;

   mutable std::mutex _mutex;

   int _historyRows;
   int _binCount{0};

   /// Each element is a spectrum row (vector of normalised values).
   CommonUtils::CircularBuffer<std::vector<float>> _rows;

   /// Timestamp for each row (parallel to _rows).
   CommonUtils::CircularBuffer<std::chrono::steady_clock::time_point> _timestamps;

   /// Off-screen rendered spectrogram image.
   QImage _image;

   ColorMap _colorMap{ColorMap::Palette::Viridis};
   float _minDb{-120.0F};
   float _maxDb{0.0F};
   bool _inputIsDb{false};

   double _centerFreqHz{0.0};
   double _bandwidthHz{0.0};

   // Current X view range (may differ from full when zoomed/panned)
   double _viewXStart{0.0};   ///< visible start as fraction of bin range [0, 1]
   double _viewXEnd{1.0};     ///< visible end as fraction of bin range [0, 1]

   // Pan state tracking
   bool _panning{false};
   QPoint _panStartPos;
   double _panStartXStart{0.0};
   double _panStartXEnd{0.0};

   // Embedded color bar
   ColorBarStrip* _colorBar{nullptr};
   static constexpr int COLOR_BAR_WIDTH = 68;

   // Layout margins
   static constexpr int MARGIN_LEFT   = 55;
   static constexpr int MARGIN_RIGHT  = 83;  // COLOR_BAR_WIDTH + 15
   static constexpr int MARGIN_TOP    = 10;
   static constexpr int MARGIN_BOTTOM = 25;

   // Cursor overlay
   PlotCursorOverlay _cursorOverlay;
};

} // namespace RealTimeGraphs

#endif // WATERFALLWIDGET_H_
