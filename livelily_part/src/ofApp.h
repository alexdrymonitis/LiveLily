#ifndef OF_APP_H
#define OF_APP_H

#include "ofMain.h"
#include "ofxOsc.h"
#include <map>
#include <vector>
#include "instrument.h"

#define OFRECVPORT 8001
#define OFSENDPORT 1234 // for sending sequencer data to other software
#define SCOREPARTPORT 9000
#define HOST "localhost"
#define BACKGROUND 15

// 64ths are the minimum note duration
// so we use 4 steps per this duration as a default
#define MINDUR 256
#define BEATVIZBRIGHTNESS 255 // transparency for showing the beat in the score
#define MINDB 60

#define OSCPORT 9000 // for receiving OSC messages from main program 

#define WINDOW_RESIZE_GAP 50
#define NOTESXOFFSETCOEF 3

class ofApp : public ofBaseApp
{
	public:
		// basic OF program structure
		void setup();
		void update();
		void draw();
		int getMappedIndex(int index);
		void keyPressed(int key);
		void keyReleased(int key);
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
		void parseStrings();
		void parseBar();
		void parseLoop();
		void parseString(string str);
		void fillInMissingInsts(int barIndex);
		int storeNewBar(string barName);
		void storeNewLoop(string loopName);
		int getBaseDurValue(string str, int denominator);
		void parseCommand(string str);
		bool isInstrument(vector<string>& commands);
		bool isBarLoop(vector<string>& commands);
		int getLastLoopIndex();
		int getLastBarIndex();
		int getPrevBarIndex();
		int getPlayingBarIndex();
		void stripLineFromBar(string str);
		void parseBarLoop(string str);
		vector<string> tokenizeChord(string input, bool includeAngleBrackets = false);
		std::pair<int, string> parseMelodicLine(string str);
		// initialize an instrument
		void initializeInstrument(int index, string instName);
		void createFirstBar(int instNdx);
		// the sequencer functions of the system
		void updateTempoBeatsInsts(int barIndex);
		// staff and notes handling
		void setScoreCoords();
		void setScoreNotes(int barIndex);
		void setNotePositions(int barIndex);
		void setNotePositions(int barIndex, int numBars);
		void calculateStaffPositions(int bar, bool windowChanged);
		void setScoreSizes();
		// rest of OF functions
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void mouseEntered(int x, int y);
		void mouseExited(int x, int y);
		void windowResized(int w, int h);
		void gotMessage(ofMessage msg);
		void dragEvent(ofDragInfo dragInfo);

		// from the SharedData struct
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

		// this vector will hold the lines to parse, sent from the main program
		vector<string> linesToParse;

		// booleans to determine what we are doing
		bool parsingBar;
		bool parsingLoop;
		bool parsingBars;

		bool correctOnSameOctaveOnly;

		string multiBarsName;
		int barsIterCounter;
		int barToCopy;
		int numBarsParsed;
		int firstInstForBarsIndex;
		bool firstInstForBarsSet;

		ofxOscReceiver oscReceiver;

		int notesID; // used mainly for debugging note and staff objects

		bool fullscreen;
		bool finishState;
		bool changeBeatColorOnUpdate;
		
		ofColor backgroundColor;
		int brightness;
		float brightnessCoeff;
		float beatBrightnessCoeff;

		ofTrueTypeFont instFont;
		ofTrueTypeFont countdownFont;

		int instFontSize; // the size of the font of the instrument display

		// global line width
		int lineWidth;

		float scoreXOffset;
		float scoreYOffset;
		bool mustUpdateScore;
		bool scoreUpdated;
		bool showUpdatePulseColor;
		bool scoreChangeOnLastBar;
		int scoreOrientation;
		// variables to determine whether we call a command for moving the score
		bool scoreMoveXCommand;
		bool scoreMoveYCommand;
		bool drawLoopStartEnd;

		map<int, int> instNameWidths;
		char noteChars[8];
		string lastInstrument;
		int lastInstrumentIndex;
		int tempNumInstruments;

		// for determing the position of single bars in horizontal view
		int prevSingleBarPos;

		// variables for receiving bar data over OSC
		int thisBarIndex;
		int instNdx;
		int lastBarIndex;

		map<int, int> instrumentIndexMap;

		int numInstruments;
		int numBarsToDisplay;

		bool displayConnectionError;

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
		int beatVizCounter;
		int beatVizDegrade;
		int beatVizCosTab[BEATVIZBRIGHTNESS+1];
		bool beatAnimate;
		bool beatTypeCommand;
		int beatVizType;
		// the variable below is used by the serquencer to iterate over one loop
		unsigned thisLoopIndex;
		int loopIndex;
		bool seqState; // to determine if we should update the loop index when creating bars and loops
		int oscLoopIndex; // received from the main program
		int tempBarLoopIndex;
		map<int, vector<int>> loopData;
		// a boolean if we call a pattern while the sequencer is running
		bool updateLoop;
		
		float newTempo;
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

		map<int, int> numBeats;
		int tempNumBeats;

		int scoreFontSize;

		map<int, int> numerator;
		map<int, int> denominator;

		int screenWidth;
		int screenHeight;
		int middleOfScreenX;
		int middleOfScreenY;
		float scoreXStartPnt;
		float notesLength;
		int staffXOffset;
		int staffWidth;
		float yStartPnt;

		bool showCountdown;
		int countdown;
		bool showTempo;
};

#endif
