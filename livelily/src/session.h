#ifndef SESSION_H
#define SESSION_H

#include <map>
#include <string>

// a variable of a session (x = 5), with its value as a string and its type
struct variable
{
	std::string varStr;
	int varType;
};

// the engine of LiveLily: the state of a session (and later the parser and the sequencer), without anything of the user interface
// (see doc/stage1_inventory.md, step 5)
// while the state moves here group by group, ofApp derives from Session, so the code that uses a moved member doesn't change
// when the parser has moved here too, ofApp will hold a Session instead of deriving from it
class Session
{
	public:
		virtual ~Session() = default;

		// variables (e.g. x = 5), by their names
		std::map<std::string, variable> variables;
		// the kinds of definitions, for checking that a name is free (see checkNameAvailability())
		std::map<std::string, int> structTypes = {{"bar", 1}, {"bars", 2}, {"loop", 3}, {"function", 4}, {"list", 5}};
};

#endif
