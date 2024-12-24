#ifndef OF_APP_H
#define OF_APP_H

#include "ofMain.h"
#include "ofxOsc.h"
#include "ofxMidi.h"
#include <map>
#include <vector>
#include "editor.h"
#include "instrument.h"

#define OFRECVPORT 8001
#define OFSENDPORT 1234 // for sending sequencer data to other software
#define SCOREPARTPORT 9000
#define KEYCLIENTPORT 9100
#define HOST "localhost"
#define BACKGROUND 15

// 64ths are the minimum note duration
// so we use 4 steps per this duration as a default
#define MINDUR 256
// size of cosine table to control the transparency of the beat visualization rectangle
#define BEATVIZBRIGHTNESS 255
#define BEATVIZCOEFF 0.8 // coefficient to reduce maximum brightness of rectange
#define MINDB 60
#define TRACEBACKDUR 2000
#define MINTRACEBACKLINES 2

#define OSCPORT 9050 // for receiving OSC messages in editors

#define WINDOW_RESIZE_GAP 50

#define INBUFFSIZE 5

#define SENDBARDATA_WAITDUR 1000 // in milliseconds
#define NOTESXOFFSETCOEF 3 // multiplication coefficient for giving offset to the notes

// class for storing data concerning livelily functions
class Function
{
	public:
		Function() {
			memoryAllocated = false;
			callingStep = initCallingStep = 1;
			stepIncrement = 0;
			stepCounter = 0;
			sequencerStep = 0;
			repetitionCounter = 0;
			numRepeatTimesSet = false;
			boundInstNdx = -1;
			numArgs = 0;
			nameError = false;
			argErrorType = 0;
			onUnbindFuncNdx = -1;
			onUnbindSet = false;
		}
		void allocate(size_t size) {
			str = new char [size];
			memoryAllocated = true;
			strSize = size;
		}

		void copyStr(string s, size_t offset) {
			strcpy(str+offset, s.c_str());
		}

		string printStr() {
			string strToPrint = "";
			for (size_t i = 0; i < strSize-1; i++) {
				strToPrint += str[i];
			}
			return strToPrint;
		}

		void setIndex(int index) {
			funcNdx = index;
		}

		int getIndex() {
			return funcNdx;
		}

		void setName(string funcName) {
			if (funcName.size() > 50) {
				nameError = true;
				return;
			}
			nameError = false;
			strcpy(name, funcName.c_str());
		}

		string getName() {
			string str(name);
			return str;
		}

		bool getNameError() {
			return nameError;
		}

		void setArgument(string str) {
			if (numArgs > 50) {
				argErrorType = 1;
				return;
			}
			if (str.size() > 20) {
				argErrorType = 2;
				return;
			}
			argErrorType = 0;
			strcpy(arguments[numArgs++], str.c_str());
		}

		void resetArgumentIndex() {
			numArgs = 0;
		}
		
		int getNumArgs() {
			return numArgs;
		}

		string getArgument(int ndx) {
			if (ndx >= 50) return "";
			string str(arguments[ndx]);
			return str;
		}

		int getArgError() {
			return argErrorType;
		}

		void setBind(int boundInstindex, int callStep, int stepIncr, int numTimes) {
			boundInstNdx = boundInstindex;
			// - 1 because LiveLily is 1-based, but C++ is 0-based
			callingStep = callStep - 1;
			initCallingStep = callingStep;
			stepIncrement = stepIncr;
			numRepeatTimes = numTimes;
			if (numRepeatTimes > 0) numRepeatTimesSet = true;
		}

		int releaseBind() {
			boundInstNdx = -1;
			stepIncrement = 0;
			stepCounter = 0;
			repetitionCounter = 0;
			numRepeatTimesSet = false;
			if (onUnbindSet) return onUnbindFuncNdx;
			else return -1;
		}

		void onUnbindFunc(int funcNdx) {
			onUnbindFuncNdx = funcNdx;
			onUnbindSet = true;
		}

		int getBoundInst() {
			return boundInstNdx;
		}

		int getCallingStep() {
			return callingStep;
		}

		void addStepIncrement(int seqStep) {
			callingStep += stepIncrement;
			sequencerStep = seqStep + 1; // + 1 so that the modulo in resetCallingStep() works properly
		}

		void resetCallingStep() {
			if (initCallingStep != callingStep && callingStep > 0) {
				callingStep %= sequencerStep;
				initCallingStep = callingStep;
			}
		}

		int incrementCallCounter() {
			int releaseBindFuncNdx = -1;
			if (numRepeatTimesSet) {
				repetitionCounter++;
				if (repetitionCounter == numRepeatTimes) {
					releaseBindFuncNdx = releaseBind();
				}
			}
			return releaseBindFuncNdx;
		}

		void setStepCounter(int num) {
			// gets the sequence steps of the bound instrument
			stepCounter = num;
		}

		void clear() {
			delete str;
			memoryAllocated = false;
		}

		~Function() {
			if (memoryAllocated) clear();
		}

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

// vectors and variables that need to be shared between main and threaded classes
typedef struct _SharedData
{
    // a mutex to avoid accessing the same data by different threads at the same time
    //ofMutex mutex;
	// a map of Instruments()
	map<int, Instrument> instruments;
	// maps of string and int to easily store data based on strings
	// but iterate over the data in the sequencer and score renderer fast
	// based on ints
	map<string, int> instrumentIndexes;
	map<int, int> instrumentIndexesOrdered; // for sending to parts in specific order
	map<string, int> barsIndexes;
	map<string, int> loopsIndexes;
	// since string keys are sorted, we need a way to retrieve them in arbirtary order
	// e.g. a bar named "5" stored first, will be moved after a bar named "4" that is stored later on
	// the map below is ordered, but we store the index as a key, which is stored with an incrementing value anyway
	map<int, string> loopsOrdered;
	// the map below keeps track of how many variants of each loop we create so we can use this as a name extension
	map<int, int> loopsVariants;

	map<int, int> distBetweenBeats;

	int numInstruments;

	float notesXStartPnt;

	int noteWidth;
	int noteHeight;

	// articulation strings so each program can treat them differently
	string articulSyms[8];
	bool showScore;
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
	// the variable below is used by the serquencer to iterate over one loop
	unsigned thisLoopIndex;
	int loopIndex;
	int tempBarLoopIndex;
	int barCounter; // used for visualization on the score
	map<int, vector<int>> loopData;
	// a boolean if we call a pattern while the sequencer is running
	bool updateLoop;
	// three different tempo placeholders
	// one for the running sequencer (tempo)
	// one for updating tempo while sequencer is running (newTempo)
	// and one to hold the tempo in ms without the conversion
	// to the global minimum duration
	map<int, double> tempo;
	map<int, double> tempoMs;
	map<int, int> BPMTempi;
	map<int, int> BPMMultiplier;
	map<int, bool> beatAtDifferentThanDivisor;
	map<int, int> beatAtValues;
	map<int, int> tempoBaseForScore;
	map<int, bool> BPMDisplayHasDot;
	// variables for positioning staffs for every pattern
	map<int, float> barFirstStaffAnchor;
	float maxBarFirstStaffAnchor;
	float allStaffDiffs;
	float yStartPnts[2];

	float staffLinesDist;
	int longestInstNameWidth;
	// variable to leave some space between edge of score and
	// instrument name, instrument name and staff, etc.
	int blankSpace;

	// STDERR
	float tracebackYCoordAnchor;
	float tracebackYCoord;
	float tracebackBase;

	map<int, Function> functions;
	
	map<int, int> numBeats;

	int scoreFontSize;

	map<int, int> numerator;
	map<int, int> denominator;

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
	map<int, uint64_t> PPQNPerUs;
	unsigned long PPQNCounter;
	uint64_t PPQNTimeStamp;
} SharedData;

typedef struct _intPair
{
	int first;
	int second;
} intPair;

class Sequencer : public ofThread
{
	public:
		void setup(SharedData *sData);
		void setSendMidiClock(bool sendMidiClockState);
		void start();
		void stop();
		void stopNow();
		void update();
		void setSequencerRunning(bool seqRun);
		void setFinish(bool finishState);
		void setCountdown(int num);
		void setMidiTune(int tuneVal);
		float midiToFreq(int midiNote);

		ofxMidiOut midiOut;
		vector<ofxMidiOut> midiOuts;
		vector<string> midiOutPorts;

	private:
		int setBarIndex(bool increment);
		void checkMute();
		void sendAllNotesOff();
		void sendBeatVizInfo(int bar);
		void threadedFunction();

		SharedData *sharedData;

		ofxOscSender oscSender;
		ofTimer timer;
		bool runSequencer;
		bool sequencerRunning;
		bool updateSequencer;
		bool updateTempo;
		bool finish;
		bool mustStop;
		bool mustStopCalled;
		int onNextStartMidiByte;
		uint64_t tickCounter;
		int beatCounter;
		int sendBeatVizInfoCounter;
		unsigned thisLoopIndex;
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
		map<string, int>::iterator instNamesIt;
		//map<int, Instrument>::iterator instMapIt;

		/* and reverse iterators */
		map<string, int>::reverse_iterator instRevIt;
		map<int, Instrument>::reverse_iterator instMapRevIt;
};

class ofApp : public ofBaseApp
{
	public:
		// basic OF program structure
		void setup();
		void update();
		void draw();
		// custom drawing functions
		void drawTraceback();
		void drawScore();

		void moveCursorOnShiftReturn();
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
		// the following two functions are used in drawTraceback to sort the indexes based on the
		// time stamps. it is copied from https://www.geeksforgeeks.org/quick-sort/ 
		int partition(uint64_t arr[], int arr2[], int low, int high);
		void quickSort(uint64_t arr[], int arr2[], int low, int high);
		// send data to score parts
		void sendToParts(ofxOscMessage m, bool delay);
		void sendCountdownToParts(int countdown);
		void sendStopCountdownToParts();
		void sendSizeToPart(int instNdx, int size);
		void sendNumBarsToPart(int instNdx, int numBars);
		void sendAccOffsetToPart(int instNdx, float accOffset);
		void sendLoopIndexToParts();
		void sendSequencerStateToParts(bool state);
		void sendNewBarToParts(string barName, int barIndex);
		void sendScoreChangeToPart(int instNdx, bool scoreChange);
		void sendChangeBeatColorToPart(int instNdx, bool changeBeatColor);
		void sendFullscreenToPart(int instNdx, bool fullscreen);
		void sendCursorToPart(int instNdx, bool cursor);
		void sendFinishToParts(bool finishState);
		// debugging
		void printVector(vector<int> v);
		void printVector(vector<string> v);
		void printVector(vector<float> v);
		// functions for parsing strings
		bool startsWith(string a, string b);
		bool endsWith(string a, string b);
		bool isNumber(string str);
		bool isFloat(string str);
		vector<int> findRepetitionInt(string str, int multIndex);
		int findNextStrCharIdx(string str, string compareStr, int index);
		bool areBracketsBalanced(string str);
		bool areParenthesesBalanced(string str);
		std::pair<int, string> expandStringBasedOnBrackets(const string& str);
		std::pair<int, string> expandChordsInString(const string& firstPart, const string& str);
		std::pair<int, string> expandSingleChars(string str);
		string detectRepetitions(string str);
		// string handling
		vector<int> findIndexesOfCharInStr(string str, string charToFind);
		string replaceCharInStr(string str, string a, string b);
		vector<string> tokenizeString(string str, string delimiter);
		void resetEditorStrings(int index, int numLines);
		void parseStrings(int index, int numLines);
		std::pair<int, string> parseString(string str, int lineNum, int numLines);
		void storeList(string str);
		std::pair<int, string> traverseList(string str, int lineNum, int numLines);
		void fillInMissingInsts(int barIndex);
		void createEmptyMelody(int index);
		void resizePatternVectors();
		void sendPushPopPattern();
		int storeNewBar(string barName);
		void storeNewLoop(string loopName);
		int getBaseDurValue(string str, int denominator);
		std::pair<int, string> getBaseDurError(string str);
		std::pair<int, string> parseCommand(string str, int lineNum, int numLines);
		std::pair<bool, std::pair<int, string>> isInstrument(vector<string>& commands, int lineNum, int numLines);
		std::pair<bool, std::pair<int, string>> isBarLoop(vector<string>& commands, int lineNum, int numLines);
		std::pair<bool, std::pair<int, string>> isFunction(vector<string>& commands, int lineNum, int numLines);
		std::pair<bool, std::pair<int, string>> isList(vector<string>& commands, int lineNum, int numLines);
		std::pair<bool, std::pair<int, string>> isOscClient(vector<string>& commands, int lineNum, int numLines);
		std::pair<int, string> functionFuncs(vector<string>& commands);
		int getLastLoopIndex();
		int getLastBarIndex();
		int getPrevBarIndex();
		int getPlayingBarIndex();
		std::pair<int, string> stripLineFromBar(string str);
		void deleteLastBar();
		void deleteLastLoop();
		void copyMelodicLine(int bar);
		std::pair<int, string> parseBarLoop(string str, int lineNum, int numLines);
		bool areWeInsideChord(size_t size, unsigned i, unsigned *chordNotesIndexes);
		std::pair<int, string> parseMelodicLine(string str);
		// set coordinates for all panes
		void setPaneCoords();
		// set the global font size to calculate dimensions
		void setFontSize();
		// various commands for the score
		std::pair<int, string> scoreCommands(vector<string>& commands, int lineNum, int numLines);
		// initialize an instrument
		void initializeInstrument(string instName);
		// the sequencer functions of the system
		int setPatternIndex(bool increment);
		unsigned setLoopIter(int ptrnIdx);
		int setInstIndex(int ptrnIdx, int i);
		int setBar(int ptrnIdx, int idx, int i);
		// staff and notes handling
		void setScoreCoords();
		void setScoreNotes(int barIndex);
		void setNotePositions(int bar);
		void setNotePositions(int bar, int numBars);
		void swapScorePosition(int orientation);
		void setScoreSizes();
		void calculateStaffPositions(int bar, bool windowChanged);
		// release memory on exit
		void exit();
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

		map<int, Editor> editors; // keys are panes indexes
		int whichPane; // this variable indexes the keys of the map above
		// rows and pair of number of columns in each row and line width between panes horizontally
		map<int, int> numPanes; 
		int paneSplitOrientation;
		float screenSplitHorizontal;

		SharedData sharedData;
		Sequencer sequencer;

		map<string, ofxOscSender> oscClients;
		ofSerial serial;

		// map of instrument index and pair of IP and port
		// so we can group instruments that send their data to the same server
		// this way, generic messages will be sent only once
		map<int, std::pair<string, int>> instrumentOSCHostPorts;
		// the vector below will store unique OSC clients, one for each server
		// additional clients that send to the same server will be ignored
		// as this is intended for generic messages, like storeNotes()
		vector<int> grouppedOSCClients;

		// time to wait for a positive/negative response from a server
		uint64_t scorePartResponseTime;
		// vector of pairs with instrument index and bar index received from a score part client
		// where an error occured in receiving bar data
		vector<std::pair<int, int>> scorePartErrors;
		// map with loop indexes as keys and ints as values to count how many times we call a loop
		// useful in case some instruments (servers) haven't received the data of a bar correctly
		map<int, int> partsReceivedOKCounters;

		bool startSequencer;
		bool midiPortOpen;

		map<int, map<string, ofColor>> commandsMap;
		map<int, map<string, ofColor>> commandsMapSecond;
		enum languages {livelily, python, lua};

		// various lists
		map<int, list<string>> listMap;
		map<string, int> listIndexes;
		bool storingList;
		int lastListIndex;
		int listIndexCounter;
		bool traversingList;

		enum errorTypeNdx {note, warning, error};

		int notesID; // used mainly for debugging note and staff objects

		bool fullScreen;
		bool isWindowResized;
		uint64_t windowResizeTimeStamp;
		
		ofColor backgroundColor;
		int brightness;
		float brightnessCoeff;

		ofTrueTypeFont font;
		ofTrueTypeFont instFont;

		ofxOscReceiver oscReceiver;
		bool oscReceiverIsSet;

		map<int, string> allStrings;
		map<int, int> allStringStartPos;
		map<int, vector<int>> allStringTabs;
		//vector<vector<int>> curlyIndexes;

		// map of function names and indexes
		// the map with int and Function objects is in SharedData
		map<string, int> functionIndexes;
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
		vector<string> tempLines;
		// temporary storage for separated bars, needed for \\bars command
		vector<string> allBars;
		// and a boolean to allow storing a pattern and to insert lines
		bool storePattern;
		bool inserting;
		// a string to hold the name of a \\bars command argument
		// until it is stored as a loop
		string barsName;

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

		// map that stores which editors can receive data from OSC
		map<int, bool> fromOsc;
		// and a map with the OSC address for each editor
		map<int, string> fromOscAddr;

		map<int, int> instNameWidths;
		// we need an index to point to the correct 2D vector
		int patternIndex;
		// the boolean below determines whether we're parsing a bar 
		bool parsingBar;
		// and one for parsing multiple bars
		bool parsingBars;
		// and one for parsing loops
		bool parsingLoop;
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
		string multiBarsName;
		// and a counter for the iterations of multiple bars parsing
		int barsIterCounter;
		// a variable to check if all instruments parsed the same number of bars
		int numBarsParsed;
		char noteChars[8];
		string lastInstrument;
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
