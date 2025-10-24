#ifndef INSTRUMENT_H
#define INSTRUMENT_H

#include "ofMain.h"
#include "ofxOsc.h"
#include "score.h"
#include <vector>

#define MINDUR 256

class Instrument
{
	public:
		Instrument();
		void setName(std::string name);
		std::string getName();
		void setID(int id);
		int getID();
		void setGroup(int groupID);
		int getGroup();
		void setRhythm(bool isRhythm);
		bool isRhythm();
		void setSendToPython(bool b);
		bool sendToPython();
		void setDelay(int64_t dur);
		bool hasDelay();
		int64_t getDelayTime();
		void setSendMIDI(bool sendMIDI);
		bool sendMIDI();
		void setTransposition(int transpo);
		int getTransposition();
		void setDefaultDur(float dur);
		void setStaccatoDur(float dur);
		void setStaccatissimoDur(float dur);
		void setTenutoDur(float dur);
		int setDuration(std::string subcommand, float dur);
		void setNumBarsToDisplay(int numBars);
		float getXCoef();
		void setPassed(bool passed);
		bool hasPassed();
		void setCopied(int barIndex, bool copyState);
		bool getCopied(int barIndex);
		void setCopyNdx(int barIndex, int barToCopy);
		int getCopyNdx(int barIndex);
		bool hasNewStep();
		std::pair<int, int> isLinked(int bar);
		void setNewStep(bool stepState);
		int getNewStep();
		bool getMultiBarsDone();
		void setMultiBarsDone(bool done);
		size_t getMultiBarsStrBegin();
		void setMultiBarsStrBegin(size_t pos);
		void setClef(int bar, int clefIdx);
		int getClef(int bar);
		void createEmptyMelody(int barIndex);
		void deleteNotesBar(int bar);
		void setMeter(int bar, int numerator, int denominator, int numBeats);
		std::pair<int, int> getMeter(int bar);
		void setScoreNotes(int bar, int numerator, int denominator, int numBeats,
				int BPMValue, int beatAtValue, bool hasDot, int BPMMultiplier);
		void setNoteCoords(float xLen, float staffLineDist, int fontSize);
		void setAccidentalsOffsetCoef(float coef);
		void setNotePositions(int bar);
		void setNotePositions(int bar, int numBars);
		void setScoreOrientation(int orientation);
		void setStaffColor(ofColor color);
		void setStaffColor(int color, int rgbVal);
		void setStaffColor(int r, int g, int b);
		ofColor getStaffColor();
		void setNotesColor(ofColor color);
		void setNotesColor(int color, int rgbVal);
		void setNotesColor(int r, int g, int b);
		ofColor getNotesColor();
		void setActiveNotesColor(ofColor color);
		void setActiveNotesColor(int color, int rgbVal);
		void setActiveNotesColor(int r, int g, int b);
		ofColor getActiveNotesColor();
		void setGlissandoStart(bool startGliss);
		bool getGlissandoStart();
		void invertColor();
		void moveScoreX(int numPixels);
		void moveScoreY(int numPixels);
		void recenterScore();
		float getStaffXLength();
		float getNoteWidth();
		float getNoteHeight();
		float getStaffYAnchor();
		void setStaffCoords(float xStartPnt, float staffLineDist);
		void setNotesFontSize(int fontSize, float staffLinesDist);
		void setAnimation(bool animationState);
		void setLoopStart(bool loopStartState);
		void setLoopEnd(bool loopEndState);
		void setScoreEnd(bool scoreEndState);
		bool isLoopStart();
		bool isLoopEnd();
		void initSeqToggle();
		void setFirstIter(bool iter);
		void setToMute(bool mute);
		bool isToBeMuted();
		void setToUnmute(bool unmute);
		bool isToBeUnmuted();
		void setMute(bool mute);
		bool isMuted();
		void setMidi(bool isMidiBool);
		bool isMidi();
		void setTimeStamp(uint64_t stamp);
		void resetNoteDur();
		void zeroNoteDur();
		bool mustFireStep(uint64_t stamp, int bar, float tempo);
		void setNoteDur(int bar, float tempo);
		bool isNoteSlurred(int bar, int dataCounter);
		bool isNoteTied(int bar, int dataCounter);
		bool isLastNoteTied(int bar);
		bool mustUpdateTempo();
		void setUpdateTempo(bool tempoUpdate);
		bool hasNotesInBar(int bar);
		bool hasNotesInStep(int bar);
		void setMidiPort(int port);
		int getMidiPort();
		void setMidiChan(int chan);
		int getMidiChan();
		void setActiveNote();
		void incrementBarDataCounter(int bar);
		int getBarDataCounter();
		void resetBarDataCounter();
		void toggleSeqToggle(int bar);
		int getSeqToggle();
		bool hasText(int bar);
		std::string getText(int bar);
		int getPitchBendVal(int bar);
		int getProgramChangeVal(int bar);
		int getMidiVel(int bar);
		int getArticulationIndex(int bar);
		float getDynamic(int bar);
		float getMinVsAnchor(int bar);
		float getMaxYPos(int bar);
		float getMinYPos(int bar);
		float getClefXOffset();
		float getMeterXOffset();
		void drawStaff(int bar, float xOffset, float yStartPnt, float yOffset, bool drawClef,
				bool drawMeter, bool drawLoopStartEnd, bool drawTempo);
		void drawNotes(int bar, int loopNdx, std::vector<int> *v, float xOffset, float yStartPnt,
				float yOffset, bool animate, float xCoef);
		void printVector(std::vector<int> v);
		void printVector(std::vector<std::string> v);
		void printVector(std::vector<float> v);

		std::string name;
		std::map<int, float> distBetweenBeats;
		// std::maps of int and std::map of int and std::vector (of std::vectors)
		// int key of outter std::map is instrument index, retrieved from instrumentIndexes above
		// int key of inner std::map is bar index, retrieved from barsIndexes above
		// std::vector if bar data
		// if std::vector of std::vectors, it is chord notes
		std::map<int, std::vector<std::vector<float>>> notes;
		std::map<int, std::vector<std::vector<int>>> midiNotes;
		std::map<int, std::vector<int>> durs;
		std::map<int, std::vector<int>> dursWithoutSlurs;
		std::map<int, std::vector<int>> midiDursWithoutSlurs;
		std::map<int, std::vector<int>> pitchBendVals;
		// std::vectors not affected by slurs and ties, needed for counting in the sequencer
		std::map<int, std::vector<int>> dursUnchanged;
		std::map<int, std::vector<float>> dynamics;
		std::map<int, std::vector<int>> midiVels;
		std::map<int, std::vector<float>> dynamicsRamps;
		std::map<int, std::vector<int>> midiDynamicsRampDurs;
		std::map<int, std::vector<int>> glissandi;
		std::map<int, std::vector<uint64_t>> glissDursMs;
		std::map<int, std::vector<int>> midiGlissDurs;
		std::map<int, std::vector<std::vector<int>>> articulations;
		std::map<int, std::vector<std::vector<int>>> midiArticulationVals;
		std::map<int, std::vector<bool>> isSlurred;
		std::map<int, std::vector<std::string>> text;
		std::map<int, std::vector<std::vector<int>>> textIndexes;
		std::map<int, std::vector<std::pair<int, int>>> slurIndexes;
		std::map<int, std::vector<int>> tieIndexes;
		// same std::vectors for sending data to the Notes objects
		std::map<int, std::vector<std::vector<int>>> scoreNotes; // notes are ints here
		std::map<int, std::vector<int>> scoreDurs;
		std::map<int, std::vector<int>> scoreDotIndexes;
		std::map<int, std::vector<unsigned>> scoreDotsCounter;
		std::map<int, std::vector<std::vector<int>>> scoreAccidentals;
		std::map<int, std::vector<std::vector<int>>> scoreNaturalSignsNotWritten;
		std::map<int, std::vector<std::vector<int>>> scoreOctaves;
		std::map<int, std::vector<int>> scoreOttavas;
		std::map<int, std::vector<int>> scoreGlissandi;
		std::map<int, std::vector<std::vector<int>>> scoreArticulations;
		std::map<int, std::vector<int>> scoreDynamics;
		std::map<int, std::vector<int>> scoreDynamicsIndexes;
		std::map<int, std::vector<int>> scoreDynamicsRampStart;
		std::map<int, std::vector<int>> scoreDynamicsRampEnd;
		std::map<int, std::vector<int>> scoreDynamicsRampDir;
		//std::map<int, std::vector<int>> scoreSlurBeginnings;
		//std::map<int, std::vector<int>> scoreSlurEndings;
		std::map<int, bool> isWholeBarSlurred;
		std::map<int, std::map<int, std::pair<int, int>>> scoreTupletRatios;
		std::map<int, std::map<int, std::pair<unsigned, unsigned>>> scoreTupletStartStop;
		std::map<int, std::vector<std::string>> scoreTexts;

		// variables for handling glissandi in the sequencer
		std::vector<float> glissStart;
		std::vector<float> glissEnd;
		std::vector<float> glissSteps;
		std::vector<int> glissNumSteps;
		int64_t glissTimeStamp;
		int64_t glissStartTimeStamp;
		int glissCounter;
		int64_t glissDurMs;
		int64_t glissTimeDiff;
		bool startGliss;

		// score parts OSC handling
		bool sendToPart;
		ofxOscSender scorePartSender;
		// map to hold a boolean determining that a part that receives data has received bar data correctly
		std::map<int, bool> barDataOKFromParts; // keys are bar indexes
		// map of instrument index and string to store expanded bar lines to send to parts
		std::map<int, std::string> barLines;

		// the std::vector below is used in case a connected instrument needs a delay for incoming messages
		// so we store a pair with the ofxOscMessage object and a time stamp
		std::vector<std::pair<ofxOscMessage, uint64_t>> oscFifo;
		// the boolean below stores the state of the delay
		bool delayState;
		int64_t delayTime;

		// MIDI stuff
		int midiPort;
		int midiChan;
		bool isMidiBool;
		// duration percentage for various notes
		// size is 8 because of the number of available articulation symbols
		float durPercentages[8];
		float staccatoDur;
		float staccatissimoDur;
		float tenutoDur;

		uint64_t beatCounter;
		int barCounter;
		int barDataCounter;
		bool barDataCounterReset;
		int seqToggle; // so we can alternate between notes on and notes off
		int64_t noteDur;
		uint64_t timeStamp;

		bool toMute;
		bool toUnmute;
		bool muteState;
		bool toMuteState;
		bool toUnmuteState;

		bool passed; // set to true when a melodic line passes all tests
		bool firstIter;

	private:
		Staff staff;
		Notes notesObj;
		int objID;
		int groupID;
		bool multiBarsDone;
		size_t multiBarsStrBegin;
		bool hasNewStepBool;
		int newStep;
		bool updateTempo;
		bool isRhythmBool;
		bool sendMIDIBool;
		bool sendToPythonBool;
		int transposition;

		std::map<int, bool> copyStates;
		std::map<int, int> copyNdxs;
};

#endif
