// the analysis of the text of LiveLily code that doesn't depend on an editor (see codetext.h)
#include "codetext.h"
#include <algorithm>
#include <cctype>

//--------------------------------------------------------------
std::string CodeText::bracketsInLine(const std::string& str)
{
	// returns the curly brackets of a line of LiveLily code, in order
	// brackets inside strings are ignored, and so is everything after a %
	// (the parser strips everything after the first % in a line, even inside strings, so we do the same)
	std::string brackets;
	bool inString = false;
	for (size_t i = 0; i < str.size(); i++) {
		char c = str[i];
		if (c == '%') break;
		if (c == '"') inString = !inString;
		else if (!inString && (c == '{' || c == '}')) brackets += c;
	}
	return brackets;
}

//--------------------------------------------------------------
bool CodeText::isLyvCodeLine(const std::string& str)
{
	// a line of LiveLily code that is not empty and not a comment
	size_t pos = str.find_first_not_of(" \t");
	return (pos != std::string::npos && str[pos] != '%');
}

//--------------------------------------------------------------
std::string CodeText::definitionCommand(const std::string& str)
{
	// returns the command a line starts with, if it's a command whose body is enclosed in curly brackets
	// otherwise returns an empty string
	// a closing curly bracket before the command is allowed (e.g. "} \bar 2 {")
	static const std::vector<std::string> definitionCommands = {"\\bar", "\\bars", "\\loop", "\\function", "\\list"};
	size_t start = str.find_first_not_of(" \t}");
	if (start == std::string::npos) return "";
	size_t end = str.find_first_of(" \t{", start);
	std::string command = str.substr(start, (end == std::string::npos ? std::string::npos : end - start));
	if (std::find(definitionCommands.begin(), definitionCommands.end(), command) != definitionCommands.end()) return command;
	// \list.traverse has lines in curly brackets too, which can be written in many lines
	if (command.size() > 10 && command.compare(command.size() - 9, 9, ".traverse") == 0 && command[0] == '\\') return command;
	return "";
}

//--------------------------------------------------------------
void CodeText::countUnmatchedBrackets(const std::string& str, int& openBrackets, int& closingBrackets)
{
	// matches the curly brackets within a line, and counts the ones that are left unmatched
	openBrackets = 0;
	closingBrackets = 0;
	for (char c : bracketsInLine(str)) {
		if (c == '{') openBrackets++;
		else if (openBrackets > 0) openBrackets--;
		else closingBrackets++;
	}
}

//--------------------------------------------------------------
std::vector<BracketChunk> CodeText::findBracketChunksInLines(const std::vector<std::string>& lines)
{
	// splits lines of LiveLily code into chunks, the line numbers of the chunks are indexes to the lines vector
	// a chunk is a definition (a bar, a set of bars, a loop, a function, or a list) or a single line
	// a definition starts at a line with a definition command that leaves a curly bracket open
	// and ends at the line with the curly bracket that closes it
	// only functions can include other definitions, but not other functions
	// so a definition that starts while a definition that can't include it is open, means that the open one is not finished
	// an unfinished definition ends at the line of code before that (or at the last line), and doesn't affect the code below it
	// indentation doesn't matter
	// each chunk stores the error of its first line with an unmatched curly bracket, if there is one
	std::vector<BracketChunk> chunks;
	const std::string unmatchedOpening = "unmatched opening curly bracket, the definition is not finished";
	const std::string unmatchedClosing = "unmatched closing curly bracket";
	// the open definitions, as a stack of their first line and whether they are functions
	std::vector<std::pair<int, bool>> openDefinitions;
	// the chunk of the outermost open definition
	BracketChunk chunk = {-1, -1, -1, "", true};
	int lastCodeLine = -1;
	auto setError = [&chunk](int line, const std::string& errorStr) {
		if (chunk.errorLine < 0 || line < chunk.errorLine) {
			chunk.errorLine = line;
			chunk.errorStr = errorStr;
		}
	};
	for (int lineNdx = 0; lineNdx < (int)lines.size(); lineNdx++) {
		const std::string& str = lines[lineNdx];
		if (!isLyvCodeLine(str)) continue;
		int openBrackets, closingBrackets;
		countUnmatchedBrackets(str, openBrackets, closingBrackets);
		std::string command = definitionCommand(str);
		bool definitionLine = !command.empty();
		bool functionLine = (command == "\\function");
		// unmatched closing brackets close the innermost open definition
		bool strayReported = false;
		for (int i = 0; i < closingBrackets; i++) {
			if (!openDefinitions.empty()) {
				openDefinitions.pop_back();
				if (openDefinitions.empty()) {
					chunk.bottom = lineNdx;
					chunks.push_back(chunk);
				}
			}
			else if (!strayReported) {
				chunks.push_back({lineNdx, lineNdx, lineNdx, unmatchedClosing, true});
				strayReported = true;
			}
		}
		// a definition line ends the open definitions that can't include it, as they are not finished
		if (definitionLine && !openDefinitions.empty()) {
			while (!openDefinitions.empty() && (!openDefinitions.back().second || functionLine)) {
				setError(openDefinitions.back().first, unmatchedOpening);
				openDefinitions.pop_back();
			}
			if (openDefinitions.empty()) {
				chunk.bottom = lastCodeLine;
				chunk.finished = false;
				chunks.push_back(chunk);
			}
		}
		if (openBrackets > 0) {
			if (definitionLine) {
				if (openDefinitions.empty()) chunk = {lineNdx, lineNdx, -1, "", true};
				for (int i = 0; i < openBrackets; i++) openDefinitions.push_back(std::make_pair(lineNdx, functionLine));
			}
			// outside a definition, an open bracket that doesn't start one has nothing to close it
			else if (openDefinitions.empty()) {
				chunks.push_back({lineNdx, lineNdx, lineNdx, unmatchedOpening, true});
			}
			// inside a definition, it's part of the line (e.g. a tuplet in a melodic line), and the parser checks it
		}
		// a line outside a definition that doesn't open or close one is a chunk of its own
		// (this includes definitions in a single line, e.g. "\bar 1 { \inst c'1 }")
		else if (openDefinitions.empty() && closingBrackets == 0) {
			chunks.push_back({lineNdx, lineNdx, -1, "", true});
		}
		lastCodeLine = lineNdx;
	}
	// definitions left open at the end are not finished
	if (!openDefinitions.empty()) {
		for (const std::pair<int, bool>& definition : openDefinitions) setError(definition.first, unmatchedOpening);
		chunk.bottom = lastCodeLine;
		chunk.finished = false;
		chunks.push_back(chunk);
	}
	return chunks;
}
