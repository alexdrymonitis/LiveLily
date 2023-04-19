#pragma once

#include "ofMain.h"

#define TABSIZE 4
#define EXECUTIONDUR 1500
#define EXECUTIONRAMPSTART 1000
#define EXECUTIONBRIGHTNESS 190 // transparency for the executing rect

class Editor
{
  public:
    Editor();
    // set the ID of each editor
    void setID(int id);
    // scan the strings to locate special characters
    void scanStrings();
    void didWeLandOnBracket();
    int areWeInBetweenBrackets(bool returnNestDepth);
    // get the number of digits of line count, used in the text drawing function
    int getNumDigitsOfLineCount();
    void drawText();
    void setStringsStartPos();
		// max number of lines that fit in the window
    void setMaxCharactersPerString();
    void setGlobalFontSize(int fontSize);
    void resetCursorPos(int oldMaxCharactersPerString);
    void setFontSize(int fontSize);
    void setMaxNumLines();
    // newline functions
    void newLine();
    void postIncrementOnNewLine();
    void moveCursorOnShiftReturn();
    // functions to handle horizontal tabs
    int isThisATab(int pos);
    bool deletingTab(int pos);
    int didWeLandOnATab();
    int maxCursorPos();
    // assemble strings from keyboard input
    void assembleString(int key, bool executing, bool lineBreaking);
    // overloaded function for all characters except from backspace, return and delete
    void assembleString(int key);
    // memory handling (vector push_back and erase) for various functionalities
    void allocateExecutionMem();
    void allocateNewLineMem(string str);
    void releaseMemOnBackspace(int lineIndex);
    void clearLineVectors(int lineIndex, int start, int length);
    void copyMemOnBackspace();
    void insertNewLineMem();
    // set variables for highlighting many characters
		// for executing multiple lines, copying, pasting, deleting, cutting
    void setHighlightManyChars(int charPos1, int charPos2, int charLine1, int charLine2);
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
    void allOtherKeys(int key);
    void setExecutingLine();
    // copying, pasting, deleting strings
    void copyString();
    void pasteString();
    void deleteString();
    vector<int> sortVec(vector<int> v);
    // traceback functions
    void setTraceback(string str, int lineNum);
    void releaseTraceback(int lineNum);
    // receive text from OSC
    void fromOscPress(int ascii);
    void fromOscRelease(int ascii);
    // save and load files
    void saveFile();
    void saveExistingFile();
    void loadFile();
    // debugging
		void printVector(vector<int> v);
		void printVector(vector<string> v);
		void printVector(vector<float> v);

    int frameWidth;
    int frameHeight;

    bool isActive;

    bool fileLoaded;
    string loadedFileStr;

    uint64_t executionRampStart;

    bool ctlPressed;
		bool shiftPressed;
		bool altPressed;

    ofTrueTypeFont font;
    int fontSize;
    vector<string> allStrings;
    vector<int> allStringStartPos;
    vector<vector<int>> allStringTabs;
		vector<vector<int>> bracketIndexes;
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

    int cursorHeight;
    int cursorLineIndex; // Y position of cursor
		int cursorPos; // X position of cursor
    int arrowCursorPos; // X position for when we navigate with the arrow keys

    int maxNumLines;
    int frameXOffset;
    int frameYOffset;
    int lineNumberWidth;
    int lineCount;
    int lineCountOffset;
		int oneCharacterWidth;
		int halfCharacterWidth;
		int oneAndHalfCharacterWidth;
		int maxCharactersPerString;
		int letterHeightDiff;
    // store last highlighted char to properly position the cursor when
    // the string on its line goes from long to fitting
    int highlightedCharIndex;
    // and a boolean to check whether a string has just got small enough to fit the window
    bool stringExceededWindow;
    //STDERR
		vector<string> tracebackStr;
		vector<uint64_t> tracebackTimeStamp;
		vector<int> tracebackTempIndex;
    // variable to dim the execution rectangle
		vector<int> executeLine;
		vector<int> executionDegrade;
		vector<uint64_t> executionTimeStamp;
		float executionStepPerMs;
		vector<int> executionLineIndex;
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
};
