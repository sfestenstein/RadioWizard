#ifndef PLOTPAINTERUTILS_H_
#define PLOTPAINTERUTILS_H_

class QPainter;
class QRect;
class QString;
class QWidget;

namespace RealTimeGraphs
{

/**
 * @brief Shared low-level drawing helpers for all plot widgets.
 *
 * These free functions encapsulate the boiler-plate background and grid
 * drawing that is duplicated across SpectrumWidget, OscilloscopeWidget,
 * and ConstellationWidget.
 */
namespace PlotPainterUtils
{

/// Tick-mark length in pixels, shared by all plot widgets.
constexpr int TICK_LENGTH = 5;

/// Standard font point size for axis tick labels.
constexpr int TICK_FONT_SIZE = 10;

/**
 * @brief Fill the widget background and the plot area.
 * @param painter   Active QPainter on the widget.
 * @param widget    The widget whose entire rect() is filled with the outer colour.
 * @param plotArea  The inner plot rectangle filled with the plot colour.
 */
void drawPlotBackground(QPainter& painter, const QWidget* widget,
                        const QRect& plotArea);

/**
 * @brief Draw a rectangular dotted grid.
 * @param painter    Active QPainter.
 * @param area       The plot rectangle.
 * @param hDivs      Number of horizontal divisions.
 * @param vDivs      Number of vertical divisions.
 * @param drawZeroH  If true, draw a solid centre horizontal line (amplitude zero).
 * @param drawZeroV  If true, draw a solid centre vertical line.
 */
void drawRectangularGrid(QPainter& painter, const QRect& area,
                         int hDivs, int vDivs,
                         bool drawZeroH = false, bool drawZeroV = false);

/**
 * @brief Set the painter font and pen for tick-label drawing.
 *
 * All plot widgets should call this before drawing tick labels so that
 * font size, weight, and colour are consistent.
 */
void setupTickLabelPainter(QPainter& painter);

/**
 * @brief Draw a Y-axis tick mark and right-aligned label.
 * @param leftMargin  Total left margin (tick + label must fit within it).
 */
void drawYTick(QPainter& painter, const QRect& plotArea,
               int leftMargin, int yPos, const QString& label);

/**
 * @brief Draw an X-axis tick mark and centred label.
 * @param bottomMargin  Total bottom margin (tick + label must fit within it).
 */
void drawXTick(QPainter& painter, const QRect& plotArea,
               int bottomMargin, int xPos, const QString& label);

} // namespace PlotPainterUtils
} // namespace RealTimeGraphs

#endif // PLOTPAINTERUTILS_H_
