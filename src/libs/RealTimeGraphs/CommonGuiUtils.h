#ifndef COMMONGUIUTILS_H_
#define COMMONGUIUTILS_H_

// System headers
#include <string>

namespace RealTimeGraphs
{

/** @brief Convert a frequency in Hz to a human-readable string (Hz/kHz/MHz/GHz). */
[[nodiscard]] std::string formatFrequency(double freqHz);

} // namespace RealTimeGraphs

#endif // COMMONGUIUTILS_H_
