#ifndef BANDWIDTHSELECTOR_H_
#define BANDWIDTHSELECTOR_H_

namespace RealTimeGraphs
{

/// Bandwidth cursor state and scaling logic.
///
/// Manages the half-width value in Hz, mouse-wheel scaling with
/// clamping, and the Hz-to-fraction conversion.  Used by PlotWidgetBase
/// to centralise bandwidth cursor logic that was previously duplicated
/// in SpectrumWidget and WaterfallWidget.
class BandwidthSelector
{
public:
   BandwidthSelector() = default;

   /// Enable or disable the bandwidth selector.
   void setEnabled(bool enabled);
   [[nodiscard]] bool isEnabled() const { return _enabled; }

   /// Set the half-width in Hz.
   void setHalfWidthHz(double hz);
   [[nodiscard]] double halfWidthHz() const { return _halfWidthHz; }

   /// Apply a mouse-wheel delta to adjust the half-width.
   /// Positive angleDelta increases, negative decreases.
   /// Returns the new half-width in Hz.
   double adjustHalfWidth(int angleDelta);

   /// Convert the current half-width to a fraction of the given bandwidth.
   /// Returns 0.0 if bandwidthHz is not positive.
   [[nodiscard]] double halfWidthFraction(double bandwidthHz) const;

private:
   bool _enabled{false};
   double _halfWidthHz{100'000.0};

   static constexpr double SCALE_FACTOR = 1.25;
   static constexpr double MIN_HZ       = 1'000.0;
   static constexpr double MAX_HZ       = 5'000'000.0;
};

} // namespace RealTimeGraphs

#endif // BANDWIDTHSELECTOR_H_
