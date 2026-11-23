#include <iostream>
#include <fstream>
#include <iterator> // std::make_reverse_iterator, see findMatchingBracket()
#include <algorithm>
#include <limits>
#include <utility> // to add pair and make_pair
#include <filesystem>
#include "editor.h"
#include "of_grid_painter.h"
#include "of_color.h"
#include <set>
#include "musicxml.h"
#include "midifile.h"
#include "ofApp.h"

//--------------------------------------------------------------
Editor::Editor()
{
	thisLang = 0; // 0 is livelily, 1 is python, and 2 is lua
	objID = 0;
	lineCount = 1;
	lineCountOffset = 0;
	cursorLineIndex = 0;
	cursorPos = 0;
	arrowCursorPos = 0;
	maxCharactersPerString = oldMaxCharactersPerString = 0;
	activeSession = true;
	// a pane becomes active with setActivity() (see ofApp::addPane())
	activity = false;

	executionStepPerMs = (float)EXECUTIONBRIGHTNESS / (float)(EXECUTIONDUR - EXECUTIONRAMPSTART);

	executingMultiple = false;
	highlightManyChars = false;
	highlightManyCharsStart = 0;

	ctrlPressed = false;
	shiftPressed = false;
	altPressed = false;
	dPressed = false;

	highlightBracket = false;
	bracketHighlightRectX = 0;
	bracketHighlightRectY = 0;

	highlightedCharIndex = 0;
	stringExceededWindow = false;

	quoteCounter = 0;
	curlyBracketCounter = 0;
	squareBracketCounter = 0;
	roundBracketCounter = 0;

	fileLoaded = false;
	convertedFile = false;
	tempFileLoaded = false;

	allLines[0].str = "";
	allLines[0].startPos = 0;
	allLines[0].tracebackStr = "";
	allLines[0].tracebackColor = 0;
	allLines[0].tracebackNumLines = 1;
	allLines[0].tracebackStrBreakPnt = false;
	tabStr = "";
	for (int i = 0; i < TABSIZE; i++) {
		tabStr += " ";
	}

	couldNotLoadFile = false;
	couldNotSaveFile = false;
	helpFileOpen = false;
	fileEdited = false;
	tempFileEdited = false;
	autobrackets = true;
	sendKeys = false;
	sendLines = false;
	fontLoaded = false;

	inserting = true;
	visualMode = false;
	typingShell = false;
	showShell = false;
	animationState = false;

	fromOscStr = "";
}

//--------------------------------------------------------------
void Editor::setID(int id)
{
	objID = id;
}

//--------------------------------------------------------------
int Editor::getID()
{
	return objID;
}

//--------------------------------------------------------------
void Editor::setSessionActivity(bool activity)
{
	activeSession = activity;
}

//--------------------------------------------------------------
bool Editor::getSessionActivity()
{
	return activeSession;
}

//--------------------------------------------------------------
void Editor::setSendKeys(int paneNdx)
{
	sendKeysPaneNdx = paneNdx;
	sendKeys = true;
}

//--------------------------------------------------------------
bool Editor::getSendKeys()
{
	return sendKeys;
}

//--------------------------------------------------------------
int Editor::getSendKeysPaneNdx()
{
	return sendKeysPaneNdx;
}

//--------------------------------------------------------------
void Editor::setSendLines(int paneNdx)
{
	sendLinesPaneNdx = paneNdx;
	sendLines = true;
}

//--------------------------------------------------------------
bool Editor::getSendLines()
{
	return sendLines;
}

//--------------------------------------------------------------
int Editor::getSendLinesPaneNdx()
{
	return sendLinesPaneNdx;
}

//--------------------------------------------------------------
void Editor::setLanguage(int langNdx)
{
	thisLang = langNdx;
}

//--------------------------------------------------------------
int Editor::getLanguage()
{
	return thisLang;
}

//--------------------------------------------------------------
void Editor::setAutocomplete(bool autocomp)
{
	autobrackets = autocomp;
}

//--------------------------------------------------------------
void Editor::setAutocompletion(bool state)
{
	autocompletion = state;
	if (!autocompletion) clearCompletions();
}

//--------------------------------------------------------------
void Editor::setPaneRow(int row)
{
	paneRow = row;
}

//--------------------------------------------------------------
void Editor::setPaneCol(int col)
{
	paneCol = col;
}

//--------------------------------------------------------------
int Editor::getPaneRow()
{
	return paneRow;
}

//--------------------------------------------------------------
int Editor::getPaneCol()
{
	return paneCol;
}

//--------------------------------------------------------------
std::string Editor::getLine(int lineNdx)
{
	std::map<int, line>::iterator it = allLines.find(lineNdx);
	if (it == allLines.end()) return "";
	else return it->second.str;
}

//--------------------------------------------------------------
int Editor::getCursorLineIndex()
{
	return cursorLineIndex;
}

//--------------------------------------------------------------
void Editor::setActivity(bool activity)
{
	Editor::activity = activity;
}

//--------------------------------------------------------------
bool Editor::isActive()
{
	return activity;
}

//--------------------------------------------------------------
int Editor::getNumTabsInStr(std::string str)
{
	size_t pos = 0;
	int numTabs = 0;
	while (str.substr(pos, TABSIZE).compare(tabStr) == 0) {
		pos += TABSIZE;
		numTabs++;
	}
	return numTabs;
}


//--------------------------------------------------------------
int Editor::scanBraces(const std::string& str, int depth)
{
	// returns the curly bracket depth after this line, given the depth at its start
	// a closing bracket without an opening one is ignored, so that a stray bracket doesn't affect the rest of the code
	// (it is reported as an error when the code is executed, see findUnmatchedBracket())
	for (char c : bracketsInLine(str)) {
		if (c == '{') depth++;
		else if (depth > 0) depth--;
	}
	return depth;
}

//--------------------------------------------------------------
int Editor::getBraceDepth(int lineNdx)
{
	// the curly bracket depth at the start of a line
	int depth = 0;
	for (auto it = allLines.begin(); it != allLines.end() && it->first < lineNdx; ++it) {
		depth = scanBraces(it->second.str, depth);
	}
	return depth;
}





//--------------------------------------------------------------
std::vector<BracketChunk> Editor::findBracketChunks(int firstLine, int lastLine)
{
	// splits the LiveLily code of the editor between firstLine and lastLine (inclusive) into chunks
	// see findBracketChunksInLines() for how chunks are defined
	std::vector<int> lineIndexes;
	std::vector<std::string> lines;
	for (auto it = allLines.lower_bound(firstLine); it != allLines.end() && it->first <= lastLine; ++it) {
		lineIndexes.push_back(it->first);
		lines.push_back(it->second.str);
	}
	std::vector<BracketChunk> chunks = findBracketChunksInLines(lines);
	// convert the indexes of the lines vector to line indexes of the editor
	for (BracketChunk& chunk : chunks) {
		chunk.top = lineIndexes[chunk.top];
		chunk.bottom = lineIndexes[chunk.bottom];
		if (chunk.errorLine > -1) chunk.errorLine = lineIndexes[chunk.errorLine];
	}
	return chunks;
}

//--------------------------------------------------------------
int Editor::findUnmatchedBracket(int firstLine, int numLines, std::string& errorStr)
{
	// checks the curly brackets of the given lines
	// returns the index of the line with the first unmatched bracket and sets errorStr, or returns -1 if there is none
	for (const BracketChunk& chunk : findBracketChunks(firstLine, firstLine + numLines - 1)) {
		if (chunk.errorLine > -1) {
			errorStr = chunk.errorStr;
			return chunk.errorLine;
		}
	}
	return -1;
}

//--------------------------------------------------------------
std::pair<int, int> Editor::findBraceChunk()
{
	// LiveLily code: find the chunk the cursor is in
	// empty lines and comments outside a definition are single line chunks
	for (const BracketChunk& chunk : findBracketChunks(allLines.begin()->first, allLines.rbegin()->first)) {
		if (chunk.top <= cursorLineIndex && cursorLineIndex <= chunk.bottom) {
			// inside a definition that is not finished, lines without unmatched brackets run on their own
			// (e.g. "\osc pd" below an unfinished bar), only its first line and lines with unmatched brackets
			// execute the definition, which gives an error
			if (!chunk.finished && cursorLineIndex != chunk.top) {
				int openBrackets, closingBrackets;
				countUnmatchedBrackets(allLines[cursorLineIndex].str, openBrackets, closingBrackets);
				if (openBrackets == 0 && closingBrackets == 0) break;
			}
			return std::make_pair(chunk.top, chunk.bottom);
		}
		if (chunk.top > cursorLineIndex) break;
	}
	return std::make_pair(cursorLineIndex, cursorLineIndex);
}

//--------------------------------------------------------------
bool Editor::isBlankOrComment(const std::string& str)
{
	size_t pos = str.find_first_not_of(" \t");
	if (pos == std::string::npos) return true;
	std::string commentStr = (thisLang == 2 ? "--" : "#");
	return str.compare(pos, commentStr.size(), commentStr) == 0;
}

//--------------------------------------------------------------
bool Editor::isIndentedLine(const std::string& str)
{
	return !isBlankOrComment(str) && (str[0] == ' ' || str[0] == '\t');
}

//--------------------------------------------------------------
bool Editor::isDecoratorLine(const std::string& str)
{
	return thisLang == 1 && startsWith(str, "@");
}

//--------------------------------------------------------------
bool Editor::continuesPreviousBlock(const std::string& str)
{
	// Python lines that are not indented but belong to the block above them, like "else:" after an "if" block
	if (thisLang != 1) return false;
	const std::string keywords[4] = {"else", "elif", "except", "finally"};
	for (const std::string& keyword : keywords) {
		if (startsWith(str, keyword)) {
			// make sure the keyword is not just the start of a longer name, like "elsewhere = 1"
			if (str.size() == keyword.size()) return true;
			char next = str[keyword.size()];
			if (!(isalnum(next) || next == '_')) return true;
		}
	}
	return false;
}

//--------------------------------------------------------------
bool Editor::scanPythonLine(const std::string& str, int& brackets, std::string& tripleQuote)
{
	// scans a line of Python code, updating the number of open brackets and whether we are inside a triple-quoted string
	// returns true if the next line continues the statement of this line
	// (because of an open bracket, an open triple-quoted string, or a backslash at the end of this line)
	size_t i = 0;
	bool inComment = false;
	while (i < str.size()) {
		if (!tripleQuote.empty()) {
			// inside a triple-quoted string, only its closing quotes matter (a backslash escapes the next character)
			if (str[i] == '\\') i += 2;
			else if (str.compare(i, 3, tripleQuote) == 0) {
				tripleQuote.clear();
				i += 3;
			}
			else i++;
			continue;
		}
		char c = str[i];
		if (c == '#') {
			inComment = true;
			break;
		}
		if (c == '"' || c == '\'') {
			std::string threeQuotes(3, c);
			if (str.compare(i, 3, threeQuotes) == 0) {
				tripleQuote = threeQuotes;
				i += 3;
				continue;
			}
			// a single-quoted string ends at the matching quote, or at the end of the line
			i++;
			while (i < str.size() && str[i] != c) {
				if (str[i] == '\\') i++;
				i++;
			}
			i++; // skip the closing quote
			continue;
		}
		if (c == '(' || c == '[' || c == '{') brackets++;
		else if ((c == ')' || c == ']' || c == '}') && brackets > 0) brackets--;
		i++;
	}
	bool backslashAtEnd = (!inComment && !str.empty() && str.back() == '\\');
	return (!tripleQuote.empty() || brackets > 0 || backslashAtEnd);
}

//--------------------------------------------------------------
std::map<int, bool> Editor::findContinuationLines()
{
	// for each line, whether it continues the statement of the line above it
	// such lines belong to the statement above, no matter their indentation
	// and they are never empty lines or comments, even if they look like it (e.g. inside a triple-quoted string)
	std::map<int, bool> continuation;
	int brackets = 0;
	std::string tripleQuote;
	bool nextIsContinuation = false;
	for (auto it = allLines.begin(); it != allLines.end(); ++it) {
		continuation[it->first] = nextIsContinuation;
		// only Python is scanned, for Lua every line is a statement of its own
		if (thisLang == 1) nextIsContinuation = scanPythonLine(it->second.str, brackets, tripleQuote);
	}
	return continuation;
}

//--------------------------------------------------------------
std::pair<int, int> Editor::findIndentChunk()
{
	// Python code: a chunk is a line that is not indented, with all the indented lines below it
	// empty lines and comments don't end a chunk, so functions and classes can include empty lines
	// lines that continue a statement (inside brackets, triple-quoted strings, or after a backslash) count as indented
	int firstLine = allLines.begin()->first;
	int lastLine = allLines.rbegin()->first;
	std::map<int, bool> continuation = findContinuationLines();
	auto lineStr = [this](int ndx) -> std::string {
		auto it = allLines.find(ndx);
		return (it == allLines.end() ? "" : it->second.str);
	};
	auto isCont = [&continuation](int ndx) {
		auto it = continuation.find(ndx);
		return (it != continuation.end() && it->second);
	};
	auto isBlank = [&](int ndx) { return !isCont(ndx) && isBlankOrComment(lineStr(ndx)); };
	auto isIndented = [&](int ndx) { return isCont(ndx) || isIndentedLine(lineStr(ndx)); };
	auto continuesBlock = [&](int ndx) { return !isCont(ndx) && continuesPreviousBlock(lineStr(ndx)); };
	auto isDecorator = [&](int ndx) { return !isCont(ndx) && isDecoratorLine(lineStr(ndx)); };
	if (isBlank(cursorLineIndex)) {
		// an empty line or a comment is part of a chunk only if there is code above it and the next code line is indented
		int below = cursorLineIndex + 1;
		while (below <= lastLine && isBlank(below)) below++;
		int above = cursorLineIndex - 1;
		while (above >= firstLine && isBlank(above)) above--;
		if (below > lastLine || above < firstLine || !(isIndented(below) || continuesBlock(below))) {
			return std::make_pair(cursorLineIndex, cursorLineIndex);
		}
	}
	// go up to the line that starts the chunk, skipping empty lines, comments, indented lines, and lines like "else:"
	int top = cursorLineIndex;
	while (top > firstLine) {
		if (!isBlank(top) && !isIndented(top) && !continuesBlock(top)) break;
		top--;
	}
	// in case the code starts with indented lines, skip any empty lines and comments at the top
	while (top < cursorLineIndex && isBlank(top)) top++;
	// include any decorators above the chunk
	while (top > firstLine && isDecorator(top-1)) top--;
	// if the cursor is on a decorator, the chunk continues to the decorated definition
	int bottom = cursorLineIndex;
	while (bottom < lastLine && isDecorator(bottom)) bottom++;
	// go down to the last line of the chunk, which is the last indented line (or line like "else:")
	// before the next line that is not indented, leaving out any empty lines and comments at the end
	for (int i = bottom + 1; i <= lastLine; i++) {
		if (isBlank(i)) continue;
		if (isIndented(i) || continuesBlock(i)) bottom = i;
		else break;
	}
	return std::make_pair(top, bottom);
}

//--------------------------------------------------------------
std::pair<int, int> Editor::findExecutingChunk()
{
	if (allLines.empty() || allLines.find(cursorLineIndex) == allLines.end()) {
		return std::make_pair(cursorLineIndex, cursorLineIndex);
	}
	if (thisLang == 0) return findBraceChunk();
	return findIndentChunk();
}

//--------------------------------------------------------------
int Editor::commentStartOf(const std::string& str)
{
	// where a comment starts in a line, or the length of the line if it has none
	// in LiveLily a comment starts at the first %, as the parser removes everything from there (see ofApp::parseString())
	// in Python it starts at a # that is not inside a string
	if (thisLang == 0) {
		size_t pos = str.find('%');
		return (pos == std::string::npos ? (int)str.size() : (int)pos);
	}
	if (thisLang == 1) {
		char quote = 0;
		for (size_t i = 0; i < str.size(); i++) {
			char c = str[i];
			if (quote) {
				if (c == quote && (i == 0 || str[i-1] != '\\')) quote = 0;
			}
			else if (c == '"' || c == '\'') quote = c;
			else if (c == '#') return (int)i;
		}
	}
	return (int)str.size();
}

//--------------------------------------------------------------
char Editor::bracketAt(const std::string& str, int pos, int commentStart)
{
	// the bracket at a position of a line, or 0 if there is no bracket there, or the position is in a comment
	// round, square and curly brackets, and in LiveLily angle brackets (of chords), except those of hairpins (\< and \>)
	// and of accents (->), as the parser does (see ofApp::tokenizeLine())
	if (pos < 0 || pos >= commentStart || pos >= (int)str.size()) return 0;
	char c = str[pos];
	if (c == '{' || c == '}' || c == '[' || c == ']' || c == '(' || c == ')') return c;
	if (thisLang == 0) {
		if (c == '<' && !(pos > 0 && str[pos-1] == '\\')) return c;
		if (c == '>' && !(pos > 0 && (str[pos-1] == '\\' || str[pos-1] == '-'))) return c;
	}
	return 0;
}

//--------------------------------------------------------------
bool Editor::findMatchingBracket(int lineNdx, int pos, int& matchLine, int& matchPos)
{
	// finds the bracket that matches the one at a position of a line, forwards for an opening bracket and backwards for a closing one
	// brackets of the same kind in between are counted, so that nested pairs are skipped (e.g. the { of { { } } matches the last })
	// positions are in the line, and not on the screen
	auto lineIt = allLines.find(lineNdx);
	if (lineIt == allLines.end() || pos < 0 || pos >= (int)lineIt->second.str.size()) return false;
	char bracket = bracketAt(lineIt->second.str, pos, commentStartOf(lineIt->second.str));
	if (bracket == 0) return false;
	const std::string openings = "{[(<";
	const std::string closings = "}])>";
	size_t kind = openings.find(bracket);
	bool forward = (kind != std::string::npos);
	if (!forward) kind = closings.find(bracket);
	if (kind == std::string::npos) return false;
	char opening = openings[kind];
	char closing = closings[kind];
	// the search stops after this many lines, so that a bracket without a match doesn't make every frame scan a very long file
	const int maxLines = 5000;
	int depth = 0;
	if (forward) {
		int startPos = pos + 1;
		for (auto it = lineIt; it != allLines.end() && it->first - lineNdx <= maxLines; ++it) {
			const std::string& str = it->second.str;
			int commentStart = commentStartOf(str);
			for (int i = (it == lineIt ? startPos : 0); i < commentStart; i++) {
				char c = bracketAt(str, i, commentStart);
				if (c == opening) depth++;
				else if (c == closing) {
					if (depth == 0) {
						matchLine = it->first;
						matchPos = i;
						return true;
					}
					depth--;
				}
			}
		}
	}
	else {
		auto it = std::make_reverse_iterator(std::next(lineIt));
		for (; it != allLines.rend() && lineNdx - it->first <= maxLines; ++it) {
			const std::string& str = it->second.str;
			int commentStart = commentStartOf(str);
			// signed positions, so that the loop ends below 0, also for empty lines
			for (int i = std::min(it->first == lineNdx ? pos - 1 : (int)str.size() - 1, commentStart - 1); i >= 0; i--) {
				char c = bracketAt(str, i, commentStart);
				if (c == closing) depth++;
				else if (c == opening) {
					if (depth == 0) {
						matchLine = it->first;
						matchPos = i;
						return true;
					}
					depth--;
				}
			}
		}
	}
	return false;
}

//--------------------------------------------------------------
int Editor::getNumDigitsOfLineCount()
{
	int numDigits = 0;
	int timesTen = 1;
	while (timesTen <= lineCount) {
		timesTen *= 10;
		numDigits++;
	}
	return numDigits;
}

//--------------------------------------------------------------
void Editor::drawText()
{
	updateModeSelection();
	static int prevCursorPos = 0;
	static char stringChar = '"'; // to be able to identify strings in Python with either single of double quotes
	/*******************************************************************/
	// without wrapping, keep the line of the cursor visible, and don't scroll past the end of the text
	// the number of visible lines changes when the area of the error messages below the panes grows or shrinks
	// (e.g. executing code with Shift+Return can print errors of many lines), and the cursor can jump by many lines
	// so this is checked before every frame, rather than everywhere the cursor or the number of lines changes
	// (with wrapping, scrollToCursorWrapped() does this, see below)
	if (!wrapLines && lineCount > 0 && allLines.find(cursorLineIndex) != allLines.end()) {
		// a long line scrolls horizontally (startPos), and if it gets shorter (e.g. deleting characters in it)
		// it scrolls back so that its end is not before the end of the pane, and the cursor stays on the same character
		line& cursorLine = allLines[cursorLineIndex];
		int maxStartPos = std::max(0, (int)cursorLine.str.size() - maxCharactersPerString);
		if (cursorLine.startPos > maxStartPos) {
			cursorPos += cursorLine.startPos - maxStartPos;
			cursorLine.startPos = maxStartPos;
		}
		if (cursorLine.startPos < 0) cursorLine.startPos = 0;
		if (cursorPos > maxCursorPos()) cursorPos = maxCursorPos();
		if (cursorPos < 0) cursorPos = 0;
	}
	if (!wrapLines && maxNumLines > 0) {
		// not scrolling past the end of the text is checked with the full height of the pane (without the area of the error messages
		// or of the shell, see offsetMaxNumLines()), so that when that area grows and shrinks again, the text returns to where it was
		// and it is checked first, so that the line of the cursor stays visible when that area makes the pane shorter
		int fullNumLines = std::max(maxNumLines, maxNumLinesReset - 1);
		lineCountOffset = std::min(lineCountOffset, std::max(0, lineCount - fullNumLines));
		if (cursorLineIndex >= lineCountOffset + maxNumLines) lineCountOffset = cursorLineIndex - maxNumLines + 1;
		if (cursorLineIndex < lineCountOffset) lineCountOffset = cursorLineIndex;
		if (lineCountOffset < 0) lineCountOffset = 0;
	}
	/*******************************************************************/
	// the hint of the arguments of a Python callable (see argHintText()) is shown in the text, between the cursor and the rest of its line
	// so while the text is drawn, the line of the cursor has as many spaces there as the hint has characters, which move the rest of the line
	// (and with wrapping, the rows after it), and the hint is drawn in these spaces after the text (see below)
	// the spaces are removed when drawing is done, so they are never part of the text
	std::string argHint = (activity ? argHintText() : std::string());
	int argHintPos = cursorPos + allLines[cursorLineIndex].startPos;
	if (argHintPos < 0 || argHintPos > (int)allLines[cursorLineIndex].str.size()) argHint.clear();
	struct ArgHintSpaces {
		std::string& str;
		int pos;
		size_t length;
		~ArgHintSpaces() { if (length > 0) str.erase(pos, length); }
	} argHintSpaces{allLines[cursorLineIndex].str, argHintPos, argHint.size()};
	if (!argHint.empty()) allLines[cursorLineIndex].str.insert(argHintPos, argHint.size(), ' ');
	/*******************************************************************/
	// get the offset of the line count
	int loopIter;
	if (lineCount < maxNumLines) loopIter = lineCount;
	else loopIter = maxNumLines + lineCountOffset; // + 1;
	// and the width of the rectangle for highlighting a line
	int rectWidth = frameWidth - (lineNumberWidth + oneAndHalfCharacterWidth);
	// with wrapped lines, a line takes as many rows as it needs, so the row of the pane where every visible line starts is stored
	// (a line longer than the whole pane can start above the top of the pane)
	// without wrapping, every line takes one row
	std::map<int, int> wrapFirstRows;
	if (wrapLines) {
		scrollToCursorWrapped();
		int row = -wrapRowSkip;
		int i = lineCountOffset;
		while (i < lineCount && row < maxNumLines) {
			wrapFirstRows[i] = row;
			row += wrapRowsOf(i);
			i++;
		}
		loopIter = i;
	}
	auto firstRowOf = [&](int lineNdx) {
		if (!wrapLines) return lineNdx - lineCountOffset;
		auto it = wrapFirstRows.find(lineNdx);
		return (it != wrapFirstRows.end() ? it->second : maxNumLines);
	};
	// the rows of a line that are visible in the pane, from its first visible row
	auto visibleRows = [&](int lineNdx, int& top, int& numRows) {
		int first = firstRowOf(lineNdx);
		int last = std::min(maxNumLines, first + (wrapLines ? wrapRowsOf(lineNdx) : 1));
		top = std::max(0, first);
		numRows = std::max(0, last - top);
	};

	/*******************************************************************/
	// get the number of digits of the line count
	int numDigits = getNumDigitsOfLineCount();
	lineNumberWidth = numDigits * oneCharacterWidth;

	/*******************************************************************/
	// the text area of the pane and its line numbers are filled into grids of cells (see cellgrid.h), in the layers they are painted in
	// and painted at the end (see of_grid_painter.h), which gives the same picture as drawing them directly
	// a line that isn't wrapped shows one more character than maxCharactersPerString
	textGrid.resize(std::max(0, maxNumLines), std::max(0, wrapLines ? wrapWidth : maxCharactersPerString + 1));
	gutterGrid.resize(std::max(0, maxNumLines), numDigits);
	ofApp* app = (ofApp*)ofGetAppPtr();
	Color foreground = toColor(app->foregroundColor);
	Color background = toColor(app->backgroundColor);
	// the color the next text is drawn in, as the editor set it with ofSetColor() before drawing a token
	Color currentColor = foreground;

	/*******************************************************************/
	// then draw the red/orange traceback rectangles
	int count = 0; // separate counter to start from 0 because i starts from lineCountOffset
	for (int i = lineCountOffset; i < loopIter; i++) {
		if (allLines[i].tracebackStr.size() > 0 && allLines[i].tracebackColor > 0) {
			GridRect rect;
			rect.color = (allLines[i].tracebackColor == 1 ? Colors::orange : Colors::red);
			visibleRows(i, rect.row, rect.numRows);
			rect.toEnd = true;
			textGrid.backgrounds.push_back(rect);
		}
		count++;
	}

	/*******************************************************************/
	// then check if we're executing any lines and draw the execution rectangle
	// needs to be first otherwise the alpha blending won't work as expected
	for (int i = lineCountOffset; i < loopIter; i++) {
		if (allLines[i].isBeingExecuted) {
			GridRect rect;
			rect.color = Colors::cyan;
			rect.alpha = allLines[i].executionDegrade;
			visibleRows(i, rect.row, rect.numRows);
			// it reaches the edge of the pane (rectWidth, see the geometry of the grid below)
			rect.toEnd = true;
			textGrid.backgrounds.push_back(rect);
			if ((ofGetElapsedTimeMillis() - allLines[i].executionTimeStamp) >= EXECUTIONRAMPSTART) {
				// calculate how many steps the brightness has to dim, depending on the elapsed time
				// and the step per millisecond
				int brightnessDegrade = (int)((ofGetElapsedTimeMillis() - (allLines[i].executionTimeStamp+EXECUTIONRAMPSTART)) * executionStepPerMs);
				allLines[i].executionDegrade = EXECUTIONBRIGHTNESS - brightnessDegrade;
				if (allLines[i].executionDegrade < 0) {
					allLines[i].executionDegrade = 0;
					allLines[i].isBeingExecuted = false;
				}
			}
		}
	}
	// check for any execution rectangle that is not visible that might be left hanging
	// in case of executing bars quickly, and zero their booleans and ramp counters
	for (size_t i = 0; i < allLines.size(); i++) {
		if (allLines[i].isBeingExecuted && ((int)i < lineCountOffset || (int)i >= lineCountOffset + loopIter)) {
			allLines[i].executionDegrade = 0;
			allLines[i].isBeingExecuted = false;
		}
	}

	/******************************************************************/
	// then draw the rectangle that highlights a bracket
	highlightBracket = false;
	if (activity) {
		// the position of the cursor in its line, as the cursor position is relative to how much the line has scrolled
		int posInLine = cursorPos + allLines[cursorLineIndex].startPos;
		if (posInLine < (int)allLines[cursorLineIndex].str.size()) {
			// a bracket in a comment, or the angle bracket of a hairpin, is not highlighted (see bracketAt())
			const std::string& cursorLineStr = allLines[cursorLineIndex].str;
			bool onBracket = (bracketAt(cursorLineStr, posInLine, commentStartOf(cursorLineStr)) != 0);
			// highlightBracket is also used when pressing return between two curly brackets, see assembleString()
			// so it is true only if autocompletion of brackets is on, as it was before
			if (onBracket && autobrackets) highlightBracket = true;
			int matchLine, matchPos;
			if (onBracket && autobrackets && findMatchingBracket(cursorLineIndex, posInLine, matchLine, matchPos)) {
				// the matching bracket is highlighted only if it is visible
				int row = -1, col = -1;
				if (matchLine >= lineCountOffset && matchLine < loopIter) {
					if (wrapLines) {
						row = firstRowOf(matchLine) + matchPos / wrapWidth;
						col = matchPos % wrapWidth;
					}
					else {
						row = firstRowOf(matchLine);
						col = matchPos - (matchLine == cursorLineIndex ? allLines[matchLine].startPos : 0);
						if (col > maxCharactersPerString) col = -1;
					}
				}
				if (row >= 0 && row < maxNumLines && col >= 0) {
					bracketHighlightRectX = lineNumberWidth + oneAndHalfCharacterWidth + frameXOffset + (col * oneCharacterWidth);
					bracketHighlightRectY = (row * cursorHeight) + frameYOffset;
					GridRect rect;
					rect.row = row;
					rect.col = col;
					rect.color = Colors::magenta;
					rect.snap = true;
					textGrid.backgrounds.push_back(rect);
				}
			}
		}
	}

	/*******************************************************************/
	// then the actual text
	std::string strOnCursorLine;
	int strXOffset = lineNumberWidth + oneAndHalfCharacterWidth + frameXOffset;
	// the extra counter below starts from 0, whereas i starts from lineCountOffset
	count = 0;
	// we'll get the color of the text where the cursor is
	// for this reason, we need to calculate the X position of the cursor here, and not below
	// where we draw the cursor
	Color cursorColor = foreground;
	// the visible part of the line before the cursor, from how far the line has scrolled
	std::string beforeCursorStr = allLines[cursorLineIndex].str.substr(std::min((size_t)allLines[cursorLineIndex].startPos, allLines[cursorLineIndex].str.size()), std::max(0, cursorPos));
	int cursorX = lineNumberWidth + font.stringWidth(beforeCursorStr) + oneAndHalfCharacterWidth + frameXOffset;
	// font.stringWidth() returns +1 when the string ends with a white space
	// so we subtract the length of one character, if this is the case
	{
		// the character before the cursor, at its position in the line (the cursor position is relative to how far the line has scrolled)
		if (!beforeCursorStr.empty() && beforeCursorStr.back() == ' ') cursorX -= oneCharacterWidth;
	}
	cursorScreenRow = cursorLineIndex - lineCountOffset;
	if (wrapLines) {
		// the cursor is in the row of its line where its position falls
		std::pair<int, int> rowCol = wrapRowCol(cursorLineIndex, cursorPos);
		cursorX = strXOffset + (rowCol.second * oneCharacterWidth);
		cursorScreenRow = firstRowOf(cursorLineIndex) + rowCol.first;
	}
	bool cursorDrawn = false;
	// the selected text, drawn in black on goldenrod after the text of its row
	auto addSelection = [&](int row, int col, const std::string& text, bool snap) {
		GridSpan span;
		span.row = row;
		span.col = col;
		span.text = text;
		span.background = Colors::goldenRod;
		span.textColor = Color(0, 0, 0);
		span.snap = snap;
		textGrid.selections.push_back(span);
	};
	for (int i = lineCountOffset; i < loopIter; i++) {
		if (i == cursorLineIndex) strOnCursorLine = allLines[i].str;
		// draw all strings that are not empty
		if (allLines[i].str.size() > 0) {
			// iterate through each word to check for keywords
			// set last argument to tokenizeString to true to include the delimiters
			std::vector<std::string> tokens = tokenizeString(allLines[i].str, delimiters[thisLang], true);
			int xOffset = strXOffset;
			if (cursorLineIndex == i) xOffset -= (allLines[i].startPos * oneCharacterWidth);
			bool isComment = false;
			bool isString = false;
			bool isCursorInsideKeyword = false;
			int charAccum = 0;
			int quotesCounter = 0;
			// the cursor drawn in the color of the token it is on, before the token, which is drawn over it (see GridCursor::splitCol)
			auto cursorInRow = [&](Color color) {
				int splitCol;
				if (wrapLines) {
					int tokenRow = firstRowOf(i) + charAccum / wrapWidth;
					splitCol = (tokenRow == cursorScreenRow ? charAccum % wrapWidth : 0);
				}
				else {
					splitCol = charAccum - allLines[i].startPos;
				}
				markCursor(cursorX, color, true, splitCol);
				currentColor = color;
			};
			size_t subtokensSizeAccum = 0;
			if (tokens.size() == 0) {
				isComment = false;
			}
			bool tokenizeDigits1 = false;
			if (thisLang == 0) {
				// the main ofApp stores the following LiveLily commands: \insts, \bar, \bars, \loop, \function, \list and \group
				// we use this to determine if we should further tokenize each token of our string
				// if a line starts with one of these commands, then we won't tokenize the token further
				tokenizeDigits1 = find(((ofApp*)ofGetAppPtr())->session.commandsToNotTokenize.begin(), ((ofApp*)ofGetAppPtr())->session.commandsToNotTokenize.end(), tokens[0]) == ((ofApp*)ofGetAppPtr())->session.commandsToNotTokenize.end();
			}
			for (auto it = tokens.begin(); it != tokens.end(); ++it) {
				// tokenizing has already been done, but to separate digits properly
				// we need to tokenize these further, as a string like "c''4" won't be tokenized properly
				std::vector<std::string> subtokens;
				bool tokenizeDigits2 = false;
				if (thisLang == 0) {
					// the main ofApp also stores all the bar, loop, function and list names to the keywords vector
					tokenizeDigits2 = find(((ofApp*)ofGetAppPtr())->session.keywords.begin(), ((ofApp*)ofGetAppPtr())->session.keywords.end(), *it) == ((ofApp*)ofGetAppPtr())->session.keywords.end();
				}
				// if the token starts with a backslash, then we don't tokenize based on digits
				if (thisLang == 0 && startsWith(*it, "\\")) tokenizeDigits2 = false;
				if (tokenizeDigits1 && tokenizeDigits2) {
					subtokens = tokenizeString(*it, "0123456789", true);
				}
				else {
					subtokens.push_back(*it);
				}
				for (auto subit = subtokens.begin(); subit != subtokens.end(); ++subit) {
					//if (i == cursorLineIndex && cursorX >= xOffset && cursorX <= xOffset + font.stringWidth(subtoken)) {
					//	if ((cursorX == xOffset + font.stringWidth(subtoken) && cursorX == xOffset + (int)allLines[i].str.size()-1) || cursorX < xOffset + font.stringWidth(subtoken)) {
					//		isCursorInsideKeyword = true;
					//	}
					//}
					if (i == cursorLineIndex) {
						size_t tokenNdx = std::distance(tokens.begin(), it);
						size_t subtokenNdx = std::distance(subtokens.begin(), subit);
						bool endOfStr = (tokenNdx == tokens.size()-1 && subtokenNdx == subtokens.size()-1 ? true : false);
						if (cursorPos - (int)subtokensSizeAccum + allLines[i].startPos < (int)(*subit).size() || (cursorPos + allLines[i].startPos == (int)allLines[i].str.size() && endOfStr)) {
							isCursorInsideKeyword = true;
						}
					}
					subtokensSizeAccum += (*subit).size();
					if (allLines[i].tracebackStr.size() > 0 && allLines[i].tracebackColor > 0) {
						currentColor = background;
					}
					else if ((thisLang == 0 && startsWith(*subit, "%")) || (thisLang == 1 && startsWith(*subit, "#"))) {
						currentColor = Colors::gray;
						isComment = true;
						if (isCursorInsideKeyword && !cursorDrawn) {
							cursorColor = Colors::gray;
							cursorInRow(cursorColor);
							cursorDrawn = true;
						}
					}
					// Python decorators (an @ inside a comment is part of the comment)
					else if (thisLang == 1 && startsWith(*subit, "@") && !isComment) {
						currentColor = Colors::coral;
						if (isCursorInsideKeyword && !cursorDrawn) {
							cursorColor = Colors::coral;
							cursorInRow(cursorColor);
							cursorDrawn = true;
						}
					}
					// quotes inside a comment are part of the comment, and don't start a string
					else if ((*subit == "\"" || (thisLang == 1 && *subit == "'")) && !isComment) {
						currentColor = Colors::aqua;
						if (quotesCounter == 0) {
							isString = true;
							quotesCounter++;
							stringChar = (*subit)[0];
						}
						else {
							if (thisLang == 0 || (thisLang == 1 && (*subit)[0] == stringChar)) {
								isString = false;
								quotesCounter = 0;
							}
						}
						if (isCursorInsideKeyword && !cursorDrawn) {
							cursorColor = Colors::aqua;
							cursorInRow(cursorColor);
							cursorDrawn = true;
						}
					}
					else {
						if ((*subit).size() > 0 && (isalpha((*subit)[0]) || (thisLang == 0 && startsWith(*subit, "\\"))) && !isComment && !isString) {
							int firstCharNdx = 0;
							Color color;
							if (startsWith(*subit, "\\") && (*subit).size() > 1) firstCharNdx = 1;
							if (((ofApp*)ofGetAppPtr())->session.commandsMap[thisLang][(*subit)[firstCharNdx]].find(*subit) != ((ofApp*)ofGetAppPtr())->session.commandsMap[thisLang][(*subit)[firstCharNdx]].end()) {
								color = ((ofApp*)ofGetAppPtr())->session.commandsMap[thisLang][(*subit)[firstCharNdx]][*subit];
								if (isCursorInsideKeyword && !cursorDrawn) {
									cursorColor = color;
									cursorInRow(cursorColor);
									cursorDrawn = true;
								}
							}
							else {
								color = foreground;
							}
							currentColor = color;
						}
						//if (((ofApp*)ofGetAppPtr())->commandsMap[thisLang].find(*subit) != ((ofApp*)ofGetAppPtr())->commandsMap[thisLang].end()
						//		&& !isComment && !isString) {
						//	ofColor color = ((ofApp*)ofGetAppPtr())->commandsMap[thisLang][*subit];
						//	ofSetColor(color*((ofApp*)ofGetAppPtr())->brightnessCoeff);
						//	if (isCursorInsideKeyword && !cursorDrawn) {
						//		cursorColor = color;
						//		cursorInRow(cursorColor);
						//		cursorDrawn = true;
						//	}
						//}
						else if ((*subit).size() == 0 && !isComment && !isString) {
							currentColor = foreground;
						}
						else if (isNumber(*subit) && !isComment && !isString) {
							currentColor = Colors::gold;
							if (isCursorInsideKeyword && !cursorDrawn) {
								cursorColor = Colors::gold;
								cursorInRow(cursorColor);
								cursorDrawn = true;
							}
						}
						else if (!isComment && !isString) {
							currentColor = foreground;
						}
						else if (isComment && (isCursorInsideKeyword && !cursorDrawn)) {
							cursorColor = Colors::gray;
							cursorInRow(cursorColor);
							cursorDrawn = true;
						}
						else if (isString && (isCursorInsideKeyword && !cursorDrawn)) {
							cursorColor = Colors::aqua;
							cursorInRow(cursorColor);
							cursorDrawn = true;
						}
					}
					if (wrapLines) {
						// the parts of the token are drawn in the rows of the line they fall in
						size_t k = 0;
						while (k < (*subit).size()) {
							int pos = charAccum + (int)k;
							int row = firstRowOf(i) + pos / wrapWidth;
							int col = pos % wrapWidth;
							size_t numChars = std::min((*subit).size() - k, (size_t)(wrapWidth - col));
							if (row >= 0 && row < maxNumLines) {
								textGrid.write(row, col, (*subit).substr(k, numChars), currentColor);
							}
							k += numChars;
						}
					}
					else {
						// the characters of the token in the columns they fall in, as the line of the cursor can be scrolled horizontally
						// (the grid has one column more than maxCharactersPerString, and leaves out what falls outside it)
						int startPos = (i == cursorLineIndex ? allLines[i].startPos : 0);
						textGrid.write(firstRowOf(i), charAccum - startPos, *subit, currentColor);
					}
					xOffset += ((*subit).size() * oneCharacterWidth);
					charAccum += (int)(*subit).size();
				}
			}
		}

		// then check if we have selected this string so we must highlight it
		if (highlightManyChars && ((i>=topLine) && (i<=bottomLine))) {
			std::vector<int> posAndSize = setSelectedStrStartPosAndSize(i);
			if (wrapLines) {
				// the selected part of the line is highlighted row by row
				int pos = posAndSize[0];
				int end = posAndSize[0] + posAndSize[1];
				while (pos < end) {
					int row = firstRowOf(i) + pos / wrapWidth;
					int col = pos % wrapWidth;
					int numChars = std::min(end - pos, wrapWidth - col);
					if (row >= 0 && row < maxNumLines) {
						addSelection(row, col, allLines[i].str.substr(pos, numChars), false);
					}
					pos += numChars;
				}
				count++;
				continue;
			}
			// the column of the selection doesn't need the offset of a line scrolled horizontally
			// but the selected text does need it
			addSelection(count, posAndSize[0], allLines[i].str.substr(posAndSize[0] + allLines[i].startPos, posAndSize[1]), true);
		}
		count++;
	}

	/*******************************************************************/
	// then highlight the tokens of the notes that play, with a rectangle around each of them, as in Strudel (see \editor.animate)
	// the tokens are found by the id of their line, among the visible lines, and only if the line still has the same text there
	if (animationState && thisLang == 0) {
		std::vector<EditorNoteSpan> spans = ((ofApp*)ofGetAppPtr())->getActiveEditorNoteSpans(objID);
		if (!spans.empty()) {
			std::map<int, int> visibleLines; // line index by line id
			for (int i = lineCountOffset; i < loopIter; i++) {
				if (allLines[i].id >= 0) visibleLines[allLines[i].id] = i;
			}
			auto addOutline = [&](int row, int col, int numCols) {
				GridRect rect;
				rect.row = row;
				rect.col = col;
				rect.numCols = numCols;
				rect.color = foreground;
				rect.outline = true;
				textGrid.outlines.push_back(rect);
			};
			for (const EditorNoteSpan& span : spans) {
				auto it = visibleLines.find(span.lineId);
				if (it == visibleLines.end()) continue;
				int i = it->second;
				const std::string& lineStr = allLines[i].str;
				if (span.start < 0 || span.start + span.length > (int)lineStr.size() || lineStr.compare(span.start, span.length, span.text) != 0) continue;
				if (wrapLines) {
					// a token can continue in the next row of a wrapped line
					int pos = span.start;
					int end = span.start + span.length;
					while (pos < end) {
						int row = firstRowOf(i) + pos / wrapWidth;
						int col = pos % wrapWidth;
						int numChars = std::min(end - pos, wrapWidth - col);
						if (row >= 0 && row < maxNumLines) {
							addOutline(row, col, numChars);
						}
						pos += numChars;
					}
				}
				else {
					// only the part of the token that is visible, as the line of the cursor can be scrolled horizontally
					int startPos = (i == cursorLineIndex ? allLines[i].startPos : 0);
					int firstCol = std::max(0, span.start - startPos);
					int lastCol = std::min(maxCharactersPerString + 1, span.start + span.length - startPos);
					if (lastCol > firstCol) {
						addOutline(firstRowOf(i), firstCol, lastCol - firstCol);
					}
				}
			}
		}
	}

	/*******************************************************************/
	// then draw the cursor
	if (!cursorDrawn) markCursor(cursorX, foreground, false, 0);

	/*******************************************************************/
	// then the hint of arguments, in the spaces that make room for it (see above), in a color between the foreground and the background
	if (!argHint.empty()) {
		Color hintColor = toColor(app->foregroundColor.getLerped(app->backgroundColor, 0.55f));
		for (size_t c = 0; c < argHint.size(); c++) {
			int row, col;
			if (wrapLines) {
				std::pair<int, int> rowCol = wrapRowCol(cursorLineIndex, argHintPos + (int)c);
				row = firstRowOf(cursorLineIndex) + rowCol.first;
				col = rowCol.second;
			}
			else {
				row = firstRowOf(cursorLineIndex);
				col = argHintPos + (int)c - allLines[cursorLineIndex].startPos;
				if (col >= maxCharactersPerString) break;
			}
			if (row < 0 || row >= maxNumLines || col < 0) continue;
			textGrid.marks.push_back({row, col, {std::string(1, argHint[c]), hintColor}});
		}
	}

	/*******************************************************************/
	// then draw the character the cursor is drawn on top of (if this is the case)
	if (activity && !inserting && !typingShell && !dPressed) {
		if (cursorPos < (int)strOnCursorLine.size() && cursorPos <= maxCursorPos()) {
			std::string onTopOfCursorStr = strOnCursorLine.substr(cursorPos+allLines[cursorLineIndex].startPos, 1);
			textGrid.cursor.charOnTop = onTopOfCursorStr;
			textGrid.cursor.charColor = Color(0, 0, 0);
		}
	}

	/*******************************************************************/
	// then draw the line numbers
	count = 0;
	// the line numbers are right-aligned in their own grid, which starts half a character from the edge of the pane
	int gutterX = halfCharacterWidth + frameXOffset;
	for (int i = lineCountOffset; i < loopIter; i++) {
		int xOffset = halfCharacterWidth;
		int timesTen = 1;
		// find how many digits variable i has less than the number of digits of lineCount
		int numDigitsLess = 0;
		if ((i+1) < pow(10, (numDigits-1))) {
			while (timesTen <= (i+1)) {
				timesTen *= 10;
				numDigitsLess++;
			}
			// the line below gives an offset to numbers with less digits
			// than the number of digits of lineCount, so that line numbering
			// follows the standard of most editors with units falling under units
			// tens after tens, hundreds under hundreds etc.
			xOffset += (oneCharacterWidth * (numDigits - numDigitsLess));
		}
		xOffset += frameXOffset;
		// a wrapped line has its number on its first row only, which might be above the top of the pane for a very long line
		int row = firstRowOf(i);
		if (row >= 0 && row < maxNumLines) gutterGrid.write(row, (xOffset - gutterX) / oneCharacterWidth, std::to_string(i+1), foreground);
		count++;
	}
	/*******************************************************************/
	// paint the grids: the text area from the end of the line numbers, with its row backgrounds reaching the edge of the pane, and the line numbers
	GridGeometry geometry;
	geometry.x = strXOffset;
	geometry.y = frameYOffset;
	geometry.cellWidth = oneCharacterWidth;
	geometry.cellHeight = cursorHeight;
	geometry.descent = characterOffset;
	geometry.toEndWidth = rectWidth;
	paintGrid(textGrid, geometry, font, app->brightnessCoeff);
	GridGeometry gutterGeometry = geometry;
	gutterGeometry.x = gutterX;
	paintGrid(gutterGrid, gutterGeometry, font, app->brightnessCoeff);
	if (prevCursorPos != cursorPos) prevCursorPos = cursorPos;
}

//--------------------------------------------------------------
void Editor::markCursor(int cursorX, Color color, bool inRow, int splitCol)
{
	// the cursor of the text grid (see drawText()): a block in normal mode, a bar in insert mode, a line under the character after d
	// an outline in a pane that isn't active, and none while a shell command is typed
	cursorScreenX = cursorX;
	cursorScreenY = (cursorScreenRow * cursorHeight) + frameYOffset;
	GridCursor& cursor = textGrid.cursor;
	if ((activity && !inserting && !typingShell) || !activity) {
		if (dPressed) cursor.shape = GridCursor::Shape::underline;
		else cursor.shape = (activity ? GridCursor::Shape::block : GridCursor::Shape::outline);
	}
	else if (activity && inserting && !typingShell) {
		cursor.shape = GridCursor::Shape::bar;
	}
	else {
		cursor.shape = GridCursor::Shape::none;
	}
	int textX = lineNumberWidth + oneAndHalfCharacterWidth + frameXOffset;
	cursor.row = cursorScreenRow;
	cursor.col = (cursorX - textX) / oneCharacterWidth;
	cursor.color = color;
	cursor.inRow = inRow;
	cursor.splitCol = splitCol;
}

//--------------------------------------------------------------
void Editor::drawPaneSeparator()
{
	/*
	draw the line that separates the panes separately than the rest of the pane
	so that the panes that are at the bottom of their column (that touch the traceback printing area)
	are drawn after the score, so that when the traceback printing area grows bigger than its default size
	it overrides the score
	*/
	ofSetColor(255*((ofApp*)ofGetAppPtr())->brightnessCoeff);
	float yCoord = maxNumLines * cursorHeight + frameYOffset;
	float strYCoord = ((maxNumLines+1)*cursorHeight) + frameYOffset - characterOffset;
	int fileNameXOffset = oneCharacterWidth + oneAndHalfCharacterWidth + frameXOffset;
	std::string fileName;
	// test if any error occured during loading a file
	if (couldNotLoadFile || couldNotSaveFile) {
		if ((ofGetElapsedTimeMillis() - fileLoadErrorTimeStamp) > FILE_LOAD_ERROR_DUR) {
			couldNotLoadFile = couldNotSaveFile = false;
		}
	}
	else {
		if (fileLoaded) fileName = loadedFileStr;
		else fileName = defaultFileNames[thisLang];
	}
	ofDrawRectangle(frameXOffset, yCoord, frameWidth, cursorHeight);
	if (couldNotLoadFile || couldNotSaveFile) {
		ofSetColor(ofColor::red*((ofApp*)ofGetAppPtr())->brightnessCoeff);
	}
	else {
		ofSetColor(((ofApp*)ofGetAppPtr())->backgroundColor*((ofApp*)ofGetAppPtr())->brightnessCoeff);
		if (fileEdited) fileName += "*";
	}
	font.drawString(ofToString(objID+1), halfCharacterWidth+frameXOffset, strYCoord);
	if (couldNotLoadFile || couldNotSaveFile) {
		font.drawString(fileErrorStr, fileNameXOffset, strYCoord);
	}
	else {
		font.drawString(fileName, fileNameXOffset, strYCoord);
	}
	float widthOffset = 0;
	if (inserting || visualMode) {
		// if we show the score vertically and we have one pane only, show the INSERT string at the middle
		//if (((ofApp*)ofGetAppPtr())->isScoreVisible() &&
		//		((ofApp*)ofGetAppPtr())->scoreOrientation == 0) {
		//	if (frameWidth > ((ofApp*)ofGetAppPtr())->sharedData.middleOfScreenX) {
		//		widthOffset = ((ofApp*)ofGetAppPtr())->sharedData.middleOfScreenX;
		//	}
		//}
		std::string s;
		if (inserting) s = "INSERT";
		else if (selectionMode == 1) s = "VISUAL LINE";
		else if (selectionMode == 2) s = "VISUAL BLOCK";
		else s = "VISUAL";
		font.drawString(s, frameXOffset+frameWidth-widthOffset-font.stringWidth(s)-10, strYCoord);
	}
}

//--------------------------------------------------------------
void Editor::setDPressed(bool dPressedState)
{
	dPressed = dPressedState;
}

//--------------------------------------------------------------
void Editor::setStringsStartPos()
{
	for (unsigned i = 0; i < allLines.size(); i++) {
		if (cursorLineIndex == (int)i) {
			allLines[i].startPos = (int)allLines[i].str.size() - maxCharactersPerString;
			if (allLines[i].startPos < 0) allLines[i].startPos = 0;
		}
		else {
			allLines[i].startPos = 0;
		}
	}
}

//--------------------------------------------------------------
void Editor::setFrameWidth(float frameWidth)
{
	Editor::frameWidth = frameWidth;
}

//--------------------------------------------------------------
void Editor::setFrameHeight(float frameHeight)
{
	Editor::frameHeight = frameHeight;
}

//--------------------------------------------------------------
float Editor::getFrameHeight()
{
	return frameHeight;
}

//--------------------------------------------------------------
void Editor::setFrameXOffset(float xOffset)
{
	frameXOffset = xOffset;
}

//--------------------------------------------------------------
void Editor::setFrameYOffset(float yOffset)
{
	frameYOffset = yOffset;
}

//--------------------------------------------------------------
float Editor::getFrameYOffset()
{
	return frameYOffset;
}

//--------------------------------------------------------------
void Editor::setMaxCharactersPerString()
{
	lineNumberWidth = getNumDigitsOfLineCount() * oneCharacterWidth;
	int width = frameWidth;
	if (((ofApp*)ofGetAppPtr())->isScoreVisible()) {
		if (frameWidth > ((ofApp*)ofGetAppPtr())->view.middleOfScreenX && ((ofApp*)ofGetAppPtr())->scoreOrientation == 0) {
			width = ((ofApp*)ofGetAppPtr())->view.middleOfScreenX;
		}
	}
	maxCharactersPerString = (width-lineNumberWidth-oneAndHalfCharacterWidth) / oneCharacterWidth;
	maxCharactersPerString -= 1;
	// when lines wrap, the rows have the width of the pane, and lines never scroll horizontally
	wrapWidth = std::max(1, maxCharactersPerString);
	if (wrapLines) maxCharactersPerString = 1000000;
	if (maxCharactersPerString != oldMaxCharactersPerString) {
		setStringsStartPos();
		oldMaxCharactersPerString = maxCharactersPerString;
	}
}

//--------------------------------------------------------------
void Editor::resetCursorPos(int oldMaxCharactersPerString)
{
	if ((int)allLines[cursorLineIndex].str.size() > 0 && \
			(int)allLines[cursorLineIndex].str.size() > maxCharactersPerString) {
		if (cursorPos == oldMaxCharactersPerString) {
			cursorPos = maxCharactersPerString;
			highlightedCharIndex = (int)allLines[cursorLineIndex].str.size();
			allLines[cursorLineIndex].startPos = (int)allLines[cursorLineIndex].str.substr(0, cursorPos).size() - maxCharactersPerString;
		}
		else if (cursorPos == 0) {
			cursorPos = 0;
			highlightedCharIndex = 0;
			allLines[cursorLineIndex].startPos = 0;
		}
		else {
			highlightedCharIndex = cursorPos + allLines[cursorLineIndex].startPos;
			// three factor method
			cursorPos = round((float)(cursorPos * maxCharactersPerString) / (float)oldMaxCharactersPerString);
			allLines[cursorLineIndex].startPos = highlightedCharIndex-cursorPos;
			if (allLines[cursorLineIndex].startPos < 0) allLines[cursorLineIndex].startPos = 0;
		}
		stringExceededWindow = true;
	}
	else {
		// if we're going from big to small
		if (stringExceededWindow) {
			cursorPos = highlightedCharIndex;
			allLines[cursorLineIndex].startPos = 0;
			stringExceededWindow = false;
		}
	}
}

//--------------------------------------------------------------
void Editor::setFontSize(int fontSize, float oneCharacterWidth, float cursorHeight)
{
	// store the previous the maximum characters per string
	int oldMaxCharactersPerString = maxCharactersPerString;
	if (!fontLoaded || fontSize != Editor::fontSize) {
		Editor::fontSize = fontSize;
		font.load("DroidSansMono.ttf", fontSize);
		// we're using a monospace font, so we get the width of any character
		Editor::oneCharacterWidth = oneCharacterWidth;
		halfCharacterWidth = oneCharacterWidth / 2;
		oneAndHalfCharacterWidth = oneCharacterWidth + halfCharacterWidth;
		// the variable below helps center the cursor at the Y axis with respect to the letters
		characterOffset = font.stringHeight("j") - font.stringHeight("l");
		characterOffset /= 2;
		Editor::cursorHeight = cursorHeight;
		setMaxCharactersPerString();
		fontLoaded = true;
	}
	resetCursorPos(oldMaxCharactersPerString);
}

//--------------------------------------------------------------
int Editor::getHalfCharacterWidth()
{
	return halfCharacterWidth;
}

//--------------------------------------------------------------
void Editor::setMaxNumLines(int numLines)
{
	maxNumLines = numLines - 1; // - 1 for the thick line with the name of file and other info
	maxNumLinesReset = maxNumLines + 1;
	// the pane keeps how far it has scrolled, and scrolls only as much as needed to keep the line of the cursor visible
	// this is called when the pane changes size, e.g. when the area of the error messages or of the shell grows or shrinks
	// or when changing between normal and insert mode (see resetMaxNumLines()), where the text should stay where it is
	if (maxNumLines > 0 && cursorLineIndex >= lineCountOffset + maxNumLines) lineCountOffset = cursorLineIndex - maxNumLines + 1;
	if (cursorLineIndex < lineCountOffset) lineCountOffset = cursorLineIndex;
	if (lineCountOffset < 0) lineCountOffset = 0;
}

//--------------------------------------------------------------
int Editor::getMaxNumLines()
{
	return maxNumLines;
}

//--------------------------------------------------------------
void Editor::offsetMaxNumLines(int offset)
{
	int maxNumLinesTemp = maxNumLinesReset;
	setMaxNumLines(maxNumLinesTemp + offset);
	// reset the variables used to reset the max number of lines
	maxNumLinesReset = maxNumLinesTemp;
}

//--------------------------------------------------------------
void Editor::resetMaxNumLines()
{
	setMaxNumLines(maxNumLinesReset);
}

//--------------------------------------------------------------
void Editor::postIncrementOnNewLine()
{
	lineCount++;
	cursorLineIndex++;
	cursorPos = arrowCursorPos = 0;
	if ((lineCount + lineCountOffset) >= maxNumLines && (cursorLineIndex - lineCountOffset) >= maxNumLines) { // >= lineCount - 1) {
		lineCountOffset++;
	}
}

//--------------------------------------------------------------
void Editor::createNewLine(std::string str, int increment)
{
	allLines[cursorLineIndex+increment].str = str;
	allLines[cursorLineIndex+increment].startPos = 0;
	allLines[cursorLineIndex+increment].tracebackStr = "";
	allLines[cursorLineIndex+increment].tracebackColor = 0;
	allLines[cursorLineIndex+increment].tracebackNumLines = 1;
	allLines[cursorLineIndex+increment].tracebackTimeStamp = 0;
	allLines[cursorLineIndex+increment].tracebackStrBreakPnt = 0;
	allLines[cursorLineIndex+increment].isBeingExecuted = false;
	allLines[cursorLineIndex+increment].executionDegrade = 0;
	allLines[cursorLineIndex+increment].executionTimeStamp = 0;
	allLines[cursorLineIndex+increment].linesConnectedToBar = -1;
	allLines[cursorLineIndex+increment].activeLineElements = -1;
	allLines[cursorLineIndex+increment].id = nextLineId++;
}

//--------------------------------------------------------------
void Editor::moveLineNumbers(int numLines)
{
	if (numLines < 0) {
		// move all strings below the line where the cursor is, one line above
		// we're subtracting numLines because it is negative but we want to actually add it
		int cursorLineIndexLocal = cursorLineIndex + 1 - numLines;
		// subtracting numLines for the same reason
		while (cursorLineIndexLocal < lineCount - numLines) {
			changeMapKeys(cursorLineIndexLocal, numLines);
			cursorLineIndexLocal++;
		}

	}
	else {
		// move all strings below the line where the cursor is, one line below
		int lineIndexLocal = lineCount - 1;
		while (lineIndexLocal > cursorLineIndex) {
			changeMapKeys(lineIndexLocal, numLines);
			lineIndexLocal--;
		}
	}
}

//--------------------------------------------------------------
void Editor::moveDataToNextLine()
{
	allLines[cursorLineIndex+1].str = allLines[cursorLineIndex].str;
	allLines[cursorLineIndex+1].startPos = allLines[cursorLineIndex].startPos;
	allLines[cursorLineIndex+1].tracebackStr = allLines[cursorLineIndex].tracebackStr;
	allLines[cursorLineIndex+1].tracebackColor = allLines[cursorLineIndex].tracebackColor;
	allLines[cursorLineIndex+1].tracebackNumLines = allLines[cursorLineIndex].tracebackNumLines;
	allLines[cursorLineIndex+1].tracebackTimeStamp = allLines[cursorLineIndex].tracebackTimeStamp;
	allLines[cursorLineIndex+1].tracebackStrBreakPnt = allLines[cursorLineIndex].tracebackStrBreakPnt;
	allLines[cursorLineIndex+1].isBeingExecuted = allLines[cursorLineIndex].isBeingExecuted;
	allLines[cursorLineIndex+1].executionDegrade = allLines[cursorLineIndex].executionDegrade;
	allLines[cursorLineIndex+1].executionTimeStamp = allLines[cursorLineIndex].executionTimeStamp;
	allLines[cursorLineIndex+1].linesConnectedToBar = allLines[cursorLineIndex].linesConnectedToBar;
	allLines[cursorLineIndex+1].activeLineElements = allLines[cursorLineIndex].activeLineElements;
	// the line moves down with its text, so its id goes with it, and the empty line it leaves behind gets a new one
	allLines[cursorLineIndex+1].id = allLines[cursorLineIndex].id;
	// if the language of this pane is Python, we have to change the traceback error string to update the line number
	if (thisLang == 1) {
		int newKey = cursorLineIndex + 1;
		size_t secondBreakPnt = allLines[newKey].tracebackStr.substr(allLines[newKey].tracebackStrBreakPnt).find(",");
		if (secondBreakPnt != std::string::npos) {
			allLines[newKey].tracebackStr = allLines[newKey].tracebackStr.substr(0, allLines[newKey].tracebackStrBreakPnt) + std::to_string(newKey+1) +
				allLines[newKey].tracebackStr.substr(allLines[newKey].tracebackStrBreakPnt+secondBreakPnt);
		}
	}

	allLines[cursorLineIndex].str = "";
	allLines[cursorLineIndex].startPos = 0;
	allLines[cursorLineIndex].tracebackStr = "";
	allLines[cursorLineIndex].tracebackColor = 0;
	allLines[cursorLineIndex].tracebackNumLines = 1;
	allLines[cursorLineIndex].tracebackTimeStamp = 0;
	allLines[cursorLineIndex].tracebackStrBreakPnt = 0;
	allLines[cursorLineIndex].isBeingExecuted = false;
	allLines[cursorLineIndex].executionDegrade = 0;
	allLines[cursorLineIndex].executionTimeStamp = 0;
	allLines[cursorLineIndex].linesConnectedToBar = -1;
	allLines[cursorLineIndex].activeLineElements = -1;
	allLines[cursorLineIndex].id = nextLineId++;
}

//--------------------------------------------------------------
//void Editor::copyOnLineDelete()
//{
//	// in case of backspace the cursorLineIndex variable has been updated before this function call
//	// in case of delete, the variable doesn't change
//	// in any case, it already points to the right key of the map below
//	std::map<int, line>::iterator it1 = allLines.find(cursorLineIndex);
//	std::map<int, line>::iterator it2 = allLines.find(cursorLineIndex+1);
//	// first concatenate the two strings
//	if (it2 != allLines.end()) {
//		it1->second.str += it2->second.str;
//		// before moving the keys of the std::maps we must erase the keys of the line below the cursor
//		eraseMapKeys(cursorLineIndex+1);
//	}
//	// since cursorLineIndex is updated, if we are deleting line 10
//	// cursorLineIndex is 9 before we enter this function
//	// but we want to change the keys of lines 11 onward
//	// advance though moves the iterator to the next element
//	// and since we have already erased the next line, we advance one element only
//	int thisKey = it1->first;
//	std::advance(it1, 1);
//	// the keys of the iterator and its previous positions should be two numbers apart
//	// which is true only if there is at least one more line after the line we have erased
//	if (it1->first == thisKey + 2) {
//		while (it1 != allLines.end()) {
//			int key = it1->first;
//			changeMapKeys(key, -1);
//			++it1;
//		}
//	}
//}
// provided by chatgpt (changes have also been made in changeMapKeys() and eraseMapKeys())
void Editor::copyOnLineDelete()
{
	auto current = allLines.find(cursorLineIndex);
	if (current == allLines.end())
		return;

	auto next = allLines.find(cursorLineIndex + 1);

	// Merge the next line into the current line.
	if (next != allLines.end()) {
		current->second.str += next->second.str;
		eraseMapKeys(cursorLineIndex + 1);
	}

	// Everything after the deleted line moves up by one.
	int key = cursorLineIndex + 2;

	while (allLines.find(key) != allLines.end()) {
		changeMapKeys(key, -1);
		++key;
	}
}

//--------------------------------------------------------------
void Editor::newLine()
{
	bool stringBreaks = false;
	// a string breaks only if we're at some mid point in the line
	// and not if we're either at the end or the beginning of it
	if (cursorPos < ((int)allLines[cursorLineIndex].str.size()-allLines[cursorLineIndex].startPos) &&
		!(cursorPos == 0 && allLines[cursorLineIndex].startPos == 0)) {
		stringBreaks = true;
	}
	if (cursorLineIndex != (lineCount-1)) {
		moveLineNumbers(1);
	}
	if (stringBreaks) {
		// create a new line with the remainder of the string
		createNewLine(allLines[cursorLineIndex].str.substr(cursorPos+allLines[cursorLineIndex].startPos), 1);
		// and change the string where the cursor is (the cursorLineIndex hasn't been updated yet)
		allLines[cursorLineIndex].str = allLines[cursorLineIndex].str.substr(0, cursorPos+allLines[cursorLineIndex].startPos);
	}
	else {
		createNewLine("", 1);
	}
	// check if we hit enter at the beginning of a line, which means we have to move all the line data
	if (cursorPos == 0 && allLines[cursorLineIndex].startPos == 0) {
		moveDataToNextLine();
	}
	postIncrementOnNewLine();
}

//--------------------------------------------------------------
void Editor::moveCursorOnShiftReturn()
{
	int tempIndex;
	if (highlightManyChars) {
		tempIndex = bottomLine;
	}
	else {
		tempIndex = cursorLineIndex;
	}
	if (tempIndex == (lineCount-1)) {
		cursorLineIndex = lineCount - 1;
		createNewLine("", 1);
		postIncrementOnNewLine();
	}
	else {
		tempIndex++;
		for (std::map<int, line>::iterator it = allLines.find(tempIndex); it != allLines.end(); ++it) {
			if (it->second.str.size() > 0) {
				tempIndex = it->first;
				break;
			}
		}
		setLineIndexesDownward(tempIndex);
	}
	cursorPos = 0;
}

//--------------------------------------------------------------
void Editor::changeMapKeys(int key, int increment)
{
	//if (allLines.find(key) != allLines.end()) {
	//	auto nodeHolder = allLines.extract(key);
	//	nodeHolder.key() = key + increment;
	//	allLines.insert(std::move(nodeHolder));
	//}
	//// get the bar number the current line is connected to
	//int barNdx = allLines[key+increment].linesConnectedToBar;
	// edit of the lines above by chatgpt
	auto it = allLines.find(key);
	if (it == allLines.end())
		return;

	auto nodeHolder = allLines.extract(it);
	nodeHolder.key() = key + increment;

	auto result = allLines.insert(std::move(nodeHolder));
	if (!result.inserted)
		return;

	// get the bar number the current line is connected to
	int barNdx = result.position->second.linesConnectedToBar;
	// end of edit

	// scroll through all instruments to see which one is connected to this bar and this line
	for (auto it = instsConnectedToLine.begin(); it != instsConnectedToLine.end(); ++it) {
		auto it2 = it->second.find(barNdx);
		if (it2 != it->second.end() && it2->second == key) {
			// update the line number this instrument connects to for this bar
			it2->second += increment;
			//break;
		}
	}
	// if the language of this pane is Python, we have to change the traceback error string
	// to update the line number
	if (thisLang == 1) {
		int newKey = key + increment;
		size_t secondBreakPnt = allLines[newKey].tracebackStr.substr(allLines[newKey].tracebackStrBreakPnt).find(",");
		if (secondBreakPnt != std::string::npos) {
			allLines[newKey].tracebackStr = allLines[newKey].tracebackStr.substr(0, allLines[newKey].tracebackStrBreakPnt) + std::to_string(newKey+1) +
				allLines[newKey].tracebackStr.substr(allLines[newKey].tracebackStrBreakPnt+secondBreakPnt);
		}
	}
}

//--------------------------------------------------------------
void Editor::eraseMapKeys(int key)
{
	// first store the value of the linesConnectedToBar variable which is the bar this line connects to
	//int barNdx = allLines[key].linesConnectedToBar;
	//allLines.erase(key);
	// edit of the lines above by chatgpt
	// first store the value of the linesConnectedToBar variable which is the bar this line connects to
	auto it = allLines.find(key);
	if (it == allLines.end())
		return;

	int barNdx = it->second.linesConnectedToBar;
	allLines.erase(it);
	// end of edit

	// scroll through all instruments to see which one is connected to this bar and this line
	for (auto it = instsConnectedToLine.begin(); it != instsConnectedToLine.end(); ++it) {
		auto it2 = it->second.find(barNdx);
		//// if this bar is stored in the instsConnectedToLine map, erase it
		//if (it2 != it->second.end()) {
		//	instsConnectedToLine[it->first].erase(it2->first);
		//}
		// edit of the commented out lines above by chatgpt
		// Only erase the connection if this bar is connected to the line
		// that was actually erased.
		if (it2 != it->second.end() && it2->second == key) {
			it->second.erase(it2);
		}
		// end of edit
	}
}

//--------------------------------------------------------------
void Editor::connectLineToBar(int lineNdx, int instNdx, int barNdx)
{
	allLines[lineNdx].linesConnectedToBar = barNdx;
	instsConnectedToLine[instNdx][barNdx] = lineNdx;
}

//--------------------------------------------------------------
int Editor::getLineConnectedToBar(int instNdx, int barNdx)
{
	if (instsConnectedToLine.find(instNdx) != instsConnectedToLine.end() &&
			instsConnectedToLine[instNdx].find(barNdx) != instsConnectedToLine[instNdx].end()) {
		return instsConnectedToLine[instNdx][barNdx];
	}
	return -1;
}

//--------------------------------------------------------------
void Editor::setActiveLineElement(int lineNdx, int elementNdx)
{
	allLines[lineNdx].activeLineElements = elementNdx;
}

//--------------------------------------------------------------
void Editor::setAnimation(bool state)
{
	animationState = state;
}

//--------------------------------------------------------------
int Editor::getLineId(int lineNdx)
{
	auto it = allLines.find(lineNdx);
	if (it == allLines.end()) return -1;
	if (it->second.id < 0) it->second.id = nextLineId++;
	return it->second.id;
}

//--------------------------------------------------------------
bool Editor::isThisATab(int pos)
{
	if (pos < 0) return false;
	if (pos + TABSIZE > (int)allLines[cursorLineIndex].str.length()) return false;
	if (allLines[cursorLineIndex].str.substr(pos, TABSIZE).compare(tabStr) == 0) {
		return true;
	}
	else return false;
}

//--------------------------------------------------------------
int Editor::didWeLandOnATab()
{
	int tabIndex = -1;
	for (int i = 0; i < TABSIZE; i++) {
		int posLocal = std::max((cursorPos-i), 0);
		if(isThisATab(posLocal)) {
			tabIndex = cursorPos - i;
			break;
		}
	}
	return tabIndex;
}

//---------------------------------------------------------------
int Editor::getTabSize()
{
	return TABSIZE;
}

//---------------------------------------------------------------
int Editor::maxCursorPos()
{
	// the cursor position is relative to how far a long line has scrolled (startPos), so the end of the line is its length minus that
	// otherwise, when a scrolled line gets shorter, the cursor could go past its end
	return std::max(0, std::min((int)allLines[cursorLineIndex].str.size() - allLines[cursorLineIndex].startPos, maxCharactersPerString));
}

//--------------------------------------------------------------
void Editor::setCursorPos(int pos)
{
	// this function is called only by ofApp.cpp
	cursorPos = pos;
	//if (cursorPos < 0) cursorPos = 0;
	//else if (cursorPos > maxCursorPos()) cursorPos = maxCursorPos();
	if (!inserting && cursorPos > 0) cursorPos--;
	arrowCursorPos = cursorPos;
	if (pos == 0) allLines[cursorLineIndex].startPos = 0;
	else if ((int)allLines[cursorLineIndex].str.size() > maxCharactersPerString) {
		allLines[cursorLineIndex].startPos = (int)allLines[cursorLineIndex].str.size() - maxCharactersPerString;
	}
}

//----------------------------------------------------------------
void Editor::assembleString(int key, bool executing, bool lineBreaking)
{
	// first check if we're receiving any of the special characters
	// 8 is backspace
	if (key == 8) {
		std::string deletedChar;
		if (highlightManyChars) {
			deleteString();
			// in case we delete more than one characters at the end of the string
			// we need to check if the saved cursor position is greater than the remaining string
			if (cursorPos >= (int)allLines[cursorLineIndex].str.size()) {
				cursorPos = (int)allLines[cursorLineIndex].str.size();
			}
		}
		else if ((allLines[cursorLineIndex].str.size() > 0) && (cursorPos > 0)) {
			int numCharsToDelete = 1;
			// check if the cursor is at the end of the string
			if (cursorPos == (int)allLines[cursorLineIndex].str.size()-allLines[cursorLineIndex].startPos) {
				if (isThisATab(cursorPos-TABSIZE)) numCharsToDelete = TABSIZE;
				if ((int)allLines[cursorLineIndex].str.size() >= numCharsToDelete) {
					deletedChar = allLines[cursorLineIndex].str.substr(allLines[cursorLineIndex].str.size()-numCharsToDelete, numCharsToDelete);
					allLines[cursorLineIndex].str = allLines[cursorLineIndex].str.substr(0, allLines[cursorLineIndex].str.size()-numCharsToDelete);
				}
			}
			else {
				if (isThisATab(cursorPos-TABSIZE)) numCharsToDelete = TABSIZE;
				std::string first;
				std::string second;
				// the position of the cursor in the line, which must be within it (see maxCursorPos())
				int posInLine = cursorPos + allLines[cursorLineIndex].startPos;
				if (posInLine <= (int)allLines[cursorLineIndex].str.size() && posInLine - numCharsToDelete >= 0) {
					first = allLines[cursorLineIndex].str.substr(0, cursorPos+allLines[cursorLineIndex].startPos-numCharsToDelete);
					second = allLines[cursorLineIndex].str.substr(cursorPos+allLines[cursorLineIndex].startPos);
					deletedChar = allLines[cursorLineIndex].str.substr(cursorPos+allLines[cursorLineIndex].startPos-numCharsToDelete, numCharsToDelete);
					allLines[cursorLineIndex].str = first + second;
				}
			}
			allLines[cursorLineIndex].startPos -= numCharsToDelete;
			if (allLines[cursorLineIndex].startPos < 0) {
				allLines[cursorLineIndex].startPos = 0;
				cursorPos -= numCharsToDelete;
				if (cursorPos < 0) cursorPos = 0;
				arrowCursorPos = cursorPos;
				if (cursorPos == 0) releaseTraceback(cursorLineIndex);
			}
			// if we're deleting a quote sign
			if (deletedChar.compare("\"") == 0) {
				quoteCounter--;
				if (quoteCounter < 0) quoteCounter = 0;
			}
			else if (deletedChar.compare("}") == 0) {
				curlyBracketCounter--;
				if (curlyBracketCounter < 0) curlyBracketCounter = 0;
			}
			else if (deletedChar.compare("]") == 0) {
				squareBracketCounter--;
				if (squareBracketCounter < 0) squareBracketCounter = 0;
			}
			else if (deletedChar.compare(")") == 0) {
				roundBracketCounter--;
				if (roundBracketCounter < 0) roundBracketCounter = 0;
			}
		}
		else if (cursorPos == 0 && cursorLineIndex > 0) {
			int thisLineStrLength = allLines[cursorLineIndex].str.size();
			setLineIndexesUpward(cursorLineIndex-1);
			copyOnLineDelete();
			// when we press backspace and the cursor is that the beginning of a line
			// if there is text in the line, it will be concatenated to the text
			// of the line above, but the cursor should not be placed at the end
			// of this concatenated string
			cursorPos = arrowCursorPos = std::max(maxCursorPos() - thisLineStrLength, 0);
			lineCount--;
			lineCountOffset--;
			if (lineCountOffset < 0) {
				lineCountOffset = 0;
			}
		}
		if ((int)allLines[cursorLineIndex].str.size() < maxCharactersPerString) {
			allLines[cursorLineIndex].startPos = 0;
		}
		fileEdited = true;
	}

	// 13 is return
	else if (key == 13) {
		quoteCounter = 0; // reset on newline
		curlyBracketCounter = 0;
		squareBracketCounter = 0;
		roundBracketCounter = 0;
		if (executing) {
			int executingLineLocal = executingLine;
			int numExecutingLines = 1;
			if (highlightManyChars) {
				executingLineLocal = topLine;
				numExecutingLines = bottomLine - topLine + 1;
				executingMultiple = true;
				highlightManyChars = false;
			}
#ifdef USEPYO
			int err = 0;
			std::string pyStr = "";
#endif
			switch (thisLang) {
				case 0:
					((ofApp*)ofGetAppPtr())->parseStrings(executingLineLocal, numExecutingLines);
					break;
				case 1:
#ifdef USEPYO
					// combine all the strings to one string separated by newline characters
					for (int i = 0; i < numExecutingLines; i++) {
						if (i > 0) pyStr += "\n";
						pyStr += allLines[i+executingLineLocal].str;
					}
					// execute the Python line in verbose mode with the last argument set to 1
					// check if we want to execute this line in the main thread
					// a help that help(name) stored in code executed elsewhere (e.g. sent from LiveLily) is not for this pane
					((ofApp*)ofGetAppPtr())->session.sharedData.pyo.takeHelp();
					if (startsWith(pyStr, "main ")) {
						err = ((ofApp*)ofGetAppPtr())->session.sharedData.pyo.exec_main(pyStr.substr(5).c_str());
					}
					else {
						err = ((ofApp*)ofGetAppPtr())->session.sharedData.pyo.exec(pyStr.c_str());
					}
					if (err) {
						// get the error message
						std::string errStr = ((ofApp*)ofGetAppPtr())->session.sharedData.pyo.getErrorMsg();
						// get the position of the end of the word "line"
						size_t lineNdx = errStr.find("line")+ 5;
						// insert the line number of the editor and not the Python interpreter
						// and remove the last character (the last substr addition) which is a newline
						errStr = errStr.substr(0, lineNdx) + std::to_string(executingLineLocal+1) + errStr.substr(lineNdx+errStr.substr(lineNdx+1).find(" "), errStr.substr(lineNdx+errStr.substr(lineNdx+1).find(" ")).size()-1);
						setTraceback(3, errStr, executingLineLocal, lineNdx);
					}
					else {
						std::vector<std::string> pyStdout = ((ofApp*)ofGetAppPtr())->session.sharedData.pyo.getStdout();
						if (pyStdout.size() > 1) {
							bool sendToLily = false;
							int whichPane = objID;
							size_t startChar = 0;
							std::string s = "";
							if (startsWith(pyStdout[0], "livelily-") || startsWith(pyStdout[0], "livelily: ")) {
								sendToLily = true;
								if (pyStdout[0].substr(8, 1).compare("-") == 0) {
									size_t colonNdx = pyStdout[0].find(":");
									if (colonNdx != std::string::npos && colonNdx > 9 &&
											isNumber(pyStdout[0].substr(9, colonNdx-9))) {
										whichPane = stoi(pyStdout[0].substr(9, colonNdx-9)) - 1;
										startChar = colonNdx + 2;
										if (startChar >= pyStdout[0].size()) {
											sendToLily = false;
										}
									}
									else {
										sendToLily = false;
									}
								}
								else {
									whichPane = ((ofApp*)ofGetAppPtr())->session.whichPane;
									startChar = pyStdout[0].find(" ") + 1;
									if (startChar >= pyStdout[0].size()) {
										sendToLily = false;
									}
								}
							}
							for (size_t i = 0; i < pyStdout.size()-1; i++) {
								s += pyStdout[i];
								if (i < pyStdout.size()-2 && !endsWith(pyStdout[i], "\n")) s += "\n";
								if (sendToLily) {
									startChar = (i == 0 ? startChar : 0);
									((ofApp*)ofGetAppPtr())->view.pyStdoutStr += s.substr(startChar);
								}
								else {
									setTraceback(1, s, executingLineLocal);
								}
							}
							if (sendToLily) {
								((ofApp*)ofGetAppPtr())->view.whichPyPane = whichPane;
								((ofApp*)ofGetAppPtr())->view.typePyStr = true;
							}
						}
						else {
							releaseTraceback(executingLineLocal);
						}
						// help(name) in the executed code opens its help in this pane, as \help does in LiveLily
						std::string help = ((ofApp*)ofGetAppPtr())->session.sharedData.pyo.takeHelp();
						size_t nameEnd = help.find('\n');
						if (nameEnd != std::string::npos) loadHelpText(help.substr(0, nameEnd), help.substr(nameEnd + 1));
					}
#endif
					break;
				default:
					break;
			}
			if (highlightManyChars) highlightManyChars = false;
		}
		else {
			// selected text is replaced by the new line, as by any other key that types, so it is deleted first (as with backspace)
			if (highlightManyChars) {
				deleteString();
				if (cursorPos >= (int)allLines[cursorLineIndex].str.size()) {
					cursorPos = (int)allLines[cursorLineIndex].str.size();
				}
			}
			if (highlightBracket && cursorPos > 0) {
				// if we hit return while in between two curly brackets
				if (allLines[cursorLineIndex].str.substr(cursorPos-1, 1).compare("{") == 0 &&
						allLines[cursorLineIndex].str.substr(cursorPos, 1).compare("}") == 0) {
					// we insert two new lines and add a horizontal tab in the middle
					newLine();
					lineBreaking = false;
					// the cursor is now at the start of the line with the closing bracket
					int numTabsToWrite;
					if (thisLang == 0) {
						// in LiveLily code, indent by the number of brackets that are open at this point
						// which includes the one we just opened
						numTabsToWrite = std::max(1, getBraceDepth(cursorLineIndex));
					}
					else {
						// otherwise indent one tab more than the line with the opening bracket
						numTabsToWrite = getNumTabsInStr(allLines[cursorLineIndex-1].str) + 1;
					}
					for (int i = 0; i < numTabsToWrite; i++) {
						assembleString(9); // write a horizontal tab
					}
					newLine();
					if (numTabsToWrite > 1) {
						cursorPos = 0;
						for (int i = 1; i < numTabsToWrite; i++) {
							assembleString(9); // write a horizontal tab in the line of the closing bracket
						}
					}
					cursorLineIndex--;
					cursorPos = arrowCursorPos = numTabsToWrite * TABSIZE;
				}
			}
			// if we are inside a curly bracket chunk
			else {
				newLine();
				lineBreaking = false;
				int numTabs = getNumTabsInStr(allLines[cursorLineIndex-1].str);
				// 1 is Python, so if we're writing in Python and the last character is a colon, we insert an extra horintal tab
				if (thisLang == 1 && cursorLineIndex > 0 && allLines[cursorLineIndex-1].str.size() > 0 && allLines[cursorLineIndex-1].str.back() == ':') {
					numTabs++;
				}
				for (int i = 0; i < numTabs; i++) {
					assembleString(9);
				}
			}
			fileEdited = true;
		}
		if (lineBreaking) {
			newLine();
		}
		// typing the lines of a definition (e.g. a bar) keeps its closing bracket in sight
		if (!executing) keepClosingBracketVisible();
	}

	// 127 is del
	else if (key == 127) {
		if (highlightManyChars) {
			deleteString();
			// in case we delete more than one characters at the end of the string
			// we need to check if the saved cursor position is greater than the remaining string
			if (cursorPos >= (int)allLines[cursorLineIndex].str.size()) {
				cursorPos = (int)allLines[cursorLineIndex].str.size();
			}
		}
		// the position of the cursor in the line, as the cursor position is relative to how far a long line has scrolled
		else if (cursorPos + allLines[cursorLineIndex].startPos < (int)allLines[cursorLineIndex].str.size()) {
			int posInLine = cursorPos + allLines[cursorLineIndex].startPos;
			int numCharsToDelete = 1;
			std::string deletedChar;
			if (isThisATab(posInLine)) numCharsToDelete = TABSIZE;
			numCharsToDelete = std::min(numCharsToDelete, (int)allLines[cursorLineIndex].str.size() - posInLine);
			std::string first = allLines[cursorLineIndex].str.substr(0, posInLine);
			std::string second = allLines[cursorLineIndex].str.substr(posInLine+numCharsToDelete);
			deletedChar = allLines[cursorLineIndex].str.substr(posInLine, numCharsToDelete);
			allLines[cursorLineIndex].str = first + second;
			if ((cursorPos == 0) && (allLines[cursorLineIndex].str.size() == 0)) {
				releaseTraceback(cursorLineIndex);
			}
			// if we're deleting a quote sign
			if (deletedChar.compare("\"") == 0) {
				quoteCounter--;
				if (quoteCounter < 0) quoteCounter = 0;
			}
			else if (deletedChar.compare("}") == 0) {
				curlyBracketCounter--;
				if (curlyBracketCounter < 0) curlyBracketCounter = 0;
			}
		}
		else {
			// if we're not at the last line
			if (cursorLineIndex < (lineCount-1)) {
				copyOnLineDelete();
				lineCount--;
				lineCountOffset--;
				if (lineCountOffset < 0) {
					lineCountOffset = 0;
				}
			}
		}
		fileEdited = true;
	}

	// all other characters
	else {
		assembleString(key);
		fileEdited = true;
	}
}

//--------------------------------------------------------------
void Editor::assembleString(int key)
{
	std::string charToInsert;
	int cursorPosIncrement = 1;
	bool doubleChar = false;
	if (highlightManyChars) deleteString();
	// horizontal tab
	if (key == 9) {
		charToInsert = tabStr;
		cursorPosIncrement = TABSIZE;
	}
	else if (key == 34) { // "
		if (autobrackets) {
			if (quoteCounter == 0) {
				// insert two quotation marks but increment the cursor by only one position
				// so it stays in between the quotation marks
				charToInsert = "\"\"";
				doubleChar = true;
			}
			else {
				// the second time we type the quotation mark don't insert it
				charToInsert = "";
			}
			quoteCounter++;
			if (quoteCounter > 1) quoteCounter = 0;
		}
		else {
			charToInsert = "\"";
		}
	}
	else if (key == 123) { // {
		if (autobrackets) {
			if (curlyBracketCounter == 0) {
				// insert opening and closing curly brackets, but move the cursor only one position
				charToInsert = "{}";
				curlyBracketCounter++;
				doubleChar = true;
			}
			else {
				charToInsert = "{";
				curlyBracketCounter--;
				if (curlyBracketCounter <= 0) curlyBracketCounter = 0;
			}
		}
		else {
			charToInsert = "{";
		}
	}
	else if (key == 125) { // }
		if (autobrackets) {
			if (curlyBracketCounter > 0) {
				charToInsert = "";
				curlyBracketCounter = 0;
			}
			else {
				charToInsert = "}";
			}
		}
		else {
			charToInsert = "}";
		}
	}
	else if (key == 91) { // [
		if (autobrackets) {
			if (squareBracketCounter == 0) {
				// insert opeining and closing square brackets, but move the cursor only one position
				charToInsert = "[]";
				squareBracketCounter++;
				doubleChar = true;
			}
			else {
				charToInsert = "[";
				squareBracketCounter--;
				if (squareBracketCounter == 0) squareBracketCounter = 0;
			}
		}
		else {
			charToInsert = "[";
		}
	}
	else if (key == 93) { // ]
		if (autobrackets) {
			if (squareBracketCounter > 0) {
				charToInsert = "";
				squareBracketCounter = 0;
			}
			else {
				charToInsert = "]";
			}
		}
		else {
			charToInsert = "]";
		}
	}
	else if (key == 40) { // (
		if (autobrackets) {
			if (roundBracketCounter == 0) {
				// insert opeining and closing round brackets, but move the cursor only one position
				charToInsert = "()";
				roundBracketCounter++;
				doubleChar = true;
			}
			else {
				charToInsert = "(";
				roundBracketCounter--;
				if (roundBracketCounter == 0) roundBracketCounter = 0;
			}
		}
		else {
			charToInsert = "(";
		}
	}
	else if (key == 41) { // )
		if (autobrackets) {
			if (roundBracketCounter > 0) {
				charToInsert = "";
				roundBracketCounter = 0;
			}
			else {
				charToInsert = ")";
			}
		}
		else {
			charToInsert = ")";
		}
	}
	else {
		charToInsert = char(key);
	}
	// if the cursor is at the end of the string just add the character
	int maxStringPos = (int)allLines[cursorLineIndex].str.size() - allLines[cursorLineIndex].startPos;
	if (cursorPos == maxStringPos) {
		allLines[cursorLineIndex].str += charToInsert;
	}
	else {
		// otherwise, if it's in some middle position, insert the character
		if (cursorPos > 0) {
			std::string strOne;
			if (cursorPos + allLines[cursorLineIndex].startPos >= (int)allLines[cursorLineIndex].str.size()) {
				strOne = allLines[cursorLineIndex].str;
			}
			else {
				strOne = allLines[cursorLineIndex].str.substr(0, cursorPos+allLines[cursorLineIndex].startPos);
			}
			std::string strTwo;
			if (cursorPos + allLines[cursorLineIndex].startPos >= (int)allLines[cursorLineIndex].str.size()) {
				strTwo = "";
			}
			else {
				strTwo = allLines[cursorLineIndex].str.substr(cursorPos+allLines[cursorLineIndex].startPos);
			}
			allLines[cursorLineIndex].str = strOne + charToInsert + strTwo;
			if ((int)allLines[cursorLineIndex].str.size() > maxCharactersPerString) {
				// if the string is long don't move the cursor but scroll the string
				allLines[cursorLineIndex].startPos += cursorPosIncrement;
				cursorPosIncrement = 0;
			}
		}
		// or if it's at the beginning, place the character at the beginning
		else {
			std::string str;
			if (cursorPos + allLines[cursorLineIndex].startPos >= (int)allLines[cursorLineIndex].str.size()) {
				str = "";
			}
			else {
				str = allLines[cursorLineIndex].str.substr(cursorPos+allLines[cursorLineIndex].startPos);
			}
			allLines[cursorLineIndex].str = charToInsert + str;
		}
	}
	if (cursorPos == maxCharactersPerString) {
		allLines[cursorLineIndex].startPos = (int)allLines[cursorLineIndex].str.size() - maxCharactersPerString;
		// if we're placing a double char ({} or [] or "" or ()) at the end of a long string
		if (doubleChar && allLines[cursorLineIndex].startPos == ((int)allLines[cursorLineIndex].str.size()-maxCharactersPerString)) {
			allLines[cursorLineIndex].startPos -= cursorPosIncrement;
		}
	}
	else {
		cursorPos += cursorPosIncrement;
	}
	// update the arrowCursorPos when we type
	arrowCursorPos = cursorPos;
}

//--------------------------------------------------------------
void Editor::setString(std::string s)
{
	beforeChange();
	if (s == "\n" || s == "\b") {
		int ascii = int(s[0]);
		if (ascii == 10) ascii = 13;
		int tempPaneNdx = ((ofApp*)ofGetAppPtr())->session.whichPane;
		// if we're executing code from another pane that runs Python
		// we must change the current active pane of the editor to the one receiving the string from Python
		// because parseStrings() in ofApp.cpp executes lines from the currently active pane
		if (ascii == 13 && (ctrlPressed || shiftPressed)) {
			((ofApp*)ofGetAppPtr())->session.whichPane = ((ofApp*)ofGetAppPtr())->view.whichPyPane;
		}
		allOtherKeys(ascii);
		// if we have executed code, we must reset the active pane
		if (ascii == 13 && (ctrlPressed || shiftPressed)) {
			((ofApp*)ofGetAppPtr())->session.whichPane = tempPaneNdx;
		}
	}
	else {
		std::string strWithoutTabs = replaceCharInStr(s, "\t", tabStr);
		allLines[cursorLineIndex].str += strWithoutTabs;
	}
	allLines[cursorLineIndex].startPos = ((int)allLines[cursorLineIndex].str.size() > maxCharactersPerString ? (int)allLines[cursorLineIndex].str.size() - maxCharactersPerString : 0);
	cursorPos = maxCursorPos();
	afterChange();
}

//--------------------------------------------------------------
void Editor::setHighlightManyChars(int charPos1, int charPos2, int charLine1, int charLine2)
{
	firstChar = std::min(charPos1, charPos2);
	lastChar = std::max(charPos1, charPos2);
	topLine = std::min(charLine1, charLine2);
	bottomLine = std::max(charLine1, charLine2);
	if ((charPos1 == charPos2) && (charLine1 == charLine2) && highlightManyChars) {
		highlightManyChars = false;
	}
	else {
		highlightManyChars = true;
	}
}

//--------------------------------------------------------------
bool Editor::isNumber(std::string str)
{
	// first check if there is a hyphen in the beginning
	int loopStart = 0;
	if (str[0] == '-') loopStart = 1;
	for (int i = loopStart; i < (int)str.length(); i++) {
		if (!isdigit(str[i])) {
			return false;
		}
	}
	return true;
}

//--------------------------------------------------------------
bool Editor::isFloat(std::string str)
{
	// first check if there is a hyphen in the beginning
	int loopStart = 0;
	int dotCounter = 0;
	if (str[0] == '-') loopStart = 1;
	for (int i = loopStart; i < (int)str.length(); i++) {
		if (!isdigit(str[i])) {
			if (str[i] == '.') {
				if (dotCounter == 0) dotCounter++;
				else return false;
			}
			else return false;
		}
	}
	return true;
}

//---------------------------------------------------------------
std::vector<std::string> Editor::tokenizeString(std::string str, std::string delimiter, bool addDelimiter)
{
	size_t prev = 0, pos;
	std::vector<std::string> tokens;
	while ((pos = str.find_first_of(delimiter, prev)) != std::string::npos)
	{
		if (pos > prev) {
			tokens.push_back(str.substr(prev, pos-prev));
		}
		if (addDelimiter) tokens.push_back(str.substr(pos, 1));
		prev = pos+1;
	}
	if (prev < str.length()) {
		tokens.push_back(str.substr(prev, std::string::npos));
	}
	return tokens;
}

//---------------------------------------------------------------
std::vector<int> Editor::findIndexesOfCharInStr(std::string str, std::string charToFind)
{
	size_t pos = str.find(charToFind, 0);
	std::vector<int> tokensIndexes;
	while (pos != std::string::npos) {
		tokensIndexes.push_back((int)pos);
		pos = str.find(charToFind, pos+1);
	}
	return tokensIndexes;
}

/************* separate functions for certain keys *************/
//---------------------------------------------------------------
bool Editor::setLineIndexesUpward(int lineIndex)
{
	if (lineIndex >= 0) {
		cursorLineIndex = lineIndex;
	}
	else {
		cursorLineIndex--;
	}
	if (cursorLineIndex < 0) {
		cursorLineIndex = 0;
		if (inserting) cursorPos = 0;
		return false;
	}
	else if (cursorLineIndex < lineCountOffset) {
		// the line of the cursor becomes the top line, also with a jump of more than one line (e.g. with gg)
		lineCountOffset = cursorLineIndex;
		return true;
	}
	else {
		return true;
	}
}

//---------------------------------------------------------------
void Editor::upArrow(int lineIndex)
{
	int prevCursorPos = cursorPos;
	int prevCursorLineIndex = cursorLineIndex;
	//if (lineIndex > 0) lineIndex = -(maxNumLines+1);
	// with wrapped lines too, the cursor moves to the line above (as with j and k in Vim)
	// and the pane scrolls so that the row of the cursor is visible (see scrollToCursorWrapped())
	if (setLineIndexesUpward(lineIndex)) {
		cursorPos = arrowCursorPos;
		// test if we arrive at a shorter string than the one below
		if (cursorPos >= ((int)allLines[cursorLineIndex].str.size()-allLines[cursorLineIndex].startPos)) {
			// in which case display the cursor right after the string
			cursorPos = std::min(maxCursorPos(), arrowCursorPos);
			if (!inserting && cursorPos == maxCursorPos() && cursorPos > 0) cursorPos--;
		}
		int tabIndex = didWeLandOnATab();
		if (tabIndex >= 0) cursorPos = tabIndex;
	}
	else {
		arrowCursorPos = cursorPos;
	}
	if (shiftPressed) {
		if (!highlightManyChars) {
			highlightManyCharsStart = prevCursorPos;
			// in normal mode we want to include the character below the cursor too
			if (!inserting) highlightManyCharsStart++;
			highlightManyCharsLineIndex = prevCursorLineIndex;
		}
		setHighlightManyChars(highlightManyCharsStart, cursorPos, highlightManyCharsLineIndex, cursorLineIndex);
	}
	else if (highlightManyChars) {
		highlightManyChars = false;
		upArrow(std::min(cursorLineIndex, highlightManyCharsLineIndex));
	}
}

//--------------------------------------------------------------
bool Editor::setLineIndexesDownward(int lineIndex)
{
	int diff;
	if (lineIndex >= 0) {
		diff = lineIndex - cursorLineIndex;
		cursorLineIndex = lineIndex;
	}
	else {
		diff = 1;
		cursorLineIndex++;
	}
	// the cursor can't go below the last line, in insert mode it goes to the end of the last line
	// (this doesn't depend on how far the pane has scrolled, which with wrapped lines can be further than the number of lines)
	if (cursorLineIndex > lineCount - 1) {
		cursorLineIndex = lineCount - 1;
		if (inserting) cursorPos = maxCursorPos();
		return false;
	}
	// scroll so that the line of the cursor is visible, without scrolling past the end of the text
	// with wrapped lines, scrollToCursorWrapped() scrolls by rows
	if (!wrapLines && cursorLineIndex >= lineCountOffset + maxNumLines) {
		lineCountOffset += diff;
		lineCountOffset = std::max(lineCountOffset, cursorLineIndex - maxNumLines + 1);
		lineCountOffset = std::min(lineCountOffset, std::max(0, lineCount - maxNumLines));
	}
	return true;
}

//--------------------------------------------------------------
void Editor::downArrow(int lineIndex)
{
	int prevCursorPos = cursorPos;
	int prevCursorLineIndex = cursorLineIndex;
	//if (lineIndex > 0) lineIndex = maxNumLines+1;
	// with wrapped lines too, the cursor moves to the line below (as with j and k in Vim)
	// and the pane scrolls so that the row of the cursor is visible (see scrollToCursorWrapped())
	if (setLineIndexesDownward(lineIndex)) {
		cursorPos = arrowCursorPos;
		// test if we arrive at a shorter string than the one above
		if (cursorPos >= ((int)allLines[cursorLineIndex].str.size()-allLines[cursorLineIndex].startPos)) {
			// in which case display the cursor right after the string
			cursorPos = std::min(maxCursorPos(), arrowCursorPos);
			if (!inserting && cursorPos == maxCursorPos() && cursorPos > 0) cursorPos--;
		}
		int tabIndex = didWeLandOnATab();
		if (tabIndex >= 0) cursorPos = tabIndex;
	}
	else {
		arrowCursorPos = cursorPos;
	}
	if (shiftPressed) {
		if (!highlightManyChars) {
			highlightManyCharsStart = prevCursorPos;
			highlightManyCharsLineIndex = prevCursorLineIndex;
		}
		int tempCursorPos = cursorPos;
		if (!inserting) tempCursorPos++;
		setHighlightManyChars(highlightManyCharsStart, tempCursorPos, highlightManyCharsLineIndex, cursorLineIndex);
	}
	else if (highlightManyChars) {
		highlightManyChars = false;
		downArrow(std::max(cursorLineIndex, highlightManyCharsLineIndex));
	}
}

//--------------------------------------------------------------
void Editor::rightArrow()
{
	int numPosToMove = 1;
	if (isThisATab(cursorPos)) {
		numPosToMove = TABSIZE;
	}
	else if (ctrlPressed) {
		// find the next white space from the position of the cursor
		size_t whiteSpaceIndex = 0;
		while ((int)whiteSpaceIndex <= cursorPos) {
			whiteSpaceIndex = allLines[cursorLineIndex].str.find(" ", whiteSpaceIndex+1);
			if (whiteSpaceIndex == std::string::npos) {
				whiteSpaceIndex = allLines[cursorLineIndex].str.size();
				break;
			}
		}
		numPosToMove = (int)whiteSpaceIndex - cursorPos;
		if (numPosToMove == 0) numPosToMove = 1;
	}
	// store a temporary copy of the position of the cursor before we update it to use it if shift is pressed (see further below)
	int tempCursorPos = cursorPos;
	cursorPos += numPosToMove;
	arrowCursorPos = cursorPos;
	int lastCursorPos = maxCursorPos();
	// store a temporary copy of the cursor line index before we change it to use if we press shift (see further below)
	int tempCursorLineIndex = cursorLineIndex;
	if (!inserting) lastCursorPos--; // in normal mode, we don't want to go after the last character
	// if we're one position before the end of the string or the edge of the window
	if (cursorPos > lastCursorPos) {
		// if the string is still too long to display
		if (((int)allLines[cursorLineIndex].str.size()-allLines[cursorLineIndex].startPos) > maxCharactersPerString) {
			allLines[cursorLineIndex].startPos += numPosToMove;
			// place the cursor back so that it stays at the last position
			cursorPos -= numPosToMove;
			arrowCursorPos = cursorPos;
		}
		// if we're at the end of the string
		else {
			if (inserting && cursorLineIndex < (lineCount-1)) {
				allLines[cursorLineIndex].startPos = 0;
				setLineIndexesDownward(cursorLineIndex+1);
				cursorPos = arrowCursorPos = 0;
			}
			// if we're at the last line, don't move the cursor
			else {
				cursorPos -= numPosToMove;
				arrowCursorPos = cursorPos;
				numPosToMove = 0;
			}
		}
	}
	if (shiftPressed) {
		if (!highlightManyChars && numPosToMove > 0) {
			highlightManyCharsStart = tempCursorPos;
			highlightManyCharsLineIndex = tempCursorLineIndex;
		}
		tempCursorPos = cursorPos;
		if (!inserting) tempCursorPos++;
		setHighlightManyChars(highlightManyCharsStart, tempCursorPos, highlightManyCharsLineIndex, cursorLineIndex);
	}
	else if (highlightManyChars) {
		highlightManyChars = false;
		if (cursorLineIndex == highlightManyCharsLineIndex) {
			if (cursorPos < highlightManyCharsStart) {
				cursorPos = highlightManyCharsStart;
			}
		}
	}
	arrowCursorPos = cursorPos;
}

//--------------------------------------------------------------
void Editor::leftArrow()
{
	int numPosToMove = 1;
	if (isThisATab(cursorPos-TABSIZE)) {
		numPosToMove = TABSIZE;
	}
	else if (ctrlPressed) {
		// find the previous white space from the position of the cursor
		size_t whiteSpaceIndex = allLines[cursorLineIndex].str.size();
		while ((int)whiteSpaceIndex >= cursorPos) {
			if (whiteSpaceIndex < 2) {
				whiteSpaceIndex = 0;
				break;
			}
			whiteSpaceIndex = allLines[cursorLineIndex].str.rfind(" ", whiteSpaceIndex-1);
			if (whiteSpaceIndex == std::string::npos) {
				whiteSpaceIndex = 0;
				break;
			}
		}
		numPosToMove = cursorPos - (int)whiteSpaceIndex;
		if (numPosToMove == 0) numPosToMove = 1;
	}
	// store a temporary copy of the position of the cursor before we update it to use it if shift is pressed (see further below)
	int tempCursorPos = cursorPos;
	cursorPos -= numPosToMove;
	arrowCursorPos = cursorPos;
	// store a temporary copy of the cursor line index before we change it to use if we press shift (see further below)
	int tempCursorLineIndex = cursorLineIndex;
	if (cursorPos < 0) {
		if ((int)allLines[cursorLineIndex].str.size() > maxCharactersPerString && allLines[cursorLineIndex].startPos > 0) {
			cursorPos += numPosToMove;
			arrowCursorPos = cursorPos;
			allLines[cursorLineIndex].startPos -= numPosToMove;
		}
		if (allLines[cursorLineIndex].startPos <= 0) {
			allLines[cursorLineIndex].startPos = 0;
			if (inserting && cursorLineIndex > 0) {
				cursorLineIndex--;
				cursorPos = arrowCursorPos = std::min((int)allLines[cursorLineIndex].str.size(), maxCharactersPerString);
				allLines[cursorLineIndex].startPos = (int)allLines[cursorLineIndex].str.size() - maxCharactersPerString;
			}
			else {
				cursorPos = arrowCursorPos = 0;
			}
		}
	}
	if (allLines[cursorLineIndex].startPos < 0) {
		allLines[cursorLineIndex].startPos = 0;
	}
	if (shiftPressed) {
		if (!highlightManyChars) {
			highlightManyCharsStart = tempCursorPos; // cursorPos + numPosToMove;
			// in normal mode we want to include the character below the cursor too
			if (!inserting) highlightManyCharsStart++;
			highlightManyCharsLineIndex = tempCursorLineIndex;
		}
		setHighlightManyChars(highlightManyCharsStart, cursorPos, highlightManyCharsLineIndex, cursorLineIndex);
	}
	else if (highlightManyChars) {
		highlightManyChars = false;
		if (cursorLineIndex == highlightManyCharsLineIndex) {
			if (cursorPos > highlightManyCharsStart) {
				cursorPos = highlightManyCharsStart;
			}
		}
	}
	arrowCursorPos = cursorPos;
}

//--------------------------------------------------------------
int Editor::pageRowsOf(int lineNdx)
{
	// the number of rows a line takes in the pane, one without wrapping
	return (wrapLines ? wrapRowsOf(lineNdx) : 1);
}


//--------------------------------------------------------------
void Editor::setCursorPosAfterPage(int prevCursorLineIndex)
{
	// the cursor keeps its column on the line it moves to, as with the up and down arrows
	if (cursorLineIndex == prevCursorLineIndex) return;
	cursorPos = arrowCursorPos;
	if (cursorPos >= ((int)allLines[cursorLineIndex].str.size()-allLines[cursorLineIndex].startPos)) {
		cursorPos = std::min(maxCursorPos(), arrowCursorPos);
		// in normal mode the cursor is on a character, and not after the last one
		if (!inserting && cursorPos == maxCursorPos() && cursorPos > 0) cursorPos--;
	}
	int tabIndex = didWeLandOnATab();
	if (tabIndex >= 0) cursorPos = tabIndex;
}

//--------------------------------------------------------------
void Editor::pageUp()
{
	// the top line of the pane becomes its bottom line, and the cursor goes to it
	// if there aren't enough lines above to scroll that much, the text scrolls to its beginning, and the cursor goes to the previous top line too
	// and if the text is already at its beginning, the cursor goes to the first line
	// lines are counted by the rows they take in the pane, so this works with wrapped lines too
	int prevCursorPos = cursorPos;
	int prevCursorLineIndex = cursorLineIndex;
	int oldTop = lineCountOffset;
	if (oldTop == 0 && !(wrapLines && wrapRowSkip > 0)) {
		// the text can't scroll up any more, so the cursor goes to the first line
		cursorLineIndex = 0;
	}
	else {
		// the top line when the previous top line is at the bottom of the pane
		int newTop = oldTop;
		int rows = pageRowsOf(oldTop);
		while (newTop > 0 && rows + pageRowsOf(newTop - 1) <= maxNumLines) {
			newTop--;
			rows += pageRowsOf(newTop);
		}
		// there are enough lines if the lines above didn't run out before filling the pane
		bool enoughLines = (newTop > 0 || rows >= maxNumLines);
		if (newTop == oldTop && oldTop > 0) {
			// the previous top line fills the whole pane on its own (a very long wrapped line)
			newTop = oldTop - 1;
			enoughLines = true;
		}
		lineCountOffset = (enoughLines && newTop < oldTop ? newTop : 0);
		// the cursor goes to the previous top line, which is now at the bottom of the pane
		// or, if the text scrolled less than a page to its beginning, somewhere below the top
		cursorLineIndex = oldTop;
		// unless the lines above it fill the whole pane (a very long wrapped line), in which case it goes to the top line
		int rowsAbove = 0;
		for (int i = lineCountOffset; i < oldTop; i++) rowsAbove += pageRowsOf(i);
		if (rowsAbove >= maxNumLines) cursorLineIndex = lineCountOffset;
		wrapRowSkip = 0;
	}
	setCursorPosAfterPage(prevCursorLineIndex);
	if (shiftPressed) {
		if (!highlightManyChars) {
			highlightManyCharsStart = prevCursorPos;
			// in normal mode we want to include the character below the cursor too
			if (!inserting) highlightManyCharsStart++;
			highlightManyCharsLineIndex = prevCursorLineIndex;
		}
		setHighlightManyChars(highlightManyCharsStart, cursorPos, highlightManyCharsLineIndex, cursorLineIndex);
	}
	else if (highlightManyChars) {
		highlightManyChars = false;
	}
}

//--------------------------------------------------------------
void Editor::pageDown()
{
	// the bottom line of the pane becomes its top line, and the cursor goes to it
	// if there aren't enough lines below to scroll that much, the text scrolls to its end, and the cursor goes to the previous bottom line too
	// and if the text is already at its end, the cursor goes to the last line
	// lines are counted by the rows they take in the pane, so this works with wrapped lines too
	int prevCursorPos = cursorPos;
	int prevCursorLineIndex = cursorLineIndex;
	// the last line that is visible, even if only partly (a wrapped line can continue below the pane)
	int bottomLine = lineCountOffset;
	int rows = pageRowsOf(lineCountOffset) - (wrapLines ? wrapRowSkip : 0);
	while (bottomLine + 1 < lineCount && rows < maxNumLines) {
		bottomLine++;
		rows += pageRowsOf(bottomLine);
	}
	// the top line when the last line is at the bottom of the pane
	int endTop = lineCount - 1;
	rows = pageRowsOf(endTop);
	while (endTop > 0 && rows + pageRowsOf(endTop - 1) <= maxNumLines) {
		endTop--;
		rows += pageRowsOf(endTop);
	}
	int newTop = bottomLine;
	// the top line fills the whole pane on its own (a very long wrapped line), so the next line comes to the top
	if (newTop == lineCountOffset) newTop = std::min(lineCountOffset + 1, lineCount - 1);
	if (lineCountOffset >= endTop) {
		// the text can't scroll down any more, so the cursor goes to the last line
		cursorLineIndex = lineCount - 1;
	}
	else {
		lineCountOffset = (newTop > lineCountOffset && newTop <= endTop ? newTop : endTop);
		// the cursor goes to the previous bottom line, which is now at the top of the pane
		// or, if the text scrolled less than a page to its end, somewhere above the bottom
		// (unless the previous top line filled the whole pane, and is the previous bottom line too)
		cursorLineIndex = std::max(bottomLine, lineCountOffset);
	}
	wrapRowSkip = 0;
	setCursorPosAfterPage(prevCursorLineIndex);
	if (shiftPressed) {
		if (!highlightManyChars) {
			highlightManyCharsStart = prevCursorPos;
			highlightManyCharsLineIndex = prevCursorLineIndex;
		}
		int tempCursorPos = cursorPos;
		if (!inserting) tempCursorPos++;
		setHighlightManyChars(highlightManyCharsStart, tempCursorPos, highlightManyCharsLineIndex, cursorLineIndex);
	}
	else if (highlightManyChars) {
		highlightManyChars = false;
	}
}

//--------------------------------------------------------------
void Editor::setShiftPressed(bool state)
{
	shiftPressed = state;
}

//--------------------------------------------------------------
void Editor::setCtrlPressed(bool state)
{
	ctrlPressed = state;
}

//--------------------------------------------------------------
void Editor::setAltPressed(bool state)
{
	altPressed = state;
}

//--------------------------------------------------------------
bool Editor::isShiftPressed()
{
	return shiftPressed;
}

//--------------------------------------------------------------
bool Editor::isCtrlPressed()
{
	return ctrlPressed;
}

//--------------------------------------------------------------
bool Editor::isAltPressed()
{
	return altPressed;
}

//--------------------------------------------------------------
void Editor::setInserting(bool insertingBool)
{
	// changing mode ends the current undo step
	undoGroupOpen = false;
	bool insertingStateChanged = (inserting != insertingBool ? true : false);
	inserting = insertingBool;
	if (!inserting && insertingStateChanged && blockInserting) finishBlockInsert();
	if (inserting) selectionMode = 0;
	if (!inserting) clearCompletions();
	if (inserting && showShell) {
		showShell = false;
	}
	else if (insertingStateChanged && !inserting && cursorPos > 0) {
		cursorPos--;
	}
}

//--------------------------------------------------------------
bool Editor::getInserting()
{
	return inserting;
}

//--------------------------------------------------------------
void Editor::setVisualMode(bool visualModeBool)
{
	visualMode = visualModeBool;
	if (!visualMode) selectionMode = 0;
}

//--------------------------------------------------------------
void Editor::startVisualSelection(int mode)
{
	visualMode = true;
	selectionMode = mode;
	selAnchorLine = cursorLineIndex;
	selAnchorCol = cursorPos + allLines[cursorLineIndex].startPos;
	highlightManyCharsLineIndex = cursorLineIndex;
	if (mode == 0) {
		// a selection of characters starts with the first cursor move (see rightArrow() etc.)
		highlightManyChars = false;
		return;
	}
	updateModeSelection();
}

//--------------------------------------------------------------
int Editor::getSelectionMode()
{
	return selectionMode;
}

//--------------------------------------------------------------
void Editor::updateModeSelection()
{
	if (!visualMode || selectionMode == 0) return;
	// the cursor moves with the functions that select characters (e.g. downArrow()), and the selection of lines or of a block
	// is set from them here, from its anchor to the cursor, so it is always shown, also when the cursor returns to the anchor
	highlightManyChars = true;
	highlightManyCharsLineIndex = selAnchorLine;
	topLine = std::min(selAnchorLine, cursorLineIndex);
	bottomLine = std::max(selAnchorLine, cursorLineIndex);
	int col = cursorPos + allLines[cursorLineIndex].startPos;
	blockLeft = std::min(selAnchorCol, col);
	// in normal mode the character under the cursor is included
	blockRight = std::max(selAnchorCol, col) + 1;
}

//--------------------------------------------------------------
void Editor::deleteToLineEnd()
{
	std::string& str = allLines[cursorLineIndex].str;
	int pos = cursorPos + allLines[cursorLineIndex].startPos;
	if (pos >= (int)str.size()) return;
	beforeChange(true);
	// the deleted text is yanked, so p pastes it, as in Vim
	yankedStr = str.substr(pos);
	yankedType = 0;
	str.erase(pos);
	// in normal mode the cursor goes to the last character that is left
	cursorPos = std::max(0, (int)str.size() - allLines[cursorLineIndex].startPos - 1);
	fileEdited = true;
	afterChange(true);
}

//--------------------------------------------------------------
void Editor::deleteCurrentLine()
{
	deleteLinesYanked(cursorLineIndex, cursorLineIndex);
}

//--------------------------------------------------------------
// D in visual mode, as in Vim: after v or V it deletes all the lines the selection touches, as dd does
// and after Ctrl+v it deletes from the left column of the block to the end of each line of the block
void Editor::deleteVisualLines()
{
	int top = std::min(selAnchorLine, cursorLineIndex);
	int bottom = std::max(selAnchorLine, cursorLineIndex);
	if (selectionMode == 2) {
		updateModeSelection();
		beforeChange(true);
		std::string yanked;
		for (int i = top; i <= bottom; i++) {
			if (allLines.find(i) == allLines.end()) continue;
			std::string& str = allLines[i].str;
			std::string part = blockLeft < (int)str.size() ? str.substr(blockLeft) : "";
			if (blockLeft < (int)str.size()) str.erase(blockLeft);
			yanked += replaceCharInStr(part, tabStr, "\t");
			if (i < bottom) yanked += "\n";
			allLines[i].startPos = 0;
		}
		// yanked as a block, so p pastes it as columns
		yankedStr = yanked;
		yankedType = 2;
		cursorLineIndex = top;
		cursorPos = std::max(0, std::min(blockLeft, (int)allLines[top].str.size()) - 1);
		if (cursorLineIndex < lineCountOffset) lineCountOffset = cursorLineIndex;
		highlightManyChars = false;
		setVisualMode(false);
		fileEdited = true;
		afterChange(true);
		return;
	}
	highlightManyChars = false;
	setVisualMode(false);
	deleteLinesYanked(top, bottom);
}

//--------------------------------------------------------------
// deletes whole lines, yanked as lines, so p pastes them below the cursor line
void Editor::deleteLinesYanked(int top, int bottom)
{
	beforeChange(true);
	// with their tabs as copyString() yanks them
	yankedStr = "";
	for (int i = top; i <= bottom; i++) {
		yankedStr += replaceCharInStr(allLines[i].str, tabStr, "\t");
		if (i < bottom) yankedStr += "\n";
	}
	yankedType = 1;
	deleteLines(top, bottom);
	// the cursor goes to the first character of the line that takes the place of the deleted one that is not a white space
	size_t first = allLines[cursorLineIndex].str.find_first_not_of(" ");
	cursorPos = (first == std::string::npos ? std::max(0, (int)allLines[cursorLineIndex].str.size() - 1) : (int)first);
	allLines[cursorLineIndex].startPos = 0;
	fileEdited = true;
	afterChange(true);
}

//--------------------------------------------------------------
// when typing the lines of a definition (e.g. \bar name { ... }) at the bottom of the pane, every Return moves its closing bracket down
// so after Return the pane scrolls, if needed, so that the closing bracket of the curly brackets the cursor is in stays visible
// (only after Return, moving the cursor with the arrow keys or j and k scrolls only to keep the cursor visible)
void Editor::keepClosingBracketVisible()
{
	if (thisLang != 0 || wrapLines || maxNumLines <= 0) return;
	// the first closing bracket below the cursor that has no opening bracket after the cursor, within the height of the pane
	int depth = 0;
	for (int i = cursorLineIndex; i < lineCount && i < cursorLineIndex + maxNumLines; i++) {
		const std::string& str = allLines[i].str;
		int end = commentStartOf(str);
		int start = (i == cursorLineIndex ? std::min((int)str.size(), cursorPos + allLines[i].startPos) : 0);
		for (int k = start; k < end; k++) {
			if (str[k] == '{') {
				depth++;
			}
			else if (str[k] == '}' && --depth < 0) {
				if (i >= lineCountOffset + maxNumLines) lineCountOffset = i - maxNumLines + 1;
				// the line of the cursor stays visible
				if (cursorLineIndex < lineCountOffset) lineCountOffset = cursorLineIndex;
				return;
			}
		}
	}
}

//--------------------------------------------------------------
void Editor::insertAtLineStart()
{
	const std::string& str = allLines[cursorLineIndex].str;
	size_t first = str.find_first_not_of(" \t");
	int pos = (first == std::string::npos ? (int)str.size() : (int)first);
	cursorPos = std::max(0, pos - allLines[cursorLineIndex].startPos);
	setInserting(true);
}

//--------------------------------------------------------------
void Editor::startBlockInsert()
{
	updateModeSelection();
	blockInsertTop = topLine;
	blockInsertBottom = bottomLine;
	blockInsertCol = std::min(blockLeft, (int)allLines[topLine].str.size());
	blockInsertLen = allLines[topLine].str.size();
	highlightManyChars = false;
	setVisualMode(false);
	// the cursor goes to the start of the block in its first line
	cursorLineIndex = topLine;
	cursorPos = std::max(0, blockInsertCol - allLines[topLine].startPos);
	if (cursorLineIndex < lineCountOffset) lineCountOffset = cursorLineIndex;
	setInserting(true);
	blockInserting = true;
}

//--------------------------------------------------------------
void Editor::finishBlockInsert()
{
	blockInserting = false;
	// only text typed in the first line of the block (without a new line) is copied
	if (cursorLineIndex != blockInsertTop || blockInsertBottom <= blockInsertTop) return;
	const std::string& topStr = allLines[blockInsertTop].str;
	if (topStr.size() <= blockInsertLen || blockInsertCol > (int)topStr.size()) return;
	std::string inserted = topStr.substr(blockInsertCol, topStr.size() - blockInsertLen);
	beforeChange(true);
	for (int i = blockInsertTop + 1; i <= blockInsertBottom; i++) {
		auto it = allLines.find(i);
		// lines that are shorter than the start of the block are left as they are, as in Vim
		if (it == allLines.end() || (int)it->second.str.size() < blockInsertCol) continue;
		it->second.str.insert(blockInsertCol, inserted);
	}
	fileEdited = true;
	afterChange(true);
}

//--------------------------------------------------------------
void Editor::deleteLines(int top, int bottom)
{
	int numLines = bottom - top + 1;
	if (numLines >= lineCount) {
		clearText();
		createNewLine("", 0);
		cursorLineIndex = cursorPos = lineCountOffset = 0;
		return;
	}
	for (int i = top; i <= bottom; i++) eraseMapKeys(i);
	// the lines below move up
	for (int key = bottom + 1; key < lineCount; key++) changeMapKeys(key, -numLines);
	lineCount -= numLines;
	cursorLineIndex = std::min(top, lineCount - 1);
	cursorPos = 0;
	lineCountOffset = std::min(lineCountOffset, std::max(0, lineCount - maxNumLines));
	if (cursorLineIndex < lineCountOffset) lineCountOffset = cursorLineIndex;
}

//--------------------------------------------------------------
bool Editor::getVisualMode()
{
	return visualMode;
}

//--------------------------------------------------------------
void Editor::detectExecutingChunk()
{
	// if lines have been selected, execute these, otherwise find the chunk the cursor is in
	if (!highlightManyChars) {
		std::pair<int, int> chunk = findExecutingChunk();
		if (chunk.first != chunk.second) {
			// set the lines to execute
			topLine = chunk.first;
			bottomLine = chunk.second;
			highlightManyChars = true;
		}
	}
	executingLine = cursorLineIndex;
	// set the execution variables
	if (highlightManyChars) {
		for (int i = topLine; i <= bottomLine; i++) {
			allLines[i].isBeingExecuted = true;
			allLines[i].executionTimeStamp = ofGetElapsedTimeMillis();
			allLines[i].executionDegrade = EXECUTIONBRIGHTNESS;
		}
	}
	else {
		allLines[cursorLineIndex].isBeingExecuted = true;
		allLines[cursorLineIndex].executionTimeStamp = ofGetElapsedTimeMillis();
		allLines[cursorLineIndex].executionDegrade = EXECUTIONBRIGHTNESS;
	}
}

//--------------------------------------------------------------
void Editor::allOtherKeys(int key)
{
	bool executing = false;
	bool lineBreaking = true;
	bool callAssemble = true;
	// Tab and Return can select and insert a suggestion of autocompletion
	if (completionKey(key)) return;
	// Ctrl+z and Ctrl+y undo and redo in insert mode
	if (ctrlPressed && (key == 122 || key == 121)) {
		clearCompletions();
		if (key == 122) undo();
		else redo();
		return;
	}
	// pasting and cutting are steps of their own, and a new line ends the current step
	bool pastingOrCutting = (ctrlPressed && (key == 118 || key == 120));
	beforeChange(pastingOrCutting);
	if (ctrlPressed) {
		if (key == 13) {
			// check if we're inside curly brackets so we can execute without using arrow keys
			detectExecutingChunk();
			lineBreaking = false;
			executing = true;
		}
		else if(key == 61) { // + (actually =)
			// if the alt key is pressed, the brightness of the editor increases
			// but that is being taken care of in ofApp.cpp
			callAssemble = false;
		}
		else if (key == 45) { // -
			// if the alt key is pressed, the brightness of the editor decreases
			// but that is being taken care of in ofApp.cpp
			callAssemble = false;
		}
		else if (key == 99) { // c for copy
			copyString(0); // 0 is index for copying to the clipboard
			callAssemble = false;
		}
		else if (key == 118) { // v for paste
			pasteString(0); // 0 is index for pasting from the clipboard
			callAssemble = false;
		}
		else if (key == 120) { // x for cut
			copyString(0, false); // 0 is index for copying to the clipboard, false is for not dehighlighting selected text
			deleteString();
			callAssemble = false;
		}
		else if (key == 115) { // s for save
			if (fileLoaded) {
				saveExistingFile();
			}
			else {
				saveDialog();
			}
			callAssemble = false;
		}
		else if (key == 83 && shiftPressed) { // S with shift pressed
			saveDialog();
			callAssemble = false;
		}
		else if (key == 111) { // o for open
			loadDialog();
			callAssemble = false;
		}
		else if (key == 97) { // a
			// Ctrl+a selects all text
			lineCountOffset = (lineCount < maxNumLines ? 0 : lineCount - maxNumLines);
			cursorLineIndex = lineCount - 1;
			cursorPos = maxCursorPos();
			highlightManyCharsStart = 0;
			highlightManyCharsLineIndex = 0;
			setHighlightManyChars(0, allLines[lineCount-1].str.size(), 0, lineCount-1);
			callAssemble = false;
		}
	}
	else if (shiftPressed) {
		if (key == 13) {
			// as with holding the Ctl key, do the same here
			// but position the cursor at the closing bracket
			detectExecutingChunk();
			moveCursorOnShiftReturn();
			lineBreaking = false;
			executing = true;
		}
	}
	if (callAssemble) {
		assembleString(key, executing, lineBreaking);
	}
	// with alignment on, editing a line realigns the bar definition the cursor is in, in the same undo step (see \editor.alignment)
	// typing a character, or deleting one with backspace or delete, but not new lines or Ctrl+key combinations
	bool editsText = (key == 8 || key == 127 || (key >= 32 && key < 127));
	if (callAssemble && editsText && !ctrlPressed && alignment && inserting && thisLang == 0) {
		((ofApp*)ofGetAppPtr())->alignAfterEdit(objID, key == 32);
	}
	afterChange(pastingOrCutting || key == 13);
	// the suggestions follow what is typed, but not Ctrl+key combinations (e.g. saving)
	updateArgHints(ctrlPressed ? 0 : key);
	if (ctrlPressed) clearCompletions();
	else updateCompletions();
}

//--------------------------------------------------------------
bool Editor::showingShell()
{
	return showShell;
}

//--------------------------------------------------------------
bool Editor::isTypingShell()
{
	return typingShell;
}

//--------------------------------------------------------------
void Editor::setTypingShell(bool typingShellBool)
{
	typingShell = typingShellBool;

}

//--------------------------------------------------------------
void Editor::setShowingShell(bool showingShellBool)
{
	showShell = showingShellBool;

}

//--------------------------------------------------------------
std::vector<int> Editor::setSelectedStrStartPosAndSize(int i)
{
	std::vector<int> v(2);
	// the selection of whole lines (V) and of a block of columns (Ctrl+v), relative to how much a long line has scrolled
	if (selectionMode == 1 || selectionMode == 2) {
		int lineLength = std::max(0, (int)allLines[i].str.size() - allLines[i].startPos);
		int left = (selectionMode == 1 ? 0 : std::clamp(blockLeft - allLines[i].startPos, 0, lineLength));
		int right = (selectionMode == 1 ? lineLength : std::clamp(blockRight - allLines[i].startPos, 0, lineLength));
		v[0] = left;
		v[1] = std::max(0, right - left);
		return v;
	}
	if (i == topLine) {
		if (i == bottomLine) {
			v[0] = firstChar;
			v[1] = lastChar - firstChar;
		}
		else {
			if (highlightManyCharsLineIndex < cursorLineIndex) {
				v[0] = highlightManyCharsStart;
			}
			else {
				v[0] = cursorPos;
			}
			v[1] = (int)allLines[i].str.size() - v[0];
		}
	}
	else if (i == bottomLine) {
		v[0] = 0;
		if (highlightManyCharsLineIndex < cursorLineIndex) {
			v[1] = cursorPos;
			if (!inserting) v[1]++;
		}
		else {
			v[1] = highlightManyCharsStart;
		}
	}
	else {
		v[0] = 0;
		v[1] = allLines[i].str.size();
	}
	// in normal mode the selection start is stored one position after the cursor
	// so on an empty line (or with the cursor on the last character) it can be past the end of the string
	// callers add startPos to v[0] and pass it to substr() or erase(), which throw if it's past the end
	int maxStart = std::max(0, (int)allLines[i].str.size() - allLines[i].startPos);
	if (v[0] > maxStart) v[0] = maxStart;
	if (v[0] < 0) v[0] = 0;
	if (v[1] < 0) v[1] = 0;
	return v;
}

//--------------------------------------------------------------
void Editor::copyString(int copyTo, bool dehighlight)
{
	updateModeSelection();
	if (copyTo == 1) yankedType = (highlightManyChars ? selectionMode : 0);
	std::string strToCopy = "";
	if (highlightManyChars) {
		for (int i = topLine; i <= bottomLine; i++) {
			std::vector<int> posAndSize = setSelectedStrStartPosAndSize(i);
			// apply the offset of the string, in case it's too long to fit in a pane
			posAndSize[0] += allLines[i].startPos;
			strToCopy += allLines[i].str.substr(posAndSize[0], posAndSize[1]);
			if (i < bottomLine) strToCopy += "\n";
		}
		strToCopy = replaceCharInStr(strToCopy, tabStr, "\t");
		switch (copyTo) {
			case 0:
				ofGetWindowPtr()->setClipboardString(strToCopy);
				break;
			case 1:
				yankedStr = strToCopy;
				break;
			default:
				break;
		}
	}
	else {
		switch (copyTo) {
			case 0:
				ofGetWindowPtr()->setClipboardString("");
				break;
			case 1:
				{
					// the position is relative to how far a long line has scrolled
					size_t posInLine = cursorPos + allLines[cursorLineIndex].startPos;
					yankedStr = posInLine < allLines[cursorLineIndex].str.size() ? allLines[cursorLineIndex].str.substr(posInLine, 1) : "";
				}
				break;
			default:
				break;
		}
	}
	if (visualMode) visualMode = false;
	// the dehighlight boolean is used because if we want to cut text
	// we want to copy it first, but we don't want to dehighlight it
	// because we then want to delete it too
	if (highlightManyChars && dehighlight) highlightManyChars = false;
	// deleting keeps the mode of the selection until the selection is deleted (see deleteStringUnrecorded())
	if (dehighlight) selectionMode = 0;
}

//--------------------------------------------------------------
void Editor::pasteString(int pasteFrom)
{
	// pasting is an undo step of its own
	beforeChange(true);
	pasteStringUnrecorded(pasteFrom);
	// with alignment on, the bar definitions the pasted text went to are aligned again, in the same undo step (see \editor.alignment)
	if (alignment && thisLang == 0) ((ofApp*)ofGetAppPtr())->alignAfterPaste(objID);
	afterChange(true);
}

//--------------------------------------------------------------
void Editor::pasteLinesUnrecorded(const std::vector<std::string>& lines)
{
	if (lines.empty()) return;
	int numLines = (int)lines.size();
	int at = cursorLineIndex + 1;
	// the lines below the cursor move down, starting from the last one, so that no line is moved onto another
	for (int key = lineCount - 1; key >= at; key--) changeMapKeys(key, numLines);
	lineCount += numLines;
	int savedLine = cursorLineIndex;
	for (int k = 0; k < numLines; k++) {
		cursorLineIndex = at + k - 1;
		createNewLine(lines[k], 1);
	}
	cursorLineIndex = savedLine;
	// the cursor goes to the first character of the first pasted line that is not a white space, as in Vim
	cursorLineIndex = at;
	size_t first = allLines[at].str.find_first_not_of(" ");
	cursorPos = (first == std::string::npos ? std::max(0, (int)allLines[at].str.size() - 1) : (int)first);
	allLines[at].startPos = 0;
	if (cursorLineIndex >= lineCountOffset + maxNumLines) lineCountOffset = cursorLineIndex - maxNumLines + 1;
}

//--------------------------------------------------------------
void Editor::pasteBlockUnrecorded(const std::vector<std::string>& rows)
{
	if (rows.empty()) return;
	// the block is pasted after the character under the cursor, in this line and the lines below it
	int col = cursorPos + allLines[cursorLineIndex].startPos + (allLines[cursorLineIndex].str.empty() ? 0 : 1);
	size_t width = 0;
	for (const std::string& row : rows) width = std::max(width, row.size());
	for (size_t k = 0; k < rows.size(); k++) {
		int lineNdx = cursorLineIndex + (int)k;
		// lines are added at the end of the text if the block goes further
		if (lineNdx >= lineCount) {
			int savedLine = cursorLineIndex;
			cursorLineIndex = lineCount - 1;
			createNewLine("", 1);
			lineCount++;
			cursorLineIndex = savedLine;
		}
		std::string& str = allLines[lineNdx].str;
		// shorter lines are filled with white spaces up to the column of the block
		if ((int)str.size() < col) str += std::string(col - str.size(), ' ');
		std::string row = rows[k];
		// with text after the block, its rows keep its width, so that the text after it stays aligned
		if ((int)str.size() > col && row.size() < width) row += std::string(width - row.size(), ' ');
		str.insert(col, row);
	}
	// the cursor goes to the start of the pasted block
	cursorPos = std::max(0, col - allLines[cursorLineIndex].startPos);
}

//--------------------------------------------------------------
void Editor::pasteStringUnrecorded(int pasteFrom)
{
	std::string fromClipboard;
	// first check if we do have a std::string to paste
	switch (pasteFrom) {
		case 0:
			fromClipboard = ofGetWindowPtr()->getClipboardString();
			if (fromClipboard.empty()) return;
			break;
		case 1:
			// an empty line yanked with V is pasted as an empty line
			if (yankedStr.empty() && yankedType != 1) return;
			break;
		default:
			break;
	}
	// yanked lines are pasted below the line of the cursor, and a yanked block as columns, as in Vim
	if (pasteFrom == 1 && (yankedType == 1 || yankedType == 2)) {
		// the yanked lines are separated by newlines, and they can be empty
		std::vector<std::string> lines;
		size_t start = 0;
		while (true) {
			size_t newline = yankedStr.find('\n', start);
			lines.push_back(replaceCharInStr(yankedStr.substr(start, newline == std::string::npos ? std::string::npos : newline - start), "\t", tabStr));
			if (newline == std::string::npos) break;
			start = newline + 1;
		}
		if (highlightManyChars) deleteString();
		if (yankedType == 1) pasteLinesUnrecorded(lines);
		else pasteBlockUnrecorded(lines);
		fileEdited = true;
		if (visualMode) setVisualMode(false);
		return;
	}
	std::vector<std::string> tokens;
	switch (pasteFrom) {
		case 0:
			tokens = tokenizeString(fromClipboard, "\n", false);
			break;
		case 1:
			tokens = tokenizeString(yankedStr, "\n", false);
			break;
		default:
			break;
	}
	// tokenizeString() drops empty tokens, so a string made of newlines only (e.g. copied empty lines)
	// gives no tokens, and the code below assumes at least one
	if (tokens.empty()) return;
	// first check if we're pasting over selected text
	if (highlightManyChars) deleteString();
	// set the position of the cursor according to the mode of the pane
	int tempCursorPos = cursorPos;
	if (!inserting) tempCursorPos++;
	// if we paste in the middle of a string
	// the last line must concatenate the pasted string with the remaining string of the line we broke in the middle
	std::string remainingStr = "";
	// but first we need to check if there is anything left in the string after the cursor
	if (tempCursorPos < (int)allLines[cursorLineIndex].str.size()) {
		remainingStr += allLines[cursorLineIndex].str.substr(tempCursorPos+allLines[cursorLineIndex].startPos);
	}
	// first create as many new lines as there are in what we're pasting
	size_t i = 0;
	// cursorLineIndex increments by one in postIncrementOnNewLine()
	// so we temporarily save it and restore it below
	int tempCursorLineIndex = cursorLineIndex;
	int tempCursorPos2 = cursorPos;
	cursorLineIndex = lineCount - 1;
	cursorPos = maxCursorPos();
	for (i = 0; i < tokens.size()-1; i++) {
		createNewLine("", 1);
		postIncrementOnNewLine();
	}
	i = 0;
	cursorLineIndex = tempCursorLineIndex;
	cursorPos = tempCursorPos2;
	// then change the keys of the lines below where we are pasting
	if (tokens.size() > 1) moveLineNumbers((int)tokens.size()-1);
	for (std::string originalToken : tokens) {
		std::string token = replaceCharInStr(originalToken, "\t", tabStr);
		if (i == 0) {
			allLines[tempCursorLineIndex].str = allLines[tempCursorLineIndex].str.substr(0, tempCursorPos+allLines[tempCursorLineIndex].startPos);
			allLines[tempCursorLineIndex].str += token;
		}
		if (i == (tokens.size()-1)) {
			// if we're pasting one line only just append the remaining string
			if (i == 0) {
				allLines[tempCursorLineIndex].str += remainingStr;
			}
			// otherwise concatenate the token with the remaining
			else {
				allLines[tempCursorLineIndex].str = token + remainingStr;
			}
		}
		if ((i > 0) && (i < (tokens.size()-1))) {
			allLines[tempCursorLineIndex].str = token;
		}
		i++;
		tempCursorLineIndex++;
	}
	switch (pasteFrom) {
		case 0:
			if (tokens.size() > 1) cursorLineIndex += (int)tokens.size() - 1;
			cursorPos += tokens[tokens.size()-1].size();
			break;
		case 1:
			cursorPos = tempCursorPos2;
			break;
		default:
			break;
	}
	fileEdited = true;
	if (visualMode) visualMode = false;
}

//--------------------------------------------------------------
void Editor::setWrapLines(bool wrap)
{
	if (wrap == wrapLines) return;
	// the position of the cursor in its line, as lines that scroll have the cursor position relative to how much they have scrolled
	int posInLine = cursorPos + allLines[cursorLineIndex].startPos;
	wrapLines = wrap;
	wrapRowSkip = 0;
	setMaxCharactersPerString();
	for (auto& l : allLines) l.second.startPos = 0;
	if (wrapLines) {
		// the cursor stays in the same column of the pane, in the first row of its line
		// (cursorPos is already the column, as it is relative to how much the line has scrolled)
		cursorPos = std::min(cursorPos, (int)allLines[cursorLineIndex].str.size());
	}
	else {
		// scroll the line of the cursor so that the cursor is visible
		int startPos = std::max(0, posInLine - maxCharactersPerString);
		allLines[cursorLineIndex].startPos = startPos;
		cursorPos = posInLine - startPos;
		if (cursorLineIndex < lineCountOffset) lineCountOffset = cursorLineIndex;
		if (cursorLineIndex >= lineCountOffset + maxNumLines) lineCountOffset = cursorLineIndex - maxNumLines + 1;
	}
	arrowCursorPos = cursorPos;
}

//--------------------------------------------------------------
bool Editor::getWrapLines()
{
	return wrapLines;
}

//--------------------------------------------------------------
int Editor::wrapRowsOf(int lineNdx)
{
	auto it = allLines.find(lineNdx);
	if (it == allLines.end()) return 1;
	int length = (int)it->second.str.size();
	return (length == 0 ? 1 : (length + wrapWidth - 1) / wrapWidth);
}

//--------------------------------------------------------------
std::pair<int, int> Editor::wrapRowCol(int lineNdx, int pos)
{
	int length = (int)allLines[lineNdx].str.size();
	int row = pos / wrapWidth;
	int col = pos % wrapWidth;
	if (pos > 0 && pos == length && col == 0) {
		row--;
		col = wrapWidth;
	}
	return std::make_pair(row, col);
}

//--------------------------------------------------------------
void Editor::scrollToCursorWrapped()
{
	if (cursorLineIndex < lineCountOffset) {
		lineCountOffset = cursorLineIndex;
		wrapRowSkip = 0;
	}
	if (lineCountOffset != cursorLineIndex || wrapRowSkip >= wrapRowsOf(lineCountOffset)) {
		if (lineCountOffset != cursorLineIndex) wrapRowSkip = std::min(wrapRowSkip, wrapRowsOf(lineCountOffset) - 1);
		else wrapRowSkip = 0;
	}
	int cursorRow = wrapRowCol(cursorLineIndex, cursorPos).first;
	if (lineCountOffset == cursorLineIndex && cursorRow < wrapRowSkip) wrapRowSkip = cursorRow;
	// the number of rows from the top of the pane to the row of the cursor, including it
	int rows = cursorRow + 1 - (lineCountOffset == cursorLineIndex ? wrapRowSkip : 0);
	for (int i = lineCountOffset; i < cursorLineIndex; i++) {
		rows += wrapRowsOf(i) - (i == lineCountOffset ? wrapRowSkip : 0);
	}
	// scroll down by whole lines, and by rows inside a line that is longer than the pane
	while (rows > maxNumLines) {
		if (lineCountOffset < cursorLineIndex) {
			rows -= wrapRowsOf(lineCountOffset) - wrapRowSkip;
			lineCountOffset++;
			wrapRowSkip = 0;
		}
		else {
			wrapRowSkip += rows - maxNumLines;
			rows = maxNumLines;
		}
	}
	// a line at the top of the pane is shown from its first row, unless the row of the cursor would then be below the pane
	if (lineCountOffset == cursorLineIndex && cursorRow < maxNumLines) wrapRowSkip = 0;
	// don't scroll past the end of the text: if the lines from the top of the pane to the end leave rows empty
	// at the bottom of the pane, show more lines above, as long as they fit
	if (wrapRowSkip == 0) {
		int rowsToEnd = 0;
		for (int i = lineCountOffset; i < lineCount && rowsToEnd <= maxNumLines; i++) rowsToEnd += wrapRowsOf(i);
		while (lineCountOffset > 0 && rowsToEnd + wrapRowsOf(lineCountOffset - 1) <= maxNumLines) {
			lineCountOffset--;
			rowsToEnd += wrapRowsOf(lineCountOffset);
		}
	}
}



//--------------------------------------------------------------
EditorSnapshot Editor::takeSnapshot()
{
	return EditorSnapshot{allLines, lineCount, lineCountOffset, cursorLineIndex, cursorPos};
}

//--------------------------------------------------------------
bool Editor::textDiffers(const EditorSnapshot& snapshot)
{
	if (snapshot.lineCount != lineCount || snapshot.lines.size() != allLines.size()) return true;
	for (auto it = allLines.begin(); it != allLines.end(); ++it) {
		auto other = snapshot.lines.find(it->first);
		if (other == snapshot.lines.end() || other->second.str != it->second.str) return true;
	}
	return false;
}

//--------------------------------------------------------------
int Editor::getCursorColumn()
{
	return cursorPos + allLines[cursorLineIndex].startPos;
}

//--------------------------------------------------------------
void Editor::setCursorColumn(int col)
{
	if (col < 0) col = 0;
	if (wrapLines) {
		allLines[cursorLineIndex].startPos = 0;
		cursorPos = col;
		return;
	}
	// the cursor position is relative to how far a long line has scrolled, so the line scrolls if the column is not visible
	int startPos = allLines[cursorLineIndex].startPos;
	if (col < startPos || col - startPos > maxCharactersPerString) startPos = std::max(0, col - maxCharactersPerString);
	allLines[cursorLineIndex].startPos = startPos;
	cursorPos = col - startPos;
}

//--------------------------------------------------------------
void Editor::beforeChange(bool newUndoStep)
{
	// only the outermost change is recorded, as a change can call others (e.g. backspace over a selection deletes it)
	if (changeDepth++ > 0) return;
	// typing continues the current step, if the cursor is where the previous change left it, and there was no long pause
	bool continueStep = (!newUndoStep && undoGroupOpen && cursorLineIndex == undoGroupCursorLine && cursorPos == undoGroupCursorPos &&
			ofGetElapsedTimeMillis() - undoGroupTimeStamp < 1500);
	if (continueStep) {
		hasPendingSnapshot = false;
	}
	else {
		pendingSnapshot = takeSnapshot();
		hasPendingSnapshot = true;
		undoGroupOpen = false;
	}
}

//--------------------------------------------------------------
void Editor::afterChange(bool endUndoStep)
{
	if (--changeDepth > 0) return;
	if (changeDepth < 0) changeDepth = 0;
	// with alignment on, a change of the text in normal mode (e.g. x, dd, dw, or deleting a selection) aligns the bar definitions again
	// in the same undo step, as typing and pasting do in insert mode (see allOtherKeys() and pasteString())
	if (alignment && !inserting && thisLang == 0 && hasPendingSnapshot && textDiffers(pendingSnapshot)) {
		((ofApp*)ofGetAppPtr())->alignAfterPaste(objID);
	}
	// the text as it was before the change is stored only if the text changed
	// so that moving the cursor or executing code doesn't create steps that undo nothing
	if (hasPendingSnapshot) {
		if (textDiffers(pendingSnapshot)) {
			undoStack.push_back(std::move(pendingSnapshot));
			// the oldest steps are dropped, so that the history doesn't grow without limit
			if (undoStack.size() > 200) undoStack.erase(undoStack.begin());
			// a new change makes the steps that were undone impossible to redo
			redoStack.clear();
			undoGroupOpen = true;
		}
		hasPendingSnapshot = false;
	}
	if (undoGroupOpen) {
		undoGroupCursorLine = cursorLineIndex;
		undoGroupCursorPos = cursorPos;
		undoGroupTimeStamp = ofGetElapsedTimeMillis();
	}
	if (endUndoStep) undoGroupOpen = false;
}

//--------------------------------------------------------------
void Editor::restoreSnapshot(const EditorSnapshot& snapshot)
{
	allLines = snapshot.lines;
	lineCount = snapshot.lineCount;
	cursorLineIndex = std::max(0, std::min(snapshot.cursorLineIndex, lineCount - 1));
	cursorPos = arrowCursorPos = std::max(0, std::min(snapshot.cursorPos, (int)allLines[cursorLineIndex].str.size()));
	// scroll so that the cursor is visible
	lineCountOffset = snapshot.lineCountOffset;
	if (cursorLineIndex < lineCountOffset) lineCountOffset = cursorLineIndex;
	if (cursorLineIndex >= lineCountOffset + maxNumLines) lineCountOffset = cursorLineIndex - maxNumLines + 1;
	if (lineCountOffset < 0) lineCountOffset = 0;
	// lines that were being executed when the snapshot was taken are not being executed now
	for (auto& l : allLines) {
		l.second.isBeingExecuted = false;
		l.second.executionDegrade = 0;
	}
	highlightManyChars = false;
	quoteCounter = curlyBracketCounter = squareBracketCounter = roundBracketCounter = 0;
	undoGroupOpen = false;
	fileEdited = true;
}

//--------------------------------------------------------------
void Editor::undo()
{
	if (undoStack.empty()) return;
	redoStack.push_back(takeSnapshot());
	restoreSnapshot(undoStack.back());
	undoStack.pop_back();
}

//--------------------------------------------------------------
void Editor::redo()
{
	if (redoStack.empty()) return;
	undoStack.push_back(takeSnapshot());
	restoreSnapshot(redoStack.back());
	redoStack.pop_back();
}

//--------------------------------------------------------------
void Editor::clearUndoHistory()
{
	undoStack.clear();
	redoStack.clear();
	hasPendingSnapshot = false;
	undoGroupOpen = false;
}

//--------------------------------------------------------------
void Editor::clearText()
{
	allLines.clear();
	lineCount = 1;
	lineCountOffset = 0;
	cursorLineIndex = cursorPos = 0;
	highlightManyChars = false;
}

//--------------------------------------------------------------
void Editor::deleteString()
{
	// deleting a selection (or a character in normal mode) is an undo step of its own
	beforeChange(true);
	deleteStringUnrecorded();
	afterChange(true);
}

//--------------------------------------------------------------
void Editor::deleteStringUnrecorded()
{
	if (highlightManyChars && selectionMode == 1) {
		// whole lines are deleted with their line breaks
		deleteLines(topLine, bottomLine);
		selectionMode = 0;
		highlightManyChars = false;
		fileEdited = true;
		return;
	}
	if (highlightManyChars && selectionMode == 2) {
		// the columns of the block are deleted from all its lines
		for (int i = topLine; i <= bottomLine; i++) {
			if (allLines.find(i) == allLines.end()) continue;
			std::vector<int> posAndSize = setSelectedStrStartPosAndSize(i);
			if (posAndSize[1] > 0) allLines[i].str.erase(posAndSize[0] + allLines[i].startPos, posAndSize[1]);
		}
		cursorLineIndex = topLine;
		int lineLength = std::max(0, (int)allLines[topLine].str.size() - allLines[topLine].startPos);
		cursorPos = std::clamp(blockLeft - allLines[topLine].startPos, 0, std::max(0, lineLength - 1));
		if (cursorLineIndex < lineCountOffset) lineCountOffset = cursorLineIndex;
		selectionMode = 0;
		highlightManyChars = false;
		fileEdited = true;
		return;
	}
	if (!inserting && !highlightManyChars) {
		// the position is relative to how far a long line has scrolled
		size_t posInLine = cursorPos + allLines[cursorLineIndex].startPos;
		if (posInLine < allLines[cursorLineIndex].str.size()) allLines[cursorLineIndex].str.erase(posInLine, 1);
	}
	else if (highlightManyChars) {
		// if we're deleting all text
		if (bottomLine - topLine == lineCount - 1 && lastChar - firstChar == (int)allLines[cursorLineIndex].str.size()) {
			clearText();
			createNewLine("", 0);
			return;
		}
		int numLinesToDelete = bottomLine - topLine;
		lineCount -= numLinesToDelete;
		for (int i = topLine; i <= bottomLine; i++) {
			if ((i > topLine) && (i < bottomLine)) {
				eraseMapKeys(i);
			}
			else {
				std::vector<int> posAndSize = setSelectedStrStartPosAndSize(i);
				if (i == topLine) {
					size_t strSizeBefore = allLines[i].str.size();
					// in normal mode the selection already includes the character under the cursor (see leftArrow() and rightArrow())
					// so it is deleted as it is, as it is yanked by copyString()
					// the position is relative to how far a long line has scrolled, so its offset is added, as in copyString()
					allLines[i].str.erase(posAndSize[0] + allLines[i].startPos, posAndSize[1]);
					if (allLines[i].str.size() == 0) {
						// if we detele the entire line, the cursor is repositioned automatically
						releaseTraceback(i);
					}
					// if we're erasing characters in one string only
					else if (topLine == bottomLine && posAndSize[0]+posAndSize[1] < (int)strSizeBefore) {
						// reposition the cursor
						cursorPos = posAndSize[0];
					}
				}
				else if (i == bottomLine) {
					allLines[i].str.erase(posAndSize[0] + allLines[i].startPos, posAndSize[1]);
					// position the cursor at the concatenation point
					cursorPos = (int)allLines[topLine].str.size();
					// then concatenate the top and bottom lines to one string
					allLines[topLine].str += allLines[bottomLine].str;
					// and erase the bottom line as it has been embedded into the top one
					eraseMapKeys(i);
				}
			}
		}
		if (cursorLineIndex > highlightManyCharsLineIndex) {
			cursorLineIndex = highlightManyCharsLineIndex;
		}
		if (numLinesToDelete > 0) {
			moveLineNumbers(-numLinesToDelete);
			lineCountOffset -= numLinesToDelete;
			if (lineCountOffset < 0) lineCountOffset = 0;
		}
	}
	fileEdited = true;
	if (highlightManyChars) highlightManyChars = false;
}

//--------------------------------------------------------------
std::vector<int> Editor::sortVec(std::vector<int> v)
{
	sort(v.begin(), v.end());
	return v;
}

//--------------------------------------------------------------
void Editor::setTraceback(int errorCode, std::string errorStr, int lineNum, size_t strBreakPnt)
{
	// error codes are: 0 - nothing, 1 - note, 2 - warning, 3 - error
	if (errorCode == 0 || errorCode == 1) allLines[lineNum].tracebackColor = 0;
	else if (errorCode == 2) allLines[lineNum].tracebackColor = 1;
	else if (errorCode == 3) allLines[lineNum].tracebackColor = 2;
	else allLines[lineNum].tracebackColor = 0;
	// find number of newlines in traceback std::string
	int numLines = count(begin(errorStr), end(errorStr), '\n') + 1;
	allLines[lineNum].tracebackStr = errorStr;
	allLines[lineNum].tracebackTimeStamp = ofGetElapsedTimeMillis();
	allLines[lineNum].tracebackNumLines = numLines;
	allLines[lineNum].tracebackStrBreakPnt = strBreakPnt;
	// a new message replaces the output of a shell command (e.g. :pwd) that might still be shown in its place
	// unless a shell command is being typed
	if (!typingShell) showShell = false;
}

//--------------------------------------------------------------
void Editor::releaseTraceback(int lineNum)
{
	std::map<int, line>::iterator it = allLines.find(lineNum);
	if (it != allLines.end()) {
		allLines[lineNum].tracebackStr = "";
		allLines[lineNum].tracebackColor = 0;
		allLines[lineNum].tracebackNumLines = 1;
		allLines[lineNum].tracebackStrBreakPnt = 0;
	}
}

//--------------------------------------------------------------
std::string Editor::getTracebackStr(int lineNum)
{
	return allLines[lineNum].tracebackStr;
}

//--------------------------------------------------------------
int Editor::getTracebackColor(int lineNum)
{
	return allLines[lineNum].tracebackColor;
}

//--------------------------------------------------------------
uint64_t Editor::getTracebackTimeStamp(int lineNum)
{
	return allLines[lineNum].tracebackTimeStamp;
}

//--------------------------------------------------------------
void Editor::setTracebackTimeStamp(int lineNum, uint64_t stamp)
{
	allLines[lineNum].tracebackTimeStamp = stamp;
}

//--------------------------------------------------------------
int Editor::getTracebackNumLines(int lineNum)
{
	return allLines[lineNum].tracebackNumLines;
}

//--------------------------------------------------------------
uint64_t Editor::getTracebackDur()
{
	return TRACEBACKDUR;
}

//--------------------------------------------------------------
std::map<int, line>::iterator Editor::getAllLinesBegin()
{
	return allLines.begin();
}

//--------------------------------------------------------------
std::map<int, line>::iterator Editor::getAllLinesEnd()
{
	return allLines.end();
}

//--------------------------------------------------------------
// single characters received over OSC for press and release
void Editor::fromOscPress(int ascii)
{
	if (!activity) {
		// call overloaded keyPressed function to set a temporary whichEditor in ofApp.cpp
		((ofApp*)ofGetAppPtr())->keyPressedOsc(ascii, objID);
	}
	else {
		((ofApp*)ofGetAppPtr())->keyPressed(ascii);
	}
}

//--------------------------------------------------------------
void Editor::fromOscRelease(int ascii)
{
	if (!activity) {
		// the same applies to releasing a key
		((ofApp*)ofGetAppPtr())->keyReleasedOsc(ascii, objID);
	}
	else {
		((ofApp*)ofGetAppPtr())->keyReleased(ascii);
	}
}

//--------------------------------------------------------------
void Editor::saveDialog()
{
	ofFileDialogResult saveFileResult = ofSystemSaveDialog("LiveLily_session.lyv", "Save file");
	if (saveFileResult.bSuccess) {
		std::string fileName = saveFileResult.filePath;
		saveFile(fileName);
	}
}

//--------------------------------------------------------------
void Editor::saveFile(std::string fileName)
{
	//std::cout << "saving \"" << fileName << "\"\n";
	if (!endsWith(fileName, ".lyv") && !endsWith(fileName, ".py") && !endsWith(fileName, ".lua")) {
		fileName += ".lyv";
	}
	std::ofstream file(fileName.c_str());
	if (!file.is_open()) {
		// if for some reason the file can't be opened
		// instead of the file name, the string "could not open file"
		// will be displayed for 2 seconds and then it will return to untitled.lyv
		couldNotSaveFile = true;
		fileErrorStr = "could not save file";
		fileLoadErrorTimeStamp = ofGetElapsedTimeMillis();
		return;
	}
	for (unsigned i = 0; i < allLines.size(); i++) {
		file << allLines[i].str << "\n";
	}
	file.close();
	fileLoaded = true;
	size_t lastBackSlash = fileName.find_last_of("/");
	loadedFileStr = fileName.substr(lastBackSlash+1);
	loadedFileFullPath = fileName;
	convertedFile = false;
	if (endsWith(loadedFileStr, ".lyv")) thisLang = 0;
	else if (endsWith(loadedFileStr, ".py")) thisLang = 1;
	else if (endsWith(loadedFileStr, ".lua")) thisLang = 2;
	else thisLang = 0;
	fileEdited = false;
}

//--------------------------------------------------------------
void Editor::saveExistingFile()
{
	if (!fileLoaded) {
		// if we try to save a file that doesn't already exist
		// instead of the file name, the string below
		// will be displayed for 2 seconds and then it will return to untitled.lyv
		couldNotSaveFile = true;
		fileErrorStr = "can't overwrite non-existing file, use :save";
		fileLoadErrorTimeStamp = ofGetElapsedTimeMillis();
		return;
	}
	// a file converted from MusicXML has not been saved yet, and its .lyv name might belong to another file
	// which must not be overwritten without asking for it explicitly with :save
	if (convertedFile && std::filesystem::exists(loadedFileFullPath)) {
		couldNotSaveFile = true;
		fileErrorStr = loadedFileFullPath.substr(loadedFileFullPath.find_last_of("/")+1) + " already exists, use :save";
		fileLoadErrorTimeStamp = ofGetElapsedTimeMillis();
		return;
	}
	std::ofstream file(loadedFileFullPath.c_str());
	if (!file.is_open()) {
		// if for some reason the file can't be opened
		// instead of the file name, the string "could not open file"
		// will be displayed for 2 seconds and then it will return to untitled.lyv
		couldNotSaveFile = true;
		fileErrorStr = "could not open file";
		fileLoadErrorTimeStamp = ofGetElapsedTimeMillis();
		return;
	}
	for (unsigned i = 0; i < allLines.size(); i++) {
		file << allLines[i].str << "\n";
	}
	file.close();
	fileEdited = false;
	// a file converted from MusicXML is now saved as a .lyv file, so its name shows the .lyv extension from now on
	if (convertedFile) loadedFileStr = loadedFileFullPath.substr(loadedFileFullPath.find_last_of("/")+1);
	convertedFile = false;
}

//--------------------------------------------------------------
void Editor::loadConvertedLines(const std::vector<std::string>& convertedLines)
{
	// the conversion of MusicXML and MIDI files to LiveLily code is done in musicxml.cpp and midifile.cpp
	// before the editor is cleared (see loadFile()), and here we only write the resulting lines to the editor
	for (const std::string& line : convertedLines) {
		createNewLine(line, 0);
		cursorLineIndex++;
	}
}

//////////////////// loading LiveLily files ////////////////////

//--------------------------------------------------------------
void Editor::loadTextFile(std::string filePath)
{
	std::ifstream file(filePath.c_str());
	std::string line;
	while (getline(file, line)) {
		std::string lineWithoutTabs = replaceCharInStr(line, "\t", tabStr);
		createNewLine(lineWithoutTabs, 0);
		cursorLineIndex++;
	}
	file.close();
}

///////////////// load files graphic interface /////////////////

//--------------------------------------------------------------
void Editor::loadDialog()
{
	// Open the Open File Dialog
	ofFileDialogResult openFileResult = ofSystemLoadDialog("Select a .lyv, .xml, .musicxml, .mxl, .mid, .py or .lua file");
	// Check if the user opened a file
	if (openFileResult.bSuccess){
		loadFile(openFileResult.filePath);
	}
}

/////////////////// main load file function ////////////////////

//--------------------------------------------------------------
bool Editor::loadFile(std::string fileName)
{
	lineCount = 1;
	cursorPos = 0;
	if (!endsWith(fileName, ".lyv") && !endsWith(fileName, ".xml") && !endsWith(fileName, ".musicxml") && !endsWith(fileName, ".mxl") && \
			!endsWith(fileName, ".py") && !endsWith(fileName, ".lua") && !endsWith(fileName, ".mid") && !endsWith(fileName, ".midi")) {
		fileName += ".lyv";
	}
	std::ifstream file(fileName.c_str());
	if (!file.is_open()) {
		// if for some reason the file can't be opened
		// instead of the file name, the string "could not open file"
		// will be displayed for 2 seconds and then it will return to untitled.lyv
		couldNotLoadFile = true;
		fileErrorStr = "could not load file";
		fileLoadErrorTimeStamp = ofGetElapsedTimeMillis();
		return false;
	}
	file.close();
	// MusicXML and MIDI files are converted to LiveLily code before the editor is cleared
	// so that if they can't be converted (e.g. a damaged file), the current contents of the editor are kept
	bool openingXML = (endsWith(fileName, ".mxl") || endsWith(fileName, ".xml") || endsWith(fileName, ".musicxml"));
	bool openingMIDI = (endsWith(fileName, ".mid") || endsWith(fileName, ".midi"));
	std::vector<std::string> convertedLines;
	std::string conversionError;
	if (openingXML) {
		MusicXMLLoader loader(tabStr);
		convertedLines = loader.load(fileName);
		conversionError = loader.getError();
	}
	else if (openingMIDI) {
		MIDIFileLoader loader(tabStr);
		convertedLines = loader.load(fileName);
		conversionError = loader.getError();
	}
	if (!conversionError.empty()) {
		couldNotLoadFile = true;
		fileErrorStr = conversionError;
		fileLoadErrorTimeStamp = ofGetElapsedTimeMillis();
		return false;
	}
	bool converting = (openingXML || openingMIDI);
	clearText();
	// determine what type of file we're opeining
	size_t lastBackSlash = fileName.find_last_of("/");
	loadedFileStr = fileName.substr(lastBackSlash+1);
	loadedFileFullPath = fileName;
	convertedFile = false;
	if (converting) {
		// the converted file is LiveLily code, so it is saved with a .lyv name (e.g. score.musicxml is saved as score.lyv)
		// so that saving it doesn't overwrite the MusicXML or MIDI file it was converted from
		// the name shown keeps the original extension until the file is saved (see saveExistingFile())
		size_t fullPathDotNdx = loadedFileFullPath.find_last_of(".");
		if (fullPathDotNdx != std::string::npos && fullPathDotNdx > (lastBackSlash == std::string::npos ? 0 : lastBackSlash)) {
			loadedFileFullPath = loadedFileFullPath.substr(0, fullPathDotNdx);
		}
		loadedFileFullPath += ".lyv";
		convertedFile = true;
		thisLang = 0;
	}
	else if (endsWith(loadedFileStr, ".lyv")) {
		thisLang = 0;
	}
	else if (endsWith(loadedFileStr, ".py")) {
		((ofApp*)ofGetAppPtr())->session.initPyo();
		thisLang = 1;
	}
	else if (endsWith(loadedFileStr, ".lua")) {
		thisLang = 2;
	}
	else {
		thisLang = 0;
	}
	if (converting) {
		loadConvertedLines(convertedLines);
	}
	else {
		loadTextFile(fileName);
	}
	// an empty file leaves no lines, but the rest of the editor assumes there is at least one
	if (allLines.empty()) {
		cursorLineIndex = 0;
		createNewLine("", 0);
	}
	lineCount = (int)allLines.size();
	cursorLineIndex = 0;
	cursorPos = arrowCursorPos = 0;
	fileLoaded = true;
	// a file converted from MusicXML has not been saved as a .lyv file yet, so it is shown as edited (with a *)
	fileEdited = convertedFile;
	clearUndoHistory();
	return true;
}

//////////////////// loading help files ////////////////////////

//--------------------------------------------------------------
void Editor::storeBufferForHelp()
{
	tempAllLines.clear();
	for (auto it = allLines.begin(); it != allLines.end(); ++it) {
		tempAllLines[it->first] = it->second;
	}
	tempFileLoaded = fileLoaded;
	tempUndoStack = undoStack;
	tempRedoStack = redoStack;
	tempFileName = loadedFileStr;
	tempFileFullPath = loadedFileFullPath;
	tempConvertedFile = convertedFile;
	tempFileEdited = fileEdited;
	tempCursorLineIndex = cursorLineIndex;
	tempCursorPos = cursorPos;
	tempExecutingLine = executingLine;
	tempLang = thisLang;
}

//--------------------------------------------------------------
bool Editor::loadHelpFile(std::string fileName, int newPane)
{
	storeBufferForHelp();
	bool fileOpened = loadFile(fileName);
	if (!fileOpened) {
		tempAllLines.clear();
		tempFileName.clear();
		undoStack = tempUndoStack;
		redoStack = tempRedoStack;
		fileLoaded = tempFileLoaded;
		loadedFileFullPath = tempFileFullPath;
		convertedFile = tempConvertedFile;
		fileEdited = tempFileEdited;
		cursorLineIndex = tempCursorLineIndex;
		cursorPos = tempCursorPos;
		lineCount = (int)allLines.size();
	}
	if (newPane == 0) helpFileOpen = fileOpened;
	return fileOpened;
}

//--------------------------------------------------------------
void Editor::loadHelpText(const std::string& name, const std::string& text)
{
	// a help that is already open is replaced, and closing the new one returns to the text of the pane, not to the help it replaced
	if (helpFileOpen) closeHelpFile();
	storeBufferForHelp();
	clearText();
	cursorLineIndex = 0;
	std::vector<std::string> lines;
	size_t start = 0;
	while (start <= text.size()) {
		size_t end = text.find('\n', start);
		if (end == std::string::npos) end = text.size();
		lines.push_back(text.substr(start, end - start));
		start = end + 1;
	}
	// without the empty lines at the end
	while (lines.size() > 1 && lines.back().find_first_not_of(" \t") == std::string::npos) lines.pop_back();
	loadConvertedLines(lines);
	if (allLines.empty()) createNewLine("", 0);
	lineCount = (int)allLines.size();
	cursorLineIndex = 0;
	cursorPos = arrowCursorPos = 0;
	lineCountOffset = 0;
	// the help has no file, so it is shown as help(name), and :w doesn't write it anywhere (it can be saved with :save)
	fileLoaded = true;
	loadedFileStr = "help(" + name + ")";
	loadedFileFullPath.clear();
	convertedFile = false;
	fileEdited = false;
	thisLang = 1;
	clearUndoHistory();
	helpFileOpen = true;
}

////// close an open help file and restore editor lines ////////

//--------------------------------------------------------------
void Editor::closeHelpFile()
{
	clearText();
	for (auto it = tempAllLines.begin(); it != tempAllLines.end(); ++it) {
		allLines[it->first] = it->second;
	}
	tempAllLines.clear();
	// the history of the file, as it was before the help file was opened
	undoStack = tempUndoStack;
	redoStack = tempRedoStack;
	tempUndoStack.clear();
	tempRedoStack.clear();
	undoGroupOpen = false;
	// restore the name and the path of the file, so that saving doesn't write to the help file
	if (tempFileLoaded) {
		loadedFileStr = tempFileName;
		loadedFileFullPath = tempFileFullPath;
	}
	convertedFile = tempConvertedFile;
	fileLoaded = tempFileLoaded;
	fileEdited = tempFileEdited;
	cursorLineIndex = tempCursorLineIndex;
	cursorPos = tempCursorPos;
	lineCount = (int)allLines.size();
	// the line that was executed (e.g. by \help or help()), if it is still there, without creating it
	auto executedIt = allLines.find(tempExecutingLine);
	if (executedIt != allLines.end()) {
		executedIt->second.isBeingExecuted = false;
		executedIt->second.executionDegrade = 0;
	}
	tempFileName.clear();
	thisLang = tempLang;
	helpFileOpen = false;
}

//--------------------------------------------------------------
bool Editor::isHelpFileOpen()
{
	return helpFileOpen;
}

/********************* loading files done *********************/

//--------------------------------------------------------------
float Editor::getCursorHeight()
{
	return cursorHeight;
}

//--------------------------------------------------------------
void Editor::printVector(std::vector<int> v)
{
	for (unsigned i = 0; i < v.size(); i++) {
		std::cout << v[i] << " ";
	}
	std::cout << endl;
}

//--------------------------------------------------------------
void Editor::printVector(std::vector<std::string> v)
{
	for (unsigned i = 0; i < v.size(); i++) {
		std::cout << v[i] << " ";
	}
	std::cout << endl;
}

//--------------------------------------------------------------
void Editor::printVector(std::vector<float> v)
{
	for (unsigned i = 0; i < v.size(); i++) {
		std::cout << v[i] << " ";
	}
	std::cout << endl;
}

/******************** autocompletion **********************/
//--------------------------------------------------------------
void Editor::clearCompletions()
{
	completions.clear();
	completionNdx = -1;
	completionPrefixLen = 0;
	completionLine = completionPos = -1;
}

//--------------------------------------------------------------
// finds the word that is typed before the cursor and the suggestions for it
// in LiveLily, suggestions appear after a backslash and the first character of a command
// and after a command (or an object, like an instrument) and a dot, with the first character of a method
// in Python they appear with the first character of a name
void Editor::updateCompletions()
{
	clearCompletions();
	if (!autocompletion || !inserting || typingShell || highlightManyChars) return;
	if (thisLang != 0 && thisLang != 1) return;
	const std::string& str = allLines[cursorLineIndex].str;
	int pos = cursorPos + allLines[cursorLineIndex].startPos;
	if (pos <= 0 || pos > (int)str.size()) return;
	// no suggestions in comments
	if (pos > commentStartOf(str)) return;
	// in LiveLily, names of bars and loops can have hyphens (e.g. \1-4, or the bars of \bars, like \name-1), so they are part of a word
	// (suggestions appear only after a backslash, so a hyphen of an articulation, e.g. c4-., is never part of one)
	// in Python a hyphen is the minus sign
	bool lilyNames = (thisLang == 0);
	auto isWordChar = [lilyNames](char c) { return isalnum((unsigned char)c) || c == '_' || (lilyNames && c == '-'); };
	// no suggestions with the cursor in the middle of a word
	if (pos < (int)str.size() && isWordChar(str[pos])) return;
	int start = pos;
	while (start > 0 && isWordChar(str[start-1])) start--;
	// in Python, the attributes of an object are suggested right after the dot (e.g. a.), before any letter is typed
	bool pythonDot = (thisLang == 1 && start == pos && str[pos-1] == '.');
	if (start == pos && !pythonDot) return;
	std::string word = str.substr(start, pos - start);
	std::string typed;
	std::vector<std::string> candidates;
	if (thisLang == 0) {
		if (start > 0 && str[start-1] == '\\') {
			typed = "\\" + word;
			candidates = ((ofApp*)ofGetAppPtr())->getCommandCompletions();
		}
		else if (start > 0 && str[start-1] == '.') {
			// the name before the dot, which must start with a backslash
			int nameEnd = start - 1;
			int nameStart = nameEnd;
			while (nameStart > 0 && isWordChar(str[nameStart-1])) nameStart--;
			if (nameStart == nameEnd || nameStart == 0 || str[nameStart-1] != '\\') return;
			typed = word;
			candidates = ((ofApp*)ofGetAppPtr())->getMethodCompletions("\\" + str.substr(nameStart, nameEnd - nameStart));
		}
		// the arguments of commands and methods (e.g. pianoroll in \score.show pianoroll), after a white space, in a line that starts with a command
		// but not in a melodic line (a line that starts with an instrument or a group), where words are notes
		else if (start > 0 && (str[start-1] == ' ' || str[start-1] == '\t')) {
			size_t lineStart = str.find_first_not_of(" \t");
			if (lineStart == std::string::npos || str[lineStart] != '\\') return;
			size_t firstEnd = str.find_first_of(" \t.", lineStart);
			std::string firstWord = str.substr(lineStart, (firstEnd == std::string::npos ? std::string::npos : firstEnd - lineStart));
			if (((ofApp*)ofGetAppPtr())->startsMelodicLine(firstWord)) return;
			typed = word;
			candidates = ((ofApp*)ofGetAppPtr())->getArgumentCompletions();
		}
		else {
			return;
		}
	}
	else {
		if (!word.empty() && isdigit((unsigned char)word[0])) return;
		// while a parameter of the hint of arguments is typed (e.g. fr of freq=1000), Tab completes it (see argHintText())
		bool typingKeyword = false;
		argHintText(&typingKeyword);
		if (typingKeyword) return;
		typed = word;
		if (pythonDot) {
			// only the attributes the interpreter knows (not the names of the pane, e.g. after the dot of a number like 1.)
			// without the names that start with an underscore, which are suggested once an underscore is typed
			std::string dotted = pythonDottedName(str, pos - 1);
			if (dotted.empty()) return;
			for (const std::string& name : ((ofApp*)ofGetAppPtr())->pythonNames(dotted)) {
				if (name[0] != '_') candidates.push_back(name);
			}
		}
		else {
			candidates = pythonCompletions(start > 0 && str[start-1] == '.', cursorLineIndex, start);
		}
	}
	for (const std::string& candidate : candidates) {
		if (candidate.size() >= typed.size() && candidate.compare(0, typed.size(), typed) == 0) completions.push_back(candidate);
	}
	// a word that is already complete needs no suggestion
	if (completions.size() == 1 && completions[0] == typed) completions.clear();
	if (completions.empty()) return;
	completionPrefixLen = (int)typed.size();
	completionLine = cursorLineIndex;
	completionPos = pos;
}

//--------------------------------------------------------------
// the keywords and builtins of Python, and the names in the pane
// after a dot, only the names in the pane, as these can be attributes and methods
// the word that is being typed (in line wordLine, starting at wordStart) is not included
std::vector<std::string> Editor::pythonCompletions(bool afterDot, int wordLine, int wordStart)
{
	static const std::vector<std::string> keywords = {
		"False", "None", "True", "and", "as", "assert", "async", "await", "break", "case", "class", "continue", "def", "del",
		"elif", "else", "except", "finally", "for", "from", "global", "if", "import", "in", "is", "lambda", "match",
		"nonlocal", "not", "or", "pass", "raise", "return", "self", "try", "while", "with", "yield",
		"abs", "all", "any", "ascii", "bin", "bool", "breakpoint", "bytearray", "bytes", "callable", "chr", "classmethod",
		"compile", "complex", "delattr", "dict", "dir", "divmod", "enumerate", "eval", "exec", "filter", "float", "format",
		"frozenset", "getattr", "globals", "hasattr", "hash", "help", "hex", "id", "input", "int", "isinstance",
		"issubclass", "iter", "len", "list", "locals", "map", "max", "memoryview", "min", "next", "object", "oct", "open",
		"ord", "pow", "print", "property", "range", "repr", "reversed", "round", "set", "setattr", "slice", "sorted",
		"staticmethod", "str", "sum", "super", "tuple", "type", "vars", "zip"
	};
	std::set<std::string> names;
	ofApp* app = (ofApp*)ofGetAppPtr();
	if (afterDot) {
		// the attributes of the object before the dot (e.g. random of np.ra), if the interpreter knows it (its code has been executed)
		// otherwise (e.g. an expression like f().), the names of the pane, as before
		std::string dotted = pythonDottedName(allLines[wordLine].str, wordStart - 1);
		if (!dotted.empty()) {
			std::vector<std::string> attributes = app->pythonNames(dotted);
			if (!attributes.empty()) return attributes;
		}
	}
	else {
		names.insert(keywords.begin(), keywords.end());
		// the names that executed code imported or defined (e.g. with import numpy as np, or from pyo import *), and the builtins
		std::vector<std::string> globals = app->pythonNames("");
		names.insert(globals.begin(), globals.end());
	}
	for (const auto& lineIt : allLines) {
		const std::string& str = lineIt.second.str;
		int end = commentStartOf(str);
		int i = 0;
		while (i < end) {
			if (isalpha((unsigned char)str[i]) || str[i] == '_') {
				int nameStart = i;
				while (i < end && (isalnum((unsigned char)str[i]) || str[i] == '_')) i++;
				if (i - nameStart > 1 && !(lineIt.first == wordLine && nameStart == wordStart)) names.insert(str.substr(nameStart, i - nameStart));
			}
			else if (isdigit((unsigned char)str[i])) {
				// skip numbers, so that e.g. the e of 1e3 is not a name
				while (i < end && (isalnum((unsigned char)str[i]) || str[i] == '.')) i++;
			}
			else {
				i++;
			}
		}
	}
	return std::vector<std::string>(names.begin(), names.end());
}

//--------------------------------------------------------------
std::string Editor::pythonDottedName(const std::string& str, int end)
{
	if (end <= 0 || end > (int)str.size()) return "";
	int start = end;
	while (start > 0 && (isalnum((unsigned char)str[start-1]) || str[start-1] == '_' || str[start-1] == '.')) start--;
	std::string dotted = str.substr(start, end - start);
	// a name, or names separated by single dots, starting with a letter or an underscore
	// and not the attribute of an expression (e.g. f().x or a[0].x), which would have to be evaluated
	if (dotted.empty() || dotted.front() == '.' || dotted.back() == '.' || isdigit((unsigned char)dotted.front())) return "";
	if (dotted.find("..") != std::string::npos) return "";
	return dotted;
}

//--------------------------------------------------------------
// the brackets and strings of the arguments are followed, so that the commas and brackets in them don't count
bool Editor::argHintValid(const ArgHint& hint)
{
	if (hint.line != cursorLineIndex) return false;
	auto lineIt = allLines.find(cursorLineIndex);
	if (lineIt == allLines.end()) return false;
	const std::string& str = lineIt->second.str;
	int pos = cursorPos + lineIt->second.startPos;
	if (hint.openPos >= pos || hint.openPos >= (int)str.size() || str[hint.openPos] != '(') return false;
	int depth = 0;
	char quote = 0;
	for (int i = hint.openPos + 1; i < pos && i < (int)str.size(); i++) {
		char c = str[i];
		if (quote) {
			if (c == '\\') i++;
			else if (c == quote) quote = 0;
		}
		else if (c == '\'' || c == '"') quote = c;
		else if (c == '(' || c == '[' || c == '{') depth++;
		else if (c == ')' || c == ']' || c == '}') {
			// the brackets of the hint are closed before the cursor
			if (--depth < 0) return false;
		}
	}
	return true;
}

//--------------------------------------------------------------
void Editor::updateArgHints(int key)
{
	if (thisLang != 1 || !inserting || typingShell) {
		argHints.clear();
		return;
	}
	auto lineIt = allLines.find(cursorLineIndex);
	if (lineIt == allLines.end()) return;
	const std::string& str = lineIt->second.str;
	int pos = cursorPos + lineIt->second.startPos;
	// ( typed right after a name, e.g. Sine( or np.linspace(, but not in a comment
	if (key == '(' && pos > 0 && pos <= (int)str.size() && str[pos-1] == '(' && pos - 1 < commentStartOf(str)) {
		std::string name = pythonDottedName(str, pos - 1);
		if (!name.empty()) {
			std::vector<std::string> params = ((ofApp*)ofGetAppPtr())->pythonSignature(name);
			if (!params.empty()) argHints.push_back({cursorLineIndex, pos - 1, params});
		}
	}
	// the hints whose brackets have been closed, or the cursor has left, are removed
	argHintText();
}

//--------------------------------------------------------------
std::string Editor::argHintText(bool* typingKeyword)
{
	if (typingKeyword) *typingKeyword = false;
	// the hint is part of autocompletion, so \editor.autocomplete off turns it off too
	if (!autocompletion || thisLang != 1 || !inserting || typingShell || highlightManyChars) {
		argHints.clear();
		return "";
	}
	while (!argHints.empty() && !argHintValid(argHints.back())) argHints.pop_back();
	if (argHints.empty()) return "";
	const ArgHint& hint = argHints.back();
	const std::vector<std::string>& params = hint.params;
	const std::string& str = allLines[cursorLineIndex].str;
	int pos = cursorPos + allLines[cursorLineIndex].startPos;
	// the arguments typed so far, separated by the commas that are not in brackets or strings
	std::vector<std::string> args(1);
	int depth = 0;
	char quote = 0;
	for (int i = hint.openPos + 1; i < pos && i < (int)str.size(); i++) {
		char c = str[i];
		if (quote) {
			if (c == '\\' && i + 1 < pos) {
				args.back() += c;
				c = str[++i];
			}
			else if (c == quote) quote = 0;
		}
		else if (c == '\'' || c == '"') quote = c;
		else if (c == '(' || c == '[' || c == '{') depth++;
		else if (c == ')' || c == ']' || c == '}') depth--;
		else if (c == ',' && depth == 0) {
			args.emplace_back();
			continue;
		}
		args.back() += c;
	}
	auto trimmed = [](const std::string& s) {
		size_t first = s.find_first_not_of(" \t");
		if (first == std::string::npos) return std::string();
		return s.substr(first, s.find_last_not_of(" \t") - first + 1);
	};
	auto paramName = [](const std::string& param) { return param.substr(0, param.find('=')); };
	// the parameters the typed arguments have taken
	// a keyword argument (name=value, but not a comparison like a==b) takes the parameter of its name
	// and a positional argument takes the first parameter that is left
	std::vector<bool> taken(params.size(), false);
	auto take = [&](const std::string& arg) {
		size_t eq = arg.find('=');
		if (eq != std::string::npos && eq > 0 && (eq + 1 >= arg.size() || arg[eq+1] != '=')) {
			std::string name = trimmed(arg.substr(0, eq));
			for (size_t p = 0; p < params.size(); p++) {
				if (!taken[p] && paramName(params[p]) == name) {
					taken[p] = true;
					return;
				}
			}
			return;
		}
		for (size_t p = 0; p < params.size(); p++) {
			if (!taken[p]) {
				taken[p] = true;
				return;
			}
		}
	};
	for (size_t a = 0; a + 1 < args.size(); a++) take(trimmed(args[a]));
	// the argument at the cursor replaces the parameter it is the start of (e.g. fr of freq=1000), which shows the rest of it
	// or, if it is not the start of any parameter (e.g. a value, 440), it takes a parameter as the others, and the hint shows the rest of the parameters
	const std::string& raw = args.back();
	std::string current = raw.substr(std::min(raw.find_first_not_of(" \t"), raw.size()));
	int match = -1;
	for (size_t p = 0; p < params.size(); p++) {
		if (!taken[p] && params[p].compare(0, current.size(), current) == 0) {
			match = (int)p;
			break;
		}
	}
	std::string text;
	if (match >= 0) {
		text = params[match].substr(current.size());
		taken[match] = true;
		if (typingKeyword && !current.empty()) *typingKeyword = true;
		// right after a comma, with a space before the parameter
		if (raw.empty() && args.size() > 1) text = " " + text;
	}
	else if (!current.empty()) {
		take(current);
	}
	for (size_t p = 0; p < params.size(); p++) {
		if (!taken[p]) text += ", " + params[p];
	}
	return text;
}

//--------------------------------------------------------------
// Tab selects the next suggestion, Shift+Tab the previous one, and Return inserts the selected suggestion
// without pressing Tab no suggestion is selected and Return makes a new line
bool Editor::completionKey(int key)
{
	if (ctrlPressed || altPressed) return false;
	if (completions.empty()) {
		// without suggestions, Tab inserts what is left of the hint of arguments (see argHintText())
		if (key == 9 && !shiftPressed) {
			std::string rest = argHintText();
			if (rest.empty()) return false;
			beforeChange();
			// the characters of the hint are inserted as they are, without closing brackets or quotes automatically
			bool autobracketsLocal = autobrackets;
			autobrackets = false;
			for (char c : rest) assembleString((int)c, false, true);
			autobrackets = autobracketsLocal;
			afterChange();
			return true;
		}
		return false;
	}
	if (key == 9) {
		int numCompletions = (int)completions.size();
		if (shiftPressed) completionNdx = (completionNdx <= 0 ? numCompletions - 1 : completionNdx - 1);
		else completionNdx = (completionNdx + 1) % numCompletions;
		return true;
	}
	if (key == 13 && !shiftPressed && completionNdx >= 0) {
		std::string rest = completions[completionNdx].substr(completionPrefixLen);
		clearCompletions();
		beforeChange();
		for (char c : rest) assembleString((int)c, false, true);
		afterChange();
		return true;
	}
	return false;
}

//--------------------------------------------------------------
// the pop-up appears below the line of the cursor, or above it if there is not enough space below
// and it is moved so that it stays inside the window
void Editor::drawCompletions()
{
	if (completions.empty()) return;
	// the suggestions are for the cursor position where they were made, so moving the cursor (e.g. with the arrow keys) hides them
	if (!activity || !inserting || typingShell || completionLine != cursorLineIndex ||
			completionPos != cursorPos + allLines[cursorLineIndex].startPos) {
		clearCompletions();
		return;
	}
	ofApp* app = (ofApp*)ofGetAppPtr();
	float brightness = app->brightnessCoeff;
	float windowWidth = ofGetWidth();
	float windowHeight = ofGetHeight();
	const int maxRows = 10;
	int numRows = std::min((int)completions.size(), std::max(1, std::min(maxRows, (int)(windowHeight / cursorHeight))));
	// with more suggestions than rows, the rows scroll with the selected suggestion
	int firstRow = (completionNdx >= numRows ? completionNdx - numRows + 1 : 0);
	size_t longest = 0;
	for (const std::string& completion : completions) longest = std::max(longest, completion.size());
	float padding = oneCharacterWidth / 2.0;
	float width = (longest * oneCharacterWidth) + (2 * padding);
	float height = numRows * cursorHeight;
	// the suggestions are aligned with the start of the typed word
	float x = cursorScreenX - (completionPrefixLen * oneCharacterWidth) - padding;
	float y = cursorScreenY + cursorHeight;
	if (y + height > windowHeight) y = cursorScreenY - height;
	if (y + height > windowHeight) y = windowHeight - height;
	if (y < 0) y = 0;
	if (x + width > windowWidth) x = windowWidth - width;
	if (x < 0) x = 0;
	ofFill();
	ofSetColor(ofColor(40) * brightness);
	ofDrawRectangle(x, y, width, height);
	for (int row = 0; row < numRows; row++) {
		int ndx = firstRow + row;
		float rowY = y + (row * cursorHeight);
		if (ndx == completionNdx) {
			ofSetColor(ofColor::cyan * brightness);
			ofDrawRectangle(x, rowY, width, cursorHeight);
			ofSetColor(ofColor::black);
		}
		else {
			ofSetColor(app->foregroundColor * brightness);
		}
		font.drawString(completions[ndx], x + padding, rowY + cursorHeight - characterOffset);
	}
	ofNoFill();
	ofSetColor(app->foregroundColor * brightness);
	ofDrawRectangle(x, y, width, height);
	ofFill();
}
