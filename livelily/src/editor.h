#ifndef EDITOR_H
#define EDITOR_H

#include "ofMain.h"
#include <vector>

#define TABSIZE 4
#define EXECUTIONDUR 1500
#define EXECUTIONRAMPSTART 1000
#define EXECUTIONBRIGHTNESS 190 // transparency for the executing rect
#define FILE_LOAD_ERROR_DUR 2000

class Editor
{
	public:
		Editor();
		// set the ID of each editor
		void setID(int id);
		void setPassed(bool passed);
		bool hasPassed();
		void setLanguage(int langNdx);
		int getLanguage();
		void setPaneRow(int row);
		void setPaneCol(int col);
		int getPaneRow();
		int getPaneCol();
		string getLine(int lineNdx);
		int getCursorLineIndex();
		void setActivity(bool activity);
		bool isActive();
		int getNumTabsInStr(string str);
		vector<int> getNestDepth();
		int findTopLine(int startLine);
		vector<int> findBraceForward(map<int, string>::iterator start, map<int, string>::iterator finish);
		vector<int> findBraceBackward();
		vector<int> findEnclosingBraceIndexes();
		// get the number of digits of line count, used in the text drawing function
		int getNumDigitsOfLineCount();
		void drawText();
		void setStringsStartPos();
		void setFrameWidth(int frameWidth);
		void setFrameHeight(int frameHeight);
		void setFrameXOffset(int xOffset);
		void setFrameYOffset(int yOffset);
		// max number of lines that fit in the window
		void setMaxCharactersPerString();
		void setGlobalFontSize(int fontSize);
		void resetCursorPos(int oldMaxCharactersPerString);
		void setFontSize(int fontSize, float cursorHeight);
		int getHalfCharacterWidth();
		void setMaxNumLines(int numLines);
		int getMaxNumLines();
		void offsetMaxNumLines(int offset);
		void resetMaxNumLines();
		// newline functions
		void postIncrementOnNewLine();
		void createNewLine(string str, int increment);
		void moveLineNumbers();
		void moveDataToNextLine();
		void copyOnLineDelete();
		void newLine();
		void moveCursorOnShiftReturn();
		bool changeMapKey(map<int, string> *m, int key, int increment, bool createNonExisting);
		void changeMapKey(map<int, int> *m, int key, int increment, bool createNonExisting);
		void changeMapKey(map<int, uint64_t> *m, int key, int increment, bool createNonExisting);
		void changeMapKey(map<int, bool> *m, int key, int increment, bool createNonExisting);
		void changeMapKeys(int key, int increment); // to change the keys of nine maps with one function call
		void eraseMapKey(map<int, string> *m, int key);
		void eraseMapKey(map<int, int> *m, int key);
		void eraseMapKey(map<int, uint64_t> *m, int key);
		void eraseMapKey(map<int, bool> *m, int key);
		void eraseMapKeys(int key); // to erase the keys of nine maps with one function call
		// functions to handle horizontal tabs
		bool isThisATab(int pos);
		int didWeLandOnATab();
		int getTabSize();
		int maxCursorPos();
		bool startsWith(string a, string b);
		bool endsWith(string a, string b);
		// assemble strings from keyboard input
		void assembleString(int key, bool executing, bool lineBreaking);
		// overloaded function for all characters except from backspace, return and delete
		void assembleString(int key);
		// memory handling (vector push_back and erase) for various functionalities
		void setExecutionVars();
		// set variables for highlighting many characters
		// for executing multiple lines, copying, pasting, deleting, cutting
		void setHighlightManyChars(int charPos1, int charPos2, int charLine1, int charLine2);
		bool isNumber(string str);
		// string handling
		string replaceCharInStr(string str, string a, string b);
		vector<string> tokenizeString(string str, string delimiter);
		vector<int> findIndexesOfCharInStr(string str, string charToFind);
		vector<int> setSelectedStrStartPosAndSize(int i);
		// set cursor and line indexes and line count offset
		// for up and down movement of the cursor
		bool setLineIndexesUpward(int lineIndex);
		bool setLineIndexesDownward(int lineIndex);
		// various specific keys functionalities
		void upArrow(int lineIndex);
		void downArrow(int lineIndex);
		void rightArrow();
		void leftArrow();
		void pageUp();
		void pageDown();
		void setShiftPressed(bool state);
		void setCtrlPressed(bool state);
		void setAltPressed(bool state);
		void allOtherKeys(int key);
		void setExecutingLine();
		// copying, pasting, deleting strings
		void copyString();
		void pasteString();
		void deleteString();
		vector<int> sortVec(vector<int> v);
		// traceback functions
		void setTraceback(std::pair<int, string> str, int lineNum);
		void releaseTraceback(int lineNum);
		string getTracebackStr(int lineNum);
		int getTracebackColor(int lineNum);
		uint64_t getTracebackTimeStamp(int lineNum);
		int getTracebackNumLines(int lineNum);
		uint64_t getTracebackDur();
		map<int, uint64_t>::iterator getTracebackTimeStampsBegin();
		map<int, uint64_t>::iterator getTracebackTimeStampsEnd();
		// receive single characters from OSC
		void fromOscPress(int ascii);
		void fromOscRelease(int ascii);
		// save and load files
		void saveFile();
		void saveExistingFile();
		void loadXMLFile(string filePath);
		void loadLyvFile(string filePath);
		void loadFile();
		// misc
		float getCursorHeight();
		// debugging
		void printVector(vector<int> v);
		void printVector(vector<string> v);
		void printVector(vector<float> v);

		int thisLang;

		int paneRow;
		int paneCol;

		int frameWidth;
		int frameHeight;

		bool activity;

		bool fileLoaded;
		string loadedFileStr;
		string defaultFileNames[3] = {"untitled.lyv", "untitled.py", "untitled.lua"};
		bool couldNotLoadFile;
		bool couldNotSaveFile;
		uint64_t fileLoadErrorTimeStamp;

		uint64_t executionRampStart;

		bool ctrlPressed;
		bool shiftPressed;
		bool altPressed;

		ofTrueTypeFont font;
		int fontSize;
		map<int, string> allStrings;
		map<int, int> allStringStartPos;
		map<int, vector<int>> allStringTabs;
		map<int, vector<int>> bracketIndexes;
		string tabStr; // to not having to write "    " every time
	
		// variables for highlighting pairs of curly brackets
		bool highlightBracket;
		int bracketHighlightRectX;
		int bracketHighlightRectY;
		string highlightedBracketStr;

		string strForClipBoard;
		string strFromClipBoard;
		string highlightManyCharsStr;
		// counters for auto-inserting quotes and curly brackets
		int quoteCounter;
		int curlyBracketCounter;
		int squareBracketCounter;
		int roundBracketCounter;

		float cursorHeight;
		int cursorLineIndex; // Y position of cursor
		int cursorPos; // X position of cursor
		int arrowCursorPos; // X position for when we navigate with the arrow keys

		int maxNumLines;
		int maxNumLinesReset;
		int frameXOffset;
		int frameYOffset;
		int lineNumberWidth;
		int lineCount;
		int lineCountOffset;
		int oneCharacterWidth;
		int halfCharacterWidth;
		int oneAndHalfCharacterWidth;
		int maxCharactersPerString;
		float characterOffset;
		// store last highlighted char to properly position the cursor when
		// the string on its line goes from long to fitting
		int highlightedCharIndex;
		// and a boolean to check whether a string has just got small enough to fit the window
		bool stringExceededWindow;
		//STDERR, keys are line numbers
		map<int, string> tracebackStr;
		map<int, int> tracebackColor;
		map<int, int> tracebackNumLines;
		map<int, uint64_t> tracebackTimeStamps;
		// variable to dim the execution rectangle
		map<int, bool> executingLines;
		map<int, int> executionDegrade;
		map<int, uint64_t> executionTimeStamp;
		float executionStepPerMs;
		// variable to set the first (or only) line to execute
		// useful in case we hit shift+return as the cursor will move
		// before the line is executed
		int executingLine;
		// used in case we're executing more than one lines
		// so as many rectangles as the executing lines are created
		// all with the same width
		int maxExecutionWidth;
		// for highlighting more than one line chosen with the arrow keys
		int topLine;
		int bottomLine;
		bool executingMultiple;
		bool highlightManyChars;
		int highlightManyCharsStart;
		int highlightManyCharsLineIndex;
		int firstChar;
		int lastChar;
	private:
		int objID;
		bool fileEdited;
		string fromOscStr;

		string xmlDurs[7];
};

#endif
