#include <iostream>
#include <fstream>
#include <iterator> // to be able to use find() with a reverse iterator, see findBraceBackward()
#include <algorithm>
#include <limits>
#include "editor.h"
//#include "readmidi.h"
#include "ofApp.h"

//--------------------------------------------------------------
Editor::Editor()
{
	thisLang = 0; // 0 is livelily, 1 is python, and 2 is lua
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

	inserting = true;
	typingCommand = false;
	showCommand = false;

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
string Editor::getLine(int lineNdx)
{
	map<int, string>::iterator it = allStrings.find(lineNdx);
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
int Editor::getNumTabsInStr(string str)
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
vector<int> Editor::getNestDepth()
{
	map<int, string>::iterator it;
	vector<int> v(2, 0); // bottom line, nest depth
	for (it = allStrings.find(cursorLineIndex); it != allStrings.end(); ++it) {
		if (it->second.substr(0, TABSIZE).compare(tabStr) == 0) {
			v[0] = it->first;
			v[1] = max(v[1], getNumTabsInStr(it->second));
		}
		else {
			if (it->first == cursorLineIndex && it->second.find("{") != string::npos) continue;
			else if (it->second.find("}") != string::npos) {
				v[0] = it->first;
				if (it->first == cursorLineIndex) {
					--it;
					while (it->second.substr(0, TABSIZE).compare(tabStr) == 0) {
						v[1] = max(v[1], getNumTabsInStr(it->second));
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
	map<int, string>::iterator it;
	for (it = allStrings.find(startLine); it != allStrings.begin(); --it) {
		if (it->second.substr(0, TABSIZE).compare(tabStr) != 0) {
			if (it->second.find("}") != string::npos) continue;
			else return it->first;
		}
	}
	return 0;
}

//--------------------------------------------------------------
vector<int> Editor::findBraceForward(map<int, string>::iterator start, map<int, string>::iterator finish)
{
	vector<int> v(2, -1);
	if (start == allStrings.end() || finish == allStrings.end()) {
		return v;
	}
	unsigned i, lineStart;
	map<int, string>::iterator it;
	// Declare a stack to hold the previous brackets.
	stack<char> temp;
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
vector<int> Editor::findBraceBackward()
{
	vector<int> v(2, -1);
	if (allStrings.find(cursorLineIndex) == allStrings.end()) {
		return v;
	}
	unsigned i, start;
	map<int, string>::reverse_iterator it = allStrings.rbegin();
	size_t step = allStrings.size() - (size_t)(cursorLineIndex + 1);
	if (step > 0) std::advance(it, step);
	// Declare a stack to hold the previous brackets.
	stack<char> temp;
	while (it != allStrings.rend()) {
		start = (unsigned)min((int)it->second.length()-1, cursorPos);
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
				ofSetColor(ofColor::orange.r*((ofApp*)ofGetAppPtr())->brightnessCoeff,
						ofColor::orange.g*((ofApp*)ofGetAppPtr())->brightnessCoeff,
						ofColor::orange.b*((ofApp*)ofGetAppPtr())->brightnessCoeff);
			}
			else if (tracebackColor[i] == 2) {
				ofSetColor(ofColor::red.r*((ofApp*)ofGetAppPtr())->brightnessCoeff,
						ofColor::red.g*((ofApp*)ofGetAppPtr())->brightnessCoeff,
						ofColor::red.b*((ofApp*)ofGetAppPtr())->brightnessCoeff);
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
					ofColor::cyan.b*((ofApp*)ofGetAppPtr())->brightnessCoeff, executionDegrade[i]);
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

	/******************************************************************/
	// then draw the rectangle that highlights a bracket
	highlightBracket = false;
	if (activity) {
		if (cursorPos < (int)allStrings[cursorLineIndex].size() && cursorPos <= maxCursorPos()) {
			// if matching brace is found, this vector will be the X and Y coordinates
			// and nest depth (only from findBraceForward() but findBraceBackward() returns a 3 element vector
			// for code integrity), which is used elsewhere
			vector<int> pos (3, -1);
			char bracketChars[7] = {'{', '}', '[', ']', '(', ')'}; // extra array item for null char
			//int direction = 0; // initialized to 0 to avoid warning messages during compilation
			char charOnTopOfCursor = allStrings[cursorLineIndex][cursorPos+allStringStartPos[cursorLineIndex]];
			for (int i = 0; i < 6; i++) {
				if (bracketChars[i] == charOnTopOfCursor) {
					//direction = i % 2;
					highlightBracket = true;
					break;
				}
			}
			//if (highlightBracket) {
			//	if (direction) pos = findBraceBackward();
			//	else pos = findBraceForward(allStrings.find(cursorLineIndex), allStrings.end(), true);
			//}
			// if the matching pair has been found, we'll get positions that are greater than -1 for both X and Y
			if (pos[0] > -1){
				ofSetColor(ofColor::magenta.r * ((ofApp*)ofGetAppPtr())->brightnessCoeff,
						ofColor::magenta.g * ((ofApp*)ofGetAppPtr())->brightnessCoeff,
						ofColor::magenta.b * ((ofApp*)ofGetAppPtr())->brightnessCoeff);
				int strWidth = font.stringWidth(allStrings[pos[1]].substr(0, pos[0] - allStringStartPos[pos[1]]));
				bracketHighlightRectX = lineNumberWidth + strWidth + oneAndHalfCharacterWidth + frameXOffset;
				bracketHighlightRectY = ((pos[1]-lineCountOffset) * cursorHeight) + frameYOffset; 
				ofDrawRectangle(bracketHighlightRectX, bracketHighlightRectY, oneCharacterWidth, cursorHeight);	
				ofSetColor(((ofApp*)ofGetAppPtr())->brightness);
			}
		}
	}

	/*******************************************************************/
	// then the actual text
	string strOnCursorLine;
	int strOnCursorLineYOffset = frameYOffset;
	int strXOffset = lineNumberWidth + oneAndHalfCharacterWidth + frameXOffset;
	// the extra counter below starts from 0, whereas i starts from lineCountOffset
	count = 0;
	// we'll get the color of the text where the cursor is
	// for this reason, we need to calculate the X position of the cursor here, and not below
	// where we draw the cursor
	ofColor cursorColor = ((ofApp*)ofGetAppPtr())->brightness;
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
			vector<string> tokens = tokenizeString(allStrings[i], "{ }*.\"", true);
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
			// the main ofApp stores the following commands: \insts, \bar, \bars, \loop, \function, list, and group
			// we use this to determine if we should further tokenize each token of our string
			// if a line starts with one of these commands, then we won't tokenize the token further
			bool tokenizeDigits1 = find(((ofApp*)ofGetAppPtr())->commandsToNotTokenize.begin(), ((ofApp*)ofGetAppPtr())->commandsToNotTokenize.end(), tokens[0]) == ((ofApp*)ofGetAppPtr())->commandsToNotTokenize.end();
			for (auto it = tokens.begin(); it != tokens.end(); ++it) {
				// tokenizing has already been done, but to separate digits properly
				// we need to tokenize these further, as a string like "c''4" won't be tokenized properly
				vector<string> subtokens;
				// the main ofApp also stores the all bar, loop, function and list names to the keywords vector
				bool tokenizeDigits2 = find(((ofApp*)ofGetAppPtr())->keywords.begin(), ((ofApp*)ofGetAppPtr())->keywords.end(), *it) == ((ofApp*)ofGetAppPtr())->keywords.end();
				// if the token starts with a backslash, then we don't tokenize based on digits
				if (startsWith(*it, "\\")) tokenizeDigits2 = false;
				if (tokenizeDigits1 && tokenizeDigits2) {
					subtokens = tokenizeString(*it, "0123456789", true);
				}
				else {
					subtokens.push_back(*it);
				}
				for (auto subit = subtokens.begin(); subit != subtokens.end(); ++subit) {
					//cout << "cursorX: " << cursorX << ", cursorPos: " << cursorPos << ", xOffset: " << xOffset << ", str width of \"" << subtoken << "\": " << font.stringWidth(subtoken) << ", whole str \"" << allStrings[i] << "\": " << font.stringWidth(allStrings[i]) << endl;
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
						//	cout << "\"" << *subit << "\": cursorPos: " << cursorPos << ", subtokensSizeAccum: " << subtokensSizeAccum << ", subtoken size: " << (*subit).size() << endl;
						if (cursorPos - (int)subtokensSizeAccum + allStringStartPos[i] < (int)(*subit).size() || (cursorPos + allStringStartPos[i] == (int)allStrings[i].size() && endOfStr)) {
							isCursorInsideKeyword = true;
							//if (prevCursorPos != cursorPos)
							//	cout << "drawing cursor for \"" << *subit << "\"\n";
						}
					}
					subtokensSizeAccum += (*subit).size();
					if (tracebackStr[i].size() > 0 && tracebackColor[i] > 0) {
						ofSetColor(((ofApp*)ofGetAppPtr())->backgroundColor);
					}
					else if (startsWith(*subit, "%")) {
						ofSetColor(ofColor::gray.r*((ofApp*)ofGetAppPtr())->brightnessCoeff,
								ofColor::gray.g*((ofApp*)ofGetAppPtr())->brightnessCoeff,
								ofColor::gray.b*((ofApp*)ofGetAppPtr())->brightnessCoeff);
						isComment = true;
						if (isCursorInsideKeyword && !cursorDrawn) {
							cursorColor = ofColor::gray;
							drawCursor(cursorX, cursorColor);
							cursorDrawn = true;
						}
					}
					else if (*subit == "\"") {
						ofSetColor(ofColor::aqua.r*((ofApp*)ofGetAppPtr())->brightnessCoeff,
								ofColor::aqua.g*((ofApp*)ofGetAppPtr())->brightnessCoeff,
								ofColor::aqua.b*((ofApp*)ofGetAppPtr())->brightnessCoeff);
						if (quotesCounter == 0) {
							isString = true;
							quotesCounter++;
						}
						else {
							isString = false;
							quotesCounter = 0;
						}
						if (isCursorInsideKeyword && !cursorDrawn) {
							cursorColor = ofColor::aqua;
							drawCursor(cursorX, cursorColor);
							cursorDrawn = true;
						}
					}
					else {
						if (((ofApp*)ofGetAppPtr())->commandsMap[thisLang].find(*subit) != ((ofApp*)ofGetAppPtr())->commandsMap[thisLang].end() \
								&& !isComment && !isString) {
							ofColor color = ((ofApp*)ofGetAppPtr())->commandsMap[thisLang][*subit];
							ofSetColor(color.r*((ofApp*)ofGetAppPtr())->brightnessCoeff,
									color.g*((ofApp*)ofGetAppPtr())->brightnessCoeff,
									color.b*((ofApp*)ofGetAppPtr())->brightnessCoeff);
							if (isCursorInsideKeyword && !cursorDrawn) {
								cursorColor = color;
								drawCursor(cursorX, cursorColor);
								cursorDrawn = true;
							}
						}
						else if ((*subit).size() == 0 && !isComment && !isString) {
							ofSetColor(((ofApp*)ofGetAppPtr())->brightness);
						}
						else if (isNumber(*subit) && !isComment && !isString) {
							ofSetColor(ofColor::gold.r*((ofApp*)ofGetAppPtr())->brightnessCoeff,
									ofColor::gold.g*((ofApp*)ofGetAppPtr())->brightnessCoeff,
									ofColor::gold.b*((ofApp*)ofGetAppPtr())->brightnessCoeff);
							if (isCursorInsideKeyword && !cursorDrawn) {
								cursorColor = ofColor::gold;
								drawCursor(cursorX, cursorColor);
								cursorDrawn = true;
							}
						}
						else if (!isComment && !isString) {
							ofSetColor(((ofApp*)ofGetAppPtr())->brightness);
						}
						else if (isComment && (isCursorInsideKeyword && !cursorDrawn)) {
							cursorColor = ofColor::gray;
							drawCursor(cursorX, cursorColor);
							cursorDrawn = true;
						}
						else if (isString && (isCursorInsideKeyword && !cursorDrawn)) {
							cursorColor = ofColor::darkOrange;
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
					xOffset += ((*subit).size() * oneCharacterWidth); // + 1 for the white space
					charAccum += ((int)(*subit).size()); // + 1);
				}
			}
		}
		
		// then check if we have selected this string so we must highlight it
		if (highlightManyChars && ((i>=topLine) && (i<=bottomLine))) {
			vector<int> posAndSize = setSelectedStrStartPosAndSize(i);
			int boxXOffset = lineNumberWidth + oneAndHalfCharacterWidth + \
							 (posAndSize[0]*oneCharacterWidth) + frameXOffset;
			int boxYOffset = (count * cursorHeight) + frameYOffset;
			// the bounding box coordinates of the highlighted string don't need the offset
			// which is greater than 0 if the string is too long
			// but the substring that is drawn on top of this box does need it
			posAndSize[0] += allStringStartPos[i];
			string strInBlack = allStrings[i].substr(posAndSize[0], posAndSize[1]);
			int widthLocal = font.stringWidth(strInBlack);
			// draw the selecting rectangle
			ofSetColor(ofColor::goldenRod.r * ((ofApp*)ofGetAppPtr())->brightnessCoeff,
					ofColor::goldenRod.g * ((ofApp*)ofGetAppPtr())->brightnessCoeff,
					ofColor::goldenRod.b * ((ofApp*)ofGetAppPtr())->brightnessCoeff);
			ofDrawRectangle(boxXOffset, boxYOffset, widthLocal, cursorHeight);
			ofSetColor(0);
			// and then the selected string in black
			font.drawString(strInBlack, boxXOffset, strYOffset);
		}
		count++;
	}

	/*******************************************************************/
	// then draw the cursor
	if (!cursorDrawn) drawCursor(cursorX, ((ofApp*)ofGetAppPtr())->brightness);

	/*******************************************************************/
	// then draw the character the cursor is drawn on top of (if this is the case)
	if (activity) {
		if (cursorPos < (int)strOnCursorLine.size() && cursorPos <= maxCursorPos()) {
			string onTopOfCursorStr = strOnCursorLine.substr(cursorPos+allStringStartPos[cursorLineIndex], 1);
			ofSetColor(0);
			font.drawString(onTopOfCursorStr, cursorX, strOnCursorLineYOffset);
			ofSetColor(((ofApp*)ofGetAppPtr())->brightness);
		}
	}

	/*******************************************************************/
	// then draw the line numbers
	count = 0;
	ofSetColor(((ofApp*)ofGetAppPtr())->brightness);
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
		font.drawString(to_string(i+1), xOffset, ((count+1)*cursorHeight)+frameYOffset-characterOffset);
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
	ofSetColor(cursorColor.r * ((ofApp*)ofGetAppPtr())->brightnessCoeff,
			cursorColor.g * ((ofApp*)ofGetAppPtr())->brightnessCoeff,
			cursorColor.b * ((ofApp*)ofGetAppPtr())->brightnessCoeff);
	if ((activity && !typingCommand) || !activity) {
		ofDrawRectangle(cursorX, cursorY, oneCharacterWidth, cursorHeight);
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
	ofSetColor(((ofApp*)ofGetAppPtr())->brightness);
	float yCoord = maxNumLines * cursorHeight + frameYOffset;
	float strYCoord = ((maxNumLines+1)*cursorHeight) + frameYOffset - characterOffset;
	int fileNameXOffset = oneCharacterWidth + oneAndHalfCharacterWidth + frameXOffset;
	string fileName;
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
		ofSetColor(ofColor::red.r*((ofApp*)ofGetAppPtr())->brightness,
				   ofColor::red.g*((ofApp*)ofGetAppPtr())->brightness,
				   ofColor::red.b*((ofApp*)ofGetAppPtr())->brightness);
	}
	else {
		ofSetColor(((ofApp*)ofGetAppPtr())->backgroundColor);
		if (fileEdited) fileName += "*";
	}
	font.drawString(ofToString(objID+1), halfCharacterWidth+frameXOffset, strYCoord);
	font.drawString(fileName, fileNameXOffset, strYCoord);
	float widthOffset = 0;
	if (inserting) {
		if (((ofApp*)ofGetAppPtr())->sharedData.showScore) {
			if (frameWidth > ((ofApp*)ofGetAppPtr())->sharedData.middleOfScreenX) {
				widthOffset = ((ofApp*)ofGetAppPtr())->sharedData.middleOfScreenX;
			}
		}
		font.drawString("INSERT", frameXOffset+frameWidth-widthOffset-font.stringWidth("INSERT")-10, strYCoord);
	}
	ofSetColor(((ofApp*)ofGetAppPtr())->brightness);
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
	if (((ofApp*)ofGetAppPtr())->sharedData.showScore) {
		if (frameWidth > ((ofApp*)ofGetAppPtr())->sharedData.middleOfScreenX) {
			width = ((ofApp*)ofGetAppPtr())->sharedData.middleOfScreenX;
		}
	}
	maxCharactersPerString = (width-lineNumberWidth-oneAndHalfCharacterWidth) / oneCharacterWidth;
	maxCharactersPerString -= 1;
	// the line below concerns the backtrace length
	maxBacktraceChars = (size_t)(((ofApp*)ofGetAppPtr())->sharedData.screenWidth / oneCharacterWidth);
	//setStringsStartPos();
}

//--------------------------------------------------------------
void Editor::setGlobalFontSize(int fontSize)
{
	((ofApp*)ofGetAppPtr())->fontSize = fontSize;
	((ofApp*)ofGetAppPtr())->setFontSize();
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
void Editor::setFontSize(int fontSize, float cursorHeight)
{
	// store the previous the maximum characters per string
	int oldMaxCharactersPerString = maxCharactersPerString;
	Editor::fontSize = fontSize;
	font.load("DroidSansMono.ttf", fontSize);
	// we're using a monospace font, so we get the width of any character
	oneCharacterWidth = font.stringWidth("a");
	Editor::cursorHeight = cursorHeight; // font.stringHeight("q");
	halfCharacterWidth = oneCharacterWidth / 2;
	oneAndHalfCharacterWidth = oneCharacterWidth + halfCharacterWidth;
	// the variable below helps center the cursor at the Y axis with respect to the letters
	characterOffset = font.stringHeight("j") - font.stringHeight("l");
	characterOffset /= 2;
	setMaxCharactersPerString();
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
void Editor::createNewLine(string str, int increment)
{
	allStrings[cursorLineIndex+increment] = str;
	allStringStartPos[cursorLineIndex+increment] = 0;
	tracebackStr[cursorLineIndex+increment] = "";
	tracebackColor[cursorLineIndex+increment] = 0;
	tracebackNumLines[cursorLineIndex+increment] = 1;
	tracebackTimeStamps[cursorLineIndex+increment] = 0;
	executingLines[cursorLineIndex+increment] = false;
	executionDegrade[cursorLineIndex+increment] = 0;
	executionTimeStamp[cursorLineIndex+increment] = 0;
}

//--------------------------------------------------------------
void Editor::moveLineNumbers(int numLines)
{
	if (numLines < 0) {
		// move all strings below the line where the cursor is, one line above
		map<int, string>::iterator it = allStrings.begin();
		std::advance(it, cursorLineIndex);
		while (it != allStrings.end()) {
			int key = it->first;
			changeMapKeys(key, numLines);
			++it;
		}
	}
	else {
		// move all strings below the line where the cursor is, one line below
		map<int, string>::reverse_iterator it = allStrings.rbegin();
		while (it->first > cursorLineIndex) {
			int key = it->first;
			//cout << "key: " << key << endl;
			changeMapKeys(key, numLines);
			++it;
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
	executingLines[cursorLineIndex+1] = executingLines[cursorLineIndex];
	executionDegrade[cursorLineIndex+1] = executionDegrade[cursorLineIndex];
	executionTimeStamp[cursorLineIndex+1] = executionTimeStamp[cursorLineIndex];

	allStrings[cursorLineIndex] = "";
	allStringStartPos[cursorLineIndex] = 0;
	tracebackStr[cursorLineIndex] = "";
	tracebackColor[cursorLineIndex] = 0;
	tracebackNumLines[cursorLineIndex] = 1;
	tracebackTimeStamps[cursorLineIndex] = 0;
	executingLines[cursorLineIndex] = false;
	executionDegrade[cursorLineIndex] = 0;
	executionTimeStamp[cursorLineIndex] = 0;
}

//--------------------------------------------------------------
void Editor::copyOnLineDelete()
{
	// in case of backspace the cursorLineIndex variable has been updated before this function call
	// in case of delete, the variable doesn't change
	// in any case, it already points to the right key of the map below
	map<int, string>::iterator it1 = allStrings.find(cursorLineIndex);
	map<int, string>::iterator it2 = allStrings.find(cursorLineIndex+1);
	// first concatenate the two strings
	if (it2 != allStrings.end()) {
		it1->second += it2->second;
		// before moving the keys of the maps we must erase the keys of the line below the cursor
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
		for (map<int, string>::iterator it = allStrings.find(tempIndex); it != allStrings.end(); ++it) {
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
bool Editor::changeMapKey(map<int, string> *m, int key, int increment, bool createNonExisting)
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
void Editor::changeMapKey(map<int, int> *m, int key, int increment, bool createNonExisting)
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
void Editor::changeMapKey(map<int, uint64_t> *m, int key, int increment, bool createNonExisting)
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
void Editor::changeMapKey(map<int, bool> *m, int key, int increment, bool createNonExisting)
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
	// and need to create a separate loop for each
	bool createNonExisting = changeMapKey(&allStrings, key, increment, false);
	// if the string map key exists, then create entries for all other maps
	// in case any of them doesn't have the current key (which should not be the case)
	changeMapKey(&allStringStartPos, key, increment, createNonExisting);
	changeMapKey(&tracebackStr, key, increment, createNonExisting);
	changeMapKey(&tracebackColor, key, increment, createNonExisting);
	changeMapKey(&tracebackNumLines, key, increment, createNonExisting);
	changeMapKey(&tracebackTimeStamps, key, increment, createNonExisting);
	changeMapKey(&executingLines, key, increment, createNonExisting);
	changeMapKey(&executionDegrade, key, increment, createNonExisting);
	changeMapKey(&executionTimeStamp, key, increment, createNonExisting);
}

//--------------------------------------------------------------
void Editor::eraseMapKey(map<int, string> *m, int key)
{
	m->erase(key);
}

//--------------------------------------------------------------
void Editor::eraseMapKey(map<int, int> *m, int key)
{
	m->erase(key);
}

//--------------------------------------------------------------
void Editor::eraseMapKey(map<int, uint64_t> *m, int key)
{
	m->erase(key);
}

void Editor::eraseMapKey(map<int, bool> *m, int key)
{
	m->erase(key);
}

//--------------------------------------------------------------
void Editor::eraseMapKeys(int key)
{
	// since all maps below are created with every new line
	// we can safely make all the calls below inside the same loop
	// and dont't need to create a separate loop for each
	eraseMapKey(&allStrings, key);
	eraseMapKey(&allStringStartPos, key);
	eraseMapKey(&tracebackStr, key);
	eraseMapKey(&tracebackColor, key);
	eraseMapKey(&tracebackNumLines, key);
	eraseMapKey(&tracebackTimeStamps, key);
	eraseMapKey(&executingLines, key);
	eraseMapKey(&executionDegrade, key);
	eraseMapKey(&executionTimeStamp, key);
}

//--------------------------------------------------------------
// check if the current position is a tab and return its index within allStringTabs
bool Editor::isThisATab(int pos)
{
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
		int posLocal = max((cursorPos-i), 0);
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
	return min((int)allStrings[cursorLineIndex].size(), maxCharactersPerString);
}

//--------------------------------------------------------------
bool Editor::startsWith(string a, string b)
{
	if (a.size() < b.size()) return false;
	if (a.substr(0, b.size()).compare(b) == 0) return true;
	return false;
}

//--------------------------------------------------------------
bool Editor::endsWith(string a, string b)
{
	if (a.size() < b.size()) return false;
	if (a.substr(a.size()-b.size()).compare(b) == 0) return true;
	return false;
}

//----------------------------------------------------------------
void Editor::assembleString(int key, bool executing, bool lineBreaking)
{
	// keep track of quotation marks so two can be insterted automatically
	// but when typing the second it gets omitted since it has already been inserted
	// static int quoteCounter = 0;
	// static int curlyBracketCounter = 0;
	// first check if we're receiving any of the special characters
	// 8 is backspace
	if (key == 8) {
		string deletedChar;
		if (highlightManyChars) {
			deleteString();
			highlightManyChars = false;
		}
		else if ((allStrings[cursorLineIndex].size() > 0) && (cursorPos > 0)) {
			int numCharsToDelete = 1;
			// check if the cursor is at the end of the string
			if (cursorPos == (int)allStrings[cursorLineIndex].size()-allStringStartPos[cursorLineIndex]) {
				if (isThisATab(cursorPos-TABSIZE)) numCharsToDelete = TABSIZE;
				deletedChar = allStrings[cursorLineIndex].substr(allStrings[cursorLineIndex].size()-numCharsToDelete, numCharsToDelete);
				allStrings[cursorLineIndex] = allStrings[cursorLineIndex].substr(0, allStrings[cursorLineIndex].size()-numCharsToDelete);
			}
			else {
				if (isThisATab(cursorPos-TABSIZE)) numCharsToDelete = TABSIZE;
				string first = allStrings[cursorLineIndex].substr(0, cursorPos+allStringStartPos[cursorLineIndex]-numCharsToDelete);
				string second = allStrings[cursorLineIndex].substr(cursorPos+allStringStartPos[cursorLineIndex]);
				deletedChar = allStrings[cursorLineIndex].substr(cursorPos+allStringStartPos[cursorLineIndex]-numCharsToDelete, numCharsToDelete);
				allStrings[cursorLineIndex] = first + second;
			}
			allStringStartPos[cursorLineIndex] -= numCharsToDelete;
			if (allStringStartPos[cursorLineIndex] < 0) {
				allStringStartPos[cursorLineIndex] = 0;
				cursorPos -= numCharsToDelete;
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
		}
		else if (cursorPos == 0) {
			if (cursorLineIndex > 0) {
				int thisLineStrLength = allStrings[cursorLineIndex].size();
				setLineIndexesUpward(cursorLineIndex-1);
				copyOnLineDelete();
				// when we press backspace and the cursor is that the beginning of a line
				// if there is text in the line, it will be concatenated to the text
				// of the line above, but the cursor should not be placed at the end
				// of this concatenated string
				cursorPos = arrowCursorPos = max(maxCursorPos() - thisLineStrLength, 0);
				lineCount--;
				lineCountOffset--;
				if (lineCountOffset < 0) {
					lineCountOffset = 0;
				}
			}
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
			((ofApp*)ofGetAppPtr())->parseStrings(executingLineLocal, numExecutingLines);
			if (highlightManyChars) highlightManyChars = false;
		}
		else {
			if (highlightBracket && cursorPos > 0) {
				// if we hit return while in between two curly brackets
				if ((allStrings[cursorLineIndex].substr(cursorPos-1, 1).compare("{") == 0) &&
						(allStrings[cursorLineIndex].substr(cursorPos, 1).compare("}") == 0)) {
					// we insert two new lines and add a horizontal tab in the middle
					newLine();
					lineBreaking = false;
					// getNestDepth() returns a vector with the bottom line of the code chunk
					// within the nest of curly brackets and the depth of the nest
					// the bottom line is used in allOtherKeys()
					vector<int> nestDepth = getNestDepth();
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
			highlightManyChars = false;
		}
		else if (cursorPos < (int)allStrings[cursorLineIndex].size()) {
			int numCharsToDelete = 1;
			string deletedChar;
			if (isThisATab(cursorPos)) numCharsToDelete = TABSIZE;
			string first = allStrings[cursorLineIndex].substr(0, cursorPos);
			string second = allStrings[cursorLineIndex].substr(cursorPos+numCharsToDelete);
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
	string charToInsert;
	int cursorPosIncrement = 1;
	bool doubleChar = false;
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
			allStrings[cursorLineIndex] = allStrings[cursorLineIndex].substr(0, cursorPos+allStringStartPos[cursorLineIndex]) + \
										  charToInsert + allStrings[cursorLineIndex].substr(cursorPos+allStringStartPos[cursorLineIndex]);
			if ((int)allStrings[cursorLineIndex].size() > maxCharactersPerString) {
				// if the string is long don't move the cursor but scroll the string
				allStringStartPos[cursorLineIndex] += cursorPosIncrement;
				cursorPosIncrement = 0; 
			}
		}
		// or if it's at the beginning, place the character at the beginning
		else {
			allStrings[cursorLineIndex] = charToInsert + allStrings[cursorLineIndex].substr(cursorPos+allStringStartPos[cursorLineIndex]);
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
void Editor::setExecutionVars()
{
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
void Editor::setHighlightManyChars(int charPos1, int charPos2, int charLine1, int charLine2)
{
	firstChar = min(charPos1, charPos2);
	lastChar = max(charPos1, charPos2);
	topLine = min(charLine1, charLine2);
	bottomLine = max(charLine1, charLine2);
	if ((charPos1 == charPos2) && (charLine1 == charLine2) && highlightManyChars) {
		highlightManyChars = false;
	}
	else {
		highlightManyChars = true;
	}
}

//--------------------------------------------------------------
bool Editor::isNumber(string str)
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
bool Editor::isFloat(string str)
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
string Editor::replaceCharInStr(string str, string a, string b)
{
	auto it = str.find(a);
	while (it != string::npos) {
		str.replace(it, a.size(), b);
		it = str.find(a);
	}
	return str;
}

//---------------------------------------------------------------
//vector<string> Editor::tokenizeString(string str, string delimiter, bool addDelimiter)
//{
//	size_t start = 0;
//	size_t end = str.find(delimiter);
//	vector<string> tokens;
//	while (end != string::npos) {
//		tokens.push_back(str.substr(start, end));
//		if (addDelimiter) tokens.push_back(delimiter);
//		start += end + 1;
//		end = str.substr(start).find(delimiter);
//	}
//	// the last token is not extracted in the loop above because end has reached string::npos
//	// so we extract it here by simply passing a substring from the last start point to the end
//	tokens.push_back(str.substr(start));
//	return tokens;
//}

//---------------------------------------------------------------
vector<string> Editor::tokenizeString(string str, string delimiter, bool addDelimiter)
{
	size_t prev = 0, pos;
	vector<string> tokens;
	while ((pos = str.find_first_of(delimiter, prev)) != string::npos)
	{
	    if (pos > prev) {
			tokens.push_back(str.substr(prev, pos-prev));
		}
		if (addDelimiter) tokens.push_back(str.substr(pos, 1));
	    prev = pos+1;
	}
	if (prev < str.length()) {
		tokens.push_back(str.substr(prev, string::npos));
	}
	return tokens;
}

//---------------------------------------------------------------
vector<int> Editor::findIndexesOfCharInStr(string str, string charToFind)
{
	size_t pos = str.find(charToFind, 0);
	vector<int> tokensIndexes;
	while (pos != string::npos) {
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
		cursorPos = 0;
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
		// test if we arrive at a shorter string than the one below
		if (cursorPos >= ((int)allStrings[cursorLineIndex].size()-allStringStartPos[cursorLineIndex])) {
			// in which case display the cursor right after the string
			cursorPos = min(maxCursorPos(), arrowCursorPos);
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
		setHighlightManyChars(highlightManyCharsStart, cursorPos,
				highlightManyCharsLineIndex, cursorLineIndex);
	}
	else if (highlightManyChars) {
		highlightManyChars = false;
		upArrow(min(cursorLineIndex, highlightManyCharsLineIndex));
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
		int limit = lineCount-(lineCount-maxNumLines)+lineCountOffset - 1;
		if (cursorLineIndex > limit) {
			if (lineCountOffset < (lineCount-maxNumLines)) {
				lineCountOffset += diff;
				lineCountOffset = min(lineCountOffset, (lineCount-maxNumLines));
			}
			else {
				cursorLineIndex = lineCount - 1;
				cursorPos = arrowCursorPos = maxCursorPos();
			}
		}
		return true;
	}
	else if (cursorLineIndex > (lineCount-1)) {
		cursorLineIndex = lineCount - 1;
		cursorPos = maxCursorPos();
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
			cursorPos = min(maxCursorPos(), arrowCursorPos);
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
		setHighlightManyChars(highlightManyCharsStart, cursorPos,
				highlightManyCharsLineIndex, cursorLineIndex);
	}
	else if (highlightManyChars) {
		highlightManyChars = false;
		downArrow(max(cursorLineIndex, highlightManyCharsLineIndex));
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
		int whiteSpaceIndex = 0;
		while (whiteSpaceIndex <= cursorPos) {
			whiteSpaceIndex = allStrings[cursorLineIndex].find(" ", whiteSpaceIndex+1);
			if (whiteSpaceIndex == (int)string::npos) {
				whiteSpaceIndex = (int)allStrings[cursorLineIndex].size();
				break;
			}
		}
		numPosToMove = whiteSpaceIndex - cursorPos;
		if (numPosToMove == 0) numPosToMove = 1;
	}
	cursorPos += numPosToMove;
	arrowCursorPos = cursorPos;
	// if we're one position before the end of the string or the edge of the window
	if (cursorPos > maxCursorPos()) {
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
	if (shiftPressed) {
		if (!highlightManyChars && numPosToMove > 0) {
			highlightManyCharsStart = cursorPos - numPosToMove;
			highlightManyCharsLineIndex = cursorLineIndex;
		}
		setHighlightManyChars(highlightManyCharsStart, cursorPos, highlightManyCharsLineIndex, cursorLineIndex);
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
	if (ctrlPressed) {
		// find the next white space from the position of the cursor
		int whiteSpaceIndex = allStrings[cursorLineIndex].size();
		while (whiteSpaceIndex >= cursorPos) {
			whiteSpaceIndex = allStrings[cursorLineIndex].rfind(" ", whiteSpaceIndex-1);
			if (whiteSpaceIndex == (int)string::npos) {
				whiteSpaceIndex = 0;
				break;
			}
		}
		numPosToMove = cursorPos - whiteSpaceIndex;
		if (numPosToMove == 0) numPosToMove = 1;
	}
	cursorPos -= numPosToMove;
	arrowCursorPos = cursorPos;
	if (cursorPos < 0) {
		if ((int)allStrings[cursorLineIndex].size() > maxCharactersPerString && allStringStartPos[cursorLineIndex] > 0) {
			cursorPos += numPosToMove;
			arrowCursorPos = cursorPos;
			allStringStartPos[cursorLineIndex] -= numPosToMove;
		}
		if (allStringStartPos[cursorLineIndex] <= 0) {
			allStringStartPos[cursorLineIndex] = 0;
			if (cursorLineIndex > 0) {
				cursorLineIndex--;
				cursorPos = arrowCursorPos = min((int)allStrings[cursorLineIndex].size(), maxCharactersPerString);
				allStringStartPos[cursorLineIndex] = (int)allStrings[cursorLineIndex].size() - maxCharactersPerString;
			}
			else {
				cursorPos = arrowCursorPos = 0;
			}
		}
	}
	if (allStringStartPos[cursorLineIndex] < 0) {
		allStringStartPos[cursorLineIndex] = 0;
	}
	if (shiftPressed) {
		if (!highlightManyChars) {
			highlightManyCharsStart = cursorPos + numPosToMove;
			highlightManyCharsLineIndex = cursorLineIndex;
			if (ctrlPressed) highlightManyCharsStart += numPosToMove;
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
	// test if we arrive at a shorter string than the one above
	if (cursorPos >= ((int)allStrings[cursorLineIndex].size()-allStringStartPos[cursorLineIndex])) {
		// in which case display the cursor right after the string
		cursorPos = min(maxCursorPos(), arrowCursorPos);
	}
	int tabIndex = didWeLandOnATab();
	if (tabIndex >= 0) cursorPos = tabIndex;
	if (shiftPressed) {
		if (!highlightManyChars) {
			highlightManyCharsStart = prevCursorPos;
			highlightManyCharsLineIndex = prevCursorLineIndex;
		}
		setHighlightManyChars(highlightManyCharsStart, cursorPos,
				highlightManyCharsLineIndex, cursorLineIndex);
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
	// test if we arrive at a shorter string than the one above
	if (cursorPos >= ((int)allStrings[cursorLineIndex].size()-allStringStartPos[cursorLineIndex])) {
		// in which case display the cursor right after the string
		cursorPos = min(maxCursorPos(), arrowCursorPos);
	}
	int tabIndex = didWeLandOnATab();
	if (tabIndex >= 0) cursorPos = tabIndex;
	if (shiftPressed) {
		if (!highlightManyChars) {
			highlightManyCharsStart = prevCursorPos;
			highlightManyCharsLineIndex = prevCursorLineIndex;
		}
		setHighlightManyChars(highlightManyCharsStart, cursorPos,
				highlightManyCharsLineIndex, cursorLineIndex);
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
void Editor::setInserting(bool insertingBool)
{
	inserting = insertingBool;
	if (inserting && showCommand) {
		showCommand = false;
		commandStr = "";
	}
}

//--------------------------------------------------------------
bool Editor::getInserting()
{
	return inserting;
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
			vector<int> nestDepth = getNestDepth();
			if (nestDepth[1] > 0) {
				// set the number of lines to execute
				topLine = findTopLine(nestDepth[0]);
				bottomLine = nestDepth[0];
				highlightManyChars = true;
			}
			setExecutingLine();
			setExecutionVars();
			lineBreaking = false;
			executing = true;
		}
		else if(key == 61) { // + (actually =)
			if (altPressed) {
				((ofApp*)ofGetAppPtr())->brightnessCoeff += 0.01;
				if (((ofApp*)ofGetAppPtr())->brightnessCoeff > 1.0) ((ofApp*)ofGetAppPtr())->brightnessCoeff = 1.0;
				((ofApp*)ofGetAppPtr())->brightness = (int)(255 * ((ofApp*)ofGetAppPtr())->brightnessCoeff);
			}
			else {
				fontSize += 2;
				setGlobalFontSize(fontSize);
			}
			callAssemble = false;
		}
		else if (key == 45) { // -
			if (altPressed) {
				((ofApp*)ofGetAppPtr())->brightnessCoeff -= 0.01;
				if (((ofApp*)ofGetAppPtr())->brightnessCoeff < 0.0) ((ofApp*)ofGetAppPtr())->brightnessCoeff = 0.0;
				((ofApp*)ofGetAppPtr())->brightness = (int)(255 * ((ofApp*)ofGetAppPtr())->brightnessCoeff);
			}
			else {
				fontSize -= 2;
				if (fontSize < 2) fontSize = 2;
				setGlobalFontSize(fontSize);
			}
			callAssemble = false;
		}
		else if (key == 99) { // c for copy
			copyString();
			callAssemble = false;
		}
		else if (key == 118) { // v for paste
			pasteString();
			callAssemble = false;
		}
		else if (key == 120) { // z for cut
			copyString();
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
	}
	else if (shiftPressed) {
		if (key == 13) {
			// as with holding the Ctl key, do the same here
			// but position the cursor at the closing bracket
			vector<int> nestDepth = getNestDepth();
			if (nestDepth[1] > 0) {
				// set the number of lines to execute
				topLine = findTopLine(nestDepth[0]);
				bottomLine = nestDepth[0];
				highlightManyChars = true;
			}
			setExecutingLine();
			setExecutionVars();
			lineBreaking = false;
			executing = true;
			moveCursorOnShiftReturn();
		}
	}
	if (callAssemble) {
		assembleString(key, executing, lineBreaking);
	}
}

//--------------------------------------------------------------
void Editor::typeCommand(int key)
{
	if (key == 13) {
		executeCommand();
		typingCommand = false;
	}
	else if (key == 8 && typingCommand) {
		commandStr = commandStr.substr(0, commandStr.size()-1);
	}
	else if (key == 105 && commandStr.size() == 0 && typingCommand) {
		typingCommand = false;
		showCommand = false;
		inserting = true;
		resetMaxNumLines();
	}
	else {
		if (key == 58) commandStr = "";
		commandStr += char(key);
		typingCommand = true;
		showCommand = true;
	}
}

//--------------------------------------------------------------
void Editor::executeCommand()
{
	commandStrColor.r = 255 * ((ofApp*)ofGetAppPtr())->brightnessCoeff;
	commandStrColor.g = 255 * ((ofApp*)ofGetAppPtr())->brightnessCoeff;
	commandStrColor.b = 255 * ((ofApp*)ofGetAppPtr())->brightnessCoeff;
	if (startsWith(commandStr, ":save ")) {
		saveFile(commandStr.substr(6));	
	}
	else if (commandStr.compare(":w") == 0) {
		saveExistingFile();
	}
	else if (startsWith(commandStr, ":load ")) {
		string filePath = commandStr.substr(6);
		if (!startsWith(filePath, "/")) {
			if (startsWith(filePath, ".")) {
				filePath = filePath.substr(1);
			}
			else {
				filePath = "/data/" + filePath;
			}
			string pwdStr = ofSystem("pwd");
			// there's a newline character at the end of ofSystem("pwd"), so we remove it below
			filePath = pwdStr.substr(0, pwdStr.size()-1) + filePath;
		}
		loadFile(filePath);
	}
	else if (commandStr.compare(":load") == 0) {
		commandStr = "Load command called without an argument";
		commandStrColor = ofColor::red;
	}
	else if (startsWith(commandStr, ":ls")) {
		string pwdStr = ofSystem("pwd");
		if (commandStr.compare(":ls") == 0 && endsWith(pwdStr, "livelily/bin\n")) {
			pwdStr = pwdStr.substr(0, pwdStr.size()-1) + "/data";
		}
		else {
			//// change pwdStr to enable relative paths
			//if (!startsWith(commandStr.substr(4), "/")) {
			//	if (startsWith(commandStr.substr(4), ".")) {
			//		
			//	}
			//	else {
			//		pwdStr = pwdStr.substr(0, pwdStr.size()-1) + "/data/" + commandStr.substr(4);
			//	}
			//}
		}
		commandStr = ofSystem("ls -p " + pwdStr);
		// it is likely that the output is too long, so we split it to show multiple items per line
		vector<string> commandStrTokens = tokenizeString(commandStr, "\n", false);
		size_t lengthCounter = 0;
		commandStr = "";
		for (string s : commandStrTokens) {
			lengthCounter += s.size();
			if (lengthCounter >= maxBacktraceChars) {
				commandStr += "\n";
				lengthCounter = s.size();
			}
			commandStr += s;
			commandStr += "  ";
			lengthCounter += 2; // the two white spaces added above
		}
	}
	else if (commandStr.compare(":pwd") == 0) {
		commandStr = ofSystem("pwd");
		if (commandStr.length() >= maxBacktraceChars) {
			commandStr = commandStr.substr(0, maxBacktraceChars) + "\n" + commandStr.substr(maxBacktraceChars);
		}
	}
	else if (commandStr.compare(":q") == 0) {
		((ofApp*)ofGetAppPtr())->removePane();
	}
	else if (commandStr.compare(":qa") == 0) {
		((ofApp*)ofGetAppPtr())->exit();
	}
	else {
		commandStr = "Not an editor command: " + commandStr;
		commandStrColor = ofColor::red;
	}
}

//--------------------------------------------------------------
bool Editor::showingCommand()
{
	return showCommand;
}

//--------------------------------------------------------------
bool Editor::isTypingCommand()
{
	return typingCommand;
}

//--------------------------------------------------------------
ofColor Editor::getCommandStrColor()
{
	ofColor color(commandStrColor.r*((ofApp*)ofGetAppPtr())->brightnessCoeff,
				  commandStrColor.g*((ofApp*)ofGetAppPtr())->brightnessCoeff,
				  commandStrColor.b*((ofApp*)ofGetAppPtr())->brightnessCoeff);
	return color;
}

//--------------------------------------------------------------
string Editor::getCommandStr()
{
	return commandStr;
}

//--------------------------------------------------------------
void Editor::setExecutingLine()
{
	executingLine = cursorLineIndex;
}

//--------------------------------------------------------------
vector<int> Editor::setSelectedStrStartPosAndSize(int i)
{
	vector<int> v(2);
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
void Editor::copyString()
{
	strForClipBoard = "";
	if (highlightManyChars) {
		for (int i = topLine; i <= bottomLine; i++) {
			vector<int> posAndSize = setSelectedStrStartPosAndSize(i);
			// apply the offset of the string, in case it's too long to fit in a pane
			posAndSize[0] += allStringStartPos[i];
			strForClipBoard += allStrings[i].substr(posAndSize[0], posAndSize[1]);
			if (i < bottomLine) strForClipBoard += "\n";
		}
		strForClipBoard = replaceCharInStr(strForClipBoard, tabStr, "\t");
		ofGetWindowPtr()->setClipboardString(strForClipBoard);
	}
	else {
		ofGetWindowPtr()->setClipboardString("");
	}
}

//--------------------------------------------------------------
void Editor::yankString()
{
	yankedStr = "";
	if (highlightManyChars) {
		for (int i = topLine; i <= bottomLine; i++) {
			vector<int> posAndSize = setSelectedStrStartPosAndSize(i);
			// apply the offset of the string, in case it's too long to fit in a pane
			posAndSize[0] += allStringStartPos[i];
			strForClipBoard += allStrings[i].substr(posAndSize[0], posAndSize[1]);
			if (i < bottomLine) strForClipBoard += "\n";
		}
		yankedStr = replaceCharInStr(strForClipBoard, tabStr, "\t");
	}
	else {
		yankedStr = "";
	}
}

//--------------------------------------------------------------
void Editor::pasteString()
{
	strFromClipBoard = ofGetWindowPtr()->getClipboardString();
	vector<string> tokens = tokenizeString(strFromClipBoard, "\n", false);
	// if we paste in the middle of a string
	// the last line must concatenate the pasted string with the remainding string
	// of the line we broke in the middle
	string remainingStr = allStrings[cursorLineIndex].substr(cursorPos+allStringStartPos[cursorLineIndex]);
	cout << "remaining string is \"" << remainingStr << "\"\n";
	// first create as many new lines as there are in what we're pasting
	unsigned i = 0;
	// cursorLineIndex increments by one in postIncrementOnNewLine()
	// so we temporarily save it and restore it below
	int tempCursorLineIndex = cursorLineIndex;
	int tempCursorPos = cursorPos;
	cursorLineIndex = lineCount - 1;
	cursorPos = maxCursorPos();
	for (i = 0; i < tokens.size()-1; i++) {
		createNewLine("", 1);
		postIncrementOnNewLine();
	}
	i = 0;
	cursorLineIndex = tempCursorLineIndex;
	cursorPos = tempCursorPos;
	// then change the keys of the lines below where we are pasting
	moveLineNumbers((int)tokens.size());
	for (string originalToken : tokens) {
		string token = replaceCharInStr(originalToken, "\t", tabStr);
		if (i == 0) {
			allStrings[cursorLineIndex] = allStrings[cursorLineIndex].substr(0, cursorPos+allStringStartPos[cursorLineIndex]);
			cout << "string is split at pos " << cursorPos+allStringStartPos[cursorLineIndex] << endl;
			cout << "first part of string is \"" << allStrings[cursorLineIndex] << "\"\n";
			allStrings[cursorLineIndex] += token;
			cout << "1: string in line " << cursorLineIndex + 1 << " is \"" << allStrings[cursorLineIndex] << "\"\n";
		}
		if (i == (tokens.size()-1)) {
			// if we're pasting one line only just append the remaining string
			if (i == 0) {
				allStrings[cursorLineIndex] += remainingStr;
				cout << "2: string in line " << cursorLineIndex + 1 << " is \"" << allStrings[cursorLineIndex] << "\"\n";
			}
			// otherwise concatenate the token with the remaining
			else {
				//createNewLine(token+remainingStr, 1);
				//postIncrementOnNewLine();
				allStrings[cursorLineIndex] = token + remainingStr;
				cout << "3: string in line " << cursorLineIndex + 1 << " is \"" << allStrings[cursorLineIndex] << "\"\n";
			}
		}
		if ((i > 0) && (i < (tokens.size()-1))) {
			//createNewLine(token, 1);
			//postIncrementOnNewLine();
			allStrings[cursorLineIndex] = token;
			cout << "4: string in line " << cursorLineIndex + 1 << " is \"" << allStrings[cursorLineIndex] << "\"\n";
		}
		i++;
		cursorLineIndex++;
	}
	cursorPos += tokens[tokens.size()-1].size();
	fileEdited = true;
}

//--------------------------------------------------------------
void Editor::pasteYankedString()
{
	vector<string> tokens = tokenizeString(yankedStr, "\n", false);
	// if we paste in the middle of a string
	// the last line must concatenate the pasted string with the remainding string
	// of the line we broke in the middle
	string remainingStr = allStrings[cursorLineIndex].substr(cursorPos+allStringStartPos[cursorLineIndex]);
	// first create as many new lines as there are in what we're pasting
	unsigned i = 0;
	// cursorLineIndex increments by one in postIncrementOnNewLine()
	// so we temporarily save it and restore it below
	int tempCursorLineIndex = cursorLineIndex;
	int tempCursorPos = cursorPos;
	cursorLineIndex = lineCount - 1;
	cursorPos = maxCursorPos();
	for (i = 0; i < tokens.size()-1; i++) {
		createNewLine("", 1);
		postIncrementOnNewLine();
	}
	i = 0;
	cursorLineIndex = tempCursorLineIndex;
	cursorPos = tempCursorPos;
	// then change the keys of the lines below where we are pasting
	moveLineNumbers((int)tokens.size());
	for (string originalToken : tokens) {
		string token = replaceCharInStr(originalToken, "\t", tabStr);
		if (i == 0) {
			allStrings[cursorLineIndex] = allStrings[cursorLineIndex].substr(0, cursorPos+allStringStartPos[cursorLineIndex]);
			allStrings[cursorLineIndex] += token;
		}
		if (i == (tokens.size()-1)) {
			// if we're pasting one line only just append the remaining string
			if (i == 0) {
				allStrings[cursorLineIndex] += remainingStr;
			}
			// otherwise concatenate the token with the remaining
			else {
				//createNewLine(token+remainingStr, 1);
				//postIncrementOnNewLine();
				allStrings[cursorLineIndex] = token + remainingStr;
			}
		}
		if ((i > 0) && (i < (tokens.size()-1))) {
			//createNewLine(token, 1);
			//postIncrementOnNewLine();
			allStrings[cursorLineIndex] = token;
		}
		i++;
		cursorLineIndex++;
	}
	cursorPos += tokens[tokens.size()-1].size();
	fileEdited = true;
}

//--------------------------------------------------------------
void Editor::deleteString()
{
	lineCount -= (bottomLine - topLine);
	int numLinesToDelete = bottomLine - topLine;
	for (int i = topLine; i <= bottomLine; i++) {
		if ((i > topLine) && (i < bottomLine)) {
			eraseMapKeys(i);
		}
		else {
			vector<int> posAndSize = setSelectedStrStartPosAndSize(i);
			if (i == topLine) {
				allStrings[i].erase(posAndSize[0], posAndSize[1]);
				if (allStrings[i].size() == 0) {
					releaseTraceback(i);
				}
				else {
					// if the first line is not deleted entirely, get one line back in the count
					lineCount++;
					// and one line less to delete
					numLinesToDelete--;
				}
			}
			else if (i == bottomLine) {
				allStrings[i].erase(posAndSize[0], posAndSize[1]);
				if (allStrings[i].size() == 0) {
					eraseMapKeys(i);
				}
				else {
					// if the string is not entirely deleted
					allStringStartPos[i] = 0;
					// if the last line is not deleted entirely, get one line back in the count
					lineCount++;
					// and one line less to delete
					numLinesToDelete--;
				}
			}
		}
	}
	if (cursorLineIndex > highlightManyCharsLineIndex) {
		cursorLineIndex = highlightManyCharsLineIndex;
		cursorPos = highlightManyCharsStart;
	}
	if (numLinesToDelete > 0) {
		moveLineNumbers(-numLinesToDelete);
	}
	fileEdited = true;
}

//--------------------------------------------------------------
vector<int> Editor::sortVec(vector<int> v)
{
	sort(v.begin(), v.end());
	return v;
}

//--------------------------------------------------------------
void Editor::setTraceback(int errorCode, string errorStr, int lineNum)
{
	// error codes are: 0 - nothing, 1 - note, 2 - warning, 3 - error
	if (errorCode == 0 || errorCode == 1) tracebackColor[lineNum] = 0; 
	else if (errorCode == 2) tracebackColor[lineNum] = 1;
	else if (errorCode == 3) tracebackColor[lineNum] = 2;
	else tracebackColor[lineNum] = 0;
	// find number of newlines in traceback string
	int numLines = count(begin(errorStr), end(errorStr), '\n') + 1;
	tracebackStr[lineNum] = errorStr;
	tracebackTimeStamps[lineNum] = ofGetElapsedTimeMillis();
	tracebackNumLines[lineNum] = numLines;
}

//--------------------------------------------------------------
void Editor::releaseTraceback(int lineNum)
{
	map<int, string>::iterator it = tracebackStr.find(lineNum);
	if (it != tracebackStr.end()) {
		tracebackStr[lineNum] = "";
		tracebackColor[lineNum] = 0;
		tracebackNumLines[lineNum] = 1;
	}
}

//--------------------------------------------------------------
string Editor::getTracebackStr(int lineNum)
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
map<int, uint64_t>::iterator Editor::getTracebackTimeStampsBegin()
{
	return tracebackTimeStamps.begin();
}

//--------------------------------------------------------------
map<int, uint64_t>::iterator Editor::getTracebackTimeStampsEnd()
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
		string fileName = saveFileResult.filePath;
		cout << "save file name: \"" << fileName << "\"\n";
		saveFile(fileName);
	}
}

//--------------------------------------------------------------
void Editor::saveFile(string fileName)
{
	if (!endsWith(fileName, ".lyv") || !endsWith(fileName, ".py") || !endsWith(fileName, ".lua")) {
		fileName += ".lyv";
	}
	ofstream file(fileName.c_str());
	if (!file.is_open()) {
		// if for some reason the file can't be opened
		// instead of the file name, the string "could not open file"
		// will be displayed for 2 seconds and then it will return to untitled.lyv
		couldNotSaveFile = true;
		fileName = "could not save file";
		fileLoadErrorTimeStamp = ofGetElapsedTimeMillis();
		return;
	}
	for (unsigned i = 0; i < allStrings.size(); i++) {
		file << allStrings[i] << "\n";
	}
	file.close();
	fileLoaded = true;
	string fullPath = fileName.c_str();
	size_t lastBackSlash = fullPath.find_last_of("/");
	loadedFileStr = fullPath.substr(lastBackSlash+1);
	if (endsWith(loadedFileStr, ".lyv")) thisLang = 0;
	else if (endsWith(loadedFileStr, ".py")) thisLang = 1;
	else if (endsWith(loadedFileStr, ".lua")) thisLang = 2;
	else thisLang = 0;
	fileEdited = false;
}

//--------------------------------------------------------------
void Editor::saveExistingFile()
{
	//fstream file(loadedFileStr);
	//if (!file.is_open()) {
	//	// if for some reason the file can't be opened
	//	// instead of the file name, the string "could not open file"
	//	// will be displayed for 2 seconds and then it will return to untitled.lyv
	//	couldNotSaveFile = true;
	//	fileLoadErrorTimeStamp = ofGetElapsedTimeMillis();
	//	return;
	//}
	//file.seekp(0, std::ios_base::beg);
	//string allStrConcat = "";
	//for (unsigned i = 0; i < allStrings.size(); i++) {
	//	//file << allStrings[i] << "\n";
	//	allStrConcat += allStrings[i] + "\n";
	//}
	//file.write(allStrConcat.c_str(), allStrConcat.size());
	//file.close();
	//fileEdited = false;
}

//--------------------------------------------------------------
size_t Editor::findChordEnd(string str)
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
size_t Editor::findChordStart(string str)
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
void Editor::loadXMLFile(string filePath)
{
	ifstream file(filePath.c_str());
	string line;
	createNewLine("\\score show", 0);
	cursorLineIndex++;
	createNewLine("\\score animate", 0);
	cursorLineIndex++;
	createNewLine("", 0);
	cursorLineIndex++;
	int partID = 0, prevPartID = 0;
	bool mustMoveKeys = false;
	int partIDOffset = -1;
	int numStaves = 1, loopIter, i;
	size_t ndx1, ndx2;
	unsigned int lineNum = 1;
	map<int, std::pair<int, std::pair<string, string>>> insts;
	while (getline(file, line)) {
		string lineWithoutTabs = replaceCharInStr(line, "\t", "");
		string lineWithoutSpaces = replaceCharInStr(lineWithoutTabs, " ", "");
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
			insts[partID-1] = std::make_pair(1, std::make_pair("P"+to_string(partID), lineWithoutSpaces.substr(ndx1+1, ndx2-ndx1-1)));
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
					// change the keys of the following items in the map
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
					insts[partID+i] = std::make_pair(numStaves, std::make_pair(insts[partID].second.first, insts[partID].second.second.substr(0, insts[partID].second.second.size()-1)+to_string(i+1)));
				}
				if (mustMoveKeys) partIDOffset += (numStaves - 1);
			}
		}
	}
	string instLine = "\\insts";
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
	map<string, int> clefNdxs {{"G", 0}, {"F", 1}, {"C", 2}};
	map<int, map<int, std::pair<bool, int>>> instClefs;
	map<int, int> prevInstClef;
	// we have read the instruments, we now read the rest of the score to store clef changes
	file.clear();
	file.seekg(0, file.beg);
	while (getline(file, line)) {
		string lineWithoutTabs = replaceCharInStr(line, "\t", "");
		string lineWithoutSpaces = replaceCharInStr(lineWithoutTabs, " ", "");
		if (startsWith(lineWithoutSpaces, "<partid")) {
			for (auto it = insts.begin(); it != insts.end(); ++it) {
				if (it->second.second.first.compare(lineWithoutSpaces.substr(9, lineWithoutSpaces.substr(9).find_last_of("\""))) == 0) {
					// if this insturment is groupped, this is the index of the first instrument of the group
					// the incrementing indexes are found further down when we get a "<staff>" line
					instNdx = it->first;
					// we determine if this instrument is groupped by querying the first value of its pair
					// which is the value to the instrument map
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
				map<int, std::pair<bool, int>> m {{barNum, std::make_pair(false, 0)}};
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
			if (lineWithoutSpaces.find("number") != string::npos) {
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
	map<int, std::pair<int, int>> times;
	while (getline(file, line)) {
		string lineWithoutTabs = replaceCharInStr(line, "\t", "");
		string lineWithoutSpaces = replaceCharInStr(lineWithoutTabs, " ", "");
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
	map<string, int> tempoBases {{"whole", 1}, {"half", 2}, {"quarter", 4}, {"eighth", 8}, {"16th", 16}, {"32nd", 32}, {"64th", 64}};
	map<int, std::pair<int, int>> tempi;
	while (getline(file, line)) {
		string lineWithoutTabs = replaceCharInStr(line, "\t", "");
		string lineWithoutSpaces = replaceCharInStr(lineWithoutTabs, " ", "");
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
			//string perminStr = lineWithoutSpaces.substr(firstDigitNdx, lastDigitNdx);
			//if (perminStr.find("c.") != string::npos) {
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
	int direction = 1;
	int dynamicNdx = 0;
	int alter = 0;
	int octave;
	size_t wedgeNdx, tupletNdx, lastDurNdx = 0;
	size_t openingAngleBracketNdx, closingAngleBracketNdx;
	map<string, string> articulations{{"marcato", "^"}, {"trill", "+"}, {"tenuto", "-"}, {"staccatissimo", "!"},
		{"accent", ">"}, {"staccato", "."}, {"portando", "_"}};
	string xmlDurs[7] = {"whole", "half", "quarter", "eighth", "16th", "32nd", "64th"};
	map<string, int> dynamicSymMap {{"ppp", 0}, {"pp", 1}, {"p", 2}, {"mp", 3}, {"mf", 4},
		{"f", 5}, {"ff", 6}, {"fff", 7}, {"crescendo", 8}, {"diminuendo", 9}, {"stop", 10}};
	map<int, string> dynamicNdxMap {{0, "\\ppp"}, {1, "\\pp"}, {2, "\\p"}, {3, "\\mp"}, {4, "\\mf"},
		{5, "\\f"}, {6, "\\ff"}, {7, "\\fff"}, {8, "\\<"}, {9, "\\>"}, {10, "\\!"}};
	string words = "";
	// map with instrument index as key and another map as value
	// value map has the bar number as key, and the Lilypond string as value
	map<int, map<int, std::pair<bool, string>>> notes;
	while (getline(file, line)) {
		string lineWithoutTabs = replaceCharInStr(line, "\t", "");
		string lineWithoutSpaces = replaceCharInStr(lineWithoutTabs, " ", "");
		//cout << lineNum << endl;
		if (startsWith(lineWithoutSpaces, "<partid")) {
			for (auto it = insts.begin(); it != insts.end(); ++it) {
				if (it->second.second.first.compare(lineWithoutSpaces.substr(9, lineWithoutSpaces.substr(9).find_last_of("\""))) == 0) {
					// if this insturment is groupped, this is the index of the first instrument of the group
					// the incrementing indexes are found further down when we get a "<staff>" line
					instNdx = it->first;
					// we determine if this instrument is groupped by querying the first value of its pair
					// which is the value to the instrument map
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
				map<int, std::pair<bool, string>> m;
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
				map<int, std::pair<bool, string>> m;
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
				if (chordInsertNdx == string::npos || chordInsertNdx < findChordStart(notes[instNdx+instNdxOffset][barNum].second)) {
					chordInsertNdx = notes[instNdx+instNdxOffset][barNum].second.find_last_of(",");
					// if there are no octave symbols, then the closing angle bracket goes right after the note character
					if (chordInsertNdx == string::npos || chordInsertNdx < findChordStart(notes[instNdx+instNdxOffset][barNum].second)) {
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
			// https://www.geeksforgeeks.org/case-conversion-lower-upper-vice-versa-string-using-bitwise-operators-cc/
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
				notes[instNdx+instNdxOffset][barNum].second.insert(tupletNdx+1, "\\tuplet "+to_string(tupletNumerator)+(string)"/"+to_string(tupletDenominator)+(string)" { ");
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
						notes[instNdx+instNdxOffset][barNum].second += to_string((int)pow(2, i));
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
		else if (startsWith(lineWithoutSpaces, "<directionplacement")) {
			if (lineWithoutSpaces.substr(21, lineWithoutSpaces.substr(21).find("\"")).compare("below") == 0) {
				direction = -1;
			}
			else {
				direction = 1;
			}
		}
		//else if (startsWith(lineWithoutSpaces, "<words")) {
		//	// we don't care to check if it's a chord or note, because the text comes right after the first chord note
		//	// and when we find a chord, we insert the necessary characters to the correct positions
		//	if (direction > 0) words += "^";
		//	else words += "_";
		//	words += "\"";
		//	// here we use line and not lineWithoutSpaces, because we want the spaces to be included in the text
		//	closingAngleBracketNdx = line.find(">");
		//	openingAngleBracketNdx = line.substr(closingAngleBracketNdx).find("<");
		//	if (openingAngleBracketNdx == string::npos) openingAngleBracketNdx = line.size() - closingAngleBracketNdx;
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
			if (lineWithoutSpaces.find("start") != string::npos) {
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
	// now write the notes in LiveLily notation
	createNewLine("", 0);
	cursorLineIndex++;
	map<int, string> clefSymbols {{0, "treble"}, {1, "bass"}, {2, "alto"}};
	// all parts have the same number of bars, so we query the first one
	// to determine how many bars we have to write
	for (auto it = notes[0].begin(); it != notes[0].end(); ++it) {
		string barOpening = "\\bar " + to_string(it->first) + " {";
		createNewLine(barOpening, 0);
		cursorLineIndex++;
		string time = tabStr + "\\time " + std::to_string(times[it->first].first) + "/" + std::to_string(times[it->first].second);
		createNewLine(time, 0);
		cursorLineIndex++;
		string tempoStr = tabStr + "\\tempo " + std::to_string(tempi[it->first].first) + " = " + std::to_string(tempi[it->first].second);
		createNewLine(tempoStr, 0);
		cursorLineIndex++;
		string beatAtStr = tabStr + "\\beatat " + std::to_string(tempi[it->first].first);
		createNewLine(beatAtStr, 0);
		cursorLineIndex++;
		for (auto it2 = insts.begin(); it2 != insts.end(); ++it2) {
			string instLine = tabStr + "\\" + it2->second.second.second;
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
//string Editor::getMidiPitch(int pitch, string *notes)
//{
//	string midiPitchStr = notes[pitch % 12];
//	int octaves = pitch - 48;
//	string octaveStr;
//	if (octaves >= 0) octaveStr = "'";
//	else octaveStr = ",";
//	octaves /= 12;
//	if (octaves == 0 && octaveStr == ",") midiPitchStr += ",";
//	for (int i = 0; i < (abs)octaves; i++) midiPitchStr += octaveStr;
//	return midiPitchStr;
//}
//
////--------------------------------------------------------------
//void Editor::loadMIDIFile(string filePath)
//{
//	MIDIFileLoader readMidi;
//	readMidi.loadFile(filePath);
//	int tempo = readMidi.getTempo();
//	int ppqn = readMidi.getPPQM();
//	vector<vector<noteData>> midiEvents = readMidi.getMidiEvents();
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
//	vector<long> prevTicks (midiEvents.size(), 0);
//	vector<int> prevMidiEventsNdx (midiEvents.size(), 0);
//	vector<int> leftOversSixtyfours (midiEvents.size(), 0);
//	vector<string> leftOversString (midiEvents.size(), "");
//	string notes[12] = {"c", "cis", "d", "dis", "e", "f", "fis", "g", "gis", "a", "bes", "b"};
//
//	createNewLine("\\score show", 0);
//	cursorLineIndex++;
//	createNewLine("\\score animate", 0);
//	cursorLineIndex++;
//	createNewLine("", 0);
//	cursorLineIndex++;
//	string insts = "\\insts";
//	for (auto it = readMidi.instNames.begin(); it != readMidi.instNames.end(); ++it) {
//		insts += " ";
//		insts += *it;
//	}
//	createNewLine(insts, 0);
//	cursorLineIndex++;
//	createNewLine("", 0);
//	cursorLineIndex++;
//
//	vector<unsigned> timeSignaturesTimeStamps;
//	for (auto it = readMidi.timeSignatures.begin(); it != readMidi.timeSignatures.end(); ++it) {
//		timeSignaturesTimeStamps.push_back(it->first);
//	}
//
//	vector<unsigned> tempoTimeStamps;
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
//		createNewLine("\\bar " + to_string(barCount++) + " {", 0);
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
//		string timeStampStr = tabStr + "\\time " + to_string(numerator) + "/" + to_string(denominator);
//		createNewLine(timeStampStr, 0);
//		cursorLineIndex++;
//		string tempoStr = tabStr + "\\tempo " + to_string(denominator) + " = " + to_string(tempo);
//		createNewLine(tempoStr, 0);
//		cursorLineIndex++;
//		for (unsigned i = 0; i < midiEvents.size(); i++) {
//			long barDur = 0;
//			int totalBarDur = numerator * (64 / denominator);
//			std::pair<int, int> duration;
//			bool barDone = false;
//			bool leftOverTakesWholeBar = false;
//			string line = tabStr + "\\" + readMidi.instNames[i] + " ";
//			string note;
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
//					line += to_string(duration.first);
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
//						line += ("r" + to_string(restDur.first));
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
//						line += to_string(duration.first);
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
void Editor::loadLyvFile(string filePath)
{
	ifstream file(filePath.c_str());
	string line;
	while (getline(file, line)) {
		string lineWithoutTabs = replaceCharInStr(line, "\t", tabStr);
		createNewLine(lineWithoutTabs, 0);
		cursorLineIndex++;
	}
	file.close();
}

///////////////// load files graphic interface /////////////////

//--------------------------------------------------------------
void Editor::loadDialog()
{
	//Open the Open File Dialog
	ofFileDialogResult openFileResult = ofSystemLoadDialog("Select a .lyv or .xml file");
	//Check if the user opened a file
	if (openFileResult.bSuccess){
		loadFile(openFileResult.filePath);
	}
}

/////////////////// main load file function ////////////////////

//--------------------------------------------------------------
void Editor::loadFile(string fileName)
{
	lineCount = 1;
	cursorPos = 0;
	if (!endsWith(fileName, ".lyv") && !endsWith(fileName, ".xml") && !endsWith(fileName, "musicxml")) {
		fileName += ".lyv";
	}
	ifstream file(fileName.c_str());
	if (!file.is_open()) {
		// if for some reason the file can't be opened
		// instead of the file name, the string "could not open file"
		// will be displayed for 2 seconds and then it will return to untitled.lyv
		couldNotLoadFile = true;
		fileName = "could not load file";
		fileLoadErrorTimeStamp = ofGetElapsedTimeMillis();
		return;
	}
	file.close();
	allStrings.clear();
	allStringStartPos.clear();
	tracebackStr.clear();
	tracebackColor.clear();
	tracebackNumLines.clear();
	cursorLineIndex = cursorPos = 0;
	// determine what type of file we're opeining
	string fullPath = fileName.c_str();
	size_t lastBackSlash = fullPath.find_last_of("/");
	loadedFileStr = fullPath.substr(lastBackSlash+1);
	bool openingXML = false;
	if (endsWith(loadedFileStr, ".mxl") || endsWith(loadedFileStr, ".xml") || endsWith(loadedFileStr, ".musicxml")) {
		thisLang = 0;
		openingXML = true;
	}
	if (endsWith(loadedFileStr, ".lyv")) {
		thisLang = 0;
	}
	else if (endsWith(loadedFileStr, ".py")) {
		thisLang = 1;
	}
	else if (endsWith(loadedFileStr, ".lua")) {
		thisLang = 2;
	}
	else {
		thisLang = 0;
	}
	file.close();
	if (openingXML) {
		loadXMLFile(fileName);
	}
	else {
		loadLyvFile(fileName);
	}
	lineCount = (int)allStrings.size();
	cursorLineIndex = 0;
	cursorPos = arrowCursorPos = 0;
	fileLoaded = true;
	fileEdited = false;
}

/********************* loading files done *********************/

//--------------------------------------------------------------
float Editor::getCursorHeight()
{
	return cursorHeight;
}

//--------------------------------------------------------------
void Editor::printVector(vector<int> v)
{
	for (unsigned i = 0; i < v.size(); i++) {
		cout << v[i] << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
void Editor::printVector(vector<string> v)
{
	for (unsigned i = 0; i < v.size(); i++) {
		cout << v[i] << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
void Editor::printVector(vector<float> v)
{
	for (unsigned i = 0; i < v.size(); i++) {
		cout << v[i] << " ";
	}
	cout << endl;
}
