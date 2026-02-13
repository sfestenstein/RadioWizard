#include "CommonGuiUtils.h"

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

} // namespace RealTimeGraphs
