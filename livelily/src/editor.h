#ifndef EDITOR_H
#define EDITOR_H

#include "ofMain.h"
#include "ofxOsc.h"
#include <vector>

#define TABSIZE 4
#define EXECUTIONDUR 1500
#define EXECUTIONRAMPSTART 1000
#define EXECUTIONBRIGHTNESS 190 // transparency for the executing rect
#define FILE_LOAD_ERROR_DUR 2000

// structure copied from MIDIFileLoader.h
struct noteData
{
	float beatPosition;//in beats from beginning
	int pitch;//as MIDI note number
	double timeMillis;
	int ticks;
	int velocity;
	long  durationTicks;
	double durationMillis;
};

class Editor
{
	public:
		Editor();
		// set the ID of each editor
		void setID(int id);
		int getID();
		void setSessionActivity(bool activity);
		bool getSessionActivity();
		void setSendKeys(int paneNdx);
		bool getSendKeys();
		int getSendKeysPaneNdx();
		void setSendLines(int paneNdx);
		bool getSendLines();
		int getSendLinesPaneNdx();
		void setPassed(bool passed);
		bool hasPassed();
		void setLanguage(int langNdx);
		int getLanguage();
		void setAutocomplete(bool autocomp);
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
		void drawCursor(int cursorX, ofColor cursorColor);
		void drawPaneSeparator();
		void setStringsStartPos();
		void setFrameWidth(float frameWidth);
		void setFrameHeight(float frameHeight);
		void setFrameXOffset(float xOffset);
		void setFrameYOffset(float yOffset);
		float getFrameYOffset();
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
		void moveLineNumbers(int numLines);
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
		void setCursorPos(int pos);
		bool startsWith(string a, string b);
		bool endsWith(string a, string b);
		// assemble strings from keyboard input
		void assembleString(int key, bool executing, bool lineBreaking);
		// overloaded function for all characters except from backspace, return and delete
		void assembleString(int key);
		// set variables for highlighting many characters
		// for executing multiple lines, copying, pasting, deleting, cutting
		void setHighlightManyChars(int charPos1, int charPos2, int charLine1, int charLine2);
		bool isNumber(string str);
		bool isFloat(string str);
		// string handling
		string replaceCharInStr(string str, string a, string b);
		vector<string> tokenizeString(string str, string delimiter, bool addDelimiter);
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
		void setInserting(bool insertingBool);
		bool getInserting();
		void setVisualMode(bool visualModeBool);
		bool getVisualMode();
		void setInsertingMoveCursor();
		void detectExecutingChunk();
		void allOtherKeys(int key);
		void typeCommand(int key);
		void executeCommand();
		bool showingCommand();
		bool isTypingCommand();
		ofColor getCommandStrColor();
		string getCommandStr();
		// copying, pasting, deleting strings
		void copyString(int copyTo);
		void pasteString(int pasteFrom);
		void deleteString();
		vector<int> sortVec(vector<int> v);
		// traceback functions
		void setTraceback(int errorCode, string errorStr, int lineNum);
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
		void saveDialog();
		void saveFile(string fileName);
		void saveExistingFile();
		size_t findChordEnd(string str);
		size_t findChordStart(string str);
		void loadXMLFile(string filePath);
		void loadLyvFile(string filePath);
		void loadDialog();
		void loadFile(string fileName);
		// misc
		float getCursorHeight();
		// debugging
		void printVector(vector<int> v);
		void printVector(vector<string> v);
		void printVector(vector<float> v);

		int thisLang;
		string lyvDelimiter = "{ }*.\"";
		string pyDelimiter = "{ }[]():*.,\"'=/";
		string luaDelimiter = "[]"; // random stuff for now
		string delimiters[3] = {lyvDelimiter, pyDelimiter, luaDelimiter};

		int paneRow;
		int paneCol;

		float frameWidth;
		float frameHeight;

		bool activity;

		bool fileLoaded;
		string loadedFileStr;
		string defaultFileNames[3] = {"untitled.lyv", "untitled.py", "untitled.lua"};
		bool couldNotLoadFile;
		bool couldNotSaveFile;
		uint64_t fileLoadErrorTimeStamp;

		ofxOscSender oscKeys; // for sending key strokes to another LiveLily editor
		
		uint64_t executionRampStart;

		bool ctrlPressed;
		bool shiftPressed;
		bool altPressed;

		bool inserting;
		bool visualMode;
		bool typingCommand;
		bool showCommand;
		ofColor commandStrColor;
		string commandStr;

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
		float frameXOffset;
		float frameYOffset;
		int lineNumberWidth;
		int lineCount;
		int lineCountOffset;
		int oneCharacterWidth;
		int halfCharacterWidth;
		int oneAndHalfCharacterWidth;
		int maxCharactersPerString;
		size_t maxBacktraceChars;
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
		bool autobrackets;
		bool activeSession;
		bool sendKeys;
		int sendKeysPaneNdx;
		bool sendLines;
		int sendLinesPaneNdx;
		string fromOscStr;
		string yankedStr;
};

#endif
