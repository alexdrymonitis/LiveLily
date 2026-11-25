#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include "score.h"
#include "utils.h"
#include <iostream>
#include "limits.h"

namespace {
// what the score draws with (see canvas.h)
Canvas& cv() { return *Canvas::current; }
}

/************************* Staff class ************************/
//--------------------------------------------------------------
Staff::Staff()
{
	lineWidth = (int)cv().lineWidth;
	clefIndex[0] = 0;
	isLoopStart = false;
	isLoopEnd = false;
	isScoreEnd = false;
	meterIndex[0] = 0;
	rhythm = false;
	scoreOrientation = 0;
	clefLength = 0;
	xCoef = 1;
	// both offsets must be initialized, otherwise the vertical lines at the sides of the staff
	// which use yStartOffset, get a random length
	xStartOffset = 0;
	yStartOffset = 0;
	groupID = -1;
	staffColor = Colors::black;
}

//--------------------------------------------------------------
void Staff::setID(int id)
{
	objID = id;
}

//--------------------------------------------------------------
void Staff::setGroup(int id)
{
	groupID = id;
}

//--------------------------------------------------------------
void Staff::setClef(int bar, int clefIdx)
{
	clefIndex[bar] = clefIdx;
}

//--------------------------------------------------------------
int Staff::getClef(int bar)
{
	return clefIndex[bar];
}

//--------------------------------------------------------------
void Staff::setRhythm(bool isRhythm)
{
	rhythm = isRhythm;
}

//--------------------------------------------------------------
void Staff::setSize(int fontSize, float staffLinesDist)
{
	Staff::fontSize = fontSize;
	notationFont = cv().loadFont("fonts/sonata.ttf", fontSize);
	BPMDisplayFont = cv().loadFont("fonts/LiberationSerif-Regular.ttf", fontSize*0.6);
	staffDist = staffLinesDist;
	noteWidth = notationFont->width(BPMDisplayNotes[2]);
	clefLength = std::max(notationFont->width(clefSyms[0]), notationFont->width(clefSyms[1]));
	clefLength = std::max(clefLength, notationFont->width(clefSyms[2]));
	meterLength = notationFont->width("64");
}

//--------------------------------------------------------------
void Staff::setMeter(int bar, int numer, int denom)
{
	numerator[bar] = numer;
	denominator[bar] = denom;
	if ((numerator[bar] == 4) && (denominator[bar] == 4)) meterIndex[bar] = 0;
	else if ((numerator[bar] == 2) && (denominator[bar] == 2)) meterIndex[bar] = 1;
	else meterIndex[bar] = 2;
}

//--------------------------------------------------------------
std::pair<int, int> Staff::getMeter(int bar)
{
	return std::make_pair(numerator[bar], denominator[bar]);
}

//--------------------------------------------------------------
void Staff::setTempo(int bar, int BPMTempo, int beatAtValue, bool hasDot)
{
	BPMTempi[bar] = BPMTempo;
	tempoBase[bar] = log(beatAtValue) / log(2);
	BPMDisplayDots[bar] = hasDot;
}

//--------------------------------------------------------------
void Staff::setOrientation(int orientation)
{
	scoreOrientation = orientation;
}

//--------------------------------------------------------------
void Staff::setNumBarsToDisplay(int numBars)
{
	// in horizontal view, the staff length is set for std::min(2, numBars) bars in ofApp::setScoreCoords()
	// so this coefficient scales it to the actual number of bars
	// (with one bar the length is already the full width, so the coefficient must be 1, not 2)
	if (numBars < 1) numBars = 1;
	xCoef = (float)std::min(2, numBars) / (float)numBars;
}

//--------------------------------------------------------------
float Staff::getXCoef()
{
	// getXLength() scales the staff for any orientation other than vertical (horizontal and full screen)
	// so the notes must be scaled the same way, otherwise they don't fit their staff
	return (scoreOrientation > 0 ? xCoef : (float)1.0);
}

//--------------------------------------------------------------
float Staff::getXStartOffset()
{
	return xStartOffset;
}

//--------------------------------------------------------------
void Staff::setCoords(float xLen, float dist)
{
	xLength = xLen;
	yDist = dist;
}

//--------------------------------------------------------------
float Staff::getXLength()
{
	// separate the clef and meter offsets because we place them closer together
	// but we don't change their font size, so we have to compansate for this
	// unchanged dimension combined with the the other changed lengths
	float len = xLength; // - getClefXOffset() - getMeterXOffset(); // - clefLength - meterLength;
	if (scoreOrientation > 0) len *= xCoef;
	return len; // + getClefXOffset() + getMeterXOffset() + clefLength + meterLength;
}

//--------------------------------------------------------------
float Staff::getClefXOffset()
{
	float y = yDist / 2;
	if (scoreOrientation > 0) y *= xCoef;
	return y + clefLength;
}

//--------------------------------------------------------------
float Staff::getMeterXOffset()
{
	float y = yDist / 2;
	if (scoreOrientation > 0) y *= xCoef;
	return y + meterLength;
}

//--------------------------------------------------------------
void Staff::moveScoreX(int numPixels)
{
	xStartOffset += (float)numPixels;
}

//--------------------------------------------------------------
void Staff::moveScoreY(int numPixels)
{
	yStartOffset += (float)numPixels;
}

//--------------------------------------------------------------
void Staff::recenterScore()
{
	xStartOffset = yStartOffset = 0;
}

//--------------------------------------------------------------
void Staff::drawStaff(int bar, float xStartPnt, float yStartPnt, float yOffset, bool drawClef,
		bool drawMeter, bool drawLoopStartEnd, bool drawTempo, int barOfLoop, int loopLength)
{
	cv().setColor(atBrightness(staffColor, cv().brightness));
	float xCoefLocal = xCoef;
	if (scoreOrientation > 0) xCoefLocal = 1;
	float xStartLocal = xStartPnt;
	xStartLocal += xStartOffset;
	float xEndLocal = getXLength() + xStartPnt;
	//if (!drawClef) xEndLocal -= getClefXOffset();
	//else if (scoreOrientation > 0) xEndLocal += getClefXOffset();
	//if (!(drawMeter && drawMeterInsideBar)) xEndLocal -= getMeterXOffset();
	//else if (scoreOrientation > 0) xEndLocal += getMeterXOffset();
	// the vertical lines at the sides of the staff go from its top line to its bottom line
	// yStartOffset (set with \movey) is not applied here, as it is not applied to the staff lines and the notes either
	// so applying it only to the top of these lines would make them longer or shorter than the staff
	float yStart = yStartPnt - (lineWidth / 2) + yOffset;
	float yEnd = (yStartPnt + (4 * yDist)) + (lineWidth / 2) + yOffset;
	cv().line(xStartLocal, yStart, xStartLocal, yEnd);
	cv().line(xEndLocal, yStart, xEndLocal, yEnd);
	if (rhythm) {
		float yPntLocal = yStartPnt + (2*yDist) + yOffset;
		cv().line(xStartLocal, yPntLocal, xEndLocal, yPntLocal);
	}
	else {
		for (int i = 0; i < 5; i++) {
			float yPntLocal = yStartPnt + (i*yDist) + yOffset;
			cv().line(xStartLocal, yPntLocal, xEndLocal, yPntLocal);
		}
	}
	if (drawClef) {
		if (clefIndex[bar] < 3) {
			notationFont->draw(clefSyms[clefIndex[bar]], xStartLocal+((yDist/2)*xCoefLocal),
									yStartPnt+(yDist*3.8)+yOffset);
		}
		else {
			float xPos = ((yDist * 1.5) * xCoefLocal) + xStartLocal;
			float yPos = yStartPnt + yDist + yOffset;
			for (int i = 0; i < 2; i++) {
				drawThickLine(xPos+(i*yDist), yPos, xPos+(i*yDist), yPos+(yDist*2), yDist*0.25);
			}
		}
	}
	if (drawMeter) {
		float xPos = xStartLocal + ((yDist / 2) * xCoefLocal);
		float numeratorY = yStartPnt + yDist + yOffset;
		float denominatorY = numeratorY + (yDist*2);
		if (drawClef) xPos += getClefXOffset();
		if (meterIndex[bar] < 2) {
			notationFont->draw(meterSyms[meterIndex[bar]], xPos, numeratorY+yDist);
		}
		else {
			notationFont->draw(std::to_string(numerator[bar]), xPos, numeratorY);
			notationFont->draw(std::to_string(denominator[bar]), xPos, denominatorY);
		}
	}
	if (drawLoopStartEnd && barOfLoop < 0) {
		if (isLoopStart) drawLoopStart(bar, xStartLocal, yStartPnt, yOffset);
		if (isLoopEnd) drawLoopEnd(bar, xEndLocal, yStartPnt, yOffset);
		if (isScoreEnd) drawScoreEnd(bar, xEndLocal, yStartPnt, yOffset);
	}
	else if (drawLoopStartEnd) {
		if (barOfLoop == 0) drawLoopStart(bar, xStartLocal, yStartPnt, yOffset);
		if (barOfLoop == loopLength - 1) {
			if (isScoreEnd) drawScoreEnd(bar, xEndLocal, yStartPnt, yOffset);
			else drawLoopEnd(bar, xEndLocal, yStartPnt, yOffset);
		}
	}
	if (drawTempo && groupID == -1) {
		std::string tempoBaseString = BPMDisplayNotes[tempoBase[bar]];
		float offset = 0;
		// the following line is taken from the clef drawing chunk above
		float y = yStartPnt+(yDist*3.8)+yOffset;
		y -= notationFont->height(clefSyms[0]);
		if (BPMDisplayDots[bar]) {
			cv().circle(xStartPnt+(noteWidth*1.5), y, staffDist*0.2);
			offset = noteWidth * 0.5;
		}
		std::string BPMString = " = " + std::to_string(BPMTempi[bar]);
		notationFont->draw(tempoBaseString, xStartPnt, y);
		BPMDisplayFont->draw(BPMString, xStartPnt+offset+noteWidth, y);
	}
}

//--------------------------------------------------------------
void Staff::drawLoopStart(int bar, float xStart, float yStartPnt, float yOffset)
{
	// the " + (yDist*3.5)" is taken from drawStaff() where the meter is drawn
	float xPos = xStart + (yDist*3.5);
	if (meterIndex[bar] < 2) {
		xPos += notationFont->width(meterSyms[meterIndex[bar]]);
	}
	else {
		float maxWidth = std::max(notationFont->width(std::to_string(numerator[bar])),
							 notationFont->width(std::to_string(denominator[bar])));
		xPos += maxWidth;
	}
	// a little bit of offset so that symbols are not clamped together
	xPos += (yDist * 0.5);
	cv().line(xPos, yStartPnt+yOffset, xPos, yStartPnt+(yDist*4)+yOffset);
	cv().line(xPos+(yDist*0.5), yStartPnt+yOffset, xPos+(yDist*0.5), yStartPnt+(yDist*4)+yOffset);
	cv().circle(xPos+yDist, yStartPnt+(yDist*1.5)+yOffset, yDist*0.2);
	cv().circle(xPos+yDist, yStartPnt+(yDist*2.5)+yOffset, yDist*0.2);
}

//--------------------------------------------------------------
void Staff::drawLoopEnd(int bar, float xEnd, float yStartPnt, float yOffset)
{
	cv().line(xEnd-(yDist*0.5), yStartPnt+yOffset,
			   xEnd-(yDist*0.5), yStartPnt+(yDist*4)+yOffset);
	cv().circle(xEnd-yDist, yStartPnt+(yDist*1.5)+yOffset, yDist*0.2);
	cv().circle(xEnd-yDist, yStartPnt+(yDist*2.5)+yOffset, yDist*0.2);
}

//--------------------------------------------------------------
void Staff::drawScoreEnd(int bar, float xEnd, float yStartPnt, float yOffset)
{
	cv().line(xEnd-(yDist*0.75), yStartPnt+yOffset,
			   xEnd-(yDist*0.75), yStartPnt+(yDist*4)+yOffset);
	float x1 = xEnd-(yDist*0.125); // multiply by half of the value in the m.addVertex lines
	float y1 = yStartPnt-((int)cv().lineWidth/2)+yOffset;
	float x2 = xEnd-(yDist*0.125);
	float y2 = yStartPnt+(yDist*4)+((int)cv().lineWidth/2)+yOffset;
	drawThickLine(x1, y1, x2, y2, yDist*0.25);
}

//--------------------------------------------------------------
void Staff::drawThickLine(float x1, float y1, float x2, float y2, float width)
{
	// draw a thick line without affecting the thickness
	// of all the other lines
	// taken from
	// https://forum.openframeworks.cc/t/how-do-i-draw-lines-of-a-
	// reasonable-thickness-on-a-very-large-canvas-given-opengl-
	// constraints/30815/7
	cv().thickLine(x1, y1, x2, y2, width);
}

/************************* Notes class ************************/
//--------------------------------------------------------------
Notes::Notes()
{
	clefIndex[0] = 0;
	beamsLineWidth = 2;
	//beats = numBeats;
	animate = false;
	mute = false;
	whichNote = -1;
	xStartOffset = 0;
	yStartOffset = 0;
	lineWidth = (int)cv().lineWidth;
	//quarterSharp.load("quarter_sharp.png");
	fontSize = 0;
	rhythm = false;
	scoreOrientation = 0;
	scoreOrientationForDots = 0;
	xStartOffset = 0;
	accidentalsOffsetCoef = 1.2;
	notesColor = Colors::black;
	activeNotesColor = Colors::red;
}

//--------------------------------------------------------------
void Notes::setID(int id)
{
	objID = id;
}

//--------------------------------------------------------------
float Notes::getDist(int bar)
{
	return distBetweenBeats[bar];
}

//--------------------------------------------------------------
void Notes::setRhythm(bool isRhythm)
{
	rhythm = isRhythm;
}

//--------------------------------------------------------------
void Notes::setFontSize(int fontSize, float staffLinesDist)
{
	if (Notes::fontSize != fontSize) {
		Notes::fontSize = fontSize;
		notationFont = cv().loadFont("fonts/sonata.ttf", fontSize);
		textFont = cv().loadFont("fonts/LiberationSerif-Regular.ttf", fontSize/2);
		noteWidth = notationFont->width(notesSyms[2]);
		noteHeight = notationFont->height(notesSyms[2]);
		for (int i = 0; i < 8; i++) {
			dynSymsWidths[i] = notationFont->width(dynSyms[i]);
			dynSymsHeights[i] = notationFont->height(dynSyms[i]);
		}
		for (int i = 0; i < 6; i++) {
			restsSymsWidths[i] = notationFont->width(restsSyms[i]);
			restsSymsHeights[i] = notationFont->height(restsSyms[i]);
		}
		staffDist = staffLinesDist;
		halfStaffDist = staffDist / 2.0;
	}
}

//--------------------------------------------------------------
void Notes::setCoords(float xLen, float staffLinesDist, int fontSize)
{
	xLength = xLen;
	staffDist = staffLinesDist;
	halfStaffDist = staffDist / 2;
	stemHeight = (int)((float)staffDist * 3.5);
	middleOfStaff = halfStaffDist * 4; // yStartPnt + (halfStaffDist * 4);
	// minimum offset must be 3
	restYOffset = 3;
	int i = 35; // 10 is the minimum staff distance, so 35 the minimum font size
	while (i <= fontSize) {
		i += 5; // every five pixels we increment restYOffset by one
		restYOffset++;
	}
	// then check if the actual font size is closer to one step below
	// of what i has incremented to (e.g. i is 40 but fontSize is 37)
	int diff = i - fontSize;
	if (diff > 2) restYOffset--;
}

//--------------------------------------------------------------
float Notes::getMaxYPos(int bar)
{
	return maxYCoord[bar]; // + yStartPnt[scoreOrientation];
}

//--------------------------------------------------------------
float Notes::getMinYPos(int bar)
{
	return minYCoord[bar]; // + yStartPnt[scoreOrientation];
}

//--------------------------------------------------------------
float Notes::getMinVsAnchor(int bar)
{
	return minYCoord[bar];
}

//--------------------------------------------------------------
float Notes::getNoteWidth()
{
	return notationFont->width(notesSyms[0]);
}

//--------------------------------------------------------------
float Notes::getNoteHeight()
{
	return notationFont->height(notesSyms[0]);
}

//--------------------------------------------------------------
void Notes::setClef(int bar, int clefIdx)
{
	clefIndex[bar] = clefIdx;
}

//--------------------------------------------------------------
void Notes::setOrientation(int orientation)
{
	scoreOrientation = orientation;
}

//--------------------------------------------------------------
void Notes::setOrientationForDots(int orientation)
{
	scoreOrientationForDots = orientation;
}

//--------------------------------------------------------------
void Notes::setAnimation(bool anim)
{
	animate = anim;
}

//--------------------------------------------------------------
void Notes::setActiveNote(int note)
{
	whichNote = note;
}

//--------------------------------------------------------------
void Notes::setMute(bool muteState)
{
	mute = muteState;
}

//--------------------------------------------------------------
void Notes::setVoice(int voice, int numVoices)
{
	voiceNum = std::max(1, voice);
	forcedStemDir = (numVoices > 1 ? (voiceNum % 2 == 1 ? 1 : -1) : 0);
}

//--------------------------------------------------------------
std::vector<Notes::NoteColumn> Notes::getColumns(int bar)
{
	std::vector<NoteColumn> columns;
	auto notesIt = allNotes.find(bar);
	if (notesIt == allNotes.end() || noteXCoords.find(bar) == noteXCoords.end()) return columns;
	for (size_t i = 0; i < notesIt->second.size(); i++) {
		NoteColumn column;
		column.x = (i < noteXCoords[bar].size() ? noteXCoords[bar][i] : 0);
		column.rest = (notesIt->second[i].empty() || notesIt->second[i][0] < 0);
		if (i < allNoteHeadCoordsY[bar].size()) column.headsY = allNoteHeadCoordsY[bar][i];
		int dur = (i < actualDurs[bar].size() ? actualDurs[bar][i] : 4);
		column.headType = (dur <= 1 ? 1 : (dur == 2 ? 2 : 4));
		column.dots = (i < dotsCounter[bar].size() ? dotsCounter[bar][i] : 0);
		columns.push_back(column);
	}
	return columns;
}

//--------------------------------------------------------------
void Notes::shiftColumn(int bar, int noteNdx, float dx)
{
	auto it = allNoteCoordsX.find(bar);
	if (it == allNoteCoordsX.end() || noteNdx < 0 || noteNdx >= (int)it->second.size()) return;
	for (float& x : it->second[noteNdx]) x += dx;
}

//--------------------------------------------------------------
void Notes::setMeter(int bar, int numer, int denom, int numBeats)
{
	numerator[bar] = numer;
	denominator[bar] = denom;
	beats[bar] = numBeats;
	distBetweenBeats[bar] = xLength / numBeats;
}

//--------------------------------------------------------------
void Notes::setAccidentalsOffsetCoef(float coef)
{
	accidentalsOffsetCoef = coef;
}

//--------------------------------------------------------------
void Notes::setNotes(int bar,
		std::vector<std::vector<int>> notes,
		std::vector<std::vector<int>> accidentals,
		std::vector<std::vector<int>> naturalsNotWritten,
		std::vector<std::vector<int>> octaves,
		std::vector<int> ottavas,
		std::vector<int> durs,
		std::vector<int> dotNdxs,
		std::vector<unsigned> dotsCntr,
		std::vector<int> gliss,
		std::vector<std::vector<int>> articul,
		std::vector<int> dyns,
		std::vector<int> dynIdx,
		std::vector<int> dynRampStart,
		std::vector<int> dynRampEnd,
		std::vector<int> dynRampDir,
		std::vector<std::pair<int, int>> slurNdxs,
		std::vector<int> tieNdxs,
		bool isWholeBarSlurred,
		std::map<int, std::pair<int, int>> tupletRatios,
		std::map<int, std::pair<unsigned, unsigned>> tupletStartStop,
		std::vector<std::string> texts,
		std::vector<std::vector<int>> textIndexes,
		int BPMMult)
{
	// if we have copied the bar by writing a bar name instead of an actual melodic line
	// then we don't need to pass any of the data or make any calculations
	if (allNotes.find(bar) != allNotes.end()) return;
	allBars.push_back(bar);
	allNotes[bar] = std::move(notes);
	allAccidentals[bar] = std::move(accidentals);
	naturalSignsNotWritten[bar] = std::move(naturalsNotWritten);
	allOctaves[bar] = std::move(octaves);
	allOttavas[bar] = std::move(ottavas);
	durations[bar] = std::move(durs);
	dotIndexes[bar] = std::move(dotNdxs);
	dotsCounter[bar] = std::move(dotsCntr);
	allGlissandi[bar] = std::move(gliss);
	allArticulations[bar] = std::move(articul);
	dynamics[bar] = std::move(dyns);
	dynamicsIndexes[bar] = std::move(dynIdx);
	dynamicsRampStart[bar] = std::move(dynRampStart);
	dynamicsRampEnd[bar] = std::move(dynRampEnd);
	dynamicsRampDir[bar] = std::move(dynRampDir);
	slurIndexes[bar] = std::move(slurNdxs);
	tieIndexes[bar] = std::move(tieNdxs);
	isWholeSlurred[bar] = std::move(isWholeBarSlurred);
	tupRatios[bar] = std::move(tupletRatios);
	tupStartStop[bar] = std::move(tupletStartStop);
	allTexts[bar] = std::move(texts);
	allTextsIndexes[bar] = std::move(textIndexes);
	maxYCoord[bar] = FLT_MIN;
	minYCoord[bar] = FLT_MAX;
	isLinked[bar] = std::make_pair(0, 0);
	BPMMultiplier[bar] = BPMMult;
	if (clefIndex[bar] != 0) changeNotesBasedOnClef(bar);
}

//--------------------------------------------------------------
void Notes::changeNotesBasedOnClef(int bar)
{
	// if we have a different clef we must offset the notes
	if (clefIndex[bar] == 1) {
		bool tacetLocal = false;
		for (unsigned i = 0; i < allNotes[bar].size(); i++) {
			if (!((allNotes[bar].size() == 1) && (allNotes[bar][i][0] == -1))) {
				for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
					if (allNotes[bar][i][j] > -1) {
						allNotes[bar][i][j] -= 2;
						if (allNotes[bar][i][j] < 0) {
							allNotes[bar][i][j] = 7 + allNotes[bar][i][j];
							allOctaves[bar][i][j]--;
						}
					}
				}
			}
			else {
				tacetLocal = true;
			}
		}
		if (!tacetLocal) {
			for (unsigned i = 0; i < allOctaves[bar].size(); i++) {
				for (unsigned j = 0; j < allOctaves[bar][i].size(); j++) {
					allOctaves[bar][i][j] += 2;
				}
			}
		}
	}
}

//--------------------------------------------------------------
void Notes::correctDynamics(int bar, std::vector<int> dyns)
{
	if (dynamics[bar].size() != dyns.size()) {
		std::cout << "vectors don't have the same size\n";
		return;
	}
	for (unsigned i = 0; i < dynamics[bar].size(); i++) {
		dynamics[bar][i] = dyns[i];
	}
}

//--------------------------------------------------------------
void Notes::setNotePositions(int bar)
{
	std::vector<std::vector<int>> vvi;
	for (auto vvit = allNotes[bar].begin(); vvit != allNotes[bar].end(); ++vvit) {
		vvi.push_back(std::vector<int>());
		for (auto vit = vvit->begin(); vit != vvit->end(); ++vit) {
			vvi.back().push_back(0);
		}
	}
	std::vector<std::vector<float>> vvf;
	for (auto vvit = allNotes[bar].begin(); vvit != allNotes[bar].end(); ++vvit) {
		vvf.push_back(std::vector<float>());
		for (auto vit = vvit->begin(); vit != vvit->end(); ++vit) {
			vvf.back().push_back(0);
		}
	}
	// special 2D vector for Y coords of articulations
	std::vector<std::vector<float>> vvf2;
	for (auto vvit = allArticulations[bar].begin(); vvit != allArticulations[bar].end(); ++vvit) {
		vvf2.push_back(std::vector<float>());
		for (auto vit = vvit->begin(); vit != vvit->end(); ++vit) {
			vvf2.back().push_back(FLT_MIN);
		}
	}
	// special 2D vector for shifting notes on the X axis, in case of clamped chords
	std::vector<std::vector<bool>> vvb;
	for (auto vvit = allNotes[bar].begin(); vvit != allNotes[bar].end(); ++vvit) {
		vvb.push_back(std::vector<bool>());
		for (auto vit = vvit->begin(); vit != vvit->end(); ++vit) {
			vvb.back().push_back(false);
		}
	}
	std::vector<int> vi1(allNotes[bar].size(), 1);
	std::vector<int> vi2(allNotes[bar].size(), 0);
	std::vector<float> vf1(allNotes[bar].size(), 0);
	std::vector<float> vf2(allNotes[bar].size(), FLT_MIN);
	std::vector<float> vf3(allNotes[bar].size(), staffDist*4);
	stemDirections[bar] = vi1;
	articulXPos[bar] = vf2;
	articulYPos[bar] = vvf2;
	actualDurs[bar] = vi2;
	hasStem[bar] = vi2;
	numTails[bar] = vi2;
	numBeams[bar] = vi2;
	allNoteCoordsX[bar] = vvf;
	allNoteShiftX[bar] = vvb;
	allNoteCoordsXOffset[bar] = vf1;
	allChordsChangeXCoef[bar] = vf1;
	allNoteHeadCoordsY[bar] = vvf;
	allNoteStemCoordsY[bar] = vf1;
	extraLinesDir[bar] = vvi;
	extraLinesYPos[bar] = vvi;
	numExtraLines[bar] = vvi;
	hasExtraLines[bar] = vvi;
	allNotesMaxYPos[bar] = vf1;
	allNotesMinYPos[bar] = vf3;
	numOctavesOffset[bar] = vi2;
	allChordsBaseIndexes[bar] = vi2;
	allChordsEdgeIndexes[bar] = vi2;
	isNoteGroupped[bar] = vi2;
	noteXCoords[bar] = vf1;

	float x = 0;
	int beamsCounter = 0;
	bool fullBeat = false;
	bool addedNotesAtFullBeat = false;
	std::vector<bool> fullBeats(allNotes[bar].size(), false);
	unsigned prevFullBeatNdx = 0;
	float durAccum = 0;
	tacet[bar] = false;
	beamIndexCounter = 0;
	beamIndexCounter2 = 0;
	std::vector<int> numNotesEveryFullBeat(allNotes[bar].size());
	int numNotesFullBeatNdx = 0;
	int nonBeamNumNotes = 0;
	bool noteIsInTuplet = false;
	int tupStopIndex = -1;
	// cast to int to avoid cast further down in if tests
	for (int i = 0; i < (int)allNotes[bar].size(); i++) {
		std::vector<float> yVec(allNotes[bar][i].size());
		if ((allNotes[bar].size() == 1) && (allNotes[bar][i][0] == -1)) {
			tacet[bar] = true;
			minYCoord[bar] = 0;
			maxYCoord[bar] = minYCoord[bar] + (staffDist * 4);
		}
		// get the duration in musical terms, 1 for whole note
		// 2 for half note, 4 for quarter, etc.
		int duration = durations[bar][i] * ((float)numerator[bar] / (float)denominator[bar]);
		// dotted notes are drawn as the note without the dots (e.g. a dotted eighth is an eighth with a dot)
		// so the dots are taken out of the duration, otherwise a dotted eighth would be a fifth of a bar in 4/4
		// which is not beamed, and a dotted half would be drawn as a whole note
		float undottedDuration = (float)duration;
		int numDots = (dotIndexes[bar][i] > 0 ? (int)dotsCounter[bar][i] : 0);
		if (numDots > 0) undottedDuration = (float)duration * (float)(1 << numDots) / (float)((1 << (numDots + 1)) - 1);
		actualDurs[bar][i] = (int)(((float)beats[bar] / undottedDuration) + 0.5);
		// detect the beats based on the denominator
		// so eights and shorter notes are groupped properly
		float thisDur = 0; // initialize to avoid compiler warnings
		int durInBeats = durations[bar][i];
		int whichNdx;
		for (int j = 0; j < 7; j++) {
			if (durInBeats <= baseDurations[j]) {
				if (durInBeats == baseDurations[j]) whichNdx = j;
				else whichNdx = j-1;
				thisDur = (float)denominator[bar] / (float)(MINDUR / baseDurations[whichNdx]);
				int subtractCounter = 1;
				while (durInBeats != baseDurations[whichNdx]) {
					durInBeats -= baseDurations[whichNdx-subtractCounter];
					thisDur += (float)denominator[bar] / (float)(MINDUR / baseDurations[whichNdx-subtractCounter]);
					subtractCounter++;
				}
				break;
			}
		}
		if (BPMMultiplier[bar] > 1) thisDur /= (float)BPMMultiplier[bar];
		// check if we're inside tuplets, which can be nested, so the duration is multiplied by the ratio of every tuplet the note is in
		// a beat is complete at the end of the outermost tuplet
		for (auto it = tupStartStop[bar].begin(); it != tupStartStop[bar].end(); ++it) {
			if (i >= (int)it->second.first && i <= (int)it->second.second) {
				thisDur = thisDur * tupRatios[bar][it->first].second / tupRatios[bar][it->first].first;
				if (!noteIsInTuplet || (int)it->second.second > (int)tupStopIndex) tupStopIndex = it->second.second;
				noteIsInTuplet = true;
			}
		}
		durAccum += thisDur;
		// if we get a whole beat or more
		if (durAccum >= 1.0) {
			fullBeat = true;
			if (noteIsInTuplet) {
				if (i == tupStopIndex) {
					fullBeats[i] = true;
					noteIsInTuplet = false;
				}
			}
			else fullBeats[i] = true;
			durAccum -= (int)durAccum;
		}
		if (actualDurs[bar][i] >= 2) {
			// checking the first note, even in case of a chord, is enough
			// to determine whether we have a rest or not
			if (allNotes[bar][i][0] > -1) {
				hasStem[bar][i] = 1;
			}
		}
		if (i > 0) {
			float thisDistBetweenBeats = durations[bar][i-1] * distBetweenBeats[bar];
			for (auto it = tupStartStop[bar].begin(); it != tupStartStop[bar].end(); ++it) {
				// since we add the distance from the previous note to the current one
				// we need to test the previous index, hence i-1
				if (i-1 >= (int)it->second.first && i-1 <= (int)it->second.second) {
					thisDistBetweenBeats = (thisDistBetweenBeats / tupRatios[bar][it->first].first) * tupRatios[bar][it->first].second;
				}
			}
			x += thisDistBetweenBeats;
		}
		else if (tacet[bar]) {
			x = xLength / 2;
		}
		noteXCoords[bar][i] = x;
		// accumulate Ys to determine stem direction
		int stemDirAccum = 0;
		float stemDirAccumDiff = 0;
		for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
			// rests are drawn in the middle of the staff (see drawRest()), so that's their Y coord
			// otherwise their note value (-1) and octave would place them below the staff
			// and make the staff look like it extends far below it
			if (allNotes[bar][i][j] < 0) {
				// the rests of voices move up (stems up) or down (stems down), so they don't collide with the other voice
				yVec[j] = middleOfStaff - (forcedStemDir * staffDist);
				continue;
			}
			// set y as the C one octave below middle C
			yVec[j] = halfStaffDist * 17;
			float noteYOffset = (allNotes[bar][i][j] * halfStaffDist) + \
								(allOctaves[bar][i][j] * 7 * halfStaffDist);
			yVec[j] -= noteYOffset;
		}
		// run the loop a second time, since the octave offsets have now been updated
		for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
			allNoteHeadCoordsY[bar][i][j] = allNoteStemCoordsY[bar][i] = yVec[j];
			// initialize all X coords to x
			allNoteCoordsX[bar][i][j] = x;
			if (yVec[j] > allNotesMaxYPos[bar][i] && yVec[j] < FLT_MAX) allNotesMaxYPos[bar][i] = yVec[j];
			if (yVec[j] < allNotesMinYPos[bar][i] && yVec[j] > -FLT_MAX) allNotesMinYPos[bar][i] = yVec[j];
			// test position of note against the middle of the staff to determine the direction of the stem
			if (abs(yVec[j] - middleOfStaff) > stemDirAccumDiff) {
				if (yVec[j] < middleOfStaff) stemDirAccum = -1;
				else stemDirAccum = 1;
				stemDirAccumDiff = abs(yVec[j] - middleOfStaff);
			}
			// we take 0 to be the F on the top line of the staff
			// so we need to subtract half the distance of the staff lines
			// times 9 (from top F to middle C)
			if (yVec[j] > (halfStaffDist * 9)) {
				if (allNotes[bar][i][j] > -1) {
					extraLinesDir[bar][i][j] = 1;
				}
			}
			// or add this once, to see if we're at A above the staff or higher
			else if (yVec[j] < halfStaffDist) {
				if (allNotes[bar][i][j] > -1) {
					extraLinesDir[bar][i][j] = -1;
				}
			}
			if (extraLinesDir[bar][i][j] != 0) {
				float yPos;
				if (extraLinesDir[bar][i][j] > 0) {
					numExtraLines[bar][i][j] = (int)((yVec[j] - (staffDist * 4)) / staffDist);
					yPos = staffDist * 4;
				}
				else {
					numExtraLines[bar][i][j] = (int)(yVec[j] / staffDist);
					yPos = 0;
				}
				extraLinesYPos[bar][i][j] = yPos;
				hasExtraLines[bar][i][j] = 1;
			}
		}
		// the stems of a voice of a staff with merged instruments have the direction of the voice (see setVoice())
		if (forcedStemDir != 0) stemDirections[bar][i] = forcedStemDir;
		else if (stemDirAccum >= 0) stemDirections[bar][i] = 1;
		// stems can point down only if this is not a rhythm staff
		else if (!rhythm) stemDirections[bar][i] = -1;
		// set the base and edge notes of the chord, and the X coords of its notes
		layoutChordNotes(bar, i, x, stemDirections[bar][i]);
		// set the Y coord to be the lowest note (highest Y value)
		// in case of stem up, otherwise the other way round
		float y = (halfStaffDist * 17);
		float yEdge = y;
		float noteYOffsetBase = (allNotes[bar][i][allChordsBaseIndexes[bar][i]] * halfStaffDist) + \
								(allOctaves[bar][i][allChordsBaseIndexes[bar][i]] * 7 * halfStaffDist);
		float noteYOffsetEdge = (allNotes[bar][i][allChordsEdgeIndexes[bar][i]] * halfStaffDist) + \
								(allOctaves[bar][i][allChordsEdgeIndexes[bar][i]] * 7 * halfStaffDist);
		y -= noteYOffsetBase;
		yEdge -= noteYOffsetEdge;
		// rests are in the middle of the staff, as above
		if (allNotes[bar][i][0] < 0) y = yEdge = middleOfStaff;
		if (y > allNotesMaxYPos[bar][i] && y < FLT_MAX) allNotesMaxYPos[bar][i] = y;
		if (y < allNotesMinYPos[bar][i] && y > -FLT_MAX) allNotesMinYPos[bar][i] = y;
		// caclulate the coords of the stems
		if (hasStem[bar][i]) {
			allNoteStemCoordsY[bar][i] = allNoteHeadCoordsY[bar][i][allChordsEdgeIndexes[bar][i]] + (noteHeight / 8.0) - (staffDist / 10.0) - ((stemHeight + halfStaffDist) * stemDirections[bar][i]);
			// the stem of a note with ledger lines must reach the middle line of the staff
			// a stem up (direction 1) ends above the note head (lower Y), a stem down (direction -1) below it
			// (the stems of voices keep their length, as the stems of the other voice are on the other side)
			if (!rhythm && forcedStemDir == 0) {
				if (stemDirections[bar][i] == 1 && allNoteStemCoordsY[bar][i] > middleOfStaff) allNoteStemCoordsY[bar][i] = middleOfStaff;
				else if (stemDirections[bar][i] == -1 && allNoteStemCoordsY[bar][i] < middleOfStaff) allNoteStemCoordsY[bar][i] = middleOfStaff;
			}
			if (allNoteStemCoordsY[bar][i] > allNotesMaxYPos[bar][i] && allNoteStemCoordsY[bar][i] < FLT_MAX) allNotesMaxYPos[bar][i] = allNoteStemCoordsY[bar][i];
			if (allNoteStemCoordsY[bar][i] < allNotesMinYPos[bar][i] && allNoteStemCoordsY[bar][i] > -FLT_MAX) allNotesMinYPos[bar][i] = allNoteStemCoordsY[bar][i];
		}
		else if (actualDurs[bar][i] == 1) {
			allNoteStemCoordsY[bar][i] = y;
		}
		if (fullBeat) {
			for (int j = (int)prevFullBeatNdx; j <= i; j++) {
				if (actualDurs[bar][j] > 4) {
					numNotesEveryFullBeat[numNotesFullBeatNdx]++;
					addedNotesAtFullBeat = true;
					nonBeamNumNotes = j + 1;
				}
				else {
					nonBeamNumNotes++;
				}
			}
			fullBeat = false;
			prevFullBeatNdx = i + 1;
			if (addedNotesAtFullBeat) numNotesFullBeatNdx++;
			addedNotesAtFullBeat = false;
		}
	}
	beamsCounter = 0;
	prevFullBeatNdx = 0;
	unsigned i;
	int j, prevNdx;
	std::vector<std::vector<int>> beamsIndexesLocal;
	for (i = 0; i < allNotes[bar].size(); i++) {
		if (fullBeats[i]) {
			std::vector<int> v;
			beamsIndexesLocal.push_back(v);
			beamsCounter = 0;
			for (j = prevFullBeatNdx; j <= (int)i; j++) {
				if (actualDurs[bar][j] > 4) { // && allNotes[bar][i][0] > -1) {
					beamsCounter++;
					beamsIndexesLocal.back().push_back(j);
				}
			}
			if (beamsIndexesLocal.back().size() > 0) {
				// check consecutive indexes to group notes together
				prevNdx = beamsIndexesLocal.back()[0];
				for (j = 1; j < beamsCounter; j++) {
					if (beamsIndexesLocal.back()[j] - prevNdx > 1) {
						// here the std::vectors must break, so we store the remaining of the std::vector to a new one
						std::vector<int> v2(beamsIndexesLocal.back().begin() + j, beamsIndexesLocal.back().end());
						// erase this portion from the existing one
						beamsIndexesLocal.back().erase(beamsIndexesLocal.back().begin() + j, beamsIndexesLocal.back().end());
						// and push the copied portion as a new std::vector
						beamsIndexesLocal.push_back(v2);
						beamsCounter -= j;
						j = 1;
						continue; // start over
					}
					prevNdx = beamsIndexesLocal.back()[j];
				}
			}
			// erase single-element or empty std::vectors
			for (auto it = beamsIndexesLocal.rbegin(); it != beamsIndexesLocal.rend(); ++it) {
				if (it->size() < 2) beamsIndexesLocal.erase(std::next(it).base());
			}
			prevFullBeatNdx = i + 1;
		}
	}
	// when done, keep only the first and last index of each std::vector in std::vector
	for (i = 0; i < beamsIndexesLocal.size(); i++) {
		if (beamsIndexesLocal[i].size() > 2) {
			beamsIndexesLocal[i].erase(beamsIndexesLocal[i].begin()+1, beamsIndexesLocal[i].end()-1);
		}
	}
	// set which note indexes are beamed
	// this is used in drawNotes() to determine whether we'll draw an individual tail or not
	int nonRestsCounter;
	// also set which beam groups have rests so we make sure we don't draw on top of them
	std::vector<int> beamGroupHasRests(beamsIndexesLocal.size(), 0);
	for (i = 0; i < beamsIndexesLocal.size(); i++) {
		nonRestsCounter = 0;
		for (j = beamsIndexesLocal[i][0]; j <= beamsIndexesLocal[i][1]; j++) {
			if (allNotes[bar][j][0] > -1) nonRestsCounter++;
			else beamGroupHasRests[i] = 1;
		}
		for (j = beamsIndexesLocal[i][0]; j <= beamsIndexesLocal[i][1]; j++) {
			if (nonRestsCounter > 1) {
				isNoteGroupped[bar][j] = 1;
			}
		}
	}
	beamsCounter = 0;
	prevFullBeatNdx = 0;
	int hasRestsLocal = 0;
	std::vector<int> hasRestsVecLocal;
	for (i = 0; i < allNotes[bar].size(); i++) {
		if (fullBeats[i]) {
			for (j = prevFullBeatNdx; j <= (int)i; j++) {
				if (actualDurs[bar][j] > 4) {
					if (allNotes[bar][j][0] < 0) {
						// we need to know if a group of connected notes has rests
						// so we can make sure the line that connects them is not
						// drawn on top of the rest
						if (actualDurs[bar][j] > hasRestsLocal) hasRestsLocal = actualDurs[bar][j];
					}
					hasRestsVecLocal.push_back(hasRestsLocal);
					hasRestsLocal = 0;
				}
			}
			prevFullBeatNdx = i + 1;
		}
	}
	prevFullBeatNdx = 0;
	int numLinesLocal = 0;
	std::vector<int> maxNumLinesLocal;
	for (i = 0; i < allNotes[bar].size(); i++) {
		if (fullBeats[i]) {
			for (j = prevFullBeatNdx; j <= (int)i; j++) {
				if (actualDurs[bar][j] > 4) {
					if (allNotes[bar][j][0] >= 0){
						// note the max number of beams as the variable stored in yLocal
						// won't be very useful if we calculate for one beam only
						// and end up with more, in which case we're likely to draw on top
						// of the rest anyway
						int numLines = (log(actualDurs[bar][j]) / log(2)) - 2;
						if (numLines > numLinesLocal) numLinesLocal = numLines;
					}
					maxNumLinesLocal.push_back(numLinesLocal);
					numLinesLocal = 0;
				}
			}
			prevFullBeatNdx = i + 1;
		}
	}
	// copy the local std::vectors to the std::maps of the class
	beamsIndexes[bar] = beamsIndexesLocal;
	hasRests[bar] = hasRestsVecLocal;
	maxNumLines[bar] = maxNumLinesLocal;
	grouppedStemDirs[bar] = std::vector<int>(beamsIndexes[bar].size());
	numNotesWithBeams[bar] = std::vector<int>(beamsIndexes[bar].size(), 0);
	// to easily separate notes with beams that are groupped from the ones that are not
	// (e.g. in case of a dotted quarter with an eighth note)
	// we first determine how many notes have beams in a group
	// and then we copy the beamIndexesLocal std::vector to beamsIndexes as a 2D std::vector
	if (beamsIndexes[bar].size() > 0) {
		for (i = 0; i < beamsIndexes[bar].size(); i++) {
			for (j = beamsIndexes[bar][i][0]; j <= beamsIndexes[bar][i][1]; j++) {
				if (allNotes[bar][j][0] > -1) {
					numNotesWithBeams[bar][i]++;
				}
			}
		}
	}
	// the code below calculates the coordinates of the beams
	if (beamsIndexes[bar].size() > 0) {
		for (i = 0; i < beamsIndexes[bar].size(); i++) {
			//float averageHeight = 0;
			int stemDir = 1;
			bool storedGroupStemDir = false;
			float stemDirDiff = 0;
			for (j = beamsIndexes[bar][i][0]; j <= beamsIndexes[bar][i][1]; j++) {
				if (allNotes[bar][j][0] > -1) {
					for (unsigned k = 0; k < allNotes[bar][j].size(); k++) {
						if (abs(allNoteHeadCoordsY[bar][j][k] - middleOfStaff) > stemDirDiff) {
							if (allNoteHeadCoordsY[bar][j][k] < middleOfStaff) stemDir = -1;
							else stemDir = 1;
							storedGroupStemDir = true;
							stemDirDiff = abs(allNoteHeadCoordsY[bar][j][k] - middleOfStaff);
						}
					}
				}
				else {
					if (j > beamsIndexes[bar][i][0]) {
						for (int k = j+1; k < beamsIndexes[bar][i][1]; k++) {
							if (allNotes[bar][k][0] > -1) {
								for (unsigned l = 0; l < allNotes[bar][k].size(); l++) {
									if (abs(allNoteHeadCoordsY[bar][k][l] - middleOfStaff) > stemDirDiff) {
										if (allNoteHeadCoordsY[bar][k][l] < middleOfStaff) stemDir = -1;
										else stemDir = 1;
										storedGroupStemDir = true;
										stemDirDiff = abs(allNoteHeadCoordsY[bar][k][l] - middleOfStaff);
									}
								}
								break;
							}
						}
					}
					else if (j < beamsIndexes[bar][i][1]) {
						for (int k = beamsIndexes[bar][i][1]; k >=0; k--) {
							if (allNotes[bar][k][0] > -1) {
								for (unsigned l = 0; l < allNotes[bar][k].size(); l++) {
									if (abs(allNoteHeadCoordsY[bar][k][l] - middleOfStaff) > stemDirDiff) {
										if (allNoteHeadCoordsY[bar][k][l] < middleOfStaff) stemDir = -1;
										else stemDir = 1;
										storedGroupStemDir = true;
										stemDirDiff = abs(allNoteHeadCoordsY[bar][k][l] - middleOfStaff);
									}
								}
								break;
							}
						}
					}
					else {
						for (unsigned k = 0; k < allNotes[bar][j].size(); k++) {
							if (abs(allNoteHeadCoordsY[bar][j][k] - middleOfStaff) > stemDirDiff) {
								if (allNoteHeadCoordsY[bar][j][k] < middleOfStaff) stemDir = -1;
								else stemDir = 1;
								storedGroupStemDir = true;
								stemDirDiff = abs(allNoteHeadCoordsY[bar][j][k] - middleOfStaff);
							}
						}
					}
				}
			}
			// groupped stem directions for groupped notes
			if (forcedStemDir != 0) grouppedStemDirs[bar][i] = forcedStemDir;
			else if (storedGroupStemDir) grouppedStemDirs[bar][i] = stemDir;
			else grouppedStemDirs[bar][i] = stemDirections[bar][i];
			for (j = beamsIndexes[bar][i][0]; j <= beamsIndexes[bar][i][1]; j++) {
				// if the stems of the group point to a different direction than the stem of this note on its own
				// the base and edge notes of a chord change, and so do the X coords of notes a second apart (e.g. <a' b'>)
				// so we lay out the note again
				if (allNotes[bar][j][0] > -1 && stemDirections[bar][j] != grouppedStemDirs[bar][i]) {
					layoutChordNotes(bar, j, noteXCoords[bar][j], grouppedStemDirs[bar][i]);
				}
				// set the second Y coordinate of the note which is the stem length as the direction might have changed
				allNoteStemCoordsY[bar][j] = allNoteHeadCoordsY[bar][j][allChordsEdgeIndexes[bar][j]] + (noteHeight / 8.0) - (staffDist / 10.0) - ((stemHeight + halfStaffDist) * grouppedStemDirs[bar][i]);
				if (allNoteStemCoordsY[bar][j] > allNotesMaxYPos[bar][j] && allNoteStemCoordsY[bar][j] < FLT_MAX) allNotesMaxYPos[bar][j] = allNoteStemCoordsY[bar][j];
				if (allNoteStemCoordsY[bar][j] < allNotesMinYPos[bar][j] && allNoteStemCoordsY[bar][j] > -FLT_MAX) allNotesMinYPos[bar][j] = allNoteStemCoordsY[bar][j];
				// update the stem directions variables too
				stemDirections[bar][j] = grouppedStemDirs[bar][i];
			}
		}
		for (int i = 0; i < (int)beamsIndexes[bar].size(); i++) {
			if (numNotesWithBeams[bar][i] > 1) {
				// store the number of beams per note
				int j;
				for (j = beamsIndexes[bar][i][0]; j <= beamsIndexes[bar][i][1]; j++) {
					numBeams[bar][j] = (log(actualDurs[bar][j]) / log(2)) - 2;
				}
				// get the extreme Y value of the group of beamed notes
				float extremes[2] = {FLT_MAX, FLT_MIN};
				int extremeNdxs[2] = {-1, -1};
				for (j = beamsIndexes[bar][i][0]; j <= beamsIndexes[bar][i][1]; j++) {
					if (allNotes[bar][j][0] > -1 && allNoteStemCoordsY[bar][j] < extremes[0]) {
						extremes[0] = allNoteStemCoordsY[bar][j];
						extremeNdxs[0] = j;
					}
					if (allNotes[bar][j][0] > -1 && allNoteStemCoordsY[bar][j] > extremes[1]) {
						extremes[1] = allNoteStemCoordsY[bar][j];
						extremeNdxs[1] = j;
					}
				}
				// first check if the notes rise of fall linearly (meaning that they either only rise of fall)
				bool linear = true;
				int beamDir;
				// if the beams fall
				if (extremeNdxs[0] < extremeNdxs[1]) {
					beamDir = 1;
					for (int k = beamsIndexes[bar][i][0]+1; k <= beamsIndexes[bar][i][1]; k++) {
						if (allNoteStemCoordsY[bar][k] < allNoteStemCoordsY[bar][k-1]) linear = false;
					}
					if (linear) {
						bool allEqual = true;
						for (int k = beamsIndexes[bar][i][0]+1; k <= beamsIndexes[bar][i][1]; k++) {
							if (allNoteStemCoordsY[bar][k] != allNoteStemCoordsY[bar][k-1]) allEqual = false;
						}
						if (allEqual) linear = false;
					}
				}
				// if they rise
				else {
					beamDir = -1;
					for (int k = beamsIndexes[bar][i][0]+1; k <= beamsIndexes[bar][i][1]; k++) {
						if (allNoteStemCoordsY[bar][k] > allNoteStemCoordsY[bar][k-1]) linear = false;
					}
					if (linear) {
						bool allEqual = true;
						for (int k = beamsIndexes[bar][i][0]+1; k <= beamsIndexes[bar][i][1]; k++) {
							if (allNoteStemCoordsY[bar][k] != allNoteStemCoordsY[bar][k-1]) allEqual = false;
						}
						if (allEqual) linear = false;
					}
				}
				// now we can set new coordinates for the stems
				if (linear) {
					// if stems up, look for the minimum Y coord (higher in the OF window)
					// and vice versa
					int xtrmNdx = (grouppedStemDirs[bar][i] == 1 ? 0 : 1);
					int ndx = (extremeNdxs[xtrmNdx] > -1 ? extremeNdxs[xtrmNdx] : beamsIndexes[bar][i][0]);
					float start = allNoteStemCoordsY[bar][ndx];
					// if the note with the extreme Y coord is a rest, look for a note instead
					if (allNotes[bar][ndx][0] == -1) {
						for (int k = beamsIndexes[bar][i][0]; k <= beamsIndexes[bar][i][1]; k++) {
							if (allNotes[bar][k][0] > -1) {
								start = allNoteStemCoordsY[bar][k] - (k * (halfStaffDist * beamDir));
								break;
							}
						}
					}
					// if the extreme value is not the first note of the beamed group
					int diff = extremeNdxs[xtrmNdx] - beamsIndexes[bar][i][0];
					// offset the start value by as many times as the index difference
					// between the extreme note and the first note of the beamed group
					// this value must be inverted, so we multiply by -1
					start += (((halfStaffDist * beamDir) * (float)diff) * -1.0);
					for (int k = beamsIndexes[bar][i][0]; k <= beamsIndexes[bar][i][1]; k++) {
						allNoteStemCoordsY[bar][k] = start;
						start += (halfStaffDist * beamDir);
						if (allNoteStemCoordsY[bar][k] > allNotesMaxYPos[bar][k] && allNoteStemCoordsY[bar][k] < FLT_MAX) allNotesMaxYPos[bar][k] = allNoteStemCoordsY[bar][k];
						if (allNoteStemCoordsY[bar][k] < allNotesMinYPos[bar][k] && allNoteStemCoordsY[bar][k] > -FLT_MAX) allNotesMinYPos[bar][k] = allNoteStemCoordsY[bar][k];
					}
				}
				else {
					// if the notes don't rise of fall linearly, set all stem coords to the same, maximum value
					for (int k = beamsIndexes[bar][i][0]; k <= beamsIndexes[bar][i][1]; k++) {
						if (grouppedStemDirs[bar][i] == 1) {
							allNoteStemCoordsY[bar][k] = extremes[0];
							// the stems now reach the beam, which is above the notes, so it is their highest point
							// (otherwise elements above the group, e.g. a tuplet bracket, would fall on the beam)
							if (allNoteStemCoordsY[bar][k] < allNotesMinYPos[bar][k] && allNoteStemCoordsY[bar][k] > -FLT_MAX) allNotesMinYPos[bar][k] = allNoteStemCoordsY[bar][k];
						}
						else {
							allNoteStemCoordsY[bar][k] = extremes[1];
							if (allNoteStemCoordsY[bar][k] > allNotesMaxYPos[bar][k] && allNoteStemCoordsY[bar][k] < FLT_MAX) allNotesMaxYPos[bar][k] = allNoteStemCoordsY[bar][k];
							if (allNoteStemCoordsY[bar][k] < allNotesMinYPos[bar][k] && allNoteStemCoordsY[bar][k] > -FLT_MAX) allNotesMinYPos[bar][k] = allNoteStemCoordsY[bar][k];
						}
					}
				}
				// make sure the beams are not drawn on top of rests
				if (beamGroupHasRests[i]) {
					// stems up
					if (grouppedStemDirs[bar][i] == 1) {
						float offset = 0;
						int lowestStemIndex = (extremeNdxs[1] > -1 ? extremeNdxs[1] : beamsIndexes[bar][i][0]);
						int maxNumBeams = 0;
						for (int k = beamsIndexes[bar][i][0]; k <= beamsIndexes[bar][i][1]; k++) {
							if (numBeams[bar][k] > maxNumBeams) maxNumBeams = numBeams[bar][k];
						}
						if (allNoteStemCoordsY[bar][lowestStemIndex] + ((maxNumBeams-1) * (staffDist*BEAMDISTCOEFF)) > 0) {
							offset = allNoteStemCoordsY[bar][lowestStemIndex] + ((maxNumBeams-1) * (staffDist*BEAMDISTCOEFF));
						}
						for (int k = beamsIndexes[bar][i][0]; k <= beamsIndexes[bar][i][1]; k++) {
							allNoteStemCoordsY[bar][k] -= offset;
							if (allNoteStemCoordsY[bar][k] < allNotesMinYPos[bar][k] && allNoteStemCoordsY[bar][k] > -FLT_MAX) allNotesMinYPos[bar][k] = allNoteStemCoordsY[bar][k];
						}
					}
					// stems down
					else {
						float offset = 0;
						int highestStemIndex = (extremeNdxs[0] > -1 ? extremeNdxs[0] : beamsIndexes[bar][i][0]);
						int maxNumBeams = 0;
						for (int k = beamsIndexes[bar][i][0]; k <= beamsIndexes[bar][i][1]; k++) {
							if (numBeams[bar][k] > maxNumBeams) maxNumBeams = numBeams[bar][k];
						}
						if (allNoteStemCoordsY[bar][highestStemIndex] - ((maxNumBeams-1) * (staffDist*BEAMDISTCOEFF)) < (staffDist * 4)) {
							offset = (staffDist * 4) - (allNoteStemCoordsY[bar][highestStemIndex] - ((maxNumBeams-1) * (staffDist*BEAMDISTCOEFF)));
						}
						for (int k = beamsIndexes[bar][i][0]; k <= beamsIndexes[bar][i][1]; k++) {
							allNoteStemCoordsY[bar][k] += offset;
							if (allNoteStemCoordsY[bar][k] > allNotesMaxYPos[bar][k] && allNoteStemCoordsY[bar][k] < FLT_MAX) allNotesMaxYPos[bar][k] = allNoteStemCoordsY[bar][k];
						}
					}
				}
			}
		}
		// make the beam of each group a straight line
		// the beams are drawn between the stems of each pair of notes, so the stems of a group must end on one straight line
		// but the stems of a group that rises or falls are set one step apart for each note, regardless of the distance between the notes
		// so the beam bends wherever the notes are not equally apart on the X axis (e.g. an eighth followed by two 16ths)
		// here we put the end of every stem of a group on the line between the stems of its first and last notes
		// and then move the whole line away from the notes, if needed, so that no stem is too short
		if (!rhythm) {
			for (int i = 0; i < (int)beamsIndexes[bar].size(); i++) {
				if (numNotesWithBeams[bar][i] < 2) continue;
				int groupStart = beamsIndexes[bar][i][0];
				int groupEnd = beamsIndexes[bar][i][1];
				int stemDir = grouppedStemDirs[bar][i];
				// the X coord of the stem of a note, without the offset of the side of the stem, which is the same for the whole group
				auto stemX = [&](int n) {
					return allNoteCoordsX[bar][n][allChordsBaseIndexes[bar][n]] + (allNoteCoordsXOffset[bar][n] * allChordsChangeXCoef[bar][n]);
				};
				// the line goes through the stems of the first and last notes (not rests) of the group
				int first = -1, last = -1;
				for (int n = groupStart; n <= groupEnd; n++) {
					if (allNotes[bar][n][0] < 0) continue;
					if (first < 0) first = n;
					last = n;
				}
				if (first < 0 || first == last || stemX(last) == stemX(first)) continue;
				float xFirst = stemX(first), xLast = stemX(last);
				float yFirst = allNoteStemCoordsY[bar][first], yLast = allNoteStemCoordsY[bar][last];
				for (int n = groupStart; n <= groupEnd; n++) {
					allNoteStemCoordsY[bar][n] = yFirst + ((yLast - yFirst) * (stemX(n) - xFirst) / (xLast - xFirst));
				}
				// the shortest a stem can be, from the note head closest to the beam to the beam
				// plus the space of the extra beams (e.g. of 16ths), which are drawn between the first beam and the note heads
				float shift = 0;
				for (int n = groupStart; n <= groupEnd; n++) {
					if (allNotes[bar][n][0] < 0) continue;
					float minLength = (staffDist * 2.5) + (std::max(0, numBeams[bar][n] - 1) * (staffDist * BEAMDISTCOEFF));
					float headY = allNoteHeadCoordsY[bar][n][allChordsEdgeIndexes[bar][n]];
					// stems up end above the note heads (lower Y), stems down below them (higher Y)
					if (stemDir == 1) shift = std::min(shift, (headY - minLength) - allNoteStemCoordsY[bar][n]);
					else shift = std::max(shift, (headY + minLength) - allNoteStemCoordsY[bar][n]);
				}
				for (int n = groupStart; n <= groupEnd; n++) {
					allNoteStemCoordsY[bar][n] += shift;
					if (allNoteStemCoordsY[bar][n] > allNotesMaxYPos[bar][n] && allNoteStemCoordsY[bar][n] < FLT_MAX) allNotesMaxYPos[bar][n] = allNoteStemCoordsY[bar][n];
					if (allNoteStemCoordsY[bar][n] < allNotesMinYPos[bar][n] && allNoteStemCoordsY[bar][n] > -FLT_MAX) allNotesMinYPos[bar][n] = allNoteStemCoordsY[bar][n];
				}
			}
		}
		// the stems of notes with ledger lines must reach the middle line of the staff
		// the stems of a beamed group end at the beam, so if any of them doesn't reach the middle line
		// we move the whole beam towards it, keeping its slope
		// (e.g. a group of notes above the staff with the stems down would otherwise have its beam above the staff)
		if (!rhythm) {
			for (int i = 0; i < (int)beamsIndexes[bar].size(); i++) {
				float shift = 0;
				for (int k = beamsIndexes[bar][i][0]; k <= beamsIndexes[bar][i][1]; k++) {
					if (allNotes[bar][k][0] < 0) continue; // rests
					// stems down end below the note heads (higher Y), so they must not end above (lower Y than) the middle line
					if (grouppedStemDirs[bar][i] == -1) shift = std::max(shift, middleOfStaff - allNoteStemCoordsY[bar][k]);
					// and the other way round for stems up
					else shift = std::min(shift, middleOfStaff - allNoteStemCoordsY[bar][k]);
				}
				if (shift == 0) continue;
				for (int k = beamsIndexes[bar][i][0]; k <= beamsIndexes[bar][i][1]; k++) {
					allNoteStemCoordsY[bar][k] += shift;
					if (allNoteStemCoordsY[bar][k] > allNotesMaxYPos[bar][k] && allNoteStemCoordsY[bar][k] < FLT_MAX) allNotesMaxYPos[bar][k] = allNoteStemCoordsY[bar][k];
					if (allNoteStemCoordsY[bar][k] < allNotesMinYPos[bar][k] && allNoteStemCoordsY[bar][k] > -FLT_MAX) allNotesMinYPos[bar][k] = allNoteStemCoordsY[bar][k];
				}
			}
		}
	}
}

//--------------------------------------------------------------
void Notes::layoutChordNotes(int bar, int i, float x, int stemDir)
{
	// sets the base (the note the stem starts from) and edge (the note at the other end) indexes of a note or chord
	// and the X coords of its notes, which depend on the direction of the stem when two notes are a second apart
	// this is called when the notes are positioned, and again if a beamed group changes the direction of the stem
	for (unsigned j = 0; j < allNoteCoordsX[bar][i].size(); j++) {
		allNoteCoordsX[bar][i][j] = x;
		allNoteShiftX[bar][i][j] = false;
	}
	allNoteCoordsXOffset[bar][i] = 0;
	allChordsChangeXCoef[bar][i] = 0;
	// get the Y coords of the extreme notes of a chord
	float maxVal = -FLT_MAX;
	float minVal = FLT_MAX;
	if (stemDir >= 0) {
		for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
			if (allNoteHeadCoordsY[bar][i][j] > maxVal) {
				maxVal = allNoteHeadCoordsY[bar][i][j];
				allChordsBaseIndexes[bar][i] = j;
			}
			if (allNoteHeadCoordsY[bar][i][j] < minVal) {
				minVal = allNoteHeadCoordsY[bar][i][j];
				allChordsEdgeIndexes[bar][i] = j;
			}
		}
	}
	else {
		for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
			if (allNoteHeadCoordsY[bar][i][j] < minVal) {
				minVal = allNoteHeadCoordsY[bar][i][j];
				allChordsBaseIndexes[bar][i] = j;
			}
			if (allNoteHeadCoordsY[bar][i][j] > maxVal) {
				maxVal = allNoteHeadCoordsY[bar][i][j];
				allChordsEdgeIndexes[bar][i] = j;
			}
		}
	}
	// once we've stored the Y coords of the note heads
	// we run a loop to see if there are any note heads close together
	// which means we'll have to change the X coord of one of them
	// e.g. in an <e f g> sequence, only f will shift on the X axis
	bool xCoordChanged = false;
	float chordBaseNoteChangeCoef = 1;
	for (unsigned j = 1; j < allNoteHeadCoordsY[bar][i].size(); j++) {
		for (unsigned k = 0; k < allNoteHeadCoordsY[bar][i].size(); k++) {
			// don't bother with the same note
			if (j == k) continue;
			// if two notes are close together on the Y axis
			if (abs(allNoteHeadCoordsY[bar][i][j] - allNoteHeadCoordsY[bar][i][k]) < staffDist) {
				// if k is on top (smaller Y coord)
				if (allNoteHeadCoordsY[bar][i][j] > allNoteHeadCoordsY[bar][i][k]) {
					// if both j and k haven't moved yet
					if ((allNoteCoordsX[bar][i][j] == x) && (allNoteCoordsX[bar][i][k] == x)) {
						// set the boolean to true, so the top note is moved when drawn
						allNoteCoordsX[bar][i][k] += noteWidth;
						allNoteShiftX[bar][i][j] = true;
						xCoordChanged = true;
						// if the base of the stem down chord changes
						// then the X axis should not take an offset
						if ((int)k == allChordsBaseIndexes[bar][i] && stemDir == -1) {
							chordBaseNoteChangeCoef = 0;
						}
					}
				}
				else {
					if ((allNoteCoordsX[bar][i][k] == x) && (allNoteCoordsX[bar][i][j] == x)) {
						allNoteCoordsX[bar][i][j] += noteWidth;
						allNoteShiftX[bar][i][j] = true;
						xCoordChanged = true;
						if ((int)j == allChordsBaseIndexes[bar][i] && stemDir == -1) {
							chordBaseNoteChangeCoef = 0;
						}
					}
				}
			}
		}
	}
	if (xCoordChanged) {
		// if we have notes close together, we shift all notes by half the note width
		// so they can be centered
		for (unsigned j = 0; j < allNoteCoordsX[bar][i].size(); j++) {
			if (stemDir == -1 && chordBaseNoteChangeCoef > 0) {
				allNoteCoordsX[bar][i][j] -= (noteWidth / 2);
			}
		}
		if (stemDir == -1) {
			allNoteCoordsXOffset[bar][i] = noteWidth;
			if (chordBaseNoteChangeCoef > 0) allChordsChangeXCoef[bar][i] = chordBaseNoteChangeCoef;
		}
	}
}

//--------------------------------------------------------------
float Notes::getXLength()
{
	return xLength;
}

//--------------------------------------------------------------
void Notes::storeArticulationsCoords(int bar)
{
	for (unsigned i = 0; i < allArticulations[bar].size(); i++) {
		for (unsigned j = 0; j < allArticulations[bar][i].size(); j++) {
			if (allArticulations[bar][i][j] > 0) {
				float xPos = allNoteCoordsX[bar][i][allChordsBaseIndexes[bar][i]];
				if (stemDirections[bar][i] > 0) xPos -= (noteWidth/4);
				else xPos -= (noteWidth/2);
				float yPos = allNoteHeadCoordsY[bar][i][allChordsBaseIndexes[bar][i]];
				// if we have more than one articulation symbol
				if (stemDirections[bar][i] > 0) yPos += (staffDist * j);
				else yPos -= (staffDist * j);
				if (stemDirections[bar][i] > 0) {
					yPos += staffDist;
					if (yPos > allNotesMaxYPos[bar][i] && yPos < FLT_MAX) allNotesMaxYPos[bar][i] = yPos;
				}
				else {
					yPos -= staffDist;
					if (yPos < allNotesMinYPos[bar][i] && yPos > -FLT_MAX) allNotesMinYPos[bar][i] = yPos;
				}
				// check if the note head is on a line, but only inside the stave
				if ((allNoteHeadCoordsY[bar][i][allChordsBaseIndexes[bar][i]] > 0) && \
						(allNoteHeadCoordsY[bar][i][allChordsBaseIndexes[bar][i]] < (staffDist*5)) && \
						!rhythm) {
					if (((int)(abs(allNoteHeadCoordsY[bar][i][allChordsBaseIndexes[bar][i]]) / halfStaffDist) % 2) == 0) {
						if (stemDirections[bar][i] > 0) yPos += halfStaffDist;
						else  yPos -= halfStaffDist;
						if (yPos + (notationFont->height(articulSyms[allArticulations[bar][i][j]-1])/2) > allNotesMaxYPos[bar][i] && yPos < FLT_MAX) {
							// articulation symbols start with index 1, because 0 is reserved for no articulation
							// so we subtract 1 from the index
							allNotesMaxYPos[bar][i] = yPos + (notationFont->height(articulSyms[allArticulations[bar][i][j]-1])/2);
						}
						if (yPos - (notationFont->height(articulSyms[allArticulations[bar][i][j]-1])/2) < allNotesMinYPos[bar][i] && yPos > -FLT_MAX) {
							allNotesMinYPos[bar][i] = yPos - (notationFont->height(articulSyms[allArticulations[bar][i][j]-1])/2);
						}
					}
				}
				articulXPos[bar][i] = xPos;
				articulYPos[bar][i][j] = yPos;
				if (allArticulations[bar][i][j] == 7) {
					float yPosPortando;
					if (stemDirections[bar][i] > 0) {
						// check if the note head is inside the stave
						if (allNoteHeadCoordsY[bar][i][allChordsBaseIndexes[bar][i]] < (staffDist*5)) {
							yPosPortando = staffDist * 5;
						}
						else {
							yPosPortando = allNoteHeadCoordsY[bar][i][allChordsBaseIndexes[bar][i]] + noteHeight;
						}
					}
					else {
						// check if the note head is inside the stave
						if (allNoteHeadCoordsY[bar][i][allChordsBaseIndexes[bar][i]] > 0) {
							yPosPortando = -staffDist;
						}
						else {
							yPosPortando = allNoteHeadCoordsY[bar][i][allChordsBaseIndexes[bar][i]] - noteHeight;
						}
					}
					// if case the articulation is a portando, we must get the height as in the line below
					float halfArticulHeight = (notationFont->height(".") + notationFont->height("_")) / 2;
					if (yPosPortando + halfArticulHeight > allNotesMaxYPos[bar][i] && yPosPortando < FLT_MAX) {
						allNotesMaxYPos[bar][i] = yPosPortando + halfArticulHeight;
					}
					if (yPosPortando - halfArticulHeight < allNotesMinYPos[bar][i] && yPosPortando > -FLT_MAX) {
						allNotesMinYPos[bar][i] = yPosPortando - halfArticulHeight;
					}
					articulYPos[bar][i][j] = yPosPortando;
				}
			}
		}
	}
}

//--------------------------------------------------------------
void Notes::storeTupletCoords(int bar)
{
	if (tupStartStop[bar].size() == 0) {
		std::vector<float> v(1, 0); // dummy std::vector to not cause a crash on bar copying
		tupXCoords[bar][0] = v;
		tupYCoords[bar][0] = v;
		tupDirs[bar][0] = 1;
		return;
	}
	// tuplets can be nested, and the brackets of inner tuplets are placed first
	// they are stored as part of their notes (see below), so the bracket of an outer tuplet goes outside them
	std::vector<std::map<int, std::pair<unsigned, unsigned>>::iterator> tupletsInnerFirst;
	for (auto it = tupStartStop[bar].begin(); it != tupStartStop[bar].end(); ++it) tupletsInnerFirst.push_back(it);
	std::stable_sort(tupletsInnerFirst.begin(), tupletsInnerFirst.end(), [](auto a, auto b) {
		return (a->second.second - a->second.first) < (b->second.second - b->second.first);
	});
	for (auto it : tupletsInnerFirst) {
		unsigned first = it->second.first;
		unsigned last = it->second.second;
		// the bracket goes on the side of most stems (above if most stems point up)
		int stemDirAccum = 0;
		for (unsigned i = first; i <= last; i++) {
			stemDirAccum += stemDirections[bar][i];
		}
		bool above = (stemDirAccum >= 0);
		int dir = (above ? -1 : 1); // the direction away from the notes on the Y axis
		// the outer edge of a note (or chord) on the side of the bracket: its highest point if the bracket is above, its lowest if below
		// this includes stems, beams and articulations, which have already been stored
		auto outerEdge = [&](unsigned n) {
			return (above ? allNotesMinYPos[bar][n] : allNotesMaxYPos[bar][n]);
		};
		std::vector<float> vX(4, 0);
		std::vector<float> vY(4, 0);
		vX[0] = allNoteCoordsX[bar][first][allChordsBaseIndexes[bar][first]];
		vX[3] = allNoteCoordsX[bar][last][allChordsBaseIndexes[bar][last]];
		// the ends of the bracket are one staff space outside the first and last notes
		vY[0] = outerEdge(first) + (staffDist * dir);
		vY[3] = outerEdge(last) + (staffDist * dir);
		// the Y coord of the bracket at an X coord, keeping its slope
		auto bracketY = [&](float x) {
			if (vX[3] == vX[0]) return vY[0];
			return vY[0] + ((vY[3] - vY[0]) * (x - vX[0]) / (vX[3] - vX[0]));
		};
		// move the bracket away from the notes, so that none of the notes in between sticks out of it
		float shift = 0;
		for (unsigned i = first + 1; i < last; i++) {
			float limit = outerEdge(i) + (staffDist * dir);
			float y = bracketY(allNoteCoordsX[bar][i][allChordsBaseIndexes[bar][i]]);
			if (above) shift = std::min(shift, limit - y);
			else shift = std::max(shift, limit - y);
		}
		vY[0] += shift;
		vY[3] += shift;
		// the bracket is interrupted in the middle, where the tuplet number is written
		std::string tuplet = std::to_string(tupRatios[bar][it->first].first);
		float stringWidth = textFont->width(tuplet);
		float halfStringHeight = textFont->height(tuplet) / 2.0;
		vX[1] = vX[2] = vX[0] + ((vX[3] - vX[0]) / 2.0);
		vX[1] -= stringWidth;
		vX[2] += stringWidth;
		vY[1] = bracketY(vX[1]);
		vY[2] = bracketY(vX[2]);
		// store the bracket and its number as part of the notes it spans, so that other elements (e.g. ottavas) don't fall on it
		for (unsigned i = first; i <= last; i++) {
			float y = bracketY(allNoteCoordsX[bar][i][allChordsBaseIndexes[bar][i]]);
			if (above) {
				if (y - halfStringHeight < allNotesMinYPos[bar][i]) allNotesMinYPos[bar][i] = y - halfStringHeight;
			}
			else {
				if (y + halfStringHeight > allNotesMaxYPos[bar][i]) allNotesMaxYPos[bar][i] = y + halfStringHeight;
			}
		}
		// move the ends to the stems, if the stems of the first and last notes point to the side of the bracket
		if (above) {
			if (stemDirections[bar][first] > 0) vX[0] += ((noteWidth / 2.0) - lineWidth);
			if (stemDirections[bar][last] > 0) vX[3] += ((noteWidth / 2.0) - lineWidth);
		}
		else {
			if (stemDirections[bar][first] < 0) vX[0] -= ((noteWidth / 2.0) - (lineWidth / 2.0));
			if (stemDirections[bar][last] < 0) vX[3] -= ((noteWidth / 2.0) - (lineWidth / 2.0));
		}
		tupXCoords[bar][it->first] = vX;
		tupYCoords[bar][it->first] = vY;
		tupDirs[bar][it->first] = (above ? 1 : -1);
	}
}

//--------------------------------------------------------------
void Notes::storeSlurCoords(int bar)
{
	int dirAccum = 0;
	// the std::vector below is used to calculate the orientation of the middle points of the curve
	std::vector<int> slurDir(slurIndexes[bar].size(), 1);
	for (unsigned i = 0; i < slurIndexes[bar].size(); i++) {
		// if both start and end indexes of a slur are -1, it means there is no slur
		if (slurIndexes[bar][i].first == -1 && slurIndexes[bar][i].second == -1 && !isWholeSlurred[bar]) {
			continue;
		}
		int slurStartNdx;
		if (slurIndexes[bar][i].first > -1) slurStartNdx = slurIndexes[bar][i].first;
		else slurStartNdx = 0;
		int slurStopNdx;
		if (slurIndexes[bar][i].second > -1) slurStopNdx = slurIndexes[bar][i].second;
		else slurStopNdx = (int)allNotes[bar].size() - 1;
		// the direction of the slur will depend on the majority of stem directions
		for (int j = slurStartNdx; j <= slurStopNdx; j++) {
			dirAccum += stemDirections[bar][j];
		}
		// when we store many bars with the \bars command, we first calculate slurs for each bar separately
		// and when done, we calculate the direction of slurs that span more than one bar
		// if this bar doesn't have a closing parenthesis, we must iterate over all consequtive bars
		// till we find the end of the slur, to determine the position of the slur curve
		if (slurIndexes[bar][i].second == -1) {
			bool foundSlurStop = false;
			int nextBar = bar + 1;
			while (!foundSlurStop) {
				// stop if there are no more bars (the slur is left open at the end of the bar, or ends in a bar that is not defined yet)
				// otherwise this loop would never end, and slurIndexes[nextBar] would create empty bars forever
				if (slurIndexes.find(nextBar) == slurIndexes.end()) break;
				// first check if the next bar is slurred entirely
				if (slurIndexes[nextBar].size() == 1 && \
						(slurIndexes[nextBar][0].first == -1 && slurIndexes[nextBar][0].second == -1)) {
					for (unsigned j = 0; j < allNotes[nextBar].size(); j++) {
						dirAccum += stemDirections[nextBar][j];
					}
				}
				else {
					for (unsigned j = 0; j < slurIndexes[nextBar].size(); j++) {
						if (slurIndexes[nextBar][j].second > -1) {
							for (int k = 0; k < slurIndexes[nextBar][j].second; k++) {
								dirAccum += stemDirections[nextBar][k];
							}
							foundSlurStop = true;
							break;
						}
					}
				}
				nextBar++;
			}
		}
		// the same logic applies to a missing opening parenthesis, but inversed
		if (slurIndexes[bar][i].first == -1) {
			bool foundSlurStart = false;
			int prevBar = bar - 1;
			while (!foundSlurStart) {
				// stop if there are no more previous bars (the slur start is missing)
				if (prevBar < 0 || slurIndexes.find(prevBar) == slurIndexes.end()) break;
				// first check if the previous bar is slurred entirely
				if (slurIndexes[prevBar].size() == 1 && \
						(slurIndexes[prevBar][0].first == -1 && slurIndexes[prevBar][0].second == -1)) {
					for (int j = (int)allNotes[prevBar].size() - 1; j >= 0; j--) {
						dirAccum += stemDirections[prevBar][j];
					}
					slurLinks[bar][i].first++;
				}
				else {
					for (int j = (int)slurIndexes[prevBar].size() - 1; j >= 0; j--) {
						if (slurIndexes[prevBar][j].first > -1) {
							for (int k = (int)allNotes[prevBar].size()-1; k >= slurIndexes[prevBar][j].first; k--) {
								dirAccum += stemDirections[prevBar][k];
							}
							foundSlurStart = true;
							break;
						}
					}
				}
				prevBar--;
			}
		}
		// if the accumulation is 0 it means we're even so we chose to go below the staff
		// this is the inverse logic of the stems, as we want to connect note heads
		// so when the stems are upward with stemDirectios[bar][k] = 1, we store a 1 to slurDir
		// but that means below, and not above
		// (slurDir has an entry per slur, not per note, so only the entry of this slur is set)
		if (dirAccum == 0) {
			slurDir[i] = 1;
		}
		else {
			slurDir[i] = (dirAccum > 0 ? 1 : 0);
		}
	}
	slurStartX[bar] = std::vector<float>(slurIndexes[bar].size(), -1);
	slurStartY[bar] = std::vector<float>(slurIndexes[bar].size(), -1);
	slurStopX[bar] = std::vector<float>(slurIndexes[bar].size(), -1);
	slurStopY[bar] = std::vector<float>(slurIndexes[bar].size(), -1);
	slurMiddleX1[bar] = std::vector<float>(slurIndexes[bar].size(), -1);
	slurMiddleX2[bar] = std::vector<float>(slurIndexes[bar].size(), -1);
	slurMiddleY1[bar] = std::vector<float>(slurIndexes[bar].size(), -1);
	slurMiddleY2[bar] = std::vector<float>(slurIndexes[bar].size(), -1);
	slurLinks[bar] = std::vector<std::pair<int, int>>(slurIndexes[bar].size(), std::make_pair(0, 0));
	for (unsigned i = 0; i < slurIndexes[bar].size(); i++) {
		// if both start and end indexes of a slur are -1, it means there is no slur
		if (slurIndexes[bar][i].first == -1 && slurIndexes[bar][i].second == -1 && !isWholeSlurred[bar]) continue;
		int slurStartNdx = slurIndexes[bar][i].first;
		int slurStopNdx = slurIndexes[bar][i].second;
		if (slurStartNdx > -1) {
			slurStartX[bar][i] = allNoteCoordsX[bar][slurStartNdx][allChordsBaseIndexes[bar][slurStartNdx]];
		}
		else {
			slurStartX[bar][i] = -(2 * staffDist);
			slurLinks[bar][i].first = 1;
			isLinked[bar].first = std::min(isLinked[bar].first, -slurLinks[bar][i].first);
			bool foundSlurStart = false;
			int prevBar = bar - 1;
			while (!foundSlurStart) {
				// stop if there are no more previous bars (the slur start is missing)
				if (prevBar < 0 || slurIndexes.find(prevBar) == slurIndexes.end()) break;
				// first check if the previous bar is slurred entirely
				if (slurIndexes[prevBar].size() == 1 && \
						(slurIndexes[prevBar][0].first == -1 && slurIndexes[prevBar][0].second == -1)) {
					slurLinks[bar][i].first++;
					isLinked[bar].first = std::min(isLinked[bar].first, -slurLinks[bar][i].first);
				}
				else {
					for (int j = (int)slurIndexes[prevBar].size() - 1; j >= 0; j--) {
						if (slurIndexes[prevBar][j].first > -1) {
							foundSlurStart = true;
							break;
						}
					}
				}
				prevBar--;
			}
		}
		if (slurDir[i] > 0) {
			if (slurStartNdx > -1) {
				if (stemDirections[bar][slurStartNdx] > 0) {
					// if slur and stem directions are the same, we store the Y coord of the note head
					slurStartY[bar][i] = allNoteHeadCoordsY[bar][slurStartNdx][allChordsBaseIndexes[bar][slurStartNdx]];
					slurStartY[bar][i] += slurYOffset();
					if (slurStartY[bar][i] < allNotesMaxYPos[bar][slurStartNdx] && slurStartY[bar][i] < FLT_MAX) {
						slurStartY[bar][i] = allNotesMaxYPos[bar][slurStartNdx] + noteHeight;
						allNotesMaxYPos[bar][slurStartNdx] = slurStartY[bar][i]; // + (noteHeight/2);
					}
				}
				else {
					// otherwise we store the Y coord of the note stem
					slurStartY[bar][i] = allNoteStemCoordsY[bar][slurStartNdx];
					slurStartY[bar][i] += halfStaffDist;
					if (slurStartY[bar][i] < allNotesMaxYPos[bar][slurStartNdx] && slurStartY[bar][i] < FLT_MAX) {
						slurStartY[bar][i] = allNotesMaxYPos[bar][slurStartNdx] + halfStaffDist;
						allNotesMaxYPos[bar][slurStartNdx] = slurStartY[bar][i]; // + (noteHeight/2);
					}
				}
			}
			else {
				slurStartY[bar][i] = 9 * halfStaffDist;
				int loopIter;
				if (slurStopNdx > -1) loopIter = slurStopNdx;
				else loopIter = (int)allNotes[bar].size();
				for (int j = 0; j < loopIter; j++) {
					if (stemDirections[bar][j] > 0) {
						if (slurStartY[bar][i] < allNoteHeadCoordsY[bar][j][allChordsBaseIndexes[bar][j]]) {
							slurStartY[bar][i] = allNoteHeadCoordsY[bar][j][allChordsBaseIndexes[bar][j]] + halfStaffDist;
							if (allNotesMaxYPos[bar][j] < slurStartY[bar][i]) {
								allNotesMaxYPos[bar][j] = slurStartY[bar][i];
							}
						}
					}
					else {
						if (slurStartY[bar][i] < allNoteStemCoordsY[bar][j]) {
							slurStartY[bar][i] = allNoteStemCoordsY[bar][j] + halfStaffDist;
							if (allNotesMaxYPos[bar][j] < slurStartY[bar][i]) {
								allNotesMaxYPos[bar][j] = slurStartY[bar][i];
							}
						}
					}
				}
			}
		}
		else {
			if (slurStartNdx > -1) {
				if (stemDirections[bar][slurStartNdx] < 0) {
					slurStartY[bar][i] = allNoteHeadCoordsY[bar][slurStartNdx][allChordsBaseIndexes[bar][slurStartNdx]];
					slurStartY[bar][i] -= slurYOffset();
					if (slurStartY[bar][i] > allNotesMinYPos[bar][slurStartNdx] && slurStartY[bar][i] > -FLT_MAX) {
						slurStartY[bar][i] = allNotesMinYPos[bar][slurStartNdx] - noteHeight;
						allNotesMinYPos[bar][slurStartNdx] = slurStartY[bar][i]; // - (noteHeight/2);
					}
				}
				else {
					slurStartY[bar][i] = allNoteStemCoordsY[bar][slurStartNdx];
					slurStartY[bar][i] -= halfStaffDist;
					if (slurStartY[bar][i] > allNotesMinYPos[bar][slurStartNdx] && slurStartY[bar][i] > -FLT_MAX) {
						slurStartY[bar][i] = allNotesMinYPos[bar][slurStartNdx] - halfStaffDist;
						allNotesMinYPos[bar][slurStartNdx] = slurStartY[bar][i]; // - (noteHeight/2);
					}
				}
			}
			else {
				slurStartY[bar][i] = -halfStaffDist;
				int loopIter;
				if (slurStopNdx > -1) loopIter = slurStopNdx;
				else loopIter = (int)allNotes[bar].size();
				for (int j = 0; j < loopIter; j++) {
					if (stemDirections[bar][j] > 0) {
						if (slurStartY[bar][i] > allNoteStemCoordsY[bar][j]) {
							slurStartY[bar][i] = allNoteStemCoordsY[bar][j] - halfStaffDist;
							if (slurStartY[bar][i] < allNotesMinYPos[bar][j] && slurStartY[bar][i] > -FLT_MAX) {
								allNotesMinYPos[bar][j] = slurStartY[bar][i];
							}
						}
					}
					else {
						if (slurStartY[bar][i] > allNoteHeadCoordsY[bar][j][allChordsBaseIndexes[bar][j]]) {
							slurStartY[bar][i] = allNoteHeadCoordsY[bar][j][allChordsBaseIndexes[bar][j]] - halfStaffDist;
							if (slurStartY[bar][i] < allNotesMinYPos[bar][j] && slurStartY[bar][i] > -FLT_MAX) {
								allNotesMinYPos[bar][j] = slurStartY[bar][i];
							}
						}
					}
				}
			}
		}
		if (slurStopNdx > -1) {
			slurStopX[bar][i] = allNoteCoordsX[bar][slurStopNdx][allChordsBaseIndexes[bar][slurStopNdx]];
		}
		else {
			slurStopX[bar][i] = xLength;
			slurLinks[bar][i].second = 1;
			isLinked[bar].second = std::max(isLinked[bar].second, slurLinks[bar][i].second);
			bool foundSlurStart = false;
			int nextBar = bar + 1;
			while (!foundSlurStart) {
				// stop if there are no more bars (the slur end is missing, or is in a bar that is not defined yet)
				// otherwise slurIndexes[nextBar] would create empty bars forever
				if (slurIndexes.find(nextBar) == slurIndexes.end()) break;
				// first check if the next bar is slurred entirely
				if (slurIndexes[nextBar].size() == 1 && \
						(slurIndexes[nextBar][0].first == -1 && slurIndexes[nextBar][0].second == -1)) {
					slurLinks[bar][i].second++;
					isLinked[bar].second = std::max(isLinked[bar].second, slurLinks[bar][i].second);
				}
				else {
					for (unsigned j = 0; j < slurIndexes[nextBar].size(); j++) {
						if (slurIndexes[nextBar][j].second > -1) {
							foundSlurStart = true;
							break;
						}
					}
				}
				nextBar++;
			}
		}
		if (slurDir[i] > 0) {
			if (slurStopNdx > -1) {
				if (stemDirections[bar][slurStopNdx] > 0) {
					slurStopY[bar][i] = allNoteHeadCoordsY[bar][slurStopNdx][allChordsBaseIndexes[bar][slurStopNdx]];
					slurStopY[bar][i] += slurYOffset();
					if (slurStopY[bar][i] < allNotesMaxYPos[bar][slurStopNdx] && slurStopY[bar][i] < FLT_MAX) {
						slurStopY[bar][i] = allNotesMaxYPos[bar][slurStopNdx] + noteHeight;
						allNotesMaxYPos[bar][slurStopNdx] = slurStopY[bar][i];
					}
				}
				else {
					slurStopY[bar][i] = allNoteStemCoordsY[bar][slurStopNdx];
					slurStopY[bar][i] += halfStaffDist;
					if (slurStopY[bar][i] < allNotesMaxYPos[bar][slurStopNdx] && slurStopY[bar][i] < FLT_MAX) {
						slurStopY[bar][i] = allNotesMaxYPos[bar][slurStopNdx] + halfStaffDist;
						allNotesMaxYPos[bar][slurStopNdx] = slurStopY[bar][i];
					}
				}
			}
			else {
				slurStopY[bar][i] = 9 * halfStaffDist;
				int loopBegin;
				if (slurStartNdx > -1) loopBegin = slurStartNdx;
				else loopBegin = 0;
				for (int j = loopBegin; j < (int)allNotes[bar].size(); j++) {
					if (stemDirections[bar][j] > 0) {
						if (slurStopY[bar][i] < allNoteHeadCoordsY[bar][j][allChordsBaseIndexes[bar][j]]) {
							slurStopY[bar][i] = allNoteHeadCoordsY[bar][j][allChordsBaseIndexes[bar][j]] + halfStaffDist;
							if (allNotesMaxYPos[bar][j] < slurStopY[bar][i]) {
								allNotesMaxYPos[bar][j] = slurStopY[bar][i];
							}
						}
					}
					else {
						if (slurStopY[bar][i] < allNoteStemCoordsY[bar][j]) {
							slurStopY[bar][i] = allNoteStemCoordsY[bar][j] + halfStaffDist;
							if (allNotesMaxYPos[bar][j] < slurStopY[bar][i]) {
								allNotesMaxYPos[bar][j] = slurStopY[bar][i];
							}
						}
					}
				}
			}
		}
		else {
			if (slurStopNdx > -1) {
				if (stemDirections[bar][slurStopNdx] < 0) {
					slurStopY[bar][i] = allNoteHeadCoordsY[bar][slurStopNdx][allChordsBaseIndexes[bar][slurStopNdx]];
					slurStopY[bar][i] -= slurYOffset();
					if (slurStopY[bar][i] > allNotesMinYPos[bar][slurStopNdx] && slurStopY[bar][i] > -FLT_MAX) {
						slurStopY[bar][i] = allNotesMinYPos[bar][slurStopNdx] - noteHeight;
						allNotesMinYPos[bar][slurStopNdx] = slurStopY[bar][i];
					}
				}
				else {
					slurStopY[bar][i] = allNoteStemCoordsY[bar][slurStopNdx];
					slurStopY[bar][i] -= halfStaffDist;
					if (slurStopY[bar][i] > allNotesMinYPos[bar][slurStopNdx] && slurStopY[bar][i] > -FLT_MAX) {
						slurStopY[bar][i] = allNotesMinYPos[bar][slurStopNdx] - halfStaffDist;
						allNotesMinYPos[bar][slurStopNdx] = slurStopY[bar][i];
					}
				}
			}
			else {
				slurStopY[bar][i] = -halfStaffDist;
				int loopBegin;
				if (slurStartNdx > -1) loopBegin = slurStartNdx;
				else loopBegin = 0;
				for (int j = loopBegin; j < (int)allNotes[bar].size(); j++) {
					if (stemDirections[bar][j] > 0) {
						if (slurStopY[bar][i] > allNoteStemCoordsY[bar][j]) {
							slurStopY[bar][i] = allNoteStemCoordsY[bar][j] - halfStaffDist;
							if (slurStopY[bar][i] < allNotesMinYPos[bar][j] && slurStopY[bar][i] > -FLT_MAX) {
								allNotesMinYPos[bar][j] = slurStopY[bar][i];
							}
						}
					}
					else {
						if (slurStopY[bar][i] > allNoteHeadCoordsY[bar][j][allChordsBaseIndexes[bar][j]]) {
							slurStopY[bar][i] = allNoteHeadCoordsY[bar][j][allChordsBaseIndexes[bar][j]] - halfStaffDist;
							if (slurStopY[bar][i] < allNotesMinYPos[bar][j] && slurStopY[bar][i] > -FLT_MAX) {
								allNotesMinYPos[bar][j] = slurStopY[bar][i];
							}
						}
					}
				}
			}
		}
	}
	for (unsigned i = 0; i < slurIndexes[bar].size(); i++) {
		// if both start and end indexes of a slur are -1, it means there is no slur
		if (slurIndexes[bar][i].first == -1 && slurIndexes[bar][i].second == -1 && !isWholeSlurred[bar]) continue;
		int slurStartNdx;
		if (slurIndexes[bar][i].first > -1) slurStartNdx = slurIndexes[bar][i].first;
		else slurStartNdx = 0;
		int slurStopNdx;
		if (slurIndexes[bar][i].second > -1) slurStopNdx = slurIndexes[bar][i].second;
		else slurStopNdx = (int)allNotes[bar].size() - 1;
		float curveSegment = (slurStopX[bar][i] - slurStartX[bar][i]) / 3.0;
		float middleX = slurStartX[bar][i] + curveSegment;
		slurMiddleX1[bar][i] = middleX;
		middleX += curveSegment;
		slurMiddleX2[bar][i] = middleX;
		float middleY1;
		if (slurDir[i] > 0) {
			middleY1 = slurStartY[bar][i] > slurStopY[bar][i] ? slurStartY[bar][i] : slurStopY[bar][i];
		}
		else {
			middleY1 = slurStartY[bar][i] < slurStopY[bar][i] ? slurStartY[bar][i] : slurStopY[bar][i];
		}
		float middleY2 = middleY1;
		if (slurDir[i] > 0) {
			middleY1 += staffDist;
			middleY2 += staffDist;
		}
		else {
			middleY1 -= staffDist;
			middleY2 -= staffDist;
		}
		for (int j = slurStartNdx; j < slurStopNdx; j++) {
			if (slurDir[i] > 0) {
				if (middleY1 < allNotesMaxYPos[bar][j]) {
					middleY1 = allNotesMaxYPos[bar][j] + halfStaffDist;
				}
				if (middleY2 < allNotesMaxYPos[bar][j]) {
					middleY2 = allNotesMaxYPos[bar][j] + halfStaffDist;
				}
				// separate test for allNotesMaxYPos, because if it is included in the
				// if tests above, only one such y position inside the slur will be updated
				// and it is possible that many notes are included in the slur
				// the actual beginning and ending of a slur though, should not be included
				// as the y position is already determined there
				if (j > 0 && j < slurStopNdx-1) {
					if ((allNotesMaxYPos[bar][j] < middleY1 && middleY1 < FLT_MAX) || (allNotesMaxYPos[bar][j] < middleY2 && middleY2 < FLT_MAX)) {
						allNotesMaxYPos[bar][j] = std::max(middleY1, middleY2); // + (noteHeight/2);
					}
				}
			}
			else {
				if (middleY1 > allNotesMinYPos[bar][j] && middleY1 > -FLT_MAX) {
					middleY1 = allNotesMinYPos[bar][j] + halfStaffDist;
				}
				if (middleY2 > allNotesMinYPos[bar][j] && middleY2 > -FLT_MAX) {
					middleY2 = allNotesMinYPos[bar][j] + halfStaffDist;
				}
				if (j > 0 && j < slurStopNdx-1) {
					if ((allNotesMinYPos[bar][j] > middleY1 && middleY1 > -FLT_MAX) || (allNotesMinYPos[bar][j] < middleY2 && middleY2 > -FLT_MAX)) {
						allNotesMinYPos[bar][j] = std::min(middleY1, middleY2); // - (noteHeight/2);
					}
				}
			}
		}
		// a tie under the slur, on the same side, curves away from its notes, so the slur goes over it
		for (int j = slurStartNdx; j < slurStopNdx || (slurIndexes[bar][i].second == -1 && j <= slurStopNdx); j++) {
			if (j >= (int)tieIndexes[bar].size() || tieIndexes[bar][j] < 0) continue;
			bool tieBelow = stemDirections[bar][j] > 0;
			if (tieBelow != (slurDir[i] > 0)) continue;
			float headY = allNoteHeadCoordsY[bar][j][allChordsBaseIndexes[bar][j]];
			// the tie starts at tieYOffset() and curves by up to 3/4 of halfStaffDist (see drawTie()), and the slur leaves a gap over it
			float tieExtent = tieYOffset() + (halfStaffDist * 0.75) + halfStaffDist;
			if (slurDir[i] > 0) {
				middleY1 = std::max(middleY1, headY + tieExtent);
				middleY2 = std::max(middleY2, headY + tieExtent);
			}
			else {
				middleY1 = std::min(middleY1, headY - tieExtent);
				middleY2 = std::min(middleY2, headY - tieExtent);
			}
		}
		slurMiddleY1[bar][i] = middleY1;
		slurMiddleY2[bar][i] = middleY2;
	}
}

//--------------------------------------------------------------
float Notes::tieXOffset()
{
	// a tie starts to the right of the center of the note head, towards its edge, so it doesn't start under the note head
	return noteWidth * 0.4;
}

//--------------------------------------------------------------
float Notes::tieYOffset()
{
	// a tie starts at the edge of the note head
	return noteHeight * 0.5;
}

//--------------------------------------------------------------
float Notes::slurYOffset()
{
	// a slur on the side of the note head starts and ends a bit further from it than a tie, lower with the stems up, higher with the stems down
	return tieYOffset() + (noteHeight * 0.5);
}

//--------------------------------------------------------------
void Notes::storeOttavaCoords(int bar)
{
	allOttavasYCoords[bar] = std::vector<float>(allOttavas[bar].size(), FLT_MIN);
	allOttavasChangedAt[bar] = std::vector<int>(allOttavas[bar].size(), -1);
	int prevOttava = 0;
	for (unsigned i = 0; i < allOttavas[bar].size(); i++) {
		if (allOttavas[bar][i] != 0) {
			if (allOttavas[bar][i] != prevOttava) {
				allOttavasChangedAt[bar][i] = i;
				float yPos;
				float textHalfHeight = notationFont->height(octaveSyms[abs(allOttavas[bar][i])]) / 2;
				if (allOttavas[bar][i] > 0) {
					// the compiler complains if I just compare with 0.0
					// so I create a float here
					float zero = 0.0;
					if (stemDirections[bar][i] > 0) {
						yPos = std::min(allNotesMinYPos[bar][i], std::min(allNoteStemCoordsY[bar][i], zero));
					}
					else {
						yPos = std::min(allNotesMinYPos[bar][i], std::min(allNoteHeadCoordsY[bar][i][0], zero));
					}
					yPos -= staffDist; //(noteHeight + halfStaffDist);
					if (stemDirections[bar][i] > 0 && allNoteStemCoordsY[bar][i] - (yPos+textHalfHeight) < halfStaffDist) {
						yPos -= halfStaffDist;
					}
					else if (stemDirections[bar][i] < 0) {
						for (unsigned j = 0; j < allNoteHeadCoordsY[bar][i].size(); j++) {
							if ((yPos+textHalfHeight) - allNoteHeadCoordsY[bar][i][j] < halfStaffDist) {
								yPos -= halfStaffDist;
							}
						}
					}
					if (yPos - textHalfHeight < allNotesMinYPos[bar][i] && yPos > -FLT_MAX) {
						allNotesMinYPos[bar][i] = yPos; // - textHalfHeight;
					}
				}
				else {
					if (stemDirections[bar][i] > 0) {
						yPos = std::max(allNotesMaxYPos[bar][i], std::max(allNoteHeadCoordsY[bar][i][0], (halfStaffDist*9)));
					}
					else {
						yPos = std::max(allNotesMaxYPos[bar][i], std::max(allNoteStemCoordsY[bar][i], (halfStaffDist*9)));
					}
					yPos += staffDist; // (noteHeight + halfStaffDist);
					if (stemDirections[bar][i] < 0 && (yPos-textHalfHeight) - allNoteStemCoordsY[bar][i] < halfStaffDist) {
						yPos += halfStaffDist;
					}
					else if (stemDirections[bar][i] > 0) {
						for (unsigned j = 0; j < allNoteHeadCoordsY[bar][i].size(); j++) {
							if (allNoteHeadCoordsY[bar][i][j] - (yPos-textHalfHeight) < halfStaffDist) {
								yPos += halfStaffDist;
							}
						}
					}
					// adding textHalfHeight to compensate for the height of the string which is not added to the actual allOttavasYCoords[bar][i]
					if (yPos + textHalfHeight > allNotesMaxYPos[bar][i] && yPos < FLT_MAX) {
						allNotesMaxYPos[bar][i] = yPos; // + textHalfHeight;
					}
				}
				allOttavasYCoords[bar][i] = yPos;
			}
		}
		else if (i > 0) {
			allOttavasChangedAt[bar][i] = i;
		}
		prevOttava = allOttavas[bar][i];
	}
	// each ottava line spans the notes from an ottava change up to the note before the next change
	// its Y coord must clear all of these notes, not only the first one, and it's the same for all of them
	// the first note's Y coord is set above, so here we check the rest of the notes, and store the line as part of them
	unsigned segmentStart = 0;
	while (segmentStart < allOttavas[bar].size()) {
		int ottava = allOttavas[bar][segmentStart];
		if (ottava == 0) {
			segmentStart++;
			continue;
		}
		unsigned segmentEnd = segmentStart;
		while (segmentEnd + 1 < allOttavas[bar].size() && allOttavas[bar][segmentEnd + 1] == ottava) segmentEnd++;
		float textHalfHeight = notationFont->height(octaveSyms[abs(ottava)]) / 2;
		float y = allOttavasYCoords[bar][segmentStart];
		for (unsigned i = segmentStart; i <= segmentEnd; i++) {
			if (ottava > 0) {
				// above the highest point of the note (and above the staff), as for the first note above
				y = std::min(y, std::min(allNotesMinYPos[bar][i], 0.0f) - staffDist);
			}
			else {
				y = std::max(y, std::max(allNotesMaxYPos[bar][i], halfStaffDist * 9) + staffDist);
			}
		}
		for (unsigned i = segmentStart; i <= segmentEnd; i++) {
			allOttavasYCoords[bar][i] = y;
			if (ottava > 0) {
				if (y - textHalfHeight < allNotesMinYPos[bar][i]) allNotesMinYPos[bar][i] = y - textHalfHeight;
			}
			else {
				if (y + textHalfHeight > allNotesMaxYPos[bar][i]) allNotesMaxYPos[bar][i] = y + textHalfHeight;
			}
		}
		segmentStart = segmentEnd + 1;
	}
}

//--------------------------------------------------------------
void Notes::storeTextCoords(int bar)
{
	//size_t size1 = 0;
	//size_t size2;
	unsigned i, j;
	std::vector<std::vector<float>> v;
	for (i = 0; i < allTextsIndexes.at(bar).size(); i++) {
		v.push_back({0.0});
		for (j = 1; j < allTextsIndexes.at(bar).at(i).size(); j++) {
			v.back().push_back(0.0);
		}
		//if (allTextsIndexes[bar][i] != 0) {
		//	size++;
		//}
	}
	//allTextsXCoords[bar] = std::vector<float>(size);
	//allTextsYCoords[bar] = std::vector<float>(size);
	allTextsYCoords[bar] = v;
	allTextsXCoords[bar] = std::vector<float>(allTextsIndexes.at(bar).size(), (float)0.0);
	int index = 0;
	for (i = 0; i < allTextsIndexes.at(bar).size(); i++) {
		for (j = 0; j < allTextsIndexes.at(bar).at(i).size(); j++) {
			if (allTextsIndexes.at(bar).at(i).at(j) != 0) {
				allTextsXCoords.at(bar).at(i) = allNoteCoordsX.at(bar).at(i).at(allChordsBaseIndexes.at(bar).at(i))-(noteWidth/2.0);
				float yPos;
				float textHalfHeight = textFont->height(allTexts[bar][index]) / 2;
				if (allTextsIndexes.at(bar).at(i).at(j) > 0) {
					allTextsYCoords.at(bar).at(i).at(j) = 0;
					if (stemDirections.at(bar).at(i) > 0) {
						yPos = std::min(allNotesMinYPos[bar][i], std::min(allNoteStemCoordsY[bar][i], allTextsYCoords.at(bar).at(i).at(j)));
					}

					else {
						yPos = std::min(allNotesMinYPos[bar][i], std::min(allNoteHeadCoordsY[bar][i][0], allTextsYCoords.at(bar).at(i).at(j)));
					}
					yPos -= (noteHeight + halfStaffDist);
					allTextsYCoords.at(bar).at(i).at(j) = yPos;
					if (yPos - textHalfHeight < allNotesMinYPos[bar][i] && yPos > -FLT_MAX) {
						allNotesMinYPos[bar][i] = yPos; // - textHalfHeight;
					}
				}
				else {
					allTextsYCoords.at(bar).at(i).at(j) = halfStaffDist*9;
					if (stemDirections[bar][i] > 0) {
						yPos =  std::max(allNotesMaxYPos[bar][i], std::max(allNoteHeadCoordsY[bar][i][0], allTextsYCoords.at(bar).at(i).at(j)));
					}
					else {
						yPos =  std::max(allNotesMaxYPos[bar][i], std::max(allNoteStemCoordsY[bar][i], allTextsYCoords.at(bar).at(i).at(j)));
					}
					yPos += (noteHeight + halfStaffDist);
					allTextsYCoords.at(bar).at(i).at(j) = yPos;
					if (yPos + textHalfHeight > allNotesMaxYPos[bar][i] && yPos < FLT_MAX) {
						allNotesMaxYPos[bar][i] = yPos; // + textHalfHeight;
					}
				}
				index++;
			}
		}
	}
}

//--------------------------------------------------------------
float Notes::lowestPointOfNote(int bar, int noteNdx, bool includeTie)
{
	// allNotesMaxYPos includes the center of the lowest note head, stems, beams, articulations, slurs, ottavas, and text
	// but not the lower half of the note heads, the accidentals, and the ties, which are drawn below the note heads
	// this returns the lowest point of these, so that elements drawn below the note (e.g. crescendi) don't overlap them
	float lowest = -FLT_MAX;
	if (noteNdx < 0 || noteNdx >= (int)allNotes[bar].size()) return lowest;
	float lowestHeadY = -FLT_MAX;
	for (unsigned j = 0; j < allNotes[bar][noteNdx].size(); j++) {
		// rests
		if (allNotes[bar][noteNdx][j] < 0) continue;
		float headY = allNoteHeadCoordsY[bar][noteNdx][j];
		if (headY >= FLT_MAX || headY <= -FLT_MAX) continue;
		lowestHeadY = std::max(lowestHeadY, headY);
		lowest = std::max(lowest, headY + (noteHeight / 2.0f));
		// accidentals are drawn with their baseline at the center of the note head, see drawAccidentals()
		int accidentalNdx = -1;
		if (isDrawnAccidental(allAccidentals[bar][noteNdx][j])) {
			accidentalNdx = allAccidentals[bar][noteNdx][j];
		}
		else if (naturalSignsNotWritten[bar][noteNdx][j]) {
			accidentalNdx = 4;
		}
		if (accidentalNdx > -1) {
			Rect rect = notationFont->bounds(accidentalSizeSym(accidentalNdx), 0, headY);
			lowest = std::max(lowest, rect.y + rect.height);
		}
	}
	// a tie of a note with the stem up is drawn below the note head, and curves down by up to 3/4 of halfStaffDist
	// see drawTie()
	if (includeTie && lowestHeadY > -FLT_MAX && noteNdx < (int)tieIndexes[bar].size() && tieIndexes[bar][noteNdx] > -1 && stemDirections[bar][noteNdx] > 0) {
		lowest = std::max(lowest, lowestHeadY + (noteHeight / 2.0f) + (halfStaffDist * 0.75f));
	}
	return lowest;
}

//--------------------------------------------------------------
void Notes::storeDynamicsCoords(int bar)
{
	dynsXCoords[bar] = std::vector<float>(dynamicsIndexes[bar].size(), 0);
	dynsYCoords[bar] = std::vector<float>(dynamicsIndexes[bar].size(), 0);
	dynsRampStartXCoords[bar] = std::vector<float>(dynamicsRampStart[bar].size(), 0);
	dynsRampStartYCoords[bar] = std::vector<float>(dynamicsRampStart[bar].size(), 0);
	dynsRampEndXCoords[bar] = std::vector<float>(dynamicsRampEnd[bar].size(), 0);
	dynsRampEndYCoords[bar] = std::vector<float>(dynamicsRampEnd[bar].size(), 0);
	// find the X Y coordinate for dynamics symbols
	for (unsigned i = 0; i < dynamicsIndexes[bar].size(); i++) {
		// cast to int to avoid cast in if test below
		for (int j = 0; j < (int)allNotesMaxYPos[bar].size(); j++) {
			if (dynamicsIndexes[bar][i] == j) {
				float y = std::max((staffDist*4), allNotesMaxYPos[bar][j]);
				y += (notationFont->height(dynSyms[dynamics[bar][i]]) * 1.2); // give a bit of room with 1.2
				if (rhythm) y -= (staffDist * 2);
				if (y > allNotesMaxYPos[bar][j]) allNotesMaxYPos[bar][j] = y;
				dynsXCoords[bar][i] = allNoteCoordsX[bar][j][allChordsBaseIndexes[bar][j]]-dynSymsWidths[dynamics[bar][i]]/2;
				dynsYCoords[bar][i] = y;
			}
		}
	}
	// then find the X Y coordinates for crescendi and decrescendi
	for (unsigned i = 0; i < dynamicsRampStart[bar].size(); i++) {
		// cast to int to avoid cast in if test below
		for (int j = 0; j < (int)allNotesMaxYPos[bar].size(); j++) {
			if (dynamicsRampStart[bar][i] == j) {
				float y = std::max((staffDist*4), allNotesMaxYPos[bar][j]);
				if (y > allNotesMaxYPos[bar][j]) allNotesMaxYPos[bar][j] = y;
				dynsRampStartXCoords[bar][i] = allNoteCoordsX[bar][j][allChordsBaseIndexes[bar][j]];
				dynsRampStartYCoords[bar][i] = y;
			}
		}
	}
	for (unsigned i = 0; i < dynamicsRampEnd[bar].size(); i++) {
		// cast to int to avoid cast in if test below
		for (int j = 0; j < (int)allNotesMaxYPos[bar].size(); j++) {
			if (dynamicsRampEnd[bar][i] == j) {
				float y = std::max((staffDist*4), allNotesMaxYPos[bar][j]);
				if (y > allNotesMaxYPos[bar][j]) allNotesMaxYPos[bar][j] = y;
				dynsRampEndXCoords[bar][i] = allNoteCoordsX[bar][j][allChordsBaseIndexes[bar][j]];
				dynsRampEndYCoords[bar][i] = y;
			}
		}
	}
	// check if a crescendo or decrescendo starts or ends at a dynamic
	for (unsigned i = 0; i < dynamicsRampStart[bar].size(); i++) {
		if (dynamicsRampStart[bar][i] > -1) {
			for (unsigned j = 0; j < dynamicsIndexes[bar].size(); j++) {
				if (dynamicsIndexes[bar][j] > -1) {
					if (dynamicsRampStart[bar][i] == dynamicsIndexes[bar][j]) {
						// if we start at a dynamic symbol add half the width of that symbol to the (de)crescendo start
						dynsRampStartXCoords[bar][i] += dynSymsWidths[dynamics[bar][j]]/2;
						// and allign ramp start with the dynamic symbol on the Y axis
						dynsRampStartYCoords[bar][i] = dynsYCoords[bar][j];
						// find the next index with a dynamic
						unsigned nextDynNdx;
						for (nextDynNdx = j + 1; nextDynNdx < dynamicsIndexes[bar].size(); nextDynNdx++) {
							if (dynamicsIndexes[bar][nextDynNdx] > -1) break;
						}
						// if no other dynamic is found after the end of the current one
						// nextDynNdx will be one greater than the last element of dynamicsIndexes[bar]
						if (nextDynNdx >= dynamicsIndexes[bar].size()) nextDynNdx = dynamicsIndexes[bar].size() - 1;
						// find the next index of a ramp end
						unsigned nextRampEndNdx;
						for (nextRampEndNdx = i + 1; nextRampEndNdx < dynamicsRampEnd[bar].size(); nextRampEndNdx++) {
							if (dynamicsRampEnd[bar][nextRampEndNdx] > -1) break;
						}
						// same goes for the ramp end index
						if (nextRampEndNdx >= dynamicsRampEnd[bar].size()) nextRampEndNdx = dynamicsRampEnd[bar].size() - 1;
						if (j < dynamicsIndexes[bar].size()-1) {
							// run through all the notes in between to make sure the ramp won't fall
							// on one of these notes or an articulation sign
							for (int k = dynamicsIndexes[bar][j]; k < dynamicsIndexes[bar][nextDynNdx]; k++) {
								dynsRampStartYCoords[bar][i] = std::max(dynsRampStartYCoords[bar][i], allNotesMaxYPos[bar][k]);
								if (dynsRampStartYCoords[bar][i] > allNotesMaxYPos[bar][k]) {
									allNotesMaxYPos[bar][k] = dynsRampStartYCoords[bar][i];
								}
							}
							// check if the currect ramp ends at the next dynamic
							if (nextRampEndNdx == nextDynNdx) {
								dynsRampEndXCoords[bar][nextRampEndNdx] -= dynSymsWidths[dynamics[bar][nextDynNdx]]/2.0;
								dynsRampStartYCoords[bar][i] = std::max(dynsRampStartYCoords[bar][i], dynsYCoords[bar][nextDynNdx]);
								dynsRampEndYCoords[bar][nextRampEndNdx] = std::max(dynsRampStartYCoords[bar][i], dynsRampEndYCoords[bar][nextRampEndNdx]);
								for (int k = dynamicsIndexes[bar][j]; k < dynamicsIndexes[bar][nextDynNdx]; k++) {
									if (dynsRampEndYCoords[bar][nextRampEndNdx] > allNotesMaxYPos[bar][k]) {
										allNotesMaxYPos[bar][k] = dynsRampEndYCoords[bar][nextRampEndNdx];
									}
								}
								// allign the two dynamics as well
								dynsYCoords[bar][j] = dynsYCoords[bar][nextDynNdx] = std::max(dynsYCoords[bar][j], dynsYCoords[bar][nextDynNdx]);
								for (int k = dynamicsIndexes[bar][j]; k < dynamicsIndexes[bar][nextDynNdx]; k++) {
									if (dynsYCoords[bar][i] > allNotesMaxYPos[bar][k]) {
										allNotesMaxYPos[bar][k] = dynsYCoords[bar][i];
									}
								}
							}
							// otherwise set the start and end of the dynamic to the same Y coord
							else {
								dynsRampStartYCoords[bar][i] = dynsRampEndYCoords[bar][nextRampEndNdx] = std::max(dynsRampStartYCoords[bar][i], dynsRampEndYCoords[bar][nextRampEndNdx]);
								for (int k = dynamicsIndexes[bar][j]; k < dynamicsIndexes[bar][nextDynNdx]; k++) {
									if (dynsRampStartYCoords[bar][i] > allNotesMaxYPos[bar][k]) {
										allNotesMaxYPos[bar][k] = dynsRampStartYCoords[bar][i];
									}
								}
							}
						}
						// if a ramp ends at the end of the bar and there are no more dynamics
						else {
							dynsRampStartYCoords[bar][i] = dynsRampEndYCoords[bar][nextRampEndNdx] = std::max(dynsRampStartYCoords[bar][i], dynsRampEndYCoords[bar][nextRampEndNdx]);
							if (j < dynamicsIndexes[bar].size()-1) {
								for (int k = dynamicsIndexes[bar][j]; k < dynamicsIndexes[bar][nextDynNdx]; k++) {
									if (dynsRampStartYCoords[bar][i] > allNotesMaxYPos[bar][k]) {
										allNotesMaxYPos[bar][k] = dynsRampStartYCoords[bar][i];
									}
								}
							}
						}
					}
				}
			}
		}
	}
	// check if a ramp starts without a dynamic sign, but ends at a dynamic sign
	for (unsigned i = 0; i < dynamicsRampEnd[bar].size(); i++) {
		if (dynamicsRampEnd[bar][i] > -1) {
			for (unsigned j = dynamicsRampEnd[bar][i]; j < dynamicsIndexes[bar].size(); j++) {
				if (dynamicsIndexes[bar][j] > -1) {
					// find the previous and next index with a dynamic
					// these are ints, as there might be no previous dynamic (-1)
					// with unsigned indexes, the loop would go below 0 and read outside the vector
					int prevDynNdx = -1;
					for (int k = (int)j - 1; k >= 0; k--) {
						if (dynamicsIndexes[bar][k] > -1) {
							prevDynNdx = k;
							break;
						}
					}
					unsigned nextDynNdx;
					for (nextDynNdx = j + 1; nextDynNdx < dynamicsIndexes[bar].size(); nextDynNdx++) {
						if (dynamicsIndexes[bar][nextDynNdx] > -1) break;
					}
					// if no other dynamic is found after the end of the current one
					// nextDynNdx will be one greater than the last element of dynamicsIndexes[bar]
					if (nextDynNdx >= dynamicsIndexes[bar].size()) nextDynNdx = dynamicsIndexes[bar].size() - 1;
					// find the previous start of a dynamic ramp (-1 if there is none)
					int prevRampStartNdx = -1;
					for (int k = (int)i - 1; k >= 0; k--) {
						// a ramp can't start and end on the same note, so a start on this note belongs to the next ramp
						if (dynamicsRampStart[bar][k] > -1) {
							prevRampStartNdx = k;
							break;
						}
					}
					if (dynamicsRampEnd[bar][i] == dynamicsIndexes[bar][j]) {
						dynsRampEndXCoords[bar][i] -= dynSymsWidths[dynamics[bar][j]]/2.0;
						// if the ramp doesn't start from a dynamic sign, align it with the dynamic sign it ends at
						// if there is no previous dynamic sign, the ramp definitely doesn't start from one
						bool rampStartsAtDynamic = (prevRampStartNdx > -1 && prevDynNdx > -1 && dynamicsRampStart[bar][prevRampStartNdx] == dynamicsIndexes[bar][prevDynNdx]);
						if (prevRampStartNdx > -1 && !rampStartsAtDynamic) {
							dynsRampEndYCoords[bar][i] = std::max(dynsRampEndYCoords[bar][i], dynsYCoords[bar][j]);
							dynsRampStartYCoords[bar][prevRampStartNdx] = dynsRampEndYCoords[bar][i];
							if (j < dynamicsIndexes[bar].size()-1) {
								for (int k = dynamicsIndexes[bar][j]; k < dynamicsIndexes[bar][nextDynNdx]; k++) {
									if (dynsRampStartYCoords[bar][prevRampStartNdx] > allNotesMaxYPos[bar][k]) {
										allNotesMaxYPos[bar][k] = dynsRampStartYCoords[bar][prevRampStartNdx];
									}
								}
							}
						}
					}
				}
			}
		}
	}
	// place the crescendi and decrescendi
	// the coordinates above are those of the start and end notes only, so the two ends of a ramp can be at different heights
	// and the notes in between are not taken into account, so a ramp can be drawn on top of a lower note
	// also, a ramp is drawn centered at its Y coordinate, extending staffDist above and below it
	// so with its Y at the lowest point of a note (or at the bottom line of the staff) it overlaps it
	// so here we put both ends of the ramp at the same height, below the lowest point of all the notes it spans
	// leaving half a staff space between them and the top of the ramp
	auto isDynamicAt = [this, bar](int noteNdx) {
		for (unsigned k = 0; k < dynamicsIndexes[bar].size(); k++) {
			if (dynamicsIndexes[bar][k] == noteNdx && dynamics[bar][k] > -1) return true;
		}
		return false;
	};
	// the same as for the dynamics symbols above
	float staffBottom = (rhythm ? staffDist * 2 : staffDist * 4);
	for (unsigned i = 0; i < dynamicsRampStart[bar].size(); i++) {
		if (dynamicsRampStart[bar][i] < 0) continue;
		// the ramp ends at the first ramp end from its start on, as in drawDynamics()
		unsigned endNdx = i;
		bool foundEnd = false;
		// (a ramp can't start and end on the same note, so an end on its first note belongs to the previous ramp)
		for (unsigned j = i + 1; j < dynamicsRampEnd[bar].size(); j++) {
			if (dynamicsRampEnd[bar][j] > -1) {
				endNdx = j;
				foundEnd = true;
				break;
			}
		}
		if (!foundEnd) continue;
		int startNote = dynamicsRampStart[bar][i];
		int endNote = dynamicsRampEnd[bar][endNdx];
		float lowestY = staffBottom;
		for (int k = startNote; k <= endNote && k < (int)allNotesMaxYPos[bar].size(); k++) {
			if (allNotesMaxYPos[bar][k] > lowestY && allNotesMaxYPos[bar][k] < FLT_MAX) lowestY = allNotesMaxYPos[bar][k];
			lowestY = std::max(lowestY, lowestPointOfNote(bar, k, k < endNote));
		}
		float rampY = lowestY + halfStaffDist + staffDist;
		// ramps that start or end at a dynamic symbol are aligned with it above, unless the notes need them lower
		// both ends of a ramp always get the same Y coord
		if (isDynamicAt(startNote) || isDynamicAt(endNote)) {
			rampY = std::max(rampY, std::max(dynsRampStartYCoords[bar][i], dynsRampEndYCoords[bar][endNdx]));
		}
		dynsRampStartYCoords[bar][i] = dynsRampEndYCoords[bar][endNdx] = rampY;
		// store the bottom of the ramp, so that the height of the bar includes it
		for (int k = startNote; k <= endNote && k < (int)allNotesMaxYPos[bar].size(); k++) {
			if (rampY + staffDist > allNotesMaxYPos[bar][k]) allNotesMaxYPos[bar][k] = rampY + staffDist;
		}
	}
}

//--------------------------------------------------------------
void Notes::storeMinMaxY(int bar)
{
	for (unsigned i = 0; i < allNotes[bar].size(); i++) {
		// compare allNotesMaxYPos[bar][i] against FLT_MAX to filter it out
		// we have an eight rest on the downbeat
		if (allNotesMaxYPos[bar][i] > maxYCoord[bar] && allNotesMaxYPos[bar][i] < FLT_MAX && !tacet[bar])
			maxYCoord[bar] = allNotesMaxYPos[bar][i];
		if (allNotesMinYPos[bar][i] < minYCoord[bar] && allNotesMinYPos[bar][i] > -FLT_MAX && !tacet[bar])
			minYCoord[bar] = allNotesMinYPos[bar][i];
	}
	if (maxYCoord[bar] < (staffDist*4) && !tacet[bar]) {
		maxYCoord[bar] = (staffDist*4);
	}
	if (minYCoord[bar] > 0 && !tacet[bar]) minYCoord[bar] = 0;
}

//--------------------------------------------------------------
void Notes::deleteBar(int bar)
{
	allNotes.erase(bar);
}

//--------------------------------------------------------------
void Notes::insertNaturalSigns(int bar, int loopNdx, std::vector<int> *v)
{
	// create an array of booleans where the index is the note and the value
	// determines whether this note has already been corrected
	int notesAlreadyCorrected[7] = {0};
	// iterate backwards through the bars already displayed to see if we need to add a natural sign
	for (int i = loopNdx-1; i >= 0; i--) {
		for (int j = (int)allNotes[v->at(i)].size()-1; j >= 0; j--) {
			for (int k = (int)allNotes[v->at(i)][j].size()-1; k >= 0; k--) {
				// once we access a note from a previous bar, we iterate over this bar to see if we have the same note
				for (unsigned l = 0; l < allNotes[bar].size(); l++) {
					for (unsigned m = 0; m < allNotes[bar][l].size(); m++) {
						if (allNotes[v->at(i)][j][k] == allNotes[bar][l][m] && \
								allAccidentals[v->at(i)][j][k] != allAccidentals[bar][l][m] &&
								(allAccidentals[bar][l][m] == -1 || allAccidentals[bar][l][m] != 4)) {
							if (!notesAlreadyCorrected[allNotes[bar][l][m]]) {
								// the boolean below will be set to false once this additional natural sign is drawn
								naturalSignsNotWritten[bar][l][m] = 1;
								notesAlreadyCorrected[allNotes[bar][l][m]] = 1;
							}
						}
					}
				}
			}
		}
	}
}

//--------------------------------------------------------------
std::pair<int, int> Notes::isBarLinked(int bar)
{
	return isLinked[bar];
}

//--------------------------------------------------------------
void Notes::moveScoreX(int numPixels)
{
	xStartOffset += (float)numPixels;
}

//--------------------------------------------------------------
void Notes::moveScoreY(int numPixels)
{
	yStartOffset += (float)numPixels;
}

//--------------------------------------------------------------
void Notes::recenterScore()
{
	xStartOffset = yStartOffset = 0;
}

//--------------------------------------------------------------
void Notes::setDynamicsRampOpenAtEnd(int bar, bool open)
{
	dynamicsRampOpenAtEnd[bar] = open;
}

//--------------------------------------------------------------
void Notes::drawNotes(int bar, int loopNdx, std::vector<int> *v, float xStartPnt, float yStartPnt, float yOffset, bool animation, float xCoef, int nextBar)
{
	if (allNotes.find(bar) == allNotes.end()) return;
	// a bar whose note positions have not been calculated yet (see setNotePositions()) can't be drawn
	// this shouldn't happen, but skipping the bar is better than a crash during a performance
	if (hasExtraLines.find(bar) == hasExtraLines.end()) return;
	cv().setColor(atBrightness(notesColor, cv().brightness));
	float xCoefLocal = xCoef;
	//if (scoreOrientation > 0) xCoefLocal = 1;
	// each bar must store its xStartPnt dynamically when drawn
	// so this can be used in linked items, like slurs, glissandi and crescendi that span over one bar
	xStartPnt += xStartOffset;
	xOffsets[bar] = xStartPnt;
	if (!mute) {
		// first determine if there is any natural sign that needs to be inserted
		// the second argument is the index within the loop that displays the bars horizontally
		// the third is a pointer to the std::vector that holds the bar indexes (map keys) of the currently playing loop
		if (loopNdx > 0) insertNaturalSigns(bar, loopNdx, v);
		for (unsigned i = 0; i < allNotes.at(bar).size(); i++) {
			Color restsColor = notesColor;
			for (unsigned j = 0; j < allNotes.at(bar).at(i).size(); j++) {
				if (hasExtraLines.at(bar).at(i).at(j)) {
					float xStartLocal = xStartPnt + (allNoteCoordsX.at(bar).at(i).at(j) * xCoefLocal);
					float yPos = yStartPnt + extraLinesYPos.at(bar).at(i).at(j) + yOffset;
					// draw the extra lines first so the animation is drawn on top
					for (int k = 0; k < abs(numExtraLines.at(bar).at(i).at(j)); k++) {
						yPos += (staffDist * extraLinesDir.at(bar).at(i).at(j));
						cv().line(xStartLocal - (noteWidth*EXTRALINECOEFF), yPos, xStartLocal + (noteWidth*EXTRALINECOEFF), yPos);
					}
				}
				if (j == 0) {
					// then set the animation color
					if (animation && animate && whichNote == (int)i) {
						cv().setColor(opaqueAtBrightness(activeNotesColor, cv().brightness));
						restsColor = activeNotesColor;
					}
				}
				// then draw all the notes heads and rests
				if (allNotes.at(bar).at(i).at(j) > -1) {
					int noteIdx = (actualDurs.at(bar).at(i) < 4 ? actualDurs.at(bar).at(i) - 1 : 2);
					float x = xStartPnt + (allNoteCoordsX.at(bar).at(i).at(j) * xCoefLocal);
					//if (allNoteShiftX.at(bar).at(i).at(j)) x += noteWidth;
					float y = yStartPnt + allNoteHeadCoordsY.at(bar).at(i).at(j)+(noteHeight/8.0)-(staffDist/10.0) + yOffset;
					notationFont->draw(notesSyms[noteIdx], x-(noteWidth/2.0), y);
					if (dotIndexes.at(bar).at(i) > 0) {
						// if the note is on a line or we have a rest
						if (((int)(((y+yOffset) - yStartPnt) / halfStaffDist) % 2) == scoreOrientationForDots) {
							// add a small offset to the dot
							y -= halfStaffDist;
						}
						for (unsigned j = 0; j < dotsCounter.at(bar).at(i); j++) {
							cv().circle(x+(noteWidth/1.5), y, staffDist*0.2);
							x += (staffDist * 0.5);
						}
					}
				}
			}
			if (allNotes.at(bar).at(i).at(0) == -1) {
				float x = xStartPnt + (allNoteCoordsX.at(bar).at(i)[allChordsBaseIndexes.at(bar).at(i)] * xCoefLocal);
				int restIndex = drawRest(bar, actualDurs.at(bar).at(i), x, yStartPnt, restsColor, yOffset);
				if (dotIndexes.at(bar).at(i) == 1) {
					for (unsigned j = 0; j < dotsCounter.at(bar).at(i); j++) {
						cv().circle(x+(notationFont->width(restsSyms[restIndex])*1.2), middleOfStaff+yStartPnt+yOffset-halfStaffDist, staffDist*0.2);
						x += (staffDist * 0.5);
					}
				}
			}
			// then draw the stems
			if (hasStem.at(bar).at(i)) {
				float x = xStartPnt + ((allNoteCoordsX.at(bar).at(i)[allChordsBaseIndexes.at(bar).at(i)] + (allNoteCoordsXOffset.at(bar).at(i) * allChordsChangeXCoef.at(bar).at(i))) * xCoefLocal);
				if (stemDirections.at(bar).at(i) == 1) x += ((noteWidth / 2.0) - lineWidth);
				else x -= ((noteWidth / 2.0) - (lineWidth / 2.0));
				float y1 = yStartPnt + allNoteHeadCoordsY.at(bar).at(i)[allChordsBaseIndexes.at(bar).at(i)]+(noteHeight/8.0)-(staffDist/10.0) + yOffset;
				float y2 = allNoteStemCoordsY.at(bar).at(i) + yStartPnt + yOffset;
				cv().line(x, y1, x, y2);
			}
			// then draw the individual tails
			int numTailsLocal = log(actualDurs.at(bar).at(i)) / log(2) - 2;
			if (actualDurs.at(bar).at(i) > 4 && !isNoteGroupped.at(bar).at(i) && numTailsLocal != 0 && allNotes.at(bar).at(i).at(0) > -1) {
				int symIdx = 3;
				int tailDir = 1;
				float xPos = xStartPnt + ((allNoteCoordsX.at(bar).at(i)[allChordsBaseIndexes.at(bar).at(i)] + (allNoteCoordsXOffset.at(bar).at(i) * allChordsChangeXCoef.at(bar).at(i))) * xCoefLocal);
				if (stemDirections.at(bar).at(i) == 1) xPos += ((noteWidth / 2.0) - lineWidth);
				else xPos -= ((noteWidth / 2.0) - (lineWidth / 2.0));
				float yPos = allNoteStemCoordsY.at(bar).at(i) + yStartPnt + yOffset;
				// minor corrections on the tails coordinates
				if (stemDirections.at(bar).at(i) < 0) {
					symIdx = 4;
					tailDir = -1;
					yPos -= (staffDist*3);
				}
				else {
					xPos -= (noteWidth - lineWidth);
					yPos += (staffDist*3);
				}
				int loopIter = abs(numTailsLocal);
				for (int j = 0; j < loopIter; j++) {
					notationFont->draw(notesSyms[symIdx], xPos, yPos+((j*(staffDist*BEAMDISTCOEFF))*tailDir));
				}
			}
			// reset the animation color
			if (animation && animate && whichNote == (int)i) {
				cv().setColor(opaqueAtBrightness(notesColor, cv().brightness));
			}
		}
		// then draw the beams
		if (beamsIndexes.at(bar).size() > 0) {
			for (int i = 0; i < (int)beamsIndexes.at(bar).size(); i++) {
				// don't draw beams for single notes that are not groupped
				if (numNotesWithBeams.at(bar).at(i) > 1) {
					for (int j = beamsIndexes.at(bar).at(i).at(0); j < beamsIndexes.at(bar).at(i).at(1); j++) {
						// if there is a rest at the beginning or the end of the beam group, don't draw beams
						if (j == beamsIndexes.at(bar).at(i).at(0) && allNotes.at(bar).at(j).at(0) == -1) continue;
						if (j == beamsIndexes.at(bar).at(i).at(1)-1 && allNotes.at(bar).at(j+1).at(0) == -1) continue;
						// it's possible to group a quarter note or longer with beamed notes
						// (e.g. two sixteenths with a quarter and an eighth note)
						// in which case we don't want to draw beams for these notes
						if (actualDurs.at(bar).at(j) < 8) {
							continue;
						}
						std::vector<float> xCoords(3);
						std::vector<float> yCoords(3);
						// the X coordinates of the two stems the beam connects, calculated the same way as the stems above
						// the chord offsets are those of each note (j and j+1), not of the beam group (i)
						xCoords.at(0) = xStartPnt + ((allNoteCoordsX.at(bar).at(j)[allChordsBaseIndexes.at(bar).at(j)] + (allNoteCoordsXOffset.at(bar).at(j) * allChordsChangeXCoef.at(bar).at(j))) * xCoefLocal);
						xCoords.at(2) = xStartPnt + ((allNoteCoordsX.at(bar).at(j+1)[allChordsBaseIndexes.at(bar).at(j+1)] + (allNoteCoordsXOffset.at(bar).at(j+1) * allChordsChangeXCoef.at(bar).at(j+1))) * xCoefLocal);
						yCoords.at(0) = allNoteStemCoordsY.at(bar).at(j) + yStartPnt + yOffset;
						yCoords.at(2) = allNoteStemCoordsY.at(bar).at(j+1) + yStartPnt + yOffset;
						if (j == beamsIndexes.at(bar).at(i).at(0)) xCoords.at(0) -= (lineWidth/2);
						if (j < beamsIndexes.at(bar).at(i).at(1)) xCoords.at(2) += (lineWidth/2);
						// stems up
						if (grouppedStemDirs.at(bar).at(i) == 1) {
							xCoords.at(0) += ((noteWidth / 2.0) - lineWidth);
							xCoords.at(2) += ((noteWidth / 2.0) - lineWidth);
							yCoords.at(0) += (beamsLineWidth / 2);
							yCoords.at(2) += (beamsLineWidth / 2);
						}
						// stems down
						else {
							xCoords.at(0) -= ((noteWidth / 2.0) - (lineWidth / 2.0));
							xCoords.at(2) -= ((noteWidth / 2.0) - (lineWidth / 2.0));
							yCoords.at(0) -= (beamsLineWidth / 2);
							yCoords.at(2) -= (beamsLineWidth / 2);
						}
						xCoords.at(1) = xCoords.at(0) + ((xCoords.at(2) - xCoords.at(0)) / 2.0);
						if (yCoords.at(0) > yCoords.at(2)) yCoords.at(1) = yCoords.at(2) + ((yCoords.at(0) - yCoords.at(2)) / 2.0);
						else yCoords.at(1) = yCoords.at(0) + ((yCoords.at(2) - yCoords.at(0)) / 2.0);
						// split the drawing of beams to two per connected pair
						// so we can draw separate number of extra beams if for example an eight note
						// is connected to two sixtennths, where the first needs to draw one beam only
						// but the other two need to draw two beams
						for (int k = 0; k < 2; k++) {
							// draw the first beam
							drawThickLine(xCoords.at(k), yCoords.at(k), xCoords.at(k+1), yCoords.at(k+1));
							// separate variable for correct offsetting in the loop below
							float yCoordOffset1;
							float yCoordOffset2;
							// draw the rest (if any) with a loop
							// but not for rests, unless these are surrounded by notes
							int groupStart = beamsIndexes.at(bar).at(i).at(0);
							int groupEnd = beamsIndexes.at(bar).at(i).at(1);
							// whether a note carries a beam of a level (the first beam is level 0)
							// a rest carries a beam only if it is surrounded by notes that carry it
							auto hasBeamAtLevel = [&](int n, int level) {
								if (n < groupStart || n > groupEnd) return false;
								if (numBeams.at(bar).at(n) <= level) return false;
								if (allNotes.at(bar).at(n).at(0) > -1) return true;
								return (n > groupStart && n < groupEnd && numBeams.at(bar).at(n-1) > level && numBeams.at(bar).at(n+1) > level);
							};
							int self = j + k;
							int partner = (k == 0 ? j + 1 : j);
							for (int l = 1; l < numBeams.at(bar).at(self); l++) {
								// this half of the pair is drawn if both notes carry this beam (a full beam between them)
								// or as a partial beam, if this note carries it but none of its neighbours does
								// (e.g. a 16th after a dotted eighth), which points to the previous note
								// or to the next one if this is the first note of the group
								bool drawBeam = false;
								if (hasBeamAtLevel(self, l)) {
									if (hasBeamAtLevel(partner, l)) {
										drawBeam = true;
									}
									else if (k == 1) {
										drawBeam = !hasBeamAtLevel(self + 1, l);
									}
									else {
										drawBeam = (self == groupStart);
									}
								}
								if (drawBeam) {
									float levelOffset = (l*(staffDist*BEAMDISTCOEFF)) * grouppedStemDirs.at(bar).at(i);
									yCoordOffset1 = yCoords.at(k) + levelOffset;
									yCoordOffset2 = yCoords.at(k+1) + levelOffset;
									float xStart = xCoords.at(k), xEnd = xCoords.at(k+1);
									// a partial beam (of a note whose neighbours don't carry this beam) covers half the distance to its neighbour
									// (from its stem to the middle point between the two stems, which is what this half of the pair is)
									// but in a group of two notes, this is too long, so it covers a quarter of the distance between the two stems
									if (!hasBeamAtLevel(partner, l) && groupEnd - groupStart == 1) {
										const float partialBeamCoef = 0.25;
										float x0 = xCoords.at(0), x2 = xCoords.at(2);
										float y0 = yCoords.at(0) + levelOffset, y2 = yCoords.at(2) + levelOffset;
										if (k == 0) {
											xEnd = x0 + ((x2 - x0) * partialBeamCoef);
											yCoordOffset2 = y0 + ((y2 - y0) * partialBeamCoef);
										}
										else {
											xStart = x2 - ((x2 - x0) * partialBeamCoef);
											yCoordOffset1 = y2 - ((y2 - y0) * partialBeamCoef);
										}
									}
									drawThickLine(xStart, yCoordOffset1, xEnd, yCoordOffset2);
								}
							}
						}
					}
				}
			}
		}
		drawAccidentals(bar, xStartPnt, yStartPnt, yOffset, xCoef);
		drawGlissandi(bar, xStartPnt, yStartPnt, yOffset, xCoef, nextBar);
		drawTuplets(bar, xStartPnt, yStartPnt, yOffset, xCoef);
		drawArticulations(bar, xStartPnt, yStartPnt, yOffset, xCoef);
		drawOttavas(bar, xStartPnt, yStartPnt, yOffset, xCoef);
		drawText(bar, xStartPnt, yStartPnt, yOffset, xCoef);
		drawSlurs(bar, loopNdx, xStartPnt, yStartPnt, yOffset, xCoef, nextBar);
		drawTies(bar, loopNdx, xStartPnt, yStartPnt, yOffset, xCoef);
		drawDynamics(bar, xStartPnt, yStartPnt, yOffset, xCoef);
	}
	else {
		// a muted instrument shows a whole rest in the middle of the bar, where a bar with r1 has it (see setNotePositions())
		// in the color of the notes that play while the bar plays (it was 255, white, which is not visible on the score)
		Color restColor = notesColor;
		float xPos = xStartPnt + ((xLength / 2) * xCoefLocal);
		if (animation && animate) restColor = activeNotesColor;
		drawRest(bar, 1, xPos, yStartPnt, restColor, yOffset);
	}
}

//--------------------------------------------------------------
int Notes::drawRest(int bar, int restDur, float x, float yStartPnt, Color color, float yOffset)
{
	cv().setColor(atBrightness(color, cv().brightness));
	// the rests of a voice of a staff with merged instruments move one staff space up (stems up) or down (stems down)
	yStartPnt -= (forcedStemDir * staffDist);
	int indexLocal;
	if (restDur < 4) {
		indexLocal = 0;
		float y = yStartPnt + staffDist + halfStaffDist+((staffDist/2.0)*(restDur-1)) + yOffset;
		if (rhythm && restDur == 1) y += staffDist;
		notationFont->draw(restsSyms[indexLocal], x-(noteHeight/2), y);
	}
	else {
		indexLocal = (log(restDur) / log(2)) - 1;
		// for an offset of 3 for the eigth rest, every other rest must go
		// 12 pixels lower, and for 4 pxs this offset is 13, so every
		// time it's plus 9
		float extraOffset;
		if (indexLocal == 1) {
			extraOffset = 12;
			if (restYOffset > 3) {
				// add 2 for every 1 increment of the restYOffset
				extraOffset += ((restYOffset-3) * 2);
			}
		}
		else {
			extraOffset = restYOffset + 9;
			extraOffset *= (indexLocal-2);
			extraOffset += halfStaffDist;
			extraOffset -= ((indexLocal-2)*(restYOffset-2)*3);
			// 32nds and 64ths should start at the top most space (below the top most line)
			// so we subtract one full space
			if (indexLocal > 3) extraOffset -= staffDist;
		}
		notationFont->draw(restsSyms[indexLocal], x-((noteHeight*1.5)/2.0),
				yStartPnt+staffDist+extraOffset+yOffset);
	}
	// we reset the color to notes color as the rest of the things we draw should have that color
	cv().setColor(opaqueAtBrightness(notesColor, cv().brightness));
	return indexLocal;
}

//--------------------------------------------------------------
void Notes::drawThickLine(float x1, float y1, float x2, float y2)
{
	// draw a thick line without affecting the thickness
	// of all the other lines
	// taken from
	// https://forum.openframeworks.cc/t/how-do-i-draw-lines-of-a-constraints/30815/7
	cv().thickLine(x1, y1, x2, y2, beamsLineWidth);
}

//--------------------------------------------------------------
bool Notes::isDrawnAccidental(int ndx)
{
	return ndx >= 0 && ndx <= 8;
}

//--------------------------------------------------------------
const std::string& Notes::accidentalSizeSym(int ndx)
{
	if (ndx == 1 || ndx == 3) return accSyms[2];
	if (ndx == 5 || ndx == 7) return accSyms[6];
	return accSyms[ndx];
}

//--------------------------------------------------------------
float Notes::accidentalWidth(int ndx)
{
	float width = notationFont->bounds(accidentalSizeSym(ndx), 0, 0).width;
	// the 1.5 flat is two flats, and the 1.5 sharp has wider horizontal bars than the sharp (see drawAccidental())
	if (ndx == 1) return width * 1.9;
	if (ndx == 7) return width * 1.3;
	return width;
}

//--------------------------------------------------------------
void Notes::drawAccidental(int ndx, float x, float y)
{
	if (!isDrawnAccidental(ndx)) return;
	if (!accSyms[ndx].empty()) {
		notationFont->draw(accSyms[ndx], x, y);
		return;
	}
	Rect rect = notationFont->bounds(accidentalSizeSym(ndx), x, y);
	if (ndx == 1 || ndx == 3) {
		// a flat drawn at drawX, mirrored on the vertical axis at its center, so it stays within the same rectangle
		auto drawMirroredFlat = [&](float drawX) {
			float centerX = rect.centerX() + (drawX - x);
			cv().push();
			cv().translate(centerX, 0);
			cv().scale(-1, 1);
			cv().translate(-centerX, 0);
			notationFont->draw(accSyms[2], drawX, y);
			cv().pop();
		};
		if (ndx == 3) {
			drawMirroredFlat(x);
		}
		else {
			// the 1.5 flat is a mirrored flat and a flat, with their stems next to each other
			// it ends where a flat would end, so the mirrored flat is to the left of the flat
			notationFont->draw(accSyms[2], x, y);
			drawMirroredFlat(x - (rect.width * 0.9));
		}
		return;
	}
	// the half sharp has one vertical stroke and the 1.5 sharp three, with the horizontal bars of a sharp
	// both have the height of a sharp and end where a sharp would end
	int numStrokes = (ndx == 5 ? 1 : 3);
	float barsWidth = (ndx == 5 ? rect.width * 0.75 : rect.width * 1.3);
	float right = rect.x + rect.width;
	float left = right - barsWidth;
	float strokeWidth = std::max(1.0f, staffDist * 0.12f);
	float barThickness = std::max(1.5f, staffDist * 0.4f);
	// the bars rise to the right, as in a sharp
	float slant = barsWidth * 0.25;
	for (int k = 0; k < numStrokes; k++) {
		float strokeX = left + (barsWidth * (k + 1) / (numStrokes + 1)) - (strokeWidth / 2.0);
		// the strokes are a little shorter than the sharp at their ends, so they don't stick out of it
		cv().rect(strokeX, rect.y + (rect.height * 0.04), strokeWidth, rect.height * 0.92);
	}
	for (int k = 0; k < 2; k++) {
		float barY = rect.y + (rect.height * (k == 0 ? 0.36 : 0.66));
		cv().beginShape();
		cv().vertex(left, barY + (slant / 2.0));
		cv().vertex(right, barY - (slant / 2.0));
		cv().vertex(right, barY - (slant / 2.0) + barThickness);
		cv().vertex(left, barY + (slant / 2.0) + barThickness);
		cv().endShape(true);
	}
}

//--------------------------------------------------------------
void Notes::drawAccidentals(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef)
{
	if (tacet[bar]) return;
	float xCoefLocal = xCoef;
	float prevX = FLT_MIN, prevY = FLT_MIN;
	//if (scoreOrientation > 0) xCoefLocal = 1;
	for (unsigned i = 0; i < allAccidentals[bar].size(); i++) {
		for (unsigned j = 0; j < allAccidentals[bar][i].size(); j++) {
			float x = allNoteCoordsX[bar][i][allChordsBaseIndexes[bar][i]];
			if (allNoteCoordsXOffset[bar][i] > 0) {
				x -= allNoteCoordsXOffset[bar][i];
			}
			float y = allNoteHeadCoordsY[bar][i][j];
			// escape lack of accidental, accidentals that are not repeated, and rests
			if (isDrawnAccidental(allAccidentals[bar][i][j]) && allNotes[bar][i][j] > -1) {
				if (abs(y-prevY) < staffDist * 3 && j > 0) {
					if (prevX == (allNoteCoordsX[bar][i][allChordsBaseIndexes[bar][i]] - allNoteCoordsXOffset[bar][i])) {
						x -= (staffDist * accidentalsOffsetCoef);
					}
				}
				drawAccidental(allAccidentals[bar][i][j], (x*xCoefLocal)+xStartPnt-(noteWidth*1.2), y+yStartPnt+yOffset);
				prevY = y;
				prevX = x;
			}
			if (naturalSignsNotWritten[bar][i][j]) {
				if (abs(y-prevY) < staffDist * 3 && j > 0) {
					if (prevX == (allNoteCoordsX[bar][i][allChordsBaseIndexes[bar][i]] - allNoteCoordsXOffset[bar][i])) {
						x -= (staffDist * accidentalsOffsetCoef);
					}
				}
				notationFont->draw(accSyms[4],
						(x*xCoefLocal)+xStartPnt-(noteWidth*1.2),
						y+yStartPnt+yOffset);
				naturalSignsNotWritten[bar][i][j] = 0;
				prevY = y;
				prevX = x;
			}
		}
	}
}

//--------------------------------------------------------------
void Notes::drawGlissandi(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef, int nextBar)
{
	float xCoefLocal = xCoef;
	//if (scoreOrientation > 0) xCoefLocal = 1;
	// a line that starts or ends at a note on a staff line starts or ends a little above or below the line, towards the other note
	auto offsetFromLine = [&](float y, float otherY) {
		if (((int)(abs((y + yOffset) - yStartPnt) / halfStaffDist) % 2) == 0) {
			float glissLineOffset = staffDist / 4.0;
			return (y > otherY ? y - glissLineOffset : y + glissLineOffset);
		}
		return y;
	};
	for (unsigned i = 0; i < allGlissandi[bar].size(); i++) {
		if (allGlissandi[bar][i] <= 0) continue;
		// a glissando on the last note goes to the first note of the next bar (the same bar if it plays alone)
		// that note is not drawn next to it (it is in another bar, which may not be visible), so the line ends at the end of the bar
		// in the direction of that note, as if it were drawn right after the end of the bar
		bool toEdge = (i + 1 >= allNoteCoordsX[bar].size());
		int targetBar = bar;
		int targetNdx = i + 1;
		if (toEdge) {
			targetBar = (nextBar >= 0 && allNoteHeadCoordsY.find(nextBar) != allNoteHeadCoordsY.end() && !allNoteHeadCoordsY[nextBar].empty() ? nextBar : bar);
			targetNdx = 0;
		}
		float xStart = (allNoteCoordsX[bar][i][allChordsBaseIndexes[bar][i]]*xCoefLocal) + xStartPnt;
		float xEndBase;
		if (toEdge) {
			// find the remaining number of beats to determine the distance left to the edge of the staff
			int beatsUpToThisNote = 0;
			for (size_t j = 0; j < i; j++) beatsUpToThisNote += durations[bar][j];
			int remainingBeats = beats[bar] - beatsUpToThisNote;
			xEndBase = ((allNoteCoordsX[bar][i][allChordsBaseIndexes[bar][i]]*xCoefLocal) + xStartPnt - noteWidth) + (distBetweenBeats[bar] * remainingBeats);
		}
		else {
			xEndBase = (allNoteCoordsX[bar][targetNdx][allChordsBaseIndexes[bar][targetNdx]]*xCoefLocal) + xStartPnt - noteWidth;
		}
		// a line from a note with a stem up starts to the right of the half of the note head, with a stem down at its right edge
		if (stemDirections[bar][i] > 0) {
			xStart += (noteWidth/2);
		}
		else {
			xStart += noteWidth;
		}
		// every note of a chord gets its own line, to the note it is paired with in the next chord, as the sequencer plays it
		// the notes are paired by their height on the staff (see glissandoPairs() in utils.h), the higher a note the smaller its y
		std::vector<float> startPitches, endPitches;
		std::vector<int> startNdxs, endNdxs;
		for (unsigned j = 0; j < allNoteHeadCoordsY[bar][i].size(); j++) {
			if (allNotes[bar][i][j] < 0) continue;
			startPitches.push_back(-allNoteHeadCoordsY[bar][i][j]);
			startNdxs.push_back(j);
		}
		for (unsigned j = 0; j < allNoteHeadCoordsY[targetBar][targetNdx].size(); j++) {
			if (allNotes[targetBar][targetNdx][j] < 0) continue;
			endPitches.push_back(-allNoteHeadCoordsY[targetBar][targetNdx][j]);
			endNdxs.push_back(j);
		}
		std::vector<int> pairs = glissandoPairs(startPitches, endPitches);
		for (size_t k = 0; k < pairs.size(); k++) {
			if (pairs[k] < 0) continue;
			int startNdx = startNdxs[k];
			int endNdx = endNdxs[pairs[k]];
			float xEnd = xEndBase;
			float yStart = allNoteHeadCoordsY[bar][i][startNdx] + yStartPnt + yOffset;
			float yEnd = allNoteHeadCoordsY[targetBar][targetNdx][endNdx] + yStartPnt + yOffset;
			if (toEdge) {
				// where the note would be after the end of the bar, at the distance its own bar has from its start
				float xTarget = (xLength*xCoefLocal) + xStartPnt + (allNoteCoordsX[targetBar][targetNdx][endNdx]*xCoefLocal);
				float yStartOnLine = offsetFromLine(yStart, yEnd);
				float yEndAtEdge = yEnd;
				if (xTarget > xStart) yEndAtEdge = yStartOnLine + ((yEnd - yStartOnLine) * ((xEnd - xStart) / (xTarget - xStart)));
				cv().line(xStart, yStartOnLine, xEnd, yEndAtEdge);
				continue;
			}
			// if we have an accidental at the end of the gliss
			if (isDrawnAccidental(allAccidentals[bar][targetNdx][endNdx]) && allAccidentals[bar][targetNdx][endNdx] != 4) {
				if (stemDirections[bar][targetNdx] < 0) {
					xEnd += staffDist;
				}
				xEnd -= (accidentalWidth(allAccidentals[bar][targetNdx][endNdx])/2.0);
			}
			float yStartOnLine = offsetFromLine(yStart, yEnd);
			float yEndOnLine = offsetFromLine(yEnd, yStart);
			cv().line(xStart, yStartOnLine, xEnd, yEndOnLine);
		}
	}
}

//--------------------------------------------------------------
void Notes::drawTuplets(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef)
{
	// code copied from drawNotes() where we draw the beams, and modified
	for (auto it = tupStartStop[bar].begin(); it != tupStartStop[bar].end(); ++it) {
		// the std::string of the tuplet number to be displayed
		std::string tuplet = std::to_string(tupRatios[bar][it->first].first);
		float halfStringHeight = textFont->height(tuplet) * 0.5;
		float halfStringWidth = textFont->width(tuplet) * 0.5;
		for (int i = 0; i < 2; i++) {
			cv().line((tupXCoords[bar][it->first][i*2] * xCoef) + xStartPnt,
					tupYCoords[bar][it->first][i*2] + yStartPnt + yOffset,
					(tupXCoords[bar][it->first][(i*2)+1] * xCoef) + xStartPnt,
					tupYCoords[bar][it->first][(i*2)+1] + yStartPnt + yOffset);
			// the hooks at the two ends of the tuplet bracket
			cv().line((tupXCoords[bar][it->first][(i*2)+i] * xCoef) + xStartPnt,
					tupYCoords[bar][it->first][(i*2)+i] + yStartPnt + yOffset,
					(tupXCoords[bar][it->first][(i*2)+i] * xCoef) + xStartPnt,
					tupYCoords[bar][it->first][(i*2)+i]+(halfStaffDist*tupDirs[bar][it->first]) + yStartPnt + yOffset);
		}
		textFont->draw(tuplet, (tupXCoords[bar][it->first][1] * xCoef) + xStartPnt + halfStringWidth,
				tupYCoords[bar][it->first][1] + halfStringHeight + yStartPnt + yOffset);
	}
}

//--------------------------------------------------------------
void Notes::drawArticulations(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef)
{
	float xCoefLocal = xCoef;
	//if (scoreOrientation > 0) xCoefLocal = 1;
	for (unsigned i = 0; i < allArticulations[bar].size(); i++) {
		for (unsigned j = 0; j < allArticulations[bar][i].size(); j++) {
			if (allArticulations[bar][i][j] > 0) {
				if (allArticulations[bar][i][j] < 7) {
					// we don't have a symbol for no articulation so bar with allArticulations[i]-1
					std::string articulStr = articulSyms[allArticulations[bar][i][j]-1];
					if ((allArticulations[bar][i][j] == 1) && (stemDirections[bar][i] > 0)) {
						// the marcato sign needs to be inverted in case of being projected under the note
						cv().push();
						cv().translate((articulXPos[bar][i]*xCoefLocal)+xStartPnt, articulYPos[bar][i][j]+yStartPnt+yOffset);
						cv().scale(1, -1);
						notationFont->draw(articulStr, 0, 0);
						cv().pop();
					}
					else {
						notationFont->draw(articulStr, (articulXPos[bar][i]*xCoefLocal)+xStartPnt, articulYPos[bar][i][j]+yStartPnt+yOffset);
					}
				}
				else {
					float offset;
					if (stemDirections[bar][i] > 0) {
						offset = halfStaffDist;
					}
					else {
						offset = -halfStaffDist;
					}
					notationFont->draw(".", (articulXPos[bar][i]*xCoefLocal)+xStartPnt+(noteWidth/2), articulYPos[bar][i][j]+yStartPnt+yOffset);
					notationFont->draw("_", (articulXPos[bar][i]*xCoefLocal)+xStartPnt, articulYPos[bar][i][j]+yStartPnt+offset+yOffset);
				}
			}
		}
	}
}

//--------------------------------------------------------------
void Notes::drawOttavas(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef)
{
	float xCoefLocal = xCoef;
	//if (scoreOrientation > 0) xCoefLocal = 1;
	int prevOttava = 0;
	for (unsigned i = 0; i < allOttavas[bar].size(); i++) {
		if (allOttavas[bar][i] != prevOttava && allOttavas[bar][i] != 0) {
			notationFont->draw(octaveSyms[abs(allOttavas[bar][i])], (allNoteCoordsX[bar][i][0]*xCoefLocal)+xStartPnt, allOttavasYCoords[bar][i]+yStartPnt+yOffset);
		}
		prevOttava = allOttavas[bar][i];
	}
	// first determine if there is no octave change at all, in which case we need to draw one line for all notes
	int numChanges = 0;
	unsigned singleChangeNdx;
	for (unsigned i = 0; i < allOttavasChangedAt[bar].size(); i++) {
		if (allOttavasChangedAt[bar][i] > -1 && allOttavas[bar].size() > 1) {
			numChanges++;
			singleChangeNdx = i;
		}
	}
	if (numChanges == 1) {
		drawOttavaLine(bar, singleChangeNdx, allOttavas[bar].size()-1, xStartPnt, yStartPnt, yOffset, xCoefLocal);
	}
	else {
		unsigned prevChange = 0;
		for (unsigned i = 1; i < allOttavasChangedAt[bar].size(); i++) {
			// if the ottava has changed
			if (allOttavasChangedAt[bar][i] > -1) {
				if (allOttavasChangedAt[bar][i-1] == -1) {
					drawOttavaLine(bar, prevChange, i-1, xStartPnt, yStartPnt, yOffset, xCoefLocal);
				}
				prevChange = i;
			}
			// if there is a line that needs to be drawn till the end of the bar
			else if (i == allOttavasChangedAt[bar].size() - 1 && allOttavasChangedAt[bar][i] == -1) {
				drawOttavaLine(bar, prevChange, i, xStartPnt, yStartPnt, yOffset, xCoefLocal);
			}
		}
	}
}

//--------------------------------------------------------------
void Notes::drawOttavaLine(int bar, unsigned startNdx, unsigned endNdx, float xStartPnt, float yStartPnt, float yOffset, float xCoef)
{
	// an ottava that applies to a single note is shown with its symbol only, without a line
	// and notes without an ottava have no line (the stretches between ottavas are passed here too)
	if (endNdx <= startNdx || allOttavas[bar][startNdx] == 0) return;
	float xCoefLocal = xCoef;
	//if (scoreOrientation > 0) xCoefLocal = 1;
	float xStart = (allNoteCoordsX[bar][startNdx][0] * xCoefLocal) + xStartPnt;
	float xEnd = (allNoteCoordsX[bar][endNdx][0] * xCoefLocal) + xStartPnt;
	float y = allOttavasYCoords[bar][startNdx] + yStartPnt + yOffset;
	float textHalfHeight = notationFont->height(octaveSyms[abs(allOttavas[bar][startNdx])]) / 2;
	xStart += notationFont->width(octaveSyms[abs(allOttavas[bar][startNdx])]);
	cv().line(xStart, y, xEnd, y);
	// the hook at the end of the line always points towards the staff
	// down for lines above the staff, up for lines below it
	if (allOttavasYCoords[bar][startNdx] < halfStaffDist * 4) {
		cv().line(xEnd, y, xEnd, y+textHalfHeight);
	}
	else {
		cv().line(xEnd, y, xEnd, y-textHalfHeight);
	}
}

//--------------------------------------------------------------
void Notes::drawText(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef)
{
	float xCoefLocal = xCoef;
	//if (scoreOrientation > 0) xCoefLocal = 1;
	int index = 0;
	for (unsigned i = 0; i < allTextsIndexes.at(bar).size(); i++) {
		for (unsigned j = 0; j < allTextsIndexes.at(bar).at(i).size(); j++) {
			if (allTextsIndexes.at(bar).at(i).at(j) != 0) {
				textFont->draw(allTexts.at(bar).at(index++), (allTextsXCoords.at(bar).at(i)*xCoefLocal)+xStartPnt, allTextsYCoords.at(bar).at(i).at(j)+yStartPnt+yOffset);
			}
		}
	}
}

//--------------------------------------------------------------
void Notes::drawSlurs(int bar, int loopNdx, float xStartPnt, float yStartPnt, float yOffset, float xCoef, int nextBar)
{
	float xCoefLocal = xCoef;
	//if (scoreOrientation > 0) xCoefLocal = 1;
	// in horizontal view, a slur that goes on in the next bar (e.g. in \bars) is drawn when that bar is drawn, from the start of the slur
	// but a slur left open at the end of a bar that is followed by another bar (or by itself, if it plays alone) is drawn to the end of the bar
	bool nextBarContinuesSlur = false;
	if (nextBar == bar + 1 && slurIndexes.find(nextBar) != slurIndexes.end()) {
		for (const auto& slur : slurIndexes[nextBar]) {
			if (slur.first == -1 && (slur.second > -1 || isWholeSlurred[nextBar])) nextBarContinuesSlur = true;
		}
	}
	cv().setFill(false);
	for (unsigned i = 0; i < slurStartX[bar].size(); i++) {
		// slurs that span over more than one bar are drawn only when we draw the bar of the end of the slur
		if (slurLinks[bar][i].second > 0 && scoreOrientation > 0 && nextBarContinuesSlur) continue;
		float x0 = 0.0;
		float y0 = slurStartY[bar][i] + yStartPnt + yOffset;
		float x1;
		float y1 = slurMiddleY1[bar][i] + yStartPnt + yOffset;
		float x2;
		float y2 = slurMiddleY2[bar][i] + yStartPnt + yOffset;
		float x3 = (slurStopX[bar][i]*xCoefLocal) + xStartPnt;
		float y3 = slurStopY[bar][i] + yStartPnt + yOffset;
		if (slurLinks[bar][i].first > 0 && scoreOrientation > 0) {
			if (loopNdx >= slurLinks[bar][i].first) {
				for (unsigned j = slurStopX[bar-slurLinks[bar][i].first].size()-1; j >=0; j--) {
					if (slurStopX[bar-slurLinks[bar][i].first][j] > -1) {
						x0 = (slurStartX[bar-slurLinks[bar][i].first][j]*xCoefLocal) + xOffsets[bar-slurLinks[bar][i].first];
						break;
					}
				}
			}
			else {
				// if the slur connects to a bar not visible
				// start the slur at the beginning of the first visible bar
				x0 = allNoteCoordsX[bar-loopNdx][0][allChordsBaseIndexes[bar][0]] - (2 * staffDist);
			}
			float curveSegment = (x3 - x0) / 3.0;
			x1 = x0 + curveSegment;
			x2 = x1 + curveSegment;
		}
		else {
			x0 = (slurStartX[bar][i]*xCoefLocal) + xStartPnt;
			x1 = (slurMiddleX1[bar][i]*xCoefLocal) + xStartPnt;
			x2 = (slurMiddleX2[bar][i]*xCoefLocal) + xStartPnt;
		}
		cv().beginShape();
		cv().vertex(x0, y0);
		cv().bezierVertex(x1, y1, x2, y2, x3, y3);
		cv().endShape(false);
	}
	cv().setFill(true);
}

//--------------------------------------------------------------
void Notes::drawTies(int thisBar, int loopNdx, float xStartPnt, float yStartPnt, float yOffset, float xCoef)
{
	// drawTies is separate from drawTie, so we can draw the extreme notes of a chord
	// with opposite orientations
	cv().setFill(false);
	// a tie on the last note goes to the end of the bar, towards the first note of the next bar (see drawTie())
	for (unsigned i = 0; i < tieIndexes[thisBar].size() && i < allNoteCoordsX[thisBar].size(); i++) {
		if (tieIndexes[thisBar][i] > -1) {
			drawTie(thisBar, i, allChordsBaseIndexes[thisBar][i], xStartPnt, yStartPnt, yOffset, xCoef, 1);
			if (allNotes[thisBar][i].size() > 1) {
				drawTie(thisBar, i, allChordsEdgeIndexes[thisBar][i], xStartPnt, yStartPnt, yOffset, xCoef, -1);
			}
			for (int j = 0; j < (int)allNotes[thisBar][i].size(); j++) {
				if (j == allChordsBaseIndexes[thisBar][i] || j == allChordsEdgeIndexes[thisBar][i]) {
					continue;
				}
				drawTie(thisBar, i, j, xStartPnt, yStartPnt, yOffset, xCoef, 1);
			}
		}
	}
	cv().setFill(true);
}

//--------------------------------------------------------------
void Notes::drawTie(int thisBar, unsigned i, unsigned j, float xStartPnt, float yStartPnt, float yOffset, float xCoef, int stemDirCoef)
{
	// a tie always starts and ends a little away from the centers of the note heads on the X axis
	float x0 = xStartPnt + ((allNoteCoordsX[thisBar][i][j] + tieXOffset()) * xCoef);
	float x1, x2;
	float x3;
	if (i + 1 < allNoteCoordsX[thisBar].size() && j < allNoteCoordsX[thisBar][i+1].size()) {
		x3 = xStartPnt + ((allNoteCoordsX[thisBar][i+1][j] - tieXOffset()) * xCoef);
	}
	else {
		x3 = xStartPnt + ((xLength - (halfStaffDist * 0.5)) * xCoef);
	}
	// a tie of a note with the stem up is drawn below the note head, and of a note with the stem down above it (the opposite with stemDirCoef -1)
	// a slur on the same side starts and ends further from the note head (see slurYOffset()), so the tie is under it
	float y = yStartPnt + allNoteHeadCoordsY[thisBar][i][j] + yOffset;
	float yMiddle;
	if (stemDirections[thisBar][i] > 0) {
		y += (tieYOffset() * stemDirCoef);
		yMiddle = y + (halfStaffDist * stemDirCoef);
	}
	else {
		y -= (tieYOffset() * stemDirCoef);
		yMiddle = y - (halfStaffDist * stemDirCoef);
	}
	float curveSegment = (x3 - x0) / 3.0;
	x1 = x0 + curveSegment;
	x2 = x1 + curveSegment;
	cv().beginShape();
	cv().vertex(x0, y);
	cv().bezierVertex(x1, yMiddle, x2, yMiddle, x3, y);
	cv().endShape(false);
}

//--------------------------------------------------------------
void Notes::drawDynamics(int bar, float xStartPnt, float yStartPnt, float yOffset, float xCoef)
{
	float xCoefLocal = xCoef;
	//if (scoreOrientation > 0) xCoefLocal = 1;
	for (unsigned i = 0; i < dynamics[bar].size(); i++) {
		if (dynamics[bar][i] > -1) {
			notationFont->draw(dynSyms[dynamics[bar][i]], (dynsXCoords[bar][i]*xCoefLocal)+xStartPnt, dynsYCoords[bar][i]+yStartPnt+yOffset);
		}
	}
	for (unsigned i = 0; i < dynamicsRampStart[bar].size(); i++) {
		if (dynamicsRampStart[bar][i] > -1) {
			// find the next index of dynsRampEndXCoords[bar] that is greater than -1
			// to determine where the dynamic ramp ends
			unsigned endNdx = i;
			// a ramp can't start and end on the same note, so an end on its first note belongs to the previous ramp
			for (unsigned j = i + 1; j < dynamicsRampEnd[bar].size(); j++) {
				if (dynamicsRampEnd[bar][j] > -1) {
					endNdx = j;
					break;
				}
			}
			float xEnd = (dynsRampEndXCoords[bar][endNdx]*xCoefLocal)+xStartPnt;
			// a crescendo or diminuendo left open at the end of the bar goes on to the next bar, so it is drawn to the end of the bar
			if (dynamicsRampOpenAtEnd[bar] && endNdx + 1 >= dynamicsRampEnd[bar].size()) {
				xEnd = std::max(xEnd, ((xLength - halfStaffDist)*xCoefLocal)+xStartPnt);
			}
			for (int j = 0; j < 2; j++) {
				cv().line((dynsRampStartXCoords[bar][i]*xCoefLocal)+xStartPnt,
						dynsRampStartYCoords[bar][i]+yStartPnt+(staffDist*(((dynamicsRampDir[bar][i]*(-1))+1)*((j*2)-1)))+yOffset,
						xEnd,
						dynsRampEndYCoords[bar][endNdx]+yStartPnt+(staffDist*(dynamicsRampDir[bar][i]*((j*2)-1)))+yOffset);
			}
		}
	}
}

//--------------------------------------------------------------
void Notes::printVector(std::vector<int> v)
{
	for (unsigned i = 0; i < v.size(); i++) {
		std::cout << v[i] << " ";
	}
	std::cout << std::endl;
}

//--------------------------------------------------------------
void Notes::printVector(std::vector<std::string> v)
{
	for (unsigned i = 0; i < v.size(); i++) {
		std::cout << v[i] << " ";
	}
	std::cout << std::endl;
}

//--------------------------------------------------------------
void Notes::printVector(std::vector<float> v)
{
	for (unsigned i = 0; i < v.size(); i++) {
		std::cout << v[i] << " ";
	}
	std::cout << std::endl;
}
