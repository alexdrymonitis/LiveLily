#ifndef X11_KEYS_H
#define X11_KEYS_H

// whether Alt is held now, as X tells it (1 or 0), or -1 where X isn't there (macOS, Windows, or no connection to X)
// JUCE follows Alt on Linux only by the presses and releases of its keys, and asks X only for Shift and Ctrl
// (juce::ModifierKeys::getCurrentModifiersRealtime()), so a release of Alt that goes to the window manager (Alt+Tab) leaves it held
int altHeldNow();

#endif
