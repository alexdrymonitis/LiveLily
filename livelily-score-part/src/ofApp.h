#pragma once

#include "ofMain.h"
#include "ofxOsc.h"
#include <vector>
#include "scores.h"


#define OFRECVPORT 9000
#define OFSENDPORT 1234
#define HOST "localhost"
#define BACKGROUND 15

#define BEATVIZBRIGHTNESS 100 // transparency for showing the beat in the score
#define MINDB 60
#define TRACEBACKDUR 2000

#define OSCPORT 9050

#define WINDOW_RESIZE_GAP 50

// vectors and variables that need to be shared between main and threaded classes
typedef struct _SequencerVectors {
	vector<Staff> staffs;
	vector<Notes> notesVec;
	// 3D and 4D vectors: x: instrument, y: pattern index, z: pattern data, w: chord notes
	vector<vector<vector<vector<float>>>> notes;
	vector<vector<vector<int>>> durs;
	vector<vector<vector<int>>> dursWithoutSlurs;
	// vectors not affected by slurs and ties, needed for counting in the sequencer
	vector<vector<vector<int>>> dursUnchanged;
	vector<vector<vector<int>>> dynamics;
	vector<vector<vector<int>>> dynamicsRamps;
	vector<vector<vector<int>>> glissandi;
	vector<vector<vector<int>>> articulations;
	vector<vector<vector<string>>> text;
	vector<vector<vector<int>>> textIndexes;
	vector<vector<int>> patternData;
	vector<string> instruments;

	vector<int> distBetweenBeats;

	float notesXStartPnt;

	int noteWidth;
	int noteHeight;

	// variables to simulate locking the mutex
	bool stepDone;
	bool updatingSharedMem;

	// articulation strings so each program can treat them differently
	string articulSyms[8];
	// variable for counting beats for every instrument
	vector<int> beatCounters;
	// variable to increment over the notes and durs z axis of every instrument
	vector<int> barDataCounters;
	bool showScore;
	// animation variables
	bool animate;
	bool setAnimation;
	bool setBeatAnimation;
	bool beatViz;
	float beatVizStepsPerMs;
	uint64_t beatVizRampStart;
	uint64_t beatVizTimeStamp;
	int beatVizCounter;
	int beatVizDegrade;
	bool beatAnimate;
	bool beatTypeCommand;
	int beatVizType;
	// the variable below is used to iterate over one loop
	unsigned thisPatternLoopIndex;
	int patternLoopIndex;
	int tempPatternLoopIndex;
	vector<vector<int>> patternLoopIndexes;
	int currentScoreIndex;
	// a boolean if we call a pattern while the sequencer is running
	bool updatePatternLoop;
	// three different tempo placeholders
	// one for the running sequencer (tempo)
	// one for updating tempo while sequencer is running (newTempo)
	// and one to hold the tempo in ms without the conversion
	// to the global minimum duration
	int tempo;
	int newTempo;
	int tempoMs;
	// variables for positioning staffs and notes
	vector<float> maxStaffScorePos;
	vector<float> minStaffScorePos;
	// variables for positioning staffs for every pattern
	vector<float> patternFirstStaffAnchor;
	float maxPatternFirstStaffAnchor;
	float allStaffDiffs;

	vector<int> activeInstruments;
	float staffLinesDist;
	int longestInstNameWidth;
	// variable to leave some space between edge of score and
	// instrument name, instrument name and staff, etc.
	int blankSpace;

	// STDERR
	int tracebackYCoord;

	int nextScoreIndex;
	bool receivedNextScoreIndex;

	int numBeats;
	int tempNumBeats;

	vector<vector<float>> scoreYAnchors;

	int scoreFontSize;

	int numerator;
	int denominator;

	int screenWidth;
	int screenHeight;
	int middleOfScreenX;
	int middleOfScreenY;
	int staffXOffset;
	int staffWidth;
} SequencerVectors;


class PosCalculator : public ofThread {

	public:
		void setup(SequencerVectors *sVec);
		void setLoopIndex(int loopIdx);

		bool sequencerRunning;
		bool runSequencer;
		// vector to store the indexes of the bars when defined with the \bars command
		// this is used so we can calculate their positions in a loop with posCalculator
		vector<int> multipleBarsPatternIndexes;

	private:
		void calculateAllStaffPositions();
		void correctAllStaffPositions();
		void threadedFunction();

		SequencerVectors *seqVec;

		int loopIndex;
		int correctAllStaffLoopIndex;
};


class ofApp : public ofBaseApp{

	public:
		// basic OF program structure
		void setup();
		void update();
		void draw();
		// OF keyboard input functions
		void keyPressed(int key);
		void keyReleased(int key);
		// debugging
		void printVector(vector<int> v);
		void printVector(vector<string> v);
		void printVector(vector<float> v);
		// initialize an instrument
		void initializeInstrument(string instName);
		// the sequencer functions of the system
		int setPatternIndex(bool increment);
		unsigned setLoopIter(int ptrnIdx);
		int setInstIndex(int ptrnIdx, int i);
		int setBar(int ptrnIdx, int idx, int i);
		void updateTempoBeatsInsts(int ptrnIdx);
		// staff and notes handling
		void createStaff();
		void initStaffCoords(int loopIndex);
		void createNotes();
		void initNotesCoords(int ptrnIdx);
		void initScoreNotes(int beats);
		void setScoreNotes(int ptrnIdx);
		void setScoreNotes(int bar, int loopIndex, int inst, int beats);
		void setScoreSizes();
		void calculateAllStaffPositions(int index);
		void correctAllStaffPositions(int index);
		// release memory on exit
		void releaseMem();
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

		SequencerVectors seqVec;
		PosCalculator posCalculator;

		int notesID; // used mainly for debugging note and staff objects

		bool fullScreen;
		bool isWindowResized;
		uint64_t windowResizeTimeStamp;
		int backGround;

		ofTrueTypeFont font;
		ofTrueTypeFont instFont;

		ofxOscReceiver oscReceiver;

		// vectors and variables for defining functions
		vector<int> functionIndexes;
		vector<string> functionNames;
		vector<vector<string>> functionBodies;
		bool storingFunction;
		int lastFunctionIndex;

		int instFontSize; // the size of the font of the instrument display

		// global line width
		int lineWidth;

		int brightness;

		// notes variables
		vector<vector<vector<int>>> slurBeginnings;
		vector<vector<vector<int>>> slurEndings;
		// same vectors for sending data to the Notes objects
		vector<vector<vector<vector<int>>>> scoreNotes; // notes are ints here
		vector<vector<vector<int>>> scoreDurs;
		vector<vector<vector<int>>> scoreDotIndexes;
		vector<vector<vector<vector<int>>>> scoreAccidentals;
		vector<vector<vector<vector<int>>>> scoreOctaves;
		vector<vector<vector<int>>> scoreGlissandi;
		vector<vector<vector<int>>> scoreArticulations;
		vector<vector<vector<int>>> scoreDynamics;
		vector<vector<vector<int>>> scoreDynamicsIndexes;
		vector<vector<vector<int>>> scoreDynamicsRampStart;
		vector<vector<vector<int>>> scoreDynamicsRampEnd;
		vector<vector<vector<int>>> scoreDynamicsRampDir;
		vector<vector<vector<int>>> scoreSlurBeginnings;
		vector<vector<vector<int>>> scoreSlurEndings;
		vector<vector<vector<string>>> scoreTexts;
		// global count of bars
		int barIndexGlobal;
		// and a boolean used to determine whether we'll increment barIndexGlobal
		bool parsedMelody;
		vector<int> instNameWidths;
		// we need an index to point to the correct 2D vector
		int patternIndex;
		char noteChars[8];;
		unsigned numInstruments;
		int tempNumInstruments;
};
