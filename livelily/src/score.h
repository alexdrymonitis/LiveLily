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
		float getXStartOffset();
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
		// instruments merged to one staff (see \merge) are voices, as in LilyPond: odd voices have their stems up and even voices down
		// voice is from 1, and with numVoices 1 the stems are placed as usual
		void setVoice(int voice, int numVoices);
		int getVoice() { return voiceNum; }
		int getForcedStemDir() { return forcedStemDir; }
		// the notes (and chords and rests) of a bar, for laying out the voices of a staff (see ofApp::applyMergeLayout())
		struct NoteColumn {
			float x;                 // the X coordinate of the column, which is the same for the notes of all voices that start together
			bool rest;
			std::vector<float> headsY; // the Y coordinates of its note heads
			int headType;            // 1 for whole, 2 for half, 4 for filled note heads
			unsigned dots;
		};
		std::vector<NoteColumn> getColumns(int bar);
		// moves the notes of a column on the X axis, so that the note heads of voices don't collide
		void shiftColumn(int bar, int noteNdx, float dx);
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
		// sets the base and edge notes of a chord and the X coords of its notes, for a stem direction
		void layoutChordNotes(int bar, int i, float x, int stemDir);
		// the lowest point of a note, including the lower half of its note heads, its accidentals, and its tie
		float lowestPointOfNote(int bar, int noteNdx, bool includeTie);
		void storeMinMaxY(int bar);
		void deleteBar(int bar);
		void insertNaturalSigns(int bar, int loopNdx, std::vector<int> *v);
		std::pair<int, int> isBarLinked(int bar);
		void moveScoreX(int numPixels);
		void moveScoreY(int numPixels);
		void recenterScore();
		// nextBar is the bar that is played after this one (the same bar if it plays alone), or -1 if it is not known
		// glissandi, ties, slurs, and crescendi or diminuendi at the end of the bar go towards its first notes
		void drawNotes(int bar, int loopNdx, std::vector<int> *v, float xStartPnt, float yStartPnt,
				float yOffset, bool animation, float xCoef, int nextBar = -1);
		// whether a crescendo or diminuendo is left open at the end of a bar, so it is drawn to the end of the bar
		void setDynamicsRampOpenAtEnd(int bar, bool open);
		void drawThickLine(float x1, float y1, float x2, float y2);
		int drawRest(int bar, int restDur, float x, float yStartPnt, ofColor color, float yOffset);
		void drawAccidentals(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		// whether an accidental index (see accSyms) is drawn (not -1 for no accidental, or 9 for an accidental that is not repeated)
		bool isDrawnAccidental(int ndx);
		// the symbol of the font that has the size of an accidental, the flat for quarter tone flats and the sharp for quarter tone sharps
		const std::string& accidentalSizeSym(int ndx);
		// draws an accidental with its baseline at y, ending at the same point as its symbol from the font would end, drawn at x
		void drawAccidental(int ndx, float x, float y);
		float accidentalWidth(int ndx);
		void drawGlissandi(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef, int nextBar);
		void drawTuplets(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		void drawArticulations(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		void drawOttavas(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		void drawOttavaLine(int bar, unsigned startNdx, unsigned endNdx, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		void drawText(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		void drawSlurs(int bar, int loopNdx, float xStartPnt, float yStartPnt, float yOffset, float xCoef, int nextBar);
		void drawTies(int thisBar, int loopNdx, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		void drawTie(int thisBar, unsigned i, unsigned j, float xStartPnt, float yStartPnt, float yOffset, float xCoef, int stemDirCoef);
		void drawDynamics(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef);
		void printVector(std::vector<int> v);
		void printVector(std::vector<std::string> v);
		void printVector(std::vector<float> v);
		bool mute;
		int voiceNum = 1;
		int forcedStemDir = 0; // 1 up, -1 down, 0 set by the position of the notes
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
		// how far a tie starts from the center of the note head, and ends from the center of the next note head
		float tieXOffset();
		// how far from the center of the note head a tie starts on the Y axis
		float tieYOffset();
		// how far from the center of the note head a slur starts and ends on the Y axis, when it is on the side of the note head
		// which is a bit further than a tie, as in LilyPond, so that a tie and a slur on the same note don't collide
		float slurYOffset();
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
		std::map<int, std::vector<float>> noteXCoords; // the X coord of each note (or chord) before its notes are shifted
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
		std::map<int, bool> dynamicsRampOpenAtEnd;
		int scoreOrientation;
		int scoreOrientationForDots;
		std::map<int, float> distBetweenBeats;
		std::map<int, bool> tacet;
		ofTrueTypeFont notationFont;
		ofTrueTypeFont textFont;
		// whole note, half note (without stem), filled note, tail for upward stem, tail for downward stem
		std::string notesSyms[5] = {"w", "ú", "ö", "j", "J"};
		// double flat, 1.5 flat, flat, half flat, natural, half sharp, sharp, 1.5 sharp, double sharp
		// the Sonata font has no symbols for quarter tones (the empty strings), so these are drawn by drawAccidental()
		// with the Stein-Zimmermann symbols, a mirrored flat for the half flat, a mirrored flat and a flat for the 1.5 flat
		// and a sharp with one or three vertical strokes for the half and 1.5 sharp
		// index 9 is for a note whose accidental is not drawn, as an earlier note in the bar has the same accidental
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
