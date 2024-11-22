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
		void storeNewBar(int barIndex);
		int getPlayingBarIndex();
		// initialize an instrument
		void initializeInstrument(int index, string instName, string hostIP, int hostPort);
		void createFirstBar(int instNdx);
		// the sequencer functions of the system
		void updateTempoBeatsInsts(int barIndex);
		// staff and notes handling
		void setScoreCoords();
		void setScoreNotes(int barIndex);
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

		ofxOscReceiver oscReceiver;
		ofxOscSender oscSender; // to notify the main program if an error occurred while transferring bar data
		bool oscSenderSetup;

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

		map<int, int> instNameWidths;
		char noteChars[8];
		string lastInstrument;
		int lastInstrumentIndex;
		int tempNumInstruments;

		// variables for receiving bar data over OSC
		int thisBarIndex;
		int instNdx;
		int lastBarIndex;
		// the keys of the map below are the bar indexes
		// and the values hold the count of instruments that receive bar data without errors
		map<int, int> instrumentCounterPerBar;
		string barName;
		int errorCounter;
		// variable to notify main program that an error occurred while transferring bar data
		bool barDataError;
		// and an int  that will hold the number of incoming data so we can check if everything arrived as it should
		int numBarData;
		// the keys of the map below are the instrument indexes and the values are
		// vectors with the bar indexes where errors occured
		map<int, vector<int>> barDataErrorsMap;
		// a map of int and pair of a bool and int, to notify the main program that all instruments in this server
		// have received the data of a bar without errors
		// keys are instrument indexes and values are the pair
		map<int, std::pair<bool, int>> sendBarDataOKMap;

		// the variables below are from the SharedData structure of the main program
		// a map of Instruments()
		map<int, Instrument> instruments;
		// maps of string and int to easily store data based on strings
		// but iterate over the data in the sequencer and score renderer fast
		// based on ints
		map<string, int> instrumentIndexes;
		map<string, int> barsIndexes;
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
		bool beatAnimate;
		bool beatTypeCommand;
		int beatVizType;
		// the variable below is used by the serquencer to iterate over one loop
		unsigned thisLoopIndex;
		int loopIndex;
		int oscLoopIndex; // received from the main program
		int tempBarLoopIndex;
		map<int, vector<int>> loopData;
		// a boolean if we call a pattern while the sequencer is running
		bool updateLoop;
		// three different tempo placeholders
		// one for the running sequencer (tempo)
		// one for updating tempo while sequencer is running (newTempo)
		// and one to hold the tempo in ms without the conversion
		// to the global minimum duration
		float tempo;
		float newTempo;
		int tempoMs;
		map<int, int> BPMTempi;
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
		map<int, int> BPMMultiplier;

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
