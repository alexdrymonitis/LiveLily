#ifndef SESSION_H
#define SESSION_H

#include <list>
#include <map>
#include <utility>
#include <vector>
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

		// lists (\list): their items by their indexes, and their indexes by their names
		std::map<int, std::list<std::string>> listMap;
		std::map<std::string, int> listIndexes;
		bool storingList;
		int lastListIndex;
		int listIndexCounter;
		bool traversingList;
		// the lines of \list.traverse, with their line numbers, and how many curly brackets are still open while they are collected
		std::vector<std::pair<int, std::string>> traverseLines;
		int traverseDepth = 0;

		// functions (\function): their indexes by their names (the Function objects are in SharedData)
		std::map<std::string, int> functionIndexes;
		bool storingFunction;
		// index of the function whose body is being stored, used only by the main thread
		// kept separate from lastFunctionIndex, which is also set when a function is called from the sequencer thread
		int storingFunctionIndex;
		// the curly bracket depth while storing a function body, the function's own brackets included
		int functionBodyDepth;
		int lastFunctionIndex;

		// the state of parsing bars (\bar), loops (\loop), and bars defined together (\bars)
		// whether there was an error in a bar, so that its melodic lines are not stored
		bool barError;
		bool parsingBar;
		bool parsingBars;
		bool parsingLoop;
		// the name of a \bars definition, until it is stored as a loop
		std::string barsName;
		// a counter of the instruments parsed, for first bars that are not complete
		int numInstsParsed;
		// the loop command sent to connected score parts
		std::string lastLoopCommand;
		// the name of the bars of \bars, and the counter of its passes, one for every bar
		std::string multiBarsName;
		int barsIterCounter;
		// whether all the instruments of \bars have the same number of bars, and that number
		bool numBarsToParseSet;
		int numBarsToParse;
		// the number of bars defined by a \time, \tempo or \beatat with more than one value separated by |, inside \bars
		// 0 if there is none, and the command that defined it, for error messages
		int barsCmdNumBars;
		std::string barsCmdName;
		int firstInstForBarsIndex;
		bool firstInstForBarsSet;
};

#endif
