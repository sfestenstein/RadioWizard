#ifndef COLORMAP_H_
#define COLORMAP_H_

// System headers
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace RealTimeGraphs
{

/// A packed RGBA color.
struct Color
{
   uint8_t r{0};
   uint8_t g{0};
   uint8_t b{0};
   uint8_t a{255};
};

/// Pre-built color maps for spectrum and waterfall displays.
///
/// Each map is a 256-entry lookup table (LUT).  Call `map(t)` with a
/// normalised value in [0, 1] to retrieve the corresponding color.
class ColorMap
{
public:
   /// Built-in palette names.
   enum class Palette : std::uint8_t
   {
      Viridis,     ///< Perceptually uniform, blue-green-yellow
      Inferno,     ///< Perceptually uniform, black-red-yellow-white
      Jet,         ///< Classic rainbow (not perceptually uniform)
      Grayscale,   ///< Black to white
      Turbo,       ///< Improved rainbow, Google
      RoyGB        ///< Red-Orange-Yellow-Green-Blue
   };

   /// Build a LUT for the requested palette.
   explicit ColorMap(Palette palette = Palette::Viridis);

   /// Look up a color for a normalised value in [0, 1].
   /// Values outside the range are clamped.
   [[nodiscard]] Color map(float t) const;

   /// Direct access to the 256-entry LUT.
   [[nodiscard]] const std::array<Color, 256>& lut() const { return _lut; }

   /// Human-readable name of the active palette.
   [[nodiscard]] const std::string& name() const { return _name; }

   /// Number of available built-in palettes.
   [[nodiscard]] static std::size_t paletteCount();

   /// Get palette enum by index (for UI combo-box population).
   [[nodiscard]] static Palette paletteAt(std::size_t index);

   /// Get human-readable name of a palette.
   [[nodiscard]] static std::string paletteName(Palette palette);

private:
   void buildViridis();
   void buildInferno();
   void buildJet();
   void buildGrayscale();
   void buildTurbo();
   void buildRoyGB();
   /// Linearly interpolate between two colors.
   [[nodiscard]] static Color lerp(const Color& a, const Color& b, float t);

   /// Build a gradient LUT from a list of key-color stops.
   void buildGradient(const std::vector<std::pair<float, Color>>& stops);

   std::array<Color, 256> _lut{};
   std::string _name;
};

} // namespace RealTimeGraphs

#endif // COLORMAP_H_
