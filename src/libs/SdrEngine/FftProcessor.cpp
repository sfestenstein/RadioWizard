// Project headers
#include "FftProcessor.h"
#include "GeneralLogger.h"

// Third-party headers
#include <fftw3.h>

// System headers
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <utility>

namespace SdrEngine
{

// ============================================================================
// Window-function helpers
// ============================================================================

namespace
{

void fillRectangular(std::vector<double>& w, size_t n)
{
   w.assign(static_cast<std::size_t>(n), 1.0);
}

void fillHanning(std::vector<double>& w, size_t n)
{
   w.resize(static_cast<std::size_t>(n));
   for (size_t i = 0; i < n; ++i)
   {
      const double temp = static_cast<double>(i) / static_cast<double>(n - 1);
      w[i] = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * temp));
   }
}

void fillBlackmanHarris(std::vector<double>& w, size_t n)
{
   w.resize(static_cast<std::size_t>(n));
   constexpr double A0 = 0.35875;
   constexpr double A1 = 0.48829;
   constexpr double A2 = 0.14128;
   constexpr double A3 = 0.01168;
   for (size_t i = 0; i < n; ++i)
   {
      const double temp = static_cast<double>(i) / static_cast<double>(n - 1);
      const double x = 2.0 * std::numbers::pi * temp;
      w[i] = A0 - (A1 * std::cos(x)) + (A2 * std::cos(2.0 * x)) - (A3 * std::cos(3.0 * x));
   }
}

void fillFlatTop(std::vector<double>& w, size_t n)
{
   w.resize(static_cast<std::size_t>(n));
   constexpr double A0 = 0.21557895;
   constexpr double A1 = 0.41663158;
   constexpr double A2 = 0.277263158;
   constexpr double A3 = 0.083578947;
   constexpr double A4 = 0.006947368;
   for (size_t i = 0; i < n; ++i)
   {
      const double temp = static_cast<double>(i) / static_cast<double>(n - 1);
      const double x = 2.0 * std::numbers::pi * temp;
      w[i] = A0 - (A1 * std::cos(x)) + (A2 * std::cos(2.0 * x))
                - (A3 * std::cos(3.0 * x)) + (A4 * std::cos(4.0 * x));
   }
}

} // anonymous namespace

// ============================================================================
// Construction / destruction
// ============================================================================

FftProcessor::FftProcessor(size_t fftSize, WindowFunction windowFunc)
   : _fftSize{fftSize}
   , _windowFunc{windowFunc}
{
   rebuild();
}

FftProcessor::~FftProcessor()
{
   destroy();
}

FftProcessor::FftProcessor(FftProcessor&& other) noexcept
   : _fftSize{other._fftSize}
   , _windowFunc{other._windowFunc}
   , _in{other._in}
   , _out{other._out}
   , _plan{other._plan}
   , _window{std::move(other._window)}
{
   other._in   = nullptr;
   other._out  = nullptr;
   other._plan = nullptr;
}

FftProcessor& FftProcessor::operator=(FftProcessor&& other) noexcept
{
   if (this != &other)
   {
      destroy();
      _fftSize    = other._fftSize;
      _windowFunc = other._windowFunc;
      _in         = other._in;
      _out        = other._out;
      _plan       = other._plan;
      _window     = std::move(other._window);
      other._in   = nullptr;
      other._out  = nullptr;
      other._plan = nullptr;
   }
   return *this;
}

// ============================================================================
// Configuration
// ============================================================================

void FftProcessor::setFftSize(size_t fftSize)
{
   const std::lock_guard<std::mutex> lock(_mutex);
   if (fftSize == _fftSize)
   {
      return;
   }
   _fftSize = fftSize;
   rebuild();
}

size_t FftProcessor::getFftSize() const
{
   const std::lock_guard<std::mutex> lock(_mutex);
   return _fftSize;
}

void FftProcessor::setWindowFunction(WindowFunction windowFunc)
{
   const std::lock_guard<std::mutex> lock(_mutex);
   if (windowFunc == _windowFunc)
   {
      return;
   }
   _windowFunc = windowFunc;
   buildWindow();
}

WindowFunction FftProcessor::getWindowFunction() const
{
   const std::lock_guard<std::mutex> lock(_mutex);
   return _windowFunc;
}

// ============================================================================
// Processing
// ============================================================================

std::vector<float> FftProcessor::process(
   const std::vector<std::complex<float>>& samples) const
{
   const std::lock_guard<std::mutex> lock(_mutex);
   const auto n = static_cast<std::size_t>(_fftSize);

   // Apply window and copy into FFTW input buffer (interleaved real/imag).
   const std::size_t copyLen = std::min(samples.size(), n);
   for (std::size_t i = 0; i < copyLen; ++i)
   {
      _in[2 * i]     = static_cast<double>(samples[i].real()) * _window[i];
      _in[(2 * i) + 1] = static_cast<double>(samples[i].imag()) * _window[i];
   }
   // Zero-pad if necessary.
   for (std::size_t i = copyLen; i < n; ++i)
   {
      _in[2 * i]     = 0.0;
      _in[(2 * i) + 1] = 0.0;
   }

   // Execute the FFT.
   fftw_execute(_plan);

   // Convert complex output → magnitude in dB, with DC-centring (fftshift).
   std::vector<float> magnitudesDb(n);
   const auto half = n / 2;

   // Normalisation factor — window coherent gain.
   double windowSum = 0.0;
   for (std::size_t i = 0; i < n; ++i)
   {
      windowSum += _window[i];
   }
   const double normFactor = (windowSum > 0.0) ? windowSum : 1.0;

   for (std::size_t i = 0; i < n; ++i)
   {
      const double re  = _out[2 * i];
      const double im  = _out[(2 * i) + 1];
      const double mag = std::sqrt((re * re) + (im * im)) / normFactor;

      // Guard against log10(0).
      constexpr double FLOOR = 1.0e-20;
      const auto dB = static_cast<float>(20.0 * std::log10(std::max(mag, FLOOR)));

      // fftshift: swap lower and upper halves.
      const std::size_t dst = (i + half) % n;
      magnitudesDb[dst] = dB;
   }

   return magnitudesDb;
}

// ============================================================================
// Internal helpers
// ============================================================================

void FftProcessor::rebuild()
{
   destroy();

   const auto n = static_cast<std::size_t>(_fftSize);

   // Allocate FFTW-aligned buffers (interleaved complex = 2× doubles).
   _in  = fftw_alloc_real(2 * n);
   _out = fftw_alloc_real(2 * n);

   if (_in == nullptr || _out == nullptr)
   {
      GPERROR("FFTW memory allocation failed for FFT size {}", _fftSize);
      return;
   }

   // Create a complex-to-complex plan (DFT_1D, forward).
   _plan = fftw_plan_dft_1d(
      static_cast<int>(_fftSize),
      reinterpret_cast<fftw_complex*>(_in),
      reinterpret_cast<fftw_complex*>(_out),
      FFTW_FORWARD, FFTW_MEASURE);

   buildWindow();

   GPINFO("FftProcessor: rebuilt plan for FFT size {}", _fftSize);
}

void FftProcessor::destroy()
{
   if (_plan != nullptr)
   {
      fftw_destroy_plan(_plan);
      _plan = nullptr;
   }
   if (_in != nullptr)
   {
      fftw_free(_in);
      _in = nullptr;
   }
   if (_out != nullptr)
   {
      fftw_free(_out);
      _out = nullptr;
   }
}

void FftProcessor::buildWindow()
{
   switch (_windowFunc)
   {
      case WindowFunction::Rectangular:
         fillRectangular(_window, _fftSize);
         break;
      case WindowFunction::Hanning:
         fillHanning(_window, _fftSize);
         break;
      case WindowFunction::BlackmanHarris:
         fillBlackmanHarris(_window, _fftSize);
         break;
      case WindowFunction::FlatTop:
         fillFlatTop(_window, _fftSize);
         break;
   }
}

} // namespace SdrEngine
