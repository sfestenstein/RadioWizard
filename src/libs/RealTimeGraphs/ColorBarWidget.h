#ifndef COLORBARWIDGET_H_
#define COLORBARWIDGET_H_

// Project headers
#include "RealTimeGraphs/ColorMap.h"

// Third-party headers
#include <QSpinBox>
#include <QWidget>

namespace RealTimeGraphs
{

/// Internal widget that paints only the gradient strip and tick labels.
/// Not intended for direct use — embedded inside `ColorBarWidget`.
class ColorBarStrip : public QWidget
{
   Q_OBJECT

public:
   explicit ColorBarStrip(QWidget* parent = nullptr);

   void setDbRange(float minDb, float maxDb);
   void setColorMap(const ColorMap& colorMap);
   void setTickCount(int count);

   [[nodiscard]] QSize sizeHint() const override;
   [[nodiscard]] QSize minimumSizeHint() const override;

protected:
   void paintEvent(QPaintEvent* event) override;

private:
   ColorMap _colorMap{ColorMap::Palette::Viridis};
   float _minDb{-120.0F};
   float _maxDb{0.0F};
   int _tickCount{6};

   static constexpr int BAR_WIDTH   = 18;
   static constexpr int LABEL_WIDTH = 40;
   static constexpr int H_PADDING   = 4;
   static constexpr int V_PADDING   = 2;
};

/// Standalone vertical color-bar legend widget with interactive dB controls.
///
/// Displays a gradient strip mapped through the active `ColorMap` with
/// dB tick labels and spin boxes to adjust the min/max dB range.
/// Emits `dbRangeChanged()` when the user adjusts either spin box.
///
/// Designed to be placed beside a spectrum or waterfall widget using
/// a `QHBoxLayout`.
class ColorBarWidget : public QWidget
{
   Q_OBJECT

public:
   explicit ColorBarWidget(QWidget* parent = nullptr);

   /// Set the dB display range (also updates the spin boxes).
   void setDbRange(float minDb, float maxDb);

   /// Change the color palette.
   void setColorMap(ColorMap::Palette palette);

   /// Set number of labelled tick marks on the gradient strip.
   void setTickCount(int count);

   /// Preferred size — narrow vertical strip with controls.
   [[nodiscard]] QSize sizeHint() const override;
   [[nodiscard]] QSize minimumSizeHint() const override;

signals:
   /// Emitted when the user changes either spin box.
   void dbRangeChanged(float minDb, float maxDb);

private:
   void onSpinBoxChanged();

   QSpinBox* _maxSpin{nullptr};
   QSpinBox* _minSpin{nullptr};
   ColorBarStrip* _strip{nullptr};

   ColorMap _colorMap{ColorMap::Palette::Viridis};
};

} // namespace RealTimeGraphs

#endif // COLORBARWIDGET_H_
