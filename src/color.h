#ifndef COLOR_H
#define COLOR_H

#include <cstdint>

// a color of LiveLily (red, green, blue, and alpha, from 0 to 255), for the colors of the code in the editor
// the colors of instruments, and the colors that can be set by name (see \score.colors and \instname.color)
// it is converted to the color type of the front end only where things are drawn
struct Color
{
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 255;
	constexpr Color() = default;
	constexpr Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}
	bool operator==(const Color& other) const { return r == other.r && g == other.g && b == other.b && a == other.a; }
	bool operator!=(const Color& other) const { return !(*this == other); }
};

// a color between two, as openFrameworks' ofColor::getLerped() computes it (every channel truncated to a whole value)
inline Color lerp(const Color& from, const Color& to, float amount)
{
	float inv = 1.f - amount;
	return Color((uint8_t)(inv * from.r + amount * to.r), (uint8_t)(inv * from.g + amount * to.g),
			(uint8_t)(inv * from.b + amount * to.b), (uint8_t)(inv * from.a + amount * to.a));
}

// the named colors, with the same names and values as those of openFrameworks (the colors of CSS, as openFrameworks computes them)
namespace Colors {
	inline constexpr Color gray{127, 127, 127};
	inline constexpr Color white{255, 255, 255};
	inline constexpr Color red{255, 0, 0};
	inline constexpr Color green{0, 255, 0};
	inline constexpr Color blue{0, 0, 255};
	inline constexpr Color cyan{0, 255, 255};
	inline constexpr Color magenta{255, 0, 255};
	inline constexpr Color yellow{255, 255, 0};
	inline constexpr Color black{0, 0, 0};
	inline constexpr Color aliceBlue{239, 248, 255};
	inline constexpr Color antiqueWhite{249, 235, 214};
	inline constexpr Color aqua{0, 255, 255};
	inline constexpr Color aquamarine{126, 255, 212};
	inline constexpr Color azure{239, 255, 255};
	inline constexpr Color beige{244, 244, 219};
	inline constexpr Color bisque{255, 228, 195};
	inline constexpr Color blanchedAlmond{255, 235, 205};
	inline constexpr Color blueViolet{137, 42, 226};
	inline constexpr Color brown{165, 42, 42};
	inline constexpr Color burlyWood{221, 184, 135};
	inline constexpr Color cadetBlue{94, 158, 160};
	inline constexpr Color chartreuse{126, 255, 0};
	inline constexpr Color chocolate{209, 105, 29};
	inline constexpr Color coral{255, 126, 80};
	inline constexpr Color cornflowerBlue{100, 149, 237};
	inline constexpr Color cornsilk{255, 248, 219};
	inline constexpr Color crimson{219, 20, 59};
	inline constexpr Color darkBlue{0, 0, 138};
	inline constexpr Color darkCyan{0, 138, 138};
	inline constexpr Color darkGoldenRod{184, 133, 11};
	inline constexpr Color darkGray{168, 168, 168};
	inline constexpr Color darkGrey{168, 168, 168};
	inline constexpr Color darkGreen{0, 100, 0};
	inline constexpr Color darkKhaki{188, 182, 107};
	inline constexpr Color darkMagenta{138, 0, 138};
	inline constexpr Color darkOliveGreen{84, 107, 47};
	inline constexpr Color darkOrange{255, 140, 0};
	inline constexpr Color darkOrchid{153, 49, 204};
	inline constexpr Color darkRed{138, 0, 0};
	inline constexpr Color darkSalmon{232, 149, 121};
	inline constexpr Color darkSeaGreen{142, 188, 142};
	inline constexpr Color darkSlateBlue{72, 61, 138};
	inline constexpr Color darkSlateGray{47, 79, 79};
	inline constexpr Color darkSlateGrey{47, 79, 79};
	inline constexpr Color darkTurquoise{0, 205, 209};
	inline constexpr Color darkViolet{147, 0, 211};
	inline constexpr Color deepPink{255, 20, 147};
	inline constexpr Color deepSkyBlue{0, 191, 255};
	inline constexpr Color dimGray{105, 105, 105};
	inline constexpr Color dimGrey{105, 105, 105};
	inline constexpr Color dodgerBlue{29, 144, 255};
	inline constexpr Color fireBrick{177, 33, 33};
	inline constexpr Color floralWhite{255, 249, 239};
	inline constexpr Color forestGreen{33, 138, 33};
	inline constexpr Color fuchsia{255, 0, 255};
	inline constexpr Color gainsboro{219, 219, 219};
	inline constexpr Color ghostWhite{248, 248, 255};
	inline constexpr Color gold{255, 214, 0};
	inline constexpr Color goldenRod{218, 165, 31};
	inline constexpr Color grey{128, 128, 128};
	inline constexpr Color greenYellow{172, 255, 47};
	inline constexpr Color honeyDew{239, 255, 239};
	inline constexpr Color hotPink{255, 105, 179};
	inline constexpr Color indianRed{205, 91, 91};
	inline constexpr Color indigo{75, 0, 130};
	inline constexpr Color ivory{255, 255, 239};
	inline constexpr Color khaki{239, 230, 140};
	inline constexpr Color lavender{230, 230, 249};
	inline constexpr Color lavenderBlush{255, 239, 244};
	inline constexpr Color lawnGreen{124, 251, 0};
	inline constexpr Color lemonChiffon{255, 249, 205};
	inline constexpr Color lightBlue{172, 216, 230};
	inline constexpr Color lightCoral{239, 128, 128};
	inline constexpr Color lightCyan{223, 255, 255};
	inline constexpr Color lightGoldenRodYellow{249, 249, 209};
	inline constexpr Color lightGray{211, 211, 211};
	inline constexpr Color lightGrey{211, 211, 211};
	inline constexpr Color lightGreen{144, 237, 144};
	inline constexpr Color lightPink{255, 182, 193};
	inline constexpr Color lightSalmon{255, 160, 121};
	inline constexpr Color lightSeaGreen{31, 177, 170};
	inline constexpr Color lightSkyBlue{135, 205, 249};
	inline constexpr Color lightSlateGray{119, 135, 153};
	inline constexpr Color lightSlateGrey{119, 135, 153};
	inline constexpr Color lightSteelBlue{175, 195, 221};
	inline constexpr Color lightYellow{255, 255, 223};
	inline constexpr Color lime{0, 255, 0};
	inline constexpr Color limeGreen{49, 205, 49};
	inline constexpr Color linen{249, 239, 230};
	inline constexpr Color maroon{128, 0, 0};
	inline constexpr Color mediumAquaMarine{102, 205, 170};
	inline constexpr Color mediumBlue{0, 0, 205};
	inline constexpr Color mediumOrchid{186, 84, 211};
	inline constexpr Color mediumPurple{147, 112, 219};
	inline constexpr Color mediumSeaGreen{59, 179, 112};
	inline constexpr Color mediumSlateBlue{123, 103, 237};
	inline constexpr Color mediumSpringGreen{0, 249, 154};
	inline constexpr Color mediumTurquoise{72, 209, 204};
	inline constexpr Color mediumVioletRed{198, 20, 133};
	inline constexpr Color midnightBlue{24, 24, 112};
	inline constexpr Color mintCream{244, 255, 249};
	inline constexpr Color mistyRose{255, 228, 225};
	inline constexpr Color moccasin{255, 228, 181};
	inline constexpr Color navajoWhite{255, 221, 172};
	inline constexpr Color navy{0, 0, 128};
	inline constexpr Color oldLace{253, 244, 230};
	inline constexpr Color olive{128, 128, 0};
	inline constexpr Color oliveDrab{107, 142, 35};
	inline constexpr Color orange{255, 165, 0};
	inline constexpr Color orangeRed{255, 68, 0};
	inline constexpr Color orchid{218, 112, 214};
	inline constexpr Color paleGoldenRod{237, 232, 170};
	inline constexpr Color paleGreen{151, 251, 151};
	inline constexpr Color paleTurquoise{175, 237, 237};
	inline constexpr Color paleVioletRed{219, 112, 147};
	inline constexpr Color papayaWhip{255, 239, 212};
	inline constexpr Color peachPuff{255, 218, 184};
	inline constexpr Color peru{205, 133, 63};
	inline constexpr Color pink{255, 191, 202};
	inline constexpr Color plum{221, 160, 221};
	inline constexpr Color powderBlue{175, 223, 230};
	inline constexpr Color purple{128, 0, 128};
	inline constexpr Color rosyBrown{188, 142, 142};
	inline constexpr Color royalBlue{65, 105, 225};
	inline constexpr Color saddleBrown{138, 68, 18};
	inline constexpr Color salmon{249, 128, 114};
	inline constexpr Color sandyBrown{244, 163, 96};
	inline constexpr Color seaGreen{45, 138, 86};
	inline constexpr Color seaShell{255, 244, 237};
	inline constexpr Color sienna{160, 82, 45};
	inline constexpr Color silver{191, 191, 191};
	inline constexpr Color skyBlue{135, 205, 235};
	inline constexpr Color slateBlue{105, 89, 205};
	inline constexpr Color slateGray{112, 128, 144};
	inline constexpr Color slateGrey{112, 128, 144};
	inline constexpr Color snow{255, 249, 249};
	inline constexpr Color springGreen{0, 255, 126};
	inline constexpr Color steelBlue{70, 130, 179};
	inline constexpr Color blueSteel{70, 130, 179};
	inline constexpr Color tan{209, 179, 140};
	inline constexpr Color teal{0, 128, 128};
	inline constexpr Color thistle{216, 191, 216};
	inline constexpr Color tomato{255, 98, 70};
	inline constexpr Color turquoise{63, 223, 207};
	inline constexpr Color violet{237, 130, 237};
	inline constexpr Color wheat{244, 221, 179};
	inline constexpr Color whiteSmoke{244, 244, 244};
	inline constexpr Color yellowGreen{154, 205, 49};
}

#endif
