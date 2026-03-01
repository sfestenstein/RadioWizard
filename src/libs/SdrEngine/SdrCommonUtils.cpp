// Project headers
#include "SdrCommonUtils.h"

// System headers
#include <algorithm>

namespace SdrEngine
{

std::vector<float> decimateSpectrum(
   const std::vector<float>& magnitudesDb,
   std::size_t maxBins)
{
   if (magnitudesDb.size() <= maxBins || maxBins == 0)
   {
      return magnitudesDb;
   }

   std::vector<float> decimated;
   decimated.reserve(maxBins);

   const std::size_t binSize = magnitudesDb.size() / maxBins;
   const std::size_t remainder = magnitudesDb.size() % maxBins;

   std::size_t srcIdx = 0;
   for (std::size_t i = 0; i < maxBins; ++i)
   {
      // Distribute remainder across bins to handle non-divisible sizes.
      const std::size_t currentBinSize = binSize + (i < remainder ? 1 : 0);

      float maxVal = magnitudesDb[srcIdx];
      for (std::size_t j = 1; j < currentBinSize; ++j)
      {
         maxVal = std::max(maxVal, magnitudesDb[srcIdx + j]);
      }

      decimated.push_back(maxVal);
      srcIdx += currentBinSize;
   }

   return decimated;
}

} // namespace SdrEngine
