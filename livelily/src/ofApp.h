#pragma once

#include "ofMain.h"
#include <map>
#include <vector>
#include "instrument.h"

#define MINDUR 256
#define BEATVIZBRIGHTNESS 255

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
    ofMutex mutex;
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

class ofApp : public ofBaseApp{
	public:
		void setup();
		void update();
		void draw();

		bool endsWith(string a, string b);
		bool startsWith(string a, string b);
		bool isNumber(string str);
		int getPrevBarIndex();
		int getLastBarIndex();
		int getLastLoopIndex();
		void initializeInstrument(string instName);
		vector<string> tokenizeString(string str, string delimiter);
		std::pair<int, string> parseMelodicLine(string str);
		void setScoreCoords();
		
		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y);
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void mouseEntered(int x, int y);
		void mouseExited(int x, int y);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);

		int lastInstrumentIndex;
		char noteChars[8];
		ofTrueTypeFont instFont;
		map<int, int> instNameWidths;
		string lastInstrument;
		bool correctOnSameOctaveOnly;

		float brightnessCoeff;
		int lineWidth;
		
		SharedData sharedData;
};
