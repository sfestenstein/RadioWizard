#include "ColorBarWidget.h"

#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QVBoxLayout>

#include <algorithm>

namespace RealTimeGraphs
{

// ============================================================================
// ColorBarStrip — internal gradient painting widget
// ============================================================================

ColorBarStrip::ColorBarStrip(QWidget* parent)
   : QWidget(parent)
{
   setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
   setAttribute(Qt::WA_OpaquePaintEvent);
}

void ColorBarStrip::setDbRange(float minDb, float maxDb)
{
   _minDb = minDb;
   _maxDb = maxDb;
   update();
}

void ColorBarStrip::setColorMap(const ColorMap& colorMap)
{
   _colorMap = colorMap;
   update();
}

void ColorBarStrip::setTickCount(int count)
{
   _tickCount = std::max(1, count);
   update();
}

QSize ColorBarStrip::sizeHint() const
{
   return {H_PADDING + BAR_WIDTH + 3 + LABEL_WIDTH + H_PADDING, 100};
}

QSize ColorBarStrip::minimumSizeHint() const
{
   return {sizeHint().width(), 60};
}

void ColorBarStrip::paintEvent(QPaintEvent* /*event*/)
{
   QPainter painter(this);
   painter.setRenderHint(QPainter::Antialiasing, false);

   painter.fillRect(rect(), QColor(25, 25, 30));

   const int barLeft   = H_PADDING;
   const int barTop    = V_PADDING;
   const int barHeight = height() - (2 * V_PADDING);

   if (barHeight <= 0)
   {
      return;
   }

   // Gradient strip — top = high value, bottom = low value
   for (int y = 0; y < barHeight; ++y)
   {
      const float norm = 1.0F - (static_cast<float>(y) / static_cast<float>(barHeight));
      const Color c = _colorMap.map(norm);
      painter.fillRect(barLeft, barTop + y, BAR_WIDTH, 1,
                       QColor(c.r, c.g, c.b, c.a));
   }

   // Border
   painter.setPen(QColor(100, 100, 110));
   painter.drawRect(barLeft, barTop, BAR_WIDTH, barHeight);

   // Tick marks and dB labels
   QFont font = painter.font();
   font.setPointSize(10);
   painter.setFont(font);

   const int labelLeft = barLeft + BAR_WIDTH + 3;

   for (int i = 0; i <= _tickCount; ++i)
   {
      const float frac = static_cast<float>(i) / static_cast<float>(_tickCount);
      const float db = _maxDb - (frac * (_maxDb - _minDb));
      const int yPos = barTop + static_cast<int>(frac * static_cast<float>(barHeight));

      painter.setPen(QColor(100, 100, 110));
      painter.drawLine(barLeft + BAR_WIDTH, yPos,
                       barLeft + BAR_WIDTH + 2, yPos);

      painter.setPen(QColor(180, 180, 190));
      const QString label = QString::number(static_cast<int>(db));
      painter.drawText(labelLeft, yPos - 6, LABEL_WIDTH, 12,
                       Qt::AlignLeft | Qt::AlignVCenter, label);
   }
}

// ============================================================================
// ColorBarWidget — composite widget with spin boxes
// ============================================================================

ColorBarWidget::ColorBarWidget(QWidget* parent)
   : QWidget(parent)
{
   setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

   auto* layout = new QVBoxLayout(this);
   layout->setContentsMargins(0, 0, 0, 0);
   layout->setSpacing(2);

   // Max dB spin box (top)
   _maxSpin = new QSpinBox;
   _maxSpin->setRange(-200, 100);
   _maxSpin->setValue(0);
   _maxSpin->setSuffix(" dB");
   _maxSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
   _maxSpin->setAlignment(Qt::AlignCenter);
   _maxSpin->setFixedWidth(69);
   _maxSpin->setToolTip("Maximum dB");
   _maxSpin->setStyleSheet(
      "QSpinBox { background: rgb(30, 30, 36); color: #B4B4BE; border: 1px solid #444;"
      " padding: 1px; font-size: 11pt; }"
   );

   // Gradient strip
   _strip = new ColorBarStrip;

   // Min dB spin box (bottom)
   _minSpin = new QSpinBox;
   _minSpin->setRange(-200, 100);
   _minSpin->setValue(-120);
   _minSpin->setSuffix(" dB");
   _minSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
   _minSpin->setAlignment(Qt::AlignCenter);
   _minSpin->setFixedWidth(69);
   _minSpin->setToolTip("Minimum dB");
   _minSpin->setStyleSheet(
      "QSpinBox { background: #1E1E24; color: #B4B4BE; border: 1px solid #444;"
      " padding: 1px; font-size: 11pt; }"
   );

   layout->addWidget(_maxSpin, 0, Qt::AlignCenter);
   layout->addWidget(_strip, 1);
   layout->addWidget(_minSpin, 0, Qt::AlignCenter);

   // Connect spin boxes
   connect(_maxSpin, QOverload<int>::of(&QSpinBox::valueChanged),
           this, &ColorBarWidget::onSpinBoxChanged);
   connect(_minSpin, QOverload<int>::of(&QSpinBox::valueChanged),
           this, &ColorBarWidget::onSpinBoxChanged);
}

void ColorBarWidget::setDbRange(float minDb, float maxDb)
{
   _maxSpin->blockSignals(true);
   _minSpin->blockSignals(true);
   _maxSpin->setValue(static_cast<int>(maxDb));
   _minSpin->setValue(static_cast<int>(minDb));
   _maxSpin->blockSignals(false);
   _minSpin->blockSignals(false);

   _strip->setDbRange(minDb, maxDb);
}

void ColorBarWidget::setColorMap(ColorMap::Palette palette)
{
   _colorMap = ColorMap(palette);
   _strip->setColorMap(_colorMap);
}

void ColorBarWidget::setTickCount(int count)
{
   _strip->setTickCount(count);
}

QSize ColorBarWidget::sizeHint() const
{
   return {69, 200};
}

QSize ColorBarWidget::minimumSizeHint() const
{
   return {69, 120};
}

void ColorBarWidget::onSpinBoxChanged()
{
   auto minDb = static_cast<float>(_minSpin->value());
   auto maxDb = static_cast<float>(_maxSpin->value());

   // Enforce min < max
   if (minDb >= maxDb)
   {
      return;
   }

   _strip->setDbRange(minDb, maxDb);
   emit dbRangeChanged(minDb, maxDb);
}

} // namespace RealTimeGraphs
