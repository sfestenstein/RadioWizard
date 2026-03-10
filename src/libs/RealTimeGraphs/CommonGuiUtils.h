#ifndef COMMONGUIUTILS_H_
#define COMMONGUIUTILS_H_

// System headers
#include <string>
#include <QWidget>
namespace RealTimeGraphs
{

/** @brief Convert a frequency in Hz to a human-readable string (Hz/kHz/MHz/GHz). */
[[nodiscard]] std::string formatFrequency(double freqHz);

/** @brief Safely trigger a QWidget update from any thread. */
void safeUpdate(QWidget* widget);

} // namespace RealTimeGraphs

#endif // COMMONGUIUTILS_H_
