#include "CommonGuiUtils.h"

#include <QMetaObject>
#include <QPainter>
#include <QWidget>

#include <cmath>
#include <format>

namespace RealTimeGraphs
{

std::string formatFrequency(double freqHz)
{
   const double absFreq = std::abs(freqHz);

   if (absFreq >= 1.0e9)
      return std::format("{:.3f} GHz", freqHz / 1.0e9);
   if (absFreq >= 1.0e6)
      return std::format("{:.3f} MHz", freqHz / 1.0e6);
   if (absFreq >= 1.0e3)
      return std::format("{:.3f} kHz", freqHz / 1.0e3);
   return std::format("{:.1f} Hz", freqHz);
}

void safeUpdate(QWidget* widget)
{
   QMetaObject::invokeMethod(widget, QOverload<>::of(&QWidget::update), Qt::AutoConnection);
}

namespace PlotUtils
{

// Shared palette constants (kept consistent across all plot widgets).
static constexpr auto OUTER_BG  = QColor(25, 25, 30);
static constexpr auto PLOT_BG   = QColor(15, 15, 20);
static constexpr auto GRID_CLR  = QColor(60, 60, 70);
static constexpr auto ZERO_CLR  = QColor(80, 80, 90);
static constexpr auto LABEL_CLR = QColor(180, 180, 190);

void drawPlotBackground(QPainter& painter, const QWidget* widget,
                        const QRect& plotArea)
{
   painter.fillRect(widget->rect(), OUTER_BG);
   painter.fillRect(plotArea, PLOT_BG);
}

void drawRectangularGrid(QPainter& painter, const QRect& area,
                         int hDivs, int vDivs,
                         bool drawZeroH, bool drawZeroV)
{
   painter.setPen(QPen(GRID_CLR, 1, Qt::DotLine));

   // Horizontal lines
   for (int i = 1; i < hDivs; ++i)
   {
      const int y = area.top() + ((i * area.height()) / hDivs);
      painter.drawLine(area.left(), y, area.right(), y);
   }

   // Vertical lines
   for (int i = 1; i < vDivs; ++i)
   {
      const int x = area.left() + ((i * area.width()) / vDivs);
      painter.drawLine(x, area.top(), x, area.bottom());
   }

   // Optional zero / centre lines (solid, slightly brighter)
   if (drawZeroH)
   {
      painter.setPen(QPen(ZERO_CLR, 1, Qt::SolidLine));
      const int y = area.top() + (area.height() / 2);
      painter.drawLine(area.left(), y, area.right(), y);
   }
   if (drawZeroV)
   {
      painter.setPen(QPen(ZERO_CLR, 1, Qt::SolidLine));
      const int x = area.left() + (area.width() / 2);
      painter.drawLine(x, area.top(), x, area.bottom());
   }
}

void setupTickLabelPainter(QPainter& painter)
{
   QFont font = painter.font();
   font.setPointSize(TICK_FONT_SIZE);
   painter.setFont(font);
   painter.setPen(LABEL_CLR);
}

void drawYTick(QPainter& painter, const QRect& plotArea,
               int leftMargin, int yPos, const QString& label)
{
   painter.setPen(LABEL_CLR);
   painter.drawLine(plotArea.left() - TICK_LENGTH, yPos,
                    plotArea.left(), yPos);
   painter.drawText(plotArea.left() - leftMargin, yPos - 6,
                    leftMargin - TICK_LENGTH - 2, 12,
                    Qt::AlignRight | Qt::AlignVCenter, label);
}

void drawXTick(QPainter& painter, const QRect& plotArea,
               int bottomMargin, int xPos, const QString& label)
{
   painter.setPen(LABEL_CLR);
   painter.drawLine(xPos, plotArea.bottom(),
                    xPos, plotArea.bottom() + TICK_LENGTH);
   constexpr int LABEL_WIDTH = 80;
   const int labelTop = plotArea.bottom() + TICK_LENGTH + 3;
   painter.drawText(xPos - (LABEL_WIDTH / 2), labelTop,
                    LABEL_WIDTH, bottomMargin - TICK_LENGTH - 6,
                    Qt::AlignCenter, label);
}

} // namespace PlotUtils
} // namespace RealTimeGraphs
