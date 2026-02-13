#include "ColorMap.h"

#include <algorithm>
#include <cmath>

namespace RealTimeGraphs
{

// ============================================================================
// Construction
// ============================================================================

ColorMap::ColorMap(Palette palette)
{
   switch (palette)
   {
      case Palette::Viridis:
         buildViridis();
         break;
      case Palette::Inferno:
         buildInferno();
         break;
      case Palette::Jet:
         buildJet();
         break;
      case Palette::Grayscale:
         buildGrayscale();
         break;
      case Palette::Turbo:
         buildTurbo();
         break;
      case Palette::RoyGB:
         buildRoyGB();
         break;
   }
   _name = paletteName(palette);
}

// ============================================================================
// Public API
// ============================================================================

Color ColorMap::map(float t) const
{
   t = std::clamp(t, 0.0F, 1.0F);
   auto index = static_cast<std::size_t>(t * 255.0F);
   index = std::min(index, std::size_t{255});
   return _lut[index];
}

std::size_t ColorMap::paletteCount()
{
   return 6;
}

ColorMap::Palette ColorMap::paletteAt(std::size_t index)
{
   // NOLINTBEGIN(clang-analyzer-deadcode.DeadStores)
   switch (index)
   {
      case 0: return Palette::Viridis;
      case 1: return Palette::Inferno;
      case 2: return Palette::Jet;
      case 3: return Palette::Grayscale;
      case 4: return Palette::Turbo;
      case 5: return Palette::RoyGB;
      default: return Palette::Viridis;
   }
   // NOLINTEND(clang-analyzer-deadcode.DeadStores)
}

std::string ColorMap::paletteName(Palette palette)
{
   // NOLINTBEGIN(clang-analyzer-deadcode.DeadStores)
   switch (palette)
   {
      case Palette::Viridis:   return "Viridis";
      case Palette::Inferno:   return "Inferno";
      case Palette::Jet:       return "Jet";
      case Palette::Grayscale: return "Grayscale";
      case Palette::Turbo:     return "Turbo";
      case Palette::RoyGB:     return "RoyGB";
   }
   return "Unknown";
   // NOLINTEND(clang-analyzer-deadcode.DeadStores)
}

// ============================================================================
// Gradient builder
// ============================================================================

Color ColorMap::lerp(const Color& a, const Color& b, float t)
{
   auto mix = [t](uint8_t x, uint8_t y) -> uint8_t
   {
      return static_cast<uint8_t>(
         std::round((static_cast<float>(x) * (1.0F - t)) +
                    (static_cast<float>(y) * t)));
   };
   return Color{.r = mix(a.r, b.r), .g = mix(a.g, b.g), .b = mix(a.b, b.b),
                .a = mix(a.a, b.a)};
}

void ColorMap::buildGradient(const std::vector<std::pair<float, Color>>& stops)
{
   if (stops.size() < 2)
   {
      return;
   }

   for (std::size_t i = 0; i < 256; ++i)
   {
      const float t = static_cast<float>(i) / 255.0F;

      // Find surrounding stops
      std::size_t upper = 1;
      while (upper < stops.size() - 1 && stops[upper].first < t)
      {
         ++upper;
      }
      const std::size_t lower = upper - 1;

      const float segLen = stops[upper].first - stops[lower].first;
      const float localT = (segLen > 0.0F) ? (t - stops[lower].first) / segLen : 0.0F;
      _lut[i] = lerp(stops[lower].second, stops[upper].second, localT);
   }
}

// ============================================================================
// Palette builders
// ============================================================================

void ColorMap::buildViridis()
{
   // Simplified Viridis with key stops
   buildGradient({
      {0.00F, {68,   1,  84, 255}},
      {0.13F, {72,  35, 116, 255}},
      {0.25F, {64,  67, 135, 255}},
      {0.38F, {52,  94, 141, 255}},
      {0.50F, {33, 144, 140, 255}},
      {0.63F, {53, 183, 121, 255}},
      {0.75F, {109, 205, 89, 255}},
      {0.88F, {180, 222, 44, 255}},
      {1.00F, {253, 231, 37, 255}},
   });
}

void ColorMap::buildInferno()
{
   buildGradient({
      {0.00F, {0,    0,   4,   255}},
      {0.14F, {40,  11,   84,  255}},
      {0.29F, {101, 21,   110, 255}},
      {0.43F, {159, 42,   99,  255}},
      {0.57F, {212, 72,   66,  255}},
      {0.71F, {245, 125,  21,  255}},
      {0.86F, {250, 193,  39,  255}},
      {1.00F, {252, 255,  164, 255}},
   });
}

void ColorMap::buildJet()
{
   buildGradient({
      {0.00F, {0,   0,   127, 255}},
      {0.11F, {0,   0,   255, 255}},
      {0.35F, {0,   255, 255, 255}},
      {0.50F, {0,   255, 0,   255}},
      {0.65F, {255, 255, 0,   255}},
      {0.89F, {255, 0,   0,   255}},
      {1.00F, {127, 0,   0,   255}},
   });
}

void ColorMap::buildGrayscale()
{
   buildGradient({
      {0.0F, {0,   0,   0,   255}},
      {1.0F, {255, 255, 255, 255}},
   });
}

void ColorMap::buildTurbo()
{
   buildGradient({
      {0.00F, {48,  18,  59,  255}},
      {0.07F, {69,  55,  129, 255}},
      {0.14F, {66,  100, 190, 255}},
      {0.21F, {35,  141, 227, 255}},
      {0.29F, {7,   178, 222, 255}},
      {0.36F, {24,  207, 174, 255}},
      {0.43F, {78,  226, 120, 255}},
      {0.50F, {149, 237, 67,  255}},
      {0.57F, {202, 237, 36,  255}},
      {0.64F, {241, 213, 19,  255}},
      {0.71F, {254, 176, 10,  255}},
      {0.79F, {249, 131, 6,   255}},
      {0.86F, {225, 85,  5,   255}},
      {0.93F, {184, 43,  4,   255}},
      {1.00F, {122, 4,   3,   255}},
   });
}
 void ColorMap::buildRoyGB()
 {
      buildGradient({
         {0.00F, {0,   0,   255, 255}}, // Blue
         {0.33F, {0,   255, 0,   255}}, // Green
         {0.66F, {255, 255, 0,   255}}, // Yellow
         {1.00F, {255, 0,   0,   255}}, // Red
      });
 }

} // namespace RealTimeGraphs
