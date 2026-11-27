#ifndef CODETEXT_H
#define CODETEXT_H

#include <string>
#include <vector>

// a chunk of LiveLily code: a definition (a bar, a set of bars, a loop, a function, or a list) or a single line
// from its first line to its last, as indexes of the lines it was found in
struct BracketChunk
{
	int top;
	int bottom;
	int errorLine; // -1 if the curly brackets of the chunk are fine
	std::string errorStr;
	bool finished; // false for a definition whose closing curly bracket is missing
};

// the analysis of the text of LiveLily code that doesn't depend on an editor, used by the editor and by the parser (on function bodies)
namespace CodeText {
	// the curly brackets of a line, in order, outside quotes, and before the first % (as the parser strips what follows it)
	std::string bracketsInLine(const std::string& str);
	// whether a line has code (it is not empty or a comment)
	bool isLyvCodeLine(const std::string& str);
	// the definition command a line starts with, whose body is in curly brackets (\bar, \bars, \loop, \function, \list
	// or a list's traverse method), also after a closing bracket (e.g. "} \bar 2 {"), or an empty string
	std::string definitionCommand(const std::string& str);
	// the curly brackets of a line that it leaves open, and those that close brackets of lines before it
	void countUnmatchedBrackets(const std::string& str, int& openBrackets, int& closingBrackets);
	// splits lines of LiveLily code into chunks (see findBracketChunksInLines() in codetext.cpp)
	std::vector<BracketChunk> findBracketChunksInLines(const std::vector<std::string>& lines);
}

#endif
