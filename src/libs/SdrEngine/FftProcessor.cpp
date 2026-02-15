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

void fillRectangular(std::vector<float>& w, size_t n)
{
   w.assign(static_cast<std::size_t>(n), 1.0F);
}

void fillHanning(std::vector<float>& w, size_t n)
{
   w.resize(static_cast<std::size_t>(n));
   for (size_t i = 0; i < n; ++i)
   {
      const float temp = static_cast<float>(i) / static_cast<float>(n - 1);
      w[i] = 0.5F * (1.0F - std::cos(2.0F * std::numbers::pi_v<float> * temp));
   }
}

void fillBlackmanHarris(std::vector<float>& w, size_t n)
{
   w.resize(static_cast<std::size_t>(n));
   constexpr float A0 = 0.35875F;
   constexpr float A1 = 0.48829F;
   constexpr float A2 = 0.14128F;
   constexpr float A3 = 0.01168F;
   for (size_t i = 0; i < n; ++i)
   {
      const float temp = static_cast<float>(i) / static_cast<float>(n - 1);
      const float x = 2.0F * std::numbers::pi_v<float> * temp;
      w[i] = A0 - (A1 * std::cos(x)) + (A2 * std::cos(2.0F * x)) - (A3 * std::cos(3.0F * x));
   }
}

void fillFlatTop(std::vector<float>& w, size_t n)
{
   w.resize(static_cast<std::size_t>(n));
   constexpr float A0 = 0.21557895F;
   constexpr float A1 = 0.41663158F;
   constexpr float A2 = 0.277263158F;
   constexpr float A3 = 0.083578947F;
   constexpr float A4 = 0.006947368F;
   for (size_t i = 0; i < n; ++i)
   {
      const float temp = static_cast<float>(i) / static_cast<float>(n - 1);
      const float x = 2.0F * std::numbers::pi_v<float> * temp;
      w[i] = A0 - (A1 * std::cos(x)) + (A2 * std::cos(2.0F * x))
                - (A3 * std::cos(3.0F * x)) + (A4 * std::cos(4.0F * x));
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
      _in[2 * i]     = samples[i].real() * _window[i];
      _in[(2 * i) + 1] = samples[i].imag() * _window[i];
   }
   // Zero-pad if necessary.
   for (std::size_t i = copyLen; i < n; ++i)
   {
      _in[2 * i]     = 0.0F;
      _in[(2 * i) + 1] = 0.0F;
   }

   // Execute the FFT.
   fftwf_execute(_plan);

   // Convert complex output → magnitude in dB, with DC-centring (fftshift).
   std::vector<float> magnitudesDb(n);
   const auto half = n / 2;

   // Normalisation factor — window coherent gain.
   float windowSum = 0.0F;
   for (std::size_t i = 0; i < n; ++i)
   {
      windowSum += _window[i];
   }
   const float normFactor = (windowSum > 0.0F) ? windowSum : 1.0F;

   for (std::size_t i = 0; i < n; ++i)
   {
      const float re  = _out[2 * i];
      const float im  = _out[(2 * i) + 1];
      const float mag = std::sqrt((re * re) + (im * im)) / normFactor;

      // Guard against log10(0).
      constexpr float FLOOR = 1.0e-20F;
      const float dB = 20.0F * std::log10(std::max(mag, FLOOR));

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

   // Allocate FFTW-aligned buffers (interleaved complex = 2× floats).
   _in  = fftwf_alloc_real(2 * n);
   _out = fftwf_alloc_real(2 * n);

   if (_in == nullptr || _out == nullptr)
   {
      GPERROR("FFTW memory allocation failed for FFT size {}", _fftSize);
      return;
   }

   // Create a complex-to-complex plan (DFT_1D, forward).
   _plan = fftwf_plan_dft_1d(
      static_cast<int>(_fftSize),
      reinterpret_cast<fftwf_complex*>(_in),
      reinterpret_cast<fftwf_complex*>(_out),
      FFTW_FORWARD, FFTW_MEASURE);

   buildWindow();

   GPINFO("FftProcessor: rebuilt plan for FFT size {}", _fftSize);
}

void FftProcessor::destroy()
{
   if (_plan != nullptr)
   {
      fftwf_destroy_plan(_plan);
      _plan = nullptr;
   }
   if (_in != nullptr)
   {
      fftwf_free(_in);
      _in = nullptr;
   }
   if (_out != nullptr)
   {
      fftwf_free(_out);
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
