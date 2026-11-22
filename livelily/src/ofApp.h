#ifndef OF_APP_H
#define OF_APP_H

#include "ofMain.h"
#include "osc.h"
#include "serial.h"
#include "audio.h"
#include "frontend.h"
#include "session.h"
#include <map>
#include <vector>
#include <utility> // to add pair
#include <mutex>
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

#define OFRECVPORT 8001
#define OFSENDPORT 1234 // for sending sequencer data to other software
#define SCOREPARTPORT 9000
#define KEYCLIENTPORT 9100
#define HOST "localhost"
#define BACKGROUND 15

// 64ths are the minimum note duration
// so we use 4 steps per this duration as a default
#define MINDUR 256
// the MIDI velocities are separated by 16 values, from ppp to fff with mp and mf included
#define MIDIVELSTEP 16
#define BEATVIZCOEFF 0.8 // coefficient to reduce maximum brightness of rectange
#define MINDB 82 // this is equivalent to 16 MIDI velocity which is set as a ppp dynamic
#define TRACEBACKDUR 2000
#define MINTRACEBACKLINES 2

#define OSCPORT 9050 // for receiving OSC messages in editors

#define WINDOW_RESIZE_GAP 50

#define INBUFFSIZE 5

#define SENDBARDATA_WAITDUR 1000 // in milliseconds
#define NOTESXOFFSETCOEF 3 // multiplication coefficient for giving offset to the notes

#define MAESTROTHRESH 100

#define GLISSGRAIN 20 // 20ms grain for glissandi

// for the piano roll
#define KEYSWIDTHCOEFF 8

#define HELPFILEPATH "~/Documents/LiveLily/help_files"

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

struct CmdInput
{
	std::vector<std::string> inputVec;
	std::vector<std::string> afterCmdVec;
	bool hasBrackets;
	bool isMainCmd;
	// true for a command inside a line, whose arguments have been evaluated before it runs (see ofApp::evaluateTokens())
	bool argsEvaluated = false;
};

struct CmdOutput
{
	int errorCode;
	std::string errorStr;
	// for a command inside a line, its output replaces the command and its arguments, unless this is true
	// in which case the output replaces only the command, and its arguments stay in the line after it (see ofApp::genOutput())
	bool keepsArgs;
	std::vector<std::string> outputVec;
};

class ofApp : public ofBaseApp, public AudioCallback, public FrontendHost, public Session
{
	public:
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
		// a warning for ties at the end of bars of a loop that are followed by other notes, and are ignored (see tieAcrossBars() in utils.h)
		// or an empty string
		std::string tieWarning(int loopNdx);
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
		// the user interface, as the engine (the parser and the sequencer) sees it, which is this app for now
		FrontendHost* frontend = this;
		void storeActiveEditorElement(int instNdx, int barNdx, int dataCounter, bool state);
		// the same for a rest, which stays highlighted until the next note or rest replaces it
		void storeActiveEditorRest(int instNdx, int barNdx, int dataCounter);
		// stops highlighting the element of an instrument (e.g. a rest followed by a muted note)
		void clearActiveEditorElement(int instNdx);
		// with the sequencer stopped, \mute, \unmute, and \solo apply at once, instead of at the start of the next loop
		void applyMutesNow();
		// highlighting the tokens of the notes that play in the editor, as in Strudel (see \editor.animate)
		// when a bar is parsed, the place of the token of every note of every instrument is stored (more than one token for a note
		// of a bar that copies another bar, e.g. both \1 and the note in bar 1), and the sequencer sets which note plays
		void storeEditorNoteSpans(int lineNum, int instNdx, int barNdx, int numNotes);
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
		// parses lines (each with its line number in the editor, used for tracebacks), and finishes any bar they define
		// returns the first error, or the result of the last line if there is no error
		// nextBarsPass is true when \bars parses its lines again for its next bar (see the end of parseLines())
		std::pair<int, std::string> parseLines(const std::vector<std::pair<int, std::string>>& lines, int numLines, bool stopOnError, bool nextBarsPass = false);
		// definitions with curly brackets can be written in one or more lines, and this brings them to the form the parser expects
		// \loop and \list in more lines are joined into one line, and \bar and \bars in one line are split into one line per statement
		std::vector<std::pair<int, std::string>> normalizeDefinitionLines(const std::vector<std::pair<int, std::string>>& lines);
		// keeps only the values of \time, \tempo, or \beatat in cmdInput, and returns the rest of the line
		std::vector<std::string> splitBarCommandArgs(CmdInput& cmdInput);
		std::pair<int, std::string> parseString(std::string str, int lineNum, int numLines);
		// stores a line of the body of the function being defined
		std::pair<int, std::string> storeFunctionLine(const std::string& str, int lineNum, int numLines);
		// calls a function bound to an instrument, the beat, the framerate etc.
		// called from the sequencer thread and from update()
		void callBoundFunction(const std::string& funcName, const std::string& args = "");
		// splits a line into tokens, keeping strings in quotes, chords in angle brackets, and curly brackets as tokens of their own
		std::vector<std::string> tokenizeLine(const std::string& input);
		// evaluates a sequence of tokens that starts with a command, which is left as it is: the commands after it are replaced by their outputs
		// each with its arguments, according to its mode (see CommandSpec), e.g. a melodic line, or the arguments of a command (see evalArgs())
		CmdOutput evaluateTokens(const std::vector<std::string>& tokens, int lineNum, int numLines);
		CmdOutput parseCommand(CmdInput cmdInput, int lineNum, int numLines);
		// evaluates the arguments of a command according to its mode (see CommandSpec), before parseCommand() runs its handler
		CmdOutput prepareArgs(CmdInput& cmdInput, int lineNum, int numLines);
		// the handler of a command, from commandTable(), which parseCommand() calls by the name of the command
		using CommandHandler = CmdOutput (ofApp::*)(CmdInput& cmdInput, int lineNum, int numLines);
		CommandHandler commandHandler(const std::string& name);
		CmdOutput cmdTime(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdTempo(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdBeatat(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdTuplet(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdOttava(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdCluster(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdTranspose(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdClef(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdEuclide(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdApply(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdPpqn(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdMidiclock(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdInstorder(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdPrint(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdOsc(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdOscserver(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdReturn(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdNuminsts(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdBarnum(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdBeatcount(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdInsts(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdMerge(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdGroup(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdScore(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdEditor(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdCursor(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdBar(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdLoop(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdBars(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdRandom(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdList(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdExpand(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdFromosc(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdRemaining(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdSolo(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdSolonow(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdMute(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdUnmute(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdMutenow(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdUnmutenow(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdFunction(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdTune(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdListmidiports(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdOpenmidiport(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdCtlchange(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdPgmchange(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdNotedur(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdListserialports(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdOpenserialport(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdSerialwrite(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdSerialprint(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdMousex(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdMousey(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdMouse(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdCloseserialport(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdSystem(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdClicktrack(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdPystr(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdActive(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdInactive(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdSendkeys(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdSendlines(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdMaestro(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdFramerate(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdListaudiodev(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdSetaudiodev(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdSetinaudiodev(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdSetoutaudiodev(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdSamplerate(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdBuffersize(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdNchannels(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdInchannels(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdOutchannels(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdLivelily(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdPython(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdLua(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdAccoffset(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput cmdHelp(CmdInput& cmdInput, int lineNum, int numLines);
		CmdOutput defineVariable(CmdInput& cmdInput, int lineNum, int numLines);
		std::pair<int, std::string> checkNameAvailability(std::string structType, std::string s);
		// inside \bars, keeps the arguments of a command like \time, \tempo or \beatat for the bar being parsed
		// e.g. "\tempo 4 = 60 | 4 = 90" keeps "4 = 60" for the first bar and "4 = 90" for the second
		CmdOutput selectBarOfBarsCommand(CmdInput& cmdInput);
		// the ratio of a tuplet (e.g. {3, 2} for 3/2), or {0, 0} if it is not valid
		std::pair<int, int> tupletRatio(const std::string& str);
		// the value of an ottava (-2 to 2), and whether it is valid
		std::pair<bool, int> ottavaValue(const std::string& str);
		std::pair<int, std::string> parseMelodicLine(std::vector<std::string> v, int lineNum, int numLines);
		std::pair<int, std::string> parseBarLoop(std::string str, int lineNum, int numLines);
		std::pair<bool, CmdOutput> isInstrument(std::vector<std::string>& commands, int lineNum, int numLines);
		std::pair<bool, CmdOutput> isBarLoop(std::vector<std::string>& commands, bool isMainCmd, int lineNum, int numLines);
		std::pair<bool, CmdOutput> isFunction(std::vector<std::string>& commands, int lineNum, int numLines);
		std::pair<bool, CmdOutput> isList(std::vector<std::string>& commands, int lineNum, int numLines);
		std::pair<int, std::string> listItemExists(size_t listItemNdx);
		std::pair<bool, CmdOutput> isOscClient(std::vector<std::string>& commands, int lineNum, int numLines);
		std::pair<bool, CmdOutput> isOscServer(std::vector<std::string>& commands, int lineNum, int numLines);
		std::pair<bool, CmdOutput> isGroup(std::vector<std::string>& commands, int lineNum, int numLines);
		std::pair<bool, CmdOutput> isVariable(std::vector<std::string>& commands, bool isMainCmd, int lineNum, int numLines);
		//---------------------------------
		// arithmetic operations functions
		std::pair<std::pair<std::string, std::string>, std::pair<int, double>> getArithmeticResult(std::string strOperand1, std::string strOperand2, int operationType);
		std::string stripValFromResult(std::pair<std::pair<std::string, std::string>, std::pair<int, double>> p);
		// a number as a string, an integer, or a float with up to 15 significant digits and without trailing zeros
		std::string formatNumber(double value, bool isFloatValue);
		//---------------------------------
		// clearing functions
		void clearBars();
		void clearLoops();
		void clearLists();
		void clearFunctions();
		void clearInstruments();
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
		//---------------------------------
		// miscellaneous functions
		int msToBPM(unsigned long ms);
		// the following two functions are used in drawTraceback to sort the indexes based on the
		// time stamps. it is copied from https://www.geeksforgeeks.org/quick-sort/
		int partition(uint64_t arr[], int arr2[], int low, int high);
		void quickSort(uint64_t arr[], int arr2[], int low, int high);
		bool isQuoted(const std::string& s);
		//---------------------------------
		// send data to score parts
		void sendToParts(OscMessage m, bool delay);
		void sendBarToParts(int barIndex);
		void sendLoopToParts();
		void sendCountdownToParts(int countdown);
		void sendStopCountdownToParts();
		void sendSizeToPart(int instNdx, int size);
		void sendNumBarsToPart(int instNdx, int numBars);
		void sendAccOffsetToPart(int instNdx, float accOffset);
		void sendLoopIndexToParts();
		void sendNewBarToParts(std::string barName, int barIndex);
		void sendScoreChangeToPart(int instNdx, bool scoreChange);
		void sendChangeBeatColorToPart(int instNdx, bool changeBeatColor);
		void sendFullscreenToPart(int instNdx, bool fullscreen);
		void sendCursorToPart(int instNdx, bool cursor);
		//---------------------------------
		// debugging
		void printVector(std::vector<int> v);
		void printVector(std::vector<std::string> v);
		void printVector(std::vector<float> v);
		//---------------------------------
		// string/char state query functions
		bool isNumber(std::string str);
		bool isFloat(std::string str);
		//---------------------------------
		// index/char location functinos
		std::vector<int> findRepetitionInt(std::string str, int multIndex);
		int findNextStrCharIdx(std::string str, std::string compareStr, int index);
		bool areBracketsBalanced(std::string str);
		bool areBracketsBalanced(std::vector<std::string> v);
		//---------------------------------
		// string handling
		std::vector<int> findIndexesOfCharInStr(std::string str, std::string charToFind);
		std::vector<std::string> tokenizeString(std::string str, std::string delimiter, bool addDelimiter=false);
		std::map<size_t, std::string> tokenizeStringWithNdxs(std::string str, std::string delimiter);
		void resetEditorStrings(int index, int numLines);
		int findMatchingBrace(const std::string& s, size_t openPos);
		std::string genStrFromVec(const std::vector<std::string>& vec);
		//---------------------------------
		// list functions
		void storeList(std::string str);
		CmdOutput traverseList(int numLines);
		// the tokens of a melodic line made by \apply, from its arguments, or an error (see the \apply command)
		std::pair<std::string, std::vector<std::string>> applyToLine(const std::vector<std::string>& args);
		// the output of a list item, which is evaluated if it is a command (see the items of \list in CommandSpec)
		CmdOutput listItemOutput(std::pair<int, std::string> item, int lineNum, int numLines);
		void createEmptyMelody(int index);
		void resizePatternVectors();
		void sendPushPopPattern();
		//---------------------------------
		// new bar/loop functions
		int storeNewBar(std::string barName);
		void storeNewLoop(std::string loopName);
		void deleteLastBar();
		void deleteLastLoop();
		int getBaseDurValue(std::string str, int denominator);
		std::pair<int, std::string> getBaseDurError(std::string str);
		//---------------------------------
		// functions that return API I/O for commands
		CmdOutput genError(std::string str);
		CmdOutput genWarning(std::string str);
		CmdOutput genNote(std::string str);
		// the output of a command, which replaces the command and its arguments in the line it is in
		// or only the command, if keepArgs is true, so that its arguments stay in the line after it (e.g. a bar name, which ignores what follows it)
		CmdOutput genOutput(std::string str, bool keepArgs = false);
		CmdOutput genOutput(std::vector<std::string> v, bool keepArgs = false);
		CmdOutput genOutputFuncs(std::pair<int, std::string> p);
		CmdInput genCmdInput(std::string str);
		CmdInput genCmdInput(std::vector<std::string> v);
		// end of command API I/O functions
		void initPyo();
		CmdOutput functionFuncs(std::vector<std::string>& commands);
		int getLastLoopIndex();
		int getLastBarIndex();
		int getPrevBarIndex();
		int getPlayingBarIndex();
		CmdOutput stripLineFromBar(std::vector<std::string> tokens, int lineNum, int numLines);
		bool areWeInsideChord(size_t size, unsigned i, unsigned *chordNotesIndexes);
		std::vector<std::string> tokenizeChord(std::string str, bool includeAngleBrackets = false);
		std::vector<std::string> detectRepetitions(std::vector<std::string> tokens);
		//---------------------------------
		// coordinates/positions/sizes functions
		void setPaneCoords();
		// set the global font size to calculate dimensions
		void setFontSize(bool calculateCoords);
		void setScoreCoords();
		void resetScoreYOffset();
		void setScoreNotes(int barIndex);
		void setNotePositions(int bar);
		void setNotePositions(int bar, int numBars);
		void swapScorePosition(int orientation);
		void setScoreSizes();
		void calculateStaffPositions(int bar, bool windowChanged);
		//---------------------------------
		// various commands for the score
		CmdOutput scoreCommands(std::vector<std::string>& originalCommands, int lineNum, int numLines);
		void setShowScore(int ndx);
		CmdOutput maestroCommands(std::vector<std::string>& commands, int lineNum, int numLines);
		bool isScoreVisible();
		//---------------------------------
		// instrument functions
		void initializeInstrument(std::string instName);
		// test mode, set from the command line (see main.cpp and testmode.cpp)
		std::string testFile;
		std::string testOut;
		bool testDone = false;
		// with --screenshot, the window is saved to this file after the test has run
		std::string testScreenshot;
		uint64_t testDoneFrame = 0;
		void runTest();
		void dumpState(std::ostream& out);
		// gives the instruments added after the first numInstsBefore ones (with \insts.add) an empty bar for every bar defined before them
		// in the sequencer and the score, and moves the bindings of functions to the beat, bars, etc., which follow the instruments
		void addedInstruments(int numInstsBefore);
		// removes an instrument (with \insts.remove) from the sequencer and the score, and gives the instruments after it the indexes before
		void removeInstrument(int instNdx);
		void fillInMissingInsts(int barIndex);
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
		int whichPane; // this variable indexes the keys of the editors map above

		// parseString() is also called from the sequencer thread (functions bound to instruments, beats etc.)
		// but the editors must only be modified from the main thread, as they are drawn there
		// so tracebacks set from other threads are queued and applied in update()
		void setEditorTraceback(int errorCode, std::string errorStr, int lineNum);
		void releaseEditorTraceback(int lineNum);
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


		// the settings of the audio stream, from \samplerate, \buffersize, the channels, and the devices
		AudioSettings audioSettings();
		// sets up the sound stream for the click track, if pyo hasn't set it up already
		void startSoundStream();
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
		// vector of strings of command names that should not be expanded
		/* the signatures of commands: how parseCommand() and evaluateTokens() treat their arguments
		   the language is the same for all commands: a command takes no argument, one argument after it, or more arguments in curly brackets
		   with the first one before the brackets or inside them, and methods are concatenated to commands with a dot
		   evaluating an argument replaces the commands in it by their outputs (e.g. \rand in \myloop.goto {\rand 1 4}, or a bar name, by the lines of the bar)
		   a command gets its arguments as they are written, and they are evaluated when it runs:
		   - with values, all of them are evaluated before its code runs (see parseCommand()), which is what most commands need
		   - with names, its arguments are instrument names (or \all), which are taken as they are written, and any other command among them
		     is evaluated before its code runs (e.g. \mute \one \two, where \one would otherwise take \two as its melodic line)
		   - with raw, its code evaluates the ones it needs itself, with evalArgs()
		     e.g. definitions store their lines as they are written (\bar, \loop, \function), \list.traverse runs its lines later
		     and \apply reads the name of its bar and evaluates its notes
		   the command at the start of a line gets the whole line as it is written (see parseCommand()), and a command inside a line
		   gets its arguments as they are written, and they are evaluated where they are, before it runs (see evaluateTokens())
		   values is the mode of any command that is not in the tables below (e.g. the name of a variable in its definition, x = 5) */
		enum class ArgsMode { values, names, raw };
		struct CommandSpec {
			ArgsMode args = ArgsMode::values;
			// the command takes no arguments, so inside a line, a command written after it is not its argument (e.g. \print \mousex \mousey)
			// but a token that is not a command, or curly brackets, written after it are, and the command gives an error (e.g. \print \mousex 3)
			bool noArgs = false;
			// the command takes nothing written after it, inside a line, and what follows it stays in the line after its output
			// (a function whose body uses no arguments, e.g. \print \pick \pick, see commandSpec())
			bool takesNothing = false;
		};
		// a command of LiveLily: its handler (see the cmd...() functions), its signature, and whether it also takes methods (e.g. \insts.init)
		// all the commands are in commandTable() in parser.cpp, which is the only place a command is registered
		struct CommandEntry {
			CommandHandler handler;
			CommandSpec spec;
			bool takesMethods = false;
		};
		static const std::map<std::string, CommandEntry>& commandTable();
		// the names defined in a session, whose methods have signatures of their own
		enum class NameKind { none, loop, list, function, variable, instrument, oscClient, oscServer, group };
		// the methods of names defined in a session, by the kind of the name and the method, "" for no method, and "*" for any other method
		// e.g. \myloop.goto \1, \mylist.traverse { \print $ }, and \myfunc.bind 2, whose lines a function stores
		const std::map<std::pair<NameKind, std::string>, CommandSpec> methodSpecs = {
			// a bar or loop name gives its bars, and with an instrument after a dot, the line of that instrument, and it takes no arguments
			// goto and locate take a bar or loop name, and check takes a number
			{{NameKind::loop, ""}, {ArgsMode::values, true}}, {{NameKind::loop, "*"}, {ArgsMode::values, true}},
			{{NameKind::loop, "goto"}, {ArgsMode::raw}}, {{NameKind::loop, "locate"}, {ArgsMode::raw}}, {{NameKind::loop, "check"}, {ArgsMode::values}},
			// an item of a list takes no arguments, and traverse runs its lines later
			{{NameKind::list, ""}, {ArgsMode::values, true}}, {{NameKind::list, "*"}, {ArgsMode::values, true}},
			{{NameKind::list, "traverse"}, {ArgsMode::raw}},
			// the lines of a function name store their data (e.g. \myfunc.bind 2), and its arguments are passed to its body
			{{NameKind::function, "*"}, {ArgsMode::raw}},
			// a variable gives its value, and takes no arguments, other than an assignment or an operation (e.g. \x = 5, or \x += 2)
			{{NameKind::variable, ""}, {ArgsMode::values, true}}, {{NameKind::variable, "*"}, {ArgsMode::values}},
			// instruments and groups take melodic lines (inside bars, which are not evaluated, see parseCommand()) and methods with values
			{{NameKind::instrument, "*"}, {ArgsMode::values}}, {{NameKind::group, "*"}, {ArgsMode::values}},
			{{NameKind::oscClient, "*"}, {ArgsMode::values}}, {{NameKind::oscServer, "*"}, {ArgsMode::values}}
		};
		NameKind nameKind(const std::string& name);
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
		// the error for defining a name that is already the name of another kind, or an empty string
		std::string nameTakenError(const std::string& name, NameKind kind);
		// the signature of the command of a token, e.g. \bar, \insts.init, or \myloop.goto (the default one, if it has none)
		CommandSpec commandSpec(const std::string& token);
		// whether a function takes arguments when it is called: its body uses them ($1 etc.), and none are set with .setargs
		bool functionTakesArgs(const std::string& name);
		// evaluates the arguments of a command (see CommandSpec): the commands in them are replaced by their outputs
		CmdOutput evalArgs(const std::vector<std::string>& args, int lineNum, int numLines);
		// evaluates the arguments of a command with names: instrument names are taken as written, and other commands are evaluated
		CmdOutput evalNames(const std::vector<std::string>& args, int lineNum, int numLines);

		bool isReservedName(const std::string& name); // the name with its backslash
		// the instrument whose staff an instrument is drawn on, which is the instrument itself if it is not merged into another one
		int mergeHostOf(int instNdx);
		// sets the voices of all instruments, and lays out all bars again
		void updateMergedVoices();
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

		bool inserting;

		// STDERR

		// variables for the score
		bool showScore;
		int scoreType;
		bool showPianoRoll;
		bool showScope;
		// showNotes is in SharedData because it is used by the sequencer
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
