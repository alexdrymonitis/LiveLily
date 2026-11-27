#include "keys.h"
#include <cctype>
#include <map>

namespace {

void tap(std::vector<KeyEvent>& events, int key, const std::vector<int>& modifiers)
{
	for (int modifier : modifiers) events.push_back({modifier, true});
	events.push_back({key, true});
	events.push_back({key, false});
	for (auto it = modifiers.rbegin(); it != modifiers.rend(); ++it) events.push_back({*it, false});
}

bool typedWithShift(char c)
{
	static const std::string shifted = "~!@#$%^&*()_+{}|:\"<>?";
	return std::isupper((unsigned char)c) || shifted.find(c) != std::string::npos;
}

}

std::vector<KeyEvent> parseKeys(const std::string& notation, std::string& error)
{
	static const std::map<std::string, int> named = {
		{"esc", Key::escape}, {"cr", Key::enter}, {"return", Key::enter}, {"enter", Key::enter}, {"bs", Key::backspace},
		{"tab", Key::tab}, {"del", Key::del}, {"space", Key::space}, {"lt", '<'}, {"left", Key::left}, {"right", Key::right},
		{"up", Key::up}, {"down", Key::down}, {"pageup", Key::pageUp}, {"pagedown", Key::pageDown}, {"home", Key::home}, {"end", Key::end},
	};
	std::vector<KeyEvent> events;
	error.clear();
	size_t i = 0;
	while (i < notation.size()) {
		char c = notation[i];
		size_t close = (c == '<' ? notation.find('>', i) : std::string::npos);
		if (close == std::string::npos) {
			tap(events, (unsigned char)c, typedWithShift(c) ? std::vector<int>{Key::shift} : std::vector<int>{});
			i++;
			continue;
		}
		std::string inside = notation.substr(i + 1, close - i - 1);
		std::vector<int> modifiers;
		// the modifiers, e.g. C-S- in <C-S-Left>
		while (inside.size() > 2 && inside[1] == '-') {
			char m = (char)std::toupper((unsigned char)inside[0]);
			if (m == 'S') modifiers.push_back(Key::shift);
			else if (m == 'C') modifiers.push_back(Key::control);
			else if (m == 'A' || m == 'M') modifiers.push_back(Key::alt);
			else break;
			inside = inside.substr(2);
		}
		std::string lower;
		for (char ch : inside) lower += (char)std::tolower((unsigned char)ch);
		// a modifier pressed or released alone, held across the keys between, e.g. <CtrlDown>==<CtrlUp>
		static const std::map<std::string, KeyEvent> holds = {
			{"shiftdown", {Key::shift, true}}, {"shiftup", {Key::shift, false}}, {"ctrldown", {Key::control, true}},
			{"ctrlup", {Key::control, false}}, {"altdown", {Key::alt, true}}, {"altup", {Key::alt, false}},
		};
		auto hold = holds.find(lower);
		auto it = named.find(lower);
		if (hold != holds.end() && modifiers.empty()) {
			events.push_back(hold->second);
		}
		else if (it != named.end()) {
			tap(events, it->second, modifiers);
		}
		else if (inside.size() == 1) {
			// a character with modifiers, e.g. <C-v>
			tap(events, (unsigned char)inside[0], modifiers);
		}
		else {
			error = "unknown key <" + notation.substr(i + 1, close - i - 1) + ">";
			return {};
		}
		i = close + 1;
	}
	return events;
}
