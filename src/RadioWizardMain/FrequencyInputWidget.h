#ifndef FREQUENCYINPUTWIDGET_H_
#define FREQUENCYINPUTWIDGET_H_

// Third-party headers
#include <QColor>
#include <QWidget>

// System headers
#include <array>

// Forward declarations
class QPainter;

/**
 * @class FrequencyInputWidget
 * @brief Custom SDR-style frequency display/editor widget.
 *
 * Displays a frequency in MHz with 5 integer digits and 4 fractional digits
 * (XXXXX.XXXX MHz) using a large monospaced font.
 *
 * Interaction:
 *   - Click above the centre of a digit to increment it.
 *   - Click below the centre of a digit to decrement it.
 *   - Scroll the mouse wheel over a digit to increment/decrement.
 *   - Digit rollover carries/borrows to the adjacent position
 *     (e.g. 9 → 0 increments the next higher digit).
 */
class FrequencyInputWidget : public QWidget
{
   Q_OBJECT

public:
   explicit FrequencyInputWidget(QWidget* parent = nullptr);

   /// Set the displayed frequency in MHz.
   void setFrequencyMhz(double freqMhz);

   /// Get the current frequency in MHz.
   [[nodiscard]] double frequencyMhz() const;

   [[nodiscard]] QSize sizeHint() const override;
   [[nodiscard]] QSize minimumSizeHint() const override;

signals:
   /// Emitted whenever the frequency changes (value in MHz).
   void frequencyChanged(double freqMhz);

protected:
   void paintEvent(QPaintEvent* event) override;
   void mousePressEvent(QMouseEvent* event) override;
   void mouseMoveEvent(QMouseEvent* event) override;
   void leaveEvent(QEvent* event) override;
   void wheelEvent(QWheelEvent* event) override;

private:
   static constexpr int NUM_INTEGER_DIGITS = 5;
   static constexpr int NUM_FRACTIONAL_DIGITS = 4;
   static constexpr int TOTAL_DIGITS = NUM_INTEGER_DIGITS + NUM_FRACTIONAL_DIGITS;
   static constexpr double MAX_FREQ_MHZ = 99999.9999;
   static constexpr double MIN_FREQ_MHZ = 0.0;

   /// Decompose _freqMhz into individual digits.
   void freqToDigits();

   /// Recompose _freqMhz from individual digits.
   void digitsToFreq();

   /// Increment digit at index with carry propagation.
   void incrementDigit(int digitIndex);

   /// Decrement digit at index with borrow propagation.
   void decrementDigit(int digitIndex);

   /// Return the digit index at pixel position x, or -1.
   [[nodiscard]] int digitIndexAtX(int x) const;

   /// Bounding rectangle for the digit at the given index.
   [[nodiscard]] QRectF digitRect(int index) const;

   /// X position where the decimal point starts.
   [[nodiscard]] double decimalPointX() const;

   /// Recompute font metrics / layout constants.
   void computeMetrics();

   /// Draw a small triangular arrow indicator.
   static void drawArrow(
      QPainter& painter, const QRectF& rect, bool up, const QColor& color);

   double _freqMhz{0.0};
   std::array<int, TOTAL_DIGITS> _digits{};
   int _hoveredDigit{-1};
   bool _hoverUpper{false};

   // Layout metrics (computed from font).
   QFont _digitFont;
   QFont _suffixFont;
   double _digitWidth{0.0};
   double _digitHeight{0.0};
   double _decimalWidth{0.0};
   double _suffixWidth{0.0};
   static constexpr double MARGIN_X = 8.0;
   static constexpr double MARGIN_Y = 6.0;
   static constexpr double DIGIT_GAP = 3.0;
   static constexpr double CELL_RADIUS = 5.0;
   static constexpr double ARROW_ZONE = 10.0;
};

#endif // FREQUENCYINPUTWIDGET_H_
