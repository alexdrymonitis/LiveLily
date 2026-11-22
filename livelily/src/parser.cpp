// the parsing functions of the ofApp class
// these are members of ofApp, declared in ofApp.h, and defined here to keep ofApp.cpp shorter
#include "ofApp.h"
#include "tokens.h"
#include <string>
#include <vector>
#include <map>
#include <ctype.h> // to add isdigit()
#include <algorithm> // for replacing characters in a string
#include <cmath> // to add abs()
#include <utility> // to add pair and make_pair
#include <sstream> // to create a string from vector<string>
#include <cstdio> // popen() for \system
#include <filesystem>
#include "clock.h"
#include <fstream>
#include <numeric> // std::lcm
#include <iostream>
#include <thread>

// name of the bound function currently being called by callBoundFunction() on this thread, empty if none
// thread_local, as bound functions are called from both the sequencer thread and the main thread
static thread_local std::string currentBoundFunction;

// a dynamic written as a token of its own (e.g. \f in \euclide {16 5 4 c' \f \p}), which is not a command
static bool isDynamicToken(const std::string& token)
{
	static const std::vector<std::string> dynamicTokens = {"\\ppp", "\\pp", "\\p", "\\mp", "\\mf", "\\f", "\\ff", "\\fff"};
	return std::find(dynamicTokens.begin(), dynamicTokens.end(), token) != dynamicTokens.end();
}

// a command is a token that starts with a backslash, other than a dynamic
static bool isCommandToken(const std::string& token)
{
	return !token.empty() && token[0] == '\\' && !isDynamicToken(token);
}

// the return values of the functions that are running, the innermost last (see \return)
// one stack per thread, as bound functions run in the sequencer thread while lines run in the main thread
struct FunctionReturn {
	bool returned = false;
	std::vector<std::string> value;
};
static thread_local std::vector<FunctionReturn> functionReturns;
// adds a return value for a call of a function, which is removed when the call ends, however it ends
struct FunctionReturnFrame {
	FunctionReturnFrame() { functionReturns.emplace_back(); }
	~FunctionReturnFrame() { functionReturns.pop_back(); }
};

// how many curly brackets a string opens, minus the ones it closes, outside strings
static int curlyBracketBalance(const std::string& str)
{
	int depth = 0;
	bool inQuotes = false;
	for (char c : str) {
		if (c == '"') inQuotes = !inQuotes;
		else if (!inQuotes && c == '{') depth++;
		else if (!inQuotes && c == '}') depth--;
	}
	return depth;
}

/********************* parsing functions **********************/
//--------------------------------------------------------------
void ofApp::parseStrings(int index, int numLines)
{
	barError = false;
	// an unmatched curly bracket (e.g. a bar definition that is not finished) is an error
	// and in this case none of the lines is parsed
	std::string bracketError;
	int unmatchedBracketLine = editors[whichPane].findUnmatchedBracket(index, numLines, bracketError);
	if (unmatchedBracketLine > -1) {
		editors[whichPane].setTraceback(3, bracketError, unmatchedBracketLine);
		return;
	}
	// collect the lines to parse from the editor
	std::vector<std::pair<int, std::string>> lines;
	for (int i = index; i < (index+numLines); i++) {
		if (editors[whichPane].getSessionActivity()) {
			lines.push_back(std::make_pair(i, editors[whichPane].allLines[i].str));
			if (editors[whichPane].getSendLines()) {
				ofxOscMessage m;
				m.setAddress("/livelily"+std::to_string(editors[whichPane].getSendLinesPaneNdx())+"/press");
				m.addStringArg(editors[whichPane].allLines[i].str);
				editors[whichPane].oscKeys.sendMessage(m, false);
			}
		}
		else {
			if (editors[whichPane].allLines[i].str == "\\active") {
				lines.push_back(std::make_pair(i, editors[whichPane].allLines[i].str));
			}
		}
	}
	parseLines(lines, numLines, false);
}

//--------------------------------------------------------------
std::vector<std::pair<int, std::string>> ofApp::normalizeDefinitionLines(const std::vector<std::pair<int, std::string>>& lines)
{
	// a comment starts at a % that is not inside a string
	auto stripComment = [](const std::string& str) {
		bool inQuotes = false;
		for (size_t i = 0; i < str.size(); i++) {
			if (str[i] == '"') inQuotes = !inQuotes;
			else if (!inQuotes && str[i] == '%') return str.substr(0, i);
		}
		return str;
	};
	auto trim = [](const std::string& str) {
		size_t first = str.find_first_not_of(" \t");
		if (first == std::string::npos) return std::string();
		return str.substr(first, str.find_last_not_of(" \t") - first + 1);
	};
	// how many curly brackets a line opens (minus the ones it closes), outside strings
	auto bracketDepth = [](const std::string& str) {
		int depth = 0;
		bool inQuotes = false;
		for (char c : str) {
			if (c == '"') inQuotes = !inQuotes;
			else if (!inQuotes && c == '{') depth++;
			else if (!inQuotes && c == '}') depth--;
		}
		return depth;
	};
	auto firstWord = [](const std::string& str) { return str.substr(0, str.find_first_of(" \t{")); };
	std::string indent(std::max(1, editors[whichPane].getTabSize()), ' ');
	// the words that start a statement inside a bar: the names of instruments and of groups of instruments
	// (also with a method, e.g. \violin.transpose), and the commands of a bar
	auto startsStatement = [&](const std::string& word) {
		if (word == "\\time" || word == "\\tempo" || word == "\\beatat") return true;
		if (word.size() < 2 || word[0] != '\\') return false;
		std::string name = word.substr(1, word.find('.') == std::string::npos ? std::string::npos : word.find('.') - 1);
		return sharedData.instrumentIndexes.find("\\" + name) != sharedData.instrumentIndexes.end() || instGroups.find("\\" + name) != instGroups.end();
	};
	std::vector<std::pair<int, std::string>> out;
	for (size_t i = 0; i < lines.size(); i++) {
		int lineNum = lines[i].first;
		std::string code = trim(stripComment(lines[i].second));
		std::string word = firstWord(code);
		bool isDefinition = (word == "\\loop" || word == "\\list" || word == "\\bar" || word == "\\bars" || word == "\\function");
		if (!isDefinition) {
			out.push_back(lines[i]);
			continue;
		}
		// the opening curly bracket can be on the next line, e.g. \loop name and then { \1 \2 } (or { alone, with the body below it)
		size_t next = i + 1;
		if (code.find('{') == std::string::npos) {
			while (next < lines.size() && trim(stripComment(lines[next].second)).empty()) next++;
			if (next < lines.size() && startsWith(trim(stripComment(lines[next].second)), "{")) {
				std::string nextCode = trim(stripComment(lines[next].second));
				code += " {";
				std::string rest = trim(nextCode.substr(1));
				i = next;
				// whatever follows the bracket in that line is parsed as the next line
				if (!rest.empty()) {
					std::vector<std::pair<int, std::string>> remaining = {std::make_pair(lines[next].first, rest)};
					remaining.insert(remaining.end(), lines.begin() + next + 1, lines.end());
					std::vector<std::pair<int, std::string>> normalizedRest;
					// the definition and the rest are normalized together, as the rest might close it
					std::vector<std::pair<int, std::string>> withHeader = {std::make_pair(lineNum, code)};
					withHeader.insert(withHeader.end(), remaining.begin(), remaining.end());
					normalizedRest = normalizeDefinitionLines(withHeader);
					out.insert(out.end(), normalizedRest.begin(), normalizedRest.end());
					return out;
				}
			}
		}
		if (word == "\\loop" || word == "\\list") {
			// a loop or a list in more lines is joined into one line, which is how they are parsed
			int depth = bracketDepth(code);
			std::string joined = code;
			size_t j = i + 1;
			while (depth > 0 && j < lines.size()) {
				std::string lineCode = trim(stripComment(lines[j].second));
				if (!lineCode.empty()) {
					joined += " " + lineCode;
					depth += bracketDepth(lineCode);
				}
				j++;
			}
			out.push_back(std::make_pair(lineNum, joined));
			i = j - 1;
			continue;
		}
		if ((word == "\\bar" || word == "\\bars") && code.find('{') != std::string::npos) {
			// a bar (or bars) in one line is split into one line per statement, which is how bars in many lines are parsed
			size_t open = code.find('{');
			size_t close = std::string::npos;
			int depth = 0;
			bool inQuotes = false;
			for (size_t k = open; k < code.size(); k++) {
				if (code[k] == '"') inQuotes = !inQuotes;
				if (inQuotes) continue;
				if (code[k] == '{') depth++;
				else if (code[k] == '}' && --depth == 0) {
					close = k;
					break;
				}
			}
			if (close != std::string::npos) {
				out.push_back(std::make_pair(lineNum, trim(code.substr(0, open)) + " {"));
				std::string body = code.substr(open + 1, close - open - 1);
				// a statement starts at a word that starts one (see startsStatement()), outside strings and curly brackets
				std::string statement;
				size_t k = 0;
				int bodyDepth = 0;
				inQuotes = false;
				while (k < body.size()) {
					bool wordStart = (k == 0 || body[k-1] == ' ' || body[k-1] == '\t') && body[k] != ' ' && body[k] != '\t';
					if (wordStart && !inQuotes && bodyDepth == 0) {
						size_t wordEnd = body.find_first_of(" \t", k);
						std::string w = body.substr(k, wordEnd == std::string::npos ? std::string::npos : wordEnd - k);
						if (startsStatement(w) && !trim(statement).empty()) {
							out.push_back(std::make_pair(lineNum, indent + trim(statement)));
							statement.clear();
						}
					}
					char c = body[k];
					if (c == '"') inQuotes = !inQuotes;
					else if (!inQuotes && c == '{') bodyDepth++;
					else if (!inQuotes && c == '}') bodyDepth--;
					statement += c;
					k++;
				}
				if (!trim(statement).empty()) out.push_back(std::make_pair(lineNum, indent + trim(statement)));
				out.push_back(std::make_pair(lineNum, "}"));
				// anything after the bar, e.g. calling it right away
				std::string after = trim(code.substr(close + 1));
				if (!after.empty()) out.push_back(std::make_pair(lineNum, after));
				continue;
			}
		}
		out.push_back(std::make_pair(lineNum, code));
	}
	return out;
}

//--------------------------------------------------------------
std::pair<int, std::string> ofApp::parseLines(const std::vector<std::pair<int, std::string>>& inputLines, int numLines, bool stopOnError, bool nextBarsPass)
{
	// parses a chunk of lines, either from the editor (see parseStrings()) or from a function body (see isFunction())
	// and finishes any bar, set of bars, or loop they define
	// definitions with curly brackets are first brought to the form the parser expects (see normalizeDefinitionLines())
	std::vector<std::pair<int, std::string>> lines = normalizeDefinitionLines(inputLines);
	if (lines.empty()) return std::make_pair(0, "");
	int index = lines.front().first;
	// if we are called while a bar, loop or list is being parsed (e.g. a function called inside a bar definition)
	// the lines are part of that, so we only parse them, and leave the rest to the call that started it
	// except when \bars parses its lines again for its next bar, which must be finished like the first one
	if (!nextBarsPass && (parsingBar || parsingBars || parsingLoop || storingList || traversingList)) {
		std::pair<int, std::string> p = std::make_pair(0, "");
		for (const std::pair<int, std::string>& line : lines) {
			p = parseString(line.second, line.first, numLines);
			if (p.first == 3 && stopOnError) break;
		}
		return p;
	}
	bool noErrors = true;
	barError = false;
	// the first error, or the result of the last line if there is no error
	std::pair<int, std::string> result = std::make_pair(0, "");
	std::vector<std::pair<int, std::string>> errors;
	for (const std::pair<int, std::string>& line : lines) {
		// parseString() returns a pair of int and string, where the int determines the error type
		// 0 for nothing, 1 for note, 2 for warning, and 3 for error
		errors.push_back(parseString(line.second, line.first, numLines));
		if (errors.back().first == 3 && stopOnError) break;
	}
	// a definition in one line (e.g. \bar 1 { \inst c'1 }) is parsed as more lines with the same line number (see normalizeDefinitionLines())
	// so the first error is shown again, in case a line after it, which had no error, released the traceback of that line number
	for (size_t i = 0; i < errors.size(); i++) {
		if (errors[i].first == 3) {
			setEditorTraceback(errors[i].first, errors[i].second, lines[i].first);
			break;
		}
	}
	for (auto error : errors) {
		if (error.first == 3) {
			if (parsingBar) {
				// must clear bars from all passed instruments
				// and entries in barsIndexes and loopsIndexes std::maps
				if (parsingBars) {
					for (int i = 0; i < barsIterCounter; i++) {
						// when an error occurs not in the first bar of all the bars in the \bars command
						// only in the first iteration of deleting the last bar we must leave the passed
						// booleans of the instruments intact
						// for all the other iterations, we must set them to true, as they have all passed
						// all the previous bars, up until the error occured
						if (i > 0) {
							for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
								it->second.setPassed(true);
							}
						}
						deleteLastBar();
					}
					barsIterCounter = 0;
					numBarsToParse = 0;
					numBarsToParseSet = false;
					barsCmdNumBars = 0;
					parsingBars = false;
					firstInstForBarsSet = false;
				}
				else {
					deleteLastBar();
				}
				parsingBar = false;
				checkIfAllPartsReceivedBarData = false;
			}
			if (parsingLoop) {
				parsingLoop = false;
			}
			if (parsingBars) {
				parsingBars = false;
				firstInstForBarsSet = false;
			}
			if (storingList) storingList = false;
			noErrors = false;
			result = error;
		}
		if (!noErrors) break;
	}
	if (noErrors && !errors.empty()) result = errors.back();
	// if we have parsed at least one melodic line
	if (noErrors && (parsingBar || parsingLoop)) {
		// the bar index has already been stored with the \bar command
		int barIndex = getLastLoopIndex();
		// first send the parsed lines to any connected instruments
		if (parsingBar && !parsingBars) {
			sendBarToParts(barIndex);
		}
		if (!parsingLoop) {
			// store the closing curly bracket in the bar lines std::map
			// sharedData.barLines is a map<string, string> so we get the key with
			// sharedData.barsOrdered[barIndex]
			sharedData.barLines[sharedData.barsOrdered[barIndex]] += "}";
			// then add a rest for any instrument that is not included in the bar
			fillInMissingInsts(barIndex);
			// if we create a bar, create a loop with this bar only
			sharedData.loopData[barIndex] = {barIndex};
			// check if no time has been specified and set the default 4/4
			if (sharedData.numerator.find(barIndex) == sharedData.numerator.end()) {
				sharedData.numerator[barIndex] = 4;
				sharedData.denominator[barIndex] = 4;
			}
			setScoreNotes(barIndex);
			if (!parsingBars) {
				// then set the note positions and calculate the staff positions
				setNotePositions(barIndex);
				calculateStaffPositions(barIndex, false);
			}
			barError = false;
			for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				it->second.setPassed(false);
			}
		}
		if (!sequencer.isThreadRunning()) {
			sharedData.loopIndex = sharedData.tempLoopIndex;
			//sendLoopIndexToParts();
		}
		inserting = false;
		parsingBar = false;
		parsingLoop = false;
	}
	if (storingFunction) {
		storingFunction = false;
	}
	if (storingList) storingList = false;
	if (traversingList) traversingList = false;
	if (noErrors && parsingBars) {
		// check if all instruments are done parsing all their bars
		bool parsingBarsDone = true;
		for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			if (!it->second.getMultiBarsDone()) {
				parsingBarsDone = false;
				break;
			}
		}
		// call this function recursively, until all instruments are done parsing their bars
		if (!parsingBarsDone) {
			return parseLines(lines, numLines, stopOnError, true);
		}
		else {
			int barIndex = sharedData.barsIndexes["\\"+multiBarsName+"-1"];
			setNotePositions(barIndex, barsIterCounter);
			for (int i = 0; i < barsIterCounter; i++) {
				barIndex = sharedData.barsIndexes["\\"+multiBarsName+"-"+std::to_string(i+1)];
				sendBarToParts(barIndex);
				calculateStaffPositions(barIndex, false);
			}
			parsingBars = false;
			firstInstForBarsSet = false;
			// once done, create a string to create a loop with all the separate bars
			std::string multiBarsLoop = "\\loop " + multiBarsName + " {";
			for (int i = 0; i < barsIterCounter-1; i++) {
				multiBarsLoop += ("\\" + multiBarsName + "-" + std::to_string(i+1) + " ");
			}
			// last iteration outside of the loop so that we don't add the extra white space
			// and we close the curly brackets
			multiBarsLoop += ("\\" + multiBarsName + "-" + std::to_string(barsIterCounter) + "}");
			parseCommand(genCmdInput(multiBarsLoop), index, 1);
			// after we create the loop of the bars we have created, we send the line with the loop command to the parts
			sendLoopToParts();
			barsIterCounter = 0;
			// lastly, check if we're calling the loop to be played, in case the sequencer is already running
			int lineNdx = lines.back().first;
			std::string lastLine = lines.back().second;
			if (startsWith(lastLine, "}") && lastLine.size() > 1) {
				lastLine = lastLine.substr(1);
				parseCommand(genCmdInput(lastLine), lineNdx, 1);
			}
		}
	}
	// if we're creating a pattern out of patterns
	// the posCalculator.calculateAllStaffPositions() function
	// is called from parseString() below, after the block that checks if
	// a line starts with a quotation mark
	return result;
}

//--------------------------------------------------------------
void ofApp::callBoundFunction(const std::string& funcName, const std::string& args)
{
	// any traceback set while this function runs (including by functions it calls)
	// is queued with this name, so it can go to the line where this function is defined
	// args are the arguments to the function, e.g. the values of an OSC message mapped to it, separated by spaces
	currentBoundFunction = funcName;
	parseString(args.empty() ? funcName : funcName + " " + args, 1, 1); // dummy 2nd and 3rd arguments
	currentBoundFunction.clear();
}

//--------------------------------------------------------------
void ofApp::setEditorTraceback(int errorCode, std::string errorStr, int lineNum)
{
	if (std::this_thread::get_id() == mainThreadId && currentBoundFunction.empty()) {
		editors[whichPane].setTraceback(errorCode, errorStr, lineNum);
		return;
	}
	std::lock_guard<std::mutex> lock(pendingTracebacksMutex);
	pendingTracebacks.push_back({whichPane, lineNum, false, errorCode, errorStr, currentBoundFunction});
}

//--------------------------------------------------------------
void ofApp::releaseEditorTraceback(int lineNum)
{
	if (std::this_thread::get_id() == mainThreadId && currentBoundFunction.empty()) {
		editors[whichPane].releaseTraceback(lineNum);
		return;
	}
	std::lock_guard<std::mutex> lock(pendingTracebacksMutex);
	pendingTracebacks.push_back({whichPane, lineNum, true, 0, "", currentBoundFunction});
}

//--------------------------------------------------------------
bool ofApp::lineDefinesFunction(const std::string& str, const std::string& funcName)
{
	// a definition line is: optional white space, "\function", white space, the function name (without the backslash)
	// followed by white space, an opening curly bracket, a comment, or the end of the line
	const std::string cmd = "\\function";
	size_t pos = str.find_first_not_of(" \t");
	if (pos == std::string::npos || str.compare(pos, cmd.size(), cmd) != 0) return false;
	pos += cmd.size();
	size_t nameStart = str.find_first_not_of(" \t", pos);
	// there must be white space between \function and the name
	if (nameStart == std::string::npos || nameStart == pos) return false;
	size_t nameEnd = str.find_first_of(" \t{%", nameStart);
	std::string name = str.substr(nameStart, nameEnd == std::string::npos ? std::string::npos : nameEnd - nameStart);
	return "\\" + name == funcName;
}

//--------------------------------------------------------------
std::pair<int, int> ofApp::findFunctionDefinitionLine(const std::string& funcName)
{
	int pane, defLine;
	{
		std::lock_guard<std::mutex> lock(sharedData.functionsMutex);
		auto funcIndexIt = functionIndexes.find(funcName);
		if (funcIndexIt == functionIndexes.end()) return std::make_pair(-1, -1);
		auto funcIt = sharedData.functions.find(funcIndexIt->second);
		if (funcIt == sharedData.functions.end()) return std::make_pair(-1, -1);
		pane = funcIt->second.getDefinitionPane();
		defLine = funcIt->second.getDefinitionLine();
	}
	auto editorIt = editors.find(pane);
	if (editorIt == editors.end()) return std::make_pair(-1, -1);
	std::map<int, line>& lines = editorIt->second.allLines;
	// first check the stored line, which is the common case and avoids searching the pane on every call
	auto lineIt = lines.find(defLine);
	if (lineIt != lines.end() && lineDefinesFunction(lineIt->second.str, funcName)) {
		return std::make_pair(pane, defLine);
	}
	// the definition has moved (e.g. lines were inserted or deleted above it), or its line was changed or deleted
	// so search the pane for it, and if it is defined more than once, pick the definition closest to the stored line
	int foundLine = -1;
	int minDistance = std::numeric_limits<int>::max();
	for (auto it = lines.begin(); it != lines.end(); ++it) {
		if (lineDefinesFunction(it->second.str, funcName)) {
			int distance = std::abs(it->first - defLine);
			if (distance < minDistance) {
				minDistance = distance;
				foundLine = it->first;
			}
		}
	}
	// the definition is no longer in the pane
	if (foundLine < 0) return std::make_pair(-1, -1);
	// store the new line, so the next lookup finds it without searching
	{
		std::lock_guard<std::mutex> lock(sharedData.functionsMutex);
		auto funcIndexIt = functionIndexes.find(funcName);
		if (funcIndexIt != functionIndexes.end()) {
			auto funcIt = sharedData.functions.find(funcIndexIt->second);
			// only if the function hasn't been redefined elsewhere in the meantime
			if (funcIt != sharedData.functions.end() && funcIt->second.getDefinitionPane() == pane && funcIt->second.getDefinitionLine() == defLine) {
				funcIt->second.setDefinitionLocation(pane, foundLine);
			}
		}
	}
	return std::make_pair(pane, foundLine);
}

//--------------------------------------------------------------
void ofApp::applyPendingTracebacks()
{
	// take the queued tracebacks while holding the lock, and apply them after releasing it
	std::vector<PendingTraceback> tracebacks;
	{
		std::lock_guard<std::mutex> lock(pendingTracebacksMutex);
		if (pendingTracebacks.empty()) return;
		tracebacks.swap(pendingTracebacks);
	}
	for (const PendingTraceback& tb : tracebacks) {
		int pane = tb.pane;
		int lineNum = tb.lineNum;
		// tracebacks of bound functions go to the line where the function is defined
		// if that line still defines it, otherwise to the dummy line they were set with
		if (!tb.boundFunction.empty()) {
			std::pair<int, int> definition = findFunctionDefinitionLine(tb.boundFunction);
			if (definition.first > -1) {
				pane = definition.first;
				lineNum = definition.second;
			}
		}
		// the pane might have been closed and the line deleted since the traceback was queued
		auto editorIt = editors.find(pane);
		if (editorIt == editors.end()) continue;
		// setTraceback() uses operator[] which would create the line if it doesn't exist
		if (editorIt->second.allLines.find(lineNum) == editorIt->second.allLines.end()) continue;
		if (tb.release) editorIt->second.releaseTraceback(lineNum);
		else editorIt->second.setTraceback(tb.errorCode, tb.errorStr, lineNum);
	}
}

//--------------------------------------------------------------
std::pair<int, std::string> ofApp::storeFunctionLine(const std::string& str, int lineNum, int numLines)
{
	// stores a line of the body of the function being defined
	// the curly brackets are tracked, so that definitions inside the function (e.g. a bar) are stored with their brackets
	// and only the bracket that closes the function ends its definition
	// brackets inside strings are ignored (comments have already been stripped by parseString())
	std::string lineToStore;
	bool inString = false;
	bool functionClosed = false;
	size_t i;
	for (i = 0; i < str.size(); i++) {
		char c = str[i];
		if (c == '"') {
			inString = !inString;
		}
		else if (!inString && c == '{') {
			functionBodyDepth++;
			// the opening bracket of the function is not part of its body
			if (functionBodyDepth == 1) continue;
		}
		else if (!inString && c == '}') {
			// a closing bracket before the function is opened is ignored
			if (functionBodyDepth == 0) continue;
			functionBodyDepth--;
			// the closing bracket of the function is not part of its body
			if (functionBodyDepth == 0) {
				functionClosed = true;
				break;
			}
		}
		if (functionBodyDepth > 0) lineToStore += c;
	}
	// strip white space at the edges
	size_t first = lineToStore.find_first_not_of(" \t");
	if (first != std::string::npos) {
		size_t last = lineToStore.find_last_not_of(" \t");
		std::lock_guard<std::mutex> lock(sharedData.functionsMutex);
		sharedData.functions[storingFunctionIndex].appendLine(lineToStore.substr(first, last - first + 1));
	}
	// the line is now part of the function, so an error it had from an earlier attempt is cleared
	// (e.g. an unmatched curly bracket that has since been closed), as with any other line that is parsed without errors
	releaseEditorTraceback(lineNum);
	if (functionClosed) {
		storingFunction = false;
		// parse anything after the closing bracket of the function as usual
		std::string rest = str.substr(i+1);
		if (rest.find_first_not_of(" \t") != std::string::npos) {
			return parseString(rest, lineNum, numLines);
		}
	}
	return std::make_pair(0, "");
}

//--------------------------------------------------------------
std::pair<int, std::string> ofApp::parseString(std::string str, int lineNum, int numLines)
{
	if (str.length() == 0) return std::make_pair(0, "");
	if (startsWith(str, "%")) return std::make_pair(0, "");
	// strip white spaces
	while (startsWith(str, " ")) {
		str = str.substr(1);
	}
	// then check again if the std::string is empty
	if (str.length() == 0) return std::make_pair(0, "");
	size_t commentIndex = str.find("%");
	if (commentIndex != std::string::npos) {
		str = str.substr(0, commentIndex);
	}
	// check if there's a trailing white space
	if (endsWith(str, " ")) {
		str = str.substr(0, str.size()-1);
	}
	// lines of a function body are stored before any curly brackets are stripped below
	// as a function can include definitions (e.g. a bar) whose brackets must be stored too
	if (storingFunction) {
		return storeFunctionLine(str, lineNum, numLines);
	}
	// the lines of \list.traverse in many lines are collected until its closing curly bracket, and then executed for every item
	if (traversingList) {
		traverseDepth += curlyBracketBalance(str);
		if (traverseDepth > 0) {
			traverseLines.push_back(std::make_pair(lineNum, str));
			return std::make_pair(0, "");
		}
		// the line that closes \list.traverse, which might have code before its closing bracket
		size_t closing = str.rfind('}');
		std::string beforeClosing = (closing == std::string::npos ? str : str.substr(0, closing));
		while (!beforeClosing.empty() && isspace(beforeClosing.back())) beforeClosing.pop_back();
		if (!beforeClosing.empty()) traverseLines.push_back(std::make_pair(lineNum, beforeClosing));
		traversingList = false;
		CmdOutput cmdOutput = traverseList(numLines);
		return std::make_pair(cmdOutput.errorCode, cmdOutput.errorStr);
	}
	// if we're left with a closing bracket only
	if (str.compare("}") == 0) return std::make_pair(0, "");
	if (startsWith(str, "{")) {
		str = str.substr(1);
		// again strip white spaces
		while (startsWith(str, " ")) {
			str = str.substr(1);
		}
	}
	// check if the std::string starts with a closing bracket but has more stuff after it
	if (startsWith(str, "}")) {
		str = str.substr(1);
		// and check for a trailing white space
		if (startsWith(str, " ")) {
			str = str.substr(1);
		}
	}
	// last check if std::string is empty
	if (str.length() == 0) return std::make_pair(0, "");
	// ignore comments
	if (startsWith(str, "%")) {
		releaseEditorTraceback(lineNum);
		return std::make_pair(0, "");
	}
	else {
		if (parsingLoop) {
			std::pair<int, std::string> error = parseBarLoop(str, lineNum, numLines);
			if (error.first > 0) {
				setEditorTraceback(error.first, error.second, lineNum);
				deleteLastLoop();
			}
			else {
				releaseEditorTraceback(lineNum);
				if (!sequencer.isThreadRunning()) {
					sharedData.loopIndex = sharedData.tempLoopIndex;
					sendLoopIndexToParts();
				}
			}
			return error;
		}
		else if (storingList) {
			storeList(str);
			return std::make_pair(0, "");
		}
		else {
			// the command at the start of the line gets the whole line as it is written
			// and evaluates its arguments when it runs (see CommandSpec and parseCommand())
			std::vector<std::string> tokens = tokenizeLine(str);
			if (tokens.empty()) {
				releaseEditorTraceback(lineNum);
				return std::make_pair(0, "");
			}
			CmdInput cmdInput = {tokens, std::vector<std::string>(), false, true};
			CmdOutput cmdOutput = parseCommand(cmdInput, lineNum, numLines);
			if (cmdOutput.errorCode > 0) {
				setEditorTraceback(cmdOutput.errorCode, cmdOutput.errorStr, lineNum);
			}
			else {
				releaseEditorTraceback(lineNum);
			}
			return std::make_pair(cmdOutput.errorCode, cmdOutput.errorStr);
		}
	}
	return std::make_pair(0, "");
}

//--------------------------------------------------------------
std::vector<std::string> ofApp::tokenizeLine(const std::string& input)
{
	bool isString = false;
	bool isChord = false;
	int quotesCounter = 0;
	std::vector<std::string> tokens;
	std::string token;
	for (size_t i = 0; i < input.size(); i++) {
		if (isspace(input.at(i))) {
			if (input.at(i) == '\t' || input.at(i) == '\n') {
				token += input.at(i);
			}
			else {
				if (!token.empty()) {
					if (!isString && !isChord) {
						tokens.push_back(token);
						token.clear();
					}
					else {
						token += input.at(i);
					}
				}
			}
		}
		else if (input.at(i) == '{' || input.at(i) == '}') {
			if (!token.empty() && !isString) {
				tokens.push_back(token);
				token.clear();
			}
			tokens.emplace_back(1, input.at(i));
		}
		else {
			token += input.at(i);
			if (input[i] == '"') {
				quotesCounter++;
				if (quotesCounter == 1) {
					isString = true;
				}
				else {
					isString = false;
					quotesCounter = 0;
				}
			}
			// a '<' after a backslash is a crescendo (\<), not a chord opening, even if a dynamic follows it (e.g. \<\mp)
			else if (input.at(i) == '<' && (i < input.size()-1 && !isspace(input.at(i+1))) && (i == 0 || input.at(i-1) != '\\')) {
				isChord = true;
			}
			// a '>' after a backslash or a hyphen is a diminuendo (\>) or an accent (->), not a chord closing
			else if (input[i] == '>' && (i == 0 || (input.at(i-1) != '\\' && input.at(i-1) != '-'))) {
				isChord = false;
			}
		}
	}
	if (!token.empty()) tokens.push_back(token);
	return tokens;
}

//--------------------------------------------------------------
CmdOutput ofApp::evaluateTokens(const std::vector<std::string>& tokens, int lineNum, int numLines)
{
	size_t index = tokens.size() - 1;
	size_t firstCmdNdx = 0;
	bool hasBrackets = false;
	// initialize a CmdOutput structure with the first command of the string
	// (all executable lines in LiveLily start with a command)
	CmdOutput cmdOutput = CmdOutput();
	if (tokens.empty()) return cmdOutput;
	// check if there are white spaces (probably a horizontal tab) at the beginning of the input vector
	for (std::string s : tokens) {
		if (s.empty() || s == " ") firstCmdNdx++;
		else break;
	}
	cmdOutput.outputVec = {tokens[firstCmdNdx]};
	std::deque<std::string> outputDeque;

	// the arguments of the commands are left as they are written, and each command's arguments are evaluated when it runs (see CommandSpec)
	// the arguments of a command are the token after it, the contents of the curly brackets after it, or both (e.g. \apply \1.\inst {c' e'})
	std::vector<bool> asWritten(tokens.size(), false);
	for (size_t p = firstCmdNdx + 1; p < tokens.size(); p++) {
		if (asWritten[p] || !isCommandToken(tokens[p])) continue;
		size_t q = p + 1;
		// a command written after a command that takes no arguments is not its argument (see CommandSpec::noArgs)
		CommandSpec spec = commandSpec(tokens[p]);
		if (spec.takesNothing) continue;
		bool noArgsBeforeCommand = (spec.noArgs && q < tokens.size() && isCommandToken(tokens[q]));
		// (a closing curly bracket is never an argument, as it closes the brackets the command is in)
		if (q < tokens.size() && tokens[q] != "{" && tokens[q] != "}" && !noArgsBeforeCommand) asWritten[q++] = true;
		if (q < tokens.size() && tokens[q] == "{") {
			int depth = 0;
			for (size_t r = q; r < tokens.size(); r++) {
				if (tokens[r] == "{") depth++;
				else if (tokens[r] == "}") depth--;
				if (depth == 0) break;
				if (r > q) asWritten[r] = true;
			}
		}
	}
	while (index > firstCmdNdx) {
		std::string token = tokens[index];
		if (asWritten[index]) {
			outputDeque.push_front(token);
			index--;
			continue;
		}
		// if the token is a command
		if (isCommandToken(token)) {
			size_t dequeNdx = 0;
			std::vector<std::string> cmd;
			cmd.push_back(token); // this is the command taken from the original tokens vector
			// a command that takes no arguments, written before another command, takes nothing (see CommandSpec::noArgs)
			// as the next item in the deque is that command, or its output (e.g. \mousey, or its value, in \print \mousex \mousey)
			// and a command that takes nothing written after it takes nothing (see CommandSpec::takesNothing)
			CommandSpec spec = commandSpec(token);
			bool noArgsBeforeCommand = (spec.noArgs && index + 1 < tokens.size() && isCommandToken(tokens[index+1]));
			if (dequeNdx >= outputDeque.size() || noArgsBeforeCommand || spec.takesNothing) {
				CmdInput cmdInput = {cmd, std::vector<std::string>(), false, false};
				CmdOutput cmdOutputLocal = parseCommand(cmdInput, lineNum, numLines);
				if (cmdOutputLocal.errorCode == 3) return cmdOutputLocal;
				// a warning of a command (e.g. \euclide) is kept, and returned with the output
				if (cmdOutputLocal.errorCode == 2 && cmdOutput.errorCode == 0) {
					cmdOutput.errorCode = 2;
					cmdOutput.errorStr = cmdOutputLocal.errorStr;
				}
				// it has no arguments, so its output replaces only the command
				outputDeque.insert(outputDeque.begin(), cmdOutputLocal.outputVec.begin(), cmdOutputLocal.outputVec.end());
			}
			else {
				// the arguments of the command: the item after it in the deque, the items inside the curly brackets after it, or both
				// collected up to the limit (the size of the deque, or of its evaluated arguments, see below)
				// some commands that are expanded return items that contain curly brackets (e.g. a bar name inside \transpose {\barname 12})
				// so the bracket pairs are counted
				// a closing curly bracket is never an argument, as it closes the brackets the command is in (e.g. \print \expand {\song.\one})
				auto collectArgs = [&](size_t limit) -> std::string {
					if (dequeNdx < limit && outputDeque[dequeNdx] != "{" && outputDeque[dequeNdx] != "}") cmd.push_back(outputDeque[dequeNdx++]);
					if (dequeNdx < limit && outputDeque[dequeNdx] == "{") {
						hasBrackets = true;
						int openBracketsCounter = 1;
						int closeBracketsCounter = 0;
						dequeNdx++; // skip open curly bracket
						while (dequeNdx < limit) {
							if (outputDeque[dequeNdx] == "{") openBracketsCounter++;
							if (outputDeque[dequeNdx] == "}") closeBracketsCounter++;
							if (closeBracketsCounter == openBracketsCounter) break;
							else cmd.push_back(outputDeque[dequeNdx]);
							dequeNdx++;
						}
						// a tuplet is written in one line, with its closing curly bracket
						if (closeBracketsCounter != openBracketsCounter && (token == "\\tuplet" || token == "\\tup")) {
							return "a tuplet must be written in one line, with its notes and its closing curly bracket";
						}
						// move to point to the item after the closing bracket, if there is one
						if (dequeNdx < limit) dequeNdx++;
					}
					return "";
				};
				std::string collectError = collectArgs(outputDeque.size());
				if (!collectError.empty()) return genError(collectError);
				// the arguments of a command with values or names are left as they are written until here (see CommandSpec)
				// they are evaluated where they are, in the deque, and collected again, so that the command gets them evaluated
				// counts the items it takes out of the deque as evaluated, and leaves any items it doesn't take evaluated
				// only its own arguments are evaluated, once, so nothing is evaluated twice
				ArgsMode argsMode = commandSpec(token).args;
				bool argsEvaluated = false;
				if (argsMode == ArgsMode::values || argsMode == ArgsMode::names) {
					std::vector<std::string> written(outputDeque.begin(), outputDeque.begin() + dequeNdx);
					CmdOutput evaluated = (argsMode == ArgsMode::values ? evalArgs(written, lineNum, numLines) : evalNames(written, lineNum, numLines));
					if (evaluated.errorCode == 3) return evaluated;
					outputDeque.erase(outputDeque.begin(), outputDeque.begin() + dequeNdx);
					outputDeque.insert(outputDeque.begin(), evaluated.outputVec.begin(), evaluated.outputVec.end());
					cmd.resize(1);
					dequeNdx = 0;
					hasBrackets = false;
					collectError = collectArgs(evaluated.outputVec.size());
					if (!collectError.empty()) return genError(collectError);
					argsEvaluated = true;
				}
				// the second argument to the data structure initialization below
				// is the tokens after the command and its argumets
				CmdInput cmdInput = {cmd, {outputDeque.begin()+dequeNdx, outputDeque.end()}, hasBrackets, false};
				cmdInput.argsEvaluated = argsEvaluated;
				CmdOutput cmdOutputLocal = parseCommand(cmdInput, lineNum, numLines);
				if (cmdOutputLocal.errorCode == 3) return cmdOutputLocal;
				// a warning of a command (e.g. \euclide) is kept, and returned with the output
				if (cmdOutputLocal.errorCode == 2 && cmdOutput.errorCode == 0) {
					cmdOutput.errorCode = 2;
					cmdOutput.errorStr = cmdOutputLocal.errorStr;
				}
				// the output replaces the command and its arguments (the dequeNdx items collected above), unless the command keeps them (see genOutput())
				if (!cmdOutputLocal.keepsArgs) {
					for (size_t i = 0; i < dequeNdx && outputDeque.size() > 0; i++) outputDeque.pop_front();
				}
				if (hasBrackets) hasBrackets = false;
				// the arguments of a command with raw arguments are not evaluated for it (see CommandSpec)
				// so if it keeps them, they are evaluated, as they would have been, had they not been its arguments
				if (argsMode == ArgsMode::raw && cmdOutputLocal.keepsArgs && dequeNdx > 0) {
					size_t numLeft = std::min(dequeNdx, outputDeque.size());
					std::vector<std::string> leftTokens(outputDeque.begin(), outputDeque.begin() + numLeft);
					CmdOutput evaluated = evalArgs(leftTokens, lineNum, numLines);
					if (evaluated.errorCode == 3) return evaluated;
					outputDeque.erase(outputDeque.begin(), outputDeque.begin() + numLeft);
					outputDeque.insert(outputDeque.begin(), evaluated.outputVec.begin(), evaluated.outputVec.end());
				}
				outputDeque.insert(outputDeque.begin(), cmdOutputLocal.outputVec.begin(), cmdOutputLocal.outputVec.end());
			}
		}
		else {
			outputDeque.push_front(token);
		}
		index--;
	}
	cmdOutput.outputVec.insert(cmdOutput.outputVec.end(), outputDeque.begin(), outputDeque.end());
	return cmdOutput;
}

//--------------------------------------------------------------
// \apply {\bar[.\inst] notes...} or \apply {\bar[.\inst] durations...}, in a melodic line (see the \apply command)
// the line of the bar (of the instrument after the dot, or of this instrument) is read from its parsed data
// with notes: the notes go to onsets of the line (its notes, not its rests or notes tied from the previous note)
// spread over them as evenly as possible in time, each on the onset closest to equal parts of the bar
// a note lasts until the next note or rest of the line, and an onset without a note after a rest is a rest
// the rests of the line stay as they are, and every note gets the dynamic and the articulations of its onset
// unless it has its own (e.g. c'\f->), there can't be more notes than onsets, and rhythm staffs can't take notes
// with durations: the notes of the onsets of the line go to the durations in turn, wrapping around if there are more durations
// a duration can have a dynamic and articulations (e.g. 8\f), and a rest (e.g. r8) takes no note
// a melodic line can't take the notes of a rhythm staff, and a rhythm staff takes the durations as they are
std::pair<std::string, std::vector<std::string>> ofApp::applyToLine(const std::vector<std::string>& args)
{
	std::vector<std::string> output;
	auto fail = [&output](const std::string& error) { return std::make_pair(error, output); };
	// the bar, and the instrument after a dot (with or without its backslash), or the instrument of this line
	const std::string& source = args[0];
	std::string barName = source.substr(0, source.find('.'));
	auto barIt = sharedData.barsIndexes.find(barName);
	if (barIt == sharedData.barsIndexes.end()) return fail(source + " is not a bar");
	int sourceBar = barIt->second;
	if (sourceBar == getLastBarIndex()) return fail("a bar can't apply its own lines");
	int sourceInst = lastInstrumentIndex;
	if (source.find('.') != std::string::npos) {
		std::string instName = source.substr(source.find('.') + 1);
		if (!startsWith(instName, "\\")) instName = "\\" + instName;
		auto instIt = sharedData.instrumentIndexes.find(instName);
		if (instIt == sharedData.instrumentIndexes.end()) return fail(instName + ": unknown instrument");
		sourceInst = instIt->second;
	}
	Instrument& src = sharedData.instruments[sourceInst];
	if (src.durs.find(sourceBar) == src.durs.end() || src.durs[sourceBar].empty()) return fail(source + " has no line");
	const std::vector<int>& durs = src.durs[sourceBar];
	const std::vector<std::vector<float>>& notes = src.notes[sourceBar];
	const std::vector<int>& ties = src.tieIndexes[sourceBar];
	bool targetRhythm = sharedData.instruments[lastInstrumentIndex].isRhythm();
	bool sourceRhythm = src.isRhythm();
	// the events of the line: an onset, a note tied from the previous note, or a rest
	size_t numEvents = durs.size();
	auto isRest = [&](size_t i) { return i >= notes.size() || notes[i].empty() || notes[i][0] < 0; };
	auto isTiedFromPrev = [&](size_t i) { return i > 0 && i - 1 < ties.size() && ties[i-1] > -1 && !isRest(i); };
	std::vector<size_t> onsets;
	for (size_t i = 0; i < numEvents; i++) {
		if (!isRest(i) && !isTiedFromPrev(i)) onsets.push_back(i);
	}
	// a duration is a number with dots, maybe followed by a dynamic or articulations (e.g. 4. or 8\f), and a rest starts with r
	auto isDurationArg = [](const std::string& str) { return !str.empty() && (isdigit(str[0]) || (str[0] == 'r' && (str.size() == 1 || isdigit(str[1])))); };
	bool durationsMode = isDurationArg(args[1]);
	for (size_t i = 1; i < args.size(); i++) {
		if (isDurationArg(args[i]) != durationsMode) return fail("the arguments after the bar must be either notes or durations, not both");
	}

	if (durationsMode) {
		// a rhythm staff takes the durations as they are
		if (targetRhythm) {
			output = {args.begin()+1, args.end()};
			return std::make_pair(std::string(), output);
		}
		if (sourceRhythm) return fail("a melodic line can't apply the notes of a rhythm staff (" + source + ") to durations");
		if (onsets.empty()) return fail(source + " has no notes");
		// the notes of the onsets, spelled with the names of the notes as they are written, their accidentals, and their octaves
		static const char noteNames[] = {'c', 'd', 'e', 'f', 'g', 'a', 'b'};
		static const int noteSemitones[] = {0, 2, 4, 5, 7, 9, 11};
		auto spell = [&](size_t i) {
			std::vector<std::string> chordNotes;
			for (size_t j = 0; j < notes[i].size(); j++) {
				int natural = 0;
				if (src.scoreNotes.count(sourceBar) && i < src.scoreNotes[sourceBar].size() && j < src.scoreNotes[sourceBar][i].size()) {
					natural = src.scoreNotes[sourceBar][i][j];
				}
				if (natural < 0 || natural > 6) natural = 0;
				// c without octave marks is MIDI note 48
				double fromC = notes[i][j] - 48.0 - noteSemitones[natural];
				int octave = (int)std::lround(fromC / 12.0);
				int quarterTones = (int)std::lround((fromC - octave * 12.0) * 2.0);
				std::string note(1, noteNames[natural]);
				for (int q = 0; q < std::abs(quarterTones) / 2; q++) note += (quarterTones > 0 ? "is" : "es");
				if (std::abs(quarterTones) % 2 == 1) note += (quarterTones > 0 ? "ih" : "eh");
				for (int o = 0; o < std::abs(octave); o++) note += (octave > 0 ? "'" : ",");
				chordNotes.push_back(note);
			}
			if (chordNotes.size() == 1) return chordNotes[0];
			std::string chord = "<";
			for (size_t j = 0; j < chordNotes.size(); j++) chord += (j > 0 ? " " : "") + chordNotes[j];
			return chord + ">";
		};
		size_t noteNdx = 0;
		for (size_t i = 1; i < args.size(); i++) {
			if (args[i][0] == 'r') {
				output.push_back(args[i]);
				continue;
			}
			output.push_back(spell(onsets[noteNdx % onsets.size()]) + args[i]);
			noteNdx++;
		}
		return std::make_pair(std::string(), output);
	}

	// notes
	if (targetRhythm) return fail("a rhythm staff can't apply notes, only durations");
	if (src.scoreTupletRatios.count(sourceBar) && !src.scoreTupletRatios[sourceBar].empty()) {
		return fail("the durations of a line with tuplets can't be applied");
	}
	// every note is its pitch (a note, or a chord in angle brackets), and what follows it (dynamics, articulations, etc.)
	std::vector<std::pair<std::string, std::string>> argNotes;
	for (size_t i = 1; i < args.size(); i++) {
		const std::string& arg = args[i];
		size_t pitchEnd;
		if (arg[0] == '<') {
			pitchEnd = arg.find('>');
			if (pitchEnd == std::string::npos) return fail(arg + ": unclosed chord");
			pitchEnd++;
		}
		else {
			if (arg[0] < 'a' || arg[0] > 'g') return fail(arg + ": not a note");
			pitchEnd = 1;
			// accidentals (is, es, ih, eh, which can be combined), and octave marks
			while (pitchEnd + 1 < arg.size() && (arg[pitchEnd] == 'i' || arg[pitchEnd] == 'e') && (arg[pitchEnd+1] == 's' || arg[pitchEnd+1] == 'h')) pitchEnd += 2;
			while (pitchEnd < arg.size() && (arg[pitchEnd] == '\'' || arg[pitchEnd] == ',')) pitchEnd++;
		}
		std::string rest = arg.substr(pitchEnd);
		if (!rest.empty() && isdigit(rest[0])) return fail(arg + ": the notes of \\apply take their durations from " + source);
		argNotes.push_back(std::make_pair(arg.substr(0, pitchEnd), rest));
	}
	if (onsets.empty()) return fail(source + " has no notes");
	if (argNotes.size() > onsets.size()) {
		return fail(std::to_string(argNotes.size()) + " notes are more than the " + std::to_string(onsets.size()) + " onsets of " + source);
	}
	// the start of every event in the bar
	std::vector<int> starts(numEvents, 0);
	int barLength = 0;
	for (size_t i = 0; i < numEvents; i++) {
		starts[i] = barLength;
		barLength += durs[i];
	}
	// the onset of every note, the one closest to an equal part of the bar, leaving enough onsets for the notes after it
	size_t numNotes = argNotes.size();
	std::vector<int> noteOfEvent(numEvents, -1);
	int prevOnset = -1;
	for (size_t k = 0; k < numNotes; k++) {
		double target = (double)barLength * k / numNotes;
		int first = prevOnset + 1;
		int last = (int)(onsets.size() - (numNotes - k));
		int best = first;
		for (int o = first; o <= last; o++) {
			if (std::fabs(starts[onsets[o]] - target) < std::fabs(starts[onsets[best]] - target)) best = o;
		}
		noteOfEvent[onsets[best]] = (int)k;
		prevOnset = best;
	}
	// the durations, plain and dotted, in units of the minimum duration, from the longest to the shortest
	std::vector<std::pair<int, std::string>> durations;
	for (int d : {1, 2, 4, 8, 16, 32, 64}) {
		int length = MINDUR / d;
		durations.push_back(std::make_pair(length, std::to_string(d)));
		if (length % 2 == 0) durations.push_back(std::make_pair(length * 3 / 2, std::to_string(d) + "."));
	}
	std::sort(durations.begin(), durations.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
	auto durationsOf = [&](int length, std::vector<std::string>& result) {
		for (const auto& d : durations) {
			while (length >= d.first) {
				result.push_back(d.second);
				length -= d.first;
			}
		}
		return length == 0;
	};
	static const std::vector<std::string> dynamicNames = {"\\ppp", "\\pp", "\\p", "\\mp", "\\mf", "\\f", "\\ff", "\\fff"};
	static const std::string articulationChars = " ^+-!>._";
	// the dynamic that holds at an event of the line, the last one written at or before it in the bar, or -1
	auto dynamicAt = [&](size_t i) {
		int dynamic = -1;
		if (!src.scoreDynamics.count(sourceBar)) return dynamic;
		const std::vector<int>& written = src.scoreDynamics[sourceBar];
		for (size_t e = 0; e <= i && e < written.size(); e++) {
			if (written[e] >= 0) dynamic = written[e];
		}
		return dynamic;
	};
	auto hasDynamic = [](const std::string& str) {
		for (size_t c = 0; c + 1 < str.size(); c++) {
			if (str[c] == '\\' && (str[c+1] == 'p' || str[c+1] == 'm' || str[c+1] == 'f')) return true;
		}
		return false;
	};
	auto hasArticulation = [](const std::string& str) {
		for (size_t c = 0; c + 1 < str.size(); c++) {
			if ((str[c] == '-' || str[c] == '^' || str[c] == '_') && std::string("^+-!>._").find(str[c+1]) != std::string::npos) return true;
		}
		return false;
	};
	// the notes and rests of the new line, with their lengths, before they are written with durations
	struct Segment {
		int note; // the index of the note, or -1 for a rest
		int length;
		size_t event; // the onset of a note
	};
	std::vector<Segment> segments;
	int activeNote = -1;
	for (size_t i = 0; i < numEvents; i++) {
		if (noteOfEvent[i] >= 0) {
			activeNote = noteOfEvent[i];
			segments.push_back({activeNote, durs[i], i});
		}
		else if (isRest(i) || activeNote < 0) {
			// a rest of the line, or an onset without a note after a rest, which ends the note before it
			activeNote = -1;
			if (!segments.empty() && segments.back().note < 0) segments.back().length += durs[i];
			else segments.push_back({-1, durs[i], i});
		}
		else {
			// an onset without a note, or a note tied from the previous note, lengthens the note before it
			segments.back().length += durs[i];
		}
	}
	int lastDynamic = -2;
	for (const Segment& seg : segments) {
		std::vector<std::string> parts;
		if (!durationsOf(seg.length, parts)) return fail("the durations of " + source + " can't be applied");
		if (seg.note < 0) {
			for (const std::string& part : parts) output.push_back("r" + part);
			continue;
		}
		const std::string& pitch = argNotes[seg.note].first;
		std::string after = argNotes[seg.note].second;
		// the dynamic and the articulations of the onset, unless the note has its own, and a dynamic only when it changes
		std::string carried;
		if (!hasDynamic(after)) {
			int dynamic = dynamicAt(seg.event);
			if (dynamic >= 0 && dynamic < (int)dynamicNames.size() && dynamic != lastDynamic) carried += dynamicNames[dynamic];
			if (dynamic >= 0) lastDynamic = dynamic;
		}
		else {
			lastDynamic = -2;
		}
		if (!hasArticulation(after) && src.articulations.count(sourceBar) && seg.event < src.articulations[sourceBar].size()) {
			for (int a : src.articulations[sourceBar][seg.event]) {
				if (a > 0 && a < (int)articulationChars.size()) carried += std::string("-") + articulationChars[a];
			}
		}
		// a length that one duration can't write is written with ties
		for (size_t p = 0; p < parts.size(); p++) {
			std::string token = pitch + parts[p];
			if (p == 0) token += carried + after;
			if (p + 1 < parts.size()) token += "~";
			output.push_back(token);
		}
	}
	return std::make_pair(std::string(), output);
}

//--------------------------------------------------------------
ofApp::NameKind ofApp::nameKind(const std::string& name)
{
	// the kind of a name defined in a session, which decides both its signature (see commandSpec()) and its dispatcher (see parseCommand())
	// the names are stored with their backslash, and if a name is defined as more than one kind, the first kind below is the one it is
	// (functionIndexes is read without its lock, as the list of function names it replaced was, and it changes only when a function is defined)
	if (variables.find(name) != variables.end()) return NameKind::variable;
	if (sharedData.instrumentIndexes.find(name) != sharedData.instrumentIndexes.end()) return NameKind::instrument;
	if (sharedData.loopsIndexes.find(name) != sharedData.loopsIndexes.end()) return NameKind::loop;
	if (functionIndexes.find(name) != functionIndexes.end()) return NameKind::function;
	if (listIndexes.find(name) != listIndexes.end()) return NameKind::list;
	if (oscClients.find(name) != oscClients.end()) return NameKind::oscClient;
	if (oscServers.find(name) != oscServers.end()) return NameKind::oscServer;
	if (instGroups.find(name) != instGroups.end()) return NameKind::group;
	return NameKind::none;
}

//--------------------------------------------------------------
CmdOutput ofApp::evalArgs(const std::vector<std::string>& args, int lineNum, int numLines)
{
	// the arguments are evaluated as the arguments of a command, after a placeholder for that command, which evaluateTokens() leaves as it is
	bool hasCommand = std::any_of(args.begin(), args.end(), isCommandToken);
	CmdOutput result = CmdOutput();
	if (!hasCommand) {
		result.outputVec = args;
		return result;
	}
	std::vector<std::string> tokens = {"\\"};
	tokens.insert(tokens.end(), args.begin(), args.end());
	result = evaluateTokens(tokens, lineNum, numLines);
	if (result.errorCode == 3) return result;
	if (!result.outputVec.empty()) result.outputVec.erase(result.outputVec.begin());
	return result;
}

//--------------------------------------------------------------
CmdOutput ofApp::evalNames(const std::vector<std::string>& args, int lineNum, int numLines)
{
	// the instrument names (and \all) are taken as they are written, and any other command is evaluated on its own (see ArgsMode::names)
	CmdOutput result = CmdOutput();
	for (const std::string& arg : args) {
		if (arg == "\\all" || sharedData.instrumentIndexes.find(arg) != sharedData.instrumentIndexes.end() || arg.empty() || arg[0] != '\\') {
			result.outputVec.push_back(arg);
			continue;
		}
		CmdOutput evaluated = evalArgs({arg}, lineNum, numLines);
		if (evaluated.errorCode == 3) return evaluated;
		result.outputVec.insert(result.outputVec.end(), evaluated.outputVec.begin(), evaluated.outputVec.end());
	}
	return result;
}

//--------------------------------------------------------------
std::string ofApp::nameTakenError(const std::string& name, NameKind kind)
{
	// a name can be defined as one kind only (e.g. a list and a bar can't have the same name), as it is called the same way for every kind
	// redefining a name as its own kind is left to the command that defines it (e.g. a function gets a new body)
	NameKind existing = nameKind(name);
	if (existing == NameKind::none || existing == kind) return "";
	static const std::map<NameKind, std::string> kindNames = {
		{NameKind::variable, "a variable"}, {NameKind::instrument, "an instrument"}, {NameKind::loop, "a bar or loop"},
		{NameKind::function, "a function"}, {NameKind::list, "a list"}, {NameKind::oscClient, "an OSC client"},
		{NameKind::oscServer, "an OSC server"}, {NameKind::group, "a group"}
	};
	return name.substr(1) + " can't be the name of " + kindNames.at(kind) + ", " + name + " is " + kindNames.at(existing);
}

//--------------------------------------------------------------
bool ofApp::functionTakesArgs(const std::string& name)
{
	// read without the lock of the functions, as nameKind() reads functionIndexes, since this is called while parsing
	// which can happen while that lock is held, and the functions change only when they are defined
	auto indexIt = functionIndexes.find(name);
	if (indexIt == functionIndexes.end()) return true;
	auto funcIt = sharedData.functions.find(indexIt->second);
	if (funcIt == sharedData.functions.end()) return true;
	// arguments set with .setargs are used instead of the ones a function is called with
	if (funcIt->second.getNumArgs() > 0) return false;
	// an argument is a $ followed by a number from 1 (see isFunction())
	std::string body = funcIt->second.printStr();
	for (size_t i = 0; i + 1 < body.size(); i++) {
		if (body[i] == '$' && body[i+1] >= '1' && body[i+1] <= '9') return true;
	}
	return false;
}

//--------------------------------------------------------------
ofApp::CommandSpec ofApp::commandSpec(const std::string& token)
{
	size_t dot = token.find('.');
	std::string name = token.substr(0, dot);
	std::string method = (dot == std::string::npos ? "" : token.substr(dot + 1));
	auto commandIt = commandTable().find(name);
	if (commandIt != commandTable().end()) return commandIt->second.spec;
	NameKind kind = nameKind(name);
	if (kind == NameKind::none) return CommandSpec();
	// a function called without a method takes as many arguments as its body uses, which can be none
	if (kind == NameKind::function && method.empty() && !functionTakesArgs(name)) {
		CommandSpec spec = methodSpecs.at(std::make_pair(NameKind::function, std::string("*")));
		spec.takesNothing = true;
		return spec;
	}
	auto methodIt = methodSpecs.find(std::make_pair(kind, method));
	if (methodIt != methodSpecs.end()) return methodIt->second;
	methodIt = methodSpecs.find(std::make_pair(kind, std::string("*")));
	if (methodIt != methodSpecs.end()) return methodIt->second;
	return CommandSpec();
}

//--------------------------------------------------------------
// \time, \tempo, and \beatat take only their own values, so that they can be followed by more statements in the same line
// (e.g. in a bar written in one line), and these are returned to be parsed after the command
// \time takes a time signature (e.g. 4/4), \tempo a tempo (e.g. 120) or a duration, = and a tempo (e.g. 4 = 120)
// and \beatat one or more durations (e.g. 4. 4.), and in \bars all of them can have values for every bar, separated by | (e.g. 4/4 | 3/4)
std::vector<std::string> ofApp::splitBarCommandArgs(CmdInput& cmdInput)
{
	const std::vector<std::string>& v = cmdInput.inputVec;
	auto onlyChars = [](const std::string& str, const char* chars) { return !str.empty() && str.find_first_not_of(chars) == std::string::npos; };
	size_t end = 1;
	if (v[0] == "\\time") {
		while (end < v.size() && onlyChars(v[end], "0123456789/|")) end++;
	}
	else if (v[0] == "\\tempo") {
		while (end < v.size() && (onlyChars(v[end], "0123456789.|") || v[end] == "=")) end++;
	}
	else {
		while (end < v.size() && onlyChars(v[end], "0123456789.|")) end++;
	}
	std::vector<std::string> rest(v.begin() + std::min(end, v.size()), v.end());
	cmdInput.inputVec.resize(std::min(end, v.size()));
	return rest;
}

//--------------------------------------------------------------
CmdOutput ofApp::prepareArgs(CmdInput& cmdInput, int lineNum, int numLines)
{
	// evaluates the arguments of a command, according to its mode (see CommandSpec), before parseCommand() runs its code
	// the arguments of a command inside a line have been evaluated already (see evaluateTokens())
	// and a line of a bar definition that starts with a command is not evaluated (melodic lines are evaluated in parseMelodicLine())
	if (cmdInput.argsEvaluated || cmdInput.inputVec.size() < 2 || (cmdInput.isMainCmd && parsingBar)) return CmdOutput();
	ArgsMode argsMode = commandSpec(cmdInput.inputVec[0]).args;
	if (argsMode == ArgsMode::raw) return CmdOutput();
	// with values, all the arguments are evaluated, and with names, instrument names (and \all) are taken as written, and other commands are evaluated
	std::vector<std::string> args = {cmdInput.inputVec.begin()+1, cmdInput.inputVec.end()};
	CmdOutput evaluated = (argsMode == ArgsMode::values ? evalArgs(args, lineNum, numLines) : evalNames(args, lineNum, numLines));
	if (evaluated.errorCode == 3) return evaluated;
	cmdInput.inputVec.resize(1);
	cmdInput.inputVec.insert(cmdInput.inputVec.end(), evaluated.outputVec.begin(), evaluated.outputVec.end());
	cmdInput.argsEvaluated = true;
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::parseCommand(CmdInput cmdInput, int lineNum, int numLines)
{
	// the arguments arrive prepared for the mode of the command (see CommandSpec), and the handlers of the commands follow
	CmdOutput prepared = prepareArgs(cmdInput, lineNum, numLines);
	if (prepared.errorCode == 3) return prepared;

	// the handler of the command (see commandHandler()), then a variable definition (e.g. x = 5)
	// and then the names defined in a session (instruments, bars and loops, functions, etc.)
	CommandHandler handler = commandHandler(cmdInput.inputVec[0]);
	if (handler != nullptr) return (this->*handler)(cmdInput, lineNum, numLines);
	if (cmdInput.inputVec.size() >= 3 && cmdInput.inputVec[1].compare("=") == 0) return defineVariable(cmdInput, lineNum, numLines);
	// a name defined in a session goes to the dispatcher of its kind (see nameKind()), which returns a pair of whether it handled it and its output
	std::string name = cmdInput.inputVec[0].substr(0, cmdInput.inputVec[0].find('.'));
	std::pair<bool, CmdOutput> p = std::make_pair(false, CmdOutput());
	switch (nameKind(name)) {
		case NameKind::variable: p = isVariable(cmdInput.inputVec, cmdInput.isMainCmd, lineNum, numLines); break;
		case NameKind::instrument: p = isInstrument(cmdInput.inputVec, lineNum, numLines); break;
		case NameKind::loop: p = isBarLoop(cmdInput.inputVec, cmdInput.isMainCmd, lineNum, numLines); break;
		case NameKind::function: p = isFunction(cmdInput.inputVec, lineNum, numLines); break;
		case NameKind::list: p = isList(cmdInput.inputVec, lineNum, numLines); break;
		case NameKind::oscClient: p = isOscClient(cmdInput.inputVec, lineNum, numLines); break;
		case NameKind::oscServer: p = isOscServer(cmdInput.inputVec, lineNum, numLines); break;
		case NameKind::group: p = isGroup(cmdInput.inputVec, lineNum, numLines); break;
		case NameKind::none: break;
	}
	if (p.first) return p.second;
	return genError(cmdInput.inputVec[0] + ": unknown command");
}

//--------------------------------------------------------------
const std::map<std::string, ofApp::CommandEntry>& ofApp::commandTable()
{
	// every command of LiveLily: its name, its handler, its signature (the mode of its arguments, and whether it takes none, see CommandSpec)
	// and whether it also takes methods (e.g. \insts.init), with the handler of the name before the dot
	// e.g. {"\\numinsts", {&ofApp::cmdNuminsts, {ArgsMode::values, true}}} for a command with values that takes no arguments
	// a handler can be shared by more names (e.g. \tuplet and \tup), and it can tell them apart by cmdInput.inputVec[0]
	static const std::map<std::string, CommandEntry> commands = {
		// commands that store or read their arguments as they are written, or evaluate them themselves
		{"\\insts", {&ofApp::cmdInsts, {ArgsMode::raw}, true}},
		{"\\bar", {&ofApp::cmdBar, {ArgsMode::raw}}},
		{"\\bars", {&ofApp::cmdBars, {ArgsMode::raw}}},
		{"\\loop", {&ofApp::cmdLoop, {ArgsMode::raw}}},
		{"\\group", {&ofApp::cmdGroup, {ArgsMode::raw}}},
		{"\\function", {&ofApp::cmdFunction, {ArgsMode::raw}}},
		{"\\help", {&ofApp::cmdHelp, {ArgsMode::raw}}},
		{"\\merge", {&ofApp::cmdMerge, {ArgsMode::raw}}},
		{"\\unmerge", {&ofApp::cmdMerge, {ArgsMode::raw}}},
		{"\\apply", {&ofApp::cmdApply, {ArgsMode::raw}}},
		{"\\list", {&ofApp::cmdList, {ArgsMode::raw}}},
		// commands with values (on, off, numbers, notes, etc.)
		{"\\print", {&ofApp::cmdPrint, {ArgsMode::values}}},
		{"\\transpose", {&ofApp::cmdTranspose, {ArgsMode::values}}},
		{"\\euclide", {&ofApp::cmdEuclide, {ArgsMode::values}}},
		{"\\accoffset", {&ofApp::cmdAccoffset, {ArgsMode::values}}},
		{"\\active", {&ofApp::cmdActive, {ArgsMode::values, true}}},
		{"\\inactive", {&ofApp::cmdInactive, {ArgsMode::values, true}}},
		{"\\buffersize", {&ofApp::cmdBuffersize, {ArgsMode::values}}},
		{"\\clicktrack", {&ofApp::cmdClicktrack, {ArgsMode::values}, true}},
		{"\\closeserialport", {&ofApp::cmdCloseserialport, {ArgsMode::values, true}}},
		{"\\cursor", {&ofApp::cmdCursor, {ArgsMode::values}}},
		{"\\framerate", {&ofApp::cmdFramerate, {ArgsMode::values}}},
		{"\\inchannels", {&ofApp::cmdInchannels, {ArgsMode::values}}},
		{"\\outchannels", {&ofApp::cmdOutchannels, {ArgsMode::values}}},
		{"\\nchannels", {&ofApp::cmdNchannels, {ArgsMode::values}}},
		{"\\listaudiodev", {&ofApp::cmdListaudiodev, {ArgsMode::values, true}}},
		{"\\listmidiports", {&ofApp::cmdListmidiports, {ArgsMode::values, true}}},
		{"\\listserialports", {&ofApp::cmdListserialports, {ArgsMode::values, true}}},
		{"\\midiclock", {&ofApp::cmdMidiclock, {ArgsMode::values}}},
		{"\\ppqn", {&ofApp::cmdPpqn, {ArgsMode::values}}},
		{"\\samplerate", {&ofApp::cmdSamplerate, {ArgsMode::values}}},
		{"\\tune", {&ofApp::cmdTune, {ArgsMode::values}}},
		// commands that return values inside other commands
		{"\\rand", {&ofApp::cmdRandom, {ArgsMode::values}}},
		{"\\random", {&ofApp::cmdRandom, {ArgsMode::values}}},
		{"\\barnum", {&ofApp::cmdBarnum, {ArgsMode::values, true}}},
		{"\\beatcount", {&ofApp::cmdBeatcount, {ArgsMode::values, true}}},
		{"\\mouse", {&ofApp::cmdMouse, {ArgsMode::values, true}}},
		{"\\mousex", {&ofApp::cmdMousex, {ArgsMode::values, true}}},
		{"\\mousey", {&ofApp::cmdMousey, {ArgsMode::values, true}}},
		{"\\expand", {&ofApp::cmdExpand, {ArgsMode::values}}},
		{"\\system", {&ofApp::cmdSystem, {ArgsMode::values}}},
		{"\\pystr", {&ofApp::cmdPystr, {ArgsMode::values}}},
		// \remaining takes the name of a bar, as it is written
		{"\\remaining", {&ofApp::cmdRemaining, {ArgsMode::raw}}},
		// commands inside bars (\time, \tempo, and \beatat start a line of a bar, which is not evaluated, see parseCommand())
		{"\\time", {&ofApp::cmdTime, {ArgsMode::values}}},
		{"\\tempo", {&ofApp::cmdTempo, {ArgsMode::values}}},
		{"\\beatat", {&ofApp::cmdBeatat, {ArgsMode::values}}},
		{"\\clef", {&ofApp::cmdClef, {ArgsMode::values}}},
		{"\\ottava", {&ofApp::cmdOttava, {ArgsMode::values}}},
		{"\\ott", {&ofApp::cmdOttava, {ArgsMode::values}}},
		{"\\tuplet", {&ofApp::cmdTuplet, {ArgsMode::values}}},
		{"\\tup", {&ofApp::cmdTuplet, {ArgsMode::values}}},
		{"\\cluster", {&ofApp::cmdCluster, {ArgsMode::values}}},
		// commands of playback and state, with instrument names, or values
		{"\\mute", {&ofApp::cmdMute, {ArgsMode::names}}},
		{"\\unmute", {&ofApp::cmdUnmute, {ArgsMode::names}}},
		{"\\solo", {&ofApp::cmdSolo, {ArgsMode::names}}},
		{"\\mutenow", {&ofApp::cmdMutenow, {ArgsMode::names}}},
		{"\\unmutenow", {&ofApp::cmdUnmutenow, {ArgsMode::names}}},
		{"\\solonow", {&ofApp::cmdSolonow, {ArgsMode::names}}},
		{"\\instorder", {&ofApp::cmdInstorder, {ArgsMode::names}}},
		{"\\score", {&ofApp::cmdScore, {ArgsMode::values}, true}},
		{"\\editor", {&ofApp::cmdEditor, {ArgsMode::values}, true}},
		{"\\notedur", {&ofApp::cmdNotedur, {ArgsMode::values}, true}},
		// commands of input and output (OSC, serial, MIDI, audio devices, and the languages of a pane)
		{"\\osc", {&ofApp::cmdOsc, {ArgsMode::values}}},
		{"\\oscclient", {&ofApp::cmdOsc, {ArgsMode::values}}},
		{"\\oscserver", {&ofApp::cmdOscserver, {ArgsMode::values}}},
		{"\\fromosc", {&ofApp::cmdFromosc, {ArgsMode::values}}},
		{"\\sendkeys", {&ofApp::cmdSendkeys, {ArgsMode::values}}},
		{"\\sendlines", {&ofApp::cmdSendlines, {ArgsMode::values}}},
		{"\\serialprint", {&ofApp::cmdSerialprint, {ArgsMode::values}}},
		{"\\serialwrite", {&ofApp::cmdSerialwrite, {ArgsMode::values}}},
		{"\\openserialport", {&ofApp::cmdOpenserialport, {ArgsMode::values}}},
		{"\\openmidiport", {&ofApp::cmdOpenmidiport, {ArgsMode::values}}},
		{"\\ctlchange", {&ofApp::cmdCtlchange, {ArgsMode::values}}},
		{"\\pgmchange", {&ofApp::cmdPgmchange, {ArgsMode::values}}},
		{"\\setaudiodev", {&ofApp::cmdSetaudiodev, {ArgsMode::values}}},
		{"\\setinaudiodev", {&ofApp::cmdSetinaudiodev, {ArgsMode::values}}},
		{"\\setoutaudiodev", {&ofApp::cmdSetoutaudiodev, {ArgsMode::values}}},
		{"\\python", {&ofApp::cmdPython, {ArgsMode::values}, true}},
		{"\\lua", {&ofApp::cmdLua, {ArgsMode::values}}},
		{"\\livelily", {&ofApp::cmdLivelily, {ArgsMode::values}}},
		{"\\maestro", {&ofApp::cmdMaestro, {ArgsMode::values}, true}},
		{"\\numinsts", {&ofApp::cmdNuminsts, {ArgsMode::values, true}}},
		// \return gives the value of the function whose body it is in
		{"\\return", {&ofApp::cmdReturn, {ArgsMode::values}}}
	};
	return commands;
}

//--------------------------------------------------------------
ofApp::CommandHandler ofApp::commandHandler(const std::string& name)
{
	// the handler of a command, by its name, or by the name before the dot, if the command takes methods (see commandTable())
	auto it = commandTable().find(name);
	if (it != commandTable().end()) return it->second.handler;
	size_t dot = name.find('.');
	if (dot != std::string::npos) {
		auto methodIt = commandTable().find(name.substr(0, dot));
		if (methodIt != commandTable().end() && methodIt->second.takesMethods) return methodIt->second.handler;
	}
	return nullptr;
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdTime(CmdInput& cmdInput, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	if (barError) return cmdOutput;
	if (!parsingBar && !parsingBars) {
		return genError("\\time command must be placed inside a bar(s) definition");
	}
	// the rest of the line after the values of the command is parsed after it (see splitBarCommandArgs())
	std::vector<std::string> restOfLine = splitBarCommandArgs(cmdInput);
	if (parsingBars) {
		CmdOutput barsOutput = selectBarOfBarsCommand(cmdInput);
		if (barsOutput.errorCode == 3) return barsOutput;
	}
	if (cmdInput.inputVec.size() > 2) {
		barError = true;
		return genError("\\time command takes one argument only");
	}
	else if (cmdInput.inputVec.size() < 2) {
		barError = true;
		return genError("\\time command needs one arguments");
	}
	int divisionSym = cmdInput.inputVec[1].find("/");
	std::string numeratorStr = cmdInput.inputVec[1].substr(0, divisionSym);
	std::string denominatorStr = cmdInput.inputVec[1].substr(divisionSym+1);
	int numeratorLocal = 0;
	int denominatorLocal = 0;
	if (isNumber(numeratorStr)) {
		numeratorLocal = stoi(numeratorStr);
	}
	else {
		barError = true;
		return genError(numeratorStr + " is not an int");
	}
	if (isNumber(denominatorStr)) {
		denominatorLocal = stoi(denominatorStr);
	}
	else {
		barError = true;
		return genError(denominatorStr + " is not an int");
	}
	if (numeratorLocal == 0 || denominatorLocal == 0) {
		barError = true;
		return genError("numerator or denominator can't be zero");
	}
	// a \time command must be placed inside a bar definition
	// and the bar index has already been stored, so we can safely query the last bar index
	int bar = getLastBarIndex();
	// a new bar takes the meter and the tempo of the previous bar (see storeNewBar())
	int prevDenominator = (sharedData.denominator.find(bar) != sharedData.denominator.end() ? sharedData.denominator[bar] : denominatorLocal);
	sharedData.numerator[bar] = numeratorLocal;
	sharedData.denominator[bar] = denominatorLocal;
	sharedData.numBeats[bar] = sharedData.numerator[bar] * (MINDUR / sharedData.denominator[bar]);
	// the sequencer counts the beats of the meter with tempoMs, the duration of a beat (the denominator), in ms
	// so it follows the denominator, and the tempo stays the same, as tempo is the duration of the minimum duration (see \tempo)
	// e.g. after a bar of 4/4 with a quarter at 120 BPM, a bar of 5/8 has eighths of 250ms, and not of 500ms
	// a \tempo after \time in the same bar sets them again
	if (sharedData.tempo.find(bar) != sharedData.tempo.end()) {
		sharedData.tempoMs[bar] = sharedData.tempo[bar] * (double)(MINDUR / sharedData.denominator[bar]);
		sharedData.PPQNPerUs[bar] = (uint64_t)(sharedData.tempoMs[bar] / (float)sharedData.PPQN) * 1000;
		if (prevDenominator != denominatorLocal) {
			// the base of \tempo in beats of the new meter (e.g. a quarter is 2 eighths), if it is a whole number of them
			long scaled = (long)sharedData.BPMMultiplier[bar] * denominatorLocal;
			if (scaled > 0 && scaled % prevDenominator == 0) sharedData.BPMMultiplier[bar] = (int)(scaled / prevDenominator);
			// the values of \beatat of the previous bar are in beats of its meter, so the beats are pulsed one by one
			// until a \beatat in this bar sets them (e.g. \beatat 4. 4 after \time 5/8)
			sharedData.beatAtValues[bar] = {1};
			sharedData.beatAtDifferentThanDivisor[bar] = true;
		}
	}
	if (sharedData.BPMMultiplier.find(bar) == sharedData.BPMMultiplier.end()) {
		sharedData.BPMMultiplier[bar] = 1;
		sharedData.beatAtValues[bar] = {1};
		sharedData.beatAtDifferentThanDivisor[bar] = true;
	}
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		it->second.setMeter(bar, sharedData.numerator[bar],
				sharedData.denominator[bar], sharedData.numBeats[bar]);
		//if (it->second.sendToPart) {
		//	sendBarIndexBeatMeterToPart(it->first, bar);
		//}
		ofxOscMessage m;
		m.setAddress("/time");
		m.addIntArg(bar);
		m.addIntArg(sharedData.numerator[bar]);
		m.addIntArg(sharedData.denominator[bar]);
		m.addIntArg(sharedData.numBeats[bar]);
		sendToParts(m, false);
	}
	// store the time information to the barLines string
	int barIndex = getLastLoopIndex();
	//for (int i = 0; i < editors[whichPane].getTabSize()-1; i++) {
	//	sharedData.barLines[sharedData.barsOrdered[barIndex]] += " ";
	//}
	sharedData.barLines[sharedData.barsOrdered[barIndex]] += '\t';
	for (size_t i = 0; i < cmdInput.inputVec.size(); i++) {
		sharedData.barLines[sharedData.barsOrdered[barIndex]] += cmdInput.inputVec[i];
		if (i < cmdInput.inputVec.size()-1) sharedData.barLines[sharedData.barsOrdered[barIndex]] += " ";
	}
	sharedData.barLines[sharedData.barsOrdered[barIndex]] += '\n';
	if (!restOfLine.empty()) {
		std::pair<int, std::string> p = parseString(genStrFromVec(restOfLine), lineNum, numLines);
		if (p.first == 3) return genError(p.second);
	}
	return cmdOutput;
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdTempo(CmdInput& cmdInput, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	if (barError) return cmdOutput;
	if (!parsingBar && !parsingBars) {
		return genError("\\tempo command must be placed inside a bar(s) definition");
	}
	// the rest of the line after the values of the command is parsed after it (see splitBarCommandArgs())
	std::vector<std::string> restOfLine = splitBarCommandArgs(cmdInput);
	if (parsingBars) {
		CmdOutput barsOutput = selectBarOfBarsCommand(cmdInput);
		if (barsOutput.errorCode == 3) return barsOutput;
	}
	if (cmdInput.inputVec.size() > 4) {
		barError = true;
		return genError("\\tempo command takes maximum three arguments");
	}
	if (cmdInput.inputVec.size() < 2) {
		barError = true;
		return genError("\\tempo command needs at least one argument");
	}
	// a duration and = without a tempo (e.g. \tempo 4 =), or two values without = (e.g. \tempo 4 120)
	if (cmdInput.inputVec.size() == 3) {
		barError = true;
		return genError("\\tempo command takes either \"tempo\" or \"duration = tempo\" as argument");
	}
	// a \tempo command must be placed inside a bar definition
	// and the bar index has already been stored, so we can safely query the last bar index
	int bar = getLastBarIndex();
	if (cmdInput.inputVec.size() == 4) {
		if (cmdInput.inputVec[2].compare("=") != 0) {
			barError = true;
			return genError("\\tempo command takes either \"tempo\" or \"duration = tempo\" as argument");
		}
		else {
			std::pair<int, std::string> err = getBaseDurError(cmdInput.inputVec[1]);
			if (err.first > 0) {
				barError = true;
				return genError(err.second);
			}
			if (!isNumber(cmdInput.inputVec[3])) {
				barError = true;
				return genError(cmdInput.inputVec[3] + " is not a number");
			}
			int tempoVal = stoi(cmdInput.inputVec[3]);
			if (tempoVal <= 0) {
				barError = true;
				return genError("tempo can't be zero or negative");
			}
			sharedData.BPMMultiplier[bar] = getBaseDurValue(cmdInput.inputVec[1], sharedData.denominator[bar]);
			sharedData.beatAtValues[bar] = {sharedData.BPMMultiplier[bar]};
			sharedData.beatAtDifferentThanDivisor[bar] = true;
			sharedData.BPMTempi[bar] = tempoVal;
			// convert BPM to ms
			sharedData.tempoMs[bar] = 1000.0 / ((float)(sharedData.BPMTempi[bar]  * sharedData.BPMMultiplier[bar]) / 60.0);
			// get the tempo of the minimum duration
			sharedData.tempo[bar] = sharedData.tempoMs[bar] / (MINDUR / sharedData.denominator[bar]);
			// get the usec duration of the PPQM
			sharedData.PPQNPerUs[bar] = (uint64_t)(sharedData.tempoMs[bar] / (float)sharedData.PPQN) * 1000;
			if (cmdInput.inputVec[1].find(".") != std::string::npos) {
				sharedData.tempoBaseForScore[bar] = stoi(cmdInput.inputVec[1].substr(0, cmdInput.inputVec[1].find(".")));
				sharedData.BPMDisplayHasDot[bar] = true;
			}
			else {
				sharedData.tempoBaseForScore[bar] = stoi(cmdInput.inputVec[1]);
				sharedData.BPMDisplayHasDot[bar] = false;
			}
		}
	}
	else {
		if (isNumber(cmdInput.inputVec[1])) {
			int tempoVal = stoi(cmdInput.inputVec[1]);
			if (tempoVal <= 0) {
				barError = true;
				return genError("tempo can't be zero or negative");
			}
			sharedData.BPMTempi[bar] = tempoVal;
			// convert BPM to ms
			sharedData.tempoMs[bar] = 1000.0 / ((float)sharedData.BPMTempi[bar] / 60.0);
			// get the tempo of the minimum duration
			sharedData.tempo[bar] = sharedData.tempoMs[bar] / (MINDUR / sharedData.denominator[bar]);
			// get the usec duration of the PPQM
			sharedData.PPQNPerUs[bar] = (uint64_t)(sharedData.tempoMs[bar] / (float)sharedData.PPQN) * 1000;
			sharedData.BPMMultiplier[bar] = 1;
			sharedData.beatAtValues[bar] = {1};
			sharedData.beatAtDifferentThanDivisor[bar] = true;
			sharedData.tempoBaseForScore[bar] = 4;
			sharedData.BPMDisplayHasDot[bar] = false;
		}
		else {
			barError = true;
			return genError(cmdInput.inputVec[1] + " is not a number");
		}
	}
	// store the tempo information to the barLines string
	int barIndex = getLastLoopIndex();
	//for (int i = 0; i < editors[whichPane].getTabSize()-1; i++) {
	//	sharedData.barLines[sharedData.barsOrdered[barIndex]] += " ";
	//}
	sharedData.barLines[sharedData.barsOrdered[barIndex]] += '\t';
	for (size_t i = 0; i < cmdInput.inputVec.size(); i++) {
		sharedData.barLines[sharedData.barsOrdered[barIndex]] += cmdInput.inputVec[i];
		if (i < cmdInput.inputVec.size()-1) sharedData.barLines[sharedData.barsOrdered[barIndex]] += " ";
	}
	sharedData.barLines[sharedData.barsOrdered[barIndex]] += '\n';
	if (!restOfLine.empty()) {
		std::pair<int, std::string> p = parseString(genStrFromVec(restOfLine), lineNum, numLines);
		if (p.first == 3) return genError(p.second);
	}
	return cmdOutput;
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdBeatat(CmdInput& cmdInput, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	if (barError) return cmdOutput;
	if (!parsingBar && !parsingBars) {
		return genError("\\beatat command must be placed inside a bar(s) definition");
	}
	// the rest of the line after the values of the command is parsed after it (see splitBarCommandArgs())
	std::vector<std::string> restOfLine = splitBarCommandArgs(cmdInput);
	if (parsingBars) {
		CmdOutput barsOutput = selectBarOfBarsCommand(cmdInput);
		if (barsOutput.errorCode == 3) return barsOutput;
	}
	if (cmdInput.inputVec.size() == 1) {
		barError = true;
		return genError("\\beatat command takes at least one argument");
	}
	int bar = getLastBarIndex();
	// the values are collected first and stored only if they are all valid
	// so that an error doesn't leave the bar with some of the new values and some of the old ones
	// they replace any values set earlier (e.g. by \tempo), and they don't assume that there are such values
	std::vector<int> beatAtVals;
	for (size_t i = 1; i < cmdInput.inputVec.size(); i++) {
		std::pair<int, std::string> err = getBaseDurError(cmdInput.inputVec[i]);
		if (err.first > 0) {
			barError = true;
			return genError(err.second);
		}
		beatAtVals.push_back(getBaseDurValue(cmdInput.inputVec[i], sharedData.denominator[bar]));
	}
	sharedData.beatAtValues[bar] = beatAtVals;
	sharedData.beatAtDifferentThanDivisor[bar] = true;
	// store the beatat information to the barLines string
	int barIndex = getLastLoopIndex();
	//for (int i = 0; i < editors[whichPane].getTabSize()-1; i++) {
	//	sharedData.barLines[sharedData.barsOrdered[barIndex]] += " ";
	//}
	sharedData.barLines[sharedData.barsOrdered[barIndex]] += '\t';
	for (size_t i = 0; i < cmdInput.inputVec.size(); i++) {
		if (i > 0) sharedData.barLines[sharedData.barsOrdered[barIndex]] += " ";
		sharedData.barLines[sharedData.barsOrdered[barIndex]] += cmdInput.inputVec[i];
	}
	sharedData.barLines[sharedData.barsOrdered[barIndex]] += '\n';
	if (!restOfLine.empty()) {
		std::pair<int, std::string> p = parseString(genStrFromVec(restOfLine), lineNum, numLines);
		if (p.first == 3) return genError(p.second);
	}
	return cmdOutput;
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdTuplet(CmdInput& cmdInput, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	if (cmdInput.isMainCmd) {
		return genError("stray tuplet");
	}
	// a tuplet is part of a melodic line, so it can only be written inside a bar
	if (!parsingBar && !parsingBars) {
		return genError("a tuplet can only be written inside a bar");
	}
	if (cmdInput.inputVec.size() < 2 || !cmdInput.hasBrackets) {
		return genError("badly formatted tuplet");
	}
	std::pair<int, int> ratio = tupletRatio(cmdInput.inputVec[1]);
	if (ratio.first == 0) {
		return genError("badly formatted tuplet ratio, it must be two numbers greater than zero, e.g. 3/2");
	}
	// the tuplet stays in the melodic line as it is, with its notes inside curly brackets (e.g. \tuplet 3/2 { c8 d e })
	// and parseMelodicLine() determines which notes it includes, so tuplets can be nested to any depth
	// the notes inside the brackets have already been expanded (e.g. a variable or another tuplet)
	cmdOutput.outputVec.push_back(cmdInput.inputVec[0]);
	cmdOutput.outputVec.push_back(cmdInput.inputVec[1]);
	cmdOutput.outputVec.push_back("{");
	cmdOutput.outputVec.insert(cmdOutput.outputVec.end(), cmdInput.inputVec.begin()+2, cmdInput.inputVec.end());
	cmdOutput.outputVec.push_back("}");
	// the output replaces the tuplet and its arguments, with its curly brackets (see genOutput())
	return cmdOutput;
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdOttava(CmdInput& cmdInput, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	if (cmdInput.isMainCmd || !parsingBar) {
		return genError("stray ottava");
	}
	if (cmdInput.inputVec.size() < 2) {
		return genError("no argument to \\ottava");
	}
	if (cmdInput.afterCmdVec.empty()) {
		return genError("no item to apply \\ottava to");
	}
	std::pair<bool, int> ottava = ottavaValue(cmdInput.inputVec[1]);
	if (!ottava.first) {
		return genError("argument to \\ottava must be a number between -2 and 2");
	}
	// the ottava stays in the melodic line as it is, and applies to all the notes after it, until the next \ottava
	// parseMelodicLine() stores it with the notes
	cmdOutput.outputVec.push_back(cmdInput.inputVec[0]);
	cmdOutput.outputVec.push_back(cmdInput.inputVec[1]);
	// the output (the command and its argument) replaces the command and its argument (see genOutput())
	return cmdOutput;
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdCluster(CmdInput& cmdInput, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	if (cmdInput.isMainCmd) {
		return genError("stray cluster");
	}
	if (cmdInput.inputVec.size() < 2 || !cmdInput.hasBrackets) {
		return genError("badly formatted \\cluster");
	}
	if (!areBracketsBalanced(cmdInput.inputVec)) {
		return genError("unbalanced brackets");
	}
	for (size_t i = 1; i < cmdInput.inputVec.size(); i++) {
		std::string clusterStr = "";
		for (size_t j = 0; j < cmdInput.inputVec[i].size(); j++) {
			clusterStr += cmdInput.inputVec[i][j];
			if (cmdInput.inputVec[i][j] == '<' && j == 0) {
				clusterStr += "\\c";
			}
		}
		cmdOutput.outputVec.push_back(clusterStr);
	}
	// the output replaces the cluster and its arguments, with its curly brackets (see genOutput())
	return cmdOutput;
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdTranspose(CmdInput& cmdInput, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	if (cmdInput.inputVec.size() < 3) {
		return genError("\\transpose takes two arguments, melodic line to transpose and number of semitones");
	}
	// the number of semitones can be a float with .5 for quarter tones (e.g. 1.5 or -0.5)
	const std::string& semitonesStr = cmdInput.inputVec.back();
	std::string semitonesError = "last argument to \\transpose must be the number of semitones to transpose, an integer, or a float with .5 for quarter tones";
	if (!isFloat(semitonesStr)) return genError(semitonesError);
	double semitonesVal = std::stod(semitonesStr);
	if (std::fmod(std::abs(semitonesVal) * 2.0, 1.0) != 0) return genError(semitonesError);
	// the transposition in quarter tones
	int quarterTonesArg = (int)std::lround(semitonesVal * 2.0);
	std::vector<std::string> line;
	if (cmdInput.inputVec[1] == "\\bar") {
		CmdOutput cmdOutputLocal = stripLineFromBar({cmdInput.inputVec.begin()+1, cmdInput.inputVec.end()-1}, lineNum, numLines);
		// the output of stripLineFromBar() contains the instrument name in the beginning, so we strip that out
		line = {cmdOutputLocal.outputVec.begin()+1, cmdOutputLocal.outputVec.end()};
	}
	else {
		line = {cmdInput.inputVec.begin()+1, cmdInput.inputVec.end()};
	}
	CmdOutput cmdOutputLocal = evaluateTokens(line, lineNum, numLines);
	if (cmdOutputLocal.errorCode == 3) {
		return cmdOutputLocal;
	}
	line.clear();
	line = cmdOutputLocal.outputVec;
	// since the list might still hold the instrument name, we have to check this and remove it
	if (sharedData.instrumentIndexes.find(line[0]) != sharedData.instrumentIndexes.end()) {
		line = {line.begin()+1, line.end()};
	}
	// now we run through the line and we transpose all notes, also the notes of chords and clusters (e.g. <c e g>4 or \c<c' c''>4)
	// a note is its letter, its accidentals (is, es, ih, eh, which can be combined, e.g. isih), and its octave marks (' and ,)
	// its pitch is calculated in quarter tones, transposed, and spelled again, with sharps when transposing up and flats when transposing down
	// the octave marks are absolute, so transposing a note across a c changes its octave marks
	static const std::map<char, int> noteSemitones = {{'c', 0}, {'d', 2}, {'e', 4}, {'f', 5}, {'g', 7}, {'a', 9}, {'b', 11}};
	static const std::map<int, char> naturalNotes = {{0, 'c'}, {2, 'd'}, {4, 'e'}, {5, 'f'}, {7, 'g'}, {9, 'a'}, {11, 'b'}};
	bool up = quarterTonesArg > 0;
	// spells a semitone from 0 to 11 as a natural note, or with a sharp or a flat
	auto spellSemitone = [&](int semitone) {
		auto it = naturalNotes.find(semitone);
		if (it != naturalNotes.end()) return std::string(1, it->second);
		if (up) return std::string(1, naturalNotes.at(semitone - 1)) + "is";
		return std::string(1, naturalNotes.at(semitone + 1)) + "es";
	};
	// transposes the note that starts at index start of str, and returns the index after it
	auto transposeNote = [&](std::string& str, size_t start) {
		size_t ndx = start + 1;
		int quarterTones = noteSemitones.at(str[start]) * 2;
		while (ndx + 1 < str.size() && (str[ndx] == 'i' || str[ndx] == 'e') && (str[ndx+1] == 's' || str[ndx+1] == 'h')) {
			int step = (str[ndx+1] == 's' ? 2 : 1);
			quarterTones += (str[ndx] == 'i' ? step : -step);
			ndx += 2;
		}
		while (ndx < str.size() && (str[ndx] == '\'' || str[ndx] == ',')) {
			quarterTones += (str[ndx] == '\'' ? 24 : -24);
			ndx++;
		}
		quarterTones += quarterTonesArg;
		// floor division, so that notes below the octave without octave marks get commas
		int octave = (quarterTones >= 0 ? quarterTones / 24 : -((-quarterTones + 23) / 24));
		int pitchClass = quarterTones - (octave * 24);
		std::string newNote;
		if (pitchClass % 2 == 0) {
			newNote = spellSemitone(pitchClass / 2);
		}
		// a quarter tone is spelled from a natural note next to it, so it needs one accidental (e.g. aih and not beseh)
		// one of the two semitones next to a quarter tone is always a natural note
		// the one below it is preferred when transposing up (with ih), and the one above it when transposing down (with eh)
		else {
			int below = (pitchClass - 1) / 2;
			int above = (pitchClass + 1) / 2;
			bool belowIsNatural = naturalNotes.find(below) != naturalNotes.end();
			bool aboveIsNatural = (above == 12 || naturalNotes.find(above) != naturalNotes.end());
			if (belowIsNatural && (up || !aboveIsNatural)) {
				newNote = std::string(1, naturalNotes.at(below)) + "ih";
			}
			else {
				if (above == 12) {
					above = 0;
					octave++;
				}
				newNote = std::string(1, naturalNotes.at(above)) + "eh";
			}
		}
		for (int j = 0; j < std::abs(octave); j++) newNote += (octave > 0 ? '\'' : ',');
		str.replace(start, ndx - start, newNote);
		return start + newNote.size();
	};
	std::vector<std::string> output;
	for (size_t i = 0; i < line.size(); i++) {
		std::string s = line[i];
		if (quarterTonesArg != 0 && !s.empty() && s[0] != '\\' && !isFloat(s)) {
			// a note starts a token, or follows a space or a < inside a chord
			// letters inside quotes are text, not notes
			bool inQuotes = false;
			size_t j = 0;
			while (j < s.size()) {
				if (s[j] == '"') inQuotes = !inQuotes;
				bool noteStart = (j == 0 || s[j-1] == ' ' || s[j-1] == '<');
				if (!inQuotes && noteStart && noteSemitones.find(s[j]) != noteSemitones.end()) {
					j = transposeNote(s, j);
				}
				else {
					j++;
				}
			}
		}
		// a cluster starts with \c
		else if (quarterTonesArg != 0 && startsWith(s, "\\c<")) {
			size_t j = 3;
			bool inQuotes = false;
			while (j < s.size()) {
				if (s[j] == '"') inQuotes = !inQuotes;
				bool noteStart = (s[j-1] == ' ' || s[j-1] == '<');
				if (!inQuotes && noteStart && noteSemitones.find(s[j]) != noteSemitones.end()) {
					j = transposeNote(s, j);
				}
				else {
					j++;
				}
			}
		}
		output.push_back(s);
	}
	if (isFloat(output.back())) output.pop_back();
	cmdOutput.outputVec = output;
	// the output replaces \transpose and its arguments, with its curly brackets (see genOutput())
	return cmdOutput;
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdClef(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (!parsingBar) {
		return genError("stray \\clef command");
	}
	if (cmdInput.inputVec.size() != 2) {
		return genError("\\clef command takes one argument, a clef name");
	}
	int clefNdx;
	if (cmdInput.inputVec[1].compare("treble") == 0) {
		sharedData.instruments[lastInstrumentIndex].setClef(getLastBarIndex(), 0);
		clefNdx = 0;
	}
	else if (cmdInput.inputVec[1].compare("bass") == 0) {
		sharedData.instruments[lastInstrumentIndex].setClef(getLastBarIndex(), 1);
		clefNdx = 1;
	}
	else if (cmdInput.inputVec[1].compare("alto") == 0) {
		sharedData.instruments[lastInstrumentIndex].setClef(getLastBarIndex(), 2);
		clefNdx = 2;
	}
	else if (cmdInput.inputVec[1].compare("perc") == 0 || cmdInput.inputVec[1].compare("percussion") == 0) {
		sharedData.instruments[lastInstrumentIndex].setClef(getLastBarIndex(), 3);
		clefNdx = 3;
	}
	else {
		return genError("unknown clef");
	}
	if (sharedData.instruments[lastInstrumentIndex].sendToPart) {
		ofxOscMessage m;
		m.setAddress("/clef");
		m.addStringArg("\\"+sharedData.instruments[lastInstrumentIndex].getName());
		m.addIntArg(clefNdx);
		sharedData.instruments[lastInstrumentIndex].scorePartSender.sendMessage(m, false);
		m.clear();
	}
	return CmdOutput();
}

//--------------------------------------------------------------
// \euclide {taps onsets [duration [offset]] [notes...]} spreads the onsets over the taps of the bar as evenly as possible (a Euclidean rhythm)
// e.g. \euclide {16 5 4 c' cis' dis'} spreads five onsets over sixteen taps (16ths in 4/4), each a quarter note if it fits before the next onset
// otherwise the longest duration that fits, dotted or not, with rests filling the rest of the space until the next onset
// the duration can be dotted (e.g. 4.), and without it an onset lasts one tap
// the offset is a number of onset durations of rest at the start of the bar, after which the onsets are spread
// e.g. \euclide {16 5 4 1 c' cis' <c' ees'>} starts with a quarter rest, and spreads the five onsets over the rest of the bar
// if the onsets don't fit after the offset, the offset is made smaller, with a warning
// the notes are applied to the onsets in turn (c', cis', <c' ees'>, c', cis'), and without notes the onsets are durations only, for rhythm staffs
// dynamics among the notes (e.g. \euclide {16 5 4 c' cis' \f \p}) are applied to the onsets in turn too, written only when they change
CmdOutput ofApp::cmdEuclide(CmdInput& cmdInput, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	if (!parsingBar) {
		return genError("stray \\euclide command");
	}
	if (cmdInput.inputVec.size() < 3) {
		return genError("\\euclide takes at least two arguments, the taps and the onsets");
	}
	if (!isNumber(cmdInput.inputVec[1]) || !isNumber(cmdInput.inputVec[2])) {
		return genError("\"taps\" and \"onsets\" must be integers");
	}
	int taps = std::stoi(cmdInput.inputVec[1]);
	int onsets = std::stoi(cmdInput.inputVec[2]);
	if (taps <= 0) {
		return genError("\"taps\" must be greater than 0");
	}
	if (onsets <= 0) {
		return genError("\"onsets\" must be greater than 0");
	}
	if (onsets > taps) {
		return genError("\"onsets\" can't be greater than \"taps\"");
	}
	const std::vector<int> dursArr{1, 2, 4, 8, 16, 32, 64};
	// a duration is a number, maybe with dots (e.g. 4.)
	auto isDuration = [](const std::string& str) {
		size_t digitsEnd = str.find_first_not_of("0123456789");
		if (digitsEnd == 0) return false;
		return digitsEnd == std::string::npos || str.find_first_not_of(".", digitsEnd) == std::string::npos;
	};
	// the optional duration of the onsets and the offset, and the notes after them
	size_t notesStart = 3;
	int onsetLength = 0; // in 64ths
	int offset = 0;
	std::string durStr;
	if (cmdInput.inputVec.size() > 3 && isDuration(cmdInput.inputVec[3])) {
		durStr = cmdInput.inputVec[3];
		int dur = std::stoi(durStr);
		if (std::find(dursArr.begin(), dursArr.end(), dur) == dursArr.end()) {
			return genError(durStr + ": invalid duration");
		}
		// every dot adds half of the previous value
		int numDots = (int)(durStr.size() - std::to_string(dur).size());
		int value = 64 / dur;
		onsetLength = value;
		for (int i = 0; i < numDots; i++) {
			if (value % 2 != 0) return genError(durStr + ": too many dots for this duration");
			value /= 2;
			onsetLength += value;
		}
		notesStart = 4;
		if (cmdInput.inputVec.size() > 4 && isNumber(cmdInput.inputVec[4])) {
			offset = std::stoi(cmdInput.inputVec[4]);
			if (offset < 0) return genError("the offset can't be negative");
			notesStart = 5;
		}
	}
	// the notes, and the dynamics (e.g. \f), which are applied to the onsets in turn, separately from the notes
	std::vector<std::string> notes;
	std::vector<std::string> dynamics;
	for (size_t i = notesStart; i < cmdInput.inputVec.size(); i++) {
		if (isDuration(cmdInput.inputVec[i])) {
			return genError("\\euclide takes up to four numbers (taps, onsets, duration, and offset), followed by notes and dynamics");
		}
		if (isDynamicToken(cmdInput.inputVec[i])) {
			dynamics.push_back(cmdInput.inputVec[i]);
		}
		else if (startsWith(cmdInput.inputVec[i], "\\")) {
			return genError(cmdInput.inputVec[i] + ": \\euclide takes notes and dynamics after its numbers");
		}
		else {
			notes.push_back(cmdInput.inputVec[i]);
		}
	}
	int bar = getLastBarIndex();
	// the length of the bar and of a tap in 64ths, the shortest duration
	int barLength = (64 / sharedData.denominator[bar]) * sharedData.numerator[bar];
	if (taps > barLength) {
		return genError("\"taps\" are more than the 64ths of the bar");
	}
	if (barLength % taps != 0) {
		return genError("the bar can't be divided into " + std::to_string(taps) + " equal taps");
	}
	int tapLength = barLength / taps;
	if (onsetLength == 0) onsetLength = tapLength;
	// the durations with their lengths in 64ths, plain and dotted, from the longest to the shortest
	std::vector<std::pair<int, std::string>> durations;
	for (int d : dursArr) {
		durations.push_back(std::make_pair(64 / d, std::to_string(d)));
		if ((64 / d) % 2 == 0) durations.push_back(std::make_pair((64 / d) * 3 / 2, std::to_string(d) + "."));
	}
	std::sort(durations.begin(), durations.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
	// the durations that make up a length, longest first, for rests
	auto durationsOf = [&](int length) {
		std::vector<std::string> result;
		for (const auto& d : durations) {
			while (length >= d.first) {
				result.push_back(d.second);
				length -= d.first;
			}
		}
		return result;
	};
	// the offset takes as many whole taps as its length needs, and the onsets must fit in the taps after it
	auto offsetTaps = [&](int numOnsets) { return (numOnsets * onsetLength + tapLength - 1) / tapLength; };
	int requestedOffset = offset;
	while (offset > 0 && taps - offsetTaps(offset) < onsets) offset--;
	int restTaps = offsetTaps(offset);
	int remainingTaps = taps - restTaps;
	std::vector<std::string> output;
	// the rests of the offset
	for (const std::string& restDur : durationsOf(restTaps * tapLength)) output.push_back("r" + restDur);
	// Euclidean distribution over the taps after the offset: the taps where an onset starts
	std::vector<int> onsetTaps;
	for (int i = 0; i < remainingTaps; ++i) {
		if (((onsets * i) % remainingTaps) < onsets) onsetTaps.push_back(restTaps + i);
	}
	std::string lastDynamic;
	for (size_t k = 0; k < onsetTaps.size(); k++) {
		// the space until the next onset, or until the end of the bar
		int nextTap = (k + 1 < onsetTaps.size() ? onsetTaps[k+1] : taps);
		int space = (nextTap - onsetTaps[k]) * tapLength;
		// the duration of the onset, or the longest duration that fits in its space, dotted or not
		std::pair<int, std::string> noteDur = durations.back();
		for (const auto& d : durations) {
			if (d.first <= std::min(onsetLength, space)) {
				noteDur = d;
				break;
			}
		}
		std::string note = (notes.empty() ? "" : notes[k % notes.size()]);
		// a dynamic is written only when it changes, as dynamics hold until the next one, also over rests
		std::string dynamic;
		if (!dynamics.empty()) {
			const std::string& thisDynamic = dynamics[k % dynamics.size()];
			if (thisDynamic != lastDynamic) dynamic = thisDynamic;
			lastDynamic = thisDynamic;
		}
		output.push_back(note + noteDur.second + dynamic);
		// rests fill the rest of the space
		for (const std::string& restDur : durationsOf(space - noteDur.first)) output.push_back("r" + restDur);
	}
	cmdOutput.outputVec = output;
	if (offset != requestedOffset) {
		cmdOutput.errorCode = 2;
		cmdOutput.errorStr = "\\euclide: " + std::to_string(onsets) + " onsets don't fit after an offset of " + std::to_string(requestedOffset) +
			", the offset is " + std::to_string(offset);
	}
	// the output replaces \euclide and its arguments, with its curly brackets (see genOutput())
	return cmdOutput;
}

//--------------------------------------------------------------
// \apply {\bar[.\inst] notes...} applies notes to the onsets of the line of a bar, spread over them as evenly as possible in time
// e.g. \apply {\1.\inst2 c' <c' ees'>}, and \apply {\bar[.\inst] durations...} applies the notes of the line, in turn, to durations
// e.g. \apply {\1.\inst2 8 8 4 2. 8} (see applyToLine())
CmdOutput ofApp::cmdApply(CmdInput& cmdInput, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	if (!parsingBar) {
		return genError("\\apply must be used in a melodic line inside a bar definition");
	}
	if (cmdInput.inputVec.size() < 3) {
		return genError("\\apply takes a bar (e.g. \\1 or \\1.\\inst) followed by notes or durations, in curly brackets");
	}
	// \apply has raw arguments (see CommandSpec): it reads the name of its bar as it is written, and evaluates the rest of its arguments
	std::vector<std::string> args = {cmdInput.inputVec.begin()+1, cmdInput.inputVec.end()};
	size_t firstValue = (sharedData.loopsIndexes.count(args[0].substr(0, args[0].find('.'))) ? 1 : 0);
	CmdOutput evaluated = evalArgs({args.begin()+firstValue, args.end()}, lineNum, numLines);
	if (evaluated.errorCode == 3) return evaluated;
	args.resize(firstValue);
	args.insert(args.end(), evaluated.outputVec.begin(), evaluated.outputVec.end());
	if (args.size() < 2) {
		return genError("\\apply takes a bar (e.g. \\1 or \\1.\\inst) followed by notes or durations, in curly brackets");
	}
	std::pair<std::string, std::vector<std::string>> applied = applyToLine(args);
	if (!applied.first.empty()) return genError("\\apply: " + applied.first);
	cmdOutput.outputVec = applied.second;
	// the curly brackets are not included in the arguments, so we pop the size of the input vector plus one (see \euclide)
	return cmdOutput;
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdPpqn(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() < 2) {
		return genError("\\ppqn command needs the PPQN number as an argument");
	}
	if (cmdInput.inputVec.size() > 2) {
		return genError("\\ppqn command takes one argument only");
	}
	if (!isNumber(cmdInput.inputVec[1])) {
		return genError("the argument to \\ppqn must be a number");
	}
	int PPQNLocal = stoi(cmdInput.inputVec[1]);
	if (PPQNLocal != 24 && PPQNLocal != 48 && PPQNLocal != 96) {
		return genError("PPQN value can be 24, 48, or 96");
	}
	sharedData.PPQN = PPQNLocal;
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdMidiclock(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() < 2) {
		return genError("\\midiclock takes one argument");
	}
	if (cmdInput.inputVec[1].compare("on") == 0) {
		// the MIDI clock goes to the ports opened with \openmidiport, not to the ports of instruments
		if (!sequencer.hasGenericMidiPorts()) {
			return genError("no MIDI port is open for the MIDI clock, open one with \\openmidiport first");
		}
		sequencer.setSendMidiClock(true);
		sharedData.PPQNTimeStamp = Clock::micros();
	}
	else if (cmdInput.inputVec[1].compare("off") == 0) {
		sequencer.setSendMidiClock(false);
	}
	else {
		return genError(cmdInput.inputVec[1] + ": unknown argument, \\midiclock takes on or off");
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdInstorder(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() != sharedData.instruments.size()+1) {
		return genError("instrument list must be equal to number of instruments");
	}
	for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
		sharedData.instrumentIndexesOrdered[i-1] = sharedData.instrumentIndexes[cmdInput.inputVec[i]];
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdPrint(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (!cmdInput.isMainCmd) {
		return genError("\\print can only be a top command");
	}
	std::vector<std::string> v = {cmdInput.inputVec.begin()+1, cmdInput.inputVec.end()};
	return genNote(genStrFromVec(v));
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdOsc(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() < 2) {
		std::string cmd;
		if (cmdInput.inputVec[0].compare("\\osc") == 0) cmd = "\\osc";
		else cmd = "\\oscclient";
		return genError(cmd + " command takes one argument, the name of the OSC client");
	}
	std::string oscClientName = "\\" + cmdInput.inputVec[1];
	// the clients are stored with the backslash of their name
	if (oscClients.find(oscClientName) != oscClients.end()) {
		return genError("OSC client already exists");
	}
	if (isReservedName(oscClientName)) {
		return genError(cmdInput.inputVec[1] + " can't be the name of an OSC client, " + oscClientName + " is a command");
	}
	std::string taken = nameTakenError(oscClientName, NameKind::oscClient);
	if (!taken.empty()) return genError(taken);
	oscClients[oscClientName] = ofxOscSender();
	commandsMap[livelily][oscClientName.substr(1)[0]][oscClientName] = Colors::lightSeaGreen;
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdOscserver(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() < 2) {
		return genError("\\oscserver command takes one argument, the name of the OSC server");
	}
	std::string oscServerName = "\\" + cmdInput.inputVec[1];
	if (isReservedName(oscServerName)) {
		return genError(cmdInput.inputVec[1] + " can't be the name of an OSC server, " + oscServerName + " is a command");
	}
	std::string taken = nameTakenError(oscServerName, NameKind::oscServer);
	if (!taken.empty()) return genError(taken);
	// the servers are stored with the backslash of their name
	if (oscServers.find(oscServerName) != oscServers.end()) {
		return genError("OSC server already exists");
	}
	oscServers[oscServerName] = ofxOscReceiver();
	commandsMap[livelily][oscServerName.substr(1)[0]][oscServerName] = Colors::lightSeaGreen;
	return CmdOutput();
}

//--------------------------------------------------------------
// \return ends the function whose body it is in, and gives the value of the function, its arguments (evaluated), or nothing
// e.g. \function double { \return $1 $1 } makes \print \double 7 print 7 7
CmdOutput ofApp::cmdReturn(CmdInput& cmdInput, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	if (functionReturns.empty()) {
		return genError("\\return can only be used in the body of a function");
	}
	std::vector<std::string> value = {cmdInput.inputVec.begin()+1, cmdInput.inputVec.end()};
	// the arguments can be in curly brackets, which are not part of the value
	if (value.size() > 1 && value.front() == "{" && value.back() == "}") value = {value.begin()+1, value.end()-1};
	functionReturns.back().returned = true;
	functionReturns.back().value = value;
	return cmdOutput;
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdNuminsts(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() > 1) return genError("\\numinsts takes no arguments");
	return genOutput(std::to_string(sharedData.instruments.size()));
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdBarnum(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() > 1) {
		return genError("\\barnum takes no arguments");
	}
	return genOutput(std::to_string(getPlayingBarIndex()));
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdBeatcount(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() > 1) {
		return genError("\\beatcount takes no arguments");
	}
	// livelily is 1-based, but C++ is 0-based, so we add one to the beat counter
	// this is not needed with the \barnum command, because livelily creates the first bar
	// with index 0 upon creating the instruments, so the first bar created by the user
	// will have index 1
	return genOutput(std::to_string(sharedData.beatCounter + 1));
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdInsts(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec[0].compare("\\insts") == 0 && cmdInput.inputVec.size() < 2) {
		return genError("the \\insts command does nothing by itself, you must call one of its methods");
	}
	//else if (cmdInput.inputVec[0].compare("\\insts") == 0 && cmdInput.inputVec.size() >= 2) {
	//	return genError("the \\insts command takes only methods, not arguments");
	//}
	bool hasDot = true;
	std::vector<std::string> commands = tokenizeString(cmdInput.inputVec[0], ".");
	if (commands.size() < 2) {
		hasDot = false;
		//return genError("the \\insts command does nothing by itself, you must call one of its methods");
	}
	commands.insert(commands.end(), cmdInput.inputVec.begin()+1, cmdInput.inputVec.end());
	if (commands[1] == "init" && sharedData.numInstruments > 0) {
		return genError("instruments have already been initialized, can only add or clear now");
	}
	// \insts.add adds to instruments that exist, so it needs \insts.init first, also after \insts.clear
	// (\insts.init also creates the default empty bar the bars and loops start from)
	if (commands.size() > 1 && commands[1] == "add" && sharedData.numInstruments == 0) {
		return genError("there are no instruments to add to, create them with \\insts.init");
	}
	//if (commands[1].compare("init") != 0 && commands[1].compare("add") != 0 && commands[1].compare("clear") != 0) {
	//	return genError("\\insts command takes only \"init\", \"add\" or \"clear\" methods");
	//}
	if (commands[1].compare("clear") == 0) {
		if (!hasDot) {
			return genError("\"clear\" method must be concatenated to \"\\inst\" with a dot");
		}
		sequencer.stopNow();
		std::lock_guard<std::recursive_mutex> instrumentsLock(sharedData.instrumentsMutex);
		for (map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			commandsMap[livelily][it->second.getName()[0]].erase("\\" + it->second.getName());
		}
		clearBars();
		clearLoops();
		clearInstruments();
	}
	// \insts.remove name ... removes instruments, from the sequencer and the score
	else if (commands[1].compare("remove") == 0) {
		if (!hasDot) {
			return genError("\"remove\" method must be concatenated to \"\\insts\" with a dot");
		}
		if (commands.size() < 3) {
			return genError("\\insts.remove takes the names of the instruments to remove");
		}
		if (commands.size() - 2 >= sharedData.instruments.size()) {
			return genError("\\insts.remove can't remove all the instruments, \\insts.clear removes all of them");
		}
		// removing an instrument while the sequencer goes through the instruments (e.g. from a bound function) is not safe
		if (sequencer.isCurrentThread()) {
			return genError("instruments can't be removed from a function that the sequencer calls");
		}
		std::vector<int> toRemove;
		for (unsigned i = 2; i < commands.size(); i++) {
			std::string name = (startsWith(commands[i], "\\") ? commands[i] : "\\" + commands[i]);
			auto instIt = sharedData.instrumentIndexes.find(name);
			if (instIt == sharedData.instrumentIndexes.end()) {
				return genError(commands[i] + ": unknown instrument");
			}
			if (std::find(toRemove.begin(), toRemove.end(), instIt->second) == toRemove.end()) toRemove.push_back(instIt->second);
		}
		// from the last one, so that the indexes of the ones left to remove don't change
		std::sort(toRemove.rbegin(), toRemove.rend());
		std::lock_guard<std::recursive_mutex> instrumentsLock(sharedData.instrumentsMutex);
		for (int instNdx : toRemove) removeInstrument(instNdx);
		updateMergedVoices();
	}
	else if (commands[1].compare("init") == 0 || commands[1].compare("add") == 0) {
		// instruments are not added while the sequencer goes through them (see removeInstrument())
		std::lock_guard<std::recursive_mutex> instrumentsLock(sharedData.instrumentsMutex);
		int numInstsBefore = (int)sharedData.instruments.size();
		if (!hasDot) {
			if (commands[1].compare("init") == 0) {
				return genError("\"init\" method must be concatenated to \"\\inst\" with a dot");
			}
			else {
				return genError("\"add\" method must be concatenated to \"\\inst\" with a dot");
			}
		}
		// check the arguments to make sure they don't start with a backslash, and that they are not commands
		for (auto it = commands.begin()+2; it != commands.end(); ++it) {
			if (startsWith(*it, "\\")) {
				return genError("instrument names can't start with a backslash");
			}
			if (isReservedName("\\" + *it)) {
				return genError(*it + " can't be an instrument name, \\" + *it + " is a command");
			}
			std::string taken = nameTakenError("\\" + *it, NameKind::instrument);
			if (!taken.empty()) return genError(taken);
		}
		// create the instruments of the arguments
		for (unsigned i = 2; i < commands.size(); i++) {
			bool instrumentExists = false;
			for (auto it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				// the instruments are stored with the backslash of their name
				if (("\\" + commands[i]).compare(it->first) == 0) {
					instrumentExists = true;
					lastInstrumentIndex = it->second;
					break;
				}
			}
			if (!instrumentExists) {
				initializeInstrument("\\" + commands[i]);
				commandsMap[livelily][commands[i][0]]["\\" + commands[i]] = Colors::greenYellow;
			}
			lastInstrument = commands[i];
		}
		if (commands[1].compare("init") == 0) {
			// create a default empty bar with a -1 bar number
			// by recursively calling parseCommand() like below
			CmdInput cmdInput = {{"\\bar", "-1"}, std::vector<std::string>(), false, true};
			parseCommand(cmdInput, 0, 1); // dummy line number and number of lines arguments
		}
		else if (commands[1].compare("add") == 0) {
			// the new instruments get empty bars (a whole rest) for all the bars defined before them (see addedInstruments())
			addedInstruments(numInstsBefore);
		}
		// space the staves of the new instruments right away, and not only when the first bar is written
		calculateStaffPositions(getLastBarIndex(), false);
	}
	else {
		// we can call \insts with any instrument command and it will result in calling
		// this command for all defined instruments
		// we can do this either with a dot or without one
		if (sharedData.numInstruments == 0) {
			return genError("instruments have not been initialized yet");
		}
		for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			int firstCommandNdx = 1;
			std::string s = "\\" + it->second.getName();
			if (hasDot) {
				s += ("." + commands[1]);
				firstCommandNdx = 2;
			}
			std::vector<std::string> v = {s};
			v.insert(v.end(), commands.begin()+firstCommandNdx, commands.end());
			std::pair<bool, CmdOutput> p = isInstrument(v, lineNum, numLines);
			if (p.second.errorCode == 3) {
				return p.second;
			}
		}
	}
	return CmdOutput();
}

//--------------------------------------------------------------
// \merge \inst1 \inst2 ... draws the instruments on the staff of the first one, as voices (see applyMergeLayout())
// the first has its stems up, the second down, the third up again, etc., as LilyPond does with voices
// \unmerge \inst ... draws the instruments on their own staves again, and \unmerge alone all of them
CmdOutput ofApp::cmdMerge(CmdInput& cmdInput, int lineNum, int numLines)
{
	bool merging = (cmdInput.inputVec[0].compare("\\merge") == 0);
	if (merging && cmdInput.inputVec.size() < 3) {
		return genError("\\merge takes at least two instruments");
	}
	std::vector<int> insts;
	for (size_t i = 1; i < cmdInput.inputVec.size(); i++) {
		auto instIt = sharedData.instrumentIndexes.find(cmdInput.inputVec[i]);
		if (instIt == sharedData.instrumentIndexes.end()) {
			return genError(cmdInput.inputVec[i] + ": unknown instrument");
		}
		if (std::find(insts.begin(), insts.end(), instIt->second) != insts.end()) {
			return genError(cmdInput.inputVec[i] + " is given more than once");
		}
		insts.push_back(instIt->second);
	}
	if (!merging && insts.empty()) {
		mergeGroups.clear();
	}
	else {
		// the instruments leave the staves they have been merged to before
		for (auto& group : mergeGroups) {
			group.erase(std::remove_if(group.begin(), group.end(), [&](int inst) {
				return std::find(insts.begin(), insts.end(), inst) != insts.end();
			}), group.end());
		}
		mergeGroups.erase(std::remove_if(mergeGroups.begin(), mergeGroups.end(), [](const std::vector<int>& group) {
			return group.size() < 2;
		}), mergeGroups.end());
		if (merging) mergeGroups.push_back(insts);
	}
	updateMergedVoices();
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdGroup(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() < 3) {
		return genError("\\group command takes at least two arguments");
	}
	if (cmdInput.inputVec.size() > sharedData.instruments.size() + 2) {
		return genError("too many arguments to \\group");
	}
	std::string groupName;
	for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
		if (sharedData.instrumentIndexes.find(cmdInput.inputVec[i]) == sharedData.instrumentIndexes.end()) {
			// if we provide two arguments (3 with the command name) then we can't provide a name for the group
			// otherwise, if we're not at the last argument, again we raise an error as only the last argument
			// can be the name of the group, the previous arguments must be instrument names
			if (cmdInput.inputVec.size() == 3 || i < cmdInput.inputVec.size() - 1) {
				return genError(cmdInput.inputVec[i] + ": unknown instrument");
			}
			else if (i == cmdInput.inputVec.size()-1) {
				groupName = "\\" + cmdInput.inputVec[i];
				if (isReservedName(groupName)) {
					return genError(cmdInput.inputVec[i] + " can't be a group name, " + groupName + " is a command");
				}
				std::string taken = nameTakenError(groupName, NameKind::group);
				if (!taken.empty()) return genError(taken);
			}
		}
	}
	for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
		if (i > 1 && sharedData.instrumentIndexes.find(cmdInput.inputVec[i]) != sharedData.instrumentIndexes.end()) {
			if (sharedData.instruments[sharedData.instrumentIndexes[cmdInput.inputVec[i]]].getID() - sharedData.instruments[sharedData.instrumentIndexes[cmdInput.inputVec[i-1]]].getID() != 1) {
				return genError("can't group instruments that are not adjacent");
			}
		}
	}
	unsigned loopIter = (groupName.empty() ? cmdInput.inputVec.size() : cmdInput.inputVec.size()-1);
	for (unsigned i = 2; i < loopIter; i++) {
		sharedData.instruments[sharedData.instrumentIndexes[cmdInput.inputVec[i]]].setGroup(sharedData.instruments[sharedData.instrumentIndexes[cmdInput.inputVec[i-1]]].getID());
	}
	ofxOscMessage m;
	m.setAddress("/group");
	for (unsigned i = 1; i < loopIter; i++) {
		m.addStringArg(cmdInput.inputVec[i]);
	}
	sendToParts(m, false);
	if (!groupName.empty()) {
		instGroups[groupName] = {cmdInput.inputVec.begin()+1, cmdInput.inputVec.end()-1};
		commandsMap[livelily][groupName.substr(1)[0]][groupName] = Colors::lightSteelBlue;
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdScore(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec[0].compare("\\score") == 0 && cmdInput.inputVec.size() < 2) {
		return genError("the \\score command does nothing by itself, you must call one of its methods");
	}
	else if (cmdInput.inputVec[0].compare("\\score") == 0 && cmdInput.inputVec.size() >= 2) {
		return genError("the \\score command takes only methods, not arguments");
	}
	return scoreCommands(cmdInput.inputVec, lineNum, numLines);
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdEditor(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec[0].compare("\\editor") == 0 && cmdInput.inputVec.size() < 2) {
		return genError("the \\editor command does nothing by itself, you must call one of its methods");
	}
	else if (cmdInput.inputVec[0].compare("\\editor") == 0 && cmdInput.inputVec.size() >= 2) {
		return genError("the \\editor command takes only methods, not arguments");
	}
	// the initial command is \editor. followed by a method
	// to separate the method we create a new vector that will copy the originalCommands vector
	// except from the first item where we trim the \editor. part
	std::vector<std::string> editorCommands;
	for (unsigned i = 0; i < cmdInput.inputVec.size(); i++) {
		if (!i) {
			// remove the "\editor." part of the command so we isolate the method
			std::string thisCommand = cmdInput.inputVec[i].substr(cmdInput.inputVec[i].find(".")+1);
			editorCommands.push_back(thisCommand);
		}
		else {
			editorCommands.push_back(cmdInput.inputVec[i]);
		}
	}
	// aligning the melodic lines of bar definitions (see alignment.cpp): on or off
	// and the width of a unit of the grid: the longest note token of a bar (long, the default) or the shortest (short)
	// which applies at once if alignment is on, and the next time it is turned on otherwise
	if (editorCommands[0].compare("alignment") == 0) {
		const std::vector<std::string> arguments = {"on", "off", "short", "long"};
		if (editorCommands.size() != 2 || std::find(arguments.begin(), arguments.end(), editorCommands[1]) == arguments.end()) {
			return genError("\\editor.alignment takes one argument, \"on\", \"off\", \"short\", or \"long\"");
		}
		Editor& editor = editors[whichPane];
		if (editorCommands[1] == "short" || editorCommands[1] == "long") {
			editor.alignShort = (editorCommands[1] == "short");
			if (editor.alignment) alignBars(whichPane);
		}
		else {
			editor.alignment = (editorCommands[1] == "on");
			if (editor.alignment) alignBars(whichPane);
			else unalignBars(whichPane);
		}
	}
	// highlighting the tokens of the notes that play is set for the pane the command is executed in
	//closing brackets and quotes automatically
	// and the pop-up of autocompletion (with the hint of arguments in Python), for the pane: on or off
	else if (editorCommands[0].compare("animation") == 0 || editorCommands[0].compare("autobrackets") == 0 || editorCommands[0].compare("autocomplete") == 0) {
		if (editorCommands.size() != 2 || (editorCommands[1] != "on" && editorCommands[1] != "off")) {
			return genError("\\editor." + editorCommands[0] + " takes one argument, \"on\" or \"off\"");
		}
		bool on = (editorCommands[1] == "on");
		if (editorCommands[0] == "animation") editors[whichPane].setAnimation(on);
		else if (editorCommands[0] == "autobrackets") editors[whichPane].setAutocomplete(on);
		else editors[whichPane].setAutocompletion(on);
	}
	else {
		return genError(editorCommands[0] + ": unknown editor command");
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdCursor(CmdInput& cmdInput, int lineNum, int numLines)
{
	if ((cmdInput.inputVec.size() == 1) || (cmdInput.inputVec.size() > 2)) {
		return genError("\\cursor takes one argument");
	}
	if (cmdInput.inputVec[1].compare("hide") == 0) {
		ofHideCursor();
	}
	else if (cmdInput.inputVec[1].compare("show") == 0) {
		ofShowCursor();
	}
	else {
		return genError("unknown argument for \\cursor");
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdBar(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() < 2) {
		barError = true;
		return genError("\\bar command takes a name as an argument");
	}
	if (cmdInput.inputVec.size() >= 2 && cmdInput.inputVec[1].compare("{") == 0) {
		barError = true;
		return genError("\\bar command takes a name as an argument");
	}
	std::pair<int, std::string> p = checkNameAvailability("bar", cmdInput.inputVec[1]);
	if (p.first == 3) {
		// the lines of the bar are not parsed, as with the other errors of \bar
		barError = true;
		return genError(p.second);
	}
	std::string barName = p.second;
	auto barExists = sharedData.barsIndexes.find(barName);
	if (barExists != sharedData.barsIndexes.end()) {
		barError = true;
		return genError((std::string)"bar " + barName.substr(1) + (std::string)" already exists");
	}
	// a bar has a melodic line for every instrument, so the instruments must exist before any bar
	if (sharedData.instruments.empty()) {
		barError = true;
		return genError("instruments have not been created yet, create them with \\insts.init before defining bars");
	}
	parsingBar = true;
	numInstsParsed = 0;
	int barIndex = storeNewBar(barName);
	// first set all instruments to not passed and not copied
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		it->second.setPassed(false);
		it->second.setCopied(barIndex, false);
		// if this is not the first bar, set a default clef similar to the last one
		if (barIndex > 0) it->second.setClef(barIndex, it->second.getClef(getPrevBarIndex()));
	}
	commandsMap[livelily][barName.substr(1)[0]][barName] = Colors::orchid;
	// since bars include simple melodic lines in their definition
	// we need to know this when we parse these lines
	if (cmdInput.inputVec.size() > 2) {
		std::vector<std::string> v = {cmdInput.inputVec.begin()+2, cmdInput.inputVec.end()};
		// a bar defined in one line, e.g. \bar 1 { \inst c'1 }, ends with its own closing bracket
		// which is not part of the melodic line, so we remove it
		// (the bar is closed at the end of parseStrings(), as with bars defined in many lines)
		if (v.size() > 1 && v.front() == "{" && v.back() == "}") {
			int openBrackets = 0;
			for (const std::string& s : v) {
				if (s == "{") openBrackets++;
				else if (s == "}") openBrackets--;
			}
			// only if the bar's brackets are balanced, otherwise the closing bracket is not the bar's
			if (openBrackets == 0) v.pop_back();
		}
		std::pair<int, std::string> p = parseString(genStrFromVec(v), lineNum, numLines);
		return genOutputFuncs(p);
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdLoop(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() < 2) {
		return genError("\\loop command takes a name as an argument");
	}
	if (cmdInput.inputVec.size() > 1 && startsWith(cmdInput.inputVec[1], "{")) {
		return genError("\\loop command must take a name as an argument");
	}
	std::pair<int, std::string> p = checkNameAvailability("loop", cmdInput.inputVec[1]);
	if (p.first == 3) {
		return genError(p.second);
	}
	std::string loopName = p.second;
	auto loopExists = sharedData.loopsIndexes.find(loopName);
	if (loopExists != sharedData.loopsIndexes.end()) {
		return genError((std::string)"loop " + loopName + (std::string)" already exists");
	}
	parsingLoop = true;
	storeNewLoop(loopName);
	commandsMap[livelily][loopName.substr(1)[0]][loopName] = Colors::lightSeaGreen;
	// loops are most likely defined in one-liners, so we parse the rest of the line here
	if (cmdInput.inputVec.size() > 2) {
		std::string restOfCommand = genStrFromVec({cmdInput.inputVec.begin()+2, cmdInput.inputVec.end()});
		std::pair<int, std::string> error = parseString(restOfCommand, lineNum, numLines);
		if (error.first > 0) {
			parsingLoop = false;
			return genOutputFuncs(error);
		}
		lastLoopCommand = genStrFromVec(cmdInput.inputVec);
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdBars(CmdInput& cmdInput, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	if (cmdInput.inputVec.size() < 2) {
		barError = true;
		return genError("\\bars command takes a name for a set of bars as an argument");
	}
	// the bars have a melodic line for every instrument, so the instruments must exist before any bars
	if (sharedData.instruments.empty()) {
		barError = true;
		return genError("instruments have not been created yet, create them with \\insts.init before defining bars");
	}
	if (barsIterCounter == 0) {
		for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			it->second.setMultiBarsDone(false);
			it->second.setMultiBarsStrBegin(0);
		}
		parsingBars = true;
	}
	std::pair<int, std::string> p = checkNameAvailability("loop", cmdInput.inputVec[1]);
	if (p.first == 3) {
		// the lines of the bars are not parsed, as with the other errors of \bars
		barError = true;
		return genError(p.second);
	}
	// in case of \bars we need to discard the backslash in the loop name
	// because we use this name to store the bars of the loop and we should not
	// have the backslash in the beginning when we call the \bar command
	multiBarsName = p.second.substr(1);
	auto barsExist = sharedData.loopsIndexes.find("\\" + multiBarsName);
	if (barsExist != sharedData.loopsIndexes.end()) {
		return genError((std::string)"bars " + multiBarsName + (std::string)" already exist");
	}
	barsIterCounter++;
	numBarsToParse = 0;
	numBarsToParseSet = false;
	barsCmdNumBars = 0;
	// store each bar separately with the "-x" suffix, where x is the counter of the iterations
	// until all instruments have had all their separate bars parsed
	cmdOutput.outputVec.insert(cmdOutput.outputVec.end(), {"\\bar", multiBarsName+"-"+std::to_string(barsIterCounter)});
	CmdInput cmdInputLocal = {cmdOutput.outputVec, std::vector<std::string>(), false, true};
	// an error in creating the bar is an error of the \bars command
	CmdOutput barOutput = parseCommand(cmdInputLocal, lineNum, numLines);
	if (barOutput.errorCode == 3) return barOutput;
	return cmdOutput;
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdRandom(CmdInput& cmdInput, int lineNum, int numLines)
{
	int start = 0, end = 0, diff = 0, randVal;
	bool symbols = false;
	if (cmdInput.inputVec.size() < 2) {
		return genError(cmdInput.inputVec[0] + " takes at least one argument");
	}
	if (cmdInput.inputVec.size() == 2) {
		if (isNumber(cmdInput.inputVec[1])) {
			end = stoi(cmdInput.inputVec[1]);
		}
		else {
			return genError("if only one argument is provided to " + cmdInput.inputVec[0] + ", it must be a number");
		}
	}
	else {
		if (cmdInput.inputVec.size() == 3) {
			if ((isNumber(cmdInput.inputVec[1]) && !isNumber(cmdInput.inputVec[2])) || (!isNumber(cmdInput.inputVec[1]) && isNumber(cmdInput.inputVec[2]))) {
				return genError("if " + cmdInput.inputVec[0] + " receives two arguments, they should both be of the same type, int or std::string");
			}
			if (isNumber(cmdInput.inputVec[1])) {
				start = stoi(cmdInput.inputVec[1]);
				end = stoi(cmdInput.inputVec[2]);
			}
			else {
				end = (int)cmdInput.inputVec.size() - 1;
				symbols = true;
			}
		}
		else {
			end = (int)cmdInput.inputVec.size() - 1;
			symbols = true;
		}
	}
	diff = end - start;
	// the random value goes from start up to end, not including end, so end must be greater
	if (diff <= 0) {
		if (cmdInput.inputVec.size() == 2) return genError("the argument of " + cmdInput.inputVec[0] + " must be greater than 0");
		return genError("the second argument of " + cmdInput.inputVec[0] + " must be greater than the first");
	}
	randVal = rand() % diff + start;
	// a symbol or a number, which replaces \rand and its arguments
	// (the output of the command at the start of a line is not used, see parseString())
	if (symbols) return genOutput(cmdInput.inputVec[randVal+1]);
	return genOutput(std::to_string(randVal));
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdList(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() < 2) {
		return genError("\\std::list takes a name as an argument");
	}
	if (cmdInput.inputVec.size() < 3) {
		return genError("\\list must be assigned at least one item, inside curly brackets");
	}
	std::pair<int, std::string> p = checkNameAvailability("list", cmdInput.inputVec[1]);
	if (p.first == 3) {
		return genError(p.second);
	}
	std::string listName = p.second;
	if (listIndexes.find(listName) != listIndexes.end()) {
		return genError("list already exists");
	}
	storingList = true;
	// the next index after the highest one (listIndexes is ordered by the names, so its last item is not the one with the highest index)
	lastListIndex = 0;
	for (const auto& list : listIndexes) lastListIndex = std::max(lastListIndex, list.second + 1);
	listIndexes[listName] = lastListIndex;
	commandsMap[livelily][listName.substr(1)[0]][listName] = Colors::lightSeaGreen;
	std::string restOfCommand = genStrFromVec({cmdInput.inputVec.begin()+2, cmdInput.inputVec.end()});
	std::pair<int, std::string> error = parseString(restOfCommand, lineNum, numLines);
	if (error.first == 3) {
		storingList = false;
		return genError(error.second);
	}
	keywords.push_back(listName);
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdExpand(CmdInput& cmdInput, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	if (cmdInput.inputVec.size() == 1) {
		return genError("\\expand takes at least one argument, a bar name to expand");
	}
	for (size_t i = 1; i < cmdInput.inputVec.size(); i++) {
		if (cmdInput.inputVec[i] == "|") {
			cmdOutput.outputVec.push_back("|");
		}
		else {
			CmdInput cmdInputLocal = {{cmdInput.inputVec[i]}, std::vector<std::string>(), false, false};
			CmdOutput cmdOutputLocal = parseCommand(cmdInputLocal, lineNum, numLines);
			for (std::string s : cmdOutputLocal.outputVec) {
				cmdOutput.outputVec.push_back(s);
			}
		}
	}
	return cmdOutput;
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdFromosc(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() > 2) {
		return genError("\\fromosc commands takes maximum one argument");
	}
	if (cmdInput.inputVec.size() > 1) {
		if (!startsWith(cmdInput.inputVec[1], "/")) {
			return genError("OSC address must start with /");
		}
		fromOscAddr[whichPane] = cmdInput.inputVec[1];
	}
	else {
		fromOscAddr[whichPane] = "/livelily" + std::to_string(whichPane+1);
	}
	fromOsc[whichPane] = true;
	if (!oscReceiverIsSet) {
		oscReceiver.setup(OSCPORT);
		oscReceiverIsSet = true;
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdRemaining(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (!parsingBar) {
		return genError("\\remaining can only be called inside a bar definition");
	}
	int numFoundInsts = 0;
	if (cmdInput.inputVec.size() == 2) {
		if (sharedData.loopsIndexes.find(cmdInput.inputVec[1]) == sharedData.loopsIndexes.end()) {
			return genError("bar " + cmdInput.inputVec[1] + " doesn't exist");
		}
	}
	else {
		return genError("\\remaining takes one argument");
	}
	for (std::map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		if (!it->second.hasPassed()) {
			numFoundInsts++;
			break;
		}
	}
	if (numFoundInsts == 0) {
		return genWarning("all instruments have been defined, nothing left for \\remaining");
	}
	for (std::map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		if (!it->second.hasPassed()) {
			std::vector<std::string> v = {"\\" + it->second.getName()};
			v.insert(v.end(), cmdInput.inputVec.begin()+1, cmdInput.inputVec.end());
			// isInstrument returns a pair of bool and CmdOutput
			// this is because in parseCommand() the bool is used to determine whether the first command is indeed an instrument
			// if it is, parseCommand() will end at calling isInstrument()
			// otherwise it goes on
			// here we know that the first item in the v std::vector is an instrument
			// but we need to comply with what isInstrument() returns
			std::pair<bool, CmdOutput> p = isInstrument(v, lineNum, numLines);
			if (p.second.errorCode > 0) {
				return genOutputFuncs(std::make_pair(p.second.errorCode, p.second.errorStr));
			}
		}
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdSolo(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() < 2) {
		return genError("\\solo commands takes at least one argument");
	}
	if (cmdInput.inputVec.size() - 1 > sharedData.instrumentIndexes.size()) {
		return genError("too many arguments for \\solo command");
	}
	if (cmdInput.inputVec.size() == 2 && cmdInput.inputVec[1].compare("\\all") == 0) {
		for (std::map<std::string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
			sharedData.instruments.at(it->second).setToUnmute(true);
		}
	}
	else {
		std::vector<std::string> soloInsts;
		for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
			if (sharedData.instrumentIndexes.find(cmdInput.inputVec[i]) == sharedData.instrumentIndexes.end()) {
				return genError(cmdInput.inputVec[i] + ": unknown instrument");
			}
			else {
				soloInsts.push_back(cmdInput.inputVec[i]);
			}
		}
		// mute all instruments but the solo ones, which are unmuted, if they have been muted before
		for (std::map<std::string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
			if (find(soloInsts.begin(), soloInsts.end(), it->first) == soloInsts.end()) {
				sharedData.instruments.at(it->second).setToMute(true);
			}
			else {
				sharedData.instruments.at(it->second).setToUnmute(true);
			}
		}
	}
	applyMutesNow();
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdSolonow(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() < 2) {
		return genError("\\solonow commands takes at least one argument");
	}
	// below we test if the number of tokens is greater than the number of instruments
	// and not greater than or equal to, because the first token in the \solonow command
	if (cmdInput.inputVec.size() - 1 > sharedData.instrumentIndexes.size()) {
		return genError("too many arguments for \\solonow command");
	}
	// \solonow \all plays all instruments, as \solo \all does
	if (cmdInput.inputVec.size() == 2 && cmdInput.inputVec[1].compare("\\all") == 0) {
		for (std::map<std::string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
			sharedData.instruments.at(it->second).setMute(false);
		}
	}
	else {
		std::vector<std::string> soloInsts;
		for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
			if (sharedData.instrumentIndexes.find(cmdInput.inputVec[i]) == sharedData.instrumentIndexes.end()) {
				return genError(cmdInput.inputVec[i] + ": unknown instrument");
			}
			else {
				soloInsts.push_back(cmdInput.inputVec[i]);
			}
		}
		// mute all instruments but the solo ones, which are unmuted, if they have been muted before
		for (std::map<std::string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
			sharedData.instruments.at(it->second).setMute(find(soloInsts.begin(), soloInsts.end(), it->first) == soloInsts.end());
		}
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdMute(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() < 2) {
		return genError("\\mute command takes at least one argument");
	}
	// again test if greater than and not if greater than or equal to
	if (cmdInput.inputVec.size() - 1 > sharedData.instruments.size()) {
		return genError("too many arguments for \\mute command");
	}
	if (cmdInput.inputVec[1].compare("\\all") == 0) {
		for (std::map<std::string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
			sharedData.instruments.at(it->second).setToMute(true);
		}
	}
	else {
		std::vector<std::string> muteInsts;
		for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
			if (sharedData.instrumentIndexes.find(cmdInput.inputVec[i]) == sharedData.instrumentIndexes.end()) {
				return genError(cmdInput.inputVec[i] + ": unknown instrument");
			}
			else {
				muteInsts.push_back(cmdInput.inputVec[i]);
			}
		}
		for (std::map<std::string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
			if (find(muteInsts.begin(), muteInsts.end(), it->first) != muteInsts.end()) {
				sharedData.instruments.at(it->second).setToMute(true);
			}
		}
	}
	applyMutesNow();
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdUnmute(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() < 2) {
		return genError("\\unmute command takes at least one argument");
	}
	// again test if greater than and not if greater than or equal to
	// -1 is there because the input std::vector contains the command too
	if (cmdInput.inputVec.size()-1 > sharedData.instruments.size()) {
		return genError("too many arguments for \\unmute command");
	}
	if (cmdInput.inputVec[1].compare("\\all") == 0) {
		for (std::map<std::string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
			sharedData.instruments[it->second].setToUnmute(true);
		}
	}
	else {
		std::vector<std::string> unmuteInsts;
		for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
			if (sharedData.instrumentIndexes.find(cmdInput.inputVec[i]) == sharedData.instrumentIndexes.end()) {
				return genError(cmdInput.inputVec[i] + ": unknown instrument");
			}
			else {
				unmuteInsts.push_back(cmdInput.inputVec[i]);
			}
		}
		for (std::map<std::string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
			if (find(unmuteInsts.begin(), unmuteInsts.end(), it->first) != unmuteInsts.end()) {
				sharedData.instruments.at(it->second).setToUnmute(true);
			}
		}
	}
	applyMutesNow();
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdMutenow(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() < 2) {
		return genError("\\mutenow command takes at least one argument");
	}
	// the arguments don't include the command name, so all instruments can be named
	if (cmdInput.inputVec.size() - 1 > sharedData.instruments.size()) {
		return genError("too many arguments for \\mutenow command");
	}
	if (cmdInput.inputVec[1].compare("\\all") == 0) {
		for (std::map<std::string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
			sharedData.instruments[it->second].setMute(true);
		}
	}
	else {
		std::vector<std::string> muteInsts;
		for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
			if (sharedData.instrumentIndexes.find(cmdInput.inputVec[i]) == sharedData.instrumentIndexes.end()) {
				return genError(cmdInput.inputVec[i] + ": unknown instrument");
			}
			else {
				muteInsts.push_back(cmdInput.inputVec[i]);
			}
		}
		for (std::map<std::string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
			if (find(muteInsts.begin(), muteInsts.end(), it->first) != muteInsts.end()) {
				sharedData.instruments.at(it->second).setMute(true);
			}
		}
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdUnmutenow(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() < 2) {
		return genError("\\unmutenow command takes at least one argument");
	}
	// the arguments don't include the command name, so all instruments can be named
	if (cmdInput.inputVec.size() - 1 > sharedData.instruments.size()) {
		return genError("too many arguments for \\unmutenow command");
	}
	if (cmdInput.inputVec[1].compare("\\all") == 0) {
		for (std::map<std::string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
			sharedData.instruments[it->second].setMute(false);
		}
	}
	else {
		std::vector<std::string> unmuteInsts;
		for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
			if (sharedData.instrumentIndexes.find(cmdInput.inputVec[i]) == sharedData.instrumentIndexes.end()) {
				return genError(cmdInput.inputVec[i] + ": unknown instrument");
			}
			else {
				unmuteInsts.push_back(cmdInput.inputVec[i]);
			}
		}
		for (std::map<std::string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
			if (find(unmuteInsts.begin(), unmuteInsts.end(), it->first) != unmuteInsts.end()) {
				sharedData.instruments.at(it->second).setMute(false);
			}
		}
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdFunction(CmdInput& cmdInput, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	if (cmdInput.inputVec.size() < 2) {
		return genError("\\function needs to be given a name");
	}
	if (startsWith(cmdInput.inputVec[1], "{")) {
		return genError("\\function needs to be given a name");
	}
	if (cmdInput.inputVec.size() > 2) {
		if (!startsWith(cmdInput.inputVec[2], "{")) {
			return genError("after the function name, the function body must be enclosed in curly brackets");
		}
	}
	{
		// the sequencer thread might be iterating over the functions map or reading a function body
		std::lock_guard<std::mutex> lock(sharedData.functionsMutex);
		if (functionIndexes.find("\\"+cmdInput.inputVec[1]) != functionIndexes.end()) {
			storingFunctionIndex = functionIndexes["\\"+cmdInput.inputVec[1]];
			sharedData.functions[storingFunctionIndex].clear();
			storingFunction = true;
		}
		else {
			int functionNdx = 0;
			std::pair<int, std::string> p = checkNameAvailability("function", cmdInput.inputVec[1]);
			if (p.first == 3) {
				return genError(p.second);
			}
			std::string functionName = p.second;
			// the next index after the highest one (functionIndexes is ordered by the names, so its last item is not the one with the highest index)
			for (const auto& func : functionIndexes) functionNdx = std::max(functionNdx, func.second + 1);
			functionIndexes[functionName] = functionNdx;
			Function function;
			sharedData.functions[functionNdx] = function;
			sharedData.functions[functionNdx].setName(functionName);
			if (sharedData.functions[functionNdx].getNameError()) {
				sharedData.functions.erase(functionNdx);
				functionIndexes.erase(functionName);
				return genError("function name too long");
			}
			commandsMap[livelily][functionName.substr(1)[0]][functionName] = Colors::turquoise;
			// the lines of a function name are not expanded, as they store their data (e.g. \funcname.bind, \funcname.onrelease etc.)
			// see methodSpecs
			storingFunction = true;
			storingFunctionIndex = functionNdx;
			keywords.push_back(functionName);
		}
		// store where the function is defined (also when redefining it), to show errors of bound functions there
		sharedData.functions[storingFunctionIndex].setDefinitionLocation(whichPane, lineNum);
	}
	// parse any characters in the rest of this line, starting with the opening bracket
	// which storeFunctionLine() uses to start tracking the brackets of the body
	// this is called without holding the lock, as storing the line locks it again
	functionBodyDepth = 0;
	std::string restOfCommand = genStrFromVec({cmdInput.inputVec.begin()+2, cmdInput.inputVec.end()});
	std::pair<int, std::string> error = parseString(restOfCommand, lineNum, numLines);
	if (error.first > 0) {
		cmdOutput.errorCode = error.first;
		cmdOutput.errorStr = error.second;
		storingFunction = false;
	}
	return cmdOutput;
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdTune(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() != 2) {
		return genError("\\tune takes one argument, the tuning frequency");
	}
	if (!isNumber(cmdInput.inputVec[1]) && !isFloat(cmdInput.inputVec[1])) {
		return genError("the argument to \\tune must be a number");
	}
	float midiTune = std::stof(cmdInput.inputVec[1]);
	if (midiTune <= 0) {
		return genError("tuning frequency must be positive");
	}
	sequencer.setMidiTune(midiTune);
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdListmidiports(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() > 1) {
		return genError("\\listmidiports command takes no arguments");
	}
	sequencer.midiOutPorts = midi->portNames();
	if (sequencer.midiOutPorts.empty()) {
		return genNote("no MIDI out ports available");
	}
	std::string noteStr = "";
	for (unsigned i = 0; i < sequencer.midiOutPorts.size()-1; i++) {
		noteStr += "MIDI out port " + std::to_string(i) + ": " + sequencer.midiOutPorts[i] += "\n";
	}
	// append the last port separately to not include the newline
	noteStr += "MIDI out port " + std::to_string(sequencer.midiOutPorts.size()-1) + ": " + sequencer.midiOutPorts.back();
	return genNote(noteStr);
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdOpenmidiport(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() != 2) {
		return genError("\\openmidiport takes one argument, the MIDI port to open");
	}
	if (!isNumber(cmdInput.inputVec[1])) {
		return genError("MIDI port must be a number");
	}
	int midiPort = stoi(cmdInput.inputVec[1]);
	if (midiPort < 0) {
		return genError("MIDI port must be positive");
	}
	if (midiPort >= (int)sequencer.midiOutPorts.size()) {
		return genError("MIDI port doesn't exist");
	}
	// a port that an instrument has opened (with \instname.midiport) can also be opened with this command
	// so that it gets the messages that don't belong to an instrument (the MIDI clock)
	if (!sequencer.isGenericMidiPort(midiPort)) {
		if (!midi->isOpen(midiPort)) midi->open(midiPort);
		sequencer.addGenericMidiPort(midiPort);
		return genNote("opened MIDI port " + cmdInput.inputVec[1]);
	}
	else {
		return genError("MIDI port " + cmdInput.inputVec[1] + " is already open");
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdCtlchange(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() != 5) {
		return genError("\\ctlchange takes four arguments, port, channel, controller nr, and value");
	}
	if (!isNumber(cmdInput.inputVec[1]) || !isNumber(cmdInput.inputVec[2]) || !isNumber(cmdInput.inputVec[3]) || !isNumber(cmdInput.inputVec[4])) {
		return genError("all arguments to \\ctlchange must be integers");
	}
	int port = stoi(cmdInput.inputVec[1]);
	if (!midi->isOpen(port)) {
		return genError("selected MIDI port is not open");
	}
	int channel = stoi(cmdInput.inputVec[2]);
	int control = stoi(cmdInput.inputVec[3]);
	int val = stoi(cmdInput.inputVec[4]);
	if (channel < 1 || channel > 16) {
		return genError("MIDI channel must be between 1 and 16");
	}
	if (control < 0) {
		return genError("control number must be positive");
	}
	if (val < 0 || val > 127) {
		return genError("MIDI value must be between 0 and 127");
	}
	midi->controlChange(port, channel, control, val);
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdPgmchange(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() != 4) {
		return genError("\\pgmchange takes three arguments, port, channel, and value");
	}
	if (!isNumber(cmdInput.inputVec[1]) || !isNumber(cmdInput.inputVec[2]) || !isNumber(cmdInput.inputVec[3])) {
		return genError("all arguments to \\pgmchange must be integers");
	}
	int port = stoi(cmdInput.inputVec[1]);
	if (!midi->isOpen(port)) {
		return genError("selected MIDI port is not open");
	}
	int channel = stoi(cmdInput.inputVec[2]);
	int val = stoi(cmdInput.inputVec[3]);
	if (channel < 1 || channel > 16) {
		return genError("MIDI channel must be between 1 and 16");
	}
	if (val < 0 || val > 127) {
		return genError("MIDI value must be between 0 and 127");
	}
	midi->programChange(port, channel, val);
	return CmdOutput();
}

//--------------------------------------------------------------
// the test below includes all functions that set duration percetanges for notes
// currently default Note On duration, staccato, staccatissimo, and tenuto
CmdOutput ofApp::cmdNotedur(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec[0].compare("\\notedur") == 0 && cmdInput.inputVec.size() < 2) {
		return genError("the \\notedur command does nothing by itself, you must call one of its methods");
	}
	else if (cmdInput.inputVec[0].compare("\\notedur") == 0 && cmdInput.inputVec.size() >= 2) {
		return genError("the \\notedur command takes only methods, not arguments");
	}
	std::string subcommand = cmdInput.inputVec[0].substr(cmdInput.inputVec[0].find(".")+1);
	if (cmdInput.inputVec.size() == 1) {
		return genError("no percentage value provided");
	}
	if (cmdInput.inputVec.size() > 2) {
		return genError("\\notedur." + subcommand + " command takes one argument only");
	}
	if (!isNumber(cmdInput.inputVec[1])) {
		return genError("percentage to \\notedur." + subcommand + " must be an integer");
	}
	int percentage = stoi(cmdInput.inputVec[1]);
	if (percentage < 0) {
		return genError("percentage to \\notedur." + subcommand + " can't be below 0");
	}
	if (percentage == 0) {
		return genError("percentage to \\notedur." + subcommand + " can't be 0");
	}
	if (percentage > 100) {
		return genError("percentage to \\notedur." + subcommand + " can't be over 100\%");
	}
	if (subcommand != "normal" && subcommand != "staccato" && subcommand != "staccatissimo" && subcommand != "tenuto") {
		return genError(subcommand + ": unknown \\notedur method");
	}
	// stored, so that instruments created afterwards get it too (see initializeInstrument())
	noteDurSettings[subcommand] = (float)percentage/100.0;
	for (unsigned i = 0; i < sharedData.instruments.size(); i++) {
		int error = sharedData.instruments[i].setDuration(subcommand, (float)percentage/100.0);
		if (error) return genError("unknown duration method");
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdListserialports(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() > 1) {
		return genError("\\listserialports command takes no arguments");
	}
	serial.listDevices();
	serialDeviceList = serial.getDeviceList();
	if (serialDeviceList.empty()) {
		return genNote("no serial ports available");
	}
	std::string outPortsOneStr = "";
	for (unsigned i = 0; i < serialDeviceList.size()-1; i++) {
		outPortsOneStr += "Serial port " + std::to_string(serialDeviceList[i].getDeviceID()) + \
						  ": " + serialDeviceList[i].getDevicePath() + "\n";
	}
	// append last port separately to not include the newline
	outPortsOneStr += "Serial port " + std::to_string(serialDeviceList.back().getDeviceID()) + \
					  ": " + serialDeviceList.back().getDevicePath();
	return genNote(outPortsOneStr);
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdOpenserialport(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() < 3 || cmdInput.inputVec.size() > 3) {
		return genError("\\openserialport takes two arguments, port path and baud rate");
	}
	int serialPort;
	bool serialPortNumSet = false;
	if (isNumber(cmdInput.inputVec[1])) {
		serialPort = stoi(cmdInput.inputVec[1]);
		if (serialPort < 0) {
			return genError("serial port list number must be positive");
		}
		if (serialPort >= (int)serialDeviceList.size()) {
			return genError("serial port list number out of range");
		}
		serialPortNumSet = true;
	}
	if (!isNumber(cmdInput.inputVec[2])) {
		return genError("second argument must be baud rate");
	}
	if (serialPortNumSet) {
		serial.setup(serialDeviceList[serialPort].getDevicePath(), stoi(cmdInput.inputVec[2]));
	}
	else {
		bool serialPortPathFound = false;
		for (unsigned i = 0; i < serialDeviceList.size(); i++) {
			if (serialDeviceList[i].getDevicePath().compare(cmdInput.inputVec[1]) == 0) {
				serialPortPathFound = true;
				break;
			}
		}
		if (serialPortPathFound) {
			serial.setup(cmdInput.inputVec[1], stoi(cmdInput.inputVec[2]));
		}
		else {
			return genError("serial port path \"" + cmdInput.inputVec[1] + "\" doesn't exist");
		}
	}
	serialPortOpen = true;
	return genNote("opened " + cmdInput.inputVec[1] + " at baud " + cmdInput.inputVec[2]);
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdSerialwrite(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() == 1) {
		return genError("\\serialwrite takes at least one argument, a byte to send over serial");
	}
	if (!serial.isInitialized()) {
		return genError("serial port is not open");
	}
	for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
		if (!isNumber(cmdInput.inputVec[i])) {
			return genError("\\serialwrite takes numbers from 0 to 255 only");
		}
		int value = stoi(cmdInput.inputVec[i]);
		if (value < 0 || value > 255) {
			return genError("\\serialwrite takes numbers from 0 to 255");
		}
		serial.writeByte((char)value);
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdSerialprint(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() == 1) {
		return genError("\\serialprint takes at least one argument, a character to send over serial");
	}
	if (!serial.isInitialized()) {
		return genError("serial port is not open");
	}
	size_t charSize = 0;
	for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
		for (unsigned j = 0; j < cmdInput.inputVec[i].size(); j++) {
			charSize++;
		}
		charSize++;
	}
	// the last addition happens one time two many, so we subtract one here
	charSize--;
	unsigned char buf[charSize];
	int index = 0;
	for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
		for (unsigned j = 0; j < cmdInput.inputVec[i].size(); j++) {
			buf[index++] = cmdInput.inputVec[i][j];
		}
		if (i < cmdInput.inputVec.size() - 1) buf[index++] = ' ';
	}
	serial.writeBytes(&buf[0], charSize);
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdMousex(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() > 1) {
		return genError("\\mousex takes no arguments");
	}
	return genOutput(std::to_string(mouseX));
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdMousey(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() > 1) {
		return genError("\\mousey takes no arguments");
	}
	return genOutput(std::to_string(mouseY));
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdMouse(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() > 1) {
		return genError("\\mouse takes no arguments");
	}
	return genOutput({std::to_string(mouseX), std::to_string(mouseY)});
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdCloseserialport(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (serial.isInitialized()) {
		serial.close();
	}
	return CmdOutput();
}

//--------------------------------------------------------------
// the output of a system command (what it prints to the standard output), for \system
static std::string systemCommandOutput(const std::string& command)
{
	std::string output;
#ifdef _WIN32
	FILE* pipe = _popen(command.c_str(), "r");
#else
	FILE* pipe = popen(command.c_str(), "r");
#endif
	if (!pipe) return output;
	char buffer[256];
	size_t numRead;
	while ((numRead = fread(buffer, 1, sizeof(buffer), pipe)) > 0) output.append(buffer, numRead);
#ifdef _WIN32
	_pclose(pipe);
#else
	pclose(pipe);
#endif
	return output;
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdSystem(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() == 1) {
		return genWarning("no command added to \\system");
	}
	unsigned i;
	std::string sysCommand = "";
	for (i = 1; i < cmdInput.inputVec.size(); i++) {
		if (i > 1) sysCommand += " ";
		sysCommand += cmdInput.inputVec[i];
	}
	return genOutput(systemCommandOutput(sysCommand));
}

//--------------------------------------------------------------
// a click track on the beats of \beatat, \clicktrack on or off, and \clicktrack.tune with the frequency of its clicks
CmdOutput ofApp::cmdClicktrack(CmdInput& cmdInput, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	if (cmdInput.inputVec[0].compare("\\clicktrack") == 0) {
		if (cmdInput.inputVec.size() != 2) {
			return genError("\\clicktrack takes one argument only, \"on\" or \"off\"");
		}
		if (cmdInput.inputVec[1].compare("on") != 0 && cmdInput.inputVec[1].compare("off") != 0) {
			return genError("argument to \\clicktrack must be \"on\" or \"off\"");
		}
		if (cmdInput.inputVec[1].compare("on") == 0) {
			startSoundStream();
			if (!soundStreamSet) {
				return genError("could not start the sound stream for the click track");
			}
			sharedData.clickTrack.setOn(true);
		}
		else {
			sharedData.clickTrack.setOn(false);
		}
	}
	else {
		std::string method = cmdInput.inputVec[0].substr(cmdInput.inputVec[0].find('.') + 1);
		if (method.compare("volume") == 0) {
			if (cmdInput.inputVec.size() != 2) {
				return genError("\\clicktrack.volume takes one argument, the volume of the clicks from 0 to 1");
			}
			if (!isNumber(cmdInput.inputVec[1]) && !isFloat(cmdInput.inputVec[1])) {
				return genError(cmdInput.inputVec[1] + " is not a number");
			}
			float vol = std::stof(cmdInput.inputVec[1]);
			if (vol < 0 || vol > 1) {
				return genError("the volume of the click track must be between 0 and 1");
			}
			sharedData.clickTrack.setVolume(vol);
			return cmdOutput;
		}
		if (method.compare("tune") != 0) {
			return genError(method + ": unknown method of \\clicktrack");
		}
		if (cmdInput.inputVec.size() != 2) {
			return genError("\\clicktrack.tune takes one argument, the frequency of the clicks in Hz");
		}
		if (!isNumber(cmdInput.inputVec[1]) && !isFloat(cmdInput.inputVec[1])) {
			return genError(cmdInput.inputVec[1] + " is not a number");
		}
		float freq = std::stof(cmdInput.inputVec[1]);
		if (freq <= 0) {
			return genError("the frequency of the click track must be greater than 0");
		}
		// the first beat of a bar is an octave higher, so it must stay below half the sample rate
		if (freq * 2 >= sampleRate / 2) {
			return genError("the frequency of the click track must be below " + std::to_string(sampleRate / 4) + "Hz, as the first beat is an octave higher");
		}
		sharedData.clickTrack.setTune(freq);
	}
	return cmdOutput;
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdPystr(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() != 2) {
		return genError("\\pystr command takes one argument, the typing mode");
	}
	if (cmdInput.inputVec[1].compare("char") == 0) {
		sharedData.typePyStrCharByChar = true;
	}
	else if (cmdInput.inputVec[1].compare("line") == 0) {
		sharedData.typePyStrCharByChar = false;
	}
	else {
		return genError(cmdInput.inputVec[1] + ": unknown argument to \\pystr");
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdActive(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() > 1) {
		return genError("\\active takes no arguments");
	}
	editors[whichPane].setSessionActivity(true);
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdInactive(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() > 1) {
		return genError("\\inactive takes no arguments");
	}
	editors[whichPane].setSessionActivity(false);
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdSendkeys(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() != 3) {
		return genError("\\sendkeys takes two arguments, host IP and pane index");
	}
	std::string hostIP = cmdInput.inputVec[1];
	int paneNdx;
	if (!isNumber(cmdInput.inputVec[2])) {
		return genError("pane index of host is not a number");
	}
	paneNdx = stoi(cmdInput.inputVec[2]);
	editors[whichPane].oscKeys.setup(hostIP, OSCPORT);
	editors[whichPane].setSendKeys(paneNdx);
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdSendlines(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() != 3) {
		return genError("\\sendlines takes two arguments, host IP and pane index");
	}
	std::string hostIP = cmdInput.inputVec[1];
	int paneNdx;
	if (!isNumber(cmdInput.inputVec[2])) {
		return genError("pane index of host is not a number");
	}
	paneNdx = stoi(cmdInput.inputVec[2]);
	editors[whichPane].oscKeys.setup(hostIP, OSCPORT);
	editors[whichPane].setSendLines(paneNdx);
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdMaestro(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec[0].compare("\\maestro") == 0 && cmdInput.inputVec.size() < 2) {
		return genError("the \\maestro command does nothing by itself, you must call one of its methods");
	}
	else if (cmdInput.inputVec[0].compare("\\maestro") == 0 && cmdInput.inputVec.size() >= 2) {
		return genError("the \\maestro command takes only methods, not arguments");
	}
	return maestroCommands(cmdInput.inputVec, lineNum, numLines);
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdFramerate(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() == 1) {
		// the frame rate as a number with its default formatting (e.g. 60 or 59.9401)
		std::ostringstream frameRate;
		frameRate << ofGetFrameRate();
		return genOutput(frameRate.str());
	}
	if (cmdInput.inputVec.size() != 2) {
		return genError("\\framerate takes one argument, the framerate");
	}
	if (!isNumber(cmdInput.inputVec[1])) {
		return genError("argument to \\framerate must be an integer");
	}
	int framerate = stoi(cmdInput.inputVec[1]);
	if (framerate <= 0) {
		return genError("framerate must be positive and higher than 0");
	}
	ofSetFrameRate(framerate);
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdListaudiodev(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() > 1) {
		return genError("\\listaudiodev takes no arguments");
	}
	std::vector<ofSoundDevice> v = soundStream.getDeviceList();
	std::string audioDevs = "";
	for (size_t i = 0; i < v.size(); i++) {
		if (i > 0) audioDevs += "\n";
		audioDevs += std::to_string(i) + ": " + v.at(i).name;
	}
	return genNote(audioDevs);
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdSetaudiodev(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() != 2) {
		return genError("\\setaudiodev takes one argument, the audio device ID");
	}
	if (!isNumber(cmdInput.inputVec[1])) {
		return genError("argument to \\setaudiodev must be an integer");
	}
	int devID = stoi(cmdInput.inputVec[1]);
	std::vector<ofSoundDevice> v = soundStream.getDeviceList();
	if (devID < 0) {
		return genError("audio device ID can't be negative");
	}
	if (devID >= (int)v.size()) {
		return genError("wrong audio device ID");
	}
	inSoundDevice = v[devID];
	outSoundDevice = v[devID];
	inSoundDeviceSet = true;
	outSoundDeviceSet = true;
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdSetinaudiodev(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() != 2) {
		return genError("\\setinaudiodev takes one argument, the audio device ID");
	}
	if (!isNumber(cmdInput.inputVec[1])) {
		return genError("argument to \\setinaudiodev must be an integer");
	}
	int devID = stoi(cmdInput.inputVec[1]);
	std::vector<ofSoundDevice> v = soundStream.getDeviceList();
	if (devID < 0) {
		return genError("audio device ID can't be negative");
	}
	if (devID >= (int)v.size()) {
		return genError("wrong audio device ID");
	}
	inSoundDevice = v[devID];
	inSoundDeviceSet = true;
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdSetoutaudiodev(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() != 2) {
		return genError("\\setoutaudiodev takes one argument, the audio device ID");
	}
	if (!isNumber(cmdInput.inputVec[1])) {
		return genError("argument to \\setoutaudiodev must be an integer");
	}
	int devID = stoi(cmdInput.inputVec[1]);
	std::vector<ofSoundDevice> v = soundStream.getDeviceList();
	if (devID < 0) {
		return genError("audio device ID can't be negative");
	}
	if (devID >= (int)v.size()) {
		return genError("wrong audio device ID");
	}
	outSoundDevice = v[devID];
	outSoundDeviceSet = true;
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdSamplerate(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() != 2) {
		return genError("\\samplerate takes one argument, the audio sample rate");
	}
	if (!isNumber(cmdInput.inputVec[1])) {
		return genError("argument to \\samplerate must be an integer");
	}
	sampleRate = stoi(cmdInput.inputVec[1]);
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdBuffersize(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() != 2) {
		return genError("\\buffersize takes one argument, the audio buffer size");
	}
	if (!isNumber(cmdInput.inputVec[1])) {
		return genError("argument to \\buffersize must be an integer");
	}
	bufferSize = stoi(cmdInput.inputVec[1]);
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdNchannels(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() != 2) {
		return genError("\\nchannels takes one argument, the number of audio I/O channels");
	}
	if (!isNumber(cmdInput.inputVec[1])) {
		return genError("argument to \\nchannels must be an integer");
	}
	inChannels = outChannels = stoi(cmdInput.inputVec[1]);
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdInchannels(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() != 2) {
		return genError("\\inchannels takes one argument, the number of audio input channels");
	}
	if (!isNumber(cmdInput.inputVec[1])) {
		return genError("argument to \\inchannels must be an integer");
	}
	inChannels = stoi(cmdInput.inputVec[1]);
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdOutchannels(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() != 2) {
		return genError("\\outchannels takes one argument, the number of audio output channels");
	}
	if (!isNumber(cmdInput.inputVec[1])) {
		return genError("argument to \\outchannels must be an integer");
	}
	outChannels = stoi(cmdInput.inputVec[1]);
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdLivelily(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() > 1) {
		return genError("language command takes no arguments");
	}
	editors[whichPane].setLanguage(livelily);
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdPython(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec[0].find(".") != std::string::npos) {
		std::vector<std::string> v = tokenizeString(cmdInput.inputVec[0], ".");
		if (v.size() > 1) {
			if (v[1].compare("send") == 0) {
				if (!pyoSet) {
					return genError("python has not been initialized");
				}
#ifdef USEPYO
				if (cmdInput.inputVec.size() > 1) {
					std::string pyoStr = genStrFromVec({cmdInput.inputVec.begin()+1, cmdInput.inputVec.end()});
					sharedData.pyo.exec(pyoStr.c_str());
				}
#endif
			}
			else {
				return genError("\\python takes only \"send\" as a method");
			}
		}
	}
	else {
		if (cmdInput.inputVec.size() > 1) {
			return genError("language command takes no arguments, only \"send\" method");
		}
		// if we switch to Python, we initialize the audio settings
		initPyo();
		editors[whichPane].setLanguage(python);
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdLua(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() > 1) {
		return genError("language command takes no arguments");
	}
	editors[whichPane].setLanguage(lua);
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdAccoffset(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() != 2) {
		return genError("\\accoffset command takes one argument, the offset for accidentals that are close together");
	}
	if (!isFloat(cmdInput.inputVec[1])) {
		return genError("argument to \\accoffset is not a number");
	}
	float coef = std::stof(cmdInput.inputVec[1]);
	// stored, so that instruments created afterwards get it too (see initializeInstrument())
	accOffsetSet = true;
	accOffsetCoef = coef;
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		it->second.setAccidentalsOffsetCoef(coef);
	}
	return CmdOutput();
}

//--------------------------------------------------------------
CmdOutput ofApp::cmdHelp(CmdInput& cmdInput, int lineNum, int numLines)
{
	if (cmdInput.inputVec.size() == 1) {
		return genError("nothing specified to open its help file");
	}
	int newPane = 0;
	if (cmdInput.inputVec.size() > 2 && (cmdInput.inputVec[2].compare("vert") == 0 || cmdInput.inputVec[2].compare("hor") == 0)) {
		if (cmdInput.inputVec[2].compare("vert") == 0) {
			newPane = 1;
		}
		else {
			newPane = 2;
		}
	}
	std::string filePath = HELPFILEPATH;
	if (startsWith(filePath, "~")) {
		filePath = getenv("HOME") + filePath.substr(1);
	}
	// the command can be given with or without its backslash
	std::string helpName = cmdInput.inputVec[1];
	if (startsWith(helpName, "\\")) helpName = helpName.substr(1);
	filePath += "/" + helpName + "-help.lyv";
	switch (newPane) {
		case 1:
			addPane(86);
			break;
		case 2:
			addPane(72);
			break;
		default:
			break;
	}
	bool fileOpened = editors[whichPane].loadHelpFile(filePath, newPane);
	if (!fileOpened) {
		if (newPane > 0) {
			removePane();
		}
		return genError("couldn't find help file for " + cmdInput.inputVec[1]);
	}
	return CmdOutput();
}

//--------------------------------------------------------------
// a variable definition or assignment (e.g. x = 5, \x = 5, or \x.val = 5), a line whose second item is =
CmdOutput ofApp::defineVariable(CmdInput& cmdInput, int lineNum, int numLines)
{
	// if we have more than three items in the input vector and the second one is an equals sign
	// then we are defining a variable, or assigning a new value to it
	// the variable can be written without a backslash (var = 1), or with one, alone or with its value method
	// (\var = 1 or \var.val = 1, e.g. inside a function), which all set the variable var
	// as the variable name is stored with a backslash, a leading backslash is not added again
	std::string plainVarName = cmdInput.inputVec[0];
	if (startsWith(plainVarName, "\\")) plainVarName = plainVarName.substr(1);
	if (endsWith(plainVarName, ".val")) plainVarName = plainVarName.substr(0, plainVarName.size() - 4);
	if (plainVarName.empty() || plainVarName.find('.') != std::string::npos || plainVarName.find('\\') != std::string::npos) {
		return genError("wrong variable name: " + cmdInput.inputVec[0]);
	}
	std::string varName = "\\" + plainVarName;
	if (isReservedName(varName)) {
		return genError(plainVarName + " can't be a variable name, " + varName + " is a command");
	}
	std::string taken = nameTakenError(varName, NameKind::variable);
	if (!taken.empty()) return genError(taken);
	if (cmdInput.inputVec.size() > 3) {
		if (cmdInput.inputVec.size() % 2 == 0) {
			return genError("wrong format in variable definition");
		}
		std::vector<std::string> v = {"*", "/", "+", "-"};
		std::vector<std::string> items = {cmdInput.inputVec.begin()+2, cmdInput.inputVec.end()};
		for (size_t i = 0; i < items.size(); i++) {
			if ((i % 2 == 0 && std::find(v.begin(), v.end(), items[i]) != v.end()) ||
					(i % 2 != 0 && std::find(v.begin(), v.end(), items[i]) == v.end())) {
				return genError("wrong format in variable definition");
			}
		}
		// the operations are done in the usual order: multiplications and divisions first, and then additions and subtractions
		// each from left to right (e.g. 10 - 2 + 3 is 11, and 8 / 2 * 2 is 8)
		// every operation replaces its two operands and its symbol with its result
		for (const std::vector<std::string>& level : {std::vector<std::string>{"*", "/"}, std::vector<std::string>{"+", "-"}}) {
			size_t j = 1;
			while (j < items.size()) {
				if (std::find(level.begin(), level.end(), items[j]) == level.end()) {
					j += 2;
					continue;
				}
				int operationType = (int)std::distance(v.begin(), std::find(v.begin(), v.end(), items[j]));
				// the result is a pair of an error and a string result (for adding strings),
				// and a pair of the result type (int or float) and the result
				std::pair<std::pair<std::string, std::string>, std::pair<int, double>> p = getArithmeticResult(items[j-1], items[j+1], operationType);
				if (!p.first.first.empty()) {
					return genError(p.first.first);
				}
				items[j-1] = stripValFromResult(p);
				items.erase(items.begin() + j, items.begin() + j + 2);
			}
		}
		variable var = {items[0], 0};
		variables[varName] = var;
	}
	else {
		variable var = {cmdInput.inputVec[2], 0};
		variables[varName] = var;
	}
	commandsMap[livelily][plainVarName[0]][varName] = Colors::darkTurquoise;
	return CmdOutput();
}

//--------------------------------------------------------------
std::pair<int, std::string> ofApp::checkNameAvailability(std::string structType, std::string s)
{
	std::string name;
	if (variables.find(s) != variables.end()) {
		if (startsWith(variables[s].varStr, "\\")) {
			name = variables[s].varStr;
		}
		else {
			name = "\\" + variables[s].varStr;
		}
		variables[s].varType = structTypes[structType];
	}
	else {
		if (startsWith(s, "\\")) {
			return std::make_pair(3, "can't use command " + s + " as " + structType + " name");
		}
		else {
			name = "\\" + s;
		}
	}
	// a name is called with a backslash, so it can't be the name of a command of LiveLily (e.g. \loop merge would be called with \merge)
	if (isReservedName(name)) {
		return std::make_pair(3, name.substr(1) + " can't be a " + structType + " name, " + name + " is a command");
	}
	// nor the name of another kind (e.g. a list named like a bar)
	NameKind kind = (structType == "list" ? NameKind::list : (structType == "function" ? NameKind::function : NameKind::loop));
	std::string taken = nameTakenError(name, kind);
	if (!taken.empty()) return std::make_pair(3, taken);
	// bars and loops are the same kind of name (bars are stored as loops), but a bar can't have the name of a loop, nor a loop the name of a bar
	bool isBar = (sharedData.barsIndexes.find(name) != sharedData.barsIndexes.end());
	bool isLoop = (sharedData.loopsIndexes.find(name) != sharedData.loopsIndexes.end());
	if (structType == "bar" && isLoop && !isBar) {
		return std::make_pair(3, name.substr(1) + " can't be the name of a bar, " + name + " is a loop");
	}
	if (structType == "loop" && isBar) {
		return std::make_pair(3, name.substr(1) + " can't be the name of a loop, " + name + " is a bar");
	}
	return std::make_pair(0, name);
}

//--------------------------------------------------------------
CmdOutput ofApp::selectBarOfBarsCommand(CmdInput& cmdInput)
{
	// inside \bars, the lines are parsed once for every bar, and the values of each bar are separated by |
	// like with the notes of the instruments, e.g. "\time 4/4 | 3/4", "\tempo 4 = 60 | 4 = 90" or "\beatat 4 | 2"
	// this keeps the values of the bar being parsed (barsIterCounter), and the command name, in cmdInput
	// with one value (no |), the same value is used for all the bars
	// with more values, there must be as many as the bars defined by the instruments
	// | can be written with or without spaces around it, e.g. "\tempo 60|90"
	std::vector<std::vector<std::string>> barArgs(1);
	for (size_t i = 1; i < cmdInput.inputVec.size(); i++) {
		const std::string& token = cmdInput.inputVec[i];
		size_t start = 0;
		while (true) {
			size_t slash = token.find('|', start);
			std::string part = token.substr(start, (slash == std::string::npos ? std::string::npos : slash - start));
			if (!part.empty()) barArgs.back().push_back(part);
			if (slash == std::string::npos) break;
			barArgs.push_back(std::vector<std::string>());
			start = slash + 1;
		}
	}
	// the values may end with |, like the notes of the instruments
	if (barArgs.size() > 1 && barArgs.back().empty()) barArgs.pop_back();
	int numBars = (int)barArgs.size();
	for (int i = 0; i < numBars; i++) {
		if (barArgs[i].empty()) {
			barError = true;
			if (numBars == 1) return genError(cmdInput.inputVec[0] + " has no values");
			return genError(cmdInput.inputVec[0] + " has no values for bar " + std::to_string(i+1));
		}
	}
	if (numBars > 1) {
		// all commands with more than one value must define the same number of bars
		if (barsCmdNumBars > 0 && barsCmdNumBars != numBars) {
			barError = true;
			return genError(cmdInput.inputVec[0] + " defines " + std::to_string(numBars) + " bars, while " + barsCmdName + " defines " + std::to_string(barsCmdNumBars));
		}
		// and as many as the bars of the instruments, if an instrument has already been parsed in this iteration
		if (numBarsToParseSet && numBarsToParse != numBars) {
			barError = true;
			return genError(cmdInput.inputVec[0] + " defines " + std::to_string(numBars) + " bars, while first instrument parsed " + std::to_string(numBarsToParse));
		}
		barsCmdNumBars = numBars;
		barsCmdName = cmdInput.inputVec[0];
	}
	int barNdx = (numBars == 1 ? 0 : barsIterCounter - 1);
	if (barNdx < 0 || barNdx >= numBars) {
		barError = true;
		return genError(cmdInput.inputVec[0] + " defines " + std::to_string(numBars) + " bars, but bar " + std::to_string(barsIterCounter) + " is being parsed");
	}
	std::vector<std::string> args = {cmdInput.inputVec[0]};
	args.insert(args.end(), barArgs[barNdx].begin(), barArgs[barNdx].end());
	cmdInput.inputVec = args;
	return CmdOutput{0, "", 0, std::vector<std::string>()};
}

//--------------------------------------------------------------
std::pair<int, int> ofApp::tupletRatio(const std::string& str)
{
	// the ratio of a tuplet, e.g. 3/2, or {0, 0} if it is not two numbers greater than zero
	size_t slash = str.find('/');
	if (slash == std::string::npos) return std::make_pair(0, 0);
	std::string numerator = str.substr(0, slash);
	std::string denominator = str.substr(slash+1);
	if (!isNumber(numerator) || !isNumber(denominator) || numerator.size() > 4 || denominator.size() > 4) return std::make_pair(0, 0);
	int num = stoi(numerator);
	int den = stoi(denominator);
	if (num <= 0 || den <= 0) return std::make_pair(0, 0);
	return std::make_pair(num, den);
}

//--------------------------------------------------------------
std::pair<bool, int> ofApp::ottavaValue(const std::string& str)
{
	// the value of an ottava, from -2 to 2, and whether it is valid
	if (str.empty() || str.size() > 2) return std::make_pair(false, 0);
	if (str.size() == 2 && (str[0] != '-' || !isdigit(str[1]))) return std::make_pair(false, 0);
	if (str.size() == 1 && !isdigit(str[0])) return std::make_pair(false, 0);
	int value = stoi(str);
	if (value < -2 || value > 2) return std::make_pair(false, 0);
	return std::make_pair(true, value);
}

// expands the repetitions of tokens (e.g. c4*3) and groups (e.g. [c d e]*2) as detectRepetitions() does
// and returns the indexes of the tokens in the order they are played
static void expandRepetitions(const std::vector<PositionedToken>& tokens, size_t& ndx, bool stopAtClosingBracket, std::vector<int>& out)
{
	while (ndx < tokens.size()) {
		const std::string& t = tokens[ndx].text;
		if (t == "]") {
			if (stopAtClosingBracket) return;
			ndx++;
			continue;
		}
		if (t == "[") {
			ndx++;
			std::vector<int> contents;
			expandRepetitions(tokens, ndx, true, contents);
			if (ndx < tokens.size() && tokens[ndx].text == "]") ndx++;
			// the multiplier after the closing bracket, e.g. ]*2, or ] * 2
			int repetitions = 1;
			if (ndx < tokens.size() && !tokens[ndx].text.empty() && tokens[ndx].text[0] == '*') {
				std::string digits = tokens[ndx].text.substr(1);
				ndx++;
				if (digits.empty() && ndx < tokens.size()) digits = tokens[ndx++].text;
				if (!digits.empty() && digits.find_first_not_of("0123456789") == std::string::npos) repetitions = std::stoi(digits);
			}
			for (int r = 0; r < repetitions; r++) out.insert(out.end(), contents.begin(), contents.end());
			continue;
		}
		int repetitions = 1;
		size_t star = t.find_last_of('*');
		if (star != std::string::npos && star + 1 < t.size() && t.substr(star + 1).find_first_not_of("0123456789") == std::string::npos) {
			repetitions = std::stoi(t.substr(star + 1));
		}
		for (int r = 0; r < repetitions; r++) out.push_back((int)ndx);
		ndx++;
	}
}

//--------------------------------------------------------------
void ofApp::storeEditorNoteSpans(int lineNum, int instNdx, int barNdx, int numNotes)
{
	// stores which token of the line of the editor every note of an instrument in a bar comes from, so that the token can be
	// highlighted while its note plays (see \editor.animate)
	// a note of a bar that copies another bar (e.g. \1 or \1.\inst) comes from the copying token and from the note of the copied bar
	// if the notes can't be matched to the tokens of the line (e.g. a line in a function), nothing is stored for the bar
	std::vector<std::vector<EditorNoteSpan>>& spans = editorNoteSpans[std::make_pair(instNdx, barNdx)];
	spans.clear();
	auto editorIt = editors.find(whichPane);
	if (editorIt == editors.end() || editorIt->second.allLines.find(lineNum) == editorIt->second.allLines.end()) return;
	const std::string& str = editorIt->second.allLines[lineNum].str;
	std::vector<PositionedToken> tokens = positionedTokens(str);
	// the melodic line starts after the name of the instrument, which can be at the start of the line or after \bar (e.g. \bar 1 { \inst c'1 })
	std::string instName = "\\" + sharedData.instruments[instNdx].getName();
	size_t first = 0;
	while (first < tokens.size() && tokens[first].text != instName && tokens[first].text.substr(0, instName.size() + 1) != instName + ".") first++;
	if (first >= tokens.size()) return;
	first++;
	// the melodic line ends at the end of the line, or at a closing bracket that doesn't belong to a tuplet (e.g. the end of a bar in one line)
	// in a bar written in one line, it also ends where the next statement starts, at the name of another instrument or group, or a command of the bar
	auto startsStatement = [&](const std::string& word) {
		if (word == "\\time" || word == "\\tempo" || word == "\\beatat") return true;
		if (word.size() < 2 || word[0] != '\\') return false;
		std::string name = word.substr(1, word.find('.') == std::string::npos ? std::string::npos : word.find('.') - 1);
		return sharedData.instrumentIndexes.find("\\" + name) != sharedData.instrumentIndexes.end() || instGroups.find("\\" + name) != instGroups.end();
	};
	size_t last = first;
	int braceDepth = 0;
	while (last < tokens.size()) {
		if (tokens[last].text == "{") braceDepth++;
		else if (tokens[last].text == "}") {
			if (braceDepth == 0) break;
			braceDepth--;
		}
		else if (braceDepth == 0 && startsStatement(tokens[last].text)) break;
		last++;
	}
	std::vector<PositionedToken> melodic(tokens.begin() + first, tokens.begin() + last);
	// with \bars, the bars are separated by |, and the notes are those of the bar being parsed
	if (parsingBars) {
		std::vector<PositionedToken> segment;
		int barCounter = 1;
		for (const PositionedToken& t : melodic) {
			if (t.text == "|") barCounter++;
			else if (barCounter == barsIterCounter) segment.push_back(t);
		}
		melodic = segment;
	}
	// commands with arguments in curly brackets make notes that are not written as they play (e.g. \euclide {16 5 4 c' cis'} or \transpose { c d e 2 })
	// so their arguments are not notes, and while their notes play, the command is highlighted as a whole
	// \euclide (and other such commands) from its backslash to its closing bracket, and \transpose with the contents of its curly brackets
	// \transpose without curly brackets has its arguments up to the number of semitones (e.g. \transpose \1 2)
	std::vector<int> groupEnd(melodic.size(), -1); // the index of the last token of a command with curly brackets
	std::vector<int> groupStart(melodic.size(), -1); // the index of the first token that is highlighted for it
	std::vector<bool> insideGroup(melodic.size(), false);
	std::vector<bool> skipToken(melodic.size(), false); // \transpose with notes only, and its number of semitones
	for (size_t i = 0; i < melodic.size(); i++) {
		const std::string& t = melodic[i].text;
		if (t.empty() || t[0] != '\\' || insideGroup[i] || t == "\\tuplet" || t == "\\tup") continue;
		bool isTranspose = (t == "\\transpose");
		size_t end = melodic.size();
		// a command that takes a name can have it before its curly brackets (e.g. \apply \1.\inst {c' e'}, see CommandSpec)
		size_t open = i + 1;
		if (i + 2 < melodic.size() && melodic[i+1].text != "{" && melodic[i+2].text == "{") open = i + 2;
		if (open < melodic.size() && melodic[open].text == "{") {
			int depth = 0;
			for (size_t j = open; j < melodic.size(); j++) {
				if (melodic[j].text == "{") depth++;
				else if (melodic[j].text == "}" && --depth == 0) {
					end = j;
					break;
				}
			}
		}
		else if (isTranspose) {
			for (size_t j = i + 1; j < melodic.size(); j++) {
				if (isFloat(melodic[j].text)) {
					end = j;
					break;
				}
			}
		}
		if (end >= melodic.size()) continue;
		if (isTranspose) {
			// with notes only (e.g. \transpose { c4 r e r -12 }), its notes and rests are highlighted as in any melodic line
			// and only \transpose and its number of semitones are not (with a bar name in it, it is highlighted as a whole)
			bool hasCommand = false;
			for (size_t j = i + 1; j <= end; j++) {
				if (!melodic[j].text.empty() && melodic[j].text[0] == '\\') hasCommand = true;
			}
			if (!hasCommand) {
				size_t amountNdx = (melodic[end].text == "}" ? end - 1 : end);
				skipToken[i] = true;
				if (amountNdx > i && isFloat(melodic[amountNdx].text)) skipToken[amountNdx] = true;
				continue;
			}
		}
		groupEnd[i] = (int)end;
		groupStart[i] = (int)(isTranspose ? i + 1 : i);
		for (size_t j = i + 1; j <= end; j++) insideGroup[j] = true;
	}
	std::vector<int> order;
	size_t ndx = 0;
	expandRepetitions(melodic, ndx, false, order);
	int pane = whichPane;
	int lineId = editorIt->second.getLineId(lineNum);
	auto spanOf = [&](int tokenNdx) {
		if (groupEnd[tokenNdx] >= 0) {
			// a command with curly brackets, from the first to the last of its highlighted tokens (see groupStart above)
			int start = melodic[groupStart[tokenNdx]].start;
			int length = melodic[groupEnd[tokenNdx]].start + (int)melodic[groupEnd[tokenNdx]].text.size() - start;
			return EditorNoteSpan{pane, lineId, start, length, str.substr(start, length)};
		}
		return EditorNoteSpan{pane, lineId, melodic[tokenNdx].start, (int)melodic[tokenNdx].text.size(), melodic[tokenNdx].text};
	};
	auto spansOf = [&](int tokenNdx) { return std::vector<EditorNoteSpan>{spanOf(tokenNdx)}; };
	// every token gives the notes it plays, each with its tokens
	// a command that is not known here (e.g. a variable or a function) gives the notes that are left, if it is the only one
	std::vector<std::vector<EditorNoteSpan>> result;
	int unknownPosition = -1; // where in result the notes of the unknown command go
	int unknownToken = -1;
	for (size_t k = 0; k < order.size(); k++) {
		const std::string& t = melodic[order[k]].text;
		if (t == "{" || t == "}" || t == "|" || t == "[" || t == "]") continue;
		// the arguments of a command with curly brackets (see groupEnd above)
		if (insideGroup[order[k]] || skipToken[order[k]]) continue;
		// commands that don't play notes, and their argument
		if (t == "\\ottava" || t == "\\ott" || t == "\\clef" || t == "\\tuplet" || t == "\\tup") {
			k++;
			continue;
		}
		if (t.empty() || t[0] != '\\') {
			// a note, a rest, or a chord, or the steps of the x - notation (e.g. x-x-), each with its own characters of the token
			std::vector<Step> steps = stepTokens(t);
			if (steps.size() > 1) {
				for (const Step& step : steps) {
					int start = melodic[order[k]].start + (int)step.start;
					result.push_back({EditorNoteSpan{pane, lineId, start, (int)step.length, t.substr(step.start, step.length)}});
				}
			}
			else {
				result.push_back(spansOf(order[k]));
			}
			continue;
		}
		// a bar that is copied, with the instrument of this line or another one, e.g. \1 or \1.\inst
		size_t dot = t.find('.');
		std::string barName = t.substr(0, dot);
		auto barIt = sharedData.barsIndexes.find(barName);
		if (barIt != sharedData.barsIndexes.end()) {
			int sourceInst = instNdx;
			if (dot != std::string::npos) {
				std::string sourceName = t.substr(dot + 1);
				if (!startsWith(sourceName, "\\")) sourceName = "\\" + sourceName;
				auto instIt = sharedData.instrumentIndexes.find(sourceName);
				if (instIt == sharedData.instrumentIndexes.end()) return;
				sourceInst = instIt->second;
			}
			auto sourceIt = editorNoteSpans.find(std::make_pair(sourceInst, barIt->second));
			int numSourceNotes = (int)sharedData.instruments[sourceInst].notes[barIt->second].size();
			for (int n = 0; n < numSourceNotes; n++) {
				std::vector<EditorNoteSpan> noteSpans = spansOf(order[k]);
				if (sourceIt != editorNoteSpans.end() && n < (int)sourceIt->second.size()) {
					noteSpans.insert(noteSpans.end(), sourceIt->second[n].begin(), sourceIt->second[n].end());
				}
				result.push_back(noteSpans);
			}
			continue;
		}
		// any other command
		if (unknownToken >= 0) return; // more than one, the notes can't be matched
		unknownToken = order[k];
		unknownPosition = (int)result.size();
	}
	if (unknownToken >= 0) {
		int remaining = numNotes - (int)result.size();
		if (remaining < 0) return;
		result.insert(result.begin() + unknownPosition, remaining, std::vector<EditorNoteSpan>{spanOf(unknownToken)});
	}
	if ((int)result.size() != numNotes) return;
	spans = result;
}

//--------------------------------------------------------------
std::pair<int, std::string> ofApp::parseMelodicLine(std::vector<std::string> tokens, int lineNum, int numLines)
{
	// a warning of a command in the melodic line (e.g. \euclide), which is returned if the line has no errors (see isInstrument())
	melodicLineWarning.clear();
	if (barError) {
		// if a barError occurs, it concerns the line where the bar name is defined
		// in this case, we don't proceed to parse the melodic lines defined in the bar
		// but we also don't raise an error here, as this has been raised in parseCommand()
		return std::make_pair(0, "");
	}
	if (!parsingBar) {
		return std::make_pair(3, "stray melodic line");
	}
	if (!areBracketsBalanced(tokens)) {
		return std::make_pair(3, "brackets are not balanced");
	}
	tokens = detectRepetitions(tokens);
	if (tokens.empty()) {
		return std::make_pair(3, "no notes in the melodic line");
	}
	if (parsingBars) {
		// first check if the last token is an upright slash, and remove it
		if (tokens.back() == "|") tokens.pop_back();
		if (startsWith(tokens[0], "\\")) {
			// first determine whether there is an instrument name as a method
			size_t dotNdx = tokens[0].find_last_of(".");
			bool foundDot = (dotNdx != std::string::npos ? true : false);
			std::string barName;
			if (foundDot) barName = tokens[0].substr(0, dotNdx);
			else barName = tokens[0];
			// determine if this is a single bar or a loop
			// if it is a loop, replace the tokens vector with the output of evaluateTokens()
			if (sharedData.loopsIndexes.find(barName) != sharedData.loopsIndexes.end() && sharedData.barsIndexes.find(barName) == sharedData.barsIndexes.end()) {
				CmdInput cmdInput = {tokens, std::vector<std::string>(), false, false};
				CmdOutput cmdOutput = parseCommand(cmdInput, lineNum, numLines);
				if (cmdOutput.errorCode == 3) return std::make_pair(3, cmdOutput.errorStr);
				tokens.clear();
				tokens = cmdOutput.outputVec;
			}
		}
		if (lastInstrumentIndex == firstInstForBarsIndex && !numBarsToParseSet) {
			// if this is the first instrument, determine the number of bars to compare to other instruments
			numBarsToParse = 0;
			for (std::string s : tokens) if (s == "|") numBarsToParse++;
			numBarsToParse++;
			// check in case the user ends bars definition with |
			if (tokens.back() == "|") numBarsToParse--;
			numBarsToParseSet = true;
			// a \time, \tempo or \beatat with more than one value, written before this line, must define as many bars
			if (barsCmdNumBars > 0 && barsCmdNumBars != numBarsToParse) {
				return std::make_pair(3, barsCmdName + " defines " + std::to_string(barsCmdNumBars) + " bars, while first instrument parsed " + std::to_string(numBarsToParse));
			}
		}
		int numBarsToParseLocal = 0;
		for (std::string s : tokens) if (s == "|") numBarsToParseLocal++;
		numBarsToParseLocal++; // have to double check in case the user ends bars definition with |
		if (lastInstrumentIndex != firstInstForBarsIndex && numBarsToParseLocal != numBarsToParse) {
			return std::make_pair(3, (std::string)"parsed " + std::to_string(numBarsToParseLocal) + (std::string)" bars, while first instrument parsed " + std::to_string(numBarsToParse));
		}
		// the int below is compared to barsIterCounter which increments to 1 as soon as we start parsing many bars
		int verticalBarsCounter = 1;
		std::vector<std::string> newTokens;
		for (std::string s : tokens) {
			if (s == "|") verticalBarsCounter++;
			if (verticalBarsCounter == barsIterCounter && s != "|") {
				newTokens.push_back(s);
			}
			if (verticalBarsCounter > barsIterCounter) {
				// reset verticalBarsCounter so that the test agaist numBarsToParse below works
				// otherwise it will be true after the first iteration
				verticalBarsCounter = barsIterCounter;
				break;
			}
		}
		if (verticalBarsCounter == numBarsToParse) sharedData.instruments[lastInstrumentIndex].setMultiBarsDone(true);
		tokens.clear();
		tokens = newTokens;
	}
	// if we are parsing many bars and in one of those we define an earlier bar from the same bars definition
	// we need to isolate the bar number from the loop name, which is loopname-barnumber (e.g. \1-4-1 for the first bar from loop \1-4)
	// so we can use it further down in the else if (tokens[0] == "\\bars") test
	int verticalBarsNdx = 1;
	// as we might ask for a previous bar of the same bars definition but from another instrument
	// we need to detect if this is the case and store the name of the desired instrument
	// the default instrument name though is the one we're parsing the current line for
	std::string instFromBars = "\\" + sharedData.instruments[lastInstrumentIndex].getName();
	if (parsingBars && startsWith(tokens[0], "\\")) {
		size_t lastHyphenNdx = tokens[0].find_last_of("-");
		size_t dotNdx = tokens[0].find_last_of(".");
		bool foundDot = (dotNdx != std::string::npos ? true : false);
		size_t endOfSubstr = (foundDot ? dotNdx : tokens[0].size());
		std::string barName;
		if (foundDot) barName = tokens[0].substr(0, dotNdx);
		else barName = tokens[0];
		if (sharedData.barsIndexes.find(barName) != sharedData.barsIndexes.end()) {
			if (lastHyphenNdx != std::string::npos && isNumber(tokens[0].substr(lastHyphenNdx+1, endOfSubstr-lastHyphenNdx-1))) {
				verticalBarsNdx = stoi(tokens[0].substr(lastHyphenNdx+1, endOfSubstr-lastHyphenNdx-1));
			}
			if (foundDot) {
				if (sharedData.instrumentIndexes.find(tokens[0].substr(dotNdx+1)) != sharedData.instrumentIndexes.end()) {
					instFromBars = tokens[0].substr(dotNdx+1);
				}
			}
		}
	}
	// before we parse the input vector to detect commands, we store it as a string
	// which will be the string of the currect bar
	std::string barLine = genStrFromVec(tokens);
	// the boolean below is used to determine if we need to update the barLine string
	bool barLineChanged = false;
	// parse the string to detect any containing commands like \ottava, \tuplet, \cluster, or a bar or loop name
	// first insert the instrument name as this has been striped
	std::vector<std::string> v = {"\\" + sharedData.instruments[lastInstrumentIndex].getName()};
	// then append the rest of the input vector
	v.insert(v.end(), tokens.begin(), tokens.end());
	CmdOutput cmdOutput = evaluateTokens(v, lineNum, numLines);
	if (cmdOutput.errorCode == 2 && melodicLineWarning.empty()) melodicLineWarning = cmdOutput.errorStr;
	// check if the input vector is different than the output of evaluateTokens()
	// and if it is, set barLineChanged to true
	// this might happen if instead of notes we pass a previous bar for this instrument or another one
	if (tokens != cmdOutput.outputVec) barLineChanged = true;
	if (cmdOutput.errorCode == 3) return std::make_pair(3, cmdOutput.errorStr);
	tokens.clear();
	if (cmdOutput.outputVec.size() > 1) tokens = {cmdOutput.outputVec.begin()+1, cmdOutput.outputVec.end()};
	// a line with an instrument and maybe a clef, but no notes (e.g. "\\inst \\clef bass")
	if (tokens.empty()) {
		return std::make_pair(3, "no notes in the melodic line");
	}
	if (tokens[0] == "\\bars") {
		// the algorithm of this if test is similar to the if (parsingBars) above
		// but here we need a boolean too because we get the entire bars definition strings e.g.
		// \bars 1-4 {
		//     \inst1 c''2 d'' | e''2 f'' | \1-4-1 | g''1
		//     \inst2 c'2 d' | e'2 f' | g'1 | \1-4-2
		// }
		// so we need to determine where to start pushing back to the newTokens vector
		int verticalBarsCounter = 1;
		bool startInserting = false;
		std::vector<std::string> newTokens;
		for (std::string s : tokens) {
			if (s == "|" && startInserting) {
				verticalBarsCounter++;
			}
			if (verticalBarsCounter == verticalBarsNdx && s != "|" && startInserting) {
				newTokens.push_back(s);
			}
			if (instFromBars == s) {
				newTokens.push_back(s);
				startInserting = true;
			}
			if (verticalBarsCounter > verticalBarsNdx) {
				break;
			}
		}
		CmdOutput cmdOutput = evaluateTokens(newTokens, lineNum, numLines);
		if (cmdOutput.errorCode == 3) return std::make_pair(3, cmdOutput.errorStr);
		if (cmdOutput.errorCode == 2 && melodicLineWarning.empty()) melodicLineWarning = cmdOutput.errorStr;
		barLine = genStrFromVec(newTokens);
		tokens.clear();
		if (cmdOutput.outputVec.size() < 2) {
			return std::make_pair(3, "no notes in this bar of the bars for \\" + sharedData.instruments[lastInstrumentIndex].getName());
		}
		// check for possible newline characters that might be inserted when inputing a loop name in a bars definition
		for (auto it = cmdOutput.outputVec.begin()+1; it != cmdOutput.outputVec.end(); ++it) {
			size_t newlineNdx = (*it).find('\n');
			if (newlineNdx != std::string::npos) tokens.push_back((*it).substr(0, newlineNdx));
			else tokens.push_back(*it);
		}
		//barLineChanged = true;
	}
	// if we provide a bar name, the tokens vector will contain the bar lines
	// so below we check if the output starts with \bar
	if (tokens[0] == "\\bar") {
		CmdOutput cmdOutput = stripLineFromBar(tokens, lineNum, numLines);
		if (cmdOutput.errorCode == 3) return std::make_pair(3, cmdOutput.errorStr);
		// the copied bar has no line for this instrument, which is always the case for the bar that is being defined
		// as its lines are stored only once it has been parsed
		if (cmdOutput.outputVec.size() < 2) {
			std::string copiedBar = (tokens.size() > 1 ? "\\" + tokens[1] : std::string("the bar"));
			std::string instName = "\\" + sharedData.instruments[lastInstrumentIndex].getName();
			if (copiedBar == sharedData.barsOrdered[getLastLoopIndex()]) {
				return std::make_pair(3, "a bar can't copy itself, " + copiedBar + " is the bar that is being defined");
			}
			// an instrument that is not in a bar (e.g. added with \insts.add after the bar was defined) has a rest for the whole bar
			// (see fillInMissingInsts() and addedInstruments()), so copying that bar gives a whole bar rest
			if (sharedData.barsIndexes.find(copiedBar) != sharedData.barsIndexes.end()) {
				cmdOutput.outputVec = {instName, "r1"};
			}
			else {
				return std::make_pair(3, copiedBar + " has no line for " + instName);
			}
		}
		tokens.clear();
		// the first item of the CmdOutput structure returned by evaluateTokens()
		// is the name of the instrument the line of which we're extracting here
		// so we discard it and keep the rest of the information
		tokens = {cmdOutput.outputVec.begin()+1, cmdOutput.outputVec.end()};
		barLineChanged = true;
	}
	// now we'll store the parsed tokens as a string to the sharedData.barLines map
	// the line is supposed to start with a horizontal tab, so we store that first
	// the bar index has already been stored with the \bar command
	// so we can retrieve it with geLastLoopIndex()
	int barIndex = getLastLoopIndex();
	std::string barName = sharedData.barsOrdered[barIndex];
	sharedData.barLines[barName] += '\t';
	sharedData.barLines[barName] += '\\' + sharedData.instruments[lastInstrumentIndex].getName() + ' ';
	if (barLineChanged) {
		sharedData.barLines[barName] += genStrFromVec(tokens);
	}
	else {
		sharedData.barLines[barName] += barLine;
	}
	sharedData.barLines[barName] += '\n';

	// now that the tokens have been expanded, we can save it, in case this instrument sends to an OSC server
	if (instrumentOSCHostPorts.find(lastInstrumentIndex) != instrumentOSCHostPorts.end()) {
		std::string forOSCPart = "\\" + sharedData.instruments[lastInstrumentIndex].getName() + " " + genStrFromVec(tokens);
		sharedData.instruments[lastInstrumentIndex].barLines[barIndex] = forOSCPart;
	}

	// the x - notation of rhythm staffs (e.g. x - x x-x-), where every x is a hit and every - a rest, each one step of \score.seqres long
	// is translated to durations (e.g. 16 r16 16 16 r16 16 r16 with 16ths), with what follows every x (e.g. x-> is 16->)
	// the bar keeps the notation as it is written (see barLines above), so that a copy of the bar in another rhythm staff is translated there
	{
		std::vector<std::string> translated;
		for (const std::string& token : tokens) {
			std::vector<Step> steps = stepTokens(token);
			if (steps.empty()) {
				translated.push_back(token);
				continue;
			}
			if (!sharedData.instruments.at(lastInstrumentIndex).isRhythm()) {
				return std::make_pair(3, token + ": the x - notation is only for rhythm staffs (e.g. \\" + sharedData.instruments.at(lastInstrumentIndex).getName() + " rhythm)");
			}
			std::string stepDur = std::to_string(seqResolution);
			for (const Step& step : steps) translated.push_back(step.hit ? stepDur + step.suffix : "r" + stepDur);
		}
		tokens = translated;
	}
	// \ottava and \tuplet stay in the melodic line as commands (e.g. \ottava 1 c'4 or \tuplet 3/2 { c8 d e })
	// here they are taken out of the tokens, and stored with the notes they apply to
	// so that the rest of this function parses notes only
	// an ottava applies to all the notes after it, until the next \ottava
	// tuplets can be nested to any depth, e.g. \tuplet 3/2 { c8 \tuplet 5/4 { d32 e f g a } b8 }
	std::vector<std::string> noteTokens;
	std::vector<int> noteOttavas;
	// the ratio, the first and the last note of every tuplet, in the order they start
	std::vector<std::pair<int, int>> tupletRatiosLocal;
	std::vector<std::pair<int, int>> tupletRanges;
	{
		std::vector<size_t> openTuplets;
		int currentOttava = 0;
		for (size_t t = 0; t < tokens.size(); t++) {
			const std::string& token = tokens[t];
			if (token == "\\ottava" || token == "\\ott") {
				if (t + 1 >= tokens.size()) return std::make_pair(3, "no argument to \\ottava");
				std::pair<bool, int> ottava = ottavaValue(tokens[t+1]);
				if (!ottava.first) return std::make_pair(3, "argument to \\ottava must be a number between -2 and 2");
				currentOttava = ottava.second;
				t++;
				continue;
			}
			if (token == "\\tuplet" || token == "\\tup") {
				if (t + 2 >= tokens.size() || tokens[t+2] != "{") return std::make_pair(3, "badly formatted tuplet, the notes of a tuplet must be inside curly brackets");
				std::pair<int, int> ratio = tupletRatio(tokens[t+1]);
				if (ratio.first == 0) return std::make_pair(3, "badly formatted tuplet ratio, it must be two numbers greater than zero, e.g. 3/2");
				openTuplets.push_back(tupletRatiosLocal.size());
				tupletRatiosLocal.push_back(ratio);
				tupletRanges.push_back(std::make_pair((int)noteTokens.size(), -1));
				t += 2;
				continue;
			}
			if (token == "{") return std::make_pair(3, "curly bracket without a tuplet");
			if (token == "}") {
				if (openTuplets.empty()) return std::make_pair(3, "closing curly bracket without a tuplet");
				size_t tuplet = openTuplets.back();
				openTuplets.pop_back();
				tupletRanges[tuplet].second = (int)noteTokens.size() - 1;
				if (tupletRanges[tuplet].second < tupletRanges[tuplet].first) return std::make_pair(3, "empty tuplet");
				continue;
			}
			noteTokens.push_back(token);
			noteOttavas.push_back(currentOttava);
		}
		if (!openTuplets.empty()) return std::make_pair(3, "tuplet without a closing curly bracket");
		if (noteTokens.empty()) return std::make_pair(3, "no notes in the melodic line");
	}
	tokens = noteTokens;

	// once the line has been parsed to expand commands and stored as a string we can move on and create the melodic line
	// the following array is used to std::map the dynamic values
	// to the corresponding MIDI velocity values
	int dynsArr[8] = {-9, -6, -3, -1, 1, 3, 6, 9};
	// an array to test against durations
	std::vector<int> dursArr{1, 2, 4, 8, 16, 32, 64};
	// the notes of the natural scale in MIDI values
	int midiNotes[7] = {0, 2, 4, 5, 7, 9, 11};
	bool dynamicsRampStarted = false;
	// booleans to determine whether we're writing a chord or not
	//bool chordStarted = false;
	bool firstChordNote = false;

	bool foundArticulation = false;
	// boolean for dynamics, whether it's a mezzo forte or mezzo piano
	bool mezzo = false;
	// index variables for loop so that they are initialized before std::vectors with unknown size
	unsigned i = 0, j = 0, k = 0; //  initialize to avoid compiler warnings
	// then detect any chords, as the size of most vectors equals the number of notes vertically
	// and not single notes within chords
	unsigned numNotesVertical = tokens.size();

	std::map<int, std::pair<int, int>> tupletRatios;
	std::map<int, std::pair<unsigned, unsigned>> tupletStartStop;

	// once we define the number of tokens and vertical notes, we can create the std::vectors with known size
	std::vector<int> durIndexes(tokens.size(), -1);
	std::vector<int> dynamicsIndexes(tokens.size(), -1);
	std::vector<int> midiVels(tokens.size(), 0);
	std::vector<int> midiPitchBendVals(tokens.size(), 0);
	std::vector<int> dursData(tokens.size(), 0);
	std::vector<int> dursDataWithoutSlurs(tokens.size(), 0);
	std::vector<int> midiDursDataWithoutSlurs(tokens.size(), 0);
	// pitch bend values are calculated by the sequencer
	// so here we store the microtones only
	std::vector<int> pitchBendVals(tokens.size(), 0);
	std::vector<int> dotIndexes(tokens.size(), 0);
	std::vector<unsigned> dotsCounter(tokens.size(), 0);
	std::vector<float> dynamicsData(tokens.size(), 0);
	std::vector<float> dynamicsRampData(tokens.size(), 0);
	std::vector<int> slurBeginningsIndexes(tokens.size(), -1);
	std::vector<int> slurEndingsIndexes(tokens.size(), -1);
	std::vector<int> tieIndexes(tokens.size(), -1);
	//std::vector<int> textIndexesLocal(tokens.size(), 0);
	std::vector<int> dursForScore(tokens.size(), 0);
	std::vector<int> dynamicsForScore(tokens.size(), -1);
	std::vector<int> dynamicsForScoreIndexes(tokens.size(), -1);
	std::vector<int> dynamicsRampStart(tokens.size(), 0);
	std::vector<int> dynamicsRampEnd(tokens.size(), 0);
	std::vector<pair<unsigned, unsigned>> dynamicsRampIndexes(tokens.size());
	std::vector<int> midiDynamicsRampDurs(tokens.size(), 0);
	std::vector<int> glissandiIndexes(tokens.size(), 0);
	std::vector<uint64_t> glissDursMs(tokens.size(), 0);
	std::vector<int> glissandiIndexesForScore(tokens.size(), 0);
	std::vector<int> midiGlissDurs(tokens.size(), 0);
	std::vector<int> dynamicsRampStartForScore(tokens.size(), -1);
	std::vector<int> dynamicsRampEndForScore(tokens.size(), -1);
	std::vector<int> dynamicsRampDirForScore(tokens.size(), -1);
	std::vector<bool> isSlurred (tokens.size(), false);
	std::vector<int> notesCounter(tokens.size(), 0);
	std::vector<int> verticalNotesIndexes(tokens.size()+1, 0); // plus one to easily break out of loops in case of chords
	std::vector<int> beginningOfChords(tokens.size(), 0);
	std::vector<int> endingOfChords(tokens.size(), 0);
	std::vector<unsigned> accidentalIndexes(tokens.size());
	std::vector<unsigned> chordNotesIndexes(tokens.size(), 0);
	std::vector<int> ottavas(tokens.size(), 0);
	std::vector<bool> foundNotes(tokens.size());
	std::vector<bool> isCluster(tokens.size(), false);
	std::vector<int> tempDurs(tokens.size(), 0);
	std::vector<bool> tempDurStored(tokens.size(), false);
	// a duration without a note (e.g. 4 in c'4~ 4) repeats the previous note, with its octave and accidental (see below, after the octaves)
	std::vector<bool> repeatsPrevNote(tokens.size(), false);
	std::vector<bool> isRest(tokens.size(), false); // needed to store durations of rests for rhythm instruments
	size_t transposedVecSize = 0;
	for (i = 0; i < tokens.size(); i++) {
		std::vector<std::string> subtokens = tokenizeChord(tokens[i]);
		transposedVecSize += subtokens.size();
	}
	std::vector<int> transposedOctaves(transposedVecSize, 0);
	std::vector<int> transposedAccidentals(transposedVecSize, 0);
	// variable to determine which character we start looking at
	// useful in case we start a bar with a chord
	std::vector<unsigned> firstChar(tokens.size(), 0);
	std::vector<unsigned> firstCharOffset(tokens.size(), 1); // this is zeroed in case of a rhythm staff with durations only
	int lastMidiNote = -1, lastNaturalScaleNote = -1; // these are used for tied notes
	// boolean used to set the values to isSlurred std::vector
	bool slurStarted = false;
	bool isLastNoteTied = false;
	bool dynAtFirstNote = false;
	// a counter for the number of notes in each chord
	unsigned index1 = 0, index2 = 0; // variables to index various data
	// create an unpopullated vector of vector of pairs of the notes as MIDI and natural scale
	// after all vectors with known size and all single variables have been created
	std::vector<std::vector<std::pair<int, int>>> notePairs;
	// then iterate over all the tokens and parse them to store the notes
	for (i = 0; i < tokens.size(); i++) {
		firstChar.at(i) = 0;
		// first check if the token is a comment so we exit the loop
		if (startsWith(tokens.at(i), "%")) continue;
		// further tokenize the token to detect chords
		std::vector<std::string> subtokens = tokenizeChord(tokens.at(i));
		foundNotes.at(i) = false;
		transposedOctaves.at(index1) = 0;
		transposedAccidentals.at(index1) = 0;
		// the first element of each token is the note
		// so we first check for it in a separate loop
		// and then we check for the rest of the stuff
		for (j = 0; j < subtokens.size(); j++) {
			int subtokenNdx = 0;
			if (j == 0 && subtokens.at(j).at(0) == '\\' && subtokens.at(j).size() > 1 && subtokens.at(j).at(1) == 'c') {
				// store the state of the chord being a cluster for now
				// when we calculate the accidentals and octaves for the current chord, we'll insert the intermediate notes too
				isCluster.at(i) = true;
				subtokenNdx = 2;
			}
			int midiNote = -1, naturalScaleNote = -1; // the second is for the score
			for (k = 0; k < 8; k++) {
				if (subtokens.at(j).at(subtokenNdx) == noteChars[k]) {
					midiNote = -1;
					if (k < 7) {
						midiNote = midiNotes[k];
						// we start one octave below the middle C
						midiNote += 48;
						naturalScaleNote = k;
						if (sharedData.instruments.at(lastInstrumentIndex).isRhythm()) {
							if (j > 0) return std::make_pair(3, "chords are not allowed in rhythm staffs");
							midiNote = 59;
							naturalScaleNote = 6;
						}
						//else if (sharedData.instruments.at(lastInstrumentIndex).getTransposition() != 0) {
						//	int transposedMidiNote = midiNotes[k] + sharedData.instruments.at(lastInstrumentIndex).getTransposition();
						//	// testing against the midiNote it is easier to determine whether we need to add an accidental
						//	if (transposedMidiNote < 0) {
						//		transposedMidiNote = 12 + midiNotes[k] + sharedData.instruments.at(lastInstrumentIndex).getTransposition();
						//	}
						//	else if (transposedMidiNote > 11) {
						//		transposedMidiNote %= 12;
						//	}
						//	if (midiNotes[k] < 4) {
						//		transposedOctaves.at(index1) -= 1;
						//	}
						//	if (transposedMidiNote > 4 && (transposedMidiNote % 2) == 0) {
						//		transposedAccidentals.at(index1) = 2;
						//	}
						//	else if (transposedMidiNote <= 4 && (transposedMidiNote % 2) == 1) {
						//		transposedAccidentals.at(index1) = 2;
						//	}
						//	naturalScaleNote = distance(midiNotes, find(begin(midiNotes), end(midiNotes), (transposedMidiNote-(transposedAccidentals.at(index1)/2))));
						//}
					}
					else {
						// the last element of the noteChars array is the rest
						if (j > 0) return std::make_pair(3, "rests can't be included in chords");
						midiNote = naturalScaleNote = -1;
						if (sharedData.instruments.at(lastInstrumentIndex).isRhythm()) {
							isRest.at(i) = true;
						}
					}
				storeNote:
					lastMidiNote = midiNote;
					lastNaturalScaleNote = naturalScaleNote;
					if (j == 0) {
						// create a new std::vector for each single note or a group of notes of a chord
						std::pair p = std::make_pair(midiNote, naturalScaleNote);
						std::vector<std::pair<int, int>> aVector(1, p);
						notePairs.push_back(std::move(aVector));
						chordNotesIndexes.at(i) = 0;
						notesCounter.at(i)++;
						dotIndexes.at(i) = 0;
						glissandiIndexes.at(i) = 0;
						glissandiIndexesForScore.at(i) = 0;
						midiGlissDurs.at(i) = 0;
						midiDynamicsRampDurs.at(i) = 0;
						pitchBendVals.at(i) = 0;
						//textIndexesLocal.at(index2) = 0;
						index2++;
						if (firstChordNote) firstChordNote = false;
					}
					else {
						// if we have a chord, push this note to the current vector
						notePairs.back().push_back(std::make_pair(midiNote, naturalScaleNote));
						// increment the counter of the chord notes
						// so we can set the index for each note in a chord properly
						chordNotesIndexes.at(i)++;
						notesCounter.at(i)++;
					}
					foundNotes.at(i) = true;
					break;
				}
			}
			if (!foundNotes.at(i)) {
				// a duration without a note repeats the previous note, as in LilyPond (e.g. after a tie or a slur), but not after a rest
				if (sharedData.instruments.at(lastInstrumentIndex).isRhythm() || isLastNoteTied || lastMidiNote > -1) {
					// to be able to write a duration without a note, for rhythm staffs
					// we need to check if the first character is a number
					if (subtokens.at(j).size() > 1) {
						if (isdigit(subtokens.at(j).at(0)) && isdigit(subtokens.at(j).at(1))) {
							tempDurs.at(i) = int(((subtokens.at(j).at(0) - 48) * 10) + (subtokens.at(j).at(1)) - 48);
						}
						else if (isdigit(subtokens.at(j).at(0))) {
							tempDurs.at(i) = int(subtokens.at(j).at(0)) - 48;
						}
						else {
							return std::make_pair(3, (std::string)"first character must be a note or a chord opening symbol (<), not \"" + subtokens.at(j).at(0) + (std::string)"\"");
						}
					}
					else if (isdigit(subtokens.at(j).at(0))) {
						tempDurs.at(i) = int(subtokens.at(j).at(0)) - 48;
					}
					else {
						return std::make_pair(3, (std::string)"first character must be a note or a chord opening symbol (<), not \"" + subtokens.at(j).at(0) + (std::string)"\"");
					}
					if (std::find(std::begin(dursArr), std::end(dursArr), tempDurs.at(i)) != std::end(dursArr)) {
						if (!sharedData.instruments.at(lastInstrumentIndex).isRhythm() && lastMidiNote > -1) {
							midiNote = lastMidiNote;
							naturalScaleNote = lastNaturalScaleNote;
							repeatsPrevNote.at(i) = (i > 0);
						}
						else {
							midiNote = 59;
							naturalScaleNote = 6;
						}
						// we need to assign 0 to firstCharOffset which defaults to 1
						// because further down it is added to firstChar
						// but, in this case, we want to check the token from its beginning, index 0
						firstCharOffset.at(i) = 0;
						tempDurStored.at(i) = true;
						if (i == 0) dynAtFirstNote = true;
						goto storeNote;
					}
					else {
						return std::make_pair(3, std::to_string(tempDurs.at(i)) + ": wrong duration");
					}
				}
				else {
					return std::make_pair(3, (std::string)"first character must be a note or a chord opening symbol (<), not \"" + subtokens.at(j).at(subtokenNdx) + (std::string)"\"");
				}
			}
			for (k = 0; k < subtokens.at(j).size(); k++) {
				// if we have a tilde, it means that this note is tide to the next one
				if (subtokens.at(j).at(k) == 126) {
					isLastNoteTied = true;
					break;
				}
			}
		}
		index1++;
	}

	// now run through the entire subtoken to check if we have a character that is forbidden inside a chord
	for (i = 0; i < tokens.size(); i++) {
		// here we need to include the angle brackets to make sure we check for invalid characters inside a chord and not outside of it
		std::vector<std::string> subtokens = tokenizeChord(tokens.at(i), true);
		if (subtokens.size() == 1) continue;
		std::vector<char> forbiddenCharsInChord{'(', ')', '-', '.', '\\', '^', '_', '{', '}'};
		bool outsideChord = true;
		for (j = 0; j < subtokens.size(); j++) {
			if (subtokens.at(j).size() == 0) break;
			if (j == 0 && subtokens.at(j).at(0) == '<') outsideChord = false;
			for (k = 0; k < subtokens.at(j).size(); k++) {
				if (j == subtokens.size()-1 && subtokens.at(j).at(k) == '>') {
					outsideChord = true;
					break;
				}
				if (j > 0 && find(forbiddenCharsInChord.begin(), forbiddenCharsInChord.end(), subtokens.at(j).at(k)) != forbiddenCharsInChord.end()) {
					// first check if we have a backslash, in which case we should check if it's actually a dynamic
					if (subtokens.at(j).at(k) == 92) {
						if (k < subtokens.at(j).size()) {
							// compare with "m", "p", "f", "<", ">", and "!"
							if (subtokens.at(j).at(k+1) == char(109) || subtokens.at(j).at(k+1) == char(112) || subtokens.at(j).at(k+1) == char(102) || \
									subtokens.at(j).at(k+1) == char(60) || subtokens.at(j).at(k+1) == char(62) || subtokens.at(j).at(k+1) == char(33)) {
								return std::make_pair(3, "dynamic can't be included within a chord, only outside of it");
							}
							// if we have a cluster, then after the backslash we'll have a 'c', which is permitted
							else if (subtokens.at(j).at(k+1) != 'c') {
								return std::make_pair(3, char(subtokens.at(j).at(k)) + (std::string)" can't be included within a chord, only outside of it");
							}
						}
						else {
							return std::make_pair(3, char(subtokens.at(j).at(k)) + (std::string)" can't be included within a chord, only outside of it");
						}
					}
					else {
						return std::make_pair(3, char(subtokens.at(j).at(k)) + (std::string)" can't be included within a chord, only outside of it");
					}
				}
				else if (std::isdigit(subtokens.at(j).at(k))) {
					return std::make_pair(3, "digits can't be included within a chord, only outside of it");
				}
			}
			// stop checking after exiting the chord
			if (outsideChord) break;
		}
	}

	// reset the indexes
	index1 = index2 = 0;

	// now we have enough information to allocate memory for the rest of the vectors
	// but we do it one by one, to allocate the required memory only
	// as each note might be a chord or not
	std::vector<std::vector<int>> midiNotesData;
	for (i = 0; i < notePairs.size(); i++) {
		int value = notePairs.at(i).at(0).first;
		std::vector<int> aVector(1, value);
		midiNotesData.push_back(std::move(aVector));
		for (j = 1; j < notePairs.at(i).size(); j++) {
			midiNotesData.back().push_back(notePairs.at(i).at(j).first);
		}
	}
	std::vector<std::vector<float>> notesData;
	for (i = 0; i < midiNotesData.size(); i++) {
		float value = midiNotesData.at(i).at(0);
		std::vector<float> aVector(1, value);
		notesData.push_back(std::move(aVector));
		for (j = 1; j < midiNotesData.at(i).size(); j++) {
			notesData.back().push_back((float)midiNotesData.at(i).at(j));
		}
	}
	// create a map with unique notes (octave agnostic) as keys and ints as values
	// to determine whether an accidental has already been assigned to a note or not
	std::map<int, int> singleOctaveNotesAccidentals;
	for (i = 0; i < midiNotesData.size(); i++) {
		for (j = 1; j < midiNotesData.at(i).size(); j++) {
			int key = midiNotesData.at(i).at(j) % 12;
			singleOctaveNotesAccidentals[key] = -1;
		}
	}
	std::vector<std::vector<int>> notesForScore;
	for (i = 0; i < notePairs.size(); i++) {
		int value = notePairs.at(i).at(0).second;
		std::vector<int> aVector(1, value);
		notesForScore.push_back(std::move(aVector));
		for (j = 1; j < notePairs.at(i).size(); j++) {
			notesForScore.back().push_back(notePairs.at(i).at(j).second);
		}
	}
	std::vector<std::vector<int>> accidentalsForScore;
	for (i = 0; i < midiNotesData.size(); i++) {
		std::vector<int> aVector(1, -1);
		accidentalsForScore.push_back(std::move(aVector));
		for (j = 1; j < midiNotesData.at(i).size(); j++) {
			accidentalsForScore.back().push_back(-1);
		}
	}
	// the 2D std::vector below is used here only for memory allocation
	// its contents are updated in drawNotes() of the Notes() class
	std::vector<std::vector<int>> naturalSignsNotWrittenForScore;
	for (i = 0; i < midiNotesData.size(); i++) {
		std::vector<int> aVector(1, 0);
		naturalSignsNotWrittenForScore.push_back(std::move(aVector));
		for (j = 1; j < midiNotesData.at(i).size(); j++) {
			naturalSignsNotWrittenForScore.back().push_back(0);
		}
	}
	int transposedIndex = 0;
	std::vector<std::vector<int>> octavesForScore;
	for (i = 0; i < midiNotesData.size(); i++) {
		if (sharedData.instruments.at(lastInstrumentIndex).isRhythm()) {
			std::vector<int> aVector(1, 1);
			octavesForScore.push_back(std::move(aVector));
		}
		else {
			std::vector<int> aVector(1, transposedOctaves[transposedIndex++]);
			octavesForScore.push_back(std::move(aVector));
		}
		for (j = 1; j < midiNotesData.at(i).size(); j++) {
			octavesForScore.back().push_back(transposedOctaves[transposedIndex++]);
		}
	}

	// before we move on to the rest of the data, we need to store the texts
	// because this is dynamically allocated memory too
	int textsCounter;
	std::vector<std::vector<int>> textIndexesLocal;
	for (i = 0; i < tokens.size(); i++) {
		// check if we have a comment, so we exit the loop
		if (startsWith(tokens.at(i), "%")) break;
		// create an empty entry for every note/chord
		if (textIndexesLocal.size() <= i) {
			textIndexesLocal.push_back({0});
		}
		textsCounter = 0;
		// then check for carrets or underscores
		for (j = 0; j < tokens.at(i).size(); j++) {
			if (j == 0 && isCluster.at(i)) {
				j++;
				continue;
			}
			if (tokens.at(i).at(j) == (94)) {
				if (j > 0) {
					// a carret is also used for articulations, so we must check if the previous character is not a hyphen
					if (tokens.at(i).at(j-1) != '-') {
						if (textsCounter == 0) {
							textIndexesLocal.back().back() = 1;
						}
						else {
							textIndexesLocal.back().push_back(1);
						}
						textsCounter++;
					}
				}
				else {
					if (textsCounter == 0) {
						textIndexesLocal.back().back() = 1;
					}
					else {
						textIndexesLocal.back().push_back(1);
					}
					textsCounter++;
				}
			}
			else if (tokens.at(i).at(j) == 95) {
				if (tokens.at(i).at(j-1) != '-') {
					if (textsCounter == 0) {
						textIndexesLocal.back().back() = -1;
					}
					else {
						textIndexesLocal.back().push_back(-1);
					}
					textsCounter++;
				}
			}
		}
	}
	// once we have sorted out the indexes of the texts, we can sort out the actual texts
	unsigned openQuote, closeQuote;
	std::vector<std::string> texts;
	for (i = 0; i < tokens.size(); i++) {
		// again, check if we have a comment, so we exit the loop
		if (startsWith(tokens.at(i), "%")) break;
		for (j = 0; j < tokens.at(i).size(); j++) {
			// ^ or _ for adding text above or below the note
			if ((tokens.at(i).at(j) == char(94)) || (tokens.at(i).at(j) == char(95))) {
				if (j > 0) {
					if (tokens.at(i).at(j-1) == '-') foundArticulation = true;
				}
				if (j >= (tokens.at(i).size()-1) && !foundArticulation) {
					if (tokens.at(i).at(j) == char(94)) {
						return std::make_pair(3, "a carret must be followed by text in quotes");
					}
					else {
						return std::make_pair(3, "an undescore must be followed by text in quotes");
					}
				}
				if (j < tokens.at(i).size()-1) {
					if (tokens.at(i).at(j+1) != char(34) && !foundArticulation) { // "
						if (tokens.at(i).at(j) == char(94)) {
							return std::make_pair(3, "a carret must be followed by a bouble quote sign");
						}
						else {
							return std::make_pair(3, "an underscore must be followed by a bouble quote sign");
						}
					}
				}
				if (!foundArticulation) {
					openQuote = j+2;
					index2 = j+2;
					closeQuote = j+2;
					while (index2 < tokens.at(i).size()) {
						if (tokens.at(i).at(index2) == char(34)) {
							closeQuote = index2;
							break;
						}
						index2++;
					}
					if (closeQuote <= openQuote) {
						return std::make_pair(3, "text must be between two double quote signs");
					}
					texts.push_back(tokens.at(i).substr(openQuote, closeQuote-openQuote));
					//if (tokens.at(i).at(j) == char(94)) {
					//	textIndexesLocal.at(i) = 1;
					//}
					//else if (tokens.at(i).at(j) == char(95)) {
					//	textIndexesLocal.at(i) = -1;
					//}
					j = closeQuote;
				}
			}
		}
		foundArticulation = false;
	}
	// then copy the texts to textsForScore
	std::vector<std::string> textsForScore;
	for (i = 0; i < texts.size(); i++) {
		textsForScore.push_back(texts.at(i));
	}
	// store articulation symbols, as these have to be allocated dynamically too
	int quotesCounter = 0; // so we can ignore anything that is passed as text
	std::vector<std::vector<int>> articulationIndexes;
	for (i = 0; i < tokens.size(); i++) {
		foundArticulation = false;
		unsigned firstArticulIndex = 0;
		if (articulationIndexes.size() <= i) {
			articulationIndexes.push_back({0});
		}
		std::vector<std::string> subtokens = tokenizeChord(tokens.at(i));
		for (j = 0; j < subtokens.back().size(); j++) {
			if (int(subtokens.back().at(j)) == 34) {
				quotesCounter++;
			}
			if (quotesCounter > 0) {
				if (quotesCounter >= 2) quotesCounter = 0;
				continue; // if we're inside text, ignore
			}
			if (subtokens.back().at(j) == char(45) && (j > 0 && subtokens.back().at(j-1) != 'o')) { // - for articulation symbols
				if (j >= (subtokens.back().size()-1)) {
					return std::make_pair(3, "a hyphen must be followed by an articulation symbol");
				}
				if (!foundArticulation) {
					firstArticulIndex = j;
				}
				// since the hyphen is used both for indicating the command for articulation
				// but also for the tenuto symbol, we have to iterate through these symbols
				// every two, otherwise, if we write a tenuto, the else if (tokens.at(i).at(j) == char(45)) test
				// will be successful not only for the articulation command, but also for the symbol
				else if ((j - firstArticulIndex) < 2) {
					continue;
				}
				foundArticulation = true;
				int articulChar = char(subtokens.back().at(j+1));
				int articulIndex = 0;
				switch (articulChar) {
					// articulation symbol indexes start from 1 because 0 is reserved for no articulation
					case 94: // ^ for marcato
						articulIndex = 1;
						break;
					case 43: // + for trill
						articulIndex = 2;
						break;
					case 45: // - for tenuto
						articulIndex = 3;
						break;
					case 33: // ! for staccatissimo
						articulIndex = 4;
						break;
					case 62: // > for accent
						articulIndex = 5;
						break;
					case 46: // . for staccato
						articulIndex = 6;
						break;
					case 95: // _ for portando
						articulIndex = 7;
						break;
					default:
						return std::make_pair(3, "unknown articulation symbol");
				}
				if (articulationIndexes.at(i).size() == 1 && articulationIndexes.at(i).at(0) == 0) {
					articulationIndexes.at(i).at(0) = articulIndex;
				}
				else {
					articulationIndexes.at(i).push_back(articulIndex);
				}
				j++;
				continue;
			}
		}
	}
	// copy the articulation indexes with an offset for MIDI
	std::vector<std::vector<int>> midiArticulationVals;
	for (i = 0; i < articulationIndexes.size(); i++) {
		std::vector<int> aVector(1, (articulationIndexes.at(i).at(0) > 0 ? articulationIndexes.at(i).at(0)+9 : 1));
		midiArticulationVals.push_back(std::move(aVector));
		for (j = 1; j < articulationIndexes.at(i).size(); j++) {
			midiArticulationVals.back().push_back(articulationIndexes.at(i).at(j)+9);
		}
	}

	// the tuplets, in the form the score expects them, by their index
	for (size_t t = 0; t < tupletRatiosLocal.size(); t++) {
		tupletRatios[(int)t] = tupletRatiosLocal[t];
		tupletStartStop[(int)t] = std::make_pair((unsigned)tupletRanges[t].first, (unsigned)tupletRanges[t].second);
	}

	// we are now done with dynamically allocating memory, so we can move on to the rest of the data
	// we can now create more variables without worrying about memory

	// the vector below is used later to check if we encounter some unknown character
	std::vector<int> foundAccidentals(tokens.size(), 0);
	float accidental = 0.0;
	// various counters
	unsigned numDurs = 0;
	unsigned numDynamics = 0;
	unsigned dynamicsRampCounter = 0;
	unsigned slursCounter = 0;
	for (i = 0; i < tokens.size(); i++) {
		// first check for accidentals, if any
		std::vector<std::string> subtokens = tokenizeChord(tokens.at(i));
		int loopStartNdx = 1;
		for (j = 0; j < subtokens.size(); j++) {
			if (j == 0 && isCluster.at(i)) {
				loopStartNdx = 3;
			}
			bool foundAccidental = false;
			// the first character of a note token is the actual note, so we start from 1 in the loop below
			// unless we have a cluster, in which case we start from 2
			for (k = loopStartNdx; k < subtokens.at(j).size(); k++) {
				if (subtokens.at(j).at(k) == char(101)) { // 101 is "e"
					if (k < subtokens.at(j).size()-1) {
						// if the character after "e" is "s" or "h" we have an accidental
						if (subtokens.at(j).at(k+1) == char(115)) { // 115 is "s"
							accidental -= 1.0; // in which case we subtract one semitone
							foundAccidental = true;
							foundAccidentals.at(i) = 1;
						}
						else if (subtokens.at(j).at(k+1) == char(104)) { // 104 is "h"
							accidental -= 0.5;
							foundAccidental = true;
							foundAccidentals.at(i) = 1;
						}
						else {
							return std::make_pair(3, subtokens.at(j).at(k+1) + (std::string)": unknown accidental character");
						}
					}
					else {
						return std::make_pair(3, "\"e\" must be followed by \"s\" or \"h\"");
					}
				}
				else if (subtokens.at(j).at(k) == char(105)) { // 105 is "i"
					if (k < subtokens.at(j).size()-1) {
						if (subtokens.at(j).at(k+1) == char(115)) { // 115 is "s"
							accidental += 1.0; // in which case we add one semitone
							foundAccidental = true;
							foundAccidentals.at(i) = 1;
						}
						else if (subtokens.at(j).at(k+1) == char(104)) { // 104 is "h"
							accidental += 0.5;
							foundAccidental = true;
							foundAccidentals.at(i) = 1;
						}
						else {
							return std::make_pair(3, subtokens.at(j).at(k+1) + (std::string)": unknown accidental character");
						}
					}
					else {
						return std::make_pair(3, "\"i\" must be followed by \"s\" or \"h\"");
					}
				}
				// we ignore "s" and "h" as we have checked them above
				else if (subtokens.at(j).at(k) == char(115) || subtokens.at(j).at(k) == char(104)) {
					accidentalIndexes.at(i)++;
					continue;
				}
				// when the accidentals characters are over we move on
				else {
					break;
				}
				accidentalIndexes.at(i)++;
			}
			int key = midiNotesData.at(i).at(j) % 12;
			if (foundAccidental) {
				notesData.at(i).at(j) += accidental;
				midiNotesData.at(i).at(j) += (int)accidental;
				// accidentals can go up to a whole tone, which is accidental=2.0
				// but in case it's one and a half tone, one tone is already added above
				// so we need the quarter tone only, which we get with accidental-(int)accidental (0.5 or -0.5)
				// MIDI and SoundFont instruments play it with a pitch bend over 12 semitones up and down (as glissandi, see Sequencer::runGlissando())
				// a pitch bend applies to all notes of a MIDI channel, so in a chord the quarter tone of the last note that has one is played
				float quarterTone = accidental - (int)accidental;
				if (quarterTone != 0) pitchBendVals.at(i) = (int)std::round(quarterTone * 8192.0 / 12.0);
				// store accidentals in the following order with the following indexes
				// 0 - double flat, 1 - one and half flat, 2 - flat, 3 - half flat,
				// 4 - natural, 5 - half sharp, 6 - sharp, 7 - one and half sharp, 8 - double sharp
				// to map the -2 to 2 range of the accidental float variable to a 0 to 8 range
				// we use the map function below
				// this is subtracting the fromLow value (-2) which results in adding 2
				// and mutliplying by the division between the difference between toHigh - toLow and fromHigh - fromLow
				// which again results to 2, since (8 - 0) / (2 - -2) = 8 / 4 = 2
				// a proper mapping function then adds the toLow value, but here it's 0 so we omit it
				int mapped = (accidental + 2) * 2;
				if (singleOctaveNotesAccidentals[key] != mapped) {
					accidentalsForScore.at(i).at(j) = mapped;
				}
				else {
					// display nothing in case this note already has the same accidental
					accidentalsForScore.at(i).at(j) = 9;
				}
				// go back one character because of the last accidentalIndexes.at(i)++ above this if chunk
				accidentalIndexes.at(i)--;
				singleOctaveNotesAccidentals[key] = mapped;
			}
			else {
				accidentalsForScore.at(i).at(j) = singleOctaveNotesAccidentals[key] = -1;
			}
			// add the accidentals based on transposition (if set)
			if (accidentalsForScore.at(i).at(j) == -1) {
				// if we have no accidental, add the transposition to the natural sign indexes with 4
				accidentalsForScore.at(i).at(j) = 4 + transposedAccidentals.at(i);
			}
			else {
				accidentalsForScore.at(i).at(j) += transposedAccidentals.at(i);
			}
			// if the transposed accidental results in natural, assign -1 instead of 4 so as to not display the natural sign
			// if the natural sign is needed to be displayed, this will be taken care of at the end of the last loop
			// that iterates through the tokens of the string we parse in this function
			if (accidentalsForScore.at(i).at(j) == 4) {
				accidentalsForScore.at(i).at(j) = -1;
			}
			accidental = 0;
		}
	}

	std::vector<int> foundDynamics(tokens.size(), 0);
	std::vector<int> foundOctaves(tokens.size(), 0);
	unsigned beginningOfChordIndex = 0;
	bool tokenInsideChord = false;
	int prevScoreDynamic = -1;
	quotesCounter = 0;
	for (i = 0; i < tokens.size(); i++) {
		foundArticulation = false;
		if (beginningOfChords.at(i)) {
			beginningOfChordIndex = i;
			tokenInsideChord = true;
		}
		std::vector<std::string> subtokens = tokenizeChord(tokens.at(i));
		// first check for octaves and ottavas
		for (j = 0; j < subtokens.size(); j++) {
			for (k = 0; k < subtokens.at(j).size(); k++) {
				if (int(subtokens.at(j).at(k)) == 34) {
					quotesCounter++;
				}
				if (quotesCounter > 0) {
					if (quotesCounter >= 2) quotesCounter = 0;
					continue; // if we're inside text, ignore
				}
				if (int(subtokens.at(j).at(k)) == 39) {
					if (!sharedData.instruments.at(lastInstrumentIndex).isRhythm()) {
						notesData.at(i).at(j) += 12;
						midiNotesData.at(i).at(j) += 12;
						octavesForScore.at(i).at(j)++;
					}
					foundOctaves.at(i) = 1;
				}
				else if (int(subtokens.at(j).at(k)) == 44) {
					if (!sharedData.instruments.at(lastInstrumentIndex).isRhythm()) {
						notesData.at(i).at(j) -= 12;
						midiNotesData.at(i).at(j) -= 12;
						octavesForScore.at(i).at(j)--;
					}
					foundOctaves.at(i) = 1;
				}
				else if (subtokens.at(j).at(k) == 'o' && k + 1 < subtokens.at(j).size() &&
						(isdigit(subtokens.at(j).at(k+1)) || (subtokens.at(j).at(k+1) == '-' && k + 2 < subtokens.at(j).size() && isdigit(subtokens.at(j).at(k+2))))) {
					// an ottava written straight after a note (e.g. c'o1) is not part of LiveLily any more
					return std::make_pair(3, "an ottava must be written with \\ottava before the notes it applies to, e.g. \\ottava 1 c'4");
				}
			}
		}
		ottavas.at(i) = noteOttavas.at(i);
		// a duration without a note gets the pitch of the previous note (the last note of a chord), with its octave, accidental, and quarter tone
		// its accidental is not shown again, and the octave of the score is taken before the ottava of the previous note is applied
		// as the ottava of this note is applied below
		if (repeatsPrevNote.at(i) && !midiNotesData.at(i-1).empty() && midiNotesData.at(i-1).back() > -1) {
			midiNotesData.at(i).at(0) = midiNotesData.at(i-1).back();
			notesData.at(i).at(0) = notesData.at(i-1).back();
			notesForScore.at(i).at(0) = notesForScore.at(i-1).back();
			octavesForScore.at(i).at(0) = octavesForScore.at(i-1).back() + ottavas.at(i-1);
			pitchBendVals.at(i) = pitchBendVals.at(i-1);
		}
		// then check for the rest of the characters
		// we don't check inside chords, as the characters we look for in this loop
		// are not permitted inside chords, so we check only at the last chord note
		int digitsCounter = 0; // needed to store two-digit durations
		for (j = 0; j < subtokens.back().size(); j++) {
			if (int(subtokens.back().at(j)) == 34) {
				quotesCounter++;
			}
			if (quotesCounter > 0) {
				if (quotesCounter >= 2) quotesCounter = 0;
				continue; // if we're inside text, ignore
			}
			if (subtokens.back().at(j) == '/') {
				// a tuplet written straight after a note (e.g. c8/3/2) is not part of LiveLily any more
				return std::make_pair(3, "a tuplet must be written with \\tuplet and its notes in curly brackets, e.g. \\tuplet 3/2 { c8 d e }");
			}
			if (isdigit(subtokens.back().at(j))) {
				// make sure this is not a digit after a hyphen (an articulation)
				// in rhythm staffs, notes can be written with a note and a duration (e.g. b16), or with a duration only (e.g. 16)
				// a duration only has already been stored (see tempDurStored above, where the first character is checked), so it is not read again here
				if (j > 0 && subtokens.back().at(j-1) != '-' && !tempDurStored.at(i)) {
					// assemble the value from its ASCII characters
					tempDurs.at(i) = tempDurs.at(i) * 10 + int(subtokens.back().at(j)) - 48;
					if (i == 0) {
						dynAtFirstNote = true;
					}
					if (i > 0 && !tokenInsideChord && !dynAtFirstNote) {
						return std::make_pair(3, "first note doesn't have a duration");
					}
					// make sure there are no digits after this one, in case this is the first digit encountered
					if (!(j < subtokens.back().size() - 1 && digitsCounter == 0 && isdigit(subtokens.back().at(j+1)))) {
						tempDurStored.at(i) = true;
					}
					// count the digits of the duration, otherwise the first digit of a two-digit duration is taken as the whole duration (16 becomes 1)
					digitsCounter++;
				}
			}
			else if (subtokens.back().at(j) == char(92)) { // back slash
				index2 = j;
				int dynamic = 0;
				bool foundDynamicLocal = false;
				bool foundGlissandoLocal = false;
				if (index2 > (subtokens.back().size()-1)) {
					return std::make_pair(3, "a backslash must be followed by a command");
				}
				// loop till you find all dynamics or other commands
				while (index2 < subtokens.back().size()) {
					if (index2+1 == subtokens.back().size()) {
						goto foundDynamicTest;
					}
					if (subtokens.back().at(index2+1) == char(102)) { // f for forte
						if (mezzo) {
							dynamic++;
							mezzo = false;
						}
						else {
							dynamic += 3;
						}
						dynamicsIndexes.at(i) = i;
						dynamicsForScoreIndexes.at(i) = i;
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							dynamicsRampEnd.at(i) = i;
							dynamicsRampEndForScore.at(i) = i;
							dynamicsRampIndexes.at(dynamicsRampCounter-1).second = i;
							if (dynamicsRampStart.at(i) == dynamicsRampEnd.at(i)) {
								std::pair<int, std::string> p = std::make_pair(3, "can't start and end a crescendo/diminuendo on the same note");
								return p;
							}
						}
						foundDynamics.at(i) = 1;
						foundDynamicLocal = true;
					}
					else if (subtokens.back().at(index2+1) == char(112)) { // p for piano
						if (mezzo) {
							dynamic--;
							mezzo = false;
						}
						else {
							dynamic -= 3;
						}
						dynamicsIndexes.at(i) = i;
						dynamicsForScoreIndexes.at(i) = i;
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							dynamicsRampEnd.at(i) = i;
							dynamicsRampEndForScore.at(i) = i;
							dynamicsRampIndexes.at(dynamicsRampCounter-1).second = i;
							if (dynamicsRampStart.at(i) == dynamicsRampEnd.at(i)) {
								std::pair<int, std::string> p = std::make_pair(3, "can't start and end a crescendo/diminuendo on the same note");
								return p;
							}
						}
						foundDynamics.at(i) = 1;
						foundDynamicLocal = true;
					}
					else if (subtokens.back().at(index2+1) == char(109)) { // m for mezzo
						mezzo = true;
					}
					else if (subtokens.back().at(index2+1) == char(60)) { // <
						// a crescendo or diminuendo that starts while another one is open ends the open one
						// (e.g. c4\\> d4 e4\\< f4 g4\\!), as if a \\! was written before it
						if (dynamicsRampStarted) {
							if (dynamicsRampStart.at(i) == (int)i) {
								return std::make_pair(3, "can't start and end a crescendo/diminuendo on the same note");
							}
							dynamicsRampEnd.at(i) = i;
							dynamicsRampEndForScore.at(i) = i;
							dynamicsRampIndexes.at(dynamicsRampCounter-1).second = i;
							dynamicsRampStarted = false;
						}
						dynamicsRampStarted = true;
						dynamicsRampStart.at(i) = i;
						dynamicsRampStartForScore.at(i) = i;
						dynamicsRampDirForScore.at(i) = 1;
						dynamicsRampIndexes.at(dynamicsRampCounter).first = i;
						dynamicsRampCounter++;
					}
					else if (subtokens.back().at(index2+1) == char(62)) { // >
						// a crescendo or diminuendo that starts while another one is open ends the open one
						// (e.g. c4\\> d4 e4\\< f4 g4\\!), as if a \\! was written before it
						if (dynamicsRampStarted) {
							if (dynamicsRampStart.at(i) == (int)i) {
								return std::make_pair(3, "can't start and end a crescendo/diminuendo on the same note");
							}
							dynamicsRampEnd.at(i) = i;
							dynamicsRampEndForScore.at(i) = i;
							dynamicsRampIndexes.at(dynamicsRampCounter-1).second = i;
							dynamicsRampStarted = false;
						}
						dynamicsRampStarted = true;
						dynamicsRampStart.at(i) = i;
						dynamicsRampStartForScore.at(i) = i;
						dynamicsRampDirForScore.at(i) = 0;
						dynamicsRampIndexes.at(dynamicsRampCounter).first = i;
						dynamicsRampCounter++;
					}
					else if (subtokens.back().at(index2+1) == char(33)) { // exclamation mark
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							dynamicsRampEnd.at(i) = i;
							dynamicsRampEndForScore.at(i) = i;
							dynamicsRampIndexes.at(dynamicsRampCounter-1).second = i;
							if (dynamicsRampStart.at(i) == dynamicsRampEnd.at(i)) {
								std::pair<int, std::string> p = std::make_pair(3, "can't start and end a crescendo/diminuendo on the same note");
								return p;
							}
						}
						else {
							return std::make_pair(3, "no crescendo or diminuendo initiated");
						}
					}
					else if (subtokens.back().at(index2+1) == char(103)) { // g for gliss
						if ((subtokens.back().substr(index2+1,5).compare("gliss") == 0) ||
								(subtokens.back().substr(index2+1,9).compare("glissando") == 0)) {
							glissandiIndexes.at(i) = 1;
							glissandiIndexesForScore.at(i) = 1;
							foundGlissandoLocal = true;
						}
					}
					else {
					foundDynamicTest:
						if (foundDynamicLocal) {
							// dynamics are stored like this:
							// ppp = -9, pp = -6, p = -3, mp = -1, mf = 1, f = 3, ff = 6, fff = 9
							// so to std::map this range to 82-100 (dB) we must first add 9 (subtract -9)
							// multiply by 1 (100 - 82) / (9 - (-9)) = 18 / 18
							// so we actually omit this multiplication
							// and add 82 (the lowest value)
							if (dynamic < -9) dynamic = -9;
							else if (dynamic > 9) dynamic = 9;
							// convert this value to an index for dynamics symbols for the score
							int dynamicForScore = std::distance(scoreDynamics.begin(), std::find(scoreDynamics.begin(), scoreDynamics.end(), dynamic));
							// and get the std::mapped values to MIDI velocities and divide by 127 to normalize to 1
							dynamicsData.at(i) = (float)mappedScoreDynamics[dynamic] / 127.0;
							if (dynamicForScore != prevScoreDynamic) {
								dynamicsForScore.at(i) = dynamicForScore;
								prevScoreDynamic = dynamicForScore;
							}
							else {
								// if it's the same dynamic, we must invalidate this index
								dynamicsForScoreIndexes.at(i) = -1;
							}
							numDynamics++;
							// to get the dynamics in MIDI velocity values, we map ppp to 16, and fff to 127
							// based on this website https://hedsounds.blogspot.com/p/midi-velocity-db-dynamics-db-and.html
							// first find the index of the dynamic value in the array {-9, -6, -3, -1, 1, 3, 6, 9}
							int midiVelIndex = std::distance(dynsArr, std::find(dynsArr, dynsArr+8, dynamic));
							midiVels.at(i) = (midiVelIndex+1)*MIDIVELSTEP;
							if (midiVels.at(i) > 127) midiVels.at(i) = 127;
						}
						// if we haven't found a dynamic and the ramp vectors are empty
						// it means that we have received some unknown character
						else if (dynamicsRampCounter == 0 && !foundGlissandoLocal) {
							return std::make_pair(3, tokens.at(i).at(index2+1) + (std::string)": unknown character");
						}
						break;
					}
					index2++;
				}
				j = index2;
			}

			// . for dotted note, which is also used for staccato, hence the second test against 45 (hyphen)
			else if (subtokens.back().at(j) == char(46) && subtokens.back().at(j-1) != char(45)) {
				dotIndexes.at(i) = 1;
				dotsCounter.at(i)++;
			}

			else if (subtokens.back().at(j) == char(40)) { // ( for beginning of slur
				slurStarted = true;
				slurBeginningsIndexes.at(i) = i;
				//slurIndexes.at(i).first = i;
				slursCounter++;
			}

			else if (subtokens.back().at(j) == char(41)) { // ) for end of slur
				slurStarted = false;
				slurEndingsIndexes.at(i) = i;
				//slurIndexes.at(i).second = i;
			}

			else if (subtokens.back().at(j) == char(126)) { // ~ for tie
				tieIndexes.at(i) = i;
			}

			// check for _ or ^ and make sure that the previous character is not -
			// of that the next is a quote
			else if ((int(subtokens.back().at(j)) == 94 || int(subtokens.back().at(j)) == 95) && int(subtokens.back().at(j-1)) != 45) {
				if (textIndexesLocal.at(i).at(0) == 0) {
					return std::make_pair(3, (std::string)"stray " + subtokens.back().at(j));
				}
				if (subtokens.back().size() < j+3) {
					return std::make_pair(3, "incorrect text notation");
				}
				if (int(subtokens.back().at(j+1)) != 34) {
					return std::make_pair(3, "a text symbol must be followed by quote signs");
				}
			}
		}
		// add the extracted octave with \ottava
		for (j = 0; j <= chordNotesIndexes.at(i); j++) {
			octavesForScore.at(i).at(j) -= ottavas.at(i);
		}
		// store durations at the end of each token only if tempDur is greater than 0
		if (tempDurs.at(i) > 0) {
			if (std::find(std::begin(dursArr), std::end(dursArr), tempDurs.at(i)) == std::end(dursArr)) {
				return std::make_pair(3, std::to_string(tempDurs.at(i)) + " is not a valid duration");
			}
			dursData.at(i) = tempDurs.at(i);
			durIndexes.at(i) = i;
			numDurs++;
			if (tokenInsideChord) {
				for (unsigned k = beginningOfChordIndex; k < i; k++) {
					dursData.at(k) = tempDurs.at(i);
				}
			}
		}
		if (endingOfChords.at(i)) {
			tokenInsideChord = false;
		}
		// once done parsing everything, check if we need to change an accidental by placing the natural sign
		// in case the same note has already been used in this bar with an accidental and now without
		int thisNote = notesForScore.at(i).at(chordNotesIndexes.at(i));
		int thisOctave = octavesForScore.at(i).at(chordNotesIndexes.at(i));
		// do a reverse loop to see if the last same note had its accidental corrected
		bool foundSameNote = false;
		// use ints instead of j (we are inside a loop using i) because j is unsigned
		// and a backwards loop won't work as it will wrap around instead of going below 0
		// a repeated note (a duration without a note) doesn't show an accidental, and is skipped when looking for the last same note
		// so that a natural sign after it refers to the note it repeats
		if (accidentalsForScore.at(i).at(chordNotesIndexes.at(i)) == -1 && !repeatsPrevNote.at(i)) {
			for (int k = (int)i-1; k >= 0; k--) {
				if (repeatsPrevNote.at(k)) continue;
				for (int l = (int)notesForScore.at(k).size()-1; l >= 0; l--) {
					if (notesForScore.at(k).at(l) == thisNote && ((correctOnSameOctaveOnly && octavesForScore.at(k).at(l) == thisOctave) || !correctOnSameOctaveOnly)) {
						if (accidentalsForScore.at(k).at(l) > -1 && accidentalsForScore.at(k).at(l) != 4) {
							accidentalsForScore.at(i).at(chordNotesIndexes.at(i)) = 4;
							foundSameNote = true;
						}
						else {
							accidentalsForScore.at(i).at(chordNotesIndexes.at(i)) = -1;
							foundSameNote = true;
						}
						break;
					}
				}
				if (foundSameNote) break;
			}
			if (!foundSameNote) accidentalsForScore.at(i).at(chordNotesIndexes.at(i)) = -1;
		}
		if (slurStarted) {
			isSlurred.at(i) = true;
		}
	}
	// a line that stops a slur before starting one is inside a slur from the previous bar until that stop
	// so the notes before the stop are slurred too (the note of the stop isn't, as it is the last note of the slur)
	for (size_t s = 0; s < slurEndingsIndexes.size(); s++) {
		if (slurBeginningsIndexes.at(s) > -1 && slurEndingsIndexes.at(s) == -1) break;
		if (slurEndingsIndexes.at(s) > -1) {
			for (size_t n = 0; n < s && n < isSlurred.size(); n++) isSlurred.at(n) = true;
			break;
		}
	}

	if (dynamicsRampStarted) {
		dynamicsRampIndexes.at(dynamicsRampCounter-1).second = i - 1;
		dynamicsRampEnd.back() = i - 1;
		dynamicsRampEndForScore.back() = i - 1;
	}

	// check if there are any unmatched quote signs
	if (quotesCounter > 0) {
		return std::make_pair(3, "unmatched quote sign");
	}

	// once all elements have been parsed we can determine whether we're fine to go on
	for (i = 0; i < tokens.size(); i++) {
		if (!foundNotes.at(i) && !foundDynamics.at(i) && !foundAccidentals.at(i) && !foundOctaves.at(i)) {
			return std::make_pair(3, tokens.at(i) + (std::string)": unknown token");
		}
	}

	if (numDurs == 0) {
		return std::make_pair(3, "no durations added");
	}

	// fill in possible empty slots of durations
	// a token without a duration takes the duration of the previous token, with its dots (e.g. the r in 16. r is a dotted 16th)
	// so the first token must have a duration of its own (e.g. r 4 is a rest without a duration and a note of a quarter, not a quarter rest)
	if (!tokens.empty() && durIndexes.at(0) != 0) {
		return std::make_pair(3, tokens.at(0) + ": the first note doesn't have a duration");
	}
	for (i = 0; i < tokens.size(); i++) {
		if ((int)i != durIndexes.at(i)) {
			dursData.at(i) = dursData.at(i-1);
			if (dotsCounter.at(i) == 0) {
				dotIndexes.at(i) = dotIndexes.at(i-1);
				dotsCounter.at(i) = dotsCounter.at(i-1);
			}
		}
	}

	// fill in possible empty slots of dynamics
	if (numDynamics == 0) {
		for (i = 0; i < tokens.size(); i++) {
			// if this is not the very first bar get the last value of the previous bar
			if (sharedData.loopData.size() > 0) {
				int prevBar = getPrevBarIndex();
				// get the last stored dynamic
				dynamicsData.at(i) = sharedData.instruments.at(lastInstrumentIndex).dynamics.at(prevBar).back();
				// do the same for the MIDI velocities
				midiVels.at(i) = sharedData.instruments.at(lastInstrumentIndex).midiVels.at(prevBar).back();
			}
			else {
				// fill in a default value of halfway from min to max dB
				dynamicsData.at(i) = (float)mappedScoreDynamics[0] / 127.0;
				// calculate half way between mp and mf in MIDI velocity
				midiVels.at(i) = ((128 - MIDIVELSTEP) / 2) + MIDIVELSTEP;
			}
		}
	}
	else {
		for (i = 0; i < tokens.size(); i++) {
			// if a dynamic has not been stored, fill in the last
			// stored dynamic in this empty slot
			if ((int)i != dynamicsIndexes.at(i)) {
				// we don't want to add data for the score, only for the sequencer
				if (i > 0) {
					dynamicsData.at(i) =  dynamicsData.at(i-1);
					midiVels.at(i) = midiVels.at(i-1);
				}
				else {
					// fill in a default value of halfway from min to max dB
					dynamicsData.at(i) = (float)mappedScoreDynamics[0] / 127.0;
					// calculate half way between mp and mf in MIDI velocity
					midiVels.at(i) = ((128 - MIDIVELSTEP) / 2) + MIDIVELSTEP;
				}
			}
		}
	}

	// a crescendo or diminuendo that is left open at the end of the bar goes on to the first note of the bar that comes next
	// which is known only when the bar is played, so the sequencer calculates its dynamics (see Sequencer::noteDynamics())
	// and its notes keep the dynamic of its start here
	std::pair<int, int> openDynamicsRamp = std::make_pair(-1, -1);
	if (dynamicsRampStarted && dynamicsRampCounter > 0) {
		int rampStart = (int)dynamicsRampIndexes.at(dynamicsRampCounter-1).first;
		openDynamicsRamp = std::make_pair(rampStart, dynamicsRampDirForScore.at(rampStart));
	}
	// correct dynamics in case of crescendi or decrescendi
	for (i = 0; i < dynamicsRampCounter; i++) {
		if (dynamicsRampStarted && i == dynamicsRampCounter - 1) continue;
		unsigned startNdx = dynamicsRampIndexes.at(i).first;
		unsigned endNdx = dynamicsRampIndexes.at(i).second;
		// check if any crescendi/diminuendi are left hanging
		int numSteps = endNdx - startNdx;
		float valsDiff = dynamicsData.at(endNdx) - dynamicsData.at(startNdx);
		// if a crescendo/diminuendo has not been explicitly defined (either left hanging or ended with an exclamation mark)
		// then we add or subtract the next dynamic from the one where the crescendo/diminuendo starts to the closing of the ramp
		// we get this value by multiplying by 127 to get back to the MIDI range, then add MIDIVELSTEP (defined in ofApp.h), which is one dynamics step
		// set the sign for this one step (see the contiuation of this comment) and then divide by 127
		// to bring it back to a range between 0 and 1
		// to determine whether we need to add or subtract, we use dynamicsRampDirForScore
		// this is 1 for crescendo and 0 for diminuendo (-1 for nothing)
		// to get a 1 or a -1, depending on the dynamics direction, we multiply by 2 and subtract 1
		if (valsDiff == 0) {
			dynamicsData.at(endNdx) += ((float)(MIDIVELSTEP * ((dynamicsRampDirForScore.at(startNdx) * 2) - 1)) / 127.0);
			valsDiff = dynamicsData.at(endNdx) - dynamicsData.at(startNdx);
		}
		int midiVelsDiff = midiVels.at(endNdx) - midiVels.at(startNdx);
		float step = valsDiff / std::max(numSteps, 1);
		int midiVelsStep = midiVelsDiff / max(numSteps, 1);
		index2 = 1;
		for (j = startNdx+1; j < endNdx; j++) {
			dynamicsData.at(j) += (step * (float)index2);
			midiVels.at(j) += (midiVelsStep * index2);
			index2++;
		}
	}
	// store the next dynamic in case of the dynamics ramps
	// replace all values within a ramp with the dynamic value of the next index
	for (i = 0; i < dynamicsRampCounter; i++) {
		for (j = dynamicsRampIndexes.at(i).first; j < dynamicsRampIndexes.at(i).second; j++) {
			// if a ramp ends at the end of the bar without setting a new dynamic
			// go back to the pre-last dynamic
			if (j == notesData.size()-1) {
				// if there's only one ramp in the bar store the first dynamic
				if (i == 0) {
					dynamicsRampData.at(j) = dynamicsData.at(0);
				}
				else {
					dynamicsRampData.at(j) = dynamicsData.at(dynamicsRampIndexes.at(i-1).first);
				}
			}
			// otherwise store the next dynamic so that the receiving program
			// ramps from its current dynamic to the next one in the duration
			// of the current note (retrieved by the sharedData.durs std::vector)
			else {
				dynamicsRampData.at(j) = dynamicsData.at(j+1);
			}
		}
	}
	// get the tempo of the minimum duration
	// barIndex has been defined at the beginning of parseMelodicLine() after done expanding commands
	if (sharedData.numerator.find(barIndex) == sharedData.numerator.end()) {
		sharedData.numerator.at(barIndex) = sharedData.denominator.at(barIndex) = 4;
	}
	int dursAccum = 0;
	int scoreDur; // durations for score should not be affected by tuplet calculations
	int halfDur; // for halving for every dot
	// the duration of a note in a tuplet is multiplied by the ratio of every tuplet it is in (e.g. by 2/3 in a 3/2 tuplet)
	// these durations are not whole numbers (e.g. a triplet eighth is 32 * 2 / 3), so they are accumulated exactly
	// as fractions with a common denominator, and every note gets the difference between the rounded accumulated durations
	// this way, every tuplet and the whole bar add up exactly, with tuplets nested to any depth
	std::vector<int64_t> tupletFactorNum(tokens.size(), 1);
	std::vector<int64_t> tupletFactorDen(tokens.size(), 1);
	for (size_t t = 0; t < tupletRatiosLocal.size(); t++) {
		for (int n = tupletRanges[t].first; n <= tupletRanges[t].second; n++) {
			tupletFactorNum.at(n) *= tupletRatiosLocal[t].second;
			tupletFactorDen.at(n) *= tupletRatiosLocal[t].first;
		}
	}
	int64_t commonDen = 1;
	for (int64_t den : tupletFactorDen) {
		commonDen = std::lcm(commonDen, den);
		if (commonDen > ((int64_t)1 << 40)) {
			return std::make_pair(3, "tuplets are nested too deeply");
		}
	}
	int64_t exactAccum = 0; // in units of 1/commonDen of the minimum duration
	int64_t roundedAccum = 0;
	// convert durations to number of beats with respect to minimum duration
	for (i = 0; i < tokens.size(); i++) {
		dursData.at(i) = MINDUR / dursData.at(i);
		halfDur = dursData.at(i) / 2;
		if (dotIndexes.at(i) == 1) {
			for (j = 0; j < dotsCounter.at(i); j++) {
				dursData.at(i) += halfDur;
				halfDur /= 2;
			}
		}
		scoreDur = dursData.at(i);
		exactAccum += (int64_t)dursData.at(i) * tupletFactorNum.at(i) * (commonDen / tupletFactorDen.at(i));
		int64_t rounded = (exactAccum + commonDen / 2) / commonDen;
		dursData.at(i) = (int)(rounded - roundedAccum);
		roundedAccum = rounded;
		dursAccum += dursData.at(i);
		dursForScore.at(i) = scoreDur;
		dursDataWithoutSlurs.at(i) = dursData.at(i);
	}
	// if all we have is "r1", then it's a tacet, otherwise, we need to check the durations sum
	if (!(tokens.size() == 1 && tokens.at(0).compare("r1") == 0)) {
		// if it's the first bar, it can be incomplete, but it's name must be "0"
		if (dursAccum < sharedData.numBeats.at(barIndex)) {
			if (sharedData.barsOrdered[barIndex].compare("\\0") == 0) {
				if (numInstsParsed == 0) {
					sharedData.numBeats.at(barIndex) = dursAccum;
					sharedData.numerator.at(barIndex) = (MINDUR / dursAccum) / sharedData.denominator.at(barIndex);
				}
				else if (sharedData.numBeats.at(barIndex) != dursAccum) {
					return std::make_pair(3, "durations sum is not equal between all instruments");
				}
			}
			else {
				return std::make_pair(3, "durations sum is less than bar duration");
			}
		}
		if (dursAccum > sharedData.numBeats.at(barIndex)) {
			return std::make_pair(3, "durations sum is greater than bar duration");
		}
	}

	// check if a chord is a cluster and insert all intermediate notes
	for (size_t i = 0; i < tokens.size(); i++) {
		if (isCluster.at(i)) {
			// clusters are defined by two notes only
			// if more have been provided by the user, this error has already been checked
			// in the parseCommands() function
			int noteDiff = midiNotesData[i][1] - midiNotesData[i][0];
			if (noteDiff > 0) noteDiff--;
			else noteDiff++;
			for (int j = 0; j < abs(noteDiff); j++) {
				int sign = 1;
				if (noteDiff < 0) sign = -1;
				notesData.at(i).insert(notesData.at(i).begin()+1+j, notesData.at(i).at(0)+(j*sign)+sign);
				midiNotesData.at(i).insert(midiNotesData.at(i).begin()+1+j, midiNotesData.at(i).at(0)+(j*sign)+sign);
			}
		}
	}

	// determine the duration of possible glissandi in ms
	for (size_t i = 0; i < glissandiIndexes.size(); i++) {
		if (glissandiIndexes.at(i) == 1) {
			glissDursMs.at(i) = (uint64_t)((double)dursData.at(i) * (sharedData.tempo[barIndex]));
		}
	}

	// now we can create a vector of pairs with the indexes of the slurs
	// every slur is a pair of its start and stop notes, in the order they appear
	// a stop before any start belongs to a slur from the previous bar (-1 for its start), and a start without a stop goes on to the next bar
	// a note can stop a slur and start the next one (e.g. c'4)( ), in which case the stop comes first
	std::vector<std::pair<int, int>> slurIndexes;
	bool slurOpen = false;
	for (i = 0; i < numNotesVertical; i++) {
		if (slurEndingsIndexes.at(i) > -1) {
			if (slurOpen) slurIndexes.back().second = slurEndingsIndexes.at(i);
			else slurIndexes.push_back(std::make_pair(-1, slurEndingsIndexes.at(i)));
			slurOpen = false;
		}
		if (slurBeginningsIndexes.at(i) > -1) {
			slurIndexes.push_back(std::make_pair(slurBeginningsIndexes.at(i), -1));
			slurOpen = true;
		}
	}
	// a bar without slurs has one pair of -1s, as the score expects at least one (see Notes::storeSlurCoords())
	if (slurIndexes.empty()) slurIndexes.push_back(std::make_pair(-1, -1));

	// once the melodic line has been parsed, we can insert the new data to the maps of the Instrument object
	std::map<std::string, int>::iterator it = sharedData.instrumentIndexes.find(lastInstrument);
	if (it == sharedData.instrumentIndexes.end()) {
		return std::make_pair(3, "instrument does not exist");
	}
	int thisInstIndex = it->second;
	sharedData.instruments.at(thisInstIndex).notes[barIndex] = std::move(notesData);
	sharedData.instruments.at(thisInstIndex).midiNotes[barIndex] = std::move(midiNotesData);
	sharedData.instruments.at(thisInstIndex).durs[barIndex] = std::move(dursData);
	sharedData.instruments.at(thisInstIndex).dursWithoutSlurs[barIndex] = std::move(dursDataWithoutSlurs);
	sharedData.instruments.at(thisInstIndex).midiDursWithoutSlurs[barIndex] = std::move(midiDursDataWithoutSlurs);
	sharedData.instruments.at(thisInstIndex).dynamics[barIndex] = std::move(dynamicsData);
	sharedData.instruments.at(thisInstIndex).midiVels[barIndex] = std::move(midiVels);
	sharedData.instruments.at(thisInstIndex).pitchBendVals[barIndex] = std::move(pitchBendVals);
	sharedData.instruments.at(thisInstIndex).dynamicsRamps[barIndex] = std::move(dynamicsRampData);
	sharedData.instruments.at(thisInstIndex).openDynamicsRamp[barIndex] = openDynamicsRamp;
	sharedData.instruments.at(thisInstIndex).glissandi[barIndex] = std::move(glissandiIndexes);
	sharedData.instruments.at(thisInstIndex).glissDursMs[barIndex] = std::move(glissDursMs);
	sharedData.instruments.at(thisInstIndex).midiGlissDurs[barIndex] = std::move(midiGlissDurs);
	sharedData.instruments.at(thisInstIndex).midiDynamicsRampDurs[barIndex] = std::move(midiDynamicsRampDurs);
	sharedData.instruments.at(thisInstIndex).articulations[barIndex] = std::move(articulationIndexes);
	sharedData.instruments.at(thisInstIndex).midiArticulationVals[barIndex] = std::move(midiArticulationVals);
	sharedData.instruments.at(thisInstIndex).isSlurred[barIndex] = std::move(isSlurred);
	sharedData.instruments.at(thisInstIndex).isCluster[barIndex] = std::move(isCluster);
	sharedData.instruments.at(thisInstIndex).text[barIndex] = std::move(texts);
	sharedData.instruments.at(thisInstIndex).textIndexes[barIndex] = std::move(textIndexesLocal);
	sharedData.instruments.at(thisInstIndex).slurIndexes[barIndex] = std::move(slurIndexes);
	sharedData.instruments.at(thisInstIndex).tieIndexes[barIndex] = std::move(tieIndexes);
	sharedData.instruments.at(thisInstIndex).scoreNotes[barIndex] = std::move(notesForScore);
	sharedData.instruments.at(thisInstIndex).scoreDurs[barIndex] = std::move(dursForScore);
	sharedData.instruments.at(thisInstIndex).scoreDotIndexes[barIndex] = std::move(dotIndexes);
	sharedData.instruments.at(thisInstIndex).scoreDotsCounter[barIndex] = std::move(dotsCounter);
	sharedData.instruments.at(thisInstIndex).scoreAccidentals[barIndex] = std::move(accidentalsForScore);
	sharedData.instruments.at(thisInstIndex).scoreNaturalSignsNotWritten[barIndex] = std::move(naturalSignsNotWrittenForScore);
	sharedData.instruments.at(thisInstIndex).scoreOctaves[barIndex] = std::move(octavesForScore);
	sharedData.instruments.at(thisInstIndex).scoreOttavas[barIndex] = std::move(ottavas);
	sharedData.instruments.at(thisInstIndex).scoreGlissandi[barIndex] = std::move(glissandiIndexesForScore);
	//sharedData.instruments.at(thisInstIndex).scoreArticulations[barIndex] = std::move(articulationIndexes);
	sharedData.instruments.at(thisInstIndex).scoreDynamics[barIndex] = std::move(dynamicsForScore);
	sharedData.instruments.at(thisInstIndex).scoreDynamicsIndexes[barIndex] = std::move(dynamicsForScoreIndexes);
	sharedData.instruments.at(thisInstIndex).scoreDynamicsRampStart[barIndex] = std::move(dynamicsRampStartForScore);
	sharedData.instruments.at(thisInstIndex).scoreDynamicsRampEnd[barIndex] = std::move(dynamicsRampEndForScore);
	sharedData.instruments.at(thisInstIndex).scoreDynamicsRampDir[barIndex] = std::move(dynamicsRampDirForScore);
	sharedData.instruments.at(thisInstIndex).scoreTupletRatios[barIndex] = std::move(tupletRatios);
	sharedData.instruments.at(thisInstIndex).scoreTupletStartStop[barIndex] = std::move(tupletStartStop);
	sharedData.instruments.at(thisInstIndex).scoreTexts[barIndex] = std::move(textsForScore);
	sharedData.instruments.at(thisInstIndex).isWholeBarSlurred[barIndex] = false;
	// in case we have no slurs, we must check if there is a slurred note that is not closed in the previous bar
	// or if the whole previous bar is slurred
	if (slurIndexes.size() == 1 && slurIndexes.at(0).first == -1 && slurIndexes.at(0).second == -1) {
		int prevBar = getPrevBarIndex();
		if ((sharedData.instruments.at(thisInstIndex).slurIndexes.at(prevBar).back().first > -1 &&
				sharedData.instruments.at(thisInstIndex).slurIndexes.at(prevBar).back().second == -1) ||
				sharedData.instruments.at(thisInstIndex).isWholeBarSlurred.at(prevBar)) {
			sharedData.instruments.at(thisInstIndex).isWholeBarSlurred.at(barIndex) = true;
		}
	}

	sharedData.instruments.at(thisInstIndex).setPassed(true);
	editors[whichPane].connectLineToBar(lineNum, thisInstIndex, barIndex);
	// the tokens of the notes, for highlighting them in the editor while they play (see \editor.animate)
	storeEditorNoteSpans(lineNum, thisInstIndex, barIndex, (int)numNotesVertical);
	// the map below is used by the sequencer, to determine which pane will have its active element set
	sharedData.barsConnectedToPanes[barIndex] = whichPane;
	numInstsParsed++;
	return std::make_pair(0, "");
}

//--------------------------------------------------------------
std::pair<int, std::string> ofApp::parseBarLoop(std::string str, int lineNum, int numLines)
{
	// first check if there is anything after the closing bracket
	size_t closingBracketIndex = str.find("}");
	// a loop is parsed in one line (see normalizeDefinitionLines()), so without a closing bracket the loop is not closed
	if (closingBracketIndex == std::string::npos) {
		return std::make_pair(3, "missing closing curly bracket for the loop");
	}
	if (closingBracketIndex > 0 && str.substr(closingBracketIndex-1, 1).compare(" ") == 0) {
		str = str.substr(0, closingBracketIndex-1) + str.substr(closingBracketIndex);
		closingBracketIndex--;
	}
	size_t strLength = str.size();
	std::string restOfCommand = "";
	// storing anything after a closing bracket enables us
	// to call a loop in the same line we define it, like this
	// \loop name {\bar1 \bar2} \name
	if (closingBracketIndex != std::string::npos) {
		restOfCommand = str.substr(closingBracketIndex);
		strLength = closingBracketIndex;
	}
	std::string wildCardStr = (endsWith(str, "}") ? str.substr(0, str.size()-1) : str);
	// if we get only an asterisk, it means we want to create a loop with all the bars
	if (wildCardStr.compare("*") == 0) {
		std::vector<int> thisBarIndexes;
		for (auto it = sharedData.barsOrdered.begin(); it != sharedData.barsOrdered.end(); ++it) {
			// the first, default bar (a tacet) has index 0, which we must ignore here
			if (it->first > 0) thisBarIndexes.push_back(it->first);
		}
		// find the last index of the stored loops and store the vector we just created to the value of loopData
		int loopNdx = getLastLoopIndex();
		sharedData.loopData[loopNdx] = thisBarIndexes;
		return std::make_pair(0, "");
	}
	// if we get only two asterisks, it means we want to create a loop with all the bars and loops
	if (wildCardStr.compare("**") == 0) {
		// find the last index of the stored loops to avoid storing its own data into itself
		int loopNdx = getLastLoopIndex();
		std::vector<int> thisLoopIndexes;
		for (auto it = sharedData.loopsOrdered.begin(); it != sharedData.loopsOrdered.end(); ++it) {
			// the first, default bar (a tacet) has index 0, which we must ignore here
			if (it->first > 0 && it->first != loopNdx) thisLoopIndexes.push_back(it->first);
		}
		std::vector<int> v;
		sharedData.loopData[loopNdx] = v;
		for (int thisLoopNdx : thisLoopIndexes) {
			for (int barNdx : sharedData.loopData[thisLoopNdx]) {
				sharedData.loopData[loopNdx].push_back(barNdx);
			}
		}
		return std::make_pair(0, "");
	}
	// check if the asterisk is used as a wild card
	size_t multIndex = wildCardStr.find("*");
	std::vector<std::string> names;
	if (multIndex != std::string::npos) {
		std::string restOfStr;
		if (multIndex < wildCardStr.size()-1 && multIndex > 0) {
			size_t whiteSpaceNdx = wildCardStr.substr(multIndex+1).find(" ");
			if (whiteSpaceNdx == std::string::npos) {
				restOfStr = wildCardStr.substr(multIndex+1);
			}
			else {
				restOfStr = wildCardStr.substr(multIndex+1, whiteSpaceNdx);
			}
		}
		if (!isNumber(restOfStr)) {
			// look for file names that include the characters of the wild card
			std::vector<int> multNdxs = findIndexesOfCharInStr(wildCardStr, "*");
			std::vector<std::string> strParts(multNdxs.size(), "");
			int prevMultNdx = 0;
			for (unsigned i = 0; i < multNdxs.size(); i++) {
				if (wildCardStr.substr(prevMultNdx, multNdxs[i]-prevMultNdx).size() > 0) {
					strParts[i] = wildCardStr.substr(prevMultNdx, multNdxs[i]-prevMultNdx);
				}
				prevMultNdx = multNdxs[i]+1;
			}
			if (wildCardStr.substr(multNdxs.back()+1).size() > 0) {
				strParts.back() = wildCardStr.substr(multNdxs.back()+1);
			}
			for (auto it = sharedData.loopsOrdered.begin(); it != sharedData.loopsOrdered.end(); ++it) {
				if (sharedData.barsIndexes.find(it->second) == sharedData.barsIndexes.end()) {
					// look for bars only, not for loops
					continue;
				}
				bool matches = true;
				for (auto it2 = strParts.begin(); it2 != strParts.end(); ++it2) {
					// in case the pattern doesn't match
					if (it->second.find(*it2) == std::string::npos) {
						matches = false;
						break;
					}
					// a wildcard might include a 1, which will result in including the default first bar
					// so we make sure not to include it with the test below
					if (it->second.compare("\\-1") == 0) {
						matches = false;
						break;
					}
				}
				if (matches) {
					names.push_back(it->second);
				}
			}
		}
		else {
			names = tokenizeString(str.substr(0, strLength), " ");
		}
	}
	else {
		names = tokenizeString(str.substr(0, strLength), " ");
	}
	size_t nameLength;
	int howManyTimes;
	std::map<std::string, int>::iterator thisBarLoopIndexIt;
	std::map<int, std::vector<int>>::iterator thisBarLoopDataIt;
	std::vector<int>::iterator barIndex;
	int i;
	std::vector<int> thisBarIndexes;
	for (std::string name : names) {
		nameLength = name.size();
		multIndex = name.find("*");
		howManyTimes = 1;
		if (multIndex != std::string::npos && multIndex < name.size()-1) {
			std::string repeatCoeff = name.substr(multIndex+1);
			if (endsWith(repeatCoeff, "}")) repeatCoeff.pop_back();
			nameLength = multIndex;
			if (!isNumber(repeatCoeff)) {
				return std::make_pair(3, "repetition coefficient not an int");
			}
			else if (multIndex == 0) {
				// the multiplication symbol cannot be the first symbol in the token
				return std::make_pair(3, "the multiplication character must be concatenated to the bar/loop name");
			}
			howManyTimes = stoi(name.substr(multIndex+1));
		}
		else if (multIndex == name.size()-1) {
			// the multiplication symbol must be followed by a number
			return std::make_pair(3, "the multiplication symbol must be concatenated to a number");
		}
		// first check for barLoops because when we define a bar with data
		// and not with combinations of other bars, we create a barLoop with the same name
		thisBarLoopIndexIt = sharedData.loopsIndexes.find(name.substr(0, nameLength));
		if (thisBarLoopIndexIt == sharedData.loopsIndexes.end()) {
			// check if the substd::string ends with a closing curly bracket, in which case we must remove it
			if (endsWith(name.substr(0, nameLength), "}")) {
				thisBarLoopIndexIt = sharedData.loopsIndexes.find(name.substr(0, nameLength-1));
			}
			else {
				return std::make_pair(3, name.substr(0, nameLength) + (std::string)" doesn't exist");
			}
		}
		// find the std::map of the loop with the std::vector that contains indexes of bars
		thisBarLoopDataIt = sharedData.loopData.find(thisBarLoopIndexIt->second);
		// then iterate over the number of repetitions of this loop
		for (i = 0; i < howManyTimes; i++) {
			// and push back every item in the std::vector that contains the indexes of bars
			for (barIndex = thisBarLoopDataIt->second.begin(); barIndex != thisBarLoopDataIt->second.end(); ++barIndex) {
				thisBarIndexes.push_back(*barIndex);
			}
		}
	}
	// a bar with a slur left open at its end used to be "linked" to the bar after it, and could only be followed by that bar in a loop
	// now a slur left open at the end of a bar goes on to whichever bar comes next (see Sequencer::fireNote())
	// and the score draws it to the end of the bar if the next bar is not the one it goes on in (see Notes::drawSlurs()), so any order is fine
	// find the last index of the stored loops and store the vector we just created to the value of loopData
	int loopNdx = getLastLoopIndex();
	sharedData.loopData[loopNdx] = thisBarIndexes;
	if (restOfCommand.size() > 0) {
		// falsify this so that the name of the loop is treated properly in parseString()
		// otherwise, parseString() will again call this function
		parsingLoop = false;
		parseString(restOfCommand, lineNum, numLines);
	}
	if (!sequencer.isThreadRunning()) {
		sharedData.loopIndex = sharedData.tempLoopIndex;
	}
	sendLoopToParts();
	return std::make_pair(0, "");
}

//--------------------------------------------------------------
std::pair<bool, CmdOutput> ofApp::isInstrument(std::vector<std::string>& originalCommands, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	bool hasDot = false;
	std::vector<std::string> commands;
	// the initial command is \instname which might be followed by a method
	// to separate the method we create a new vector that will copy the originalCommands vector
	// except from the first item where we trim the \instname. part
	for (unsigned i = 0; i < originalCommands.size(); i++) {
		if (!i) {
			// separate the name of the instrument from a possible method
			std::vector<std::string> tokens = tokenizeString(originalCommands[i], ".");
			for (unsigned j = 0; j < tokens.size(); j++) {
				commands.push_back(tokens[j]);
			}
			if (tokens.size() > 1) hasDot = true;
		}
		else {
			commands.push_back(originalCommands[i]);
		}
	}
	bool instrumentExists = false;
	unsigned commandNdxOffset = 1;
	std::pair<int, std::string> error = std::make_pair(0, "");
	if (sharedData.instruments.size() > 0) {
		if (sharedData.instrumentIndexes.find(commands[0]) != sharedData.instrumentIndexes.end()) {
			instrumentExists = true;
			lastInstrument = commands[0];
			lastInstrumentIndex = sharedData.instrumentIndexes[commands[0]];
			if (commands.size() > 1) {
				if (commands[1].compare("rhythm") == 0) {
					if (hasDot) {
						return std::make_pair(instrumentExists, genError("\"rhythm\" is an argument, not a method, should not be concatenated to instrument name with a dot"));
					}
					if (commands.size() > 2) {
						return std::make_pair(instrumentExists, genError("\"rhythm\" argument takes no further arguments"));
					}
					sharedData.instruments[lastInstrumentIndex].setRhythm(true);
				}
				else if (commands[1].compare("transpose") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"transpose\" is a method, must be concatenated to instrument name with a dot"));
					}
					if (commands.size() != 3) {
						return std::make_pair(instrumentExists, genError("\"transpose\" command takes one argument"));
					}
					if (!isNumber(commands[2])) {
						return std::make_pair(instrumentExists, genError("argument to \"transpose\" command must be a number"));
					}
					int transposition = stoi(commands[2]);
					if (transposition < -11 || transposition > 11) {
						return std::make_pair(instrumentExists, genError("argument to \"transpose\" command must be between -11 and 11"));
					}
					sharedData.instruments[lastInstrumentIndex].setTransposition(transposition);
					if (transposition == 0) {
						return std::make_pair(instrumentExists, genWarning("0 \"transpose\" has no effect"));
					}
				}
				else if (commands[1].compare("sendmidi") == 0) {
					if (hasDot) {
						return std::make_pair(instrumentExists, genError("\"sendmidi\" is an argument, not a method, should not be concatenated to instrument name with a dot"));
					}
					if (commands.size() > 2) {
						return std::make_pair(instrumentExists, genError("\"sendmidi\" takes no arguments"));
					}
					sharedData.instruments[lastInstrumentIndex].setSendMIDI(true);
				}
				else if (commands[1].compare("sndfont") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"sndfont\" is a method, must be concatenated to instrument name with a dot"));
					}
					if (commands.size() < 3) {
						return std::make_pair(instrumentExists, genError("no SoundFont path provided"));
					}
					if (commands.size() > 5) {
						return std::make_pair(instrumentExists, genError("\"sndfont\" takes the path to a SoundFont, and optionally a preset and a bank number"));
					}
					// the path can be quoted, start with ~ for the home directory, or be relative to the data directory of LiveLily (bin/data)
					std::string path = commands[2];
					if (path.size() > 1 && path.front() == '"' && path.back() == '"') path = path.substr(1, path.size()-2);
					if (startsWith(path, "~")) path = getenv("HOME") + path.substr(1);
					else if (!startsWith(path, "/")) path = (std::filesystem::path(dataDirectory) / path).string();
					// the preset (e.g. an instrument of a General MIDI SoundFont) and its bank, 0 by default
					int preset = 0, bank = 0;
					if (commands.size() > 3) {
						if (!isNumber(commands[3]) || stoi(commands[3]) < 0 || stoi(commands[3]) > 127) {
							return std::make_pair(instrumentExists, genError("the SoundFont preset must be a number from 0 to 127"));
						}
						preset = stoi(commands[3]);
					}
					if (commands.size() > 4) {
						if (!isNumber(commands[4]) || stoi(commands[4]) < 0 || stoi(commands[4]) > 16383) {
							return std::make_pair(instrumentExists, genError("the SoundFont bank must be a number from 0 to 16383"));
						}
						bank = stoi(commands[4]);
					}
					// the instrument plays on its own channel of the SoundFont synthesizer, which it keeps if it loads another SoundFont
					int channel = sharedData.instruments[lastInstrumentIndex].getSoundFontChannel();
					std::string error = sharedData.soundFonts.assign(path, bank, preset, channel);
					if (!error.empty()) {
						return std::make_pair(instrumentExists, genError(error));
					}
					sharedData.instruments[lastInstrumentIndex].setSoundFontChannel(channel);
					// the instrument sends MIDI, which the sequencer sends to its SoundFont instead of a MIDI port
					sharedData.instruments[lastInstrumentIndex].setMidi(true);
					sharedData.instruments[lastInstrumentIndex].setSendMIDI(true);
				}
				else if (commands[1].compare("sendto") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"sendto\" is a method, must be concatenated to instrument name with a dot"));
					}
					int sendNotice = 0;
					if (commands.size() > 4) {
						return std::make_pair(instrumentExists, genError("\"sendto\" takes zero to two arguments, remote IP and/or port\nor \"python\" without quotes, to send sequencer data straight to Python"));
					}
					if (commands.size() == 3 && commands[2].compare("python") == 0) {
						sharedData.instruments[lastInstrumentIndex].setSendToPython(true);
						return std::make_pair(instrumentExists, cmdOutput);
					}
					std::string remoteIP;
					int port;
					if (commands.size() < 3) {
						remoteIP = "127.0.0.1";
						port = SCOREPARTPORT;
						sendNotice = 1;
					}
					else if (commands.size() > 2) {
						if (commands.size() == 4) {
							if (isNumber(commands[2]) || !isNumber(commands[3])) {
								return std::make_pair(instrumentExists, genError("if two arguments are provided, first argument must be remote IP and second must be port number"));
							}
							remoteIP = commands[2];
							port = stoi(commands[3]);
						}
						else {
							if (isNumber(commands[2])) {
								remoteIP = "127.0.0.1";
								port = stoi(commands[2]);
								sendNotice = 2;
							}
							else {
								remoteIP = commands[2];
								port = SCOREPARTPORT;
								sendNotice = 3;
							}
						}
					}
					std::pair<std::string, int> IPAndPort = std::make_pair(remoteIP, port);
					bool clientExists = false;
					for (auto it = instrumentOSCHostPorts.begin(); it != instrumentOSCHostPorts.end(); ++it) {
						if (IPAndPort == it->second) {
							// if there is already a client that sends to the same server
							// ignore this instrument, as the already existing client will send
							// the generic OSC messages
							clientExists = true;
							break;
						}
					}
					if (!clientExists) {
						sharedData.grouppedOSCClients.push_back(lastInstrumentIndex);
					}
					instrumentOSCHostPorts[lastInstrumentIndex] = IPAndPort;
					sharedData.instruments[lastInstrumentIndex].sendToPart = true;
					sharedData.instruments[lastInstrumentIndex].scorePartSender.setup(remoteIP, port);
					numScorePartSenders++;
					ofxOscMessage m;
					m.setAddress("/initinst");
					m.addIntArg(lastInstrumentIndex);
					m.addStringArg(sharedData.instruments[lastInstrumentIndex].getName());
					sharedData.instruments[lastInstrumentIndex].scorePartSender.sendMessage(m, false);
					m.clear();
					if (sendNotice > 0) {
						switch (sendNotice) {
							case 1:
								cmdOutput = genNote("setting localhost and port 9000");
								break;
							case 2:
								cmdOutput = genNote("setting localhost");
								break;
							case 3:
								cmdOutput = genNote("setting port 9000");
								break;
							default:
								break;
						}
						return std::make_pair(instrumentExists, cmdOutput);
					}
				}
				else if (commands[1].compare("fullscreen") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"fullscreen\" is a method, must be concatenated to instrument name with a dot"));
					}
					if (!sharedData.instruments[lastInstrumentIndex].sendToPart) {
						return std::make_pair(instrumentExists, genError("instrument doesn't have OSC set"));
					}
					if (commands.size() < 3) {
						return std::make_pair(instrumentExists, genError("\"fullscreen\" takes one argument, on or off"));
					}
					if (commands.size() > 3) {
						return std::make_pair(instrumentExists, genError("\"fullscreen\" takes one argument only"));
					}
					if (commands[2].compare("on") == 0) {
						sendFullscreenToPart(lastInstrumentIndex, true);
					}
					else if (commands[2].compare("off") == 0) {
						sendFullscreenToPart(lastInstrumentIndex, false);
					}
					else {
						return std::make_pair(instrumentExists, genError("unknown argument to \"fullscreen\""));
					}
				}
				else if (commands[1].compare("cursor") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"cursor\" is a method, must be concatenated to instrument name with a dot"));
					}
					if (!sharedData.instruments[lastInstrumentIndex].sendToPart) {
						return std::make_pair(instrumentExists, genError("instrument doesn't have OSC set"));
					}
					if (commands.size() < 3) {
						return std::make_pair(instrumentExists, genError("\"cursor\" takes one argument, show or hide "));
					}
					if (commands.size() > 3) {
						return std::make_pair(instrumentExists, genError("\"cursor\" takes one argument only"));
					}
					if (commands[2].compare("show") == 0) {
						sendCursorToPart(lastInstrumentIndex, true);
					}
					else if (commands[2].compare("hide") == 0) {
						sendCursorToPart(lastInstrumentIndex, false);
					}
					else {
						return std::make_pair(instrumentExists, genError("unknown argument to \"cursor\""));
					}
				}
				else if (commands[1].compare("update") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"update\" is a method, must be concatenated to instrument name with a dot"));
					}
					if (commands.size() < 3) {
						return std::make_pair(instrumentExists, genError("\"update\" takes one argument, \"onlast\" or \"immediately\""));
					}
					if (commands[2].compare("onlast") == 0) {
						sendScoreChangeToPart(lastInstrumentIndex, true);
					}
					else if (commands[2].compare("immediately") == 0) {
						sendScoreChangeToPart(lastInstrumentIndex, false);
					}
					else if (commands[2].compare("help") == 0) {
						std::string noteStr = "sets when the connected LiveLily-Part score will be update\n";
						noteStr += "arguments are \"onlast\" or \"immediately\"\n";
						noteStr += "\"onlast\" sets the update to happen when last visible part is playing, \"immediately\" sets the update to happen immediately";
						return std::make_pair(instrumentExists, genNote(noteStr));
					}
					else {
						return std::make_pair(instrumentExists, genError(commands[2] + (std::string)"unknown argument to \"update\", must be \"onlast\" or \"immediately\""));
					}
				}
				else if (commands[1].compare("beatcolor") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"beatcolor\" is a method, must be concatenated to instrument name with a dot"));
					}
					if (commands.size() < 3) {
						return std::make_pair(instrumentExists, genError("\"beatcolor\" takes one argument, \"change\" or \"keep\""));
					}
					if (commands[2].compare("change") == 0) {
						sendChangeBeatColorToPart(lastInstrumentIndex, true);
					}
					else if (commands[2].compare("keep") == 0) {
						sendChangeBeatColorToPart(lastInstrumentIndex, false);
					}
					else {
						return std::make_pair(instrumentExists, genError(commands[2] + (std::string)"unknown argument to \"beatcolor\", must be \"change\" or \"keep\""));
					}
				}
				else if (commands[1].compare("midiport") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"midiport\" is a method, must be concatenated to instrument name with a dot"));
					}
					if (commands.size() != 3) {
						return std::make_pair(instrumentExists, genError("\"midiport\" takes one argument, the MIDI port to open"));
					}
					if (!isNumber(commands[2])) {
						return std::make_pair(instrumentExists, genError("MIDI port must be a number"));
					}
					int midiPort = stoi(commands[2]);
					if (midiPort < 0) {
						return std::make_pair(instrumentExists, genError("MIDI port must be positive"));
					}
					if (midiPort >= (int)sequencer.midiOutPorts.size()) {
						return std::make_pair(instrumentExists, genError("MIDI port doesn't exist"));
					}
					bool openedMidiPort = false;
					if (!midi->isOpen(midiPort)) {
						midi->open(midiPort);
						openedMidiPort = true;
					}
					sharedData.instruments[lastInstrumentIndex].setMidiPort(midiPort);
					if (openedMidiPort) {
						return std::make_pair(instrumentExists, genNote("opened MIDI port " + commands[2]));
					}
				}
				else if (commands[1].compare("midichan") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"midichan\" is a method, must be concatenated to instrument name with a dot"));
					}
					if (commands.size() == 2) {
						return std::make_pair(instrumentExists, genError("no MIDI channel set"));
					}
					if (commands.size() > 3) {
						return std::make_pair(instrumentExists, genError("\"midichan\" takes one argument only"));
					}
					if (!isNumber(commands[2])) {
						return std::make_pair(instrumentExists, genError("MIDI channel must be a number"));
					}
					if (sharedData.instruments[lastInstrumentIndex].getMidiPort() < 0) {
						return std::make_pair(instrumentExists, genError("no MIDI port has been set for this instrument"));
					}
					int midiChan = stoi(commands[2]);
					if (midiChan < 1 || midiChan > 16) {
						return std::make_pair(instrumentExists, genError("MIDI must be between 1 and 16"));
					}
					sharedData.instruments[lastInstrumentIndex].setMidiChan(midiChan);
					sharedData.instruments[lastInstrumentIndex].setMidi(true);
				}
				else if (commands[1].compare("size") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"size\" is a method, must be concatenated to instrument name with a dot"));
					}
					if (!sharedData.instruments[lastInstrumentIndex].sendToPart) {
						return std::make_pair(instrumentExists, genError("instrument doesn't have OSC set"));
					}
					if (commands.size() != 3) {
						return std::make_pair(instrumentExists, genError("\"size\" command takes one argument, the size value"));
					}
					if (!isNumber(commands[2])) {
						return std::make_pair(instrumentExists, genError(commands[2] + " is not a number"));
					}
					sendSizeToPart(lastInstrumentIndex, stoi(commands[2]));
				}
				else if (commands[1].compare("numbars") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"numbars\" is a method, must be concatenated to instrument name with a dot"));
					}
					if (!sharedData.instruments[lastInstrumentIndex].sendToPart) {
						return std::make_pair(instrumentExists, genError("instrument doesn't have OSC set"));
					}
					if (commands.size() != 3) {
						return std::make_pair(instrumentExists, genError("\"numbars\" command takes one argument, the number of bars to display"));
					}
					if (!isNumber(commands[2])) {
						return std::make_pair(instrumentExists, genError(commands[2] + " is not a number"));
					}
					sendNumBarsToPart(lastInstrumentIndex, stoi(commands[2]));
				}
				else if (commands[1].compare("accoffset") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"accoffset\" is a method, must be concatenated to instrument name with a dot"));
					}
					if (!sharedData.instruments[lastInstrumentIndex].sendToPart) {
						return std::make_pair(instrumentExists, genError("instrument doesn't have OSC set"));
					}
					if (commands.size() != 3) {
						return std::make_pair(instrumentExists, genError("\"accoffset\" command takes one argument, the offset for accidentals that are close together"));
					}
					if (!isFloat(commands[2])) {
						return std::make_pair(instrumentExists, genError(commands[2] + " is not a number"));
					}
					sendAccOffsetToPart(lastInstrumentIndex, std::stof(commands[2]));

				}
				else if (commands[1].compare("delay") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"delay\" is a method, must be concatenated to instrument name with a dot"));
					}
					if (commands.size() != 3) {
						return std::make_pair(instrumentExists, genError("\"delay\" takes one argument, the time in milliseconds the instrument needs to sound its notes"));
					}
					if (!isNumber(commands[2]) && !isFloat(commands[2])) {
						return std::make_pair(instrumentExists, genError(commands[2] + " is not a number"));
					}
					double delayMs = std::stod(commands[2]);
					if (delayMs < 0) {
						return std::make_pair(instrumentExists, genError("the delay can't be negative"));
					}
					// the sequencer plays this instrument ahead of the score by its delay (see Sequencer::playDelayedInstrument())
					// so its delay is changed while the sequencer doesn't go through the instruments
					std::lock_guard<std::recursive_mutex> lock(sharedData.instrumentsMutex);
					Instrument& inst = sharedData.instruments[lastInstrumentIndex];
					int64_t newDelay = (int64_t)std::llround(delayMs * 1000.0); // in microseconds
					int64_t oldDelay = inst.getDelayTime();
					if (newDelay == oldDelay) return std::make_pair(instrumentExists, cmdOutput);
					if (sequencer.isThreadRunning()) {
						// while the sequencer plays, the instrument stops its notes, and starts again at the start of the next bar
						sequencer.silenceInstrument(inst);
						// without a delay it plays with the score again (see Sequencer::threadedFunction())
						if (newDelay == 0) inst.timeSynced = false;
					}
					inst.setDelay(newDelay);
				}
				else {
					if (parsingBars && barsIterCounter == 1 && !firstInstForBarsSet) {
						firstInstForBarsIndex = lastInstrumentIndex;
						firstInstForBarsSet = true;
					}
					std::pair<int, std::string> p = parseMelodicLine({commands.begin()+commandNdxOffset, commands.end()}, lineNum, numLines);
					// a line without errors shows the warning of a command in it, if there is one
					if (p.first == 0 && !melodicLineWarning.empty()) p = std::make_pair(2, melodicLineWarning);
					if (p.first > 0) return std::make_pair(instrumentExists, genOutputFuncs(p));
					else return std::make_pair(instrumentExists, genOutput(p.second));
				}
			}
			else {
				cmdOutput.outputVec = commands;
				return std::make_pair(instrumentExists, cmdOutput);
			}
		}
	}
	return std::make_pair(instrumentExists, cmdOutput);
}

//--------------------------------------------------------------
std::pair<bool, CmdOutput> ofApp::isBarLoop(std::vector<std::string>& originalCommands, bool isMainCmd, int lineNum, int numLines)
{
	bool barLoopExists = false;
	CmdOutput cmdOutput = CmdOutput();
	if (sharedData.loopsIndexes.size() > 0) {
		std::string barLoopName;
		if (originalCommands[0].find(".") != std::string::npos) barLoopName = originalCommands[0].substr(0, originalCommands[0].find("."));
		else barLoopName = originalCommands[0];
		if (sharedData.loopsIndexes.find(barLoopName) != sharedData.loopsIndexes.end()) {
			barLoopExists = true;
			int indexLocal = sharedData.loopsIndexes[barLoopName];
			// the initial command is \barloopname which might be followed by a method
			// to separate the method we create a new vector that will copy the originalCommands vector
			// except from the first item where we trim the \barloopname. part
			bool hasDot = false;
			std::vector<std::string> commands;
			for (unsigned i = 0; i < originalCommands.size(); i++) {
				if (!i) {
					// separate the name of the bar/loop from the method
					std::vector<std::string> tokens = tokenizeString(originalCommands[i], ".");
					for (unsigned j = 0; j < tokens.size(); j++) {
						commands.push_back(tokens[j]);
					}
					if (tokens.size() > 1) hasDot = true;
				}
				else {
					commands.push_back(originalCommands[i]);
				}
			}
			if (commands.size() > 1) {
				if (!hasDot) {
					// in certain cases we might use a bar name as an argument with more arguments following
					// in such a case we must ignore all following arguments that are not concatenated with a dot
					unsigned sizeBeforeDeleting = commands.size();
					for (unsigned j = 1; j < sizeBeforeDeleting; j++) {
						commands.pop_back();
					}
				}
				else if (sharedData.loopsIndexes.find(commands[0]) != sharedData.loopsIndexes.end() && sharedData.instrumentIndexes.find(commands[1]) != sharedData.instrumentIndexes.end()) {
					unsigned sizeBeforeDeleting = commands.size();
					for (unsigned j = 2; j < sizeBeforeDeleting; j++) {
						commands.pop_back();
					}
				}
			}
			// goto and locate have raw arguments (see CommandSpec): they take a bar or loop name as it is written, or a value, which is evaluated here
			// e.g. \myloop.goto \1, or \myloop.goto {\rand 1 4}
			if (commands.size() > 2 && (commands[1] == "goto" || commands[1] == "locate") && sharedData.loopsIndexes.find(commands[2]) == sharedData.loopsIndexes.end()) {
				std::vector<std::string> args = {commands.begin()+2, commands.end()};
				if (args.size() > 1 && args.front() == "{" && args.back() == "}") args = {args.begin()+1, args.end()-1};
				CmdOutput evaluated = evalArgs(args, lineNum, numLines);
				if (evaluated.errorCode == 3) return std::make_pair(barLoopExists, evaluated);
				commands.resize(2);
				commands.insert(commands.end(), evaluated.outputVec.begin(), evaluated.outputVec.end());
			}
			if (commands.size() > 1) {
				if (commands[1].compare("goto") == 0 || commands[1].compare("locate") == 0 || commands[1].compare("check") == 0) {
					if (!hasDot) {
						return std::make_pair(barLoopExists, genError("\"goto\", \"locate\", and \"check\" and instrument names as methods must be concatenated to the loop name with a dot"));
					}
					if ((commands[1].compare("goto") == 0 || commands[1].compare("locate") == 0) && sharedData.barsIndexes.find(commands[0]) != sharedData.barsIndexes.end()) {
						return std::make_pair(barLoopExists, genError("\"goto\" and \"locate\" methods are aimed at loops, not bars"));
					}
				}
				else if (sharedData.instrumentIndexes.find(commands[1]) == sharedData.instrumentIndexes.end()) {
					return std::make_pair(barLoopExists, genError("if methods or arguments are provided to a bar/loop, first must be \"goto\", \"locate\", \"check\", or name of instrument"));
				}
				if (commands.size() < 3 && commands[1].compare("goto") == 0) {
					return std::make_pair(barLoopExists, genError("\"goto\" takes one argument, the bar/loop-name/index to go to"));
				}
				if (commands.size() < 3 && commands[1].compare("locate") == 0) {
					return std::make_pair(barLoopExists, genError("\"locate\" takes one argument, the bar/loop name to locate"));
				}
				if (commands.size() == 3 && commands[1].compare("check") == 0 && !isNumber(commands[2])) {
					return std::make_pair(barLoopExists, genError("\"check\" takes an int as an argument"));
				}
				if (commands.size() == 3) {
					if (commands[1].compare("check") == 0) {
						sequencer.setCheckLoop(barLoopName, stoi(commands[2]));
					}
					else {
						if (sharedData.instrumentIndexes.find(commands[1]) != sharedData.instrumentIndexes.end()) {
							return std::make_pair(barLoopExists, genError("a bar/loop with an instrument name as a method takes no arguments"));
						}
						// find() and not [], which would add the argument to the loops, if it is not a bar or loop
						auto argIt = sharedData.loopsIndexes.find(commands[2]);
						auto it = (argIt == sharedData.loopsIndexes.end() ? sharedData.loopData[indexLocal].end() :
								find(sharedData.loopData[indexLocal].begin(), sharedData.loopData[indexLocal].end(), argIt->second));
						int gotoIndexOrBarName = 0;
						if (it == sharedData.loopData[indexLocal].end()) {
							// "locate" gives the index of a bar or loop, so it takes a name, and only "goto" takes an index
							if (isNumber(commands[2]) && commands[1].compare("locate") == 0) {
								return std::make_pair(barLoopExists, genError("\"locate\" takes the name of a bar or loop, not an index"));
							}
							if (isNumber(commands[2])) {
								size_t indexToGoto = stoi(commands[2]);
								if (indexToGoto < 1) {
									return std::make_pair(barLoopExists, genError("index to \"goto\" must be greater than 0"));
								}
								if (indexToGoto <= sharedData.loopData[indexLocal].size()) {
									sharedData.thisBarIndex = indexToGoto - 1; // argument is 1-based
									gotoIndexOrBarName = 1;
								}
								else {
									return std::make_pair(barLoopExists, genError("index to \"goto\" is greater than loop's size"));
								}
							}
							else {
								return std::make_pair(barLoopExists, genError(commands[2] + " doesn't exist in " + sharedData.loopsOrdered[indexLocal]));
							}
						}
						if (commands[1].compare("goto") == 0) {
							if (gotoIndexOrBarName == 0) sharedData.thisBarIndex = it - sharedData.loopData[indexLocal].begin();
							ofxOscMessage m;
							m.setAddress("/thisloopndx");
							m.addIntArg(sharedData.thisBarIndex);
							sendToParts(m, false);
							m.clear();
						}
						else {
							return std::make_pair(barLoopExists, genOutput(std::to_string(it - sharedData.loopData[indexLocal].begin() + 1)));
						}
					}
				}
				else if (commands.size() == 2) {
					if (commands[1].compare("check") != 0 && sharedData.instrumentIndexes.find(commands[1]) == sharedData.instrumentIndexes.end()) {
						return std::make_pair(barLoopExists, genError(commands[1] + ": unknown method to " + commands[0]));
					}
					if (commands[1].compare("check") == 0) {
						sequencer.setCheckLoop(barLoopName, 1);
					}
					else {
						// first check if this is a loop and not a bar
						if (sharedData.barsIndexes.find(barLoopName) == sharedData.barsIndexes.end()) {
							std::vector<std::string> v;
							for (int i : sharedData.loopData[sharedData.loopsIndexes[barLoopName]]) {
								v.push_back(sharedData.loopsOrdered.at(i) + "." + commands[1]);
								v.push_back("|");
							}
							// remove the last slash
							v.pop_back();
							return std::make_pair(barLoopExists, genOutput(v, true)); // a bar or loop name ignores what follows it, which stays in the line
						}
						std::vector<std::string> barLinesTokens = tokenizeString(sharedData.barLines[barLoopName], "\n");
						for (auto it = barLinesTokens.begin(); it != barLinesTokens.end(); ++it) {
							std::vector<std::string> instBarLinesTokens = tokenizeLine(*it);
							size_t instNameNdx = 0;
							// bar definitions have their lines indented by a horizontal tab written as single white spaces
							// these white spaces have been left out when the string is tokenized
							// below we determine which token in the vector to look at, which must be the first after these spaces
							if (instBarLinesTokens.size() > 0 && startsWith(instBarLinesTokens.at(0), "\t")) instNameNdx++;
							if (instBarLinesTokens.size() > 0 && instBarLinesTokens.at(0).substr(instNameNdx) == commands[1]) {
								std::vector<std::string> v = {instBarLinesTokens.begin()+1, instBarLinesTokens.end()};
								return std::make_pair(barLoopExists, genOutput(v, true)); // a bar or loop name ignores what follows it, which stays in the line
							}
						}
						return std::make_pair(barLoopExists, genError("couldn't find " + commands[1] + " in " + barLoopName));
					}
				}
			}
			else {
				if (isMainCmd) {
					// without any arguments and as a main command we're calling a loop to be played
					sharedData.tempLoopIndex = sharedData.loopsIndexes[commands[0]];
					if (sequencer.isThreadRunning()) {
						sequencer.update();
						mustUpdateScore = true;
						ofxOscMessage m;
						m.setAddress("/update");
						m.addIntArg(sharedData.tempLoopIndex);
						sendToParts(m, true);
						m.clear();
						// ties at the end of bars that are followed by other notes are ignored
						// (with the sequencer idle, this is checked when it starts, see \score.play)
						std::string warning = tieWarning(sharedData.tempLoopIndex);
						if (!warning.empty()) return std::make_pair(barLoopExists, genWarning(warning));
					}
					else {
						sharedData.loopIndex = sharedData.tempLoopIndex;
						sendLoopIndexToParts();
						// update the previous number of bars displayed and the previous position to properly display single bars in horizontal view
						sharedData.prevNumBars = sharedData.numBars;
						sharedData.prevPosition = sharedData.thisPosition;
					}
				}
				else {
					std::vector<std::string> v;
					// first check if this is a loop and not a bar
					if (sharedData.barsIndexes.find(barLoopName) == sharedData.barsIndexes.end()) {
						for (int i : sharedData.loopData[sharedData.loopsIndexes[barLoopName]]) {
							v.push_back(sharedData.loopsOrdered.at(i));
							v.push_back("|");
						}
						// remove the last slash
						v.pop_back();
					}
					else {
						v = tokenizeLine(sharedData.barLines[barLoopName]);
					}
					return std::make_pair(barLoopExists, genOutput(v, true)); // a bar or loop name ignores what follows it, which stays in the line
				}
			}
		}
	}
	return std::make_pair(barLoopExists, cmdOutput);
}

//--------------------------------------------------------------
std::pair<bool, CmdOutput> ofApp::isFunction(std::vector<std::string>& commands, int lineNum, int numLines)
{
	bool functionExists = false;
	bool executeFunction = false;
	CmdOutput cmdOutput = CmdOutput();
	// this function is called from both the main and the sequencer thread
	// so we copy what we need from the function while holding the lock
	// and then execute its body without holding it, as executing it calls parseString()
	int funcNdx = -1;
	std::string functionBody;
	std::vector<std::string> storedArgs;
	{
		std::lock_guard<std::mutex> lock(sharedData.functionsMutex);
		if (functionIndexes.size() > 0) {
			std::string functionName;
			if (commands[0].find(".") != std::string::npos) functionName = commands[0].substr(0, commands[0].find("."));
			else functionName = commands[0];
			if (functionIndexes.find(functionName) != functionIndexes.end()) {
				functionExists = true;
				executeFunction = true;
				lastFunctionIndex = functionIndexes[functionName];
				funcNdx = lastFunctionIndex;
				// functionFuncs() uses lastFunctionIndex, so it must be called while holding the lock
				cmdOutput = functionFuncs(commands);
				if (cmdOutput.errorCode > 0) return std::make_pair(functionExists, cmdOutput);
				if (!cmdOutput.outputVec.empty() && cmdOutput.outputVec[0].compare("noexec") == 0) executeFunction = false;
				if (executeFunction) {
					functionBody = sharedData.functions[funcNdx].printStr();
					for (int i = 0; i < sharedData.functions[funcNdx].getNumArgs(); i++) {
						storedArgs.push_back(sharedData.functions[funcNdx].getArgument(i));
					}
				}
			}
		}
	}
	if (functionExists && executeFunction) {
		// the return value of this call (see \return), which a function called inside its body doesn't change, as it has its own
		FunctionReturnFrame returnFrame;
		std::vector<std::string> functionLines = tokenizeString(functionBody, "\n");
		std::pair<int, std::string> p = std::make_pair(0, "");
		// the lines of the body with their $ arguments replaced
		std::vector<std::string> bodyLines;
		for (std::string line : functionLines) {
			// detect $ args and replace them with actual args
			// a $ arg is a $ followed by digits only, up to the next white space or the end of the line (e.g. $1 or $12)
			// $n is replaced by the n-th argument set with .setargs, if there are at least n such arguments,
			// otherwise by the n-th argument the function is called with (commands[n], as commands[0] is the function name),
			// otherwise by 0
			// $0 is not an argument and is left as is
			// the line is scanned once from left to right, so text inserted by an argument is never scanned again
			std::string lineWithArgs;
			size_t pos = 0;
			while (pos < line.size()) {
				if (line[pos] != '$') {
					lineWithArgs += line[pos++];
					continue;
				}
				size_t endNdx = line.find(" ", pos);
				if (endNdx == std::string::npos) endNdx = line.size();
				std::string digits = line.substr(pos+1, endNdx-pos-1);
				// limit the number of digits so that stoi() can't overflow
				if (!isNumber(digits) || digits[0] == '-' || digits.size() > 6 || stoi(digits) == 0) {
					lineWithArgs += line[pos++];
					continue;
				}
				size_t dollarVal = (size_t)stoi(digits);
				if (dollarVal <= storedArgs.size()) lineWithArgs += storedArgs[dollarVal-1];
				else if (dollarVal < commands.size()) lineWithArgs += commands[dollarVal];
				else lineWithArgs += "0";
				pos = endNdx;
			}
			bodyLines.push_back(lineWithArgs);
		}
		// split the body into chunks the same way the editor does (definitions like bars, and single lines)
		// and parse each chunk separately, so that each definition is finished before the next chunk is parsed
		for (const BracketChunk& chunk : Editor::findBracketChunksInLines(bodyLines)) {
			if (chunk.errorLine > -1) {
				return std::make_pair(functionExists, genError(chunk.errorStr + " in the body of the function"));
			}
			std::vector<std::pair<int, std::string>> chunkLines;
			for (int i = chunk.top; i <= chunk.bottom; i++) {
				// all the lines of a function body get the line number of the call, for tracebacks
				chunkLines.push_back(std::make_pair(lineNum, bodyLines[i]));
			}
			p = parseLines(chunkLines, numLines, true);
			if (p.first == 3) { // on error, return
				return std::make_pair(functionExists, genError(p.second));
			}
			// \return ends the function, and the lines after it are not run
			if (functionReturns.back().returned) break;
		}
		// the value given with \return, which replaces the function and its arguments when it is called inside a line
		bool hasReturnValue = functionReturns.back().returned;
		std::vector<std::string> returnValue = functionReturns.back().value;
		int onUnbindFuncNdx = -1;
		std::string onUnbindFuncName;
		{
			std::lock_guard<std::mutex> lock(sharedData.functionsMutex);
			// use find() instead of [], as the functions might have been cleared while the body was executing
			auto funcIt = sharedData.functions.find(funcNdx);
			if (funcIt != sharedData.functions.end()) {
				onUnbindFuncNdx = funcIt->second.incrementCallCounter();
				if (onUnbindFuncNdx > -1) {
					auto onUnbindIt = sharedData.functions.find(onUnbindFuncNdx);
					if (onUnbindIt != sharedData.functions.end()) onUnbindFuncName = onUnbindIt->second.getName();
					else onUnbindFuncNdx = -1;
				}
			}
		}
		if (onUnbindFuncNdx > -1) {
			CmdInput cmdInput = {{onUnbindFuncName}, std::vector<std::string>(), false, true};
			CmdOutput unbound = parseCommand(cmdInput, 1, 1);
			if (hasReturnValue && unbound.errorCode < 3) return std::make_pair(functionExists, genOutput(returnValue));
			return std::make_pair(functionExists, unbound);
		}
		else {
			if (hasReturnValue) return std::make_pair(functionExists, genOutput(returnValue));
			// if we're not unbinding the function, check if we get a note or warning so we can return these
			switch (p.first) {
				case 1:
					return std::make_pair(functionExists, genNote(p.second));
				case 2:
					return std::make_pair(functionExists, genWarning(p.second));
				default:
					break;
			}
		}
	}
	return std::make_pair(functionExists, cmdOutput);
}

//--------------------------------------------------------------
std::pair<bool, CmdOutput> ofApp::isList(std::vector<std::string>& originalCommands, int lineNum, int numLines)
{
	bool listExists = false;
	CmdOutput cmdOutput = CmdOutput();
	if (listIndexes.size() > 0) {
		std::string listName;
		if (originalCommands[0].find(".") != std::string::npos) listName = originalCommands[0].substr(0, originalCommands[0].find("."));
		else listName = originalCommands[0];
		if (listIndexes.find(listName) != listIndexes.end()) {
			listExists = true;
			lastListIndex = listIndexes[listName];
			// the initial command is listname which might be followed by a method
			// to separate the method we create a new vector that will copy the originalCommands vector
			// except from the first item where we trim the \listname. part
			bool hasDot = false;
			std::vector<std::string> commands;
			for (unsigned i = 0; i < originalCommands.size(); i++) {
				if (!i) {
					// separate the name of the list from the method
					std::vector<std::string> tokens = tokenizeString(originalCommands[i], ".");
					for (unsigned j = 0; j < tokens.size(); j++) {
						commands.push_back(tokens[j]);
					}
					if (tokens.size() > 1) hasDot = true;
				}
				else {
					commands.push_back(originalCommands[i]);
				}
			}
			if (commands.size() < 2) {
				return std::make_pair(listExists, cmdOutput);
			}
			if (commands[1].compare("traverse") == 0) {
				if (!hasDot) {
					return std::make_pair(listExists, genError("methods must be concatenated to the list name with a dot"));
				}
				// the lines to execute for every item are in curly brackets, in this line or in the lines below it
				std::string body = genStrFromVec({commands.begin()+2, commands.end()});
				size_t firstNonSpace = body.find_first_not_of(" \t");
				if (firstNonSpace == std::string::npos || body[firstNonSpace] != '{') {
					return std::make_pair(listExists, genError("\"traverse\" takes the lines to execute for every item in curly brackets"));
				}
				body = body.substr(firstNonSpace + 1);
				listIndexCounter = 0;
				traverseLines.clear();
				traverseDepth = 1 + curlyBracketBalance(body);
				if (traverseDepth > 0) {
					// the closing curly bracket is in a line below, so the lines are collected until then (see parseString())
					while (!body.empty() && isspace(body.front())) body.erase(0, 1);
					while (!body.empty() && isspace(body.back())) body.pop_back();
					if (!body.empty()) traverseLines.push_back(std::make_pair(lineNum, body));
					traversingList = true;
					return std::make_pair(listExists, cmdOutput);
				}
				// all in this line
				size_t closing = body.rfind('}');
				body = body.substr(0, closing);
				while (!body.empty() && isspace(body.back())) body.pop_back();
				if (!body.empty()) traverseLines.push_back(std::make_pair(lineNum, body));
				cmdOutput = traverseList(numLines);
				return std::make_pair(listExists, cmdOutput);
			}
			else if (isNumber(commands[1])) {
				std::pair<int, std::string> p = listItemExists(stoi(commands[1]));
				// the item (code 0) or the error of a wrong index
				return std::make_pair(listExists, listItemOutput(p, lineNum, numLines));
			}
			else {
				std::vector<std::string> v = {commands.begin()+1, commands.end()};
				cmdOutput = parseCommand(genCmdInput(v), lineNum, numLines);
				if (cmdOutput.errorCode > 0) {
					return std::make_pair(listExists, cmdOutput);
				}
				else {
					if (!isNumber(cmdOutput.outputVec[0])) {
						return std::make_pair(listExists, genError("a list can only be indexed with an integer"));
					}
					size_t listItemNdx = stoi(cmdOutput.outputVec[0]);
					std::pair<int, std::string> p = listItemExists(listItemNdx);
					// the item (code 0) or the error of a wrong index
					return std::make_pair(listExists, listItemOutput(p, lineNum, numLines));
				}
			}
		}
	}
	return std::make_pair(listExists, cmdOutput);
}

//--------------------------------------------------------------
CmdOutput ofApp::listItemOutput(std::pair<int, std::string> item, int lineNum, int numLines)
{
	// the items of a list are stored as they are written (see CommandSpec), so an item that is a command (e.g. a bar or an instrument name)
	// is evaluated when it is read, and gives its output (e.g. the lines of the bar), as if it were written in its place
	const std::string& str = item.second;
	bool isCommand = isCommandToken(str);
	// an item ignores what follows it, which stays in the line (see genOutput())
	if (item.first > 0) return genOutputFuncs(item);
	if (!isCommand) return genOutput(str, true);
	CmdOutput evaluated = evalArgs({str}, lineNum, numLines);
	evaluated.keepsArgs = true;
	return evaluated;
}

//--------------------------------------------------------------
std::pair<int, std::string> ofApp::listItemExists(size_t listItemNdx)
{
	if (listItemNdx < 1) {
		return std::make_pair(3, "lists are 1-based indexed, the first item is 1");
	}
	if (listItemNdx > listMap[lastListIndex].size()) {
		return std::make_pair(3, "list index out of range");
	}
	listItemNdx--; // LiveLily is 1-based counting
	auto lit = listMap[lastListIndex].begin();
	std::advance(lit, listItemNdx);
	return std::make_pair(0, *lit);
}

//--------------------------------------------------------------
std::pair<bool, CmdOutput> ofApp::isOscClient(std::vector<std::string>& originalCommands, int lineNum, int numLines)
{
	bool oscClientExists = false;
	CmdOutput cmdOutput = CmdOutput();
	if (oscClients.size() > 0) {
		std::string oscClientName;
		if (originalCommands[0].find(".") != std::string::npos) oscClientName = originalCommands[0].substr(0, originalCommands[0].find("."));
		else oscClientName = originalCommands[0];
		if (oscClients.find(oscClientName) != oscClients.end()) {
			oscClientExists = true;
			// the initial command is \oscclientname which might be followed by a method
			// to separate the method we create a new vector that will copy the originalCommands vector
			// except from the first item where we trim the \oscclientname. part
			bool hasDot = false;
			std::vector<std::string> commands;
			for (unsigned i = 0; i < originalCommands.size(); i++) {
				if (!i) {
					// separate the name of the OSC client from the method
					std::vector<std::string> tokens = tokenizeString(originalCommands[i], ".");
					for (unsigned j = 0; j < tokens.size(); j++) {
						commands.push_back(tokens[j]);
					}
					if (tokens.size() > 1) hasDot = true;
				}
				else {
					commands.push_back(originalCommands[i]);
				}
			}
			if (!hasDot) {
				return std::make_pair(oscClientExists, genError("OSC client name and method must be concatenated with a dot"));
			}
			if (commands.size() < 2) {
				return std::make_pair(oscClientExists, genWarning("no methods provided"));
			}
			bool sendingBar = false;
			for (unsigned i = 1; i < commands.size(); i++) {
				if (commands[i].compare("setup") == 0) {
					if (i > 1) {
						return std::make_pair(oscClientExists, genError("\"setup\" can't be combined with other commands"));
					}
					if (commands.size() > i+3) {
						return std::make_pair(oscClientExists, genError("\"setup\" takes two arguments maximum, host IP and port"));
					}
					if (commands.size() < i+2) {
						return std::make_pair(oscClientExists, genError("\"setup\" takes the port number, and optionally the host IP before it"));
					}
					std::string host;
					int port;
					if (commands.size() > i+2) {
						if (isNumber(commands[i+1]) || !isNumber(commands[i+2])) {
							return std::make_pair(oscClientExists, genError("when two arguments are provided to \"setup\", the first must be the IP address"));
						}
						host = commands[i+1];
						port = stoi(commands[i+2]);
					}
					else {
						if (isNumber(commands[i+1])) {
							host = "localhost";
							port = stoi(commands[i+1]);
						}
						else {
							return std::make_pair(oscClientExists, genError("if one argument is provided to \"setup\", this must be the port number"));
						}
					}
					oscClients[commands[0]].setup(host, port);
					return std::make_pair(oscClientExists, cmdOutput);
				}
				else if (commands[i].compare("send") == 0) {
					if (i > 1) {
						return std::make_pair(oscClientExists, genError("\"send\" can't be combined with other commands"));
					}
					if (commands.size() < i+2) {
						return std::make_pair(oscClientExists, genError("no OSC address specified, must begin with \"/\" and be inside quotes"));
					}
					if (isQuoted(commands[i+1]) && startsWith(commands[i+1], "\"/")) {
						if (commands.size() < i+3) {
							return std::make_pair(oscClientExists, genWarning("no data specified, not sending message"));
						}
						ofxOscMessage m;
						m.setAddress(commands[i+1].substr(1, commands[i+1].size()-2));
						// create an empty string to concatenate all vector items that are not numbers
						// but are not defined as strings either, in case they are missing quotes
						std::string oscStr = "";
						for (unsigned j = i+2; j < commands.size(); j++) {
							if (isNumber(commands[j]) && !sendingBar) {
								m.addIntArg(stoi(commands[j]));
							}
							else if (isFloat(commands[j]) && !sendingBar) {
								m.addFloatArg(stof(commands[j]));
							}
							else if (isQuoted(commands[j])) {
								m.addStringArg(commands[j].substr(1, commands[j].size()-2));
							}
							else {
								if (commands[j].compare("\\bar") == 0 || commands[j].compare("\\bars") == 0) {
									sendingBar = true;
								}
								if (!oscStr.empty()) oscStr += " ";
								oscStr += commands[j];
							}
						}
						// after running through all vector items, check if the string has been populated
						// if it has, store it
						if (!oscStr.empty()) m.addStringArg(oscStr);
						oscClients[commands[0]].sendMessage(m, false);
						m.clear();
					}
					else {
						return std::make_pair(oscClientExists, genError("no OSC address specified, must begin with \"/\" and be inside quotes"));
					}
				}
			}
		}
	}
	return std::make_pair(oscClientExists, cmdOutput);
}

//--------------------------------------------------------------
std::pair<bool, CmdOutput> ofApp::isOscServer(std::vector<std::string>& originalCommands, int lineNum, int numLines)
{
	bool oscServerExists = false;
	CmdOutput cmdOutput = CmdOutput();
	if (oscServers.size() > 0) {
		std::string oscServerName;
		if (originalCommands[0].find(".") != std::string::npos) oscServerName = originalCommands[0].substr(0, originalCommands[0].find("."));
		else oscServerName = originalCommands[0];
		if (oscServers.find(oscServerName) != oscServers.end()) {
			oscServerExists = true;
			// the initial command is \oscclientname which might be followed by a method
			// to separate the method we create a new vector that will copy the originalCommands vector
			// except from the first item where we trim the \oscclientname. part
			bool hasDot = false;
			std::vector<std::string> commands;
			for (unsigned i = 0; i < originalCommands.size(); i++) {
				if (!i) {
					// separate the name of the OSC client from the method
					std::vector<std::string> tokens = tokenizeString(originalCommands[i], ".");
					for (unsigned j = 0; j < tokens.size(); j++) {
						commands.push_back(tokens[j]);
					}
					if (tokens.size() > 1) hasDot = true;
				}
				else {
					commands.push_back(originalCommands[i]);
				}
			}
			if (!hasDot) {
				return std::make_pair(oscServerExists, genError("OSC server name and method must be concatenated with a dot"));
			}
			if (commands.size() < 2) {
				return std::make_pair(oscServerExists, genWarning("no methods provided"));
			}
			std::string command = commands.at(1);
			if (command.compare("setup") == 0) {
				if (commands.size() != 3) {
					return std::make_pair(oscServerExists, genError("\"setup\" takes one argument, the port number"));
				}
				if (!isNumber(commands.at(2))) {
					return std::make_pair(oscServerExists, genError("port argument must be a number"));
				}
				int port = std::stoi(commands.at(2));
				if (port < 1024) {
					return std::make_pair(oscServerExists, genError("port number can't be below 1024"));
				}
				// the port might be used by another program, or by another OSC server
				if (!oscServers[oscServerName].setup(port)) {
					return std::make_pair(oscServerExists, genError("could not listen to port " + std::to_string(port) + ", it might be used by another program"));
				}
			}
			else if (command.compare("map") == 0) {
				if (commands.size() < 4) {
					return std::make_pair(oscServerExists, genError("\"map\" takes two arguments, the OSC address and the function to map that address to"));
				}
				if (!isQuoted(commands.at(2))) {
					return std::make_pair(oscServerExists, genError("the OSC address is a string and must be quoted"));
				}
				if (!startsWith(commands.at(2), "\"/")) {
					return std::make_pair(oscServerExists, genError("the OSC address must start with a forward slash"));
				}
				std::string oscFuncName;
				{
					std::lock_guard<std::mutex> lock(sharedData.functionsMutex);
					auto funcIndexIt = functionIndexes.find("\\"+commands.at(3));
					if (funcIndexIt == functionIndexes.end()) {
						return std::make_pair(oscServerExists, genError(commands.at(3) + ": function doesn't exist"));
					}
					oscFuncName = sharedData.functions[funcIndexIt->second].getName();
				}
				// an address is mapped to one function, so mapping it again (e.g. executing the same line again) replaces the function
				std::string address = commands.at(2).substr(1, commands.at(2).size()-2);
				bool replaced = false;
				for (auto& mapping : oscServerFuncs[oscServerName]) {
					if (mapping.first == address) {
						mapping.second = oscFuncName;
						replaced = true;
					}
				}
				if (!replaced) oscServerFuncs[oscServerName].push_back(std::make_pair(address, oscFuncName));
			}
		}
	}
	return std::make_pair(oscServerExists, cmdOutput);
}

//--------------------------------------------------------------
std::pair<bool, CmdOutput> ofApp::isGroup(std::vector<std::string>& originalCommands, int lineNum, int numLines)
{
	bool groupExists = false;
	CmdOutput cmdOutput = CmdOutput();
	if (instGroups.size() > 0) {
		std::string groupName;
		if (originalCommands[0].find(".") != std::string::npos) groupName = originalCommands[0].substr(0, originalCommands[0].find("."));
		else groupName = originalCommands[0];
		if (instGroups.find(groupName) != instGroups.end()) {
			groupExists = true;
			// the initial command is \groupname which might be followed by a method
			// to separate the method we create a new vector that will copy the originalCommands vector
			// except from the first item where we trim the \groupname. part
			bool hasDot = false;
			std::vector<std::string> commands;
			for (unsigned i = 0; i < originalCommands.size(); i++) {
				if (!i) {
					// separate the name of the group from the method
					std::vector<std::string> tokens = tokenizeString(originalCommands[i], ".");
					for (unsigned j = 0; j < tokens.size(); j++) {
						commands.push_back(tokens[j]);
					}
					if (tokens.size() > 1) hasDot = true;
				}
				else {
					commands.push_back(originalCommands[i]);
				}
			}
			if (commands.size() == 1) {
				return std::make_pair(groupExists, genWarning("group called with no methods or arguments"));
			}
			for (std::string inst : instGroups[groupName]) {
				size_t cmdOffset = 1;
				std::vector<std::string> v;
				if (hasDot) {
					v.push_back(inst + "." + commands[1]);
					cmdOffset = 2;
				}
				else {
					v.push_back(inst);
				}
				if (cmdOffset < commands.size()) {
					v.insert(v.end(), commands.begin()+cmdOffset, commands.end());
				}
				std::pair<bool, CmdOutput> p = isInstrument(v, lineNum, numLines);
				if (p.second.errorCode == 3) {
					return p;
				}
				if (p.second.errorCode > cmdOutput.errorCode) {
					cmdOutput.errorCode = p.second.errorCode;
					cmdOutput.errorStr = p.second.errorStr;
				}
			}
		}
	}
	return std::make_pair(groupExists, cmdOutput);
}

//--------------------------------------------------------------
std::pair<bool, CmdOutput> ofApp::isVariable(std::vector<std::string>& originalCommands, bool isMainCmd, int lineNum, int numLines)
{
	bool varExists = false;
	CmdOutput cmdOutput = CmdOutput();
	if (variables.size() > 0) {
		std::string varName;
		if (originalCommands[0].find(".") != std::string::npos) varName = originalCommands[0].substr(0, originalCommands[0].find("."));
		else varName = originalCommands[0];
		if (variables.find(varName) != variables.end()) {
			varExists = true;
			// the initial command is \varname which might be followed by a method
			// to separate the method we create a new vector that will copy the originalCommands vector
			// except from the first item where we trim the \varname. part
			// for now variables don't have their own methods, so we don't need to check for a dot
			// this is left here for a possible future development
			bool hasDot = false;
			std::vector<std::string> commands;
			for (unsigned i = 0; i < originalCommands.size(); i++) {
				if (!i) {
					// separate the name of the group from the method
					std::vector<std::string> tokens = tokenizeString(originalCommands[i], ".");
					for (unsigned j = 0; j < tokens.size(); j++) {
						commands.push_back(tokens[j]);
					}
					if (tokens.size() > 1) hasDot = true;
				}
				else {
					commands.push_back(originalCommands[i]);
				}
			}
			// assigning a new value with \var = value, or \var.val = value, is the same as var = value
			// (e.g. inside a function, \var.val = $1), including arithmetic (e.g. \var.val = $1 * 2)
			if ((commands.size() >= 3 && !hasDot && commands[1] == "=") || (commands.size() >= 4 && hasDot && commands[1] == "val" && commands[2] == "=")) {
				size_t valueNdx = (hasDot ? 3 : 2);
				std::vector<std::string> assignment = {varName.substr(1), "="};
				assignment.insert(assignment.end(), commands.begin() + valueNdx, commands.end());
				CmdInput cmdInput = {assignment, std::vector<std::string>(), false, isMainCmd};
				return std::make_pair(varExists, parseCommand(cmdInput, lineNum, numLines));
			}
			std::vector<std::string> operations = {"*=", "/=", "+=", "-="};
			if ((commands.size() == 3 && !hasDot && variables[varName].varType == 0 && std::find(operations.begin(), operations.end(), commands[1]) != operations.end()) ||
					(commands.size() == 4 && hasDot && commands[1].compare("val") == 0 && std::find(operations.begin(), operations.end(), commands[2]) != operations.end())) {
				int commandNdx = commands.size() == 3 ? 1 : 2;
				auto it = std::find(operations.begin(), operations.end(), commands[commandNdx]);
				int operationType = std::distance(operations.begin(), it);
				std::string operand2 = commands[commandNdx+1];
				std::pair<std::pair<std::string, std::string>, std::pair<int, double>> p = getArithmeticResult(variables[varName].varStr, operand2, operationType);
				if (!p.first.first.empty()) {
					return std::make_pair(varExists, genError(p.first.first));
				}
				variables[varName].varStr = stripValFromResult(p);
				cmdOutput.outputVec.push_back(variables[varName].varStr);
			}
			else if (commands.size() == 3 && !hasDot && variables[varName].varType != 0 &&
					std::find(operations.begin(), operations.end(), commands[1]) != operations.end()) {
				return std::make_pair(varExists, genError("a structure has been assigned to " + varName + "; use " + varName + ".val for arithmetic operations"));
			}
			else {
				if (commands.size() == 2) {
					if (!hasDot) {
						if (commands[1].compare("}") == 0) {
							// in case a variable name is inside brackets (in case of calling it with \expand)
							// remove the closing bracket and treat it as if it is called without methods
							commands.pop_back();
							goto checkVarType;
						}
						return std::make_pair(varExists, genError("methods to variable must be concatenated with a dot"));
					}
					if (commands[1].compare("val") == 0) {
						cmdOutput.outputVec = {variables[varName].varStr};
						return std::make_pair(varExists, cmdOutput);
					}
					else {
						goto checkVarType;
					}
				}
				else {
				checkVarType:
					size_t ndxOffset = 1;
					// variable type 0 is simple variable
					if (!variables[varName].varType) {
						cmdOutput.outputVec.push_back(variables[varName].varStr);
					}
					else {
						// assemble the command changing the variable name with its value
						std::string mainCmd = "\\" + variables[varName].varStr;
						if (hasDot && commands.size() > 1) {
							mainCmd += ("." + commands[1]);
							ndxOffset = 2;
						}
						std::vector<std::string> v = {mainCmd};
						for (size_t i = ndxOffset; i < commands.size(); i++) v.push_back(commands[i]);
						std::pair<bool, CmdOutput> p;
						// variable types 1, 2, and 3 are bar, bars, and loop, respectively
						if (variables[varName].varType < 4) {
							p = isBarLoop(v, isMainCmd, lineNum, numLines);
						}
						else {
							switch (variables[varName].varType) {
								case 4:
									p = isFunction(v, lineNum, numLines);
									break;
								case 5:
									p = isList(v, lineNum, numLines);
									break;
							}
						}
						cmdOutput = p.second;
					}
				}
			}
		}
	}
	return std::make_pair(varExists, cmdOutput);
}
