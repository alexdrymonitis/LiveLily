#ifndef EDITOR_H
#define EDITOR_H

#include "ofMain.h"
#include "osc.h"
#include <vector>
#include <string>
#include <fstream>
#include <utility> // to add pair and make_pair
#include "utils.h"

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

// structure holding data for each line of the editor
struct line
{
	std::string str;
	int startPos;
	std::string tracebackStr;
	int tracebackColor;
	int tracebackNumLines;
	uint64_t tracebackTimeStamp;
	size_t tracebackStrBreakPnt;
	// variable to dim the execution rectangle
	bool isBeingExecuted;
	int executionDegrade;
	uint64_t executionTimeStamp;
	int linesConnectedToBar;
	int activeLineElements;
	// an id that stays with the line when lines above it are added or removed, -1 if it has none yet
	// used to find the tokens of the notes of a bar in the line, see EditorNoteSpan
	int id = -1;
};

// the place of a token in a line of an editor, for highlighting the token while its note plays (see \editor.animate)
// the line is found by its id, as its index changes when lines above it are added or removed
// and the token is highlighted only if the line still has the same text at that place
struct EditorNoteSpan
{
	int pane;
	int lineId;
	int start;
	int length;
	std::string text;
};

// the state of the text of an editor, stored for undo and redo
struct EditorSnapshot
{
	std::map<int, line> lines;
	int lineCount;
	int lineCountOffset;
	int cursorLineIndex;
	int cursorPos;
};

// a chunk of LiveLily code (a definition with curly brackets, or a single line)
// and the first error in its curly brackets, if there is one
struct BracketChunk
{
	int top;
	int bottom;
	int errorLine; // -1 if the curly brackets of the chunk are fine
	std::string errorStr;
	bool finished; // false for a definition whose closing curly bracket is missing
};

// structure for binding keys to actions when in normal mode
struct binding
{
	std::vector<std::string> bindingVec;
	std::vector<std::string> actionVec;
	bool bindingKeyPressed;
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
		// turns the pop-up of autocompletion on and off (see updateCompletions()), with \editor.autocomplete on or off
		void setAutocompletion(bool state);
		void setPaneRow(int row);
		void setPaneCol(int col);
		int getPaneRow();
		int getPaneCol();
		std::string getLine(int lineNdx);
		int getCursorLineIndex();
		void setActivity(bool activity);
		bool isActive();
		int getNumTabsInStr(std::string str);
		// finding the code chunk to execute
		// LiveLily code chunks are defined by curly brackets, Python (and Lua) code chunks by indentation
		// these don't depend on the contents of the editor, so they are static, to be used by ofApp on function bodies
		static std::string bracketsInLine(const std::string& str);
		int scanBraces(const std::string& str, int depth);
		static bool isLyvCodeLine(const std::string& str);
		static std::string definitionCommand(const std::string& str);
		static void countUnmatchedBrackets(const std::string& str, int& openBrackets, int& closingBrackets);
		static std::vector<BracketChunk> findBracketChunksInLines(const std::vector<std::string>& lines);
		std::vector<BracketChunk> findBracketChunks(int firstLine, int lastLine);
		// returns the line of an unmatched curly bracket in the given lines (and sets errorStr), or -1 if there is none
		int findUnmatchedBracket(int firstLine, int numLines, std::string& errorStr);
		int getBraceDepth(int lineNdx);
		std::pair<int, int> findBraceChunk();
		bool isBlankOrComment(const std::string& str);
		bool isIndentedLine(const std::string& str);
		bool isDecoratorLine(const std::string& str);
		bool continuesPreviousBlock(const std::string& str);
		bool scanPythonLine(const std::string& str, int& brackets, std::string& tripleQuote);
		std::map<int, bool> findContinuationLines();
		std::pair<int, int> findIndentChunk();
		// returns the top and bottom lines of the chunk the cursor is in (the same line if it's a single line)
		std::pair<int, int> findExecutingChunk();
		// the bracket that matches the one at a position of a line, returns false if there is none
		bool findMatchingBracket(int lineNdx, int pos, int& matchLine, int& matchPos);
		// where a comment starts in a line (its length if it has none), and the bracket at a position (0 if none), see findMatchingBracket()
		int commentStartOf(const std::string& str);
		char bracketAt(const std::string& str, int pos, int commentStart);
		std::vector<int> findEnclosingBraceIndexes();
		// get the number of digits of line count, used in the text drawing function
		int getNumDigitsOfLineCount();
		void drawText();
		void drawCursor(int cursorX, ofColor cursorColor);
		void drawPaneSeparator();
		void setDPressed(bool dPressedState);
		void setStringsStartPos();
		void setFrameWidth(float frameWidth);
		void setFrameHeight(float frameHeight);
		float getFrameHeight();
		// the position and size of the pane, and the position of the cursor on the screen, for moving between panes (see ofApp::moveToPane())
		float getFrameWidth() { return frameWidth; }
		float getFrameXOffset() { return frameXOffset; }
		int getCursorScreenX() { return cursorScreenX; }
		int getCursorScreenY() { return cursorScreenY; }
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
		// the number of lines of the pane (with the line of the file name) that setMaxNumLines() set, before any offset (see offsetMaxNumLines())
		int getMaxNumLinesReset() { return maxNumLinesReset; }
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
		void changeMapKeys(int key, int increment); // to change the keys of nine std::maps with one function call
		void eraseMapKeys(int key); // to erase the keys of nine maps with one function call
		void connectLineToBar(int lineNdx, int instNdx, int barNdx);
		int getLineConnectedToBar(int instNdx, int barNdx);
		void setActiveLineElement(int lineNdx, int elementNdx);
		void setAnimation(bool state);
		// the id of a line (see line::id), which the line gets if it doesn't have one yet
		int getLineId(int lineNdx);
		// functions to handle horizontal tabs
		bool isThisATab(int pos);
		int didWeLandOnATab();
		int getTabSize();
		int maxCursorPos();
		void setCursorPos(int pos);
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
		void undo();
		void redo();
		// long lines scroll horizontally by default, and with :set wrap they wrap around the edge of the pane (:set wrap! switches back)
		void setWrapLines(bool wrap);
		bool getWrapLines();
		// a new file has its own history
		void clearUndoHistory();
		bool isShiftPressed();
		bool isCtrlPressed();
		bool isAltPressed();
		bool getInserting();
		void setVisualMode(bool visualModeBool);
		// visual modes as in Vim, 0 for characters (v), 1 for whole lines (V), 2 for a block of columns (Ctrl+v)
		// the selection goes from where it starts (its anchor) to the cursor (see updateModeSelection())
		void startVisualSelection(int mode);
		int getSelectionMode();
		// sets the selection of lines or of a block from its anchor to the cursor, after the cursor has moved
		void updateModeSelection();
		// D in normal mode, deletes from the cursor to the end of the line (and yanks it)
		void deleteToLineEnd();
		// dd in normal mode, deletes the line of the cursor (and yanks it as a line, so p pastes it below the line of the cursor)
		void deleteCurrentLine();
		void deleteVisualLines();
		void deleteLinesYanked(int top, int bottom);
		// after Return inside curly brackets, scrolls so that the closing bracket below the cursor stays visible
		void keepClosingBracketVisible();
		// I in normal mode, inserts at the first character of the line that is not a white space
		void insertAtLineStart();
		// I (Shift+i) after Ctrl+v, inserts at the start of the block in its first line, and when insert mode ends (with Esc)
		// the typed text is inserted in the other lines of the block too
		void startBlockInsert();
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
		void pasteStringUnrecorded(int pasteFrom);
		void clearText();
		void deleteString();
		void deleteStringUnrecorded();
		std::vector<int> sortVec(std::vector<int> v);
		// traceback functions
		void setTraceback(int errorCode, std::string errorStr, int lineNum, size_t strBreakPnt=0);
		void releaseTraceback(int lineNum);
		std::string getTracebackStr(int lineNum);
		int getTracebackColor(int lineNum);
		uint64_t getTracebackTimeStamp(int lineNum);
		void setTracebackTimeStamp(int lineNum, uint64_t stamp);
		int getTracebackNumLines(int lineNum);
		uint64_t getTracebackDur();
		std::map<int, line>::iterator getAllLinesBegin();
		std::map<int, line>::iterator getAllLinesEnd();
		// receive single characters from OSC
		void fromOscPress(int ascii);
		void fromOscRelease(int ascii);
		// save and load files
		void saveDialog();
		void saveFile(std::string fileName);
		void saveExistingFile();
		// whether the text has changes that are not saved, and the name of the file (untitled.lyv etc. if it has none)
		bool isFileEdited() { return fileEdited; }
		std::string getFileName() { return (fileLoaded ? loadedFileStr : defaultFileNames[thisLang]); }
		// writes the LiveLily code converted from a MusicXML or a MIDI file to the editor (see loadFile())
		void loadConvertedLines(const std::vector<std::string>& convertedLines);
		void loadTextFile(std::string filePath);
		void loadDialog();
		bool loadFile(std::string fileName);
		bool loadHelpFile(std::string fileName, int newPane);
		// opens the help of help(name) of Python (see Pyo::takeHelp()) in the pane, as loadHelpFile() does with a help file
		// the pane stays a Python pane, so the help is colored as Python code, and the help is closed as a help file is
		void loadHelpText(const std::string& name, const std::string& text);
		void closeHelpFile();
		bool isHelpFileOpen();
		// misc
		float getCursorHeight();
		// debugging
		void printVector(std::vector<int> v);
		void printVector(std::vector<std::string> v);
		void printVector(std::vector<float> v);

		int thisLang;
		std::string lyvDelimiter = "{ }[]*.\"";
		std::string pyDelimiter = "{ }[]():*.,\"'=/";
		std::string luaDelimiter = "[]"; // random stuff for now
		std::string delimiters[3] = {lyvDelimiter, pyDelimiter, luaDelimiter};

		int paneRow;
		int paneCol;

		float frameWidth;
		float frameHeight;

		bool activity;

		bool fileLoaded;
		bool tempFileLoaded;
		std::string tempFileFullPath;
		bool tempConvertedFile;
		// true for a file converted from MusicXML, until it is saved
		// such a file gets a .lyv name, and saving to it must not overwrite an existing .lyv file with the same name
		bool convertedFile;
		std::string loadedFileStr;
		// the error shown in place of the file name for a while, when a file can't be loaded or saved
		// kept apart from loadedFileStr, so that the name of the file in the editor is shown again afterwards
		std::string fileErrorStr;
		std::string tempFileName;
		std::string loadedFileFullPath;
		std::string defaultFileNames[3] = {"untitled.lyv", "untitled.py", "untitled.lua"};
		bool couldNotLoadFile;
		bool couldNotSaveFile;
		uint64_t fileLoadErrorTimeStamp;

		std::shared_ptr<OscSender> oscKeys = makeOscSender(); // for sending key strokes to another LiveLily editor

		uint64_t executionRampStart;

		bool inserting;
		bool visualMode;
		int selectionMode = 0; // see startVisualSelection()
		int selAnchorLine = 0;
		int selAnchorCol = 0;
		// the columns of a block selection (with the offset of long lines that scroll), the right one excluded
		int blockLeft = 0;
		int blockRight = 0;
		// an insertion in a block (see startBlockInsert())
		bool blockInserting = false;
		int blockInsertTop = 0;
		int blockInsertBottom = 0;
		int blockInsertCol = 0;
		size_t blockInsertLen = 0;
		// copies the text typed at the start of a block to its other lines
		void finishBlockInsert();
		// deletes whole lines, for V
		void deleteLines(int top, int bottom);
		bool typingShell;
		bool showShell;

		ofTrueTypeFont font;
		int fontSize;
		bool fontLoaded;
		std::map<int, line> allLines;
		// to open a help file without losing the editor's contents, we need a temporary map with the lines data
		std::map<int, line> tempAllLines;
		// undo and redo, with Ctrl+z and Ctrl+y in insert mode, and u and Ctrl+r in normal mode
		// every step stores the text as it was before a change, and changes that follow each other are one step
		// (e.g. typing a word), until a new line, a pause, moving the cursor, or changing mode
		std::vector<EditorSnapshot> undoStack;
		std::vector<EditorSnapshot> redoStack;
		std::vector<EditorSnapshot> tempUndoStack; // the history of the file while a help file is open
		std::vector<EditorSnapshot> tempRedoStack;
		EditorSnapshot pendingSnapshot;
		bool hasPendingSnapshot = false;
		bool undoGroupOpen = false;
		int undoGroupCursorLine = 0;
		int undoGroupCursorPos = 0;
		uint64_t undoGroupTimeStamp = 0;
		int changeDepth = 0; // changes can call other changes (e.g. setString() calls allOtherKeys()), only the outermost one is recorded
		// line wrapping (see setWrapLines())
		// when lines wrap, maxCharactersPerString is so large that lines never scroll horizontally
		// so startPos is always 0 and cursorPos is the position in the line, and wrapWidth is the number of characters in a row of the pane
		bool wrapLines = false;
		int wrapWidth = 1;
		// the rows of the first visible line that are above the top of the pane, for a line that is longer than the whole pane
		int wrapRowSkip = 0;
		// the row of the pane where the cursor is drawn, set in drawText()
		int cursorScreenRow = 0;
		// the number of rows a line takes in the pane
		int wrapRowsOf(int lineNdx);
		// the row of a line and the column in that row of a position in the line
		// the position at the end of a line that fills its last row completely is at the end of that row, and not at the start of a new one
		std::pair<int, int> wrapRowCol(int lineNdx, int pos);
		// scrolls so that the row of the cursor is visible
		void scrollToCursorWrapped();
		// scrolling by pages (see pageUp() and pageDown())
		int pageRowsOf(int lineNdx);
		void setCursorPosAfterPage(int prevCursorLineIndex);
		EditorSnapshot takeSnapshot();
		void restoreSnapshot(const EditorSnapshot& snapshot);
		bool textDiffers(const EditorSnapshot& snapshot);
		// called before and after every change of the text, to store an undo step if the text changed
		// melodic lines of bar definitions are aligned to a grid of durations (see \editor.alignment and alignment.cpp in ofApp)
		bool alignment = false;
		// a unit of the grid is as wide as the shortest note token of the bar, instead of the longest (see \editor.alignment short and long)
		bool alignShort = false;
		// the column of the cursor in its line, and placing it at a column, scrolling a long line so that the cursor is visible
		int getCursorColumn();
		void setCursorColumn(int col);
		void setFileEdited(bool edited) { fileEdited = edited; }
		void beforeChange(bool newUndoStep = false);
		void afterChange(bool endUndoStep = false);
		std::string tabStr; // to not having to write "    " every time

		// variables to connect lines to bars for animating the editor
		bool animationState;
		// the id of the next line that gets one (see line::id)
		int nextLineId = 0;
		std::map<int, std::map<int, int>> instsConnectedToLine;

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
		// the two variables below are used for opening help files and storing the cursor line and position temporarily
		int tempCursorLineIndex;
		int tempCursorPos;

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
		int oldMaxCharactersPerString;
		size_t maxBacktraceChars;
		float characterOffset;
		// store last highlighted char to properly position the cursor when
		// the std::string on its line goes from long to fitting
		int highlightedCharIndex;
		// and a boolean to check whether a std::string has just got small enough to fit the window
		bool stringExceededWindow;
		float executionStepPerMs;
		// variable to set the first (or only) line to execute
		// useful in case we hit shift+return as the cursor will move
		// before the line is executed
		int executingLine;
		// a temporary storage for the executing line when loading help files
		int tempExecutingLine;
		// the language of the pane, which a help file might change (e.g. a .lyv help file in a Python pane), restored when the help is closed
		int tempLang = 0;
		// stores the text and the state of the pane before a help file or help text replaces them, to be restored by closeHelpFile()
		void storeBufferForHelp();
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
		std::map<int, binding> bindings;
		// autocompletion, with a pop-up of suggestions for the word that is typed in insert mode
		// in LiveLily, commands after a backslash and methods after a dot, in Python, keywords, builtins, and the names in the pane
		// Tab selects the next suggestion (Shift+Tab the previous one) and Return inserts the selected one
		void updateCompletions();
		void clearCompletions();
		// draws the pop-up of suggestions, called after everything else is drawn so that it is on top
		void drawCompletions();
	private:
		// returns true if the key was used by the pop-up of suggestions (Tab, or Return with a selected suggestion)
		bool completionKey(int key);
		std::vector<std::string> pythonCompletions(bool afterDot, int wordLine, int wordStart);
		// the dotted name (e.g. np.random) that ends right before position end of a string, or an empty string
		std::string pythonDottedName(const std::string& str, int end);
		// the hint of the arguments of a Python callable, which appears in the text when ( is typed after its name, e.g. Sine(
		// with its parameters (freq=1000, phase=0, mul=1, add=0), dimmed, between the cursor and the rest of the line, without being part of the text
		// what is typed replaces it, argument by argument, and Tab inserts what is left of it
		// typing ( in its brackets after the name of another callable starts the hint of that one, until its brackets are closed
		struct ArgHint {
			int line;
			int openPos; // the position of ( in the line
			std::vector<std::string> params;
		};
		std::vector<ArgHint> argHints;
		// called after every key in insert mode, starts a hint when ( is typed after the name of a callable
		void updateArgHints(int key);
		// the text of the hint at the cursor, which is shown after it and inserted with Tab, or an empty string
		// typingKeyword is true while the argument at the cursor is typed as a prefix of a parameter (e.g. fr of freq=1000)
		// in which case the pop-up of suggestions is not shown, so that Tab completes the parameter
		std::string argHintText(bool* typingKeyword = nullptr);
		bool argHintValid(const ArgHint& hint);
		std::vector<std::string> completions;
		int completionNdx = -1; // -1 for no selected suggestion, until Tab is pressed
		int completionPrefixLen = 0; // how many characters of the suggestions are already typed
		int completionLine = -1; // the line and position in the line of the cursor the suggestions were made for
		int completionPos = -1;
		// the position of the cursor on the screen, stored when it is drawn
		int cursorScreenX = 0;
		int cursorScreenY = 0;
		int objID;
		bool helpFileOpen;
		bool fileEdited;
		bool tempFileEdited;
		bool autobrackets;
		bool autocompletion = true;
		bool activeSession;
		bool sendKeys;
		int sendKeysPaneNdx;
		bool sendLines;
		int sendLinesPaneNdx;
		bool ctrlPressed;
		bool shiftPressed;
		bool altPressed;
		bool dPressed;
		std::string fromOscStr;
		std::string yankedStr;
		// how the yanked text was selected, 0 for characters, 1 for whole lines (V), 2 for a block (Ctrl+v)
		// so that p pastes lines below the line of the cursor, and a block as columns, as in Vim
		int yankedType = 0;
		// pasting yanked lines and blocks (see pasteStringUnrecorded())
		void pasteLinesUnrecorded(const std::vector<std::string>& lines);
		void pasteBlockUnrecorded(const std::vector<std::string>& rows);
		//std::ifstream file;
};

#endif
