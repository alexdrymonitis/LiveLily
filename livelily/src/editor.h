#ifndef EDITOR_H
#define EDITOR_H

#include "ofMain.h"
#include "ofxOsc.h"
#include <vector>
#include <string>
#include <fstream>
#include <utility> // to add pair and make_pair

#define TABSIZE 4
#define EXECUTIONDUR 700
#define EXECUTIONRAMPSTART 200
#define EXECUTIONBRIGHTNESS 220 // transparency for the executing rect
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
		//// disable copying (because of std::ifstream)
		//Editor(const Editor&) = delete;
		//Editor& operator=(const Editor&) = delete;
		//// add move constructor
		//Editor(Editor&& other) noexcept
		//	: file(std::move(other.file)) // transfer ownership of stream
		//{
		//	// move other state here if you have any
		//}
		//// add move assignment operator
		//Editor& operator=(Editor&& other) noexcept {
		//	if (this != &other) {
		//		file = std::move(other.file); // transfer ownership
		//		// move other state here if you have any
		//	}
		//	return *this;
		//}
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
		std::string getLine(int lineNdx);
		int getCursorLineIndex();
		void setActivity(bool activity);
		bool isActive();
		int getNumTabsInStr(std::string str);
		std::vector<int> getNestDepth();
		int findTopLine(int startLine);
		std::vector<int> findBraceForward(std::map<int, std::string>::iterator start, std::map<int, std::string>::iterator finish);
		std::vector<int> findBraceBackward();
		std::vector<int> findEnclosingBraceIndexes();
		// get the number of digits of line count, used in the text drawing function
		int getNumDigitsOfLineCount();
		void drawText();
		void drawCursor(int cursorX, ofColor cursorColor);
		void drawPaneSeparator();
		void setStringsStartPos();
		void setFrameWidth(float frameWidth);
		void setFrameHeight(float frameHeight);
		float getFrameHeight();
		void setFrameXOffset(float xOffset);
		void setFrameYOffset(float yOffset);
		float getFrameYOffset();
		// max number of lines that fit in the window
		void setMaxCharactersPerString();
		void resetCursorPos(int oldMaxCharactersPerString);
		void setFontSize(int fontSize, float oneCharacterWidth, float cursorHeight);
		int getHalfCharacterWidth();
		void setMaxNumLines(int numLines);
		int getMaxNumLines();
		void offsetMaxNumLines(int offset);
		void resetMaxNumLines();
		// newline functions
		void postIncrementOnNewLine();
		void createNewLine(std::string str, int increment);
		void moveLineNumbers(int numLines);
		void moveDataToNextLine();
		void copyOnLineDelete();
		void newLine();
		void moveCursorOnShiftReturn();
		bool changeMapKey(std::map<int, std::string> *m, int key, int increment, bool createNonExisting);
		void changeMapKey(std::map<int, int> *m, int key, int increment, bool createNonExisting);
		void changeMapKey(std::map<int, uint64_t> *m, int key, int increment, bool createNonExisting);
		void changeMapKey(std::map<int, bool> *m, int key, int increment, bool createNonExisting);
		void changeMapKeys(int key, int increment); // to change the keys of nine std::maps with one function call
		void eraseMapKey(std::map<int, std::string> *m, int key);
		void eraseMapKey(std::map<int, int> *m, int key);
		void eraseMapKey(std::map<int, uint64_t> *m, int key);
		void eraseMapKey(std::map<int, bool> *m, int key);
		void eraseMapKeys(int key); // to erase the keys of nine maps with one function call
		void connectLineToBar(int lineNdx, int instNdx, int barNdx);
		int getLineConnectedToBar(int instNdx, int barNdx);
		void setActiveLineElement(int lineNdx, int elementNdx);
		void setAnimation(bool state);
		// functions to handle horizontal tabs
		bool isThisATab(int pos);
		int didWeLandOnATab();
		int getTabSize();
		int maxCursorPos();
		void setCursorPos(int pos);
		bool startsWith(std::string a, std::string b);
		bool endsWith(std::string a, std::string b);
		// assemble strings from keyboard input
		void assembleString(int key, bool executing, bool lineBreaking);
		// overloaded function for all characters except from backspace, return and delete
		void assembleString(int key);
		// set entire string to a line (used for remote typing)
		void setString(std::string s);
		// set variables for highlighting many characters
		// for executing multiple lines, copying, pasting, deleting, cutting
		void setHighlightManyChars(int charPos1, int charPos2, int charLine1, int charLine2);
		bool isNumber(std::string str);
		bool isFloat(std::string str);
		// std::string handling
		std::string replaceCharInStr(std::string str, std::string a, std::string b);
		std::vector<std::string> tokenizeString(std::string str, std::string delimiter, bool addDelimiter);
		std::vector<int> findIndexesOfCharInStr(std::string str, std::string charToFind);
		std::vector<int> setSelectedStrStartPosAndSize(int i);
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
		bool isShiftPressed();
		bool isCtrlPressed();
		bool isAltPressed();
		bool getInserting();
		void setVisualMode(bool visualModeBool);
		bool getVisualMode();
		void detectExecutingChunk();
		void allOtherKeys(int key);
		void replaceCmdStrNewlines();
		bool showingShell();
		bool isTypingShell();
		void setTypingShell(bool typingShellBool);
		void setShowingShell(bool showingShellBool);
		// copying, pasting, deleting std::strings
		void copyString(int copyTo, bool dehighlight = true);
		void pasteString(int pasteFrom);
		void clearText();
		void deleteString();
		std::vector<int> sortVec(std::vector<int> v);
		// traceback functions
		void setTraceback(int errorCode, std::string errorStr, int lineNum, size_t strBreakPnt=0);
		void releaseTraceback(int lineNum);
		std::string getTracebackStr(int lineNum);
		int getTracebackColor(int lineNum);
		uint64_t getTracebackTimeStamp(int lineNum);
		int getTracebackNumLines(int lineNum);
		uint64_t getTracebackDur();
		std::map<int, uint64_t>::iterator getTracebackTimeStampsBegin();
		std::map<int, uint64_t>::iterator getTracebackTimeStampsEnd();
		// receive single characters from OSC
		void fromOscPress(int ascii);
		void fromOscRelease(int ascii);
		// save and load files
		void saveDialog();
		void saveFile(std::string fileName);
		void saveExistingFile();
		size_t findChordEnd(std::string str);
		size_t findChordStart(std::string str);
		void loadXMLFile(std::string filePath);
		void loadTextFile(std::string filePath);
		void loadDialog();
		void loadFile(std::string fileName);
		//bool isFileOpen();
		//void closeFile();
		// misc
		float getCursorHeight();
		// debugging
		void printVector(std::vector<int> v);
		void printVector(std::vector<std::string> v);
		void printVector(std::vector<float> v);

		int thisLang;
		std::string lyvDelimiter = "{ }*.\"";
		std::string pyDelimiter = "{ }[]():*.,\"'=/";
		std::string luaDelimiter = "[]"; // random stuff for now
		std::string delimiters[3] = {lyvDelimiter, pyDelimiter, luaDelimiter};

		int paneRow;
		int paneCol;

		float frameWidth;
		float frameHeight;

		bool activity;

		bool fileLoaded;
		std::string loadedFileStr;
		std::string loadedFileFullPath;
		std::string defaultFileNames[3] = {"untitled.lyv", "untitled.py", "untitled.lua"};
		bool couldNotLoadFile;
		bool couldNotSaveFile;
		uint64_t fileLoadErrorTimeStamp;

		ofxOscSender oscKeys; // for sending key strokes to another LiveLily editor

		uint64_t executionRampStart;

		bool inserting;
		bool visualMode;
		bool typingShell;
		bool showShell;

		ofTrueTypeFont font;
		int fontSize;
		bool fontLoaded;
		std::map<int, std::string> allStrings;
		std::map<int, int> allStringStartPos;
		std::map<int, std::vector<int>> allStringTabs;
		std::map<int, std::vector<int>> bracketIndexes;
		std::string tabStr; // to not having to write "    " every time

		// variables to connect lines to bars for animating the editor
		bool animationState;
		std::map<int, int> linesConnectedToBar;
		std::map<int, std::map<int, int>> instsConnectedToLine;
		std::map<int, int> activeLineElements;

		// variables for highlighting pairs of curly brackets
		bool highlightBracket;
		int bracketHighlightRectX;
		int bracketHighlightRectY;
		std::string highlightedBracketStr;

		std::string highlightManyCharsStr;
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
		// the std::string on its line goes from long to fitting
		int highlightedCharIndex;
		// and a boolean to check whether a std::string has just got small enough to fit the window
		bool stringExceededWindow;
		//STDERR, keys are line numbers
		std::map<int, std::string> tracebackStr;
		std::map<int, int> tracebackColor;
		std::map<int, int> tracebackNumLines;
		std::map<int, uint64_t> tracebackTimeStamps;
		std::map<int, size_t> tracebackStrBreakPnt;
		// variable to dim the execution rectangle
		std::map<int, bool> executingLines;
		std::map<int, int> executionDegrade;
		std::map<int, uint64_t> executionTimeStamp;
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
		bool ctrlPressed;
		bool shiftPressed;
		bool altPressed;
		std::string fromOscStr;
		std::string yankedStr;
		//std::ifstream file;
};

#endif
