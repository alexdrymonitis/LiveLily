#ifndef OF_APP_H
#define OF_APP_H

#include "ofMain.h"
#include "ofxOsc.h"
#include "ofxMidi.h"
#include <map>
#include <vector>
#include <utility> // to add pair
#include "editor.h"
#include "instrument.h"
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
// size of cosine table to control the transparency of the beat visualization rectangle
#define BEATVIZBRIGHTNESS 255
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
#define LOWESTKEY 21 // should be 21 // this is an A four octaves below tuning A
#define HIGHESTKEY 108 // should be 108 // this is a C four octaves above middle C
#define KEYSWIDTHCOEFF 8

// class for storing data concerning livelily functions
class Function
{
	public:
		Function();
		void allocate(size_t size);
		void copyStr(std::string s, size_t offset);
		std::string printStr();
		void setIndex(int index);
		int getIndex();
		void setName(std::string funcName);
		std::string getName();
		bool getNameError();
		void setArgument(std::string str);
		void resetArgumentIndex();
		int getNumArgs();
		std::string getArgument(int ndx);
		int getArgError();
		void setBind(int boundInstindex, int callStep, int stepIncr, int numTimes);
		int releaseBind();
		void onUnbindFunc(int funcNdx);
		int getBoundInst();
		int getCallingStep();
		void addStepIncrement(int seqStep);
		void resetCallingStep();
		int incrementCallCounter();
		void setStepCounter(int num);
		void clear();
		~Function();
	private:
		size_t strSize;
		int funcNdx;
		bool memoryAllocated;
		int callingStep;
		int numRepeatTimes;
		bool numRepeatTimesSet;
		int repetitionCounter;
		int initCallingStep;
		int stepIncrement;
		int stepCounter;
		int sequencerStep;
		int boundInstNdx;
		int numArgs;
		bool nameError;
		int argErrorType;
		int onUnbindFuncNdx;
		bool onUnbindSet;
		// overhead for not needing to allocate memory durint runtime
		// up to 50 arguments and up to 20 chars for each arg (+1 for num terminating char)
		char arguments[50][21];
		char name[51]; // again overhead
		char *str;
};

struct pyStdoutStrKeyModifier
{
	bool isModifier;
	int ndxInc;
	int modifier;
	int action;
};

// vectors and variables that need to be shared between main and threaded classes
struct SharedData
{
	// a map of Instruments()
	std::map<int, Instrument> instruments;
	// maps of string and int to easily store data based on strings
	// but iterate over the data in the sequencer and score renderer fast based on ints
	std::map<std::string, int> instrumentIndexes;
	std::map<int, int> instrumentIndexesOrdered; // for sending to parts in specific order
	std::map<std::string, int> barsIndexes;
	std::map<std::string, int> loopsIndexes;
	// since string keys are sorted, we need a way to retrieve them in arbirtary order
	// e.g. a bar named "5" stored first, will be moved after a bar named "4" that is stored later on
	// the map below is ordered, but we store the index as a key, which is stored with an incrementing value anyway
	std::map<int, std::string> loopsOrdered;
	// same goes for bars
	std::map<int, std::string> barsOrdered;
	// the map below stores the strings of each defined bar, since these can be edited or deleted in the editor.
	// These can be used elsewhere, like sent to an AI model that is trained on LiveLily files
	// the key is the bar name and the value is another std::string assembled by the strings of the bar lines
	// separated with newline characters
	std::map<std::string, std::string> barLines;
	// the map below keeps track of how many variants of each loop we create so we can use this as a name extension
	std::map<int, int> loopsVariants;

	// the map below is used to animate the editor
	std::map<int, int> barsConnectedToPanes;

	// the vector below will store unique OSC clients, one for each server
	// additional clients that send to the same server will be ignored
	// as this is intended for generic messages, like storeNotes()
	// it is defined here because the sequencer needs access to it too
	std::vector<int> grouppedOSCClients;
#ifdef USEPYO
	// the Pyo object is created here so that the sequencer has access to it
	Pyo pyo;
#endif
	// handle strings typed from a Python pane to a LiveLily pane
	std::string pyStdoutStr;
	size_t pyStdoutStrNdx;
	bool typePyStrCharByChar;
	bool typePyStrLineByLine;
	int whichPyPane;
	bool typePyStr;
	std::vector<std::string> pyStdoutStrVec;
	int pyStdoutStrVecNdx;
	std::vector<pyStdoutStrKeyModifier> pyStdoutKeyModifiers;
	int pyStdoutKeyModNdx;
	std::vector<int> pyStdoutKeyModNdxs;

	std::map<int, int> distBetweenBeats;

	int numInstruments;

	float notesXStartPnt;

	int noteWidth;
	int noteHeight;

	// articulation strings so each program can treat them differently
	std::string articulSyms[8];
	bool showNotes;
	bool showPianoRoll;
	bool showScope;
	// from the piano roll variables below, only pianoRollTimeStamp needs to be in SharedData
	// but we define all of them here for consistency
	int pianoRollNumWhiteKeys;
	float pianoRollMinDur;
	float pianoRollKeysWidth;
	float pianoRollKeysHeight;
	uint64_t pianoRollTimeStamp;
	std::map<int, int> pianoRollAccidentals = {{0, -2}, {2, -1}, {6, 1}, {8, 2}};
	std::vector<int> pianoRollAccidentalKeys = {1, 3, 6, 8, 10};
	// a map to map MIDI notes to seven notes only (the white keys) including black keys mapped to the previous white key
	// to use this with a clavier that starts from A instead of C, we start with A = 0
	std::map<int, int> pianoRollTwelveToSevenMap = {{0, 0}, {1, 0}, {2, 1}, {3, 2}, {4, 2}, {5, 3}, {6, 3}, {7, 4}, {8, 5}, {9, 5}, {10, 6}, {11, 6}};
	int pianoRollNaturalNotes[7] = {0, 2, 4, 5, 7, 9, 11};
	// beat counter used in case we need to send it over OSC or for possible other reasons
	int beatCounter;
	// animation variables
	bool animate;
	bool setAnimation;
	bool setBeatAnimation;
	bool beatViz;
	float beatVizStepsPerMs;
	uint64_t beatVizRampStart;
	uint64_t beatVizTimeStamp;
	int beatVizDegrade;
	int beatVizCosTab[BEATVIZBRIGHTNESS+1];
	bool beatAnimate;
	bool beatTypeCommand;
	int beatVizType;
	bool beatUpdated;
	// the variable below is used by the serquencer to iterate over one loop
	unsigned thisBarIndex;
	int loopIndex;
	int tempLoopIndex;
	int numBars;
	int thisPosition;
	int prevNumBars;
	int prevPosition;
	int barCounter; // used for visualization on the score
	std::map<int, std::vector<int>> loopData;
	// a boolean if we call a pattern while the sequencer is running
	bool updateLoop;
	// three different tempo placeholders
	// one for the running sequencer (tempo)
	// one for updating tempo while sequencer is running (newTempo)
	// and one to hold the tempo in ms without the conversion
	// to the global minimum duration
	std::map<int, double> tempo;
	std::map<int, double> tempoMs;
	std::map<int, int> BPMTempi;
	std::map<int, int> BPMMultiplier;
	std::map<int, bool> beatAtDifferentThanDivisor;
	std::map<int, int> beatAtValues;
	std::map<int, int> tempoBaseForScore;
	std::map<int, bool> BPMDisplayHasDot;
	// variables for positioning staffs for every pattern
	std::map<int, float> barFirstStaffAnchor;
	float maxBarFirstStaffAnchor;
	float allStaffDiffs;
	float yStartPnts[3];

	float staffLinesDist;
	int longestInstNameWidth;
	// variable to leave some space between edge of score and
	// instrument name, instrument name and staff, etc.
	int blankSpace;

	// STDERR
	float tracebackYCoordAnchor;
	float tracebackYCoord;
	float tracebackBase;

	std::map<int, Function> functions;

	std::map<int, int> numBeats;

	int scoreFontSize;

	std::map<int, int> numerator;
	std::map<int, int> denominator;

	int screenWidth;
	int screenHeight;
	int middleOfScreenX;
	int middleOfScreenY;
	int staffXOffset;
	int staffWidth;
	bool drawLoopStartEnd;
	int scoreHorizontalViewToggle;

	// MIDI clock
	unsigned long PPQN;
	std::map<int, uint64_t> PPQNPerUs;
	unsigned long PPQNCounter;
	uint64_t PPQNTimeStamp;
};

struct CmdInput
{
	std::vector<std::string> inputVec;
	std::vector<std::string> afterCmdVec;
	bool hasBrackets;
	bool isMainCmd;
};

struct CmdOutput
{
	int errorCode;
	std::string errorStr;
	size_t toPop;
	std::vector<std::string> outputVec;
};

class Sequencer : public ofThread
{
	public:
		void setup(SharedData *sData);
		void setSendMidiClock(bool sendMidiClockState);
		void start();
		void stop();
		void stopNow();
		void update();
		bool isUpdated();
		void setSequencerRunning(bool seqRun);
		void setFinish(bool finishState);
		void setCountdown(int num);
		void setMidiTune(int tuneVal);

		ofxMidiOut midiOut;
		std::vector<ofxMidiOut> midiOuts;
		std::vector<std::string> midiOutPorts;
		std::map<int, int> midiPortsMap;

	private:
		int setBarIndex(bool increment);
		void checkMute();
		void sendAllNotesOff();
		void sendBeatVizInfo(int bar);
		float midiToFreq(float midiNote);
		float mapVal(float inVal, float fromLow, float fromHigh, float toLow, float toHigh);
		void startGlissando(int instNdx, int bar, int barDataCounter);
		void runGlissando(int instNdx, int bar);
		void sendToParts(ofxOscMessage m, bool delay);
		void sendSequencerStateToParts(bool state);
		void sendLoopIndexToParts();
		void sendStopCountdownToParts();
		void sendCountdownToParts(int coundown);
		void sendFinishToParts(bool finishState);
		void threadedFunction();

		SharedData *sharedData;

		ofxOscSender oscSender;
		ofTimer timer;
		bool runSequencer;
		bool sequencerRunning;
		bool updateSequencer;
		bool sequencerUpdated;
		bool updateTempo;
		bool finish;
		bool mustStop;
		bool mustStopCalled;
		int onNextStartMidiByte;
		uint64_t tickCounter;
		int beatCounter;
		int sendBeatVizInfoCounter;
		unsigned thisBarIndex;
		bool firstIter;
		bool endOfBar;
		int numBeats;
		bool finished;
		bool sendMidiClock;
		bool muteChecked;
		int countdownCounter;
		bool countdown;
		int midiTuneVal;

		/* iterators for accessing data in Instrument objects */
		std::map<std::string, int>::iterator instNamesIt;
		//std::map<int, Instrument>::iterator instMapIt;

		/* and reverse iterators */
		std::map<std::string, int>::reverse_iterator instRevIt;
		std::map<int, Instrument>::reverse_iterator instMapRevIt;
};

class ofApp : public ofBaseApp
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
		void drawPianoRoll();
		void drawBlackKeysOutline(float xPos, float yPos);
		void drawScope();

		void sendBeatVizInfo(int bar);
		void moveCursorOnShiftReturn();
		//---------------------------------
		// the following two functions execute key commands
		// so each can be called from any of the overloaded keyPressed() and keyRelease() below
		// functions and lock the mutex without creating a dead lock
		void executeKeyPressed(int key);
		void executeKeyReleased(int key);
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
		void storeActiveEditorElement(int instNdx, int barNdx, int dataCounter, bool state);
		//---------------------------------
		// parsing functions
		void parseStrings(int index, int numLines);
		std::pair<int, std::string> parseString(std::string str, int lineNum, int numLines);
		CmdOutput expandCommands(const std::string& input, int lineNum, int numLines);
		std::vector<std::string> tokenizeExpandedCommands(const std::string& input);
		CmdOutput parseExpandedCommands(const std::vector<std::string>& tokens, int lineNum, int numLines);
		CmdOutput parseCommand(CmdInput cmdInput, int lineNum, int numLines);
		std::pair<int, std::string> parseMelodicLine(std::vector<std::string> v, int lineNum, int numLines);
		std::pair<int, std::string> parseBarLoop(std::string str, int lineNum, int numLines);
		std::pair<bool, CmdOutput> isInstrument(std::vector<std::string>& commands, int lineNum, int numLines);
		std::pair<bool, CmdOutput> isBarLoop(std::vector<std::string>& commands, bool isMainCmd, int lineNum, int numLines);
		std::pair<bool, CmdOutput> isFunction(std::vector<std::string>& commands, int lineNum, int numLines);
		std::pair<bool, CmdOutput> isList(std::vector<std::string>& commands, int lineNum, int numLines);
		std::pair<int, std::string> listItemExists(size_t listItemNdx);
		std::pair<bool, CmdOutput> isOscClient(std::vector<std::string>& commands, int lineNum, int numLines);
		std::pair<bool, CmdOutput> isGroup(std::vector<std::string>& commands, int lineNum, int numLines);
		//---------------------------------
		// command typing/executing funcitons
		void typeShellCommand(int key);
		void replaceShellStrNewlines();
		void executeShellCommand();
		void formatShellLsOutput(std::vector<std::string> lsOutputTokens);
		//---------------------------------
		// miscellaneous functions
		int msToBPM(unsigned long ms);
		// the following two functions are used in drawTraceback to sort the indexes based on the
		// time stamps. it is copied from https://www.geeksforgeeks.org/quick-sort/
		int partition(uint64_t arr[], int arr2[], int low, int high);
		void quickSort(uint64_t arr[], int arr2[], int low, int high);
		//---------------------------------
		// send data to score parts
		void sendToParts(ofxOscMessage m, bool delay);
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
		bool startsWith(std::string a, std::string b);
		bool endsWith(std::string a, std::string b);
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
		std::string replaceCharInStr(std::string str, std::string a, std::string b);
		//---------------------------------
		// list functions
		void storeList(std::string str);
		CmdOutput traverseList(std::string str, int lineNum, int numLines);
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
		CmdOutput genOutput(std::string str);
		CmdOutput genOutput(std::vector<std::string> v);
		CmdOutput genOutput(std::string str, size_t toPop);
		CmdOutput genOutput(std::vector<std::string> v, size_t toPop);
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
		void showScore(int ndx);
		CmdOutput maestroCommands(std::vector<std::string>& commands, int lineNum, int numLines);
		bool isScoreVisible();
		//---------------------------------
		// instrument functions
		void initializeInstrument(std::string instName);
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
		void audioIn(ofSoundBuffer & input);
		void audioOut(ofSoundBuffer & buffer);

		// a mutex to avoid accessing the same data by different threads at the same time
    	ofMutex mutex;

		std::map<int, Editor> editors; // keys are panes indexes
		int whichPane; // this variable indexes the keys of the editors map above
		std::map<int, int> numPanes; // rows and columns of panes
		int paneSplitOrientation;
		float screenSplitHorizontal;
		float oneCharacterWidth;
		int halfCharacterWidth;

		SharedData sharedData;
		Sequencer sequencer;

		// Pyo stuff
		ofSoundStream soundStream;
		bool pyoSet;
		int sampleRate;
		int bufferSize;
		int nChannels;
		ofSoundDevice inSoundDevice;
		ofSoundDevice outSoundDevice;
		bool inSoundDeviceSet;
		bool outSoundDeviceSet;
		// oscilloscope drawing stuff
		ofSoundBuffer scopeBuffer;
		ofPolyline scopeWaveformLeft;
		ofPolyline scopeWaveformRight;
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
		std::vector<std::string> shellCommands = {":save", ":w", ":load", ":ls", ":pwd", ":q"};

		// interfacing stuff
		std::map<std::string, ofxOscSender> oscClients;
		ofSerial serial;
		std::vector<ofSerialDeviceInfo> serialDeviceList;
		bool serialPortOpen;

		// map of instrument index and pair of IP and port
		// so we can group instruments that send their data to the same server
		// this way, generic messages will be sent only once
		std::map<int, std::pair<std::string, int>> instrumentOSCHostPorts;

		// time to wait for a positive/negative response from a server
		uint64_t scorePartResponseTime;
		// std::vector of pairs with instrument index and bar index received from a score part client
		// where an error occured in receiving bar data
		std::vector<std::pair<int, int>> scorePartErrors;
		// std::map with loop indexes as keys and ints as values to count how many times we call a loop
		// useful in case some instruments (servers) haven't received the data of a bar correctly
		std::map<int, int> partsReceivedOKCounters;

		// logic to convert dynamics values set from Lilypond notation to indexes and MIDI velocities
		std::vector<int> scoreDynamics = {-9, -6, -3, -1, 1, 3, 6, 9};
		// below we also set a default 0 key std::mapped to 72 MIDI velocity
		std::map<int, int> mappedScoreDynamics = {{-9, 16}, {-6, 32}, {-3, 48}, {-1, 64}, {0, 72}, {1, 80}, {3, 96}, {6, 112}, {9, 127}};

		bool startSequencer;
		bool midiPortOpen;

		std::map<int, std::map<char, std::map<std::string, ofColor>>> commandsMap;
		std::map<std::string, ofColor> colorNameMap;
		// the two vectors below is used to determine if we must tokenize words based on digits
		std::vector<std::string> commandsToNotTokenize;
		std::vector<std::string> keywords;
		// the map of vectors below is used to determine the number of lines to execute
		// if we start from the first line of a chunk that is not indented
		std::map<int, std::vector<std::string>> noIndentCheck;
		enum languages {livelily, python, lua};
		// vector of strings of command names that should not be expanded
		std::vector<std::string> nonExpandableCommands = {"\\bar", "\\bars", "\\loop", "\\group", "\\function"};

		// instrument groups
		std::map<std::string, std::vector<std::string>> instGroups;

		// various lists
		std::map<int, std::list<std::string>> listMap;
		std::map<std::string, int> listIndexes;
		bool storingList;
		int lastListIndex;
		int listIndexCounter;
		bool traversingList;

		// variables for controlling LiveLily with an accelerometer
		std::string maestroAddress;
		std::string maestroToggleAddress;
		std::string maestroLevareAddress;
		int maestroValNdx;
		bool maestroToggleSet;
		bool receivingMaestro;
		bool maestroInitialized;
		int maestroBeatsIn;
		unsigned long maestroTimeStamp;
		float maestroValThresh;
		std::vector<float> maestroVec;

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

		ofxOscReceiver oscReceiver;
		bool oscReceiverIsSet;

		std::map<int, std::string> allStrings;
		std::map<int, int> allStringStartPos;
		std::map<int, std::vector<int>> allStringTabs;
		//std::vector<std::vector<int>> curlyIndexes;

		// map of function names and indexes
		// the map with int and Function objects is in SharedData
		std::map<std::string, int> functionIndexes;
		int functionBodyOffset;
		bool storingFunction;
		int lastFunctionIndex;

		int fontSize; // the size of the font of the editor
		int instFontSize; // the size of the font of the instrument display
		float cursorHeight;

		bool shiftPressed;
		bool ctrlPressed;
		bool altPressed;

		// global line width
		int lineWidth;

		// boolean to determine whethere there was an error when storing a bar
		// useful to avoid storing melodic lines in such a case
		bool barError;
		// variable used for copying contents of bars
		int barToCopy;

		int minDur;

		// variable to determine when to start storing patterns
		int whenToStore;
		// temporary storage of editor lines, needed for \\bars command
		std::vector<std::string> tempLines;
		// temporary storage for separated bars, needed for \\bars command
		std::vector<std::string> allBars;
		// and a boolean to allow storing a pattern and to insert lines
		bool storePattern;
		bool inserting;
		// a string to hold the name of a \\bars command argument
		// until it is stored as a loop
		std::string barsName;
		// a counter of parsed instruments that is used for first, incomplete bars
		int numInstsParsed;

		// STDERR
		bool parsingCommand;

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
		// a vector of colors to appear in order of instrument creation for the piano roll
		std::vector<std::string> instrumentColors = {"magenta", "green", "yellow", "cyan", "violet", "orchid", "orange"};
		// map that stores which editors can receive data from OSC
		std::map<int, bool> fromOsc;
		// and a map with the OSC address for each editor
		std::map<int, std::string> fromOscAddr;

		std::map<int, int> instNameWidths;
		// we need an index to point to the correct 2D vector
		int patternIndex;
		// the boolean below determines whether we're parsing a bar
		bool parsingBar;
		// and one for parsing multiple bars
		bool parsingBars;
		// and one for parsing loops
		bool parsingLoop;
		// a string to hold the loop command to be sent over OSC to connected servers
		std::string lastLoopCommand;
		// a boolean for sending data defined with \bars to score parts one at each frame
		bool waitToSendBarDataToParts;
		// and one to denote that all bars have finished parsing so we can start sending to parts
		bool sendBarDataToPartsBool;
		// a counter that increments at each frame
		int sendBarDataToPartsCounter;
		// another counter that increments when we receive an OK from parts
		int barDataOKFromPartsCounter;
		int numScorePartSenders;
		bool checkIfAllPartsReceivedBarData;
		uint64_t sendBarDataToPartsTimeStamp;
		// a string to termporarily store the name of the multiple bars
		std::string multiBarsName;
		// and a counter for the iterations of multiple bars parsing
		int barsIterCounter;
		bool numBarsToParseSet;
		// a variable to check if all instruments parsed the same number of bars
		int numBarsToParse;
		char noteChars[8];
		std::string lastInstrument;
		int lastInstrumentIndex;
		int firstInstForBarsIndex;
		bool firstInstForBarsSet;
		int tempNumInstruments;
		// about adding a natural after the same note has been inserted with an accidental
		bool correctOnSameOctaveOnly;
		bool showBarCount;
		bool showTempo;
};

#endif
