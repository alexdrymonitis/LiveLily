#ifndef KEYS_H
#define KEYS_H

// the keys of LiveLily, as the editor and the app take them: a printable character is its ASCII code, and the other keys have the codes below
// these are the codes of openFrameworks, which the app has always used, and which go to other machines too (\sendkeys, \fromosc)
// and come from Python (see \pystr), so a front end makes key events with these codes, whatever its own codes are
// the modifiers are keys of their own, pressed and released around the keys they modify (e.g. Shift+Return is
// shift pressed, return pressed, return released, shift released), and the app keeps their state

#include <string>
#include <vector>

namespace Key {
	constexpr int shift = 0x1;
	constexpr int control = 0x2;
	constexpr int alt = 0x4;
	constexpr int backspace = 8;
	constexpr int tab = 9;
	constexpr int enter = 13;
	constexpr int escape = 27;
	constexpr int space = 32;
	constexpr int del = 127;
	constexpr int f1 = 0xe000; // to f12 = 0xe00b
	constexpr int left = 0xe00c;
	constexpr int up = 0xe00d;
	constexpr int right = 0xe00e;
	constexpr int down = 0xe00f;
	constexpr int pageUp = 0xe010;
	constexpr int pageDown = 0xe011;
	constexpr int home = 0xe012;
	constexpr int end = 0xe013;
	constexpr int insert = 0xe014;
}

struct KeyEvent
{
	int key = 0;
	bool press = true;
};

// key events from a notation like Vim's: a character is pressed and released, a capital letter or a symbol typed with Shift
// on a US keyboard (e.g. $) with Shift around it, and a key in angle brackets is a named key with modifiers before it
// e.g. <Esc>, <CR> (or <Return>), <BS>, <Tab>, <Del>, <Space>, <Left>, <Right>, <Up>, <Down>, <PageUp>, <PageDown>, <Home>, <End>
// <lt> for <, and <S-CR>, <C-v>, <A-x> or <C-S-Left> with Shift, Control or Alt
// and <ShiftDown>, <ShiftUp>, <CtrlDown>, <CtrlUp>, <AltDown>, <AltUp> for a modifier held across keys (e.g. <CtrlDown>==<CtrlUp>)
// returns an empty vector and an error message for a key it doesn't know
std::vector<KeyEvent> parseKeys(const std::string& notation, std::string& error);

#endif
