#ifndef OF_APP_H
#define OF_APP_H

#include "ofMain.h"
#include "osc.h"
#include "serial.h"
#include "audio.h"
#include "frontend.h"
#include "session.h"
#include "of_instrument_display.h"
#include "of_grid_painter.h"
#include "view_state.h"

// what is drawn of an instrument (its staff and its notes), which the front end attaches to every instrument
using ScoreView = OfInstrumentDisplay;
#include <map>
#include <vector>
#include <utility> // to add pair
#include <mutex>
#include <atomic>
#include <thread>
#include "editor.h"
#include "function.h"
#include "sequencer.h"
#include "utils.h"
#include "instrument.h"
#include "color.h"
#include "of_color.h"
#include "ofTimer.h"
#ifdef USEPYO
#include "PyoClass.h"
#endif

// a traceback change made outside the main thread (e.g. by a function called from the sequencer)
// which is stored and applied to the editor in ofApp::update(), on the main thread
struct PendingTraceback
{
	int pane;
	int lineNum;
	bool release; // true to release the traceback, false to set it with the two fields below
	int errorCode;
	std::string errorStr;
	// name of the bound function that was running when this traceback was set, empty if none
	// if not empty, the traceback goes to the line where this function is defined, if that line still defines it
	std::string boundFunction;
};

class ofApp : public ofBaseApp, public AudioCallback, public FrontendHost
{
	public:
		// the engine of LiveLily (see session.h): the state of the session, the parser, and the sequencer
		// declared first, so that it is destroyed after everything else of the app (e.g. the sequencer's thread calls the app)
		Session session;
		// the state of the view: the window, the positions of the staffs, the piano roll, the beat visualization and the area below the panes
		ViewState view;
		// basic OF program structure
		//---------------------------------
		void setup();
		void update();
		void draw();
		void drawTraceback();
		void drawShell();
		void drawScore();
		// whether the shell (commands typed after a colon and their output) was shown in the last frame, see draw()
		bool shellWasShown = false;
		// the number of lines of the area for error messages, set in drawTraceback(), -1 to calculate it again
		int tracebackNumLinesShown = MINTRACEBACKLINES;
		// the number of lines of the area for the shell (commands and their output), set in drawShell(), -1 to calculate it again
		int shellNumLinesShown = MINTRACEBACKLINES;
		// the output of a shell command that doesn't fit the area below the panes scrolls with the arrow keys (see executeKeyPressed())
		// shellScroll is its first line that is shown, of shellTotalLines, of which shellVisibleLines are shown
		int shellScroll = 0;
		int shellTotalLines = 0;
		int shellVisibleLines = MINTRACEBACKLINES;
		// the pane that was active when the panes were made shorter for the shell, which keeps its height the longest (see layoutPanesForShell())
		int shellLayoutPane = -1;
		// makes the panes shorter so that the shell area below them gets the number of lines it wants
		// as long as every row of panes keeps two lines, one for its text and one for the line with its file name
		// the rows shrink from the bottom up, and the row of the active pane last, so it keeps its height the longest
		// as with Ctrl+w followed by _ in Vim, which leaves one line to the other panes
		// returns the number of lines the shell area gets
		int layoutPanesForShell(int wanted);
		// gives the panes back the layout they had before the shell made them shorter
		void restorePaneLayout();
		// the number of lines, the vertical position, and the height of every pane before the shell made them shorter, by pane index
		// empty while the panes have the layout of setPaneCoords() (and of the score, see swapScorePosition())
		struct PaneBase {
			int lines;
			float y;
			float height;
		};
		std::map<int, PaneBase> paneBase;
		// draws text with many lines, with the line height of the editor
		void drawLines(const std::string& str, float x, float y);
		void moveScoreWithTraceback(float prevTracebackYCoord);
		void drawPianoRoll();
		// sets the range of the keys of the piano roll (MIDI notes), from a white key to a white key, and the size of the keys
		void setPianoRollRange(int lowestKey, int highestKey);
		// a note name with its octave (e.g. c1, fis4, a5) as a MIDI note, or -1
		int noteNameToMidi(const std::string& name);
		// the place of a note on the keyboard of the piano roll, in widths of white keys from the lowest white key
		// for the lane it rolls in (and the black key, for a black key), as on a piano, where the three white keys from C to E
		// have five equal lanes, and the four white keys from F to B have seven, so black keys lean towards the outer keys of their group
		std::pair<float, float> pianoRollLane(int note);
		void drawScope();
		void drawSequencer();

		void sendBeatVizInfo(int bar);
		void moveCursorOnShiftReturn();
		//---------------------------------
		// the following two functions execute key commands
		// so each can be called from any of the overloaded keyPressed() and keyRelease() below
		// functions and lock the mutex without creating a dead lock
		void executeKeyPressed(int key);
		void executeKeyReleased(int key);
		// in normal mode, Shift makes cursor moves select text only in visual mode
		void setNormalModeShift();
		// suggestions for the autocompletion of LiveLily panes (see Editor::updateCompletions())
		// all commands, with their backslash, and the methods of a command or of an object (an instrument, a function, etc.), by its name with a backslash
		std::vector<std::string> getCommandCompletions();
		// the arguments of commands and methods, and the methods (the names of commandsMap without a backslash, e.g. pianoroll)
		std::vector<std::string> getArgumentCompletions();
		// whether a line that starts with this word is a melodic line (it is an instrument or a group of instruments)
		bool startsMelodicLine(const std::string& word);
		std::vector<std::string> getMethodCompletions(const std::string& name);
		// suggestions for the autocompletion of Python panes, from the Python interpreter, so they are what executed code imported or defined
		// the names of __main__ and the builtins for an empty dotted name, or the attributes of the object of a dotted name (e.g. a or np.random)
		// empty if the interpreter is not running, or the first name is not known to it (its code hasn't been executed)
		std::vector<std::string> pythonNames(const std::string& dotted);
		// the parameters of a callable (e.g. Sine), as name=default or name, for the hint of its arguments (see Editor::argHintText())
		std::vector<std::string> pythonSignature(const std::string& dotted);
		// what autocompletion knows of the interpreter: all its names, and the attributes and parameters that have been asked for
		// it is made again, when it is used, after any execution of Python code (from a pane, the sequencer, or LiveLily, see Pyo::getExecutions())
		// so that it follows the objects that are created, changed, or deleted, and the interpreter is asked only about names it has
		struct PythonTable {
			bool made = false;
			unsigned executions = 0;
			std::set<std::string> names;
			std::map<std::string, std::vector<std::string>> attributes;
			std::map<std::string, std::vector<std::string>> signatures;
		} pythonTable;
		// makes the table again if Python code has been executed since it was made, returns false if the interpreter is not running
		bool refreshPythonTable();
		// overloaded keyPressed and keyReleased functions
		// so out-of-focus editors can receive input from OSC
		void keyPressedOsc(int key, int thisEditor);
		void keyReleasedOsc(int key, int thisEditor);
		// OF keyboard input functions
		void keyPressed(int key);
		void keyReleased(int key);
		// the following two functions are used to check for key modifiers received from a Python pane
		pyStdoutStrKeyModifier checkPyStdoutKeyModifier(int ndx);
		void handleKeyModifier(pyStdoutStrKeyModifier modifierStruct);
		//---------------------------------
		// adding/removing editor panes
		void addPane(int key);
		void removePane();
		// sets the row and column of every pane from the layout of the panes (see addPane() and removePane())
		void setPanesRowsAndCols();
		// makes the pane in a direction from the active one active (0 left, 1 down, 2 up, 3 right), the one by the cursor if there are more
		void moveToPane(int direction);
		// moves the score away from the active pane, if it covers it (with Alt+number and Ctrl+w)
		void revealActivePane();
		// what the engine asks of the user interface (see frontend.h)
		void notePlaying(int instNdx, int bar, int noteNdx, bool playing) override { storeActiveEditorElement(instNdx, bar, noteNdx, playing); }
		void restPlaying(int instNdx, int bar, int noteNdx) override { storeActiveEditorRest(instNdx, bar, noteNdx); }
		void nothingPlaying(int instNdx) override { clearActiveEditorElement(instNdx); }
		void setLineMessage(int pane, int line, int errorCode, const std::string& text, const std::string& boundFunction) override;
		void clearLineMessage(int pane, int line, const std::string& boundFunction) override;
		void showMouseCursor(bool show) override { if (show) ofShowCursor(); else ofHideCursor(); }
		float frameRate() override { return ofGetFrameRate(); }
		void setFrameRate(int framesPerSecond) override { ofSetFrameRate(framesPerSecond); }
		void setPaneLanguage(int pane, int language) override { editors[pane].setLanguage(language); }
		bool openHelp(int pane, const std::string& filePath, int newPane) override;
		void setEditorOption(int pane, const std::string& option, const std::string& value) override;
		void setPaneActive(int pane, bool active) override { editors[pane].setSessionActivity(active); }
		void sendKeysTo(int pane, const std::string& host, int targetPane) override { editors[pane].oscKeys->setup(host, OSCPORT); editors[pane].setSendKeys(targetPane); }
		void sendLinesTo(int pane, const std::string& host, int targetPane) override { editors[pane].oscKeys->setup(host, OSCPORT); editors[pane].setSendLines(targetPane); }
		int tabSize(int pane) override { return editors[pane].getTabSize(); }
		void connectLineToBar(int pane, int line, int instNdx, int bar) override { editors[pane].connectLineToBar(line, instNdx, bar); }
		std::string scoreViewCommand(const std::vector<std::string>& methodAndArgs) override;
		void scoreMustUpdate() override { mustUpdateScore = true; }
		void instrumentCreated(int instNdx) override;
		void instrumentRemoved(int instNdx) override;
		void instrumentsCleared() override;
		void layoutInstrumentBar(int instNdx, int bar) override;
		bool scoreShown() override;
		void sequencerStarted(float beatStepsPerMs, uint64_t beatRampStart) override;
		void beat(uint64_t timeStamp, float stepsPerMs, uint64_t rampStart) override;
		void barStarted(uint64_t timeStamp) override;
		void loopChanged() override;
		void setPythonTyping(bool charByChar) override;
		std::pair<int, int> mousePosition() override { return {mouseX, mouseY}; }
		void setPaneFromOsc(int pane, const std::string& address) override { fromOscAddr[pane] = address; fromOsc[pane] = true; }
		void storeActiveEditorElement(int instNdx, int barNdx, int dataCounter, bool state);
		// the same for a rest, which stays highlighted until the next note or rest replaces it
		void storeActiveEditorRest(int instNdx, int barNdx, int dataCounter);
		// stops highlighting the element of an instrument (e.g. a rest followed by a muted note)
		void clearActiveEditorElement(int instNdx);
		// highlighting the tokens of the notes that play in the editor, as in Strudel (see \editor.animate)
		// when a bar is parsed, the place of the token of every note of every instrument is stored (more than one token for a note
		// of a bar that copies another bar, e.g. both \1 and the note in bar 1), and the sequencer sets which note plays
		void storeEditorNoteSpans(int lineNum, int instNdx, int barNdx, int numNotes) override;
		std::vector<EditorNoteSpan> getActiveEditorNoteSpans(int paneNdx);
		// the tokens of every note, by instrument and bar
		std::map<std::pair<int, int>, std::vector<std::vector<EditorNoteSpan>>> editorNoteSpans;
		// the note that plays for every instrument, as its bar and its index, set by the sequencer thread, so guarded by a mutex
		std::map<int, std::pair<int, int>> activeEditorNotes;
		// the instruments whose active element is a rest, which is highlighted only if it is written as a rest in the line (see getActiveEditorNoteSpans())
		std::set<int> activeEditorRests;
		std::mutex activeEditorNotesMutex;
		//---------------------------------
		// parsing functions
		void parseStrings(int index, int numLines);
		//---------------------------------
		// command typing/executing funcitons
		void typeShellCommand(int key);
		// completes the last word of a shell command with the Tab key
		void completeShellCommand();
		// shows the output of a shell command below the panes
		void setShellOutput(const std::string& output, bool error);
		// quitting asks to save every pane with changes that are not saved, before calling exit()
		// with "save file? y/n" in the shell, where y saves the file (asking for a path if it has none, see quitPromptKey()) and n doesn't
		// Esc cancels quitting
		void requestQuit();
		// the keys typed while asking to save before quitting
		void quitPromptKey(int key);
		// asks to save the next pane with changes that are not saved, or quits if there are none left
		void quitPromptNext();
		// 0 when not quitting, 1 when asking whether to save a pane, 2 when asking for the path to save it to
		int quitPromptState = 0;
		// the panes that are left to ask for
		std::vector<int> quitPromptPanes;
		// breaks the lines of a string of the shell to fit the width of the window (maxShellChars, see drawShell()), and counts them
		// the strings are broken only when they are drawn, so that they fit the window also after it is resized
		std::string wrapShellLines(const std::string& str, int& numLines);
		int run_command(const std::string& cmd, std::string& output);
		void executeShellCommand();
		void formatShellLsOutput(std::vector<std::string> lsOutputTokens);
		void sendCountdownToParts(int countdown);
		void sendStopCountdownToParts();
		void sendNewBarToParts(std::string barName, int barIndex);
		void resetEditorStrings(int index, int numLines);
		void createEmptyMelody(int index);
		void resizePatternVectors();
		void sendPushPopPattern();
		bool areWeInsideChord(size_t size, unsigned i, unsigned *chordNotesIndexes);
		//---------------------------------
		// coordinates/positions/sizes functions
		void setPaneCoords();
		// set the global font size to calculate dimensions
		void setFontSize(bool calculateCoords);
		void setScoreCoords();
		void resetScoreYOffset();
		void setScoreNotes(int barIndex) override;
		void setNotePositions(int bar) override;
		void setNotePositions(int bar, int numBars) override;
		void swapScorePosition(int orientation);
		void setScoreSizes();
		void calculateStaffPositions(int bar, bool windowChanged) override;
		void setShowScore(int ndx);
		bool isScoreVisible();
		// test mode, set from the command line (see main.cpp and testmode.cpp)
		std::string testFile;
		std::string testOut;
		bool testDone = false;
		// with --screenshot, the window is saved to this file after the test has run
		std::string testScreenshot;
		uint64_t testDoneFrame = 0;
		void runTest();
		void dumpState(std::ostream& out);
		// adds the grids of the panes and of the area below them to the dump, as they were drawn (see draw())
		void appendTestScreen();
		bool testScreenWritten = false;
		//---------------------------------
		// the sequencer functions of the system
		int setPatternIndex(bool increment);
		unsigned setLoopIter(int ptrnIdx);
		int setInstIndex(int ptrnIdx, int i);
		int setBar(int ptrnIdx, int idx, int i);
		//---------------------------------
		// staff and notes handling
		void setActivePane(int activePane);
		//---------------------------------
		// release memory on exit
		void exit();
		//---------------------------------
		// rest of OF functions
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void mouseEntered(int x, int y);
		void mouseExited(int x, int y);
		void resizeWindow();
		void windowResized(int w, int h);
		void gotMessage(ofMessage msg);
		void dragEvent(ofDragInfo dragInfo);
		//---------------------------------
		// audio functions
		// called by the audio stream (see audio.h) from the audio thread
		void onAudioIn(const float* in, int numFrames, int numChannels) override;
		void onAudioOut(float* out, int numFrames, int numChannels, int sampleRate) override;

		// a mutex to avoid accessing the same data by different threads at the same time
		ofMutex mutex;

		std::map<int, Editor> editors; // keys are panes indexes

		void applyPendingTracebacks();
		// returns the pane and line where a function is defined, searching its pane if the definition has moved
		// returns {-1, -1} if the definition is no longer in the pane
		std::pair<int, int> findFunctionDefinitionLine(const std::string& funcName);
		bool lineDefinesFunction(const std::string& str, const std::string& funcName);
		std::thread::id mainThreadId;
		std::mutex pendingTracebacksMutex;
		std::vector<PendingTraceback> pendingTracebacks;
		std::map<int, int> numPanes; // rows and columns of panes
		int paneSplitOrientation;
		float screenSplitHorizontal;
		float oneCharacterWidth;
		int halfCharacterWidth;


		// oscilloscope drawing stuff
		// the last output buffer, copied by the audio thread for the oscilloscope, and its number of channels
		std::vector<float> scopeSamples;
		int scopeChannels = 0;
		std::mutex scopeMutex;
		std::vector<ofPolyline> scopeWaveforms;
		float scopeRms;

		// shell commands stuff
		std::string shellStr;
		std::string shellLsOutputStr;
		int numShellLines;
		int numShellLsOutputLines;
		int shellTabCounter;
		int shellStrCursorPos;
		size_t maxShellChars;
		ofColor shellStrColor;
		// the area below the panes (the messages of the lines, or the shell), as a grid of cells (see cellgrid.h), filled and painted by drawTraceback() and drawShell()
		CellGrid belowGrid;
		// paints belowGrid, whose rows start at the top of the area below the panes, with the text at the bottom of every row
		void paintBelowGrid();
		void writeBelowRows(const std::string& lines, int firstRow, Color color);
		bool executingBind;
		int editorBindingKey;
		int editorBindingArg;
		bool editorBindingIsDollar;
		// true after g is pressed in normal mode, so that a second g moves the cursor to the first line, as in Vim
		bool gPressed;
		// true after d is pressed in normal mode without a selection, so that a second d deletes the line, as in Vim
		bool dPressed = false;
		// true after Ctrl+w in normal mode, so that h, j, k, or l moves to the pane on the left, below, above, or on the right, as in Vim
		bool ctrlWPressed = false;
		// the commands of the editor, which are completed with the Tab key together with the programs in the directories of PATH
		// (see completeShellCommand()), other shell commands (e.g. :ls or :pwd) are executed by the shell
		std::vector<std::string> shellCommands = {":save", ":w", ":wq", ":wa", ":wqa", ":x", ":xa", ":load", ":q", ":q!", ":qa", ":qa!", ":set", ":bind", ":unbind", ":cd", ":vs", ":vsplit", ":sp", ":split"};
		// the working directory before the last :cd, for :cd -
		std::string prevWorkingDir;




		// the map of vectors below is used to determine the number of lines to execute
		// if we start from the first line of a chunk that is not indented
		std::map<int, std::vector<std::string>> noIndentCheck;
		// aligning the melodic lines of bar definitions in a pane to a grid of durations (see \editor.alignment and alignment.cpp)
		// all of them, the one with the cursor after its text is edited (space is true for a space), and back to single spaces
		void alignBars(int pane);
		void alignAfterEdit(int pane, bool space);
		// after pasting, which can add lines to more than one bar definition (e.g. a whole bar), all of them, with the cursor staying where it is
		void alignAfterPaste(int pane);
		void unalignBars(int pane);
		// aligns the lines of a bar definition (from firstLine to lastLine), and gives the new column of the cursor if it is in one of them
		// the cursor goes to the token after it (after a space), or with keepInToken, stays in the token it is in (after any other edit)
		// with keepInToken, nothing is aligned if the line of the cursor can't be read (e.g. a token that is half typed)
		bool alignBarLines(int pane, int firstLine, int lastLine, int cursorLine, int cursorCol, int& newCursorCol, bool keepInToken = false);
		// moves the tokens the notes highlight (see storeEditorNoteSpans()) in a line whose tokens have moved, from their old starts to their new ones
		void moveHighlights(int pane, int lineNdx, const std::map<int, int>& newStarts);

		// moves note heads of the voices of a staff that sound together so they don't collide, as LilyPond does by default
		void applyMergeLayout(int bar);




		enum errorTypeNdx {note, warning, error};

		int notesID; // used mainly for debugging note and staff objects

		bool fullScreen;
		bool isWindowResized;
		uint64_t windowResizeTimeStamp;

		ofColor backgroundColor;
		ofColor foregroundColor;
		ofColor beatColor;
		ofColor scoreBackgroundColor;
		float brightnessCoeff;

		ofTrueTypeFont font;
		ofTrueTypeFont instFont;
		bool fontLoaded;


		std::map<int, std::string> allStrings;
		std::map<int, int> allStringStartPos;
		std::map<int, std::vector<int>> allStringTabs;
		//std::vector<std::vector<int>> curlyIndexes;


		int fontSize; // the size of the font of the editor
		int instFontSize; // the size of the font of the instrument display
		float cursorHeight;

		bool shiftPressed;
		bool ctrlPressed;
		bool altPressed;

		// global line width
		int lineWidth;


		// STDERR

		// variables for the score
		bool showScore;
		int scoreType;
		// whether the oscilloscope is the view shown (see setShowScore()), which the audio thread reads to copy the output for it
		std::atomic<bool> showScope{false};
		// showNotes is in ViewState (view_state.h), and the sequencer asks for it with scoreShown()
		float scoreXOffset;
		float scoreYOffset;
		float scoreBackgroundWidth;
		float scoreBackgroundHeight;
		// variables to determine whether we call a command for moving the score
		bool scoreMoveXCommand;
		bool scoreMoveYCommand;
		int scoreOrientation;
		float scoreXStartPnt;
		int numBarsToDisplay;
		float notesLength; // used to derive the ratio when we display more than two bars horizontally
		bool mustUpdateScore;
		bool scoreUpdated;
		bool scoreChangeOnLastBar;
		// map that stores which editors can receive data from OSC
		std::map<int, bool> fromOsc;
		// and a map with the OSC address for each editor
		std::map<int, std::string> fromOscAddr;

		std::map<int, int> instNameWidths;
		bool showBarCount;
		bool showTempo;

		bool exiting;

		// mouse's X and Y coords
		int mouseX;
		int mouseY;
};

#endif
