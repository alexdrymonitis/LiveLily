#ifndef SCORE_H
#define SCORE_H

#include "ofMain.h"
#include <vector>

#define EXTRALINECOEFF 0.7
#define BEAMDISTCOEFF 0.75

typedef struct _intPair
{
	int first;
	int second;
} intPair;

typedef struct _uintPair
{
	unsigned first;
	unsigned second;
} uintPair;

class Staff
{
	public:
		Staff();
		void setID(int id);
		void setClef(int bar, int clefIdx);
		int getClef(int bar);
		void setRhythm(bool isRhythm);
		void setSize(int fontSize, float staffLinesDist);
		void setMeter(int bar, int numer, int denom);
		intPair getMeter(int bar);
		void setTempo(int bar, int BPMTempo, int beatAtValue, bool hasDot);
		void setOrientation(int orientation);
		void setNumBarsToDisplay(int numBars);
		float getXCoef();
		void setCoords(float xLen, float y1, float y2, float dist);
		void correctYAnchor(float y1, float y2);
		float getXLength();
		float getYAnchor();
		void moveScoreX(float numPixels);
		void moveScoreY(float numPixels);
		void recenterScore();
		float getClefXOffset();
		float getMeterXOffset();
		void copyMelodicLine(int barIndex, int barToCopy);
		void drawStaff(int loopIndex, float xStartPnt, float yOffset, bool drawClef,
				bool drawMeter, bool drawLoopStartEnd, bool drawTempo);
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
		int scoreOrientationForDots;
		map<int, int> numerator;
		map<int, int> denominator;
		map<int, int> BPMTempi;
		map<int, int> tempoBase;
		// half note, quarter note, eighth note, sixteenth note, thirty-second note, sixty-fourth note
		string BPMDisplayNotes[7] = {"w", "h", "q", "e", "x", "r", "Æ"};
		map<int, bool> BPMDisplayDots;
		map<int, int> meterIndex;
		int lineWidth;
		float staffDist;
		float noteWidth;
		// G, F, C clefs
		string clefSyms[3] = {"&", "?", "B"};
		string meterSyms[2] = {"c", "C"};
		map<int, int> clefIndex;
		int fontSize;
		bool rhythm; // for rhythmic instruments
		ofTrueTypeFont notationFont;
		ofTrueTypeFont BPMDisplayFont;
};


class Notes
{
	public:
		Notes();
		void setID(int id);
		float getDist(int bar);
		void setRhythm(bool isRhythm);
		void setFontSize(int fontSize, float staffLinesDist);
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
		void setClef(int bar, int clefIdx);
		void setOrientation(int orientation);
		void setOrientationForDots(int orientation);
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
				vector<vector<int>>articul,
				vector<int> dyns,
				vector<int> dynIdx,
				vector<int> dynRampStart,
				vector<int> dynRampEnd,
				vector<int> dynRampDir,
				vector<intPair> slurIndexes,
				bool isWholeBarSlurred,
				map<int, intPair> tupletRatios,
				map<int, uintPair> tupletStartStop,
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
		void storeTupletCoords(int bar);
		void storeArticulationsCoords(int bar);
		void storeSlurCoords(int bar);
		void storeOttavaCoords(int bar);
		void storeTextCoords(int bar);
		void storeDynamicsCoords(int bar);
		void storeMinMaxY(int bar);
		void copyMelodicLine(int barIndex, int barToCopy);
		void insertNaturalSigns(int bar, int loopNdx, vector<int> *v);
		intPair isBarLinked(int bar);
		void drawNotes(int bar, int loopNdx, vector<int> *v, float xStartPnt, float yOffset, bool animation, float xCoef);
		void drawBeams(float x1, float y1, float x2, float y2);
		int drawRest(int bar, int restDur, float x, int color, float yOffset);
		void drawAccidentals(int bar, float xStartPnt, float yOffset, float xCoef);
		void drawGlissandi(int bar, float xStartPnt, float yOffset, float xCoef);
		void drawTuplets(int bar, float xStartPnt, float yOffset, float xCoef);
		void drawArticulations(int bar, float xStartPnt, float yOffset, float xCoef);
		void drawOttavas(int bar, float xStartPnt, float yOffset, float xCoef);
		void drawOttavaLine(int bar, unsigned startNdx, unsigned endNdx, float xStartPnt, float yOffset, float xCoef);
		void drawText(int bar, float xStartPnt, float yOffset, float xCoef);
		void drawSlurs(int bar, int loopNdx, float xStartPnt, float yOffset, float xCoef);
		void drawDynamics(int bar, float xStartPnt, float yOffset, float xCoef);
		void printVector(vector<int> v);
		void printVector(vector<string> v);
		void printVector(vector<float> v);
		~Notes();
		bool mute;
	private:
		int objID;
		bool rhythm;
		vector<int> allBars;
		map<int, vector<vector<int>>> allNotes;
		map<int, vector<int>> actualDurs;
		map<int, vector<int>> hasStem;
		// base and edge indexes of Y coords for chords
		// base is the bottom note with a stem up and edge is the top one
		// and vice verse with a stem down
		map<int, vector<int>> allChordsBaseIndexes;
		map<int, vector<int>> allChordsEdgeIndexes;
		map<int, vector<vector<int>>> allAccidentals;
		map<int, vector<vector<int>>> naturalSignsNotWritten;
		map<int, vector<vector<int>>> allOctaves;
		map<int, vector<int>> allOttavas;
		map<int, vector<int>> allGlissandi;
		map<int, vector<vector<int>>> allArticulations;
		//map<int, vector<int>> slurBeginnings;
		//map<int, vector<int>> slurEndings;
		map<int, vector<intPair>> slurIndexes;
		map<int, vector<float>> slurStartX;
		map<int, vector<float>> slurStopX;
		map<int, vector<float>> slurStartY;
		map<int, vector<float>> slurStopY;
		map<int, vector<float>> slurMiddleX1;
		map<int, vector<float>> slurMiddleY1;
		map<int, vector<float>> slurMiddleX2;
		map<int, vector<float>> slurMiddleY2;
		map<int, vector<intPair>> slurLinks;
		map<int, bool> isWholeSlurred;
		// the map below is queried when creating loops, to determine whether a bar can be included in a loop
		// if a bar is linked to another bar due to slurs, glissandi, or (de)crescendi spanning over more than one bar
		// then it will not be able to include this bar in a loop
		map<int, intPair> isLinked;
		map<int, map<int, intPair>> tupRatios;
		map<int, map<int, uintPair>> tupStartStop;
		map<int, map<int, vector<float>>> tupXCoords;
		map<int, map<int, vector<float>>> tupYCoords;
		map<int, map<int, int>> tupDirs;
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
		map<int, vector<vector<float>>> allNoteCoordsX;
		map<int, vector<float>> allNoteCoordsXOffset;
		map<int, vector<float>> allChordsChangeXCoef;
		map<int, vector<vector<float>>> allNoteHeadCoordsY;
		map<int, vector<float>> allNoteStemCoordsY;
		// two vectors to hold the maximum and minimum Y coordinates of each (group of) note(s)
		map<int, vector<float>> allNotesMaxYPos;
		map<int, vector<float>> allNotesMinYPos;
		// a vector to hold number of octaves offset, in case a note is drawn to high or too low
		map<int, vector<int>> numOctavesOffset;
		map<int, vector<float>> allOttavasYCoords;
		map<int, vector<int>> allOttavasChangedAt;
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
		map<int, vector<vector<float>>> articulYPos;
		map<int, float> maxYCoord;
		map<int, float> minYCoord;
		float yStartPnt[2];
		float xCoef; // multiplied by X coordinates, based on how many bars are displayed in horizontal view
		float xOffset;
		// a map of xOffsets so that these can be queried for other bars than the one currently drawn
		// this is useful when drawing items that span over more than one bar, like slurs etc.
		map<int, float> xOffsets;
		float yStartPntRecenter[2];
		float xLength;
		int scoreOrientation;
		int scoreOrientationForDots;
		map<int, float> distBetweenBeats;
		map<int, bool> tacet;
		ofTrueTypeFont notationFont;
		ofTrueTypeFont textFont;
		// whole note, half note (without stem), filled note, tail for upward stem, tail for downward stem
		string notesSyms[5] = {"w", "ú", "ö", "j", "J"};
		// double flat, dummy 1.5 flat, flat, dummy half flat, natural,
		// dummy half sharp, sharp, dummy 1.5 sharp, double sharp
		string accSyms[9] = {"º", "", "b", "", "n", "", "#", "", "Ü"};
		// whole and half (first symbol), and the rest of rests
		string restsSyms[6] = {"î", "Î", "ä", "Å", "¨", "ô"};
		// ppp, pp, p, mp, mf, f, ff, fff
		string dynSyms[8] = {"¸", "¹", "p", "P", "F", "f", "Ä", "ì"};
		// marcato, trill, tenuto, staccatissimo, accent, staccato
		string articulSyms[6] = {"^", "Ù", "_", "à", ">", "."};
		string octaveSyms[3] = {"", "8", "15"};
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
		map<int, int> clefIndex;
		float restYOffset;
		float halfStaffDist;
		map<int, int> denominator;
		map<int, int> numerator;
		map<int, int> beats;
		bool animate;
		int whichNote;
};

#endif
