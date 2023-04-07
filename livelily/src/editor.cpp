#include <iostream>
#include <fstream>
#include "editor.h"
#include "ofApp.h"

//--------------------------------------------------------------
Editor::Editor(){
	executionRampStart = EXECUTIONRAMPSTART;
	lineCount = 0;
	lineCountOffset = 0;
	cursorLineIndex = 0;
	cursorPos = 0;
	arrowCursorPos = 0;

	executionStepPerMs = (float)EXECUTIONBRIGHTNESS / (float)(EXECUTIONDUR - EXECUTIONRAMPSTART);

	executingMultiple = false;
	highlightManyChars = false;
	highlightManyCharsStart = 0;

	ctlPressed = false;
	shiftPressed = false;

	highlightBracket = false;
	bracketHighlightRectX = 0;
	bracketHighlightRectY = 0;

	highlightedCharIndex = 0;
	stringExceededWindow = false;

	quoteCounter = 0;
	curlyBracketCounter = 0;
	squareBracketCounter = 0;

	fileLoaded = false;

	allStrings.push_back("");
	allStringStartPos.push_back(0);
	tracebackStr.push_back("");
	tabStr = "";
	for (int i = 0; i < TABSIZE; i++) {
		tabStr += " ";
	}
}

//--------------------------------------------------------------
void Editor::setID(int id){
	objID = id;
}

//--------------------------------------------------------------
void Editor::scanStrings(){
	allStringTabs.clear();
	bracketIndexes.clear();

	vector<vector<int>> bracketsIndexesLocal;
	vector<int> bracketsIndexesOneLine;
	for (unsigned i = 0; i < allStrings.size(); i++) {
		allStringTabs.push_back(findIndexesOfCharInStr(allStrings[i], tabStr));
		//bracketsIndexesLocal.push_back(findIndexesOfCharInStr(allStrings[i], "{"));
		bracketsIndexesOneLine = findIndexesOfCharInStr(allStrings[i], "{");
		vector<int> squareBracketsLocal = findIndexesOfCharInStr(allStrings[i], "[");
		for (unsigned j = 0; j < squareBracketsLocal.size(); j++) {
			//bracketsIndexesLocal[i].push_back(squareBracketsLocal[j]);
			bracketsIndexesOneLine.push_back(squareBracketsLocal[j]);
		}
		vector<int> closeCurlyIndexes = findIndexesOfCharInStr(allStrings[i], "}");
		vector<int> closeSquareIndexes = findIndexesOfCharInStr(allStrings[i], "]");
		for (unsigned j = 0; j < closeCurlyIndexes.size(); j++) {
			//bracketsIndexesLocal[i].push_back(closeCurlyIndexes[j]);
			bracketsIndexesOneLine.push_back(closeCurlyIndexes[j]);
		}
		for (unsigned j = 0; j < closeSquareIndexes.size(); j++) {
			//bracketsIndexesLocal[i].push_back(closeSquareIndexes[j]);
			bracketsIndexesOneLine.push_back(closeSquareIndexes[j]);
		}
		bracketsIndexesLocal.push_back(sortVec(bracketsIndexesOneLine));
	}

	vector<int> stackIdx;
	vector<int> stackLine;
	for (unsigned i = 0; i < bracketsIndexesLocal.size(); i++) {
		for (unsigned j = 0; j < bracketsIndexesLocal[i].size(); j++) {
			// safety test
			if ((int)allStrings[i].size() > bracketsIndexesLocal[i][j]) {
				// stack all opening brackets with line index and index in string
				if (allStrings[i].substr(bracketsIndexesLocal[i][j], 1).compare("{") == 0 || \
					allStrings[i].substr(bracketsIndexesLocal[i][j], 1).compare("[") == 0) {
					stackLine.push_back(i);
					stackIdx.push_back(bracketsIndexesLocal[i][j]);
				}
				// then if we get a closing bracket, check if the last stored element
				// is an opening bracket, in which case we store the line index and
				// string index of opening bracket, and the same pair for closing bracket
				// and then we remove the pair of the opening bracket from the stack
				else if (allStrings[i].substr(bracketsIndexesLocal[i][j], 1).compare("}") == 0 || \
						 allStrings[i].substr(bracketsIndexesLocal[i][j], 1).compare("]") == 0) {
					// safety test
					if (stackLine.size() > 0) {
						// another safety test
						if ((int)allStrings.size() > stackLine.back()) {
							// a third safety test
							if ((int)allStrings[stackLine.back()].size() > stackIdx.back()) {
								if (allStrings[stackLine.back()].substr(stackIdx.back(), 1).compare("{") == 0 || \
									allStrings[stackLine.back()].substr(stackIdx.back(), 1).compare("[") == 0) {
									vector<int> v {stackLine.back(), stackIdx.back(), (int)i, bracketsIndexesLocal[i][j]};
									bracketIndexes.push_back(v);
									stackLine.pop_back();
									stackIdx.pop_back();
								}
							}
						}
					}
				}
			}
		}
	}
	didWeLandOnBracket();
}

//-------------------------------------------------------------
void Editor::didWeLandOnBracket(){
	// check if the cursor is on top of a curly bracket
	// so that we can highlight its pairing bracket
	bool landedOnBracket = false;
	for (unsigned i = 0; i < bracketIndexes.size(); i++) {
		int cursorPosLocal = cursorPos + allStringStartPos[i];
		if (cursorLineIndex == bracketIndexes[i][0]) {
			if (cursorPosLocal == bracketIndexes[i][1]) {
				landedOnBracket = true;
				bracketHighlightRectX = bracketIndexes[i][3] - allStringStartPos[i];
				bracketHighlightRectY = bracketIndexes[i][2]; // - lineCountOffset;
				highlightedBracketStr = "}";
			}
		}
		if (landedOnBracket) {
			break;
		}
		if (cursorLineIndex == bracketIndexes[i][2]) {
			if (cursorPosLocal == bracketIndexes[i][3]) {
				landedOnBracket = true;
				bracketHighlightRectX = bracketIndexes[i][1] - allStringStartPos[i];
				bracketHighlightRectY = bracketIndexes[i][0]; // - lineCountOffset;
				highlightedBracketStr = "{";
			}
		}
		if (landedOnBracket) {
			break;
		}
	}
	if (landedOnBracket) {
		highlightBracket = true;
	}
	else {
		highlightBracket = false;
	}
}

//--------------------------------------------------------------
int Editor::areWeInBetweenBrackets(bool returnNestDepth){
	int nestDepth = 0;
	int closingBracketIndex = -1;
	for (unsigned i = 0; i < bracketIndexes.size(); i++) {
		// bracketIndexes contains the line of the opening curly brackets in element 0
		// and the line of closing brackets in element 2
		// elements 1 and 3 contain the cursor position of these brackets
		if (cursorLineIndex >= bracketIndexes[i][0]) {
			nestDepth++;
			closingBracketIndex = i;
		}
		if (cursorLineIndex >= bracketIndexes[i][2]) {
			nestDepth--;
			closingBracketIndex = -1;
		}
		if (cursorLineIndex < bracketIndexes[i][0]) {
			break;
		}
	}
	if (nestDepth < 0) {
		nestDepth = 0;
		closingBracketIndex = -1;
	}
	if (returnNestDepth) return nestDepth;
	else return closingBracketIndex;
}

//--------------------------------------------------------------
int Editor::getNumDigitsOfLineCount(){
	int numDigits = 0;
	int timesTen = 1;
	while (timesTen <= lineCount + 1) {
		timesTen *= 10;
		numDigits++;
	}
	return numDigits;
}

//--------------------------------------------------------------
void Editor::drawText(){
	/*******************************************************************/
	// get the number of digits of the line count
	int numDigits = getNumDigitsOfLineCount();
	lineNumberWidth = numDigits * oneCharacterWidth;
	int lineNumberHeight;
	if (lineCount < maxNumLines) {
		lineNumberHeight = (lineCount + 1 - lineCountOffset) * cursorHeight;
	}
	else {
		lineNumberHeight = (maxNumLines + 1) * cursorHeight;
	}
	// the variable below is for the rectangle that encloses the line numbers
	int lineNumRectWidth = lineNumberWidth + oneCharacterWidth;

	/*******************************************************************/
	// then draw the red traceback rectangles
	int count = 0; // separate counter to start from 0 because i starts from lineCountOffset
	int loopIter;
	int rectWidth = frameWidth - (lineNumberWidth + oneCharacterWidth);
	if (lineCount < maxNumLines) loopIter = lineCount + 1;
	else loopIter = maxNumLines + lineCountOffset + 1;
	ofSetColor(255, 0, 0, 150);
	for (int i = lineCountOffset; i < loopIter; i++) {
		if (tracebackStr[i].size() > 0) {
			int xOffset = lineNumberWidth + oneAndHalfCharacterWidth + frameXOffset;
			int yOffset = count * cursorHeight + letterHeightDiff + frameYOffset;
			ofDrawRectangle(xOffset, yOffset, rectWidth, cursorHeight);
		}
		count++;
	}
	ofSetColor(((ofApp*)ofGetAppPtr())->brightness);

	/*******************************************************************/
	// then check if we're executing any lines and draw the execution rectangle
	// needs to be first otherwise the alpha blending won't work as expected
	if (executeLine.size() > 0) {
		int allLinesExecuted = 0;
		for (unsigned i = 0; i < executeLine.size(); i++) {
			ofSetColor(0, 180, 255, executionDegrade[i]);
			int xOffset = lineNumberWidth + oneAndHalfCharacterWidth + frameXOffset;
			int yOffset;
			int rectHeight;
			if (executingMultiple) {
				yOffset = topLine * cursorHeight + letterHeightDiff - (lineCountOffset * cursorHeight);
				rectHeight = cursorHeight * (bottomLine - topLine + 1);
			}
			else {
				rectHeight = cursorHeight;
				yOffset = (executionLineIndex[i]-lineCountOffset) * cursorHeight + letterHeightDiff;
			}
			yOffset += frameYOffset;
			// rectWidth has been calculated above, before the loop that draw traceback rectangles
			ofDrawRectangle(xOffset, yOffset, rectWidth, rectHeight);
			if ((ofGetElapsedTimeMillis() - executionTimeStamp[i]) >= EXECUTIONRAMPSTART) {
				// calculate how many steps the brightness has to dim, depending on the elapsed time
				// and the step per millisecond
				int brightnessDegrade = (int)((ofGetElapsedTimeMillis() - \
										(executionTimeStamp[i]+EXECUTIONRAMPSTART)) * \
										executionStepPerMs);
				executionDegrade[i] = EXECUTIONBRIGHTNESS - brightnessDegrade;
				if (executionDegrade[i] < 0) {
					executionDegrade[i] = 0;
					allLinesExecuted++;
				}
			}
		}
		if (allLinesExecuted == (int)executeLine.size()) {
			executeLine.clear();
			executionDegrade.clear();
			executionTimeStamp.clear();
			executionLineIndex.clear();
		}
		if (executeLine.size() == 0) {
			// we're setting executingMultiple to false even if
			// we're executing one line only, just to avoid any test
			// as this operation is seemless
			executingMultiple = false;
		}
	}
	ofSetColor(((ofApp*)ofGetAppPtr())->brightness);

	/*******************************************************************/
	// then the rectangle to include the line numbering
	ofSetColor(((ofApp*)ofGetAppPtr())->backGround*3);
	int rectHeight = lineNumberHeight + letterHeightDiff;
	if (lineCount > (maxNumLines-1)) {
		rectHeight = ((ofApp*)ofGetAppPtr())->seqVec.tracebackYCoord - (((ofApp*)ofGetAppPtr())->lineWidth/2);
	}
	ofDrawRectangle(frameXOffset, frameYOffset, lineNumRectWidth, rectHeight);
	ofSetColor(((ofApp*)ofGetAppPtr())->brightness);

	/*******************************************************************/
	// then draw the line numbers
	count = 0;
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
		font.drawString(to_string(i+1), xOffset, ((count+1)*cursorHeight)+frameYOffset);
		count++;
	}

	/*******************************************************************/
	// then draw the rectangle that highlights a bracket
	if (highlightBracket) {
		ofSetColor(((ofApp*)ofGetAppPtr())->brightness/1.5, ((ofApp*)ofGetAppPtr())->brightness/3, ((ofApp*)ofGetAppPtr())->brightness/1.5);
		// X position is copied from the cursor's X position with cursorLineIndex and cursorPos
		// being replaced by bracketHighlightRectY and bracketHighlightRectX respectively
		int bracketX = lineNumberWidth + \
					   font.stringWidth(allStrings[bracketHighlightRectY].substr(0, bracketHighlightRectX)) + \
					   oneAndHalfCharacterWidth;
		int bracketY = ((bracketHighlightRectY-lineCountOffset) * cursorHeight) + letterHeightDiff;
    	//int bracketY = (bracketHighlightRectY * cursorHeight) + letterHeightDiff +(lineCountOffset * cursorHeight);
    	bracketX += frameXOffset;
    	bracketY += frameYOffset;
		ofDrawRectangle(bracketX, bracketY, oneCharacterWidth, cursorHeight);
	}

	/*******************************************************************/
	// then the actual text
	string strOnCursorLine;
	int strOnCursorLineXOffset = 0;
	int strOnCursorLineYOffset = frameYOffset;
	int strXOffset = lineNumberWidth + oneAndHalfCharacterWidth + frameXOffset;
	count = 0; // this extra counter starts from 0, whereas i starts from lineCountOffset
	for (int i = lineCountOffset; i < loopIter; i++) {
		int strYOffset = ((count+1)*cursorHeight) + frameYOffset;
		// first determine whether the string fits in the screen
		string strToDraw;
		if (i == cursorLineIndex) {
			if ((int)allStrings[i].size() > maxCharactersPerString) {
				// if the last part of the string is displayed don't display the ">" character
				// this is determined by testing whether the startPos equals the length of the string
				// minus the maxCharactersPerString, or whether startPos is one less, and the cursor
				// is at the end position. This last case occurs when inserting double characters
				// like brackets and quotes
				if ((int)allStrings[i].size()-maxCharactersPerString == allStringStartPos[i] || \
					(((int)allStrings[i].size()-maxCharactersPerString)-1 == allStringStartPos[i] && \
					 cursorPos == maxCharactersPerString)) {
					strToDraw = "<" + allStrings[i].substr(allStringStartPos[i]+1, maxCharactersPerString);
				}
				// if there's string that doesn't fit on either side, display both "<" and ">"
				else if (allStringStartPos[i] > 0) {
					strToDraw = "<" + allStrings[i].substr(allStringStartPos[i]+1, maxCharactersPerString-2) + ">";
				}
				// otherwise display only ">"
				else {
					strToDraw = allStrings[i].substr(allStringStartPos[i], maxCharactersPerString-1) + ">";
				}
			}
			else {
				strToDraw = allStrings[i];
			}
			strOnCursorLine = strToDraw;
			strOnCursorLineXOffset = strXOffset;
			strOnCursorLineYOffset = strYOffset;
		}
		else {
			if ((int)allStrings[i].size() > maxCharactersPerString) {
				strToDraw = allStrings[i].substr(0, maxCharactersPerString-1) + ">";
			}
			else {
				strToDraw = allStrings[i];
			}
		}
		// then check if we have selected this string so we must highlight it
		if (highlightManyChars && ((i>=topLine) && (i<=bottomLine))) {
			int maxWidth = 0;
			vector<int> posAndSize = setSelectedStrStartPosAndSize(i);
			int boxXOffset = lineNumberWidth + oneAndHalfCharacterWidth + \
                    		 (posAndSize[0]*oneCharacterWidth) + frameXOffset;
			int boxYOffset = ((count * cursorHeight) + letterHeightDiff) + frameYOffset;
			string strInBlack = strToDraw.substr(posAndSize[0], posAndSize[1]);
			int widthLocal = rectWidth;
			if (i == bottomLine) {
				widthLocal = font.stringWidth(strInBlack);
				if (widthLocal > maxWidth) maxWidth = widthLocal;
			}
			ofSetColor(((ofApp*)ofGetAppPtr())->brightness);
			// first draw the whole string in white in case we haven't selected the whole of it
			font.drawString(strToDraw, strXOffset, strYOffset);
			// then the selecting rectangle
			ofSetColor(((ofApp*)ofGetAppPtr())->brightness,
                	   (((ofApp*)ofGetAppPtr())->brightness/3)*2,
                	   ((ofApp*)ofGetAppPtr())->brightness/3);
			ofDrawRectangle(boxXOffset, boxYOffset, widthLocal, cursorHeight);
			ofSetColor(0);
			// and finally the selected string in black
			font.drawString(strInBlack, boxXOffset, strYOffset);
		}
		// then draw all other strings as long as they're not empty
		else if (strToDraw.size() > 0) {
			ofSetColor(((ofApp*)ofGetAppPtr())->brightness);
			font.drawString(strToDraw, strXOffset, strYOffset);
		}
		count++;
	}
	// reset color so the traceback line and text are visible
	// even when the last part of the text is highlighted
	ofSetColor(((ofApp*)ofGetAppPtr())->brightness);

	/*******************************************************************/
	// then draw the cursor
	if (!isActive) {
		ofNoFill();
	}
	int cursorX = lineNumberWidth + \
				  font.stringWidth(allStrings[cursorLineIndex].substr(0, cursorPos)) + \
				  oneAndHalfCharacterWidth + frameXOffset;
	int cursorY = ((cursorLineIndex-lineCountOffset) * cursorHeight) + letterHeightDiff + frameYOffset;
	ofDrawRectangle(cursorX, cursorY, oneCharacterWidth, cursorHeight);
	if (!isActive) {
		ofFill();
	}

	/*******************************************************************/
	// then draw the character the cursor is drawn on top of (if this is the case)
	if (isActive) {
		if (cursorPos < (int)strOnCursorLine.size()) {
			string onTopOfCursorStr = strOnCursorLine.substr(cursorPos, 1);
			ofSetColor(0);
			int maskedStrXOffset = font.stringWidth(strOnCursorLine.substr(0, cursorPos)) + strOnCursorLineXOffset;
			font.drawString(onTopOfCursorStr, maskedStrXOffset, strOnCursorLineYOffset);
			ofSetColor(((ofApp*)ofGetAppPtr())->brightness);
		}
	}
}

//--------------------------------------------------------------
void Editor::setStringsStartPos(){
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
void Editor::setMaxCharactersPerString(){
	lineNumberWidth = getNumDigitsOfLineCount() * oneCharacterWidth;
  	int width = frameWidth;
	if (((ofApp*)ofGetAppPtr())->seqVec.showScore) {
    	if (frameWidth > ((ofApp*)ofGetAppPtr())->seqVec.middleOfScreenX) {
      		width = ((ofApp*)ofGetAppPtr())->seqVec.middleOfScreenX;
    	}
	}
  	maxCharactersPerString = (width-lineNumberWidth-oneAndHalfCharacterWidth) / oneCharacterWidth;
	maxCharactersPerString -= 1;
	//setStringsStartPos();
}

//--------------------------------------------------------------
void Editor::setGlobalFontSize(int fontSize){
  ((ofApp*)ofGetAppPtr())->fontSize = fontSize;
  ((ofApp*)ofGetAppPtr())->changeFontSizeForAllEditors();
}

//--------------------------------------------------------------
void Editor::resetCursorPos(int oldMaxCharactersPerString){
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
void Editor::setFontSize(int fontSize){
	// store the previous the maximum characters per string
	int oldMaxCharactersPerString = maxCharactersPerString;
	Editor::fontSize = fontSize;
	font.load("DroidSansMono.ttf", fontSize);
	// we're using a monospace font, so we get the width of any character
	oneCharacterWidth = font.stringWidth("a");
	cursorHeight = font.stringHeight("q");
	halfCharacterWidth = oneCharacterWidth / 2;
	oneAndHalfCharacterWidth = oneCharacterWidth + halfCharacterWidth;
	// the variable below helps center the cursor at the Y axis with respect to the letters
	letterHeightDiff = font.stringHeight("j") - font.stringHeight("l");
	letterHeightDiff /= 2;
	setMaxCharactersPerString();
	setMaxNumLines();
	resetCursorPos(oldMaxCharactersPerString);
}

//--------------------------------------------------------------
void Editor::setMaxNumLines(){
	maxNumLines = (frameHeight-(letterHeightDiff*2)) / cursorHeight;
	maxNumLines -= 1; // leave some space for traceback
	lineCountOffset = cursorLineIndex - maxNumLines;
	if (lineCountOffset < 0) lineCountOffset = 0;
	else if ((lineCountOffset + maxNumLines) > lineCount) {
		lineCountOffset = lineCount = maxNumLines;
	}
}

//--------------------------------------------------------------
void Editor::postIncrementOnNewLine(){
	lineCount++;
	cursorLineIndex++;
	cursorPos = arrowCursorPos = 0;
	if ((lineCount + lineCountOffset) > maxNumLines) {
		lineCountOffset++;
	}
}

//--------------------------------------------------------------
void Editor::allocateNewLineMem(string str){
	allStringStartPos[cursorLineIndex] = 0;
	allStrings.push_back(str);
	allStringStartPos.push_back(0);
	tracebackStr.push_back("");
}

//--------------------------------------------------------------
void Editor::insertNewLineMem(){
	allStrings.insert(allStrings.begin()+cursorLineIndex+1,
					  allStrings[cursorLineIndex].substr(cursorPos+allStringStartPos[cursorLineIndex]));
	allStrings[cursorLineIndex] = allStrings[cursorLineIndex].substr(0, cursorPos+allStringStartPos[cursorLineIndex]);
	allStringStartPos.insert(allStringStartPos.begin()+cursorLineIndex+1, 0);
	tracebackStr.insert(tracebackStr.begin()+cursorLineIndex, "");
}

//--------------------------------------------------------------
void Editor::clearLineVectors(int lineIndex, int start, int length){
	allStrings[lineIndex].erase(start, length);
	allStringStartPos[lineIndex] = 0;
	tracebackStr[lineIndex] = "";
}

//--------------------------------------------------------------
void Editor::releaseMemOnBackspace(int lineIndex){
	allStrings.erase(allStrings.begin()+lineIndex);
	allStringStartPos.erase(allStringStartPos.begin()+lineIndex);
	tracebackStr.erase(tracebackStr.begin()+lineIndex);
}

//--------------------------------------------------------------
void Editor::copyMemOnBackspace(){
	allStrings[cursorLineIndex] += allStrings[cursorLineIndex+1];
	allStringStartPos[cursorLineIndex] = allStringStartPos[cursorLineIndex+1];
	tracebackStr[cursorLineIndex] += tracebackStr[cursorLineIndex+1];
}

//--------------------------------------------------------------
void Editor::newLine(){
	bool stringBreaks = false;

	if (cursorPos < ((int)allStrings[cursorLineIndex].size()-allStringStartPos[cursorLineIndex])) {
		stringBreaks = true;
	}
	if (cursorLineIndex == lineCount) {
		if (stringBreaks) {
			allocateNewLineMem(allStrings[cursorLineIndex].substr(cursorPos+allStringStartPos[cursorLineIndex]));
			//allStrings.push_back(allStrings[cursorLineIndex].substr(cursorPos+allStringStartPos[cursorLineIndex]));
			allStrings[cursorLineIndex] = allStrings[cursorLineIndex].substr(0, cursorPos+allStringStartPos[cursorLineIndex]);
		}
		else {
			allocateNewLineMem("");
		}
	}
	// if the cursor is not at the end, we have to insert
	// an empty string at the Y position of the cursor
	else {
		insertNewLineMem();
	}
	postIncrementOnNewLine();
}

//--------------------------------------------------------------
void Editor::moveCursorOnShiftReturn(){
	int tempIndex; // = cursorLineIndex + 1;
	if (highlightManyChars) tempIndex = bottomLine;
	else tempIndex = cursorLineIndex;
	if (tempIndex == lineCount) {
		allocateNewLineMem("");
		postIncrementOnNewLine();
		cursorLineIndex = lineCount;
	}
	else {
		tempIndex++;
		for (int i = tempIndex; i <= lineCount; i++) {
			if (allStrings[i].size() > 0) {
				tempIndex = i;
				break;
			}
		}
		setLineIndexesDownward(tempIndex);
	}
	cursorPos = 0;
}

//--------------------------------------------------------------
// check if the current position is a tab and return its index within allStringTabs
int Editor::isThisATab(int pos){
	// first check if a tab character exists in this string
	if (find(allStringTabs[cursorLineIndex].begin(), allStringTabs[cursorLineIndex].end(),
					 pos) != allStringTabs[cursorLineIndex].end()) {
		// then find its index
		auto it = find(allStringTabs[cursorLineIndex].begin(), allStringTabs[cursorLineIndex].end(), pos);
		int index = it - allStringTabs[cursorLineIndex].begin();
		return index;
	}
	else {
		return -1;
	}
}

//--------------------------------------------------------------
bool Editor::deletingTab(int pos){
	int index = isThisATab(pos);
	if (index >= 0) {
		return true;
	}
	else {
		return false;
	}
}

//--------------------------------------------------------------
int Editor::didWeLandOnATab(){
	int tabIndex = -1;
	for (int i = 0; i < TABSIZE; i++) {
		int posLocal = max((cursorPos-i), 0);
		if(isThisATab(posLocal) >= 0) {
			tabIndex = cursorPos - i;
			break;
		}
	}
	return tabIndex;
}

//---------------------------------------------------------------
int Editor::maxCursorPos(){
	return min((int)allStrings[cursorLineIndex].size(), maxCharactersPerString);
}

//----------------------------------------------------------------
void Editor::assembleString(int key, bool executing, bool lineBreaking){
	// keep track of quotation marks so two can be insterted automatically
	// but when typing the second it gets omitted since it has already been inserted
	// static int quoteCounter = 0;
	// static int curlyBracketCounter = 0;
	// first check if we're receiving any of the special characters
	// 8 is backspace
	if (key == 8) {
		bool deleteTab = false;
		string deletedChar;
		if (highlightManyChars) {
			deleteString();
			highlightManyChars = false;
		}
		else if ((allStrings[cursorLineIndex].size() > 0) && (cursorPos > 0)) {
			int numCharsToDelete = 1;
			// check if the cursor is at the end of the string
			if (cursorPos == (int)allStrings[cursorLineIndex].size()-allStringStartPos[cursorLineIndex]) {
				deleteTab = deletingTab(cursorPos-TABSIZE);
				if (deleteTab) numCharsToDelete = TABSIZE;
				deletedChar = allStrings[cursorLineIndex].substr(allStrings[cursorLineIndex].size()-numCharsToDelete, numCharsToDelete);
				allStrings[cursorLineIndex] = allStrings[cursorLineIndex].substr(0, allStrings[cursorLineIndex].size()-numCharsToDelete);
			}
			else {
				deleteTab = deletingTab(cursorPos-TABSIZE);
				if (deleteTab) numCharsToDelete = TABSIZE;
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
				setLineIndexesUpward(cursorLineIndex-1);
				cursorPos = arrowCursorPos = maxCursorPos();
				copyMemOnBackspace();
				releaseMemOnBackspace(cursorLineIndex+1);
				lineCount--;
				lineCountOffset--;
				if (lineCountOffset < 0) {
					lineCountOffset = 0;
				}
			}
		}
	}

	// 13 is return
	else if (key == 13) {
		quoteCounter = 0; // reset on newline
		curlyBracketCounter = 0;
		if (executing) {
			int executingLineLocal = executingLine;
			int numExecutingLines = 1;
			if (highlightBracket) {
				// we set the executingLine variable as the second line to enclose
				// the highlighted lines that will be executed, because in case of
				// shift+return, the cursor will have moved down before we set
				// the highlighted lines and characters
				setHighlightManyChars(0, 0, bracketHighlightRectY, executingLine);
			}
			if (highlightManyChars) {
				executingLineLocal = topLine;
				numExecutingLines = bottomLine - topLine + 1;
				executingMultiple = true;
				highlightManyChars = false;
			}
			((ofApp*)ofGetAppPtr())->parseStrings(executingLineLocal, numExecutingLines);
		}
		else {
			if (highlightBracket) {
				// if we hit return while in between two curly brackets
				if ((highlightedBracketStr.compare("{") == 0) &&
					(allStrings[cursorLineIndex].substr(cursorPos, 1).compare("}") == 0) &&
					((cursorPos-bracketHighlightRectX) == 1)) {
					// we insert two new lines and add a horizontal tab in the middle
					newLine();
					newLine();
					lineBreaking = false;
					scanStrings();
					int curlyNestDepth = areWeInBetweenBrackets(true);
					for (int i = 0; i < curlyNestDepth; i++) {
						assembleString(9); // write a horizontal tab
					}
					cursorLineIndex--;
					// if we add at least one tab to the line of the closing bracket
					// the cursor will be at least in position TABSIZE which will be
					// greater than the length of the string in the middle (this will be 0)
					// and the program will crash when checking if we're at the end of the string
					// see overloaded assembleString() where cursorPos is compared to maxCursorPos
					cursorPos = arrowCursorPos = 0;
					scanStrings();
					curlyNestDepth = areWeInBetweenBrackets(true);
					for (int i = 0; i < curlyNestDepth; i++) {
						assembleString(9); // write a horizontal tab
					}
				}
			}
			// if we are inside a curly bracket chunk
			else {
				int curlyNestDepth = areWeInBetweenBrackets(true);
				if (curlyNestDepth > 0) {
					newLine();
					lineBreaking = false;
					for (int i = 0; i < curlyNestDepth; i++) {
						assembleString(9);
					}
				}
			}
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
			if (deletingTab(cursorPos)) numCharsToDelete = TABSIZE;
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
			if (cursorLineIndex < lineCount) {
				copyMemOnBackspace();
				releaseMemOnBackspace(cursorLineIndex+1);
				lineCount--;
				lineCountOffset--;
				if (lineCountOffset < 0) {
					lineCountOffset = 0;
				}
			}
		}
	}

	// all other characters
	else {
		assembleString(key);
	}
	scanStrings();
}

//--------------------------------------------------------------
void Editor::assembleString(int key){
	string charToInsert;
	int cursorPosIncrement = 1;
	bool doubleChar = false;
	// horizontal tab
	if (key == 9) {
		charToInsert = tabStr;
		cursorPosIncrement = TABSIZE;
	}
	else if (key == 34) { // "
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
	else if (key == 123) { // {
		if (curlyBracketCounter == 0) {
			// insert opening and closing curly brackets, but move the cursor only one position
			charToInsert = "{}";
			curlyBracketCounter++;
			doubleChar = true;
		}
		else {
			charToInsert = "{";
			curlyBracketCounter--;
			if (curlyBracketCounter == 0) curlyBracketCounter = 0;
		}
	}
	else if (key == 125) { // }
		if (curlyBracketCounter > 0) {
			charToInsert = "";
			curlyBracketCounter = 0;
		}
		else {
			charToInsert = "}";
		}
	}
	else if (key == 91) { // [
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
	else if (key == 93) { // ]
		if (squareBracketCounter > 0) {
			charToInsert = "";
			squareBracketCounter = 0;
		}
		else {
			charToInsert = "]";
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
		// if we're placing a double char ({} or [] or "") at the end of a long string
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
void Editor::allocateExecutionMem(){
	// even if we execute many lines at once
	// we allocate memory for one line only, as the rectangle
	// that marks the execution copies its dimensions from
	// the rectangle that highlights multiple chosen lines
	executeLine.push_back(1);
	executionTimeStamp.push_back(ofGetElapsedTimeMillis());
	executionLineIndex.push_back(executingLine);
	executionDegrade.push_back(EXECUTIONBRIGHTNESS);
}

//--------------------------------------------------------------
void Editor::setHighlightManyChars(int charPos1, int charPos2, int charLine1, int charLine2){
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

//---------------------------------------------------------------
string Editor::replaceCharInStr(string str, string a, string b){
	auto it = str.find(a);
  while (it != string::npos) {
		str.replace(it, a.size(), b);
    it = str.find(a);
  }
	return str;
}

//---------------------------------------------------------------
vector<string> Editor::tokenizeString(string str, string delimiter){
	size_t pos = 0;
	string token;
	vector<string> tokens;
	while ((pos = str.find(delimiter)) != string::npos) {
    token = str.substr(0, pos);
		tokens.push_back(token);
    str.erase(0, pos + delimiter.length());
	}
	tokens.push_back(str);
	return tokens;
}

//---------------------------------------------------------------
vector<int> Editor::findIndexesOfCharInStr(string str, string charToFind){
	vector<int> tokensIndexes;
	size_t pos = str.find(charToFind, 0);
	while (pos != string::npos) {
    tokensIndexes.push_back((int)pos);
    pos = str.find(charToFind, pos+1);
	}
	return tokensIndexes;
}

/************* separate functions for certain keys *************/
//---------------------------------------------------------------
bool Editor::setLineIndexesUpward(int lineIndex){
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
void Editor::upArrow(int lineIndex){
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
	didWeLandOnBracket();
	
}

//--------------------------------------------------------------
bool Editor::setLineIndexesDownward(int lineIndex){
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
		int limit = lineCount-(lineCount-maxNumLines)+lineCountOffset;
		if (cursorLineIndex > limit) {
			if (lineCountOffset < (lineCount-maxNumLines)) {
				lineCountOffset += diff;
			}
			else {
				cursorLineIndex = lineCount;
				cursorPos = maxCursorPos();
			}
		}
		return true;
	}
	else if (cursorLineIndex > lineCount) {
    	cursorLineIndex = lineCount;
		cursorPos = maxCursorPos();
		return false;
	}
	else {
		return true;
	}
}

//--------------------------------------------------------------
void Editor::downArrow(int lineIndex){
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
	didWeLandOnBracket();
}

//--------------------------------------------------------------
void Editor::rightArrow(){
	int numPosToMove = 1;
	int maxChar = min((int)allStrings[cursorLineIndex].size(), maxCharactersPerString);
	if (isThisATab(cursorPos) >= 0) {
		numPosToMove = TABSIZE;
	}
	else if (ctlPressed) {
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
	if (cursorPos >= maxChar-1) {
		// if the string is still too long to display
		if (((int)allStrings[cursorLineIndex].size()-allStringStartPos[cursorLineIndex]) > maxCharactersPerString) {
			allStringStartPos[cursorLineIndex] += numPosToMove;
			// place the cursor back so that it doesn't fall on the ">" character
			cursorPos -= numPosToMove;
			arrowCursorPos = cursorPos;
		}
	}
	// if we're at the end of the string
	if (cursorPos > maxChar) {
		if (cursorLineIndex < lineCount) {
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
	if (shiftPressed) {
		if (!highlightManyChars && numPosToMove > 0) {
			highlightManyCharsStart = cursorPos - numPosToMove;
			highlightManyCharsLineIndex = cursorLineIndex;
		}
		setHighlightManyChars(highlightManyCharsStart, cursorPos,
													highlightManyCharsLineIndex, cursorLineIndex);
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
	didWeLandOnBracket();
}

//--------------------------------------------------------------
void Editor::leftArrow(){
	int numPosToMove = 1;
	if (isThisATab(cursorPos-TABSIZE) >= 0) {
		numPosToMove = TABSIZE;
	}
	if (ctlPressed) {
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
		if (cursorLineIndex > 0) {
			cursorLineIndex--;
			cursorPos = arrowCursorPos = min((int)allStrings[cursorLineIndex].size(), maxCharactersPerString);
			allStringStartPos[cursorLineIndex] = (int)allStrings[cursorLineIndex].size() - maxCharactersPerString;
		}
		else {
			cursorPos = arrowCursorPos = 0;
		}
	}
	else {
		if ((int)allStrings[cursorLineIndex].size() > maxCharactersPerString) {
			if (cursorPos <= 0 && allStringStartPos[cursorLineIndex] > 0) {
				cursorPos += numPosToMove;
				arrowCursorPos = cursorPos;
				allStringStartPos[cursorLineIndex] -= numPosToMove;
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
			if (ctlPressed) highlightManyCharsStart += numPosToMove;
		}
		setHighlightManyChars(highlightManyCharsStart, cursorPos,
													highlightManyCharsLineIndex, cursorLineIndex);
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
	didWeLandOnBracket();
}

//--------------------------------------------------------------
void Editor::pageUp(){
	int prevCursorPos = cursorPos;
	int prevCursorLineIndex = cursorLineIndex;
	if (cursorLineIndex == 0) {
		cursorPos = 0;
		if (highlightManyChars) highlightManyChars = false;
		return;
	}
	cursorLineIndex -= (maxNumLines+1);
	lineCountOffset -= (maxNumLines+1);
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
	didWeLandOnBracket();
}

//--------------------------------------------------------------
void Editor::pageDown(){
	int prevCursorPos = cursorPos;
	int prevCursorLineIndex = cursorLineIndex;
	if (cursorLineIndex == lineCount) {
		cursorPos = maxCursorPos();
		if (highlightManyChars) highlightManyChars = false;
		return;
	}
	cursorLineIndex += (maxNumLines+1);
	lineCountOffset += (maxNumLines+1);
	if (cursorLineIndex > lineCount) cursorLineIndex = lineCount;
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
	didWeLandOnBracket();
}

//--------------------------------------------------------------
void Editor::allOtherKeys(int key){
	bool executing = false;
	bool lineBreaking = true;
	bool callAssemble = true;
	if (ctlPressed) {
		if (key == 13) {
			// check if we're inside curly brackets so we can execute without using arrow keys
			int closingBracketIndex = areWeInBetweenBrackets(false);
			if (closingBracketIndex >= 0) {
				// set the number of lines to execute
				setHighlightManyChars(bracketIndexes[closingBracketIndex][1], bracketIndexes[closingBracketIndex][3],
									  bracketIndexes[closingBracketIndex][0], bracketIndexes[closingBracketIndex][2]);
			}
			setExecutingLine();
			allocateExecutionMem();
			lineBreaking = false;
			executing = true;
		}
		else if(key == 61) { // + (actually =)
			if (altPressed) {
				((ofApp*)ofGetAppPtr())->brightness++;
				if (((ofApp*)ofGetAppPtr())->brightness > 255) ((ofApp*)ofGetAppPtr())->brightness = 255;
			}
			else {
				fontSize += 2;
				setGlobalFontSize(fontSize);
			}
			callAssemble = false;
		}
		else if (key == 45) { // -
			if (altPressed) {
				((ofApp*)ofGetAppPtr())->brightness--;
				if (((ofApp*)ofGetAppPtr())->brightness < 0) ((ofApp*)ofGetAppPtr())->brightness = 0;
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
        		saveFile();
      		}
      		callAssemble = false;
    	}
    	else if (key == 83 && shiftPressed) { // S with shift pressed
      		saveFile();
      		callAssemble = false;
    	}
    	else if (key == 111) { // o for open
      		loadFile();
      		callAssemble = false;
    	}
	}
	else if (shiftPressed) {
		if (key == 13) {
			// as with holding the Ctl key, do the same here
			// but position the cursor at the closing bracket
			int closingBracketIndex = areWeInBetweenBrackets(false);
			if (closingBracketIndex >= 0) {
				cursorLineIndex = bracketIndexes[closingBracketIndex][2];
				cursorPos = bracketIndexes[closingBracketIndex][3];
				// set the number of lines to execute
				setHighlightManyChars(bracketIndexes[closingBracketIndex][1], cursorPos,
									  bracketIndexes[closingBracketIndex][0], cursorLineIndex);
			}
			setExecutingLine();
			allocateExecutionMem();
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
void Editor::setExecutingLine(){
	executingLine = cursorLineIndex;
}

//--------------------------------------------------------------
vector<int> Editor::setSelectedStrStartPosAndSize(int i){
	vector<int> v;
	if (i == topLine) {
		if (i == bottomLine) {
			v.push_back(firstChar);
			v.push_back(lastChar - firstChar);
		}
		else {
			if (highlightManyCharsLineIndex < cursorLineIndex) {
				v.push_back(highlightManyCharsStart);
			}
			else {
				v.push_back(cursorPos);
			}
			v.push_back(allStrings[i].size());
		}
	}
	else if (i == bottomLine) {
		v.push_back(0);
		if (highlightManyCharsLineIndex < cursorLineIndex) {
			v.push_back(cursorPos);
		}
		else {
			v.push_back(highlightManyCharsStart);
		}
	}
	else {
		v.push_back(0);
		v.push_back(allStrings[i].size());
	}
	return v;
}

//--------------------------------------------------------------
void Editor::copyString(){
	strForClipBoard = "";
	if (highlightManyChars) {
		for (int i = topLine; i <= bottomLine; i++) {
			vector<int> posAndSize = setSelectedStrStartPosAndSize(i);
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
void Editor::pasteString(){
	strFromClipBoard = ofGetWindowPtr()->getClipboardString();
	vector<string> tokens = tokenizeString(strFromClipBoard, "\n");
	// if we paste in the middle of a string
	// the last line must concatenate the pasted string with the remainding string
	// of the line we broke in the middle
	string remaindingStr = "";
	// we must also keep track of the remainng tabs
	vector<int> remainingTabs;
	for (unsigned i = 0; i < tokens.size(); i++) {
		vector<int> tabsOnToken = findIndexesOfCharInStr(tokens[i], "\t");
		tokens[i] = replaceCharInStr(tokens[i], "\t", tabStr);
		if (i == 0) {
			remaindingStr = allStrings[cursorLineIndex].substr(cursorPos+allStringStartPos[cursorLineIndex]);
			allStrings[cursorLineIndex] = allStrings[cursorLineIndex].substr(0,
																		cursorPos+allStringStartPos[cursorLineIndex]);
			allStrings[cursorLineIndex] += tokens[i];
		}
		if (i == (tokens.size()-1)) {
			// if we're pasting one line only just append the remaining string
			if (i == 0) {
				allStrings[cursorLineIndex] += remaindingStr;
			}
			// otherwise concatenate the token with the remaining
			else {
				allStrings[cursorLineIndex] = tokens[i]+remaindingStr;
			}
		}
		if ((i > 0) && (i < (tokens.size()-1))) {
			allStrings[cursorLineIndex] = tokens[i];
		}
		if (i < (tokens.size()-1)) {
			allocateNewLineMem("");
			postIncrementOnNewLine();
		}
	}
	cursorPos += tokens[tokens.size()-1].size();
	scanStrings();
}

//--------------------------------------------------------------
void Editor::deleteString(){
	lineCount -= (bottomLine - topLine);
	for (int i = topLine; i <= bottomLine; i++) {
		if ((i > topLine) && (i < bottomLine)) {
			releaseMemOnBackspace(i);
		}
		else {
			vector<int> posAndSize = setSelectedStrStartPosAndSize(i);
			//allStrings[i].erase(posAndSize[0], posAndSize[1]);
			if (i == topLine) {
				clearLineVectors(i, posAndSize[0], posAndSize[1]);
			}
			else if (i == bottomLine) {
				allStrings[i].erase(posAndSize[0], posAndSize[1]);
				if (allStrings[i].size() == 0) {
					releaseMemOnBackspace(i);
				}
				else {
					// if the string is not entirely deleted
					allStringStartPos[i] = 0;
					// if the last line is not deleted entirely, get one line back in the count
					lineCount++;
				}
			}
		}
	}
	if (cursorLineIndex > highlightManyCharsLineIndex) {
		cursorLineIndex = highlightManyCharsLineIndex;
		cursorPos = highlightManyCharsStart;
	}
}

//--------------------------------------------------------------
vector<int> Editor::sortVec(vector<int> v){
	sort(v.begin(), v.end());
	return v;
}

//--------------------------------------------------------------
void Editor::setTraceback(string str, int lineNum){
	tracebackStr[lineNum] = str;
	tracebackTimeStamp.push_back(ofGetElapsedTimeMillis());
	tracebackTempIndex.push_back(lineNum);
}

//--------------------------------------------------------------
void Editor::releaseTraceback(int lineNum){
	if (tracebackStr[lineNum].size() > 0) {
		tracebackStr[lineNum] = "";
	}
}

//--------------------------------------------------------------
void Editor::fromOscPress(int ascii){
  if (!isActive) {
    // int tempCursorLineIndex = cursorLineIndex;
    // int tempCursorPos = cursorPos;
    // cursorLineIndex = lineCount;
    // cursorPos = maxCursorPos();
    // allOtherKeys(ascii);
    // cursorLineIndex = tempCursorLineIndex;
    // cursorPos = tempCursorPos;

	// call overloaded keyPressed function to set a temporary whichEditor in ofApp.cpp
	((ofApp*)ofGetAppPtr())->keyPressed(ascii, objID);
  }
  else {
	((ofApp*)ofGetAppPtr())->keyPressed(ascii);
  }
}

//--------------------------------------------------------------
void Editor::fromOscRelease(int ascii){
  if (!isActive) {
    // int tempCursorLineIndex = cursorLineIndex;
    // int tempCursorPos = cursorPos;
    // cursorLineIndex = lineCount;
    // cursorPos = maxCursorPos();
    // allOtherKeys(ascii);
    // cursorLineIndex = tempCursorLineIndex;
    // cursorPos = tempCursorPos;

	// the same applies to releasing a key
	((ofApp*)ofGetAppPtr())->keyReleased(ascii, objID);
  }
  else {
	((ofApp*)ofGetAppPtr())->keyReleased(ascii);
  }
}

//--------------------------------------------------------------
void Editor::saveFile(){
  ofFileDialogResult saveFileResult = ofSystemSaveDialog("LiveLily_session.lyv", "Save file");
  if (saveFileResult.bSuccess){
    ofstream file(saveFileResult.filePath.c_str());
    if (!file.is_open()) {
      cout << "Cannot open file\n";
      return;
    }
    for (unsigned i = 0; i < allStrings.size(); i++) {
      file << allStrings[i] << "\n";
    }
    file.close();
    cout << "File saved\n";
  }
}

//--------------------------------------------------------------
void Editor::saveExistingFile(){
  ofstream file(loadedFileStr);
  if (!file.is_open()) {
    cout << "Cannot open file\n";
    return;
  }
  for (unsigned i = 0; i < allStrings.size(); i++) {
    file << allStrings[i] << "\n";
  }
  file.close();
  cout << "File saved\n";
}

//--------------------------------------------------------------
void Editor::loadFile(){
  //Open the Open File Dialog
  ofFileDialogResult openFileResult = ofSystemLoadDialog("Select a .lyv file");
  //Check if the user opened a file
  if (openFileResult.bSuccess){
    allStrings.clear();
    allStringStartPos.clear();
    lineCount = 0;
    cursorPos = 0;
    ifstream file(openFileResult.filePath.c_str());
    if (!file.is_open()) {
      cout << "Cannot open file\n";
      return;
    }
    string line;
	allStrings.clear();
    while (getline(file, line)) {
      allStrings.push_back(line);
    }
	lineCount = (int)allStrings.size();
	cursorLineIndex = 0;
	cursorPos = arrowCursorPos = 0;
	scanStrings();
    cout << "Opened file\n";
    file.close();
    fileLoaded = true;
    loadedFileStr = openFileResult.filePath.c_str();
  }
}

//--------------------------------------------------------------
void Editor::printVector(vector<int> v){
	for (unsigned i = 0; i < v.size(); i++) {
		cout << v[i] << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
void Editor::printVector(vector<string> v){
	for (unsigned i = 0; i < v.size(); i++) {
		cout << v[i] << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
void Editor::printVector(vector<float> v){
	for (unsigned i = 0; i < v.size(); i++) {
		cout << v[i] << " ";
	}
	cout << endl;
}
