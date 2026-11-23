#ifndef SESSION_H
#define SESSION_H

#include <cstdint>
#include <list>
#include <memory>
#include <map>
#include <set>
#include <utility>
#include <vector>
#include "color.h"
#include "midi.h"
#include "osc.h"
#include "serial.h"
#include "audio.h"
#include "frontend.h"
#include "tokens.h"
#include "utils.h"
#include "sequencer.h"
#include <string>

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

// a variable of a session (x = 5), with its value as a string and its type
struct variable
{
	std::string varStr;
	int varType;
};

struct CmdInput
{
	std::vector<std::string> inputVec;
	std::vector<std::string> afterCmdVec;
	bool hasBrackets;
	bool isMainCmd;
	// true for a command inside a line, whose arguments have been evaluated before it runs (see Session::evaluateTokens())
	bool argsEvaluated = false;
};

struct CmdOutput
{
	int errorCode;
	std::string errorStr;
	// for a command inside a line, its output replaces the command and its arguments, unless this is true
	// in which case the output replaces only the command, and its arguments stay in the line after it (see Session::genOutput())
	bool keepsArgs;
	std::vector<std::string> outputVec;
};

// the engine of LiveLily: the state of a session, the parser (parser.cpp, session.cpp) and the sequencer, without the user interface
// (see doc/stage1_inventory.md, step 5)
// it reaches the user interface only through FrontendHost (frontend.h), and the system only through the interfaces of the platform
// the front end (ofApp for now) holds a Session, sets its frontend (and audioCallback) when it starts, and then calls setup()
class Session
{
	public:
		Session() { sequencer.setSession(this); }
		virtual ~Session() = default;

		// the user interface, as the engine sees it (see frontend.h), which the front end sets when it starts
		FrontendHost* frontend = nullptr;
		// sets the defaults of the session and the tables of the names of its languages, once frontend is set (see session_setup.cpp)
		// foreground is the color of names that are not highlighted
		void setup(Color foreground);
		// the pane whose code runs, which the front end sets (the active pane)
		int whichPane = 0;

		// the handler of a command, from commandTable(), which parseCommand() calls by the name of the command
		using CommandHandler = CmdOutput (Session::*)(CmdInput& cmdInput, int lineNum, int numLines);
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

		// the engine functions of session.cpp: helpers of the commands, storing bars, loops, lists and instruments, score parts, pyo and audio
		bool isReservedName(const std::string& name); // the name with its backslash
		// a warning for ties at the end of bars of a loop that are followed by other notes, and are ignored (see tieAcrossBars() in utils.h)
		// or an empty string
		std::string tieWarning(int loopNdx);
		// with the sequencer stopped, \mute, \unmute, and \solo apply at once, instead of at the start of the next loop
		void applyMutesNow();
		// the handler of a command, from commandTable(), which parseCommand() calls by the name of the command
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
		// miscellaneous functions
		int msToBPM(unsigned long ms);
		bool isQuoted(const std::string& s);
		//---------------------------------
		// send data to score parts
		void sendToParts(OscMessage m, bool delay);
		void sendBarToParts(int barIndex);
		void sendLoopToParts();
		void sendSizeToPart(int instNdx, int size);
		void sendNumBarsToPart(int instNdx, int numBars);
		void sendAccOffsetToPart(int instNdx, float accOffset);
		void sendLoopIndexToParts();
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
		int findMatchingBrace(const std::string& s, size_t openPos);
		std::string genStrFromVec(const std::vector<std::string>& vec);
		//---------------------------------
		// list functions
		void storeList(std::string str);
		CmdOutput traverseList(int numLines);
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
		std::vector<std::string> tokenizeChord(std::string str, bool includeAngleBrackets = false);
		std::vector<std::string> detectRepetitions(std::vector<std::string> tokens);
		//---------------------------------
		// various commands for the score
		CmdOutput scoreCommands(std::vector<std::string>& originalCommands, int lineNum, int numLines);
		CmdOutput maestroCommands(std::vector<std::string>& commands, int lineNum, int numLines);
		//---------------------------------
		// instrument functions
		void initializeInstrument(std::string instName);
		// gives the instruments added after the first numInstsBefore ones (with \insts.add) an empty bar for every bar defined before them
		// in the sequencer and the score, and moves the bindings of functions to the beat, bars, etc., which follow the instruments
		void addedInstruments(int numInstsBefore);
		// removes an instrument (with \insts.remove) from the sequencer and the score, and gives the instruments after it the indexes before
		void removeInstrument(int instNdx);
		void fillInMissingInsts(int barIndex);
		// the settings of the audio stream, from \samplerate, \buffersize, the channels, and the devices
		AudioSettings audioSettings();
		// sets up the sound stream for the click track, if pyo hasn't set it up already
		void startSoundStream();
		// the instrument whose staff an instrument is drawn on, which is the instrument itself if it is not merged into another one
		int mergeHostOf(int instNdx);
		// sets the voices of all instruments, and lays out all bars again
		void updateMergedVoices();

		// the parser (parser.cpp): parsing lines of code, the commands and their signatures, and the names of a session
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
		// the tokens of a melodic line made by \apply, from its arguments, or an error (see the \apply command)
		std::pair<std::string, std::vector<std::string>> applyToLine(const std::vector<std::string>& args);
		// the output of a list item, which is evaluated if it is a command (see the items of \list in CommandSpec)
		CmdOutput listItemOutput(std::pair<int, std::string> item, int lineNum, int numLines);
		// parseString() is also called from the sequencer thread (functions bound to instruments, beats etc.)
		// but the editors must only be modified from the main thread, as they are drawn there
		// so tracebacks set from other threads are queued and applied in update()
		void setEditorTraceback(int errorCode, std::string errorStr, int lineNum);
		void releaseEditorTraceback(int lineNum);
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

		// the data the sequencer plays (instruments, bars, loops, tempi, functions), which it shares with the parser (see utils.h)
		SharedData sharedData;
		Sequencer sequencer;
		// set by \score.play, and the sequencer starts at the next update of the app
		bool startSequencer{};

		// the data directory of LiveLily (bin/data), where relative paths start, e.g. of a SoundFont, set by the app at its start
		std::string dataDirectory;

		// the languages of the code of the panes
		enum languages {livelily, python, lua};
		// the names of the code (commands, and the names defined in a session) with their colors in the editor
		// by language and first character, and the colors that can be set by name
		std::map<int, std::map<char, std::map<std::string, Color>>> commandsMap;
		std::map<std::string, Color> colorNameMap;
		// which words the editor splits at digits when it colors them (e.g. the duration of c''4), see Editor::drawText()
		std::vector<std::string> commandsToNotTokenize;
		std::vector<std::string> keywords;
		// the colors of the instruments in the piano roll and the step sequencer, in the order they are created
		std::vector<std::string> instrumentColors = {"magenta", "green", "yellow", "cyan", "violet", "orchid", "orange"};

		// variables (e.g. x = 5), by their names
		std::map<std::string, variable> variables;
		// the kinds of definitions, for checking that a name is free (see checkNameAvailability())
		std::map<std::string, int> structTypes = {{"bar", 1}, {"bars", 2}, {"loop", 3}, {"function", 4}, {"list", 5}};

		// lists (\list): their items by their indexes, and their indexes by their names
		std::map<int, std::list<std::string>> listMap;
		std::map<std::string, int> listIndexes;
		bool storingList{};
		int lastListIndex{};
		int listIndexCounter{};
		bool traversingList{};
		// the lines of \list.traverse, with their line numbers, and how many curly brackets are still open while they are collected
		std::vector<std::pair<int, std::string>> traverseLines;
		int traverseDepth = 0;

		// functions (\function): their indexes by their names (the Function objects are in SharedData)
		std::map<std::string, int> functionIndexes;
		bool storingFunction{};
		// index of the function whose body is being stored, used only by the main thread
		// kept separate from lastFunctionIndex, which is also set when a function is called from the sequencer thread
		int storingFunctionIndex{};
		// the curly bracket depth while storing a function body, the function's own brackets included
		int functionBodyDepth{};
		int lastFunctionIndex{};
		// the function set with .onrelease of a function that \func.unbind has just unbound, which isFunction() runs, or -1
		int onReleaseFuncNdx = -1;

		// the state of parsing bars (\bar), loops (\loop), and bars defined together (\bars)
		// whether there was an error in a bar, so that its melodic lines are not stored
		bool barError{};
		bool parsingBar{};
		bool parsingBars{};
		bool parsingLoop{};
		// the name of a \bars definition, until it is stored as a loop
		std::string barsName;
		// a counter of the instruments parsed, for first bars that are not complete
		int numInstsParsed{};
		// the loop command sent to connected score parts
		std::string lastLoopCommand;
		// the name of the bars of \bars, and the counter of its passes, one for every bar
		std::string multiBarsName;
		int barsIterCounter{};
		// whether all the instruments of \bars have the same number of bars, and that number
		bool numBarsToParseSet{};
		int numBarsToParse{};
		// the number of bars defined by a \time, \tempo or \beatat with more than one value separated by |, inside \bars
		// 0 if there is none, and the command that defined it, for error messages
		int barsCmdNumBars{};
		std::string barsCmdName;
		int firstInstForBarsIndex{};
		bool firstInstForBarsSet{};

		// instruments (the Instrument objects are in SharedData)
		// groups of instruments (\group), by their names
		std::map<std::string, std::vector<std::string>> instGroups;
		// instruments merged to one staff with \merge, the first of each is the instrument whose staff they are drawn on (the host)
		// and the others are its voices, which are drawn on it (see applyMergeLayout())
		std::vector<std::vector<int>> mergeGroups;
		// the settings of \notedur and \accoffset, so that instruments created after these commands get them too
		// (each key of noteDurSettings is a \notedur method, e.g. staccato, and its value the duration as a fraction)
		std::map<std::string, float> noteDurSettings;
		bool accOffsetSet = false;
		float accOffsetCoef = 0;
		// the instrument whose line is parsed last, and the number of instruments
		std::string lastInstrument;
		int lastInstrumentIndex{};
		int tempNumInstruments{};
		// a warning of a command in a melodic line, which parseMelodicLine() returns if the line has no errors
		std::string melodicLineWarning;
		// the names of the notes, for parsing melodic lines
		char noteChars[8]{};
		// adding a natural after the same note with an accidental only in the same octave (\score.correct onoctave), or in any
		bool correctOnSameOctaveOnly{};
		// the duration of a step of the step sequencer and of the x - notation of rhythm staffs (\score.seqres)
		int seqResolution{};
		// dynamics from LilyPond notation (ppp to fff) as indexes, and as MIDI velocities, with a default 0 key mapped to 72
		std::vector<int> scoreDynamics = {-9, -6, -3, -1, 1, 3, 6, 9};
		std::map<int, int> mappedScoreDynamics = {{-9, 16}, {-6, 32}, {-3, 48}, {-1, 64}, {0, 72}, {1, 80}, {3, 96}, {6, 112}, {9, 127}};

		// the names of the commands of LiveLily (e.g. \merge), which can't be used as names of bars, loops, functions, lists
		// variables, instruments, groups, or OSC clients and servers, as these are called by their name with a backslash
		std::set<std::string> reservedNames;

		// input and output, through the interfaces of the platform (see midi.h, osc.h, serial.h)
		// the MIDI output, for the parser and the sequencer
		std::unique_ptr<MidiOutput> midi = makeMidiOutput();
		// OSC clients and servers (\oscclient, \oscserver), by their names, and the functions mapped to the addresses of servers
		std::map<std::string, std::shared_ptr<OscSender>> oscClients;
		std::map<std::string, std::shared_ptr<OscReceiver>> oscServers;
		std::map<std::string, std::vector<std::pair<std::string, std::string>>> oscServerFuncs;
		// the receiver of OSC messages to LiveLily itself (e.g. of the maestro, or keys typed in another LiveLily)
		std::shared_ptr<OscReceiver> oscReceiver = makeOscReceiver();
		bool oscReceiverIsSet{};
		// the serial port, and the serial devices as \listserialports listed them last, which \openserialport opens by their number
		std::unique_ptr<SerialPort> serial = makeSerialPort();
		std::vector<SerialDevice> serialDeviceList;
		bool serialPortOpen{};

		// score parts on other computers (\instname.sendto)
		// the IP and port of every instrument, so that instruments that send to the same server get the generic messages once
		std::map<int, std::pair<std::string, int>> instrumentOSCHostPorts;
		// time to wait for a positive/negative response from a server
		uint64_t scorePartResponseTime{};
		// pairs of instrument and bar where a score part client had an error receiving the data of the bar
		std::vector<std::pair<int, int>> scorePartErrors;
		// how many times a loop is called, by its index, in case some score parts haven't received the data of a bar correctly
		std::map<int, int> partsReceivedOKCounters;
		// sending the data of the bars of \bars to score parts, one at each frame
		bool waitToSendBarDataToParts{};
		bool sendBarDataToPartsBool{};
		int sendBarDataToPartsCounter{};
		int barDataOKFromPartsCounter{};
		int numScorePartSenders{};
		bool checkIfAllPartsReceivedBarData{};
		uint64_t sendBarDataToPartsTimeStamp{};

		// controlling LiveLily with an accelerometer (\maestro)
		std::string maestroAddress;
		std::string maestroToggleAddress;
		std::string maestroLevareAddress;
		int maestroValNdx{};
		bool maestroToggleSet{};
		bool receivingMaestro{};
		bool maestroInitialized{};
		int maestroBeatsIn{};
		unsigned long maestroTimeStamp{};
		float maestroValThresh{};
		std::vector<float> maestroVec;

		// Python (pyo) and audio
		bool pyoSet{};
		// the audio stream for pyo and the click track (see audio.h), and whether it has been set up (see startSoundStream())
		// with the callback of its samples, which the front end sets (it copies the output for the oscilloscope)
		AudioCallback* audioCallback = nullptr;
		std::unique_ptr<AudioStream> audioStream = makeAudioStream();
		bool soundStreamSet = false;
		// \samplerate, \buffersize, \inchannels, \outchannels, and the devices of \setaudiodev, \setinaudiodev and \setoutaudiodev
		int sampleRate{};
		int bufferSize{};
		int inChannels{};
		int outChannels{};
		AudioDevice inSoundDevice;
		AudioDevice outSoundDevice;
		bool inSoundDeviceSet{};
		bool outSoundDeviceSet{};
};

#endif
