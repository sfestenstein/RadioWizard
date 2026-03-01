#ifndef SDRCOMMONUTILS_H_
#define SDRCOMMONUTILS_H_

// System headers
#include <cstddef>
#include <vector>

namespace SdrEngine
{

/**
 * @brief Decimate a spectrum to a target number of bins, preserving peak values.
 *
 * When the input has more bins than @p maxBins, groups of consecutive bins are
 * merged by taking the maximum value in each group.  The remainder bins are
 * distributed evenly across the first groups so no data is lost.
 *
 * If the input already has @p maxBins or fewer bins, it is returned unchanged.
 *
 * @param magnitudesDb  Input spectrum magnitudes (typically in dB).
 * @param maxBins       Maximum number of output bins (default 2048).
 * @return Decimated spectrum with at most @p maxBins entries.
 */
[[nodiscard]] std::vector<float> decimateSpectrum(
   const std::vector<float>& magnitudesDb,
   std::size_t maxBins = 2048);

} // namespace SdrEngine

#endif // SDRCOMMONUTILS_H_
