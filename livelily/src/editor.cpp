#include <iostream>
#include <fstream>
#include <iterator> // to be able to use find() with a reverse iterator, see findBraceBackward()
#include <algorithm>
#include <limits>
#include <utility> // to add pair and make_pair
#include "editor.h"
//#include "readmidi.h"
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
	activeSession = true;

	executionStepPerMs = (float)EXECUTIONBRIGHTNESS / (float)(EXECUTIONDUR - EXECUTIONRAMPSTART);

	executingMultiple = false;
	highlightManyChars = false;
	highlightManyCharsStart = 0;

	ctrlPressed = false;
	shiftPressed = false;

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

	allStrings[0] = "";
	allStringStartPos[0] = 0;
	tracebackStr[0] = "";
	tracebackColor[0] = 0;
	tracebackNumLines[0] = 1;
	tracebackStrBreakPnt[0] = false;
	tabStr = "";
	for (int i = 0; i < TABSIZE; i++) {
		tabStr += " ";
	}

	couldNotLoadFile = false;
	couldNotSaveFile = false;
	fileEdited = false;
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
	std::map<int, std::string>::iterator it = allStrings.find(lineNdx);
	if (it == allStrings.end()) return "";
	else return it->second;
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
std::vector<int> Editor::getNestDepth()
{
	std::map<int, std::string>::iterator it;
	std::vector<int> v(2, 0); // bottom line, nest depth
	bool insideNest = false;
	for (it = allStrings.find(cursorLineIndex); it != allStrings.end(); ++it) {
		if (((thisLang == 0 && startsWith(it->second, "%")) || (thisLang == 1 && startsWith(it->second, "#")) || (thisLang == 2 && startsWith(it->second, "--"))) && !insideNest) break;
		if (it->second.substr(0, TABSIZE).compare(tabStr) == 0) {
			v[0] = it->first;
			v[1] = std::max(v[1], getNumTabsInStr(it->second));
			insideNest = true;
		}
		else {
			if (it->first == cursorLineIndex && ((thisLang == 0 && it->second.find("{") != std::string::npos) || (thisLang == 1 && it->second.find(":") != std::string::npos))) continue;
			else if (it->second.find("}") != std::string::npos) {
				v[0] = it->first;
				if (it->first == cursorLineIndex) {
					--it;
					while (it->second.substr(0, TABSIZE).compare(tabStr) == 0) {
						v[1] = std::max(v[1], getNumTabsInStr(it->second));
						--it;
					}
				}
				return v;
			}
			else return v;
		}
	}
	return v;
}

//--------------------------------------------------------------
int Editor::findTopLine(int startLine)
{
	std::map<int, std::string>::iterator it;
	for (it = allStrings.find(startLine); it != allStrings.begin(); --it) {
		if (it->second.substr(0, TABSIZE).compare(tabStr) != 0) {
			if (it->second.find("}") != std::string::npos) continue;
			else return it->first;
		}
	}
	return 0;
}

//--------------------------------------------------------------
std::vector<int> Editor::findBraceForward(std::map<int, std::string>::iterator start, std::map<int, std::string>::iterator finish)
{
	std::vector<int> v(2, -1);
	if (start == allStrings.end() || finish == allStrings.end()) {
		return v;
	}
	unsigned i, lineStart;
	std::map<int, std::string>::iterator it;
	// Declare a stack to hold the previous brackets.
	std::stack<char> temp;
	for (it = start; it != finish; ++it) {
		lineStart = cursorPos;
		if (it->first > cursorLineIndex) lineStart = 0;
		for (i = lineStart; i < it->second.length(); i++) {
			if (it->second[i] == '{' || it->second[i] == '[' || it->second[i] == '(') {
				temp.push(it->second[i]);
			}
			else if (it->second[i] == '}' || it->second[i] == ']' || it->second[i] == ')') {
				if (temp.empty()) return v;
				if ((temp.top() == '{' && it->second[i] == '}')
					|| (temp.top() == '[' && it->second[i] == ']')
					|| (temp.top() == '(' && it->second[i] == ')')) {
					temp.pop();
					if (temp.empty()) {
						v[0] = i;
						v[1] = it->first;
						return v;
					}
				}
			}
		}
	}
	return v;
}

//--------------------------------------------------------------
std::vector<int> Editor::findBraceBackward()
{
	std::vector<int> v(2, -1);
	if (allStrings.find(cursorLineIndex) == allStrings.end()) {
		return v;
	}
	unsigned i, start;
	std::map<int, std::string>::reverse_iterator it = allStrings.rbegin();
	size_t step = allStrings.size() - (size_t)(cursorLineIndex + 1);
	if (step > 0) std::advance(it, step);
	// Declare a stack to hold the previous brackets.
	std::stack<char> temp;
	while (it != allStrings.rend()) {
		start = (unsigned)std::min((int)it->second.length()-1, cursorPos);
		if (it->first < cursorLineIndex) start = it->second.length() - 1;
		for (i = start; i >= 0; i--) {
			if (it->second[i] == '}' || it->second[i] == ']' || it->second[i] == ')') {
				temp.push(it->second[i]);
			}
			else if (it->second[i] == '{' || it->second[i] == '[' || it->second[i] == '(') {
				if (temp.empty()) return v;
				if ((it->second[i] == '{' && temp.top() == '}')
					|| (it->second[i] == '[' && temp.top() == ']')
					|| (it->second[i] == '(' && temp.top() == ')')) {
					temp.pop();
					if (temp.empty()) {
						v[0] = i;
						v[1] = it->first;
						return v;
					}
				}
			}
		 }
		++it;
	}
	return v;
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
	static int prevCursorPos = 0;
	static char stringChar = '"'; // to be able to identify strings in Python with either single of double quotes
	/*******************************************************************/
	// get the offset of the line count
	int loopIter;
	if (lineCount < maxNumLines) loopIter = lineCount;
	else loopIter = maxNumLines + lineCountOffset; // + 1;
	// and the width of the rectangle for highlighting a line
	int rectWidth = frameWidth - (lineNumberWidth + oneAndHalfCharacterWidth);

	/*******************************************************************/
	// get the number of digits of the line count
	int numDigits = getNumDigitsOfLineCount();
	lineNumberWidth = numDigits * oneCharacterWidth;

	/*******************************************************************/
	// then draw the red/orange traceback rectangles
	int count = 0; // separate counter to start from 0 because i starts from lineCountOffset
	for (int i = lineCountOffset; i < loopIter; i++) {
		if (tracebackStr[i].size() > 0 && tracebackColor[i] > 0) {
			if (tracebackColor[i] == 1) {
				ofSetColor(ofColor::orange*((ofApp*)ofGetAppPtr())->brightnessCoeff);
			}
			else if (tracebackColor[i] == 2) {
				ofSetColor(ofColor::red*((ofApp*)ofGetAppPtr())->brightnessCoeff);
			}
			int xOffset = lineNumberWidth + oneAndHalfCharacterWidth + frameXOffset;
			int yOffset = count * cursorHeight + frameYOffset;
			ofDrawRectangle(xOffset, yOffset, rectWidth, cursorHeight);
		}
		count++;
	}

	/*******************************************************************/
	// then check if we're executing any lines and draw the execution rectangle
	// needs to be first otherwise the alpha blending won't work as expected
	for (int i = lineCountOffset; i < loopIter; i++) {
		if (executingLines[i]) {
			ofSetColor(ofColor::cyan.r*((ofApp*)ofGetAppPtr())->brightnessCoeff,
					ofColor::cyan.g*((ofApp*)ofGetAppPtr())->brightnessCoeff,
					ofColor::cyan.b*((ofApp*)ofGetAppPtr())->brightnessCoeff,
					executionDegrade[i]);
			int xOffset = lineNumberWidth + oneAndHalfCharacterWidth + frameXOffset;
			int yOffset = (i-lineCountOffset) * cursorHeight;
			yOffset += frameYOffset;
			// rectWidth has been calculated above, before the loop that draw traceback rectangles
			ofDrawRectangle(xOffset, yOffset, rectWidth, cursorHeight);
			if ((ofGetElapsedTimeMillis() - executionTimeStamp[i]) >= EXECUTIONRAMPSTART) {
				// calculate how many steps the brightness has to dim, depending on the elapsed time
				// and the step per millisecond
				int brightnessDegrade = (int)((ofGetElapsedTimeMillis() - (executionTimeStamp[i]+EXECUTIONRAMPSTART)) * executionStepPerMs);
				executionDegrade[i] = EXECUTIONBRIGHTNESS - brightnessDegrade;
				if (executionDegrade[i] < 0) {
					executionDegrade[i] = 0;
					executingLines[i] = false;
				}
			}
		}
	}
	// check for any execution rectangle that is not visible that might be left hanging
	// in case of executing bars quickly, and zero their booleans and ramp counters
	for (size_t i = 0; i < allStrings.size(); i++) {
		if (executingLines[i] && ((int)i < lineCountOffset || (int)i >= lineCountOffset + loopIter)) {
			executionDegrade[i] = 0;
			executingLines[i] = false;
		}
	}

	/******************************************************************/
	// then draw the rectangle that highlights a bracket
	highlightBracket = false;
	if (activity) {
		if (cursorPos < (int)allStrings[cursorLineIndex].size() && cursorPos <= maxCursorPos()) {
			// if matching brace is found, this vector will be the X and Y coordinates
			// and nest depth (only from findBraceForward() but findBraceBackward() returns a 3 element vector
			// for code integrity), which is used elsewhere
			std::vector<int> pos (3, -1);
			char bracketChars[7] = {'{', '}', '[', ']', '(', ')'}; // extra array item for null char
			//int direction = 0; // initialized to 0 to avoid warning messages during compilation
			char charOnTopOfCursor = allStrings[cursorLineIndex][cursorPos+allStringStartPos[cursorLineIndex]];
			for (int i = 0; i < 6; i++) {
				if (bracketChars[i] == charOnTopOfCursor) {
					//direction = i % 2;
					// make highlightBracket true only if autocompletion of brackets if on
					if (autobrackets) highlightBracket = true;
					break;
				}
			}
			//if (highlightBracket) {
			//	if (direction) pos = findBraceBackward();
			//	else pos = findBraceForward(allStrings.find(cursorLineIndex), allStrings.end(), true);
			//}
			// if the matching pair has been found, we'll get positions that are greater than -1 for both X and Y
			if (pos[0] > -1){
				ofSetColor(ofColor::magenta*((ofApp*)ofGetAppPtr())->brightnessCoeff);
				int strWidth = font.stringWidth(allStrings[pos[1]].substr(0, pos[0] - allStringStartPos[pos[1]]));
				bracketHighlightRectX = lineNumberWidth + strWidth + oneAndHalfCharacterWidth + frameXOffset;
				bracketHighlightRectY = ((pos[1]-lineCountOffset) * cursorHeight) + frameYOffset;
				ofDrawRectangle(bracketHighlightRectX, bracketHighlightRectY, oneCharacterWidth, cursorHeight);
				ofSetColor(255*((ofApp*)ofGetAppPtr())->brightnessCoeff);
			}
		}
	}

	/*******************************************************************/
	// then the actual text
	std::string strOnCursorLine;
	int strOnCursorLineYOffset = frameYOffset;
	int strXOffset = lineNumberWidth + oneAndHalfCharacterWidth + frameXOffset;
	// the extra counter below starts from 0, whereas i starts from lineCountOffset
	count = 0;
	// we'll get the color of the text where the cursor is
	// for this reason, we need to calculate the X position of the cursor here, and not below
	// where we draw the cursor
	ofColor cursorColor = ((ofApp*)ofGetAppPtr())->brightnessCoeff;
	int cursorX = lineNumberWidth + font.stringWidth(allStrings[cursorLineIndex].substr(0, cursorPos)) + \
				  oneAndHalfCharacterWidth + frameXOffset;
	bool cursorDrawn = false;
	for (int i = lineCountOffset; i < loopIter; i++) {
		int strYOffset = ((count+1)*cursorHeight) + frameYOffset - characterOffset;
		if (i == cursorLineIndex) {
			strOnCursorLine = allStrings[i];
			strOnCursorLineYOffset = strYOffset;
		}
		// draw all strings that are not empty
		if (allStrings[i].size() > 0) {
			// iterate through each word to check for keywords
			// set last argument to tokenizeString to true to include the delimiters
			std::vector<std::string> tokens = tokenizeString(allStrings[i], delimiters[thisLang], true);
			int xOffset = strXOffset;
			if (cursorLineIndex == i) xOffset -= (allStringStartPos[i] * oneCharacterWidth);
			bool isComment = false;
			bool isString = false;
			bool isCursorInsideKeyword = false;
			int charAccum = 0;
			int quotesCounter = 0;
			size_t subtokensSizeAccum = 0;
			if (tokens.size() == 0) {
				isComment = false;
			}
			bool tokenizeDigits1 = false;
			if (thisLang == 0) {
				// the main ofApp stores the following LiveLily commands: \insts, \bar, \bars, \loop, \function, \list and \group
				// we use this to determine if we should further tokenize each token of our string
				// if a line starts with one of these commands, then we won't tokenize the token further
				tokenizeDigits1 = find(((ofApp*)ofGetAppPtr())->commandsToNotTokenize.begin(), ((ofApp*)ofGetAppPtr())->commandsToNotTokenize.end(), tokens[0]) == ((ofApp*)ofGetAppPtr())->commandsToNotTokenize.end();
			}
			int activeElementCounter = 0;
			int activeElementXPos = 0;
			int chordCounter = 0;
			bool isChord = false;
			std::string strForAnimationRect;
			for (auto it = tokens.begin(); it != tokens.end(); ++it) {
				strForAnimationRect += *it;
				int firstCharAscii = int((*it)[0]);
				bool isNoteElement = false;
				if (isChord) chordCounter++;
				// check for characters from 'a' to 'g'
				if (firstCharAscii >= 97 && firstCharAscii <= 103 && !isChord) {
					if ((firstCharAscii == 98 && *it != "bass" && *it != "alto") || firstCharAscii != 98) {
						activeElementCounter++;
						isNoteElement = true;
					}
				}
				// we first check for > to close a chord because we test against isChord
				// which is set to true in the else if below
				else if (isChord) {
					size_t chordClosePos = (*it).find(">");
					if (chordClosePos != std::string::npos && chordClosePos > 0) {
						// > is also used for dynamics and articulations so we have to make sure it's used to close a chord
						if ((*it)[chordClosePos-1] != '\\' && (*it)[chordClosePos-1] != '-') {
							isChord = false;
							isNoteElement = true;
						}
					}
				}
				// if the current token is not a note, check if it is a chord by testing against ASCII 60 which is <
				else if (firstCharAscii == 60) {
					isChord = true;
					activeElementCounter++;
				}
				// tokenizing has already been done, but to separate digits properly
				// we need to tokenize these further, as a string like "c''4" won't be tokenized properly
				std::vector<std::string> subtokens;
				bool tokenizeDigits2 = false;
				if (thisLang == 0) {
					// the main ofApp also stores all the bar, loop, function and list names to the keywords vector
					tokenizeDigits2 = find(((ofApp*)ofGetAppPtr())->keywords.begin(), ((ofApp*)ofGetAppPtr())->keywords.end(), *it) == ((ofApp*)ofGetAppPtr())->keywords.end();
				}
				// if the token starts with a backslash, then we don't tokenize based on digits
				if (thisLang == 0 && startsWith(*it, "\\")) tokenizeDigits2 = false;
				if (tokenizeDigits1 && tokenizeDigits2) {
					subtokens = tokenizeString(*it, "0123456789", true);
				}
				else {
					subtokens.push_back(*it);
				}
				int subtokCounter = 0;
				for (auto subit = subtokens.begin(); subit != subtokens.end(); ++subit) {
					//std::cout << "cursorX: " << cursorX << ", cursorPos: " << cursorPos << ", xOffset: " << xOffset << ", str width of \"" << subtoken << "\": " << font.stringWidth(subtoken) << ", whole str \"" << allStrings[i] << "\": " << font.stringWidth(allStrings[i]) << endl;
					//if (i == cursorLineIndex && cursorX >= xOffset && cursorX <= xOffset + font.stringWidth(subtoken)) {
					//	if ((cursorX == xOffset + font.stringWidth(subtoken) && cursorX == xOffset + (int)allStrings[i].size()-1) || cursorX < xOffset + font.stringWidth(subtoken)) {
					//		isCursorInsideKeyword = true;
					//	}
					//}
					if (i == cursorLineIndex) {
						size_t tokenNdx = std::distance(tokens.begin(), it);
						size_t subtokenNdx = std::distance(subtokens.begin(), subit);
						bool endOfStr = (tokenNdx == tokens.size()-1 && subtokenNdx == subtokens.size()-1 ? true : false);
						//if (prevCursorPos != cursorPos)
						//	std::cout << "\"" << *subit << "\": cursorPos: " << cursorPos << ", subtokensSizeAccum: " << subtokensSizeAccum << ", subtoken size: " << (*subit).size() << endl;
						if (cursorPos - (int)subtokensSizeAccum + allStringStartPos[i] < (int)(*subit).size() || (cursorPos + allStringStartPos[i] == (int)allStrings[i].size() && endOfStr)) {
							isCursorInsideKeyword = true;
							//if (prevCursorPos != cursorPos)
							//	std::cout << "drawing cursor for \"" << *subit << "\"\n";
						}
					}
					subtokensSizeAccum += (*subit).size();
					if (tracebackStr[i].size() > 0 && tracebackColor[i] > 0) {
						ofSetColor(((ofApp*)ofGetAppPtr())->backgroundColor*((ofApp*)ofGetAppPtr())->brightnessCoeff);
					}
					else if ((thisLang == 0 && startsWith(*subit, "%")) || (thisLang == 1 && startsWith(*subit, "#"))) {
						ofSetColor(ofColor::gray*((ofApp*)ofGetAppPtr())->brightnessCoeff);
						isComment = true;
						if (isCursorInsideKeyword && !cursorDrawn) {
							cursorColor = ofColor::gray;
							drawCursor(cursorX, cursorColor);
							cursorDrawn = true;
						}
					}
					else if (*subit == "\"" || (thisLang == 1 && *subit == "'")) {
						ofSetColor(ofColor::aqua*((ofApp*)ofGetAppPtr())->brightnessCoeff);
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
							cursorColor = ofColor::aqua;
							drawCursor(cursorX, cursorColor);
							cursorDrawn = true;
						}
					}
					else {
						if ((*subit).size() > 0 && (isalpha((*subit)[0]) || (thisLang == 0 && startsWith(*subit, "\\"))) && !isComment && !isString) {
							int firstCharNdx = 0;
							ofColor color;
							if (startsWith(*subit, "\\") && (*subit).size() > 1) firstCharNdx = 1;
							if (((ofApp*)ofGetAppPtr())->commandsMap[thisLang][(*subit)[firstCharNdx]].find(*subit) != ((ofApp*)ofGetAppPtr())->commandsMap[thisLang][(*subit)[firstCharNdx]].end()) {
								color = ((ofApp*)ofGetAppPtr())->commandsMap[thisLang][(*subit)[firstCharNdx]][*subit];
								if (isCursorInsideKeyword && !cursorDrawn) {
									cursorColor = color;
									drawCursor(cursorX, cursorColor);
									cursorDrawn = true;
								}
							}
							else {
								color = ((ofApp*)ofGetAppPtr())->foregroundColor;
							}
							ofSetColor(color*((ofApp*)ofGetAppPtr())->brightnessCoeff);
						}
						//if (((ofApp*)ofGetAppPtr())->commandsMap[thisLang].find(*subit) != ((ofApp*)ofGetAppPtr())->commandsMap[thisLang].end()
						//		&& !isComment && !isString) {
						//	ofColor color = ((ofApp*)ofGetAppPtr())->commandsMap[thisLang][*subit];
						//	ofSetColor(color*((ofApp*)ofGetAppPtr())->brightnessCoeff);
						//	if (isCursorInsideKeyword && !cursorDrawn) {
						//		cursorColor = color;
						//		drawCursor(cursorX, cursorColor);
						//		cursorDrawn = true;
						//	}
						//}
						else if ((*subit).size() == 0 && !isComment && !isString) {
							ofSetColor(((ofApp*)ofGetAppPtr())->foregroundColor*((ofApp*)ofGetAppPtr())->brightnessCoeff);
						}
						else if (isNumber(*subit) && !isComment && !isString) {
							ofSetColor(ofColor::gold*((ofApp*)ofGetAppPtr())->brightnessCoeff);
							if (isCursorInsideKeyword && !cursorDrawn) {
								cursorColor = ofColor::gold;
								drawCursor(cursorX, cursorColor);
								cursorDrawn = true;
							}
						}
						else if (!isComment && !isString) {
							ofSetColor(((ofApp*)ofGetAppPtr())->foregroundColor*((ofApp*)ofGetAppPtr())->brightnessCoeff);
						}
						else if (isComment && (isCursorInsideKeyword && !cursorDrawn)) {
							cursorColor = ofColor::gray;
							drawCursor(cursorX, cursorColor);
							cursorDrawn = true;
						}
						else if (isString && (isCursorInsideKeyword && !cursorDrawn)) {
							cursorColor = ofColor::aqua;
							drawCursor(cursorX, cursorColor);
							cursorDrawn = true;
						}
					}
					size_t numCharsToRemove = 0;
					size_t firstCharToDisplay = 0;
					int xOffsetModified = xOffset;
					int startPos = 0;
					if (i == cursorLineIndex) startPos = allStringStartPos[i];
					if (charAccum < startPos) {
						firstCharToDisplay = (size_t)(startPos - charAccum);
						xOffsetModified += (firstCharToDisplay * oneCharacterWidth);
					}
					if ((charAccum + (int)(*subit).size() - startPos) > maxCharactersPerString) {
						numCharsToRemove = ((size_t)charAccum + (*subit).size()) - (size_t)startPos - (size_t)maxCharactersPerString - 1;
					}
					if (firstCharToDisplay < (*subit).size() && numCharsToRemove < (*subit).size()) {
						font.drawString((*subit).substr(firstCharToDisplay, (*subit).size()-numCharsToRemove), xOffsetModified, strYOffset);
					}
					if (subtokCounter == 0 && chordCounter == 0) {
						activeElementXPos = xOffsetModified;
					}
					xOffset += ((*subit).size() * oneCharacterWidth);
					charAccum += (int)(*subit).size();
					subtokCounter++;
				}
				if (thisLang == 0 && animationState && linesConnectedToBar[i] > -1 && isNoteElement && activeLineElements[i] > -1 && activeLineElements[i] == activeElementCounter - 1) {
					float strWidth = font.stringWidth(strForAnimationRect);
					int yPos = (i * cursorHeight) + frameYOffset;
					ofSetColor(((ofApp*)ofGetAppPtr())->foregroundColor*((ofApp*)ofGetAppPtr())->brightnessCoeff);
					ofNoFill();
					ofDrawRectangle(activeElementXPos, yPos, strWidth, cursorHeight);
					ofFill();
				}
				if (!isChord) {
					chordCounter = 0;
					strForAnimationRect.clear();
				}
			}
		}

		// then check if we have selected this string so we must highlight it
		if (highlightManyChars && ((i>=topLine) && (i<=bottomLine))) {
			std::vector<int> posAndSize = setSelectedStrStartPosAndSize(i);
			int boxXOffset = lineNumberWidth + oneAndHalfCharacterWidth + \
							 (posAndSize[0]*oneCharacterWidth) + frameXOffset;
			int boxYOffset = (count * cursorHeight) + frameYOffset;
			// the bounding box coordinates of the highlighted string don't need the offset
			// which is greater than 0 if the string is too long
			// but the substring that is drawn on top of this box does need it
			posAndSize[0] += allStringStartPos[i];
			std::string strInBlack = allStrings[i].substr(posAndSize[0], posAndSize[1]);
			int widthLocal = font.stringWidth(strInBlack);
			// draw the selecting rectangle
			ofSetColor(ofColor::goldenRod*((ofApp*)ofGetAppPtr())->brightnessCoeff);
			ofDrawRectangle(boxXOffset, boxYOffset, widthLocal, cursorHeight);
			ofSetColor(0);
			// and then the selected string in black
			font.drawString(strInBlack, boxXOffset, strYOffset);
		}
		count++;
	}

	/*******************************************************************/
	// then draw the cursor
	if (!cursorDrawn) drawCursor(cursorX, ((ofApp*)ofGetAppPtr())->foregroundColor);

	/*******************************************************************/
	// then draw the character the cursor is drawn on top of (if this is the case)
	if (activity && !inserting && !typingShell) {
		if (cursorPos < (int)strOnCursorLine.size() && cursorPos <= maxCursorPos()) {
			std::string onTopOfCursorStr = strOnCursorLine.substr(cursorPos+allStringStartPos[cursorLineIndex], 1);
			ofSetColor(0);
			font.drawString(onTopOfCursorStr, cursorX, strOnCursorLineYOffset);
		}
	}

	/*******************************************************************/
	// then draw the line numbers
	count = 0;
	ofSetColor(((ofApp*)ofGetAppPtr())->foregroundColor*((ofApp*)ofGetAppPtr())->brightnessCoeff);
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
		font.drawString(std::to_string(i+1), xOffset, ((count+1)*cursorHeight)+frameYOffset-characterOffset);
		count++;
	}
	if (prevCursorPos != cursorPos) prevCursorPos = cursorPos;
}

//--------------------------------------------------------------
void Editor::drawCursor(int cursorX, ofColor cursorColor)
{
	if (!activity) {
		ofNoFill();
	}
	// we have calculated the X position of the cursor before we drew the text
	int cursorY = ((cursorLineIndex-lineCountOffset) * cursorHeight) + frameYOffset; //+ characterOffset
	ofSetColor(cursorColor*((ofApp*)ofGetAppPtr())->brightnessCoeff);
	if ((activity && !inserting && !typingShell) || !activity) {
		ofDrawRectangle(cursorX, cursorY, oneCharacterWidth, cursorHeight);
	}
	else if (activity && inserting && !typingShell) {
		ofDrawLine(cursorX, cursorY, cursorX, cursorY+cursorHeight);
	}
	if (!activity) {
		ofFill();
	}
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
		font.drawString(loadedFileStr, fileNameXOffset, strYCoord);
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
		else s = "VISUAL";
		font.drawString(s, frameXOffset+frameWidth-widthOffset-font.stringWidth(s)-10, strYCoord);
	}
}

//--------------------------------------------------------------
void Editor::setStringsStartPos()
{
	for (unsigned i = 0; i < allStrings.size(); i++) {
		if (cursorLineIndex == (int)i) {
			allStringStartPos[i] = (int)allStrings[i].size() - maxCharactersPerString;
			if (allStringStartPos[i] < 0) allStringStartPos[i] = 0;
		}
		else {
			allStringStartPos[i] = 0;
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
		if (frameWidth > ((ofApp*)ofGetAppPtr())->sharedData.middleOfScreenX) {
			width = ((ofApp*)ofGetAppPtr())->sharedData.middleOfScreenX;
		}
	}
	maxCharactersPerString = (width-lineNumberWidth-oneAndHalfCharacterWidth) / oneCharacterWidth;
	maxCharactersPerString -= 1;
	//setStringsStartPos();
}

//--------------------------------------------------------------
void Editor::resetCursorPos(int oldMaxCharactersPerString)
{
	if ((int)allStrings[cursorLineIndex].size() > 0 && \
			(int)allStrings[cursorLineIndex].size() > maxCharactersPerString) {
		if (cursorPos == oldMaxCharactersPerString) {
			cursorPos = maxCharactersPerString;
			highlightedCharIndex = (int)allStrings[cursorLineIndex].size();
			allStringStartPos[cursorLineIndex] = (int)allStrings[cursorLineIndex].substr(0, cursorPos).size() - maxCharactersPerString;
		}
		else if (cursorPos == 0) {
			cursorPos = 0;
			highlightedCharIndex = 0;
			allStringStartPos[cursorLineIndex] = 0;
		}
		else {
			highlightedCharIndex = cursorPos + allStringStartPos[cursorLineIndex];
			// three factor method
			cursorPos = round((float)(cursorPos * maxCharactersPerString) / (float)oldMaxCharactersPerString);
			allStringStartPos[cursorLineIndex] = highlightedCharIndex-cursorPos;
			if (allStringStartPos[cursorLineIndex] < 0) allStringStartPos[cursorLineIndex] = 0;
		}
		stringExceededWindow = true;
	}
	else {
		// if we're going from big to small
		if (stringExceededWindow) {
			cursorPos = highlightedCharIndex;
			allStringStartPos[cursorLineIndex] = 0;
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
	lineCountOffset = cursorLineIndex - maxNumLines + 1;
	if (lineCountOffset < 0) lineCountOffset = 0;
	else if ((lineCountOffset + maxNumLines) > lineCount) {
		lineCountOffset = lineCount = maxNumLines;
	}
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
	allStrings[cursorLineIndex+increment] = str;
	allStringStartPos[cursorLineIndex+increment] = 0;
	tracebackStr[cursorLineIndex+increment] = "";
	tracebackColor[cursorLineIndex+increment] = 0;
	tracebackNumLines[cursorLineIndex+increment] = 1;
	tracebackTimeStamps[cursorLineIndex+increment] = 0;
	tracebackStrBreakPnt[cursorLineIndex+increment] = 0;
	executingLines[cursorLineIndex+increment] = false;
	executionDegrade[cursorLineIndex+increment] = 0;
	executionTimeStamp[cursorLineIndex+increment] = 0;
	linesConnectedToBar[cursorLineIndex+increment] = -1;
	activeLineElements[cursorLineIndex+increment] = -1;
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
	allStrings[cursorLineIndex+1] = allStrings[cursorLineIndex];
	allStringStartPos[cursorLineIndex+1] = allStringStartPos[cursorLineIndex];
	tracebackStr[cursorLineIndex+1] = tracebackStr[cursorLineIndex];
	tracebackColor[cursorLineIndex+1] = tracebackColor[cursorLineIndex];
	tracebackNumLines[cursorLineIndex+1] = tracebackNumLines[cursorLineIndex];
	tracebackTimeStamps[cursorLineIndex+1] = tracebackTimeStamps[cursorLineIndex];
	tracebackStrBreakPnt[cursorLineIndex+1] = tracebackStrBreakPnt[cursorLineIndex];
	executingLines[cursorLineIndex+1] = executingLines[cursorLineIndex];
	executionDegrade[cursorLineIndex+1] = executionDegrade[cursorLineIndex];
	executionTimeStamp[cursorLineIndex+1] = executionTimeStamp[cursorLineIndex];
	// if the language of this pane is Python, we have to change the traceback error string to update the line number
	if (thisLang == 1) {
		int newKey = cursorLineIndex + 1;
		size_t secondBreakPnt = tracebackStr[newKey].substr(tracebackStrBreakPnt[newKey]).find(",");
		if (secondBreakPnt != std::string::npos) {
			tracebackStr[newKey] = tracebackStr[newKey].substr(0, tracebackStrBreakPnt[newKey]) + std::to_string(newKey+1) + tracebackStr[newKey].substr(tracebackStrBreakPnt[newKey]+secondBreakPnt);
		}
	}

	allStrings[cursorLineIndex] = "";
	allStringStartPos[cursorLineIndex] = 0;
	tracebackStr[cursorLineIndex] = "";
	tracebackColor[cursorLineIndex] = 0;
	tracebackNumLines[cursorLineIndex] = 1;
	tracebackTimeStamps[cursorLineIndex] = 0;
	tracebackStrBreakPnt[cursorLineIndex] = 0;
	executingLines[cursorLineIndex] = false;
	executionDegrade[cursorLineIndex] = 0;
	executionTimeStamp[cursorLineIndex] = 0;
}

//--------------------------------------------------------------
void Editor::copyOnLineDelete()
{
	// in case of backspace the cursorLineIndex variable has been updated before this function call
	// in case of delete, the variable doesn't change
	// in any case, it already points to the right key of the std::map below
	std::map<int, std::string>::iterator it1 = allStrings.find(cursorLineIndex);
	std::map<int, std::string>::iterator it2 = allStrings.find(cursorLineIndex+1);
	// first concatenate the two std::strings
	if (it2 != allStrings.end()) {
		it1->second += it2->second;
		// before moving the keys of the std::maps we must erase the keys of the line below the cursor
		eraseMapKeys(cursorLineIndex+1);
	}
	// since cursorLineIndex is updated, if we are deleting line 10
	// cursorLineIndex is 9 before we enter this function
	// but we want to change the keys of lines 11 onward
	// advance though moves the iterator to the next element
	// and since we have already erased the next line, we advance one element only
	int thisKey = it1->first;
	std::advance(it1, 1);
	// the keys of the iterator and its previous positions should be two numbers apart
	// which is true only if there is at least one more line after the line we have erased
	if (it1->first == thisKey + 2) {
		while (it1 != allStrings.end()) {
			int key = it1->first;
			changeMapKeys(key, -1);
			++it1;
		}
	}
}

//--------------------------------------------------------------
void Editor::newLine()
{
	bool stringBreaks = false;
	// a string breaks only if we're at some mid point in the line
	// and not if we're either at the end or the beginning of it
	if (cursorPos < ((int)allStrings[cursorLineIndex].size()-allStringStartPos[cursorLineIndex]) &&
		!(cursorPos == 0 && allStringStartPos[cursorLineIndex] == 0)) {
		stringBreaks = true;
	}
	if (cursorLineIndex != (lineCount-1)) {
		moveLineNumbers(1);
	}
	if (stringBreaks) {
		// create a new line with the remainder of the string
		createNewLine(allStrings[cursorLineIndex].substr(cursorPos+allStringStartPos[cursorLineIndex]), 1);
		// and change the string where the cursor is (the cursorLineIndex hasn't been updated yet)
		allStrings[cursorLineIndex] = allStrings[cursorLineIndex].substr(0, cursorPos+allStringStartPos[cursorLineIndex]);
	}
	else {
		createNewLine("", 1);
	}
	// check if we hit enter at the beginning of a line, which means we have to move all the line data
	if (cursorPos == 0 && allStringStartPos[cursorLineIndex] == 0) {
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
		for (std::map<int, std::string>::iterator it = allStrings.find(tempIndex); it != allStrings.end(); ++it) {
			if (it->second.size() > 0) {
				tempIndex = it->first;
				break;
			}
		}
		setLineIndexesDownward(tempIndex);
	}
	cursorPos = 0;
}

//--------------------------------------------------------------
bool Editor::changeMapKey(std::map<int, std::string> *m, int key, int increment, bool createNonExisting)
{
	if (m->find(key) != m->end()) {
		auto nodeHolder = m->extract(key);
		nodeHolder.key() = key + increment;
		m->insert(std::move(nodeHolder));
		return true;
	}
	else {
		if (createNonExisting) (*m)[key+increment] = "";
		return false;
	}
}

//--------------------------------------------------------------
void Editor::changeMapKey(std::map<int, int> *m, int key, int increment, bool createNonExisting)
{
	if (m->find(key) != m->end()) {
		auto nodeHolder = m->extract(key);
		nodeHolder.key() = key + increment;
		m->insert(std::move(nodeHolder));
	}
	else {
		if (createNonExisting) (*m)[key+increment] = 0;
	}
}

//--------------------------------------------------------------
void Editor::changeMapKey(std::map<int, uint64_t> *m, int key, int increment, bool createNonExisting)
{
	if (m->find(key) != m->end()) {
		auto nodeHolder = m->extract(key);
		nodeHolder.key() = key + increment;
		m->insert(std::move(nodeHolder));
	}
	else {
		if (createNonExisting) (*m)[key+increment] = 0;
	}
}

//--------------------------------------------------------------
void Editor::changeMapKey(std::map<int, bool> *m, int key, int increment, bool createNonExisting)
{
	if (m->find(key) != m->end()) {
		auto nodeHolder = m->extract(key);
		nodeHolder.key() = key + increment;
		m->insert(std::move(nodeHolder));
	}
	else {
		if (createNonExisting) (*m)[key+increment] = false;
	}
}

//--------------------------------------------------------------
void Editor::changeMapKeys(int key, int increment)
{
	// since all maps below are created with every new line
	// we can safely make all the calls below inside the same loop
	// and not need to create a separate loop for each
	bool createNonExisting = changeMapKey(&allStrings, key, increment, false);
	// if the string map key exists, then create entries for all other maps
	// in case any of them doesn't have the current key (which should not be the case)
	changeMapKey(&allStringStartPos, key, increment, createNonExisting);
	changeMapKey(&tracebackStr, key, increment, createNonExisting);
	changeMapKey(&tracebackColor, key, increment, createNonExisting);
	changeMapKey(&tracebackNumLines, key, increment, createNonExisting);
	changeMapKey(&tracebackTimeStamps, key, increment, createNonExisting);
	changeMapKey(&tracebackStrBreakPnt, key, increment, createNonExisting);
	changeMapKey(&executingLines, key, increment, createNonExisting);
	changeMapKey(&executionDegrade, key, increment, createNonExisting);
	changeMapKey(&executionTimeStamp, key, increment, createNonExisting);
	changeMapKey(&linesConnectedToBar, key, increment, createNonExisting);
	changeMapKey(&activeLineElements, key, increment, createNonExisting);
	// get the bar number the current line is connected to
	int barNdx = linesConnectedToBar[key+increment];
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
		size_t secondBreakPnt = tracebackStr[newKey].substr(tracebackStrBreakPnt[newKey]).find(",");
		if (secondBreakPnt != std::string::npos) {
			tracebackStr[newKey] = tracebackStr[newKey].substr(0, tracebackStrBreakPnt[newKey]) + std::to_string(newKey+1) + tracebackStr[newKey].substr(tracebackStrBreakPnt[newKey]+secondBreakPnt);
		}
	}
}

//--------------------------------------------------------------
void Editor::eraseMapKey(std::map<int, std::string> *m, int key)
{
	m->erase(key);
}

//--------------------------------------------------------------
void Editor::eraseMapKey(std::map<int, int> *m, int key)
{
	m->erase(key);
}

//--------------------------------------------------------------
void Editor::eraseMapKey(std::map<int, uint64_t> *m, int key)
{
	m->erase(key);
}

//--------------------------------------------------------------
void Editor::eraseMapKey(std::map<int, bool> *m, int key)
{
	m->erase(key);
}

//--------------------------------------------------------------
void Editor::eraseMapKeys(int key)
{
	// first store the value of the linesConnectedToBar map which is the bar this line connects to
	int barNdx = linesConnectedToBar[key];
	// since all maps below are created with every new line
	// we can safely make all the calls below inside the same loop
	// and dont't need to create a separate loop for each
	eraseMapKey(&allStrings, key);
	eraseMapKey(&allStringStartPos, key);
	eraseMapKey(&tracebackStr, key);
	eraseMapKey(&tracebackColor, key);
	eraseMapKey(&tracebackNumLines, key);
	eraseMapKey(&tracebackTimeStamps, key);
	eraseMapKey(&tracebackStrBreakPnt, key);
	eraseMapKey(&executingLines, key);
	eraseMapKey(&executionDegrade, key);
	eraseMapKey(&executionTimeStamp, key);
	eraseMapKey(&linesConnectedToBar, key);
	eraseMapKey(&activeLineElements, key);
	// scroll through all instruments to see which one is connected to this bar and this line
	for (auto it = instsConnectedToLine.begin(); it != instsConnectedToLine.end(); ++it) {
		auto it2 = it->second.find(barNdx);
		// if this bar is stored in the instsConnectedToLine map, erase it
		if (it2 != it->second.end()) {
			instsConnectedToLine[it->first].erase(it2->first);
		}
	}
}

//--------------------------------------------------------------
void Editor::connectLineToBar(int lineNdx, int instNdx, int barNdx)
{
	linesConnectedToBar[lineNdx] = barNdx;
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
	activeLineElements[lineNdx] = elementNdx;
}

//--------------------------------------------------------------
void Editor::setAnimation(bool state)
{
	animationState = state;
}

//--------------------------------------------------------------
bool Editor::isThisATab(int pos)
{
	// check if the current position is a tab and return its index within allStringTabs
	if (pos < 0) return false;
	if (pos + TABSIZE > (int)allStrings[cursorLineIndex].length()) return false;
	if (allStrings[cursorLineIndex].substr(pos, TABSIZE).compare(tabStr) == 0) {
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
	return std::min((int)allStrings[cursorLineIndex].size(), maxCharactersPerString);
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
	if (pos == 0) allStringStartPos[cursorLineIndex] = 0;
	else if ((int)allStrings[cursorLineIndex].size() > maxCharactersPerString) {
		allStringStartPos[cursorLineIndex] = (int)allStrings[cursorLineIndex].size() - maxCharactersPerString;
	}
}

//--------------------------------------------------------------
bool Editor::startsWith(std::string a, std::string b)
{
	if (a.size() < b.size()) return false;
	if (a.substr(0, b.size()).compare(b) == 0) return true;
	return false;
}

//--------------------------------------------------------------
bool Editor::endsWith(std::string a, std::string b)
{
	if (a.size() < b.size()) return false;
	if (a.substr(a.size()-b.size()).compare(b) == 0) return true;
	return false;
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
		}
		else if ((allStrings[cursorLineIndex].size() > 0) && (cursorPos > 0)) {
			int numCharsToDelete = 1;
			// check if the cursor is at the end of the string
			if (cursorPos == (int)allStrings[cursorLineIndex].size()-allStringStartPos[cursorLineIndex]) {
				if (isThisATab(cursorPos-TABSIZE)) numCharsToDelete = TABSIZE;
				if ((int)allStrings[cursorLineIndex].size() >= numCharsToDelete) {
					deletedChar = allStrings[cursorLineIndex].substr(allStrings[cursorLineIndex].size()-numCharsToDelete, numCharsToDelete);
					allStrings[cursorLineIndex] = allStrings[cursorLineIndex].substr(0, allStrings[cursorLineIndex].size()-numCharsToDelete);
				}
			}
			else {
				if (isThisATab(cursorPos-TABSIZE)) numCharsToDelete = TABSIZE;
				std::string first;
				std::string second;
				if ((int)allStrings[cursorLineIndex].size() >= cursorPos+allStringStartPos[cursorLineIndex]-numCharsToDelete) {
					first = allStrings[cursorLineIndex].substr(0, cursorPos+allStringStartPos[cursorLineIndex]-numCharsToDelete);
					second = allStrings[cursorLineIndex].substr(cursorPos+allStringStartPos[cursorLineIndex]);
					deletedChar = allStrings[cursorLineIndex].substr(cursorPos+allStringStartPos[cursorLineIndex]-numCharsToDelete, numCharsToDelete);
					allStrings[cursorLineIndex] = first + second;
				}
			}
			allStringStartPos[cursorLineIndex] -= numCharsToDelete;
			if (allStringStartPos[cursorLineIndex] < 0) {
				allStringStartPos[cursorLineIndex] = 0;
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
			int thisLineStrLength = allStrings[cursorLineIndex].size();
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
		if ((int)allStrings[cursorLineIndex].size() < maxCharactersPerString) {
			allStringStartPos[cursorLineIndex] = 0;
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
						pyStr += allStrings[i+executingLineLocal];
					}
					// execute the Python line in verbose mode with the last argument set to 1
					err = ((ofApp*)ofGetAppPtr())->sharedData.pyo.exec(pyStr.c_str(), 1);
					if (err) {
						std::string errStr = ((ofApp*)ofGetAppPtr())->sharedData.pyo.getErrorMsg();
						size_t lineNdx = errStr.find("line")+ 5;
						errStr = errStr.substr(0, lineNdx) + std::to_string(executingLineLocal+1) + errStr.substr(lineNdx+errStr.substr(lineNdx+1).find(" "));
						setTraceback(3, errStr, executingLineLocal, lineNdx);
					}
					else {
						std::vector<std::string> pyStdout = ((ofApp*)ofGetAppPtr())->sharedData.pyo.getStdout();
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
									whichPane = ((ofApp*)ofGetAppPtr())->whichPane;
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
									((ofApp*)ofGetAppPtr())->sharedData.pyStdoutStr += s.substr(startChar);
								}
								else {
									setTraceback(1, s, executingLineLocal);
								}
						    }
							if (sendToLily) {
								((ofApp*)ofGetAppPtr())->sharedData.whichPyPane = whichPane;
								((ofApp*)ofGetAppPtr())->sharedData.typePyStr = true;
							}
						}
						else {
						    releaseTraceback(executingLineLocal);
						}
					}
#endif
					break;
				default:
					break;
			}
			if (highlightManyChars) highlightManyChars = false;
		}
		else {
			if (highlightBracket && cursorPos > 0) {
				// if we hit return while in between two curly brackets
				if (allStrings[cursorLineIndex].substr(cursorPos-1, 1).compare("{") == 0 &&
						allStrings[cursorLineIndex].substr(cursorPos, 1).compare("}") == 0) {
					// we insert two new lines and add a horizontal tab in the middle
					newLine();
					lineBreaking = false;
					// getNestDepth() returns a vector with the bottom line of the code chunk
					// within the nest of curly brackets and the depth of the nest
					// the bottom line is used in allOtherKeys()
					std::vector<int> nestDepth = getNestDepth();
					// if it's the first nest depth, it will be 0 as no tab has been inserted yet
					// so we test against nestDepth[1]+1
					int numTabsToWrite = nestDepth[1] + 1;
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
				int numTabs = getNumTabsInStr(allStrings[cursorLineIndex-1]);
				// 1 is Python, so if we're writing in Python and the last character is a colon, we insert an extra horintal tab
				if (thisLang == 1 && cursorLineIndex > 0 && allStrings[cursorLineIndex-1].size() > 0 && allStrings[cursorLineIndex-1].back() == ':') {
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
	}

	// 127 is del
	else if (key == 127) {
		if (highlightManyChars) {
			deleteString();
		}
		else if (cursorPos < (int)allStrings[cursorLineIndex].size()) {
			int numCharsToDelete = 1;
			std::string deletedChar;
			if (isThisATab(cursorPos)) numCharsToDelete = TABSIZE;
			std::string first = allStrings[cursorLineIndex].substr(0, cursorPos);
			std::string second = allStrings[cursorLineIndex].substr(cursorPos+numCharsToDelete);
			deletedChar = allStrings[cursorLineIndex].substr(cursorPos, numCharsToDelete);
			allStrings[cursorLineIndex] = first + second;
			if ((cursorPos == 0) && (allStrings[cursorLineIndex].size() == 0)) {
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
	int maxStringPos = (int)allStrings[cursorLineIndex].size() - allStringStartPos[cursorLineIndex];
	if (cursorPos == maxStringPos) {
		allStrings[cursorLineIndex] += charToInsert;
	}
	else {
		// otherwise, if it's in some middle position, insert the character
		if (cursorPos > 0) {
			//std::cout << cursorPos << " " << allStringStartPos[cursorLineIndex] << " " << allStrings[cursorLineIndex].size() << std::endl;
			std::string strOne;
			if (cursorPos + allStringStartPos[cursorLineIndex] >= (int)allStrings[cursorLineIndex].size()) {
				strOne = allStrings[cursorLineIndex];
			}
			else {
				strOne = allStrings[cursorLineIndex].substr(0, cursorPos+allStringStartPos[cursorLineIndex]);
			}
			std::string strTwo;
			if (cursorPos + allStringStartPos[cursorLineIndex] >= (int)allStrings[cursorLineIndex].size()) {
				strTwo = "";
			}
			else {
				strTwo = allStrings[cursorLineIndex].substr(cursorPos+allStringStartPos[cursorLineIndex]);
			}
			allStrings[cursorLineIndex] = strOne + charToInsert + strTwo;
			if ((int)allStrings[cursorLineIndex].size() > maxCharactersPerString) {
				// if the string is long don't move the cursor but scroll the string
				allStringStartPos[cursorLineIndex] += cursorPosIncrement;
				cursorPosIncrement = 0;
			}
		}
		// or if it's at the beginning, place the character at the beginning
		else {
			std::string str;
			if (cursorPos + allStringStartPos[cursorLineIndex] >= (int)allStrings[cursorLineIndex].size()) {
				str = "";
			}
			else {
				str = allStrings[cursorLineIndex].substr(cursorPos+allStringStartPos[cursorLineIndex]);
			}
			allStrings[cursorLineIndex] = charToInsert + str;
		}
	}
	if (cursorPos == maxCharactersPerString) {
		allStringStartPos[cursorLineIndex] = (int)allStrings[cursorLineIndex].size() - maxCharactersPerString;
		// if we're placing a double char ({} or [] or "" or ()) at the end of a long string
		if (doubleChar && allStringStartPos[cursorLineIndex] == ((int)allStrings[cursorLineIndex].size()-maxCharactersPerString)) {
			allStringStartPos[cursorLineIndex] -= cursorPosIncrement;
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
	if (s == "\n" || s == "\b") {
		int ascii = int(s[0]);
		if (ascii == 10) ascii = 13;
		int tempPaneNdx = ((ofApp*)ofGetAppPtr())->whichPane;
		// if we're executing code from another pane that runs Python
		// we must change the current active pane of the editor to the one receiving the string from Python
		// because parseStrings() in ofApp.cpp executes lines from the currently active pane
		if (ascii == 13 && (ctrlPressed || shiftPressed)) {
			((ofApp*)ofGetAppPtr())->whichPane = ((ofApp*)ofGetAppPtr())->sharedData.whichPyPane;
		}
		allOtherKeys(ascii);
		// if we have executed code, we must reset the active pane
		if (ascii == 13 && (ctrlPressed || shiftPressed)) {
			((ofApp*)ofGetAppPtr())->whichPane = tempPaneNdx;
		}
	}
	else {
		std::string strWithoutTabs = replaceCharInStr(s, "\t", tabStr);
		allStrings[cursorLineIndex] += strWithoutTabs;
	}
	allStringStartPos[cursorLineIndex] = ((int)allStrings[cursorLineIndex].size() > maxCharactersPerString ? (int)allStrings[cursorLineIndex].size() - maxCharactersPerString : 0);
	cursorPos = maxCursorPos();
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
std::string Editor::replaceCharInStr(std::string str, std::string a, std::string b)
{
	auto it = str.find(a);
	while (it != std::string::npos) {
		str.replace(it, a.size(), b);
		it = str.find(a);
	}
	return str;
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
	int diff;
	if (lineIndex >= 0) {
		diff = cursorLineIndex - lineIndex;
		cursorLineIndex = lineIndex;
	}
	else {
		diff = 1;
		cursorLineIndex--;
	}
	if (cursorLineIndex < 0) {
		cursorLineIndex = 0;
		if (inserting) cursorPos = 0;
		return false;
	}
	else if (cursorLineIndex < lineCountOffset) {
		lineCountOffset -= diff;
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
	if (setLineIndexesUpward(lineIndex)) {
		cursorPos = arrowCursorPos;
		// test if we arrive at a shorter std::string than the one below
		if (cursorPos >= ((int)allStrings[cursorLineIndex].size()-allStringStartPos[cursorLineIndex])) {
			// in which case display the cursor right after the std::string
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
	if (lineCount > maxNumLines) {
		int limit = lineCount - (lineCount - maxNumLines) + lineCountOffset - 1;
		if (cursorLineIndex > limit) {
			if (lineCountOffset < (lineCount-maxNumLines)) {
				lineCountOffset += diff;
				lineCountOffset = std::min(lineCountOffset, (lineCount-maxNumLines));
			}
			else {
				cursorLineIndex = lineCount - 1;
				if (inserting) cursorPos = arrowCursorPos = maxCursorPos();
			}
		}
		return true;
	}
	else if (cursorLineIndex > (lineCount-1)) {
		cursorLineIndex = lineCount - 1;
		if (inserting) cursorPos = maxCursorPos();
		return false;
	}
	else {
		return true;
	}
}

//--------------------------------------------------------------
void Editor::downArrow(int lineIndex)
{
	int prevCursorPos = cursorPos;
	int prevCursorLineIndex = cursorLineIndex;
	//if (lineIndex > 0) lineIndex = maxNumLines+1;
	if (setLineIndexesDownward(lineIndex)) {
		cursorPos = arrowCursorPos;
		// test if we arrive at a shorter string than the one above
		if (cursorPos >= ((int)allStrings[cursorLineIndex].size()-allStringStartPos[cursorLineIndex])) {
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
			whiteSpaceIndex = allStrings[cursorLineIndex].find(" ", whiteSpaceIndex+1);
			if (whiteSpaceIndex == std::string::npos) {
				whiteSpaceIndex = allStrings[cursorLineIndex].size();
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
		if (inserting) {
			// if the string is still too long to display
			if (((int)allStrings[cursorLineIndex].size()-allStringStartPos[cursorLineIndex]) > maxCharactersPerString) {
				allStringStartPos[cursorLineIndex] += numPosToMove;
				// place the cursor back so that it stays at the last position
				cursorPos -= numPosToMove;
				arrowCursorPos = cursorPos;
			}
			// if we're at the end of the string
			else {
				if (cursorLineIndex < (lineCount-1)) {
					allStringStartPos[cursorLineIndex] = 0;
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
		else {
			cursorPos -= numPosToMove;
			arrowCursorPos = cursorPos;
			numPosToMove = 0;
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
		size_t whiteSpaceIndex = allStrings[cursorLineIndex].size();
		while ((int)whiteSpaceIndex >= cursorPos) {
			if (whiteSpaceIndex < 2) {
				whiteSpaceIndex = 0;
				break;
			}
			whiteSpaceIndex = allStrings[cursorLineIndex].rfind(" ", whiteSpaceIndex-1);
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
		if (inserting) {
			if ((int)allStrings[cursorLineIndex].size() > maxCharactersPerString && allStringStartPos[cursorLineIndex] > 0) {
				cursorPos += numPosToMove;
				arrowCursorPos = cursorPos;
				allStringStartPos[cursorLineIndex] -= numPosToMove;
			}
			if (allStringStartPos[cursorLineIndex] <= 0) {
				allStringStartPos[cursorLineIndex] = 0;
				if (cursorLineIndex > 0) {
					cursorLineIndex--;
					cursorPos = arrowCursorPos = std::min((int)allStrings[cursorLineIndex].size(), maxCharactersPerString);
					allStringStartPos[cursorLineIndex] = (int)allStrings[cursorLineIndex].size() - maxCharactersPerString;
				}
				else {
					cursorPos = arrowCursorPos = 0;
				}
			}
		}
		else {
			cursorPos = arrowCursorPos = 0;
		}
	}
	if (allStringStartPos[cursorLineIndex] < 0) {
		allStringStartPos[cursorLineIndex] = 0;
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
void Editor::pageUp()
{
	int prevCursorPos = cursorPos;
	int prevCursorLineIndex = cursorLineIndex;
	if (cursorLineIndex == 0) {
		cursorPos = 0;
		if (highlightManyChars) highlightManyChars = false;
		return;
	}
	cursorLineIndex -= (maxNumLines-1);
	lineCountOffset -= (maxNumLines-1);
	if (cursorLineIndex < 0) cursorLineIndex = 0;
	if (lineCountOffset < 0) lineCountOffset = 0;
	cursorPos = arrowCursorPos;
	// test if we arrive at a shorter std::string than the one above
	if (cursorPos >= ((int)allStrings[cursorLineIndex].size()-allStringStartPos[cursorLineIndex])) {
		// in which case display the cursor right after the std::string
		cursorPos = std::min(maxCursorPos(), arrowCursorPos);
	}
	int tabIndex = didWeLandOnATab();
	if (tabIndex >= 0) cursorPos = tabIndex;
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
	int prevCursorPos = cursorPos;
	int prevCursorLineIndex = cursorLineIndex;
	if (cursorLineIndex == (lineCount-1)) {
		cursorPos = maxCursorPos();
		if (highlightManyChars) highlightManyChars = false;
		return;
	}
	cursorLineIndex += (maxNumLines-1);
	lineCountOffset += (maxNumLines-1);
	if (cursorLineIndex > (lineCount-1)) cursorLineIndex = lineCount - 1;
	if (lineCountOffset > (lineCount - maxNumLines)) {
		lineCountOffset = lineCount - maxNumLines;
		if (lineCountOffset < 0) {
			lineCountOffset = 0;
		}
	}
	cursorPos = arrowCursorPos;
	// test if we arrive at a shorter std::string than the one above
	if (cursorPos >= ((int)allStrings[cursorLineIndex].size()-allStringStartPos[cursorLineIndex])) {
		// in which case display the cursor right after the string
		cursorPos = std::min(maxCursorPos(), arrowCursorPos);
	}
	int tabIndex = didWeLandOnATab();
	if (tabIndex >= 0) cursorPos = tabIndex;
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
	bool insertingStateChanged = (inserting != insertingBool ? true : false);
	inserting = insertingBool;
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
}

//--------------------------------------------------------------
bool Editor::getVisualMode()
{
	return visualMode;
}

//--------------------------------------------------------------
void Editor::detectExecutingChunk()
{
	std::vector<int> nestDepth = getNestDepth();
	if (nestDepth[1] > 0) {
		// set the number of lines to execute
		topLine = findTopLine(nestDepth[0]);
		bottomLine = nestDepth[0];
		highlightManyChars = true;
	}
	executingLine = cursorLineIndex;
	// set the execution variables
	if (highlightManyChars) {
		for (int i = topLine; i <= bottomLine; i++) {
			executingLines[i] = true;
			executionTimeStamp[i] = ofGetElapsedTimeMillis();
			executionDegrade[i] = EXECUTIONBRIGHTNESS;
		}
	}
	else {
		executingLines[cursorLineIndex] = true;
		executionTimeStamp[cursorLineIndex] = ofGetElapsedTimeMillis();
		executionDegrade[cursorLineIndex] = EXECUTIONBRIGHTNESS;
	}
}

//--------------------------------------------------------------
void Editor::allOtherKeys(int key)
{
	bool executing = false;
	bool lineBreaking = true;
	bool callAssemble = true;
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
		else if (key == 120) { // z for cut
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
			setHighlightManyChars(0, allStrings[lineCount-1].size(), 0, lineCount-1);
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
			v[1] = (int)allStrings[i].size() - v[0];
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
		v[1] = allStrings[i].size();
	}
	return v;
}

//--------------------------------------------------------------
void Editor::copyString(int copyTo, bool dehighlight)
{
	std::string strToCopy = "";
	if (highlightManyChars) {
		for (int i = topLine; i <= bottomLine; i++) {
			std::vector<int> posAndSize = setSelectedStrStartPosAndSize(i);
			// apply the offset of the std::string, in case it's too long to fit in a pane
			posAndSize[0] += allStringStartPos[i];
			strToCopy += allStrings[i].substr(posAndSize[0], posAndSize[1]);
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
				yankedStr = allStrings[cursorLineIndex].substr(cursorPos, 1);
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
}

//--------------------------------------------------------------
void Editor::pasteString(int pasteFrom)
{
	std::string fromClipboard;
	// first check if we do have a std::string to paste
	switch (pasteFrom) {
		case 0:
			fromClipboard = ofGetWindowPtr()->getClipboardString();
			if (fromClipboard.empty()) return;
			break;
		case 1:
			if (yankedStr.empty()) return;
			break;
		default:
			break;
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
	// first check if we're pasting over selected text
	if (highlightManyChars) deleteString();
	// set the position of the cursor according to the mode of the pane
	int tempCursorPos = cursorPos;
	if (!inserting) tempCursorPos++;
	// if we paste in the middle of a std::string
	// the last line must concatenate the pasted std::string with the remaining std::string of the line we broke in the middle
	std::string remainingStr = "";
	// but first we need to check if there is anything left in the std::string after the cursor
	if (tempCursorPos < (int)allStrings[cursorLineIndex].size()) {
		remainingStr += allStrings[cursorLineIndex].substr(tempCursorPos+allStringStartPos[cursorLineIndex]);
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
			allStrings[tempCursorLineIndex] = allStrings[tempCursorLineIndex].substr(0, tempCursorPos+allStringStartPos[tempCursorLineIndex]);
			allStrings[tempCursorLineIndex] += token;
		}
		if (i == (tokens.size()-1)) {
			// if we're pasting one line only just append the remaining std::string
			if (i == 0) {
				allStrings[tempCursorLineIndex] += remainingStr;
			}
			// otherwise concatenate the token with the remaining
			else {
				allStrings[tempCursorLineIndex] = token + remainingStr;
			}
		}
		if ((i > 0) && (i < (tokens.size()-1))) {
			allStrings[tempCursorLineIndex] = token;
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
void Editor::clearText()
{
	allStrings.clear();
	allStringStartPos.clear();
	tracebackStr.clear();
	tracebackColor.clear();
	tracebackNumLines.clear();
	tracebackStrBreakPnt.clear();
	lineCount = 1;
	lineCountOffset = 0;
	cursorLineIndex = cursorPos = 0;
	highlightManyChars = false;
}

//--------------------------------------------------------------
void Editor::deleteString()
{
	if (!inserting && !highlightManyChars) {
		allStrings[cursorLineIndex].erase(cursorPos, 1);
	}
	else if (highlightManyChars) {
		// if we're deleting all text
		if (bottomLine - topLine == lineCount - 1) {
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
					size_t strSizeBefore = allStrings[i].size();
					if (topLine == bottomLine && !inserting && posAndSize[0]+posAndSize[1] < maxCursorPos()) posAndSize[1]++;
					allStrings[i].erase(posAndSize[0], posAndSize[1]);
					if (allStrings[i].size() == 0) {
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
					if (!inserting && posAndSize[0]+posAndSize[1] < maxCursorPos()) posAndSize[1]++;
					allStrings[i].erase(posAndSize[0], posAndSize[1]);
					// position the cursor at the concatenation point
					cursorPos = (int)allStrings[topLine].size();
					// then concatenate the top and bottom lines to one string
					allStrings[topLine] += allStrings[bottomLine];
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
	if (errorCode == 0 || errorCode == 1) tracebackColor[lineNum] = 0;
	else if (errorCode == 2) tracebackColor[lineNum] = 1;
	else if (errorCode == 3) tracebackColor[lineNum] = 2;
	else tracebackColor[lineNum] = 0;
	// find number of newlines in traceback std::string
	int numLines = count(begin(errorStr), end(errorStr), '\n') + 1;
	tracebackStr[lineNum] = errorStr;
	tracebackTimeStamps[lineNum] = ofGetElapsedTimeMillis();
	tracebackNumLines[lineNum] = numLines;
	tracebackStrBreakPnt[lineNum] = strBreakPnt;
}

//--------------------------------------------------------------
void Editor::releaseTraceback(int lineNum)
{
	std::map<int, std::string>::iterator it = tracebackStr.find(lineNum);
	if (it != tracebackStr.end()) {
		tracebackStr[lineNum] = "";
		tracebackColor[lineNum] = 0;
		tracebackNumLines[lineNum] = 1;
		tracebackStrBreakPnt[lineNum] = 0;
	}
}

//--------------------------------------------------------------
std::string Editor::getTracebackStr(int lineNum)
{
	return tracebackStr[lineNum];
}

//--------------------------------------------------------------
int Editor::getTracebackColor(int lineNum)
{
	return tracebackColor[lineNum];
}

//--------------------------------------------------------------
uint64_t Editor::getTracebackTimeStamp(int lineNum)
{
	return tracebackTimeStamps[lineNum];
}

//--------------------------------------------------------------
int Editor::getTracebackNumLines(int lineNum)
{
	return tracebackNumLines[lineNum];
}

//--------------------------------------------------------------
uint64_t Editor::getTracebackDur()
{
	return TRACEBACKDUR;
}

//--------------------------------------------------------------
std::map<int, uint64_t>::iterator Editor::getTracebackTimeStampsBegin()
{
	return tracebackTimeStamps.begin();
}

//--------------------------------------------------------------
std::map<int, uint64_t>::iterator Editor::getTracebackTimeStampsEnd()
{
	return tracebackTimeStamps.end();
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
	std::cout << "saving \"" << fileName << "\"\n";
	if (!endsWith(fileName, ".lyv") || !endsWith(fileName, ".py") || !endsWith(fileName, ".lua")) {
		fileName += ".lyv";
	}
	std::ofstream file(fileName.c_str());
	if (!file.is_open()) {
		// if for some reason the file can't be opened
		// instead of the file name, the string "could not open file"
		// will be displayed for 2 seconds and then it will return to untitled.lyv
		couldNotSaveFile = true;
		loadedFileStr = "could not save file";
		fileLoadErrorTimeStamp = ofGetElapsedTimeMillis();
		return;
	}
	for (unsigned i = 0; i < allStrings.size(); i++) {
		file << allStrings[i] << "\n";
	}
	file.close();
	fileLoaded = true;
	size_t lastBackSlash = fileName.find_last_of("/");
	loadedFileStr = fileName.substr(lastBackSlash+1);
	loadedFileFullPath = fileName;
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
		loadedFileStr = "can't overwrite non-existing file, use :save";
		fileLoadErrorTimeStamp = ofGetElapsedTimeMillis();
		return;
	}
	std::ofstream file(loadedFileFullPath.c_str());
	if (!file.is_open()) {
		// if for some reason the file can't be opened
		// instead of the file name, the string "could not open file"
		// will be displayed for 2 seconds and then it will return to untitled.lyv
		couldNotSaveFile = true;
		loadedFileStr = "could not open file";
		fileLoadErrorTimeStamp = ofGetElapsedTimeMillis();
		return;
	}
	for (unsigned i = 0; i < allStrings.size(); i++) {
		file << allStrings[i] << "\n";
	}
	file.close();
	fileEdited = false;
}

//--------------------------------------------------------------
size_t Editor::findChordEnd(std::string str)
{
	size_t ndx = str.find_last_of(">");
	if (ndx + 1 < str.size()) {
		if (str[ndx+1] == '}') {
			ndx = str.substr(0, ndx).find_last_of(" ");
		}
	}
	if (ndx > 0) {
		// if the previous character is a hyphen
		// it means that this ">" symbol is an articulation symbol
		// or if the previous character is a backslash
		// it means that this ">" symbol is a diminuendo
		// not a chord end symbol
		if (str[ndx-1] == '-' || str[ndx-1] == '\\') {
			ndx = str.substr(0, ndx).find_last_of(">");
		}
	}
	return ndx;
}

//--------------------------------------------------------------
size_t Editor::findChordStart(std::string str)
{
	size_t ndx = str.find_last_of("<");
	if (ndx > 0) {
		// if the previous character is a hyphen
		// it means that this ">" symbol is an articulation symbol
		// or if the previous character is a backslash
		// it means that this ">" symbol is a diminuendo
		// not a chord end symbol
		if (str[ndx-1] == '-' || str[ndx-1] == '\\') {
			ndx = str.substr(0, ndx).find_last_of("<");
		}
	}
	return ndx;
}

/************************ loading files ***********************/

/////////////////// loading MusicXML files /////////////////////

//--------------------------------------------------------------
void Editor::loadXMLFile(std::string filePath)
{
	std::ifstream file(filePath.c_str());
	std::string line;
	createNewLine("\\score.show", 0);
	cursorLineIndex++;
	createNewLine("\\score.animate", 0);
	cursorLineIndex++;
	createNewLine("", 0);
	cursorLineIndex++;
	int partID = 0, prevPartID = 0;
	bool mustMoveKeys = false;
	int partIDOffset = -1;
	int numStaves = 1, loopIter, i;
	size_t ndx1, ndx2;
	unsigned int lineNum = 1;
	std::map<int, std::pair<int, std::pair<std::string, std::string>>> insts;
	while (getline(file, line)) {
		std::string lineWithoutTabs = replaceCharInStr(line, "\t", "");
		std::string lineWithoutSpaces = replaceCharInStr(lineWithoutTabs, " ", "");
		if (startsWith(lineWithoutSpaces, "<score-partid")) {
			partID = stoi(lineWithoutSpaces.substr(16, lineWithoutSpaces.substr(16).find_last_of("\"")));
			// some software that export MusicXML add sequential indexes to parts, even if a part has more than one staff
			// to remedy this, we make the test below and use the mustMoveKeys boolean for certain actions further down
			if (partID - prevPartID == 1) mustMoveKeys = true;
			else mustMoveKeys = false;
			prevPartID = partID;
		}
		else if (startsWith(lineWithoutSpaces, "<part-name>")) {
			ndx1 = lineWithoutSpaces.find(">");
			ndx2 = lineWithoutSpaces.find_last_of("<");
			// the key is an incrementing index
			// and the pair is a pair of pairs, where the first item is the number of staves of the instrument
			// and the second part is a pair with the part ID and the actual instrument name
			// the default number of staves is 1, hence the 1 below
			insts[partID-1] = std::make_pair(1, std::make_pair("P"+std::to_string(partID), lineWithoutSpaces.substr(ndx1+1, ndx2-ndx1-1)));
		}
		if (startsWith(lineWithoutSpaces, "<partid")) {
			partID = stoi(lineWithoutSpaces.substr(10, lineWithoutSpaces.substr(10).find_last_of("\"")));
			partID += partIDOffset;
		}
		else if (startsWith(lineWithoutSpaces, "<staves>")) {
			numStaves = stoi(lineWithoutSpaces.substr(8, lineWithoutSpaces.substr(8).find("<")));
			if (numStaves > 1) {
				if (mustMoveKeys) {
					// if an instrument has more than one staff
					// and the part IDs for different instruments are sequential,
					// change the keys of the following items in the std::map
					loopIter = (int)insts.size() - 1;
					while (loopIter > partID) {
						if (insts.find(loopIter) != insts.end()) {
							auto nodeHolder = insts.extract(loopIter);
							nodeHolder.key() = loopIter + numStaves - 1;
							insts.insert(std::move(nodeHolder));
						}
						loopIter--;
					}
				}
				insts[partID].first = numStaves;
				insts[partID].second.second += "1";
				for (i = 1; i < numStaves; i++) {
					insts[partID+i] = std::make_pair(numStaves, std::make_pair(insts[partID].second.first, insts[partID].second.second.substr(0, insts[partID].second.second.size()-1)+std::to_string(i+1)));
				}
				if (mustMoveKeys) partIDOffset += (numStaves - 1);
			}
		}
	}
	std::string instLine = "\\insts.init";
	for (auto it = insts.begin(); it != insts.end(); ++it) {
		instLine += " ";
		instLine += it->second.second.second;
	}
	createNewLine(instLine, 0);
	cursorLineIndex++;
	int instNdx = 0;
	int barNum = 0;
	int clefOffset = 0;
	size_t barNumNdx = 0;
	bool isGroupped = false;
	bool clefFollows = false;
	std::map<std::string, int> clefNdxs {{"G", 0}, {"F", 1}, {"C", 2}};
	std::map<int, std::map<int, std::pair<bool, int>>> instClefs;
	std::map<int, int> prevInstClef;
	// we have read the instruments, we now read the rest of the score to store clef changes
	file.clear();
	file.seekg(0, file.beg);
	while (getline(file, line)) {
		std::string lineWithoutTabs = replaceCharInStr(line, "\t", "");
		std::string lineWithoutSpaces = replaceCharInStr(lineWithoutTabs, " ", "");
		if (startsWith(lineWithoutSpaces, "<partid")) {
			for (auto it = insts.begin(); it != insts.end(); ++it) {
				if (it->second.second.first.compare(lineWithoutSpaces.substr(9, lineWithoutSpaces.substr(9).find_last_of("\""))) == 0) {
					// if this insturment is groupped, this is the index of the first instrument of the group
					// the incrementing indexes are found further down when we get a "<staff>" line
					instNdx = it->first;
					// we determine if this instrument is groupped by querying the first value of its pair
					// which is the value to the instrument std::map
					// this value is the number of staves of this instrument
					if (it->second.first > 1) isGroupped = true;
					else isGroupped = false;
					break;
				}
			}
		}
		else if (startsWith(lineWithoutSpaces, "<measurenumber=")) {
			barNumNdx = lineWithoutSpaces.substr(16).find("\"");
			barNum = stoi(lineWithoutSpaces.substr(16, barNumNdx));
			if (instClefs.find(instNdx) == instClefs.end()) {
				std::map<int, std::pair<bool, int>> m {{barNum, std::make_pair(false, 0)}};
				instClefs[instNdx] = m;
			}
			if (prevInstClef.find(instNdx) == prevInstClef.end()) {
				prevInstClef[instNdx] = 0;
			}
			//instClefs[instNdx][barNum] = 0;
			if (isGroupped) {
				for (i = 1; i < insts[instNdx].first; i++) {
					instClefs[instNdx+i][barNum] = std::make_pair(false, 0);
					if (prevInstClef.find(instNdx+i) == prevInstClef.end()) {
						prevInstClef[instNdx+i] = 0;
					}
				}
			}
		}
		// compare against "<clef" and not "<clef>" because instruments with more than one staves
		// use "<clef number=1>" etc.
		else if (startsWith(lineWithoutSpaces, "<clef")) {
			clefFollows = true;
			if (lineWithoutSpaces.find("number") != std::string::npos) {
				clefOffset = stoi(lineWithoutSpaces.substr(13, lineWithoutSpaces.substr(13).find("\""))) - 1;
			}
		}
		else if (startsWith(lineWithoutSpaces, "<sign>") && clefFollows) {
			instClefs[instNdx+clefOffset][barNum] = std::make_pair(true, clefNdxs[lineWithoutSpaces.substr(6, lineWithoutSpaces.find_last_of("<")-6)]);
			prevInstClef[instNdx+clefOffset] = instClefs[instNdx+clefOffset][barNum].second;
			clefFollows = false;
		}
	}
	file.clear();
	file.seekg(0, file.beg);
	int beatNumerator = 4, beatDenominator = 4;
	std::map<int, std::pair<int, int>> times;
	while (getline(file, line)) {
		std::string lineWithoutTabs = replaceCharInStr(line, "\t", "");
		std::string lineWithoutSpaces = replaceCharInStr(lineWithoutTabs, " ", "");
		if (startsWith(lineWithoutSpaces, "<measurenumber=")) {
			barNumNdx = lineWithoutSpaces.substr(16).find("\"");
			barNum = stoi(lineWithoutSpaces.substr(16, barNumNdx));
		}
		else if (startsWith(lineWithoutSpaces, "<beats>")) {
			beatNumerator = stoi(lineWithoutSpaces.substr(7, lineWithoutSpaces.substr(7).find("<")));
		}
		else if (startsWith(lineWithoutSpaces, "<beat-type>")) {
			beatDenominator = stoi(lineWithoutSpaces.substr(11, lineWithoutSpaces.substr(11).find("<")));
			times[barNum] = std::make_pair(beatNumerator, beatDenominator);
		}
	}
	for (int i = 1; i <= barNum; i++) {
		if (times.find(i) == times.end()) {
			times[i] = std::make_pair(beatNumerator, beatDenominator);
		}
		else {
			beatNumerator = times[i].first;
			beatDenominator = times[i].second;
		}
	}
	file.clear();
	file.seekg(0, file.beg);
	int tempoBase = 4, tempo = 120;
	std::map<std::string, int> tempoBases {{"whole", 1}, {"half", 2}, {"quarter", 4}, {"eighth", 8}, {"16th", 16}, {"32nd", 32}, {"64th", 64}};
	std::map<int, std::pair<int, int>> tempi;
	while (getline(file, line)) {
		std::string lineWithoutTabs = replaceCharInStr(line, "\t", "");
		std::string lineWithoutSpaces = replaceCharInStr(lineWithoutTabs, " ", "");
		if (startsWith(lineWithoutSpaces, "<measurenumber=")) {
			barNumNdx = lineWithoutSpaces.substr(16).find("\"");
			barNum = stoi(lineWithoutSpaces.substr(16, barNumNdx));
		}
		else if (startsWith(lineWithoutSpaces, "<beat-unit")) {
			tempoBase = tempoBases[lineWithoutSpaces.substr(11, lineWithoutSpaces.substr(11).find("<"))];
		}
		else if (startsWith(lineWithoutSpaces, "<per-minute")) {
			size_t firstDigitNdx = lineWithoutSpaces.find_first_of("0123456789");
			size_t lastDigitNdx = lineWithoutSpaces.find_last_of("0123456789");
			//std::string perminStr = lineWithoutSpaces.substr(firstDigitNdx, lastDigitNdx);
			//if (perminStr.find("c.") != std::string::npos) {
			//	firstDigitNdx = lineWithoutSpaces.find_first_of("0123456789");
			//	lastDigitNdx = lineWithoutSpaces.find_last_of("0123456789");
			//}
			tempo = stoi(lineWithoutSpaces.substr(firstDigitNdx, lastDigitNdx));
			tempi[barNum] = std::make_pair(tempoBase, tempo);
		}
	}
	for (int i = 1; i <= barNum; i++) {
		if (tempi.find(i) == tempi.end()) {
			tempi[i] = std::make_pair(tempoBase, tempo);
		}
		else {
			tempoBase = tempi[i].first;
			tempo = tempi[i].second;
		}
	}
	file.clear();
	file.seekg(0, file.beg);
	// and now we store the rest of the score data
	bool foundNote = false;
	bool foundChordSym = false;
	bool isChord = false;
	bool isRest = false;
	bool firstChordNote = false;
	bool foundDynamic = false;
	bool foundArticulation = false;
	bool tupletStops = false;
	bool areWeInsideTuplet = false;
	bool timeModificationFollows = false;
	bool mustAddDynamicToNextNote = false;
	bool foundWords = false;
	bool addedText = false;
	int instNdxOffset = 0;
	lineNum = 1;
	int tupletNumerator = 3, tupletDenominator = 2;
	//int direction = 1;
	int dynamicNdx = 0;
	int alter = 0;
	int octave;
	size_t wedgeNdx, tupletNdx, lastDurNdx = 0;
	//size_t openingAngleBracketNdx, closingAngleBracketNdx;
	std::map<std::string, std::string> articulations{{"marcato", "^"}, {"trill", "+"}, {"tenuto", "-"}, {"staccatissimo", "!"},
		{"accent", ">"}, {"staccato", "."}, {"portando", "_"}};
	std::string xmlDurs[7] = {"whole", "half", "quarter", "eighth", "16th", "32nd", "64th"};
	std::map<std::string, int> dynamicSymMap {{"ppp", 0}, {"pp", 1}, {"p", 2}, {"mp", 3}, {"mf", 4},
		{"f", 5}, {"ff", 6}, {"fff", 7}, {"crescendo", 8}, {"diminuendo", 9}, {"stop", 10}};
	std::map<int, std::string> dynamicNdxMap {{0, "\\ppp"}, {1, "\\pp"}, {2, "\\p"}, {3, "\\mp"}, {4, "\\mf"},
		{5, "\\f"}, {6, "\\ff"}, {7, "\\fff"}, {8, "\\<"}, {9, "\\>"}, {10, "\\!"}};
	std::string words = "";
	// std::map with instrument index as key and another std::map as value
	// value std::map has the bar number as key, and the Lilypond std::string as value
	std::map<int, std::map<int, std::pair<bool, std::string>>> notes;
	while (getline(file, line)) {
		std::string lineWithoutTabs = replaceCharInStr(line, "\t", "");
		std::string lineWithoutSpaces = replaceCharInStr(lineWithoutTabs, " ", "");
		//std::cout << lineNum << endl;
		if (startsWith(lineWithoutSpaces, "<partid")) {
			for (auto it = insts.begin(); it != insts.end(); ++it) {
				if (it->second.second.first.compare(lineWithoutSpaces.substr(9, lineWithoutSpaces.substr(9).find_last_of("\""))) == 0) {
					// if this insturment is groupped, this is the index of the first instrument of the group
					// the incrementing indexes are found further down when we get a "<staff>" line
					instNdx = it->first;
					// we determine if this instrument is groupped by querying the first value of its pair
					// which is the value to the instrument std::map
					// this value is the number of staves of this instrument
					if (it->second.first > 1) isGroupped = true;
					else isGroupped = false;
					break;
				}
			}
		}
		else if (startsWith(lineWithoutSpaces, "<measurenumber")) {
			instNdxOffset = 0;
			barNumNdx = lineWithoutSpaces.substr(16).find("\"");
			barNum = stoi(lineWithoutSpaces.substr(16, barNumNdx));
			if (notes.find(instNdx) == notes.end()) {
				std::map<int, std::pair<bool, std::string>> m;
				notes[instNdx] = m;
			}
			if (notes[instNdx].find(barNum) == notes[instNdx].end()) {
				notes[instNdx][barNum] = std::make_pair(false, "");
			}
			// falsify isChord here because a dynamics symbol might be present before the first note
			// and if isChord is true from the previous bar, the character will be inserted instead of appended, causing a crash
			isChord = false;
			foundNote = false;
		}
		else if (startsWith(lineWithoutSpaces, "</measure>")) {
			if (tupletStops) {
				notes[instNdx+instNdxOffset][barNum].second += " }";
				tupletStops = false;
				areWeInsideTuplet = false;
			}
		}
		else if (startsWith(lineWithoutSpaces, "</backup>")) {
			// <backup> appears when a staff of a group is done, and we move on to the next staff
			if (tupletStops) {
				notes[instNdx+instNdxOffset][barNum].second += " }";
				tupletStops = false;
				areWeInsideTuplet = false;
			}
			instNdxOffset++;
			if (notes.find(instNdx+instNdxOffset) == notes.end()) {
				std::map<int, std::pair<bool, std::string>> m;
				notes[instNdx+instNdxOffset] = m;
			}
			if (notes[instNdx+instNdxOffset].find(barNum) == notes[instNdx+instNdxOffset].end()) {
				notes[instNdx+instNdxOffset][barNum] = std::make_pair(false, "");
			}
			isChord = false;
			foundNote = false;
		}
		else if (startsWith(lineWithoutSpaces, "<note")) {
			foundNote = true;
			isRest = false;
		}
		else if (startsWith(lineWithoutSpaces, "<chord")) {
			foundChordSym = true;
		}
		else if (startsWith(lineWithoutSpaces, "<pitch>")) {
			if (foundChordSym) {
				if (!isChord) firstChordNote = true;
				else firstChordNote = false;
				isChord = true;
				foundChordSym = false;
			}
			else {
				isChord = false;
			}
			if (isChord && firstChordNote) {
				// first the last white space, and insert an opening angle bracket after it
				size_t chordInsertNdx = notes[instNdx+instNdxOffset][barNum].second.find_last_of(" ");
				//if (areWeInsideTuplet) chordInsertNdx = notes[instNdx+instNdxOffset][barNum].second.substr(0, chordInsertNdx-1).find_last_of(" ");
				// if we are inside a tuplet, there's an extra white space, before the closing curly bracket
				//if (areWeInsideTuplet) chordInsertNdx = notes[instNdx+instNdxOffset][barNum].second.substr(0, chordInsertNdx).find_last_of(" ");
				notes[instNdx+instNdxOffset][barNum].second.insert(chordInsertNdx+1, "<");
				// then find the index of the last octave symbol
				chordInsertNdx = notes[instNdx+instNdxOffset][barNum].second.find_last_of("'");
				if (chordInsertNdx == std::string::npos || chordInsertNdx < findChordStart(notes[instNdx+instNdxOffset][barNum].second)) {
					chordInsertNdx = notes[instNdx+instNdxOffset][barNum].second.find_last_of(",");
					// if there are no octave symbols, then the closing angle bracket goes right after the note character
					if (chordInsertNdx == std::string::npos || chordInsertNdx < findChordStart(notes[instNdx+instNdxOffset][barNum].second)) {
						// we search for the last white space and add 3, one for the white space, one for the opening angle bracket
						// and one for the note character
						chordInsertNdx = notes[instNdx+instNdxOffset][barNum].second.find_last_of(" ") + 3;
						if (notes[instNdx+instNdxOffset][barNum].second.size() >= chordInsertNdx + 2) {
							// if there is an accidental concatenated to the note
							if (notes[instNdx+instNdxOffset][barNum].second.substr(chordInsertNdx, 2) == "es" || \
									notes[instNdx+instNdxOffset][barNum].second.substr(chordInsertNdx, 2) == "is" || \
									notes[instNdx+instNdxOffset][barNum].second.substr(chordInsertNdx, 2) == "eh" || \
									notes[instNdx+instNdxOffset][barNum].second.substr(chordInsertNdx, 2) == "ih") {
								chordInsertNdx += 2;
								if (notes[instNdx+instNdxOffset][barNum].second.size() >= chordInsertNdx + 2) {
									// if there is an additional accidental concatenated to the note
									if (notes[instNdx+instNdxOffset][barNum].second.substr(chordInsertNdx, 2) == "es" || \
											notes[instNdx+instNdxOffset][barNum].second.substr(chordInsertNdx, 2) == "is" || \
											notes[instNdx+instNdxOffset][barNum].second.substr(chordInsertNdx, 2) == "eh" || \
											notes[instNdx+instNdxOffset][barNum].second.substr(chordInsertNdx, 2) == "ih") {
										chordInsertNdx += 2;
									}
								}
							}
						}
					}
					else {
						chordInsertNdx += 1;
					}
				}
				else {
					chordInsertNdx += 1;
				}
				notes[instNdx+instNdxOffset][barNum].second.insert(chordInsertNdx, ">");
			}
		}
		else if (startsWith(lineWithoutSpaces, "</note>")) {
			if (mustAddDynamicToNextNote) {
				if (!isChord) notes[instNdx+instNdxOffset][barNum].second += dynamicNdxMap[dynamicNdx];
				mustAddDynamicToNextNote = false;
			}
			if (addedText) addedText = false;
		}
		else if (startsWith(lineWithoutSpaces, "<step>")) {
			// got idea for converting the case from upper to lower from
			// https://www.geeksforgeeks.org/case-conversion-lower-upper-vice-versa-std::string-using-bitwise-operators-cc/
			const char c = (lineWithoutSpaces.substr(6, 1)[0] | 32);
			if (!isChord) {
				if (tupletStops) {
					notes[instNdx+instNdxOffset][barNum].second += " }";
					tupletStops = false;
					areWeInsideTuplet = false;
				}
				notes[instNdx+instNdxOffset][barNum].second += " ";
				notes[instNdx+instNdxOffset][barNum].second += c;
			}
			else {
				size_t noteInsertNdx = findChordEnd(notes[instNdx+instNdxOffset][barNum].second);
				notes[instNdx+instNdxOffset][barNum].second.insert(noteInsertNdx, " ");
				noteInsertNdx++;
				notes[instNdx+instNdxOffset][barNum].second = notes[instNdx+instNdxOffset][barNum].second.substr(0, noteInsertNdx) + c + notes[instNdx+instNdxOffset][barNum].second.substr(noteInsertNdx);
			}
		}
		else if (startsWith(lineWithoutSpaces, "<alter>")) {
			alter = stoi(lineWithoutSpaces.substr(7, lineWithoutSpaces.substr(7).find("<")));
			size_t alterInsertNdx;
			switch (alter) {
				case -1:
					if (!isChord) {
						notes[instNdx+instNdxOffset][barNum].second += "es";
					}
					else {
						alterInsertNdx = findChordEnd(notes[instNdx+instNdxOffset][barNum].second);
						notes[instNdx+instNdxOffset][barNum].second.insert(alterInsertNdx, "es");
					}
					break;
				case 1:
					if (!isChord) {
						notes[instNdx+instNdxOffset][barNum].second += "is";
					}
					else {
						alterInsertNdx = findChordEnd(notes[instNdx+instNdxOffset][barNum].second);
						notes[instNdx+instNdxOffset][barNum].second.insert(alterInsertNdx, "is");
					}
					break;
				default:
					break;
			}
			// reset alter for safety
			alter = 0;
		}
		// time modification concerns tuplets
		else if (startsWith(lineWithoutSpaces, "<time-modification>")) {
			timeModificationFollows = true;
		}
		else if (startsWith(lineWithoutSpaces, "</time-modification>")) {
			timeModificationFollows = false;
		}
		else if (startsWith(lineWithoutSpaces, "<actual-notes>") && timeModificationFollows && !areWeInsideTuplet) {
			tupletNumerator = stoi(lineWithoutSpaces.substr(14, lineWithoutSpaces.substr(14).find("<")));
		}
		else if (startsWith(lineWithoutSpaces, "<normal-notes>") && timeModificationFollows && !areWeInsideTuplet) {
			tupletDenominator = stoi(lineWithoutSpaces.substr(14, lineWithoutSpaces.substr(14).find("<")));
		}
		else if (startsWith(lineWithoutSpaces, "<tuplet")) {
			tupletNdx = lineWithoutSpaces.find("type");
			if (lineWithoutSpaces.substr(tupletNdx+6, lineWithoutSpaces.substr(tupletNdx+6).find("\"")).compare("start") == 0 && !areWeInsideTuplet) {
				// first the last white space, and insert an opening angle bracket after it
				tupletNdx = notes[instNdx+instNdxOffset][barNum].second.find_last_of(" ");
				notes[instNdx+instNdxOffset][barNum].second.insert(tupletNdx+1, "\\tuplet "+std::to_string(tupletNumerator)+(std::string)"/"+std::to_string(tupletDenominator)+(std::string)" { ");
				areWeInsideTuplet = true;
			}
			else if (lineWithoutSpaces.substr(tupletNdx+6, lineWithoutSpaces.substr(tupletNdx+6).find("\"")).compare("stop") == 0) {
				tupletStops = true;
			}
		}
		else if (startsWith(lineWithoutSpaces, "<octave>")) {
			octave = stoi(lineWithoutSpaces.substr(8, lineWithoutSpaces.find_last_of("<")-8));
			if (octave > 3) {
				for (i = 3; i < octave; i++) {
					if (!isChord) {
						notes[instNdx+instNdxOffset][barNum].second += "'";
					}
					else {
						size_t octaveInsertNdx = findChordEnd(notes[instNdx+instNdxOffset][barNum].second);
						notes[instNdx+instNdxOffset][barNum].second.insert(octaveInsertNdx, "'");
					}
				}
			}
			else if (octave < 3) {
				for (i = 3; i > octave; i--) {
					if (!isChord) {
						notes[instNdx+instNdxOffset][barNum].second += ",";
					}
					else {
						size_t octaveInsertNdx = findChordEnd(notes[instNdx+instNdxOffset][barNum].second);
						notes[instNdx+instNdxOffset][barNum].second.insert(octaveInsertNdx, ",");
					}
				}
			}
		}
		else if (startsWith(lineWithoutSpaces, "<rest")) {
			if (tupletStops) {
				notes[instNdx+instNdxOffset][barNum].second += " }";
				tupletStops = false;
				areWeInsideTuplet = false;
			}
			notes[instNdx+instNdxOffset][barNum].second += " r";
			isRest = true;
			if (isChord) isChord = false;
		}
		else if (startsWith(lineWithoutSpaces, "<type>")) {
			for (i = 0; i < 7; i++) {
				if (lineWithoutSpaces.substr(6, lineWithoutSpaces.find_last_of("<")-6).compare(xmlDurs[i]) == 0) {
					if (!isChord) {
						notes[instNdx+instNdxOffset][barNum].second += std::to_string((int)pow(2, i));
						lastDurNdx = notes[instNdx+instNdxOffset][barNum].second.size();
					}
					break;
				}
			}
			if (foundWords) {
				notes[instNdx+instNdxOffset][barNum].second += words;
				words = "";
				foundWords = false;
				addedText = true;
			}
		}
		//else if (startsWith(lineWithoutSpaces, "<directionplacement")) {
		//	if (lineWithoutSpaces.substr(21, lineWithoutSpaces.substr(21).find("\"")).compare("below") == 0) {
		//		direction = -1;
		//	}
		//	else {
		//		direction = 1;
		//	}
		//}
		//else if (startsWith(lineWithoutSpaces, "<words")) {
		//	// we don't care to check if it's a chord or note, because the text comes right after the first chord note
		//	// and when we find a chord, we insert the necessary characters to the correct positions
		//	if (direction > 0) words += "^";
		//	else words += "_";
		//	words += "\"";
		//	// here we use line and not lineWithoutSpaces, because we want the spaces to be included in the text
		//	closingAngleBracketNdx = line.find(">");
		//	openingAngleBracketNdx = line.substr(closingAngleBracketNdx).find("<");
		//	if (openingAngleBracketNdx == std::string::npos) openingAngleBracketNdx = line.size() - closingAngleBracketNdx;
		//	else openingAngleBracketNdx--;
		//	words += line.substr(closingAngleBracketNdx+1, openingAngleBracketNdx);
		//	words += "\"";
		//	foundWords = true;
		//}
		else if (startsWith(lineWithoutSpaces, "<dot")) {
			if (!isChord) {
				// if there is text in the current note, the dot will go after the text if we just concatenate
				// so, for safety, we insert the dot right after the duration
				if (addedText) {
					size_t dotInsertNdx = lastDurNdx;
					if (dotInsertNdx == notes[instNdx+instNdxOffset][barNum].second.size()-1) {
						notes[instNdx+instNdxOffset][barNum].second += ".";
					}
					else {
						notes[instNdx+instNdxOffset][barNum].second.insert(dotInsertNdx, ".");
					}
				}
				else {
					notes[instNdx+instNdxOffset][barNum].second += ".";
				}
			}
		}
		else if (startsWith(lineWithoutSpaces, "<tiedtype")) {
			if (lineWithoutSpaces.find("start") != std::string::npos) {
				if (!isChord) notes[instNdx+instNdxOffset][barNum].second += "~";
				notes[instNdx+instNdxOffset][barNum].first = true;
			}
		}
		else if (foundDynamic) {
			if (!foundNote) {
				dynamicNdx = dynamicSymMap[lineWithoutSpaces.substr(1, lineWithoutSpaces.substr(1).find("/"))];
				mustAddDynamicToNextNote = true;
			}
			else {
				if (!isChord) {
					notes[instNdx+instNdxOffset][barNum].second += ("\\" + lineWithoutSpaces.substr(1, lineWithoutSpaces.substr(1).find("/")));
				}
				else  {
					size_t dynInsertNdx = findChordEnd(notes[instNdx+instNdxOffset][barNum].second);
					notes[instNdx+instNdxOffset][barNum].second.insert(dynInsertNdx, ("\\" + lineWithoutSpaces.substr(1, lineWithoutSpaces.substr(1).find("/"))));
				}
			}
			foundDynamic = false;
		}
		// <dymanics come before the actual dynamic which is parsed in the else if above
		// by assigning true to foundDynamic after the parsing chunk, we ensure that this will work properly
		else if (startsWith(lineWithoutSpaces, "<dynamics")) {
			foundDynamic = true;
		}
		else if (startsWith(lineWithoutSpaces, "<wedge")) {
			wedgeNdx = lineWithoutSpaces.find("type");
			if (!foundNote) {
				dynamicNdx = dynamicSymMap[lineWithoutSpaces.substr(wedgeNdx+6, lineWithoutSpaces.substr(wedgeNdx+6).find("\""))];
				mustAddDynamicToNextNote = true;
			}
			else {
				if (lineWithoutSpaces.substr(wedgeNdx+6, lineWithoutSpaces.substr(wedgeNdx+6).find("\"")).compare("crescendo") == 0) {
					if (!isChord) notes[instNdx+instNdxOffset][barNum].second += "\\<";
				}
				else if (lineWithoutSpaces.substr(wedgeNdx+6, lineWithoutSpaces.substr(wedgeNdx+6).find("\"")).compare("diminuendo") == 0) {
					if (!isChord) notes[instNdx+instNdxOffset][barNum].second += "\\>";
				}
				else if (lineWithoutSpaces.substr(wedgeNdx+6, lineWithoutSpaces.substr(wedgeNdx+6).find("\"")).compare("stop") == 0) {
					if (!isChord) {
						notes[instNdx+instNdxOffset][barNum].second += "\\!";
							notes[instNdx+instNdxOffset][barNum].first = true; // this is a linked bar
					}
				}
			}
		}
		else if (startsWith(lineWithoutSpaces, "</articulations")) {
			foundArticulation = false;
		}
		else if (foundArticulation) {
			if (articulations.find(lineWithoutSpaces.substr(1, lineWithoutSpaces.substr(1).find("/"))) != articulations.end()) {
				if (!isChord) {
					notes[instNdx+instNdxOffset][barNum].second += "-";
					notes[instNdx+instNdxOffset][barNum].second += articulations[lineWithoutSpaces.substr(1, lineWithoutSpaces.substr(1).find("/"))];
				}
			}
		}
		else if (startsWith(lineWithoutSpaces, "<articulations")) {
			if (!isRest) foundArticulation = true;
		}
		lineNum++;
	}
	// in case we have a tuplet at the very end of the score
	if (tupletStops) {
		notes[instNdx+instNdxOffset][barNum].second += " }";
	}
	file.close();
	// check if we have a zero tempo, which might be the case with a first, incomplete bar
	for (auto it = tempi.begin(); it != tempi.end(); ++it) {
		if (it->second.first == 0 || it->second.second == 0) {
			std::cout << "got zero in tempo " << it->second.first << " " << it->second.second << std::endl;
			if (it->second.first == 0) {
				int nonZeroVal = 0;
				std::map<int, std::pair<int, int>>::iterator nonZeroIt = std::next(it);
				while (nonZeroVal == 0) {
					if (nonZeroIt == tempi.end() && it != tempi.begin()) {
						nonZeroIt = std::prev(it);
					}
					else {
						// notify user of failure
						return;
					}
					if (nonZeroIt->first != 0) {
						nonZeroVal = nonZeroIt->first;
						it->second.first = nonZeroVal;
					}
				}
			}
			if (it->second.second == 0) {
				int nonZeroVal = 0;
				std::map<int, std::pair<int, int>>::iterator nonZeroIt = std::next(it);
				while (nonZeroVal == 0) {
					if (nonZeroIt == tempi.end() && it != tempi.begin()) {
						nonZeroIt = std::prev(it);
					}
					else {
						// notify user of failure
						return;
					}
					if (nonZeroIt->first != 0) {
						nonZeroVal = nonZeroIt->first;
						it->second.second = nonZeroVal;
					}
				}
			}
		}
	}
	// incomplete bars might not contain a time signature at all
	// in this case, we check if the notes map is beigger than the tempi map
	// and it it is, we create a new entry with the first key of the notes map
	// and the values of the first tempi item
	if (notes[0].size() > tempi.size()) {
		auto notesIt = notes[0].begin();
		auto tempiIt = tempi.begin();
		tempi[notesIt->first] = std::make_pair(tempiIt->second.first, tempiIt->second.second);
	}
	// now write the notes in LiveLily notation
	createNewLine("", 0);
	cursorLineIndex++;
	std::map<int, std::string> clefSymbols {{0, "treble"}, {1, "bass"}, {2, "alto"}};
	// all parts have the same number of bars, so we query the first one
	// to determine how many bars we have to write
	for (auto it = notes[0].begin(); it != notes[0].end(); ++it) {
		std::string barOpening = "\\bar " + std::to_string(it->first) + " {";
		createNewLine(barOpening, 0);
		cursorLineIndex++;
		std::string time = tabStr + "\\time " + std::to_string(times[it->first].first) + "/" + std::to_string(times[it->first].second);
		createNewLine(time, 0);
		cursorLineIndex++;
		std::string tempoStr = tabStr + "\\tempo " + std::to_string(tempi[it->first].first) + " = " + std::to_string(tempi[it->first].second);
		createNewLine(tempoStr, 0);
		cursorLineIndex++;
		std::string beatAtStr = tabStr + "\\beatat " + std::to_string(tempi[it->first].first);
		createNewLine(beatAtStr, 0);
		cursorLineIndex++;
		for (auto it2 = insts.begin(); it2 != insts.end(); ++it2) {
			std::string instLine = tabStr + "\\" + it2->second.second.second;
			if (instClefs[it2->first][it->first].first) {
				instLine += " \\clef ";
				instLine += clefSymbols[instClefs[it2->first][it->first].second];
			}
			else {
				instLine += " \\clef ";
				instLine += clefSymbols[prevInstClef[it2->first]];
			}
			instLine += notes[it2->first][it->first].second;
			createNewLine(instLine, 0);
			cursorLineIndex++;
		}
		createNewLine("}", 0);
		cursorLineIndex++;
		createNewLine("", 0);
		cursorLineIndex++;
	}
}

////////////////////// loading MIDI files ///////////////////////

//--------------------------------------------------------------
//std::pair<int, int> Editor::getMidiDur(long ticks, int ppqn)
//{
//	float floatDur = (float)ticks / (float)ppqn;
//	floatDur *= 16; // 64 / 4 (64th is the shortest note and PPQN is Pulses Per Quarter Note, therefore 4)
//	int intDur = (int)(floatDur + 0.5);
//	int duration = 4;
//	int numDots = 0;
//	int powCoef = 0;
//	bool found = false;
//	while (!found) {
//		if (pow(2, powCoef) > intDur) {
//			int thisDur = pow(2, powCoef-1);
//			int remaining = intDur - thisDur;
//			float div = (float)remaining / (float)thisDur;
//			while (true) {
//				div *= 2;
//				int intDiv = (int)div;
//				numDots++;
//				if (div - (float)intDiv == 0) break;
//				else div -= intDiv;
//			}
//			found = true;
//			duration = 64 / thisDur;
//		}
//		powCoef++;
//	}
//	return std::make_pair(duration, numDots);
//}
//
////--------------------------------------------------------------
//long Editor::roundMidiTicks(long ticks, int ppqn)
//{
//	long base = (long)ppqn / 16; // for a 64th note
//	// array of mutliplication coefficients to include dotted notes too
//	long mutlCoefs[13] = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96};
//	for (int i = 0; i < 13; i++) {
//		long compare = base * multCoefs[i];
//		if (compare - ticks > 0) {
//			return compare;
//		}
//	}
//	return base;
//}
//
////--------------------------------------------------------------
//std::string Editor::getMidiPitch(int pitch, std::string *notes)
//{
//	std::string midiPitchStr = notes[pitch % 12];
//	int octaves = pitch - 48;
//	std::string octaveStr;
//	if (octaves >= 0) octaveStr = "'";
//	else octaveStr = ",";
//	octaves /= 12;
//	if (octaves == 0 && octaveStr == ",") midiPitchStr += ",";
//	for (int i = 0; i < (abs)octaves; i++) midiPitchStr += octaveStr;
//	return midiPitchStr;
//}
//
////--------------------------------------------------------------
//void Editor::loadMIDIFile(std::string filePath)
//{
//	MIDIFileLoader readMidi;
//	readMidi.loadFile(filePath);
//	int tempo = readMidi.getTempo();
//	int ppqn = readMidi.getPPQM();
//	std::vector<std::vector<noteData>> midiEvents = readMidi.getMidiEvents();
//	bool hasNoteOffs = false;
//	for (unsigned i = 0; i < midiEvents.size(); i++) {
//		for (unsigned j = 0; j < midiEvents[i].size(); j++) {
//			if (midiEvents[i][j].velocity == 0) {
//				hasNoteOffs = true;
//				break;
//			}
//		}
//		if (hasNoteOffs) break;
//	}
//	std::vector<long> prevTicks (midiEvents.size(), 0);
//	std::vector<int> prevMidiEventsNdx (midiEvents.size(), 0);
//	std::vector<int> leftOversSixtyfours (midiEvents.size(), 0);
//	std::vector<std::string> leftOversString (midiEvents.size(), "");
//	std::string notes[12] = {"c", "cis", "d", "dis", "e", "f", "fis", "g", "gis", "a", "bes", "b"};
//
//	createNewLine("\\score show", 0);
//	cursorLineIndex++;
//	createNewLine("\\score animate", 0);
//	cursorLineIndex++;
//	createNewLine("", 0);
//	cursorLineIndex++;
//	std::string insts = "\\insts";
//	for (auto it = readMidi.instNames.begin(); it != readMidi.instNames.end(); ++it) {
//		insts += " ";
//		insts += *it;
//	}
//	createNewLine(insts, 0);
//	cursorLineIndex++;
//	createNewLine("", 0);
//	cursorLineIndex++;
//
//	std::vector<unsigned> timeSignaturesTimeStamps;
//	for (auto it = readMidi.timeSignatures.begin(); it != readMidi.timeSignatures.end(); ++it) {
//		timeSignaturesTimeStamps.push_back(it->first);
//	}
//
//	std::vector<unsigned> tempoTimeStamps;
//	for (auto it = readMidi.tempoMap.begin(); it != readMidi.tempoMap.end(); ++it) {
//		tempoTimeStamps.push_back(it->first);
//	}
//
//	int barCount = 1;
//	bool hasMoreData = true;
//	int dataDoneCounter = 0;
//	long totalPulses = 0;
//	int numerator = 4, denominator = 4;
//	int tempo = 120;
//	while (hasMoreData) {
//		createNewLine("\\bar " + std::to_string(barCount++) + " {", 0);
//		cursorLineIndex++;
//		if (timeSignaturesTimeStamps.size() > 0) {
//			if (*timeSignaturesTimeStamps.begin() == totalPulses) {
//				numerator = readMidi.timeSignatures[totalPulses].first;
//				denominator = readMidi.timeSignatures[totalPulses].second;
//				timeSignaturesTimeStamps.erase(timeSignaturesTimeStamps.begin());
//			}
//		}
//		if (tempoTimeStamps.size() > 0) {
//			if (*tempoTimeStamps.begin() == totalPulses) {
//				tempo = readMidi.tempoMap[totalPulses];
//				tempoTimeStamps.erase(tempoTimeStamps.begin());
//			}
//		}
//		long pulsesPerBar = numerator * (ppqn / (denominator / 4));
//		int numSixtyfoursPerBar = numerator * (64 / denominator);
//		std::string timeStampStr = tabStr + "\\time " + std::to_string(numerator) + "/" + std::to_string(denominator);
//		createNewLine(timeStampStr, 0);
//		cursorLineIndex++;
//		std::string tempoStr = tabStr + "\\tempo " + std::to_string(denominator) + " = " + std::to_string(tempo);
//		createNewLine(tempoStr, 0);
//		cursorLineIndex++;
//		for (unsigned i = 0; i < midiEvents.size(); i++) {
//			long barDur = 0;
//			int totalBarDur = numerator * (64 / denominator);
//			std::pair<int, int> duration;
//			bool barDone = false;
//			bool leftOverTakesWholeBar = false;
//			std::string line = tabStr + "\\" + readMidi.instNames[i] + " ";
//			std::string note;
//			for (unsigned j = prevMidiEventsNdx[i]; j < midiEvents[i].size(); j++) {
//				long ticks = midiEvents[i][j].ticks;
//				long roundedDurationTicks = roundMidiTicks(midiEvents[i][j].durationTicks, ppqn);
//				long nextTicks;
//				if (j < midiEvents[i].size() - 1) {
//					nextTicks = midiEvents[i][j+1].ticks;
//				}
//				else {
//					nextTicks = totalPulses + pulsesPerBar - midiEvents[i][j].ticks;
//				}
//				if (leftOvers[i] > 0) {
//					line = note;
//					if (leftOvers[i] > numSixtyfoursPerBar) {
//						duration = getMidiDur(pulsesPerBar, ppqn);
//						leftOversSixtyfours[i] = leftOversSixtyfours[i] - numSixtyfoursPerBar;
//						leftOversString[i] = note;
//						leftOverTakesWholeBar = true;
//					}
//					else {
//						duration = getMidiDur(leftOversSixtyfours[i], ppqn);
//						leftOversSixtyfours[i] = 0;
//					}
//					line += std::to_string(duration.first);
//					barDur += (64 / duration.first);
//					int halfDur = duration.first / 2;
//					for (int k = 0; k < duration.second; k++) {
//						line += ".";
//						barDur += (64 / halfDur);
//						halfDur /= 2;
//					}
//					if (leftOverTakesWholeBar) {
//						line += "~";
//					}
//					else {
//						line += " ";
//					}
//				}
//				if (leftOverTakesWholeBar) {
//					break;
//				}
//				else {
//					if (nextTicks - ticks > roundedDurationTicks) {
//						std::pair<int, int> restDur = getMidiDur(nextTicks - ticks - roundedDurationTicks, ppqn);
//						line += ("r" + std::to_string(restDur.first));
//						barDur += (64 / restDur.first);
//						int halfDur = restDur.first / 2;
//						for (int k = 0; k < restDur.second; k++) {
//							line += ".";
//							barDur += (64 / halfDur);
//							halfDur /= 2;
//						}
//						note = "r";
//						if (nextTicks - ticks < pulsesPerBar) {
//							line += " ";
//						}
//					}
//					if (nextTicks - ticks <= pulsesPerBar) {
//						note += getMidiPitch(midiEvents[i][j].pitch, notes);
//						line += note;
//						duration = getMidiDur(roundedDurationTicks, ppqn);
//						line += std::to_string(duration.first);
//						barDur += (64 / duration.first);
//						int halfDur = duration.first / 2;
//						for (int k = 0; k < duration.second; k++) {
//							line += ".";
//							barDur += (64 / halfDur);
//							halfDur /= 2;
//						}
//						if (barDur >= numSixtyfoursPerBar) {
//							if (barDur > numSixtyfoursPerBar) {
//								leftOvers[i] = barDur - numSixtyfoursPerBar;
//								line += "~";
//							}
//							prevMidiEventsNdx[i] = j;
//							barDone = true;
//						}
//						else {
//							line += " ";
//						}
//					}
//				}
//				if (barDone) {
//					leftOversString[i] = note;
//					break;
//				}
//			}
//			createNewLine(line, 0);
//			cursorLineIndex++;
//		}
//		totalPulses += pulsesPerBar;
//	}
//}

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
	ofFileDialogResult openFileResult = ofSystemLoadDialog("Select a .lyv, .xml, .py or .lua file");
	// Check if the user opened a file
	if (openFileResult.bSuccess){
		loadFile(openFileResult.filePath);
	}
}

/////////////////// main load file function ////////////////////

//--------------------------------------------------------------
void Editor::loadFile(std::string fileName)
{
	lineCount = 1;
	cursorPos = 0;
	if (!endsWith(fileName, ".lyv") && !endsWith(fileName, ".xml") && !endsWith(fileName, ".musicxml") && \
			!endsWith(fileName, ".py") && !endsWith(fileName, ".lua") && !endsWith(fileName, ".mid")) {
		fileName += ".lyv";
	}
	//if (file.is_open()) {
	//	file.close();
	//}
	std::ifstream file(fileName.c_str());
	if (!file.is_open()) {
		// if for some reason the file can't be opened
		// instead of the file name, the string "could not open file"
		// will be displayed for 2 seconds and then it will return to untitled.lyv
		couldNotLoadFile = true;
		loadedFileStr = "could not load file";
		fileLoadErrorTimeStamp = ofGetElapsedTimeMillis();
		std::cout << "can't open file\n";
		return;
	}
	file.close();
	clearText();
	// determine what type of file we're opeining
	size_t lastBackSlash = fileName.find_last_of("/");
	loadedFileStr = fileName.substr(lastBackSlash+1);
	loadedFileFullPath = fileName;
	bool openingXML = false, openingMIDI = false;
	if (endsWith(loadedFileStr, ".mxl") || endsWith(loadedFileStr, ".xml") || endsWith(loadedFileStr, ".musicxml")) {
		thisLang = 0;
		openingXML = true;
	}
	if (endsWith(loadedFileStr, ".lyv")) {
		thisLang = 0;
	}
	else if (endsWith(loadedFileStr, ".py")) {
		((ofApp*)ofGetAppPtr())->initPyo();
		thisLang = 1;
	}
	else if (endsWith(loadedFileStr, ".lua")) {
		thisLang = 2;
	}
	else if (endsWith(loadedFileStr, ".mid")) {
		openingMIDI = true;
	}
	else {
		thisLang = 0;
	}
	if (openingXML) {
		loadXMLFile(fileName);
	}
	else if (openingMIDI) {
		// for now LiveLily doesn't support MIDI files, but we're close
		couldNotLoadFile = true;
		fileName = "MIDI files are not yet supported";
		fileLoadErrorTimeStamp = ofGetElapsedTimeMillis();
		file.close();
		return;
	}
	else {
		loadTextFile(fileName);
	}
	lineCount = (int)allStrings.size();
	cursorLineIndex = 0;
	cursorPos = arrowCursorPos = 0;
	fileLoaded = true;
	fileEdited = false;
}

//--------------------------------------------------------------
//bool Editor::isFileOpen()
//{
//	return file.is_open();
//}

//--------------------------------------------------------------
//void Editor::closeFile()
//{
//	file.close();
//}

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
