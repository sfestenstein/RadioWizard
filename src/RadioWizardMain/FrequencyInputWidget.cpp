// Project headers
#include "FrequencyInputWidget.h"

// Third-party headers
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

// System headers
#include <algorithm>
#include <cmath>

// ============================================================================
// Construction
// ============================================================================

FrequencyInputWidget::FrequencyInputWidget(QWidget* parent)
   : QWidget(parent)
{
   setMouseTracking(true);
   setCursor(Qt::PointingHandCursor);

   // Use a large monospaced font for digits.
   _digitFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
   _digitFont.setPointSize(20);
   _digitFont.setWeight(QFont::DemiBold);

   // Smaller font for the "MHz" suffix.
   _suffixFont = _digitFont;
   _suffixFont.setPointSize(11);
   _suffixFont.setWeight(QFont::Normal);

   computeMetrics();
   freqToDigits();
}

// ============================================================================
// Public API
// ============================================================================

void FrequencyInputWidget::setFrequencyMhz(double freqMhz)
{
   freqMhz = std::clamp(freqMhz, MIN_FREQ_MHZ, MAX_FREQ_MHZ);
   if (std::abs(freqMhz - _freqMhz) < 1.0e-5)
   {
      return;
   }
   _freqMhz = freqMhz;
   freqToDigits();
   update();
}

double FrequencyInputWidget::frequencyMhz() const
{
   return _freqMhz;
}

QSize FrequencyInputWidget::sizeHint() const
{
   // Each digit cell has a gap after it (except last fractional),
   // plus the decimal-point width and the suffix.
   const double digitsWidth =
      (_digitWidth * TOTAL_DIGITS) + (DIGIT_GAP * (TOTAL_DIGITS - 1))
      + _decimalWidth + _suffixWidth;
   const auto totalWidth =
      static_cast<int>(std::ceil((MARGIN_X * 2.0) + digitsWidth));
   const auto totalHeight =
      static_cast<int>(std::ceil((MARGIN_Y * 2.0) + _digitHeight));
   return {totalWidth, totalHeight};
}

QSize FrequencyInputWidget::minimumSizeHint() const
{
   return sizeHint();
}

// ============================================================================
// Events
// ============================================================================

void FrequencyInputWidget::paintEvent(QPaintEvent* /*event*/)
{
   QPainter painter(this);
   painter.setRenderHint(QPainter::Antialiasing);

   // --- Color palette ---
   const QColor bgTop(22, 24, 30);
   const QColor bgBottom(14, 15, 20);
   const QColor cellIdle(32, 34, 42);
   const QColor cellBorderIdle(52, 54, 64);
   const QColor cellHover(42, 40, 52);
   const QColor cellBorderHover(180, 150, 60);
   const QColor digitNormal(220, 185, 70);       // warm amber-gold
   const QColor digitHover(255, 225, 110);       // bright gold on hover
   const QColor decimalColor(160, 135, 55);      // slightly dimmer dot
   const QColor suffixColor(90, 85, 70);         // muted warm grey
   const QColor arrowIdle(100, 92, 60, 0);       // transparent when idle
   const QColor arrowActive(220, 190, 80, 200);  // visible on hover

   // --- Background gradient ---
   QLinearGradient bgGrad(0, 0, 0, height());
   bgGrad.setColorAt(0.0, bgTop);
   bgGrad.setColorAt(1.0, bgBottom);
   painter.fillRect(rect(), bgGrad);

   // Outer rounded border.
   {
      QPainterPath outline;
      outline.addRoundedRect(
         QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
      painter.setPen(QPen(QColor(55, 55, 65), 1.0));
      painter.drawPath(outline);
   }

   // --- Digit cells ---
   for (int i = 0; i < TOTAL_DIGITS; ++i)
   {
      const QRectF r = digitRect(i);
      const bool hovered = (i == _hoveredDigit);

      // Cell background with subtle top-down gradient.
      {
         QLinearGradient cellGrad(r.topLeft(), r.bottomLeft());
         const QColor base = hovered ? cellHover : cellIdle;
         cellGrad.setColorAt(0.0, base.lighter(115));
         cellGrad.setColorAt(1.0, base);

         QPainterPath cellPath;
         cellPath.addRoundedRect(r, CELL_RADIUS, CELL_RADIUS);
         painter.fillPath(cellPath, cellGrad);

         // Cell border.
         painter.setPen(
            QPen(hovered ? cellBorderHover : cellBorderIdle, 1.0));
         painter.drawPath(cellPath);
      }

      // Arrow indicators (only visible on hover).
      if (hovered)
      {
         const QRectF arrowUp(
            r.left(), r.top(), r.width(), ARROW_ZONE);
         const QRectF arrowDown(
            r.left(), r.bottom() - ARROW_ZONE, r.width(), ARROW_ZONE);

         // Brighter arrow on the side the cursor is on.
         drawArrow(painter,  arrowUp,   true,
                   _hoverUpper ? arrowActive : arrowActive.darker(200));
         drawArrow(painter, arrowDown, false,
                   _hoverUpper ? arrowActive.darker(200) : arrowActive);
      }

      // Digit text.
      painter.setFont(_digitFont);
      painter.setPen(hovered ? digitHover : digitNormal);
      painter.drawText(r, Qt::AlignCenter, QString::number(_digits[i]));
   }

   // --- Decimal point ---
   const double dpX = decimalPointX();
   const QRectF dpRect(dpX, MARGIN_Y + ARROW_ZONE,
                        _decimalWidth, _digitHeight - (2.0 * ARROW_ZONE));
   painter.setFont(_digitFont);
   painter.setPen(decimalColor);
   painter.drawText(dpRect, Qt::AlignCenter, QStringLiteral("."));

   // --- "MHz" suffix ---
   const double suffixX =
      dpX + _decimalWidth + ((_digitWidth + DIGIT_GAP) * NUM_FRACTIONAL_DIGITS);
   const QRectF suffixRect(suffixX, MARGIN_Y, _suffixWidth, _digitHeight);
   painter.setFont(_suffixFont);
   painter.setPen(suffixColor);
   painter.drawText(suffixRect, Qt::AlignVCenter | Qt::AlignLeft,
                    QStringLiteral("MHz"));
}

void FrequencyInputWidget::mousePressEvent(QMouseEvent* event)
{
   const int idx = digitIndexAtX(static_cast<int>(event->position().x()));
   if (idx < 0)
   {
      return;
   }

   // Click above vertical centre → increment; below → decrement.
   const QRectF r = digitRect(idx);
   const double midY = r.top() + (r.height() / 2.0);

   if (event->position().y() < midY)
   {
      incrementDigit(idx);
   }
   else
   {
      decrementDigit(idx);
   }

   digitsToFreq();
   update();
   emit frequencyChanged(_freqMhz);
}

void FrequencyInputWidget::mouseMoveEvent(QMouseEvent* event)
{
   const int idx = digitIndexAtX(static_cast<int>(event->position().x()));
   bool upper = false;
   if (idx >= 0)
   {
      const QRectF r = digitRect(idx);
      upper = (event->position().y() < r.top() + (r.height() / 2.0));
   }

   if (idx != _hoveredDigit || upper != _hoverUpper)
   {
      _hoveredDigit = idx;
      _hoverUpper = upper;
      update();
   }
}

void FrequencyInputWidget::leaveEvent(QEvent* /*event*/)
{
   if (_hoveredDigit != -1)
   {
      _hoveredDigit = -1;
      update();
   }
}

void FrequencyInputWidget::wheelEvent(QWheelEvent* event)
{
   const int idx = digitIndexAtX(static_cast<int>(event->position().x()));
   if (idx < 0)
   {
      return;
   }

   // angleDelta().y() > 0 is scroll up → increment.
   if (event->angleDelta().y() > 0)
   {
      incrementDigit(idx);
   }
   else if (event->angleDelta().y() < 0)
   {
      decrementDigit(idx);
   }

   digitsToFreq();
   update();
   emit frequencyChanged(_freqMhz);
   event->accept();
}

// ============================================================================
// Internal helpers
// ============================================================================

void FrequencyInputWidget::freqToDigits()
{
   // Convert frequency to an integer count of 0.0001 MHz steps
   // to avoid floating-point rounding issues.
   auto ticks = static_cast<int64_t>(std::round(_freqMhz * 10000.0));
   ticks = std::clamp(ticks, static_cast<int64_t>(0),
                      static_cast<int64_t>(999999999));

   // Extract digits from most-significant to least-significant.
   // Digit 0 is the ten-thousands place of the integer part,
   // digit 8 is the ten-thousandths (0.0001) place.
   for (int i = TOTAL_DIGITS - 1; i >= 0; --i)
   {
      _digits[static_cast<size_t>(i)] = static_cast<int>(ticks % 10);
      ticks /= 10;
   }
}

void FrequencyInputWidget::digitsToFreq()
{
   int64_t ticks = 0;
   for (int i = 0; i < TOTAL_DIGITS; ++i)
   {
      ticks = (ticks * 10) + _digits[static_cast<size_t>(i)];
   }
   _freqMhz = std::clamp(static_cast<double>(ticks) / 10000.0,
                          MIN_FREQ_MHZ, MAX_FREQ_MHZ);
}

void FrequencyInputWidget::incrementDigit(int digitIndex)
{
   if (digitIndex < 0 || digitIndex >= TOTAL_DIGITS)
   {
      return;
   }

   auto idx = static_cast<size_t>(digitIndex);
   if (_digits[idx] < 9)
   {
      _digits[idx]++;
   }
   else
   {
      _digits[idx] = 0;
      // Carry to the next more-significant digit.
      if (digitIndex > 0)
      {
         incrementDigit(digitIndex - 1);
      }
   }
}

void FrequencyInputWidget::decrementDigit(int digitIndex)
{
   if (digitIndex < 0 || digitIndex >= TOTAL_DIGITS)
   {
      return;
   }

   auto idx = static_cast<size_t>(digitIndex);
   if (_digits[idx] > 0)
   {
      _digits[idx]--;
   }
   else
   {
      _digits[idx] = 9;
      // Borrow from the next more-significant digit.
      if (digitIndex > 0)
      {
         decrementDigit(digitIndex - 1);
      }
   }
}

int FrequencyInputWidget::digitIndexAtX(int x) const
{
   for (int i = 0; i < TOTAL_DIGITS; ++i)
   {
      const QRectF r = digitRect(i);
      if (x >= r.left() && x < r.right())
      {
         return i;
      }
   }
   return -1;
}

QRectF FrequencyInputWidget::digitRect(int index) const
{
   double xPos = MARGIN_X;
   if (index < NUM_INTEGER_DIGITS)
   {
      xPos += (_digitWidth + DIGIT_GAP) * index;
   }
   else
   {
      // Integer digits + decimal point + fractional offset.
      xPos += ((_digitWidth + DIGIT_GAP) * NUM_INTEGER_DIGITS)
            + _decimalWidth
            + ((_digitWidth + DIGIT_GAP)
               * (index - NUM_INTEGER_DIGITS));
   }
   return {xPos, MARGIN_Y, _digitWidth, _digitHeight};
}

double FrequencyInputWidget::decimalPointX() const
{
   return MARGIN_X + ((_digitWidth + DIGIT_GAP) * NUM_INTEGER_DIGITS);
}

void FrequencyInputWidget::drawArrow(
   QPainter& painter, const QRectF& rect, bool up, const QColor& color)
{
   if (color.alpha() == 0)
   {
      return;
   }

   const double cx = rect.center().x();
   const double cy = rect.center().y();
   const double halfW = 4.0;
   const double halfH = 3.0;

   QPolygonF tri;
   if (up)
   {
      tri << QPointF(cx, cy - halfH)
          << QPointF(cx - halfW, cy + halfH)
          << QPointF(cx + halfW, cy + halfH);
   }
   else
   {
      tri << QPointF(cx, cy + halfH)
          << QPointF(cx - halfW, cy - halfH)
          << QPointF(cx + halfW, cy - halfH);
   }

   painter.setPen(Qt::NoPen);
   painter.setBrush(color);
   painter.drawPolygon(tri);
   painter.setBrush(Qt::NoBrush);
}

void FrequencyInputWidget::computeMetrics()
{
   const QFontMetricsF fm(_digitFont);

   // Use the widest digit (usually '0' or 'M') for uniform spacing.
   _digitWidth = 0.0;
   for (char c = '0'; c <= '9'; ++c)
   {
      _digitWidth = std::max(_digitWidth,
                             fm.horizontalAdvance(QChar(c)));
   }
   // Padding so digits don't touch cell edges.
   _digitWidth += 6.0;

   // Height includes space for arrow zones above and below the digit.
   _digitHeight = fm.height() + (2.0 * ARROW_ZONE)+ 4.0;
   _decimalWidth = fm.horizontalAdvance(QChar('.')) + 2.0;

   const QFontMetricsF sfm(_suffixFont);
   _suffixWidth = sfm.horizontalAdvance(QStringLiteral("MHz")) + 8.0;

   setFixedSize(sizeHint());
}
