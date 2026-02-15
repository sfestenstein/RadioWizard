#ifndef WATERFALLWIDGET_H_
#define WATERFALLWIDGET_H_

// Project headers
#include "CircularBuffer.h"
#include "PlotWidgetBase.h"
#include "ColorMap.h"

// Third-party headers
#include <QImage>

// System headers
#include <chrono>
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
class WaterfallWidget : public PlotWidgetBase
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

   /// Show or hide the built-in color-bar legend.
   void setColorBarVisible(bool visible);

   /// Get the minimum and maximum amplitude values (in dB) from all rows in history.
   /// Returns std::nullopt if no data is available.
   /// @return std::optional containing {minDb, maxDb} if data exists, std::nullopt otherwise.
   [[nodiscard]] std::optional<std::pair<float, float>> getAmplitudeRange() const;

protected:
   void paintEvent(QPaintEvent* event) override;
   void resizeEvent(QResizeEvent* event) override;
   void wheelEvent(QWheelEvent* event) override;
   void mousePressEvent(QMouseEvent* event) override;
   void mouseMoveEvent(QMouseEvent* event) override;
   void mouseReleaseEvent(QMouseEvent* event) override;
   void mouseDoubleClickEvent(QMouseEvent* event) override;
   void leaveEvent(QEvent* event) override;

   [[nodiscard]] QRect plotArea() const override;

private:
   /// Rebuild the off-screen image from circular buffer contents.
   void rebuildImage();

   /// Draw frequency tick labels along the x-axis.
   void drawFrequencyLabels(QPainter& painter, const QRect& area) const;

   /// Draw time-age labels along the y-axis.
   void drawTimeLabels(QPainter& painter, const QRect& area) const;

   /// Format a duration as a human-readable age string.
   [[nodiscard]] static std::string formatAge(double seconds);

   /// Convert a linear magnitude to normalised [0, 1] within the dB range.
   [[nodiscard]] float toNormalised(float value) const;

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

   // Pan state tracking
   bool _panning{false};
   QPoint _panStartPos;
   double _panStartXStart{0.0};
   double _panStartXEnd{0.0};

   // Embedded color bar
   ColorBarStrip* _colorBar{nullptr};
};

} // namespace RealTimeGraphs

#endif // WATERFALLWIDGET_H_
