#ifndef SPECTRUMWIDGET_H_
#define SPECTRUMWIDGET_H_

// Project headers
#include "PlotWidgetBase.h"
#include "ColorMap.h"

// System headers
#include <chrono>
#include <cstdint>
#include <mutex>
#include <vector>

namespace RealTimeGraphs
{

class ColorBarStrip; // forward declaration

/**
 * @class SpectrumWidget
 * @brief Custom QPainter-based spectrum / FFT bar-chart widget.
 *
 * Renders a horizontal frequency axis and a vertical amplitude axis.
 * Each bin is drawn as a filled vertical bar whose color comes from
 * the active ColorMap.
 *
 * Call `setData()` from any thread; the widget double-buffers the data
 * and schedules a repaint.
 */
class SpectrumWidget : public PlotWidgetBase
{
   Q_OBJECT

public:
   explicit SpectrumWidget(QWidget* parent = nullptr);

   /**
    * @brief Replace the current spectrum data.  The vector is copied.
    * Thread-safe — can be called from a producer thread.
    * @param magnitudes  Linear magnitudes (0 … 1 normalised).
    */
   void setData(const std::vector<float>& magnitudes);

   /** @brief Set the dB display range (e.g., -120 to 0). */
   void setDbRange(float minDb, float maxDb);

   /** @brief Set whether data is already in dB (true) or linear (false). */
   void setInputIsDb(bool isDb);

   /** @brief Change the color palette. */
   void setColorMap(ColorMap::Palette palette);

   /** @brief Set number of horizontal grid lines. */
   void setGridLines(int count);

   /**
    * @brief Reset the view to show the full data range (both axes).
    * This can also be triggered by double-clicking the plot.
    */
   void resetView();

   /** @brief Show or hide the built-in color-bar legend. */
   void setColorBarVisible(bool visible);

   /**
    * @brief Show or hide the X-axis labels and tick marks.
    * When hidden the bottom margin is collapsed, useful when stacked
    * above another widget that shows the same X axis.
    */
   void setXAxisVisible(bool visible);

   /** @brief Enable or disable decaying max-hold trace. */
   void setMaxHoldEnabled(bool enabled);

   /** @brief Set the decay rate for max hold (dB per second).  Default is 10 dB/s. */
   void setMaxHoldDecayRate(float dbPerSecond);

signals:
   /** @brief Emitted when the spectrum color-bar dB range changes. */
   void dbRangeChanged(float minDb, float maxDb);

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
   void onViewRangeChanged() override;

private:
   void drawBackground(QPainter& painter, const QRect& area) const;
   void drawGrid(QPainter& painter, const QRect& area) const;
   void drawSpectrum(QPainter& painter, const QRect& area) const;
   void drawMaxHold(QPainter& painter, const QRect& area) const;
   void drawLabels(QPainter& painter, const QRect& area) const;
   void drawFps(QPainter& painter, const QRect& area) const;

   // Convert a linear magnitude to normalised [0, 1] within the current view range.
   [[nodiscard]] float toNormalised(float value) const;

   // Sync the color-bar strip with the current view range and palette.
   void syncColorBar();

   mutable std::mutex _mutex;
   std::vector<float> _data;
   std::vector<float> _maxHoldData;  ///< Per-bin max-hold values (in same units as _data)

   ColorMap _colorMap{ColorMap::Palette::Viridis};
   float _minDb{-120.0F};
   float _maxDb{0.0F};
   bool _inputIsDb{false};
   int _gridLines{6};
   bool _maxHoldEnabled{false};
   float _maxHoldDecayRate{10.0F};  ///< dB per second

   // Current Y-axis view range (may differ from data range when zoomed/panned)
   double _viewMinDb{-120.0};
   double _viewMaxDb{0.0};

   // Which axes are affected by the current interaction
   enum class PanAxis : std::uint8_t { Both, XOnly, YOnly };

   // Pan state tracking
   bool _panning{false};
   PanAxis _panAxis{PanAxis::Both};
   QPoint _panStartPos;
   double _panStartMinDb{0.0};
   double _panStartMaxDb{0.0};
   double _panStartXStart{0.0};
   double _panStartXEnd{0.0};

   // Embedded color bar
   ColorBarStrip* _colorBar{nullptr};

   // Spectrum-specific layout constants
   static constexpr int MARGIN_BOTTOM_HIDDEN = 2;
   static constexpr int TICK_LENGTH   = 5;

   bool _xAxisVisible{true};

   // FPS counter
   std::chrono::steady_clock::time_point _fpsLastTime{std::chrono::steady_clock::now()};
   int _fpsFrameCount{0};
   float _fpsDisplay{0.0F};
};

} // namespace RealTimeGraphs

#endif // SPECTRUMWIDGET_H_
