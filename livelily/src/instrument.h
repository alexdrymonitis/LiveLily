#ifndef INSTRUMENT_H
#define INSTRUMENT_H

#include "ofMain.h"
#include "ofxOsc.h"
#include "score.h"
#include <vector>

#define MINDUR 256
#define MINDB 60

class Instrument
{
	public:
		Instrument();
		void setName(string name);
		string getName();
		void setID(int id);
		void setRhythm(bool isRhythm);
		bool isRhythm();
		void setTransposition(int transpo);
		int getTransposition();
		void setDefaultDur(float dur);
		void setStaccatoDur(float dur);
		void setStaccatissimoDur(float dur);
		void setTenutoDur(float dur);
		void setNumBarsToDisplay(int numBars);
		void setPassed(bool passed);
		bool hasPassed();
		void setCopied(int barIndex, bool copyState);
		bool getCopied(int barIndex);
		void setCopyNdx(int barIndex, int barToCopy);
		int getCopyNdx(int barIndex);
		bool hasNewStep();
		void setNewStep(bool stepState);
		int getNewStep();
		bool getMultiBarsDone();
		void setMultiBarsDone(bool done);
		size_t getMultiBarsStrBegin();
		void setMultiBarsStrBegin(size_t pos);
		void setClef(int clefIdx);
		void copyMelodicLine(int barIndex);
		void createEmptyMelody(int barIndex);
		void setMeter(int bar, int numerator, int denominator, int numBeats);
		void setScoreNotes(int bar, int numerator, int denominator, int numBeats);
		void setNoteCoords(float xLen, float yPos1, float yPos2, float staffLineDist, int fontSize);
		void setNotePositions(int bar);
		void correctScoreYAnchor(float yAnchor1, float yAnchor2);
		void setScoreOrientation(int orientation);
		void moveScoreX(int numPixels);
		void moveScoreY(int numPixels);
		void recenterScore();
		float getStaffXLength();
		float getNoteWidth();
		float getNoteHeight();
		float getStaffYAnchor();
		void setStaffSize(int fontSize);
		void setStaffCoords(float xStartPnt, float yAnchor1, float yAnchor2, float staffLineDist);
		void setNotesFontSize(int fontSize);
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
		bool mustUpdateTempo();
		void setUpdateTempo(bool tempoUpdate);
		bool hasNotesInBar(int bar);
		bool hasNotesInStep(int bar);
		void setMidiChan(int chan);
		int getMidiChan();
		void setActiveNote();
		void incrementBarDataCounter(int bar);
		int getBarDataCounter();
		void resetBarDataCounter();
		void toggleSeqToggle(int bar);
		int getSeqToggle();
		bool hasText(int bar);
		string getText(int bar);
		int getPitchBendVal(int bar);
		int getProgramChangeVal(int bar);
		int getMidiVel(int bar);
		int getArticulationIndex(int bar);
		int getDynamic(int bar);
		float getMinVsAnchor(int bar);
		float getMaxYPos(int bar);
		float getMinYPos(int bar);
		float getClefXOffset();
		float getMeterXOffset();
		void drawStaff(int bar, float xOffset, float yOffset, bool drawClef, bool drawMeter, bool drawLoopStartEnd);
		void drawNotes(int bar, int loopNdx, vector<int> *v, float xOffset, float yOffset, bool animate, float xCoef);
		void printVector(vector<int> v);
		void printVector(vector<string> v);
		void printVector(vector<float> v);

		string name;
		map<int, float> distBetweenBeats;
		// maps of int and map of int and vector (of vectors)
		// int key of outter map is instrument index, retrieved from instrumentIndexes above
		// int key of inner map is bar index, retrieved from barsIndexes above
		// vector if bar data
		// if vector of vectors, it is chord notes
		map<int, vector<vector<float>>> notes;
		map<int, vector<vector<int>>> midiNotes;
		map<int, vector<int>> durs;
		map<int, vector<int>> dursWithoutSlurs;
		map<int, vector<int>> midiDursWithoutSlurs;
		map<int, vector<int>> pitchBendVals;
		// vectors not affected by slurs and ties, needed for counting in the sequencer
		map<int, vector<int>> dursUnchanged;
		map<int, vector<int>> dynamics;
		map<int, vector<int>> midiVels;
		map<int, vector<int>> dynamicsRamps;
		map<int, vector<int>> midiDynamicsRampDurs;
		map<int, vector<int>> glissandi;
		map<int, vector<int>> midiGlissDurs;
		map<int, vector<int>> articulations;
		map<int, vector<int>> midiArticulationVals;
		map<int, vector<bool>> isSlurred;
		map<int, vector<string>> text;
		map<int, vector<int>> textIndexes;
		//map<int, vector<int>> slurBeginnings;
		//map<int, vector<int>> slurEndings;
		map<int, vector<std::pair<int, int>>> slurIndexes;
		// same vectors for sending data to the Notes objects
		map<int, vector<vector<int>>> scoreNotes; // notes are ints here
		map<int, vector<int>> scoreDurs;
		map<int, vector<int>> scoreDotIndexes;
		map<int, vector<vector<int>>> scoreAccidentals;
		map<int, vector<vector<int>>> scoreNaturalSignsNotWritten;
		map<int, vector<vector<int>>> scoreOctaves;
		map<int, vector<int>> scoreOttavas;
		map<int, vector<int>> scoreGlissandi;
		map<int, vector<int>> scoreArticulations;
		map<int, vector<int>> scoreDynamics;
		map<int, vector<int>> scoreDynamicsIndexes;
		map<int, vector<int>> scoreDynamicsRampStart;
		map<int, vector<int>> scoreDynamicsRampEnd;
		map<int, vector<int>> scoreDynamicsRampDir;
		//map<int, vector<int>> scoreSlurBeginnings;
		//map<int, vector<int>> scoreSlurEndings;
		map<int, bool> isWholeBarSlurred;
		map<int, map<int, std::pair<int, int>>> scoreTupRatios;
		map<int, map<int, std::pair<unsigned, unsigned>>> scoreTupStartStop;
		map<int, vector<string>> scoreTexts;

		// score parts OSC handling
		bool sendToPart;
		ofxOscSender scorePartSender;
		// map to hold a boolean determining that a part that receives data has received bar data correctly
		map<int, bool> barDataOKFromParts; // keys are bar indexes
	
		// MIDI stuff
		int midiChan;
		bool isMidiBool;
		// duration percentage for various notes
		// size is 8 because of the number of available articulation symbols
		float durPercentages[8];
		float staccatoDur;
		float staccatissimoDur;
		float tenutoDur;

		uint64_t beatCounter;
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
		bool multiBarsDone;
		size_t multiBarsStrBegin;
		bool hasNewStepBool;
		int newStep;
		bool updateTempo;
		bool isRhythmBool;
		int transposition;

		map<int, bool> copyStates;
		map<int, int> copyNdxs;
};

#endif
