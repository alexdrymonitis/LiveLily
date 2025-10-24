#ifndef SCORE_H
#define SCORE_H

#include "ofMain.h"
#include <vector>

#define EXTRALINECOEFF 0.7
#define BEAMDISTCOEFF 0.75
#define MINDUR 256

class Staff
{
	public:
		Staff();
		void setID(int id);
		void setGroup(int id);
		void setClef(int bar, int clefIdx);
		int getClef(int bar);
		void setRhythm(bool isRhythm);
		void setSize(int fontSize, float staffLinesDist);
		void setMeter(int bar, int numer, int denom);
		std::pair<int, int> getMeter(int bar);
		void setTempo(int bar, int BPMTempo, int beatAtValue, bool hasDot);
		void setOrientation(int orientation);
		void setNumBarsToDisplay(int numBars);
		float getXCoef();
		void setCoords(float xLen, float dist);
		float getXLength();
		float getClefXOffset();
		float getMeterXOffset();
		void moveScoreX(int numPixels);
		void moveScoreY(int numPixels);
		void recenterScore();
		void drawStaff(int loopIndex, float xStartPnt, float yStartPnt, float yOffset, bool drawClef,
				bool drawMeter, bool drawLoopStartEnd, bool drawTempo);
		void drawLoopStart(int loopIndex, float xStart, float yStartPnt, float yOffset);
		void drawLoopEnd(int loopIndex, float xEnd, float yStartPnt, float yOffset);
		void drawScoreEnd(int loopIndex, float xEnd, float yStartPnt, float yOffset);
		void drawThickLine(float x1, float y1, float x2, float y2, float width);
		bool isLoopStart;
		bool isLoopEnd;
		bool isScoreEnd;
		ofColor staffColor;
	private:
		int objID;
		int groupID;
		float xLength;
		float xCoef; // multiplied by X coordinates, based on how many bars are displayed in horizontal view
		float yAnchor[2];
		float yAnchorRecenter[2];
		float yDist;
		float clefLength;
		float meterLength;
		int scoreOrientation;
		int scoreOrientationForDots;
		std::map<int, int> numerator;
		std::map<int, int> denominator;
		std::map<int, int> BPMTempi;
		std::map<int, int> tempoBase;
		// half note, quarter note, eighth note, sixteenth note, thirty-second note, sixty-fourth note
		std::string BPMDisplayNotes[7] = {"w", "h", "q", "e", "x", "r", "Æ"};
		std::map<int, bool> BPMDisplayDots;
		std::map<int, int> meterIndex;
		int lineWidth;
		float staffDist;
		float noteWidth;
		float xStartOffset;
		float yStartOffset;
		// G, F, C clefs
		std::string clefSyms[3] = {"&", "?", "B"};
		std::string meterSyms[2] = {"c", "C"};
		std::map<int, int> clefIndex;
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
		void setCoords(float xLen, float staffLinesDist, int fontSize);
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
		void setAccidentalsOffsetCoef(float coef);
		void setNotes(int bar,
				std::vector<std::vector<int>> notes,
				std::vector<std::vector<int>> accidentals,
				std::vector<std::vector<int>> naturalsNotWritten,
				std::vector<std::vector<int>> octaves,
				std::vector<int> ottavas,
				std::vector<int> durs,
				std::vector<int> dotNdxs,
				std::vector<unsigned> dotsCntr,
				std::vector<int> gliss,
				std::vector<std::vector<int>>articul,
				std::vector<int> dyns,
				std::vector<int> dynIdx,
				std::vector<int> dynRampStart,
				std::vector<int> dynRampEnd,
				std::vector<int> dynRampDir,
				std::vector<std::pair<int, int>> slurIndexes,
				std::vector<int> tieNdxs,
				bool isWholeBarSlurred,
				std::map<int, std::pair<int, int>> tupletRatios,
				std::map<int, std::pair<unsigned, unsigned>> tupletStartStop,
				std::vector<std::string> texts,
				std::vector<std::vector<int>> textIndexes,
				int BPMMult);
		void changeNotesBasedOnClef(int bar);
		void correctDynamics(int bar, std::vector<int> dyns);
		void setNotePositions(int bar);
		float getXLength();
		void storeTupletCoords(int bar);
		void storeArticulationsCoords(int bar);
		void storeSlurCoords(int bar);
		void storeOttavaCoords(int bar);
		void storeTextCoords(int bar);
		void storeDynamicsCoords(int bar);
		void storeMinMaxY(int bar);
		void deleteBar(int bar);
		void insertNaturalSigns(int bar, int loopNdx, std::vector<int> *v);
		std::pair<int, int> isBarLinked(int bar);
		void moveScoreX(int numPixels);
		void moveScoreY(int numPixels);
		void recenterScore();
		void drawNotes(int bar, int loopNdx, std::vector<int> *v, float xStartPnt, float yStartPnt,
				float yOffset, bool animation, float xCoef);
		void drawBeams(float x1, float y1, float x2, float y2);
		int drawRest(int bar, int restDur, float x, float yStartPnt, ofColor color, float yOffset);
		void drawAccidentals(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		void drawGlissandi(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		void drawTuplets(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		void drawArticulations(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		void drawOttavas(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		void drawOttavaLine(int bar, unsigned startNdx, unsigned endNdx, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		void drawText(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		void drawSlurs(int bar, int loopNdx, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		void drawTies(int thisBar, int loopNdx, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		void drawTie(int thisBar, unsigned i, unsigned j, float xStartPnt, float yStartPnt, float yOffset, float xCoef, int stemDirCoef);
		void drawDynamics(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		void printVector(std::vector<int> v);
		void printVector(std::vector<std::string> v);
		void printVector(std::vector<float> v);
		bool mute;
		ofColor notesColor;
		ofColor activeNotesColor;
	private:
		int objID;
		bool rhythm;
		std::vector<int> allBars;
		std::map<int, std::vector<std::vector<int>>> allNotes;
		std::map<int, std::vector<int>> actualDurs;
		std::map<int, std::vector<int>> hasStem;
		// base and edge indexes of Y coords for chords
		// base is the bottom note with a stem up and edge is the top one
		// and vice verse with a stem down
		std::map<int, std::vector<int>> allChordsBaseIndexes;
		std::map<int, std::vector<int>> allChordsEdgeIndexes;
		std::map<int, std::vector<std::vector<int>>> allAccidentals;
		std::map<int, std::vector<std::vector<int>>> naturalSignsNotWritten;
		std::map<int, std::vector<std::vector<int>>> allOctaves;
		std::map<int, std::vector<int>> allOttavas;
		std::map<int, std::vector<int>> allGlissandi;
		std::map<int, std::vector<std::vector<int>>> allArticulations;
		//std::map<int, std::vector<int>> slurBeginnings;
		//std::map<int, std::vector<int>> slurEndings;
		std::map<int, std::vector<std::pair<int, int>>> slurIndexes;
		std::map<int, std::vector<float>> slurStartX;
		std::map<int, std::vector<float>> slurStopX;
		std::map<int, std::vector<float>> slurStartY;
		std::map<int, std::vector<float>> slurStopY;
		std::map<int, std::vector<float>> slurMiddleX1;
		std::map<int, std::vector<float>> slurMiddleY1;
		std::map<int, std::vector<float>> slurMiddleX2;
		std::map<int, std::vector<float>> slurMiddleY2;
		std::map<int, std::vector<std::pair<int, int>>> slurLinks;
		std::map<int, std::vector<int>> tieIndexes;
		std::map<int, bool> isWholeSlurred;
		// the std::map below is queried when creating loops, to determine whether a bar can be included in a loop
		// if a bar is linked to another bar due to slurs, glissandi, or (de)crescendi spanning over more than one bar
		// then it will not be able to include this bar in a loop
		std::map<int, std::pair<int, int>> isLinked;
		std::map<int, std::map<int, std::pair<int, int>>> tupRatios;
		std::map<int, std::map<int, std::pair<unsigned, unsigned>>> tupStartStop;
		std::map<int, std::map<int, std::vector<float>>> tupXCoords;
		std::map<int, std::map<int, std::vector<float>>> tupYCoords;
		std::map<int, std::map<int, int>> tupDirs;
		std::map<int, std::vector<int>> stemDirections;
		std::map<int, std::vector<int>> grouppedStemDirs;
		std::map<int, std::vector<int>> grouppedStemDirsCoeff;
		std::map<int, std::vector<int>> durations;
		std::map<int, std::vector<int>> dotIndexes;
		std::map<int, std::vector<unsigned>> dotsCounter;
		std::map<int, std::vector<int>> dynamics;
		std::map<int, std::vector<int>> dynamicsIndexes;
		std::map<int, std::vector<int>> dynamicsRampStart;
		std::map<int, std::vector<int>> dynamicsRampEnd;
		std::map<int, std::vector<std::vector<int>>> numExtraLines;
		std::map<int, std::vector<std::vector<int>>> extraLinesYPos;
		std::map<int, std::vector<std::vector<int>>> extraLinesDir;
		std::map<int, std::vector<std::vector<int>>> hasExtraLines;
		std::map<int, std::vector<int>> numNotesWithBeams;
		std::map<int, std::vector<std::vector<int>>> beamsIndexes;
		std::map<int, std::vector<int>> numTails;
		std::map<int, std::vector<int>> isNoteGroupped;
		std::map<int, std::vector<int>> hasRests;
		std::map<int, std::vector<int>> maxNumLines;
		std::map<int, std::vector<float>> startPnts;
		std::map<int, std::vector<std::vector<float>>> allNoteCoordsX;
		std::map<int, std::vector<std::vector<bool>>> allNoteShiftX;
		std::map<int, std::vector<float>> allNoteCoordsXOffset;
		std::map<int, std::vector<float>> allChordsChangeXCoef;
		std::map<int, std::vector<std::vector<float>>> allNoteHeadCoordsY;
		std::map<int, std::vector<float>> allNoteStemCoordsY;
		// two std::vectors to hold the maximum and minimum Y coordinates of each (group of) note(s)
		std::map<int, std::vector<float>> allNotesMaxYPos;
		std::map<int, std::vector<float>> allNotesMinYPos;
		// a std::vector to hold number of octaves offset, in case a note is drawn to high or too low
		std::map<int, std::vector<int>> numOctavesOffset;
		std::map<int, std::vector<float>> allOttavasYCoords;
		std::map<int, std::vector<int>> allOttavasChangedAt;
		std::map<int, std::vector<std::string>> allTexts;
		std::map<int, std::vector<std::vector<int>>> allTextsIndexes;
		std::map<int, std::vector<float>> allTextsXCoords;
		std::map<int, std::vector<std::vector<float>>> allTextsYCoords;
		std::map<int, std::vector<int>> numBeams;
		std::map<int, std::vector<std::vector<float>>> beamYCoords1;
		std::map<int, std::vector<std::vector<float>>> beamYCoords2;
		std::map<int, std::vector<float>> dynsXCoords;
		std::map<int, std::vector<float>> dynsYCoords;
		std::map<int, std::vector<float>> dynsRampStartXCoords;
		std::map<int, std::vector<float>> dynsRampStartYCoords;
		std::map<int, std::vector<float>> dynsRampEndXCoords;
		std::map<int, std::vector<float>> dynsRampEndYCoords;
		std::map<int, std::vector<int>> dynamicsRampDir;
		std::map<int, std::vector<float>> articulXPos;
		std::map<int, std::vector<std::vector<float>>> articulYPos;
		std::map<int, float> maxYCoord;
		std::map<int, float> minYCoord;
		std::map<int, int> BPMMultiplier;
		float xCoef; // multiplied by X coordinates, based on how many bars are displayed in horizontal view
		float xStartOffset;
		float yStartOffset;
		// a std::map of xOffsets so that these can be queried for other bars than the one currently drawn
		// this is useful when drawing items that span over more than one bar, like slurs etc.
		std::map<int, float> xOffsets;
		float yStartPntRecenter[2];
		float xLength;
		int scoreOrientation;
		int scoreOrientationForDots;
		std::map<int, float> distBetweenBeats;
		std::map<int, bool> tacet;
		ofTrueTypeFont notationFont;
		ofTrueTypeFont textFont;
		// whole note, half note (without stem), filled note, tail for upward stem, tail for downward stem
		std::string notesSyms[5] = {"w", "ú", "ö", "j", "J"};
		// double flat, dummy 1.5 flat, flat, dummy half flat, natural,
		// dummy half sharp, sharp, dummy 1.5 sharp, double sharp
		std::string accSyms[9] = {"º", "", "b", "", "n", "", "#", "", "Ü"};
		// whole and half (first symbol), and the rest of rests
		std::string restsSyms[6] = {"î", "Î", "ä", "Å", "¨", "ô"};
		// ppp, pp, p, mp, mf, f, ff, fff
		std::string dynSyms[8] = {"¸", "¹", "p", "P", "F", "f", "Ä", "ì"};
		// marcato, trill, tenuto, staccatissimo, accent, staccato
		std::string articulSyms[6] = {"^", "Ù", "_", "à", ">", "."};
		std::string octaveSyms[3] = {"", "8", "15"};
		int baseDurations[7] = {4, 8, 16, 32, 64, 128, 256};
		// arrays to hold the widths and heights of std::strings
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
		std::map<int, int> clefIndex;
		float restYOffset;
		float halfStaffDist;
		float accidentalsOffsetCoef;
		std::map<int, int> denominator;
		std::map<int, int> numerator;
		std::map<int, int> beats;
		bool animate;
		int whichNote;
};

#endif
