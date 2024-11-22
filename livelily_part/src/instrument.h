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
		int getID();
		void setGroup(int groupID);
		int getGroup();
		void setRhythm(bool isRhythm);
		void setNumBarsToDisplay(int numBars);
		float getXCoef();
		void setClef(int bar, int clefIdx);
		int getClef(int bar);
		void setCopied(int barIndex, bool copyState);
		bool getCopied(int barIndex);
		void setCopyNdx(int barIndex, int barToCopy);
		int getCopyNdx(int barIndex);
		vector<vector<int>> packIntVector(vector<int> v, int val);
		void setNotes(int bar, vector<int> v);
		void setAccidentals(int bar, vector<int> v);
		void setNaturalSignsNotWritten(int bar, vector<int> v);
		void setOctaves(int bar, vector<int> v);
		void setOttavas(int bar, vector<int> v);
		void setDurs(int bar, vector<int> v);
		void setDotIndexes(int bar, vector<int> v);
		void setGlissandi(int bar, vector<int> v);
		void setArticulations(int bar, vector<int> v);
		void setDynamics(int bar, vector<int> v);
		void setDynamicsIndexes(int bar, vector<int> v);
		void setDynamicsRampStart(int bar, vector<int> v);
		void setDynamicsRampEnd(int bar, vector<int> v);
		void setDynamicsRampDir(int bar, vector<int> v);
		void setSlurIndexes(int bar, vector<int> v);
		void setWholeBarSlurred(int bar, bool b);
		void setTies(int bar, vector<int> v);
		void setTupRatios(int bar, vector<int> v);
		void setTupStartStop(int bar, vector<int> v);
		void setTexts(int bar, vector<string> v);
		void setTextIndexes(int bar, vector<int> v);
		bool checkVecSizesForEquality(int bar);
		void copyMelodicLine(int barIndex);
		void createEmptyMelody(int barIndex);
		void setMeter(int bar, int numerator, int denominator, int numBeats);
		std::pair<int, int> getMeter(int bar);
		void setScoreNotes(int bar, int denominator, int numerator, int numBeats,
				int BPMTempo, int beatAtValue, bool hasDot, int BPMMultiplier);
		void setNotePositions(int bar);
		void setNoteCoords(float xLen, float staffLineDist, int fontSize);
		void setAccidentalsOffsetCoef(float coef);
		void moveScoreX(int numPixels);
		void moveScoreY(int numPixels);
		void recenterScore();
		float getStaffXLength();
		float getNoteWidth();
		float getNoteHeight();
		void setStaffCoords(float xStartPnt, float staffLineDist);
		void setNotesFontSize(int fontSize, float staffLinesDist);
		void setAnimation(bool animationState);
		void setLoopStart(bool loopStartState);
		void setLoopEnd(bool loopEndState);
		void setScoreEnd(bool scoreEndState);
		bool isLoopStart();
		bool isLoopEnd();
		void setActiveNote(int note);
		float getMinVsAnchor(int bar);
		float getMaxYPos(int bar);
		float getMinYPos(int bar);
		float getClefXOffset();
		float getMeterXOffset();
		void drawStaff(int bar, float xOffset, float yStartPnt, float yOffset, bool drawClef, bool drawMeter, bool drawLoopStartEnd, bool drawTempo);
		void drawNotes(int bar, int loopNdx, vector<int> *v, float xOffset, float yStartPnt, float yOffset, bool animate, float xCoef);
		void printVector(vector<int> v);
		void printVector(vector<string> v);
		void printVector(vector<float> v);

	private:
		Staff staff;
		Notes notesObj;
		int objID;
		int groupID;
		string name;
		// maps of int and map of int and vector (of vectors)
		// int key of outter map is instrument index, retrieved from instrumentIndexes above
		// int key of inner map is bar index, retrieved from barsIndexes above
		// vector of bar data
		// if vector of vectors, it is chord notes
		map<int, vector<vector<int>>> scoreNotes;
		map<int, vector<vector<int>>> scoreAccidentals;
		map<int, vector<vector<int>>> scoreNaturalSignsNotWritten;
		map<int, vector<vector<int>>> scoreOctaves;
		map<int, vector<int>> scoreOttavas;
		map<int, vector<int>> scoreNotesUnpacked;
		map<int, vector<int>> scoreAccidentalsUnpacked;
		map<int, vector<int>> scoreOctavesUnpacked;
		map<int, vector<int>> scoreDurs;
		map<int, vector<int>> scoreDotIndexes;
		map<int, vector<int>> scoreGlissandi;
		map<int, vector<vector<int>>> scoreArticulations;
		map<int, vector<int>> scoreDynamics;
		map<int, vector<int>> scoreDynamicsIndexes;
		map<int, vector<int>> scoreDynamicsRampStart;
		map<int, vector<int>> scoreDynamicsRampEnd;
		map<int, vector<int>> scoreDynamicsRampDir;
		//map<int, vector<int>> scoreSlurBeginnings;
		//map<int, vector<int>> scoreSlurEndings;
		map<int, vector<std::pair<int, int>>> scoreSlurIndexes;
		map<int, bool> isWholeBarSlurred;
		map<int, vector<int>> tieIndexes;
		map<int, map<int, std::pair<int, int>>> scoreTupRatios;
		map<int, map<int, std::pair<unsigned, unsigned>>> scoreTupStartStop;
		map<int, vector<string>> scoreTexts;
		map<int, vector<int>> scoreTextIndexes;

		map<int, bool> copyStates;
		map<int, int> copyNdxs;

		map<int, bool> isClefSet;
		int clef;
};

#endif
