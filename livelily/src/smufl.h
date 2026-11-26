#ifndef SMUFL_H
#define SMUFL_H

#include <string>

// the glyphs of the score in a SMuFL font (Standard Music Font Layout, see https://w3c.github.io/smufl/), Bravura (fonts/Bravura.otf)
// by their code points, as UTF-8 strings, with the names SMuFL gives them
// a SMuFL font is drawn at four staff spaces to the em, with noteheads and the like centered on the baseline
namespace Smufl {

// a code point as UTF-8 (SMuFL's glyphs are in the Private Use Area, U+E000 to U+F8FF, three bytes each)
inline std::string glyph(char32_t c)
{
	std::string s;
	if (c < 0x80) s += (char)c;
	else if (c < 0x800) {
		s += (char)(0xC0 | (c >> 6));
		s += (char)(0x80 | (c & 0x3F));
	}
	else {
		s += (char)(0xE0 | (c >> 12));
		s += (char)(0x80 | ((c >> 6) & 0x3F));
		s += (char)(0x80 | (c & 0x3F));
	}
	return s;
}

// the font size at which the glyphs fit staves whose lines are staffSpace apart
inline int fontSize(float staffSpace) { return (int)(staffSpace * 4 + 0.5f); }

// clefs
inline const std::string gClef = glyph(0xE050);
inline const std::string cClef = glyph(0xE05C);
inline const std::string fClef = glyph(0xE062);
// time signatures: the digits 0 to 9 (timeSig0 to timeSig9), common time and cut time
inline std::string timeSigNumber(int number)
{
	std::string s, digits = std::to_string(number);
	for (char d : digits) s += (d >= '0' && d <= '9' ? glyph(0xE080 + (d - '0')) : std::string(1, d));
	return s;
}
inline const std::string timeSigCommon = glyph(0xE08A);
inline const std::string timeSigCutCommon = glyph(0xE08B);
// noteheads and flags
inline const std::string noteheadWhole = glyph(0xE0A2);
inline const std::string noteheadHalf = glyph(0xE0A3);
inline const std::string noteheadBlack = glyph(0xE0A4);
inline const std::string flag8thUp = glyph(0xE240);
inline const std::string flag8thDown = glyph(0xE241);
// the flag of a note with a number of flags (1 for the 8th, 2 for the 16th, up to 8 for the 1024th), with its stem up or down
// (flag8thUp, flag8thDown, flag16thUp, flag16thDown, and so on)
inline std::string flag(int numFlags, bool stemUp)
{
	if (numFlags < 1) numFlags = 1;
	if (numFlags > 8) numFlags = 8;
	return glyph(0xE240 + (2 * (numFlags - 1)) + (stemUp ? 0 : 1));
}
// accidentals, with the quarter tones of Stein and Zimmermann
inline const std::string accidentalDoubleFlat = glyph(0xE264);
inline const std::string accidentalThreeQuarterTonesFlatZimmermann = glyph(0xE281);
inline const std::string accidentalFlat = glyph(0xE260);
inline const std::string accidentalQuarterToneFlatStein = glyph(0xE280);
inline const std::string accidentalNatural = glyph(0xE261);
inline const std::string accidentalQuarterToneSharpStein = glyph(0xE282);
inline const std::string accidentalSharp = glyph(0xE262);
inline const std::string accidentalThreeQuarterTonesSharpStein = glyph(0xE283);
inline const std::string accidentalDoubleSharp = glyph(0xE263);
// rests
inline const std::string restWhole = glyph(0xE4E3);
inline const std::string restHalf = glyph(0xE4E4);
inline const std::string restQuarter = glyph(0xE4E5);
inline const std::string rest8th = glyph(0xE4E6);
inline const std::string rest16th = glyph(0xE4E7);
inline const std::string rest32nd = glyph(0xE4E8);
inline const std::string rest64th = glyph(0xE4E9);
// dynamics
inline const std::string dynamicPPP = glyph(0xE52A);
inline const std::string dynamicPP = glyph(0xE52B);
inline const std::string dynamicPiano = glyph(0xE520);
inline const std::string dynamicMP = glyph(0xE52C);
inline const std::string dynamicMF = glyph(0xE52D);
inline const std::string dynamicForte = glyph(0xE522);
inline const std::string dynamicFF = glyph(0xE52F);
inline const std::string dynamicFFF = glyph(0xE530);
// articulations and the trill
inline const std::string articMarcatoAbove = glyph(0xE4AC);
inline const std::string ornamentTrill = glyph(0xE566);
inline const std::string articTenutoAbove = glyph(0xE4A4);
inline const std::string articStaccatissimoAbove = glyph(0xE4A6);
inline const std::string articAccentAbove = glyph(0xE4A0);
inline const std::string articStaccatoAbove = glyph(0xE4A2);
// octave lines
inline const std::string ottava = glyph(0xE510);
inline const std::string quindicesima = glyph(0xE514);
// the notes of a metronome mark (a tempo), with their stems up
inline const std::string metNoteWhole = glyph(0xECA2);
inline const std::string metNoteHalfUp = glyph(0xECA3);
inline const std::string metNoteQuarterUp = glyph(0xECA5);
inline const std::string metNote8thUp = glyph(0xECA7);
inline const std::string metNote16thUp = glyph(0xECA9);
inline const std::string metNote32ndUp = glyph(0xECAB);
inline const std::string metNote64thUp = glyph(0xECAD);

}

#endif
