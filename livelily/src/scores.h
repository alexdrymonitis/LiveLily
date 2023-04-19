#pragma once

#include "ofMain.h"

#define EXTRALINECOEFF 0.7

class Staff
{
  public:
    Staff(float x, float y, float z, float dist, int numer, int denom);
    void setSize(int fontSize);
    void setCoords(int loopIndex, float x, float y, float z, float dist, int numer, int denom);
    void setClef(int clefIdx);
    float getXPos();
    void drawStaff(int loopIndex);
    void drawLoopStart(int loopIndex);
    void drawLoopEnd(int loopIndex);
    void drawScoreEnd(int loopIndex);
    void drawThickLine(float x1, float y1, float x2, float y2, float width);
    bool isLoopStart;
    bool isLoopEnd;
    bool isScoreEnd;
    ~Staff();
  private:
    int numLines;
    float xStartPnt;
    vector<float> yPnt;
    float xEndPnt;
    float yDist;
    int numerator;
    int denominator;
    int meterIndex;
    int lineWidth;
    string clefSyms[3];
    string meterSyms[2];
    // store character widths so we can call them from a threaded function
    float meterSymsWidths[66];
    int clefIndex;
    int fontSize;
    ofTrueTypeFont notationFont;
};


class Notes {
  public:
    Notes(int denom, int fontSize, int id);
    void setFontSize(int fontSize);
    void setCoords(int bar, int loopIndex, float xStart, float yStart,
                   float xEnd, float staffLinesDist, int fontSize, int numBeats);
    void setLength(int bar, int numBeats);
    float getMaxYPos(int bar);
    float getMinYPos(int bar);
    float getMinVsAnchor(int loopIndex, int bar);
    float getNoteWidth();
    float getNoteHeight();
    void setClef(int clefIdx);
    void setAnimation(bool anim);
    void setActiveNote(int note);
    void setNotes(int bar, int loopIndex, vector<int> durs, int numBeats);
    void setNotes(int bar, int loopIndex,
                  vector<vector<int>> notes,
                  vector<vector<int>> accidentals,
                  vector<vector<int>> octaves, vector<int> durs,
                  vector<int> dots,
                  vector<int> gliss, vector<int>articul,
                  vector<int> dyns, vector<int> dynIdx,
                  vector<int> dynRampStart, vector<int> dynRampEnd,
                  vector<int> dynRampDir,
                  vector<int> slurBegin,
                  vector<int> slurEnd,
                  vector<string> texts, vector<int> textIndexes,
                  int numBeats);
    void correctDynamics(int bar, vector<int> dyns);
    void resizeLoopIndexBasedVectors(int bar, int loopIndex);
    void setNotePositions(int bar, int loopIndex);
    void saveBeamsIndexes(int bar, int beamIndex, float x, vector<float> y, int rests, int numLines);
    void saveBeamsIndexes(int bar, int beamIndex, float x);
    void storeAllNotesSecondYCoords(int bar, int loopIndex, int noteIndex, float y);
    void sortAllNotesCoords(int bar, int loopIndex);
    void storeArticulationsCoords(int bar, int loopIndex);
    void storeTextCoords(int bar, int loopIndex);
    void storeSlurCoords(int bar, int loopIndex);
    void storeDynamicsCoords(int bar, int loopIndex);
    void storeMinMaxY(int bar, int loopIndex);
    void findExtremes(int bar, int loopIndex);
    float offsetToExtreme(int loopIndex, int index1, int index2, float extreme);
    void drawNotes(int bar, int loopIndex);
    void drawBeams(float x1, float y1, float x2, float y2);
    int drawRest(int bar, int loopIndex, int restDur, float x, int color);
    void drawAccidentals(int bar, int loopIndex);
    void drawGlissandi(int bar, int loopIndex);
    void drawArticulations(int bar, int loopIndex);
    void drawText(int bar, int loopIndex);
    void drawSlurs(int bar, int loopIndex);
    void drawDynamics(int bar, int loopIndex);
    void drawTails(float xPos, float yPos, int numTails, int stemDir);
    void printVector(vector<int> v);
		void printVector(vector<string> v);
    void printVector(vector<float> v);
    ~Notes();
    bool mute;
  private:
    int objID;
    vector<vector<vector<int>>> allNotes;
    vector<vector<int>> actualDurs;
    vector<vector<bool>> hasStem;
    vector<vector<float>> stemsX;
    vector<vector<vector<float>>> stemsY1;
    vector<vector<vector<float>>> stemsY2;
    // base and edge indexes of Y coords for chords
    vector<vector<int>> whichIdxBase;
    vector<vector<int>> whichIdxEdge;
    vector<vector<vector<int>>> allAccidentals;
    vector<vector<vector<int>>> allOctaves;
    vector<vector<int>> allGlissandi;
    vector<vector<int>> allArticulations;
    vector<vector<int>> slurBeginnings;
    vector<vector<int>> slurEndings;
    vector<vector<float>> slurStartX;
    vector<vector<float>> slurStopX;
    vector<vector<vector<float>>> slurStartY;
    vector<vector<vector<float>>> slurStopY;
    vector<vector<float>> slurMiddleX1;
    vector<vector<vector<float>>> slurMiddleY1;
    vector<vector<float>> slurMiddleX2;
    vector<vector<vector<float>>> slurMiddleY2;
    vector<vector<int>> stemDirections;
    vector<vector<int>> grouppedStemDirs;
    vector<vector<int>> grouppedStemDirsCoeff;
    vector<vector<int>> durations;
    vector<vector<int>> dotIndexes;
    vector<vector<int>> dynamics;
    vector<vector<int>> dynamicsIndexes;
    vector<vector<int>> dynamicsRampStart;
    vector<vector<int>> dynamicsRampEnd;
    vector<vector<vector<int>>> numExtraLines;
    vector<vector<vector<int>>> extraLinesYPos;
    vector<vector<vector<int>>> extraLinesDir;
    vector<vector<vector<bool>>> hasExtraLines;
    vector<vector<int>> numNotesWithBeams;
    vector<vector<float>> beamsX;
    vector<vector<int>> beamsIndexes;
    vector<vector<vector<float>>> allBeamYCoords;
    vector<vector<int>> numTails;
    vector<vector<int>> hasRests;
    vector<vector<int>> maxNumLines;
    vector<vector<float>> startPnts;
    vector<vector<vector<float>>> allNotesXCoords;
    vector<vector<vector<vector<float>>>> allNotesYCoords1;
    vector<vector<vector<vector<float>>>> allNotesYCoords2;
    // two vectors to hold the maximum and minimum Y coordinates of each (group of) note(s)
    vector<vector<float>> allNotesMaxYPos;
    vector<vector<float>> allNotesMinYPos;
    // a vector to hold number of octaves offset, in case a note is drawn to high or too low
    vector<vector<int>> numOctavesOffset;
    // 2D version of the three 3D vector above for proper sorting
    vector<vector<float>> allNotesYCoords2D2;
    vector<vector<string>> allTexts;
    vector<vector<int>> allTextsIndexes;
    vector<vector<float>> allTextsXCoords;
    vector<vector<vector<float>>> allTextsYCoords;
    vector<vector<vector<int>>> numBeams;
    vector<vector<vector<float>>> beamXCoords1;
    vector<vector<vector<float>>> beamXCoords2;
    vector<vector<vector<vector<float>>>> beamYCoords1;
    vector<vector<vector<vector<float>>>> beamYCoords2;
    vector<vector<float>> dynsXCoords;
    vector<vector<vector<float>>> dynsYCoords;
    vector<vector<float>> dynsRampStartXCoords;
    vector<vector<vector<float>>> dynsRampStartYCoords;
    vector<vector<float>> dynsRampEndXCoords;
    vector<vector<vector<float>>> dynsRampEndYCoords;
    vector<vector<int>> dynamicsRampDir;
    vector<vector<float>> articulXPos;
    vector<vector<vector<float>>> articulYPos;
    vector<float> maxYCoord;
    vector<float> minYCoord;
    vector<float> xStartPnt;
    vector<vector<float>> yStartPnt;
    vector<float> xEndPnt;
    vector<float> xLength;
    vector<float> distBetweenBeats;
    vector<bool> tacet;
    ofTrueTypeFont notationFont;
    ofTrueTypeFont textFont;
    string notesSyms[5];
    string accSyms[9];
    string restsSyms[6];
    string dynSyms[8];
    string articulSyms[6];
    string octaveSym;
    // arrays to hold the widths and heights of strings
    // since these can be called from a threaded class and calling stringWidth() won't work
    float dynSymsWidths[8];
    float dynSymsHeights[8];
    float restsSymsWidths[6];
    float restsSymsHeights[6];
    ofImage quarterSharp;
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
    int beats;
    bool animate;
    int whichNote;
};
