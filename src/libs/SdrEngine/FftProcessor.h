#ifndef FFTPROCESSOR_H_
#define FFTPROCESSOR_H_

// Project headers
#include "SdrTypes.h"

// System headers
#include <complex>
#include <cstddef>
#include <mutex>
#include <vector>

// Forward declaration — avoids exposing fftw3.h in the header.
struct fftwf_plan_s;

namespace SdrEngine
{

/// Performs windowed FFT on complex I/Q data and produces a magnitude
/// spectrum in dB.  Uses FFTW for the heavy lifting.
///
   /// Thread-safety: all public methods are protected by an internal mutex,
/// so setFftSize / setWindowFunction can be called from the GUI thread
/// while process() runs on the SdrEngine processing thread.
class FftProcessor
{
public:
   /// @param fftSize      Number of FFT bins (must be power of two).
   /// @param windowFunc   Windowing function to apply before the FFT.
   explicit FftProcessor(size_t fftSize = 2048,
                         WindowFunction windowFunc = WindowFunction::BlackmanHarris);

   ~FftProcessor();

   // Non-copyable.
   FftProcessor(const FftProcessor&) = delete;
   FftProcessor& operator=(const FftProcessor&) = delete;

   // Movable.
   FftProcessor(FftProcessor&& other) noexcept;
   FftProcessor& operator=(FftProcessor&& other) noexcept;

   /// Change the FFT size.  Rebuilds internal FFTW plan.
   void setFftSize(size_t fftSize);

   /// @return The current FFT size.
   [[nodiscard]] size_t getFftSize() const;

   /// Change the windowing function.
   void setWindowFunction(WindowFunction windowFunc);

   /// @return The current windowing function.
   [[nodiscard]] WindowFunction getWindowFunction() const;

   /// Compute the magnitude spectrum from a block of I/Q samples.
   ///
   /// If `samples` has fewer elements than the FFT size the remainder is
   /// zero-padded.  If it has more, only the first `fftSize` samples are
   /// used.
   ///
   /// @param samples  Complex float I/Q data.
   /// @return Magnitude spectrum in dB, length == fftSize, DC-centred
   ///         (negative frequencies on the left).
   [[nodiscard]] std::vector<float> process(
      const std::vector<std::complex<float>>& samples) const;

private:
   /// (Re-)create the FFTW plan and window coefficients.
   void rebuild();

   /// Destroy the current FFTW plan and free aligned buffers.
   void destroy();

   /// Populate _window with the selected function.
   void buildWindow();

   mutable std::mutex _mutex;  ///< Guards all FFTW resources and window.

   size_t _fftSize;
   WindowFunction _windowFunc;

   // FFTW resources — opaque so we don't expose fftw3.h.
   float* _in{nullptr};        ///< FFTW input  (real interleaved as pairs).
   float* _out{nullptr};       ///< FFTW output (complex interleaved).
   fftwf_plan_s* _plan{nullptr};

   std::vector<float> _window;  ///< Pre-computed window coefficients.
};

} // namespace SdrEngine

#endif // FFTPROCESSOR_H_
