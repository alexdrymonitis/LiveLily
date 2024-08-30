#ifndef SCORE_H
#define SCORE_H

#include "ofMain.h"
#include <vector>

#define EXTRALINECOEFF 0.7
#define BEAMDISTCOEFF 0.75

class Staff
{
	public:
		Staff();
		void setClef(int clefIdx);
		void setRhythm(bool isRhythm);
		void setSize(int fontSize);
		void setMeter(int bar, int numer, int denom);
		void setOrientation(int orientation);
		void setNumBarsToDisplay(int numBars);
		void setCoords(float xLen, float y1, float y2, float dist);
		void correctYAnchor(float y1, float y2);
		float getXLength();
		float getYAnchor();
		void moveScoreX(float numPixels);
		void moveScoreY(float numPixels);
		void recenterScore();
		float getClefXOffset();
		float getMeterXOffset();
		void drawStaff(int loopIndex, float xStartPnt, float yOffset, bool drawClef, bool drawMeter, bool drawLoopStartEnd);
		void drawLoopStart(int loopIndex, float xStart, float yOffset);
		void drawLoopEnd(int loopIndex, float xEnd, float yOffset);
		void drawScoreEnd(int loopIndex, float xEnd, float yOffset);
		void drawThickLine(float x1, float y1, float x2, float y2, float width);
		bool isLoopStart;
		bool isLoopEnd;
		bool isScoreEnd;
		~Staff();
	private:
		int objID;
		float xLength;
		float xCoef; // multiplied by X coordinates, based on how many bars are displayed in horizontal view
		float xOffset;
		float yAnchor[2];
		float yAnchorRecenter[2];
		float yDist;
		float clefLength;
		float meterLength;
		int scoreOrientation;
		map<int, int> numerator;
		map<int, int> denominator;
		int meterIndex;
		int lineWidth;
		string clefSyms[3];
		string meterSyms[2];
		int clefIndex;
		int fontSize;
		bool rhythm; // for rhythmic instruments
		ofTrueTypeFont notationFont;
};


class Notes
{
	public:
		Notes();
		void setID(int id);
		float getDist(int bar);
		void setRhythm(bool isRhythm);
		void setFontSize(int fontSize);
		void setNumBarsToDisplay(int numBars);
		void setXCoef(float coef);
		void setCoords(float xLen, float yStart1, float yStart2, float staffLinesDist, int fontSize);
		void setLength(int bar, int numBeats);
		void setNumBeats(int bar, int numBeats);
		float getMaxYPos(int bar);
		float getMinYPos(int bar);
		float getMinVsAnchor(int bar);
		float getNoteWidth();
		float getNoteHeight();
		void setClef(int clefIdx);
		void setOrientation(int orientation);
		void setAnimation(bool anim);
		void setActiveNote(int note);
		void setMute(bool muteState);
		void setMeter(int bar, int numer, int denom, int numBeats);
		void setNotes(int bar,
				vector<vector<int>> notes,
				vector<vector<int>> accidentals,
				vector<vector<int>> naturalsNotWritten,
				vector<vector<int>> octaves,
				vector<int> ottavas,
				vector<int> durs,
				vector<int> dots,
				vector<int> gliss,
				vector<int>articul,
				vector<int> dyns,
				vector<int> dynIdx,
				vector<int> dynRampStart,
				vector<int> dynRampEnd,
				vector<int> dynRampDir,
				vector<int> slurBegin,
				vector<int> slurEnd,
				vector<string> texts,
				vector<int> textIndexes);
		void changeNotesBasedOnClef(int bar);
		void correctDynamics(int bar, vector<int> dyns);
		void setNotePositions(int bar);
		void correctYAnchor(float yAnchor1, float yAnchor2);
		void moveScoreX(float numPixels);
		void moveScoreY(float numPixels);
		void recenterScore();
		float getXLength();
		void storeArticulationsCoords(int bar);
		void storeSlurCoords(int bar);
		void storeOttavaCoords(int bar);
		void storeTextCoords(int bar);
		void storeDynamicsCoords(int bar);
		void storeMinMaxY(int bar);
		void copyMelodicLine(int barIndex, int barToCopy);
		void insertNaturalSigns(int bar, int loopNdx, vector<int> *v);
		void drawNotes(int bar, int loopNdx, vector<int> *v, float xStartPnt, float yOffset, bool animation, float xCoef);
		void drawBeams(float x1, float y1, float x2, float y2);
		int drawRest(int bar, int restDur, float x, int color, float yOffset);
		void drawAccidentals(int bar, float xStartPnt, float yOffset, float xCoef);
		void drawGlissandi(int bar, float xStartPnt, float yOffset, float xCoef);
		void drawArticulations(int bar, float xStartPnt, float yOffset, float xCoef);
		void drawOttavas(int bar, float xStartPnt, float yOffset, float xCoef);
		void drawText(int bar, float xStartPnt, float yOffset, float xCoef);
		void drawSlurs(int bar, float xStartPnt, float yOffset, float xCoef);
		void drawDynamics(int bar, float xStartPnt, float yOffset, float xCoef);
		void printVector(vector<int> v);
		void printVector(vector<string> v);
		void printVector(vector<float> v);
		~Notes();
		bool mute;
	private:
		int objID;
		bool rhythm;
		map<int, vector<vector<int>>> allNotes;
		map<int, vector<int>> actualDurs;
		map<int, vector<int>> hasStem;
		// base and edge indexes of Y coords for chords
		map<int, vector<int>> whichIdxBase;
		map<int, vector<int>> whichIdxEdge;
		map<int, vector<vector<int>>> allAccidentals;
		map<int, vector<vector<int>>> naturalSignsNotWritten;
		map<int, vector<vector<int>>> allOctaves;
		map<int, vector<int>> allOttavas;
		map<int, vector<int>> allGlissandi;
		map<int, vector<int>> allArticulations;
		map<int, vector<int>> slurBeginnings;
		map<int, vector<int>> slurEndings;
		map<int, vector<float>> slurStartX;
		map<int, vector<float>> slurStopX;
		map<int, vector<float>> slurStartY;
		map<int, vector<float>> slurStopY;
		map<int, vector<float>> slurMiddleX1;
		map<int, vector<float>> slurMiddleY1;
		map<int, vector<float>> slurMiddleX2;
		map<int, vector<float>> slurMiddleY2;
		map<int, vector<int>> stemDirections;
		map<int, vector<int>> grouppedStemDirs;
		map<int, vector<int>> grouppedStemDirsCoeff;
		map<int, vector<int>> durations;
		map<int, vector<int>> dotIndexes;
		map<int, vector<int>> dynamics;
		map<int, vector<int>> dynamicsIndexes;
		map<int, vector<int>> dynamicsRampStart;
		map<int, vector<int>> dynamicsRampEnd;
		map<int, vector<vector<int>>> numExtraLines;
		map<int, vector<vector<int>>> extraLinesYPos;
		map<int, vector<vector<int>>> extraLinesDir;
		map<int, vector<vector<int>>> hasExtraLines;
		map<int, vector<int>> numNotesWithBeams;
		map<int, vector<vector<int>>> beamsIndexes;
		map<int, vector<int>> numTails;
		map<int, vector<int>> isNoteGroupped;
		map<int, vector<int>> hasRests;
		map<int, vector<int>> maxNumLines;
		map<int, vector<float>> startPnts;
		map<int, vector<vector<float>>> allNotesXCoords;
		map<int, vector<vector<float>>> allNotesYCoords1;
		map<int, vector<float>> allNotesYCoords2;
		// two vectors to hold the maximum and minimum Y coordinates of each (group of) note(s)
		map<int, vector<float>> allNotesMaxYPos;
		map<int, vector<float>> allNotesMinYPos;
		// a vector to hold number of octaves offset, in case a note is drawn to high or too low
		map<int, vector<int>> numOctavesOffset;
		map<int, vector<float>> allOttavasYCoords;
		map<int, vector<string>> allTexts;
		map<int, vector<int>> allTextsIndexes;
		map<int, vector<float>> allTextsXCoords;
		map<int, vector<float>> allTextsYCoords;
		map<int, vector<int>> numBeams;
		map<int, vector<vector<float>>> beamYCoords1;
		map<int, vector<vector<float>>> beamYCoords2;
		map<int, vector<float>> dynsXCoords;
		map<int, vector<float>> dynsYCoords;
		map<int, vector<float>> dynsRampStartXCoords;
		map<int, vector<float>> dynsRampStartYCoords;
		map<int, vector<float>> dynsRampEndXCoords;
		map<int, vector<float>> dynsRampEndYCoords;
		map<int, vector<int>> dynamicsRampDir;
		map<int, vector<float>> articulXPos;
		map<int, vector<float>> articulYPos;
		map<int, float> maxYCoord;
		map<int, float> minYCoord;
		float yStartPnt[2];
		float xCoef; // multiplied by X coordinates, based on how many bars are displayed in horizontal view
		float xOffset;
		float yStartPntRecenter[2];
		float xLength;
		int scoreOrientation;
		map<int, float> distBetweenBeats;
		map<int, bool> tacet;
		ofTrueTypeFont notationFont;
		ofTrueTypeFont textFont;
		string notesSyms[5];
		string accSyms[9];
		string restsSyms[6];
		string dynSyms[8];
		string articulSyms[6];
		string octaveSyms[3];
		// arrays to hold the widths and heights of strings
		// since these can be called from a threaded class and calling stringWidth() won't work
		float dynSymsWidths[8];
		float dynSymsHeights[8];
		float restsSymsWidths[6];
		float restsSymsHeights[6];
		//ofImage quarterSharp;
		float noteWidth;
		float noteHeight;
		float middleOfStaff;
		float stemHeight;
		int beamIndexCounter;
		int beamIndexCounter2;
		int lineWidth; // from ofApp.h to give the correct offset to stems and connecting lines
		int beamsLineWidth;
		float staffDist;
		int fontSize;
		int clefIndex;
		float restYOffset;
		float halfStaffDist;
		int denominator;
		int numerator;
		int beats;
		bool animate;
		int whichNote;
};

#endif
