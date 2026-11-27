#include "x11_keys.h"

#if defined(__linux__)
#include <X11/Xlib.h>

int altHeldNow()
{
	// a connection of its own, made once (JUCE's is private to it)
	static Display* display = XOpenDisplay(nullptr);
	if (display == nullptr) return -1;
	Window root, child;
	int rootX, rootY, x, y;
	unsigned int mask = 0;
	if (!XQueryPointer(display, DefaultRootWindow(display), &root, &child, &rootX, &rootY, &x, &y, &mask)) return -1;
	// Alt is the first of the modifiers of X (Mod1), as JUCE finds it (see updateModifierMappings() in JUCE)
	return ((mask & Mod1Mask) != 0 ? 1 : 0);
}

#else

int altHeldNow()
{
	return -1;
}

#endif
