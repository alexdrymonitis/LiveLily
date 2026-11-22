#ifndef OF_COLOR_H
#define OF_COLOR_H

#include "ofColor.h"
#include "color.h"

// conversions between the colors of LiveLily (see color.h) and those of openFrameworks, for the code that draws with openFrameworks
inline ofColor toOf(const Color& c)
{
	return ofColor(c.r, c.g, c.b, c.a);
}

inline Color toColor(const ofColor& c)
{
	return Color(c.r, c.g, c.b, c.a);
}

#endif
