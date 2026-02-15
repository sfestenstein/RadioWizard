#include "BandwidthSelector.h"

#include <algorithm>

namespace RealTimeGraphs
{

void BandwidthSelector::setEnabled(bool enabled)
{
   _enabled = enabled;
}

void BandwidthSelector::setHalfWidthHz(double hz)
{
   _halfWidthHz = std::clamp(hz, MIN_HZ, MAX_HZ);
}

double BandwidthSelector::adjustHalfWidth(int angleDelta)
{
   if (angleDelta > 0)
   {
      _halfWidthHz *= SCALE_FACTOR;
   }
   else
   {
      _halfWidthHz /= SCALE_FACTOR;
   }
   _halfWidthHz = std::clamp(_halfWidthHz, MIN_HZ, MAX_HZ);
   return _halfWidthHz;
}

double BandwidthSelector::halfWidthFraction(double bandwidthHz) const
{
   if (bandwidthHz <= 0.0)
   {
      return 0.0;
   }
   return _halfWidthHz / bandwidthHz;
}

} // namespace RealTimeGraphs
