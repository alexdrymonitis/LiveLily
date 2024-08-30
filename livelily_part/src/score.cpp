#include <algorithm>
#include "score.h"
#include "ofApp.h"
#include "limits.h"

/************************* Staff class ************************/
//--------------------------------------------------------------
Staff::Staff()
{
	lineWidth = ((ofApp*)ofGetAppPtr())->lineWidth;
	// G, F, C clefs
	string clefSymsLocal[3] = {"&", "?", "B"};
	string meterSymsLocal[2] = {"c", "C"};
	for (int i = 0; i < 3; i++) {
		clefSyms[i] = clefSymsLocal[i];
	}
	for (int i = 0; i < 2; i++) {
		meterSyms[i] = meterSymsLocal[i];
	}
	clefIndex = 0;
	isLoopStart = false;
	isLoopEnd = false;
	isScoreEnd = false;
	meterIndex = 0;
	rhythm = false;
	scoreOrientation = 0;
	clefLength = 0;
	xCoef = 1;
	xOffset = 0;
}

//--------------------------------------------------------------
void Staff::setClef(int clefIdx)
{
	clefIndex = clefIdx;
}

//--------------------------------------------------------------
void Staff::setRhythm(bool isRhythm)
{
	rhythm = isRhythm;
}

//--------------------------------------------------------------
void Staff::setSize(int fontSize)
{
	Staff::fontSize = fontSize;
	notationFont.load("sonata.ttf", fontSize);
	clefLength = max(notationFont.stringWidth(clefSyms[0]), notationFont.stringWidth(clefSyms[1]));
	clefLength = max(clefLength, notationFont.stringWidth(clefSyms[2]));
	meterLength = notationFont.stringWidth("64");
}

//--------------------------------------------------------------
void Staff::setMeter(int bar, int numer, int denom)
{
	numerator[bar] = numer;
	denominator[bar] = denom;
	if ((numerator[bar] == 4) && (denominator[bar] == 4)) meterIndex = 0;
	else if ((numerator[bar] == 2) && (denominator[bar] == 2)) meterIndex = 1;
	else meterIndex = 2;
}

//--------------------------------------------------------------
void Staff::setOrientation(int orientation)
{
	scoreOrientation = orientation;
}

//--------------------------------------------------------------
void Staff::setNumBarsToDisplay(int numBars)
{
	xCoef = 2.0 / (float)numBars;
}

//--------------------------------------------------------------
void Staff::setCoords(float xLen, float y1, float y2, float dist)
{
	yAnchor[0] = y1;
	yAnchorRecenter[0] = y1;
	yAnchor[1] = y2;
	yAnchorRecenter[1] = y2;
	xLength = xLen;
	yDist = dist;
}

//--------------------------------------------------------------
void Staff::correctYAnchor(float y1, float y2)
{
	yAnchor[0] = y1;
	yAnchorRecenter[0] = y1;
	yAnchor[1] = y2;
	yAnchorRecenter[1] = y2;
}

//--------------------------------------------------------------
float Staff::getXLength()
{
	// separate the clef and meter offsets because we place them closer together
	// but we don't change their font size, so we have to compansate for this
	// unchanged dimension combined with the the other changed lengths
	float len = xLength - getClefXOffset() - getMeterXOffset() - clefLength - meterLength;
	if (scoreOrientation == 1) len *= xCoef;
	return len + getClefXOffset() + getMeterXOffset() + clefLength + meterLength;
}

//--------------------------------------------------------------
float Staff::getYAnchor()
{
	return yAnchor[scoreOrientation];
}

//--------------------------------------------------------------
void Staff::moveScoreX(float numPixels)
{
	xOffset += numPixels;
}

//--------------------------------------------------------------
void Staff::moveScoreY(float numPixels)
{
	yAnchor[scoreOrientation] += numPixels;
}

//--------------------------------------------------------------
void Staff::recenterScore()
{
	yAnchor[scoreOrientation] = yAnchorRecenter[scoreOrientation];
}

//--------------------------------------------------------------
float Staff::getClefXOffset()
{
	float y = yDist / 2;
	if (scoreOrientation == 1) y *= xCoef;
	return y + clefLength;
}

//--------------------------------------------------------------
float Staff::getMeterXOffset()
{
	float y = yDist / 2;
	if (scoreOrientation == 1) y *= xCoef;
	return y + meterLength; 
}

//--------------------------------------------------------------
void Staff::drawStaff(int bar, float xStartPnt, float yOffset, bool drawClef, bool drawMeter, bool drawLoopStartEnd)
{
	ofSetColor(0);
	float xCoefLocal = xCoef;
	if (scoreOrientation == 0) xCoefLocal = 1;
	float xStartLocal = xStartPnt;
	float xEndLocal = getXLength() + xStartPnt;
	if (!drawClef) xEndLocal -= getClefXOffset();
	if (!drawMeter) xEndLocal -= getMeterXOffset();
	float yStart = yAnchor[scoreOrientation] - (lineWidth / 2) + yOffset;
	float yEnd = (yAnchor[scoreOrientation] + (4 * yDist)) + (lineWidth / 2) + yOffset;
	ofDrawLine(xStartLocal, yStart, xStartLocal, yEnd);
	ofDrawLine(xEndLocal, yStart, xEndLocal, yEnd);
	if (rhythm) {
		float yPntLocal = yAnchor[scoreOrientation] + (2*yDist) + yOffset;
		ofDrawLine(xStartLocal, yPntLocal, xEndLocal, yPntLocal);
	}
	else {
		for (int i = 0; i < 5; i++) {
			float yPntLocal = yAnchor[scoreOrientation] + (i*yDist) + yOffset;
			ofDrawLine(xStartLocal, yPntLocal, xEndLocal, yPntLocal);
		}
	}
	if (drawClef) {
		if (clefIndex < 3) {
			notationFont.drawString(clefSyms[clefIndex], xStartLocal+((yDist/2)*xCoefLocal),
									yAnchor[scoreOrientation]+(yDist*4)-(yDist*0.2)+yOffset);
		}
		else {
			float xPos = ((yDist * 1.5) * xCoefLocal) + xStartLocal;
			float yPos = yAnchor[scoreOrientation] + yDist + yOffset;
			for (int i = 0; i < 2; i++) {
				drawThickLine(xPos+(i*yDist), yPos, xPos+(i*yDist), yPos+(yDist*2), yDist*0.25);
			}
		}
	}
	if (drawMeter) {
		float xPos = xStartLocal + ((yDist / 2) * xCoefLocal);
		float numeratorY = yAnchor[scoreOrientation] + yDist + yOffset;
		float denominatorY = numeratorY + (yDist*2) + yOffset;
		if (drawClef) xPos += getClefXOffset();
		if (meterIndex < 2) {
			notationFont.drawString(meterSyms[meterIndex], xPos, numeratorY+yDist);
		}
		else {
			notationFont.drawString(ofToString(numerator[bar]), xPos, numeratorY);
			notationFont.drawString(ofToString(denominator[bar]), xPos, denominatorY);
		}
	}
	if (drawLoopStartEnd) {
		if (isLoopStart) drawLoopStart(bar, xStartLocal, yOffset);
		if (isLoopEnd) drawLoopEnd(bar, xEndLocal, yOffset);
		if (isScoreEnd) drawScoreEnd(bar, xEndLocal, yOffset);
	}
}

//--------------------------------------------------------------
void Staff::drawLoopStart(int bar, float xStart, float yOffset)
{
	// the " + (yDist*3.5)" is taken from drawStaff() where the meter is drawn
	float xPos = xStart + (yDist*3.5);
	if (meterIndex < 2) {
		xPos += notationFont.stringWidth(meterSyms[meterIndex]);
	}
	else {
		float maxWidth = max(notationFont.stringWidth(ofToString(numerator[bar])),
							 notationFont.stringWidth(ofToString(denominator[bar])));
		xPos += maxWidth;
	}
	// a little bit of offset so that symbols are not clamped together
	xPos += (yDist * 0.5);
	ofDrawLine(xPos, yAnchor[scoreOrientation]+yOffset, xPos, yAnchor[scoreOrientation]+(yDist*4)+yOffset);
	ofDrawLine(xPos+(yDist*0.5), yAnchor[scoreOrientation]+yOffset, xPos+(yDist*0.5), yAnchor[scoreOrientation]+(yDist*4)+yOffset);
	ofDrawCircle(xPos+yDist, yAnchor[scoreOrientation]+(yDist*1.5)+yOffset, yDist*0.2);
	ofDrawCircle(xPos+yDist, yAnchor[scoreOrientation]+(yDist*2.5)+yOffset, yDist*0.2);
}

//--------------------------------------------------------------
void Staff::drawLoopEnd(int bar, float xEnd, float yOffset)
{
	ofDrawLine(xEnd-(yDist*0.5), yAnchor[scoreOrientation]+yOffset,
			   xEnd-(yDist*0.5), yAnchor[scoreOrientation]+(yDist*4)+yOffset);
	ofDrawCircle(xEnd-yDist, yAnchor[scoreOrientation]+(yDist*1.5)+yOffset, yDist*0.2);
	ofDrawCircle(xEnd-yDist, yAnchor[scoreOrientation]+(yDist*2.5)+yOffset, yDist*0.2);
}

//--------------------------------------------------------------
void Staff::drawScoreEnd(int bar, float xEnd, float yOffset)
{
	ofDrawLine(xEnd-(yDist*0.75), yAnchor[scoreOrientation]+yOffset,
			   xEnd-(yDist*0.75), yAnchor[scoreOrientation]+(yDist*4)+yOffset);
	float x1 = xEnd-(yDist*0.125); // multiply by half of the value in the m.addVertex lines
	float y1 = yAnchor[scoreOrientation]-(((ofApp*)ofGetAppPtr())->lineWidth/2)+yOffset;
	float x2 = xEnd-(yDist*0.125);
	float y2 = yAnchor[scoreOrientation]+(yDist*4)+(((ofApp*)ofGetAppPtr())->lineWidth/2)+yOffset;
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
	ofPoint a;
	ofPoint b;
	a.x = x1;
	a.y = y1;
	b.x = x2;
	b.y = y2;
	ofPoint diff = (a - b).getNormalized();
	diff.rotate(90, ofPoint(0,0,1));
	ofMesh m;
	m.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);
	m.addVertex(a + diff * width);
	m.addVertex(a - diff * width);
	m.addVertex(b + diff * width);
	m.addVertex(b - diff * width);
	m.draw();
}

//--------------------------------------------------------------
Staff::~Staff(){}

/************************* Notes class ************************/
//--------------------------------------------------------------
Notes::Notes()
{
	clefIndex = 0;
	// whole and half (first symbol), and the rest of rests
	string restsSymsLocal[6] = {"î", "Î", "ä", "Å", "¨", "ô"};
	// ppp, pp, p, mp, mf, f, ff, fff
	string dynSymsLocal[8] = {"¸", "¹", "p", "P", "F", "f", "Ä", "ì"};
	// double flat, dummy 1.5 flat, flat, dummy half flat, natural,
	// dummy half sharp, sharp, dummy 1.5 sharp, double sharp
	string accSymsLocal[9] = {"º", "", "b", "", "n", "", "#", "", "Ü"};
	// whole note, half note, filled note, tail for upward stem, tail for downward stem
	string notesSymsLocal[5] = {"w", "ú", "ö", "j", "J"};
	// marcato, trill, tenuto, staccatissimo, accent, staccato
	string articulSymsLocal[6] = {"^", "Ù", "_", "à", ">", "."};
	for (int i = 0; i < 6; i++) {
		restsSyms[i] = restsSymsLocal[i];
	}
	for (int i = 0; i < 8; i++) {
		dynSyms[i] = dynSymsLocal[i];
	}
	for (int i = 0; i < 9; i++) {
		accSyms[i] = accSymsLocal[i];
	}
	for (int i = 0; i < 5; i++) {
		notesSyms[i] = notesSymsLocal[i];
	}
	for (int i = 0; i < 6; i++) {
		articulSyms[i] = articulSymsLocal[i];
	}
	octaveSyms[0] = "";
	octaveSyms[1] = "8";
	octaveSyms[2] = "15";
	beamsLineWidth = 2;
	//beats = numBeats;
	animate = false;
	mute = false;
	whichNote = -1;
	lineWidth = ((ofApp*)ofGetAppPtr())->lineWidth;
	//quarterSharp.load("quarter_sharp.png");
	fontSize = 0;
	rhythm = false;
	scoreOrientation = 0;
	xOffset = 0;
	setFontSize(fontSize);
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
void Notes::setFontSize(int fontSize)
{
	if (Notes::fontSize != fontSize) {
		Notes::fontSize = fontSize;
		notationFont.load("sonata.ttf", fontSize);
		textFont.load("times-new-roman.ttf", fontSize/2);
		noteWidth = notationFont.stringWidth(notesSyms[2]);
		noteHeight = notationFont.stringHeight(notesSyms[2]);
		// store the widths and heights of dynamic and rest symbols
		// since these are called from a threaded class and calling stringWidth won't work
		for (int i = 0; i < 8; i++) {
			dynSymsWidths[i] = notationFont.stringWidth(dynSyms[i]);
			dynSymsHeights[i] = notationFont.stringHeight(dynSyms[i]);
		}
		for (int i = 0; i < 6; i++) {
			restsSymsWidths[i] = notationFont.stringWidth(restsSyms[i]);
			restsSymsHeights[i] = notationFont.stringHeight(restsSyms[i]);
		}
	}
}

//--------------------------------------------------------------
void Notes::setCoords(float xLen, float yStart1, float yStart2, float staffLinesDist, int fontSize)
{
	yStartPnt[0] = yStart1;
	yStartPntRecenter[0] = yStart1;
	yStartPnt[1] = yStart2;
	yStartPntRecenter[1] = yStart2;
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
	return maxYCoord[bar] + yStartPnt[scoreOrientation];
}

//--------------------------------------------------------------
float Notes::getMinYPos(int bar)
{
	return minYCoord[bar] + yStartPnt[scoreOrientation];
}

//--------------------------------------------------------------
float Notes::getMinVsAnchor(int bar)
{
	return minYCoord[bar];
}

//--------------------------------------------------------------
float Notes::getNoteWidth()
{
	return notationFont.stringWidth(notesSyms[0]);
}

//--------------------------------------------------------------
float Notes::getNoteHeight()
{
	return notationFont.stringHeight(notesSyms[0]);
}

//--------------------------------------------------------------
void Notes::setClef(int clefIdx)
{
	clefIndex = clefIdx;
}

//--------------------------------------------------------------
void Notes::setOrientation(int orientation)
{
	scoreOrientation = orientation;
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
void Notes::setMeter(int bar, int numer, int denom, int numBeats)
{
	numerator = numer;
	denominator = denom;
	beats = numBeats;
	distBetweenBeats[bar] = xLength / numBeats;
}

//--------------------------------------------------------------
void Notes::setNotes(int bar,
		vector<vector<int>> notes,
		vector<vector<int>> accidentals,
		vector<vector<int>> naturalsNotWritten,
		vector<vector<int>> octaves,
		vector<int> ottavas,
		vector<int> durs,
		vector<int> dots,
		vector<int> gliss,
		vector<int> articul,
		vector<int> dyns,
		vector<int> dynIdx,
		vector<int> dynRampStart,
		vector<int> dynRampEnd,
		vector<int> dynRampDir,
		vector<int> slurBegin,
		vector<int> slurEnd,
		vector<string> texts,
		vector<int> textIndexes)
{
	// if we have copied the bar by writing a bar name instead of an actual melodic line
	// then we don't need to pass any of the data or make any calculations
	if (allNotes.find(bar) != allNotes.end()) return;
	allNotes[bar] = notes;
	allAccidentals[bar] = accidentals;
	naturalSignsNotWritten[bar] = naturalsNotWritten;
	allOctaves[bar] = octaves;
	allOttavas[bar] = ottavas;
	durations[bar] = durs;
	dotIndexes[bar] = dots;
	allGlissandi[bar] = gliss;
	allArticulations[bar] = articul;
	dynamics[bar] = dyns;
	dynamicsIndexes[bar] = dynIdx;
	dynamicsRampStart[bar] = dynRampStart;
	dynamicsRampEnd[bar] = dynRampEnd;
	dynamicsRampDir[bar] = dynRampDir;
	slurBeginnings[bar] = slurBegin;
	slurEndings[bar] = slurEnd;
	allTexts[bar] = texts;
	allTextsIndexes[bar] = textIndexes;
	maxYCoord[bar] = FLT_MIN;
	minYCoord[bar] = FLT_MAX;
	if (clefIndex != 0) changeNotesBasedOnClef(bar);
	setNotePositions(bar);
}

//--------------------------------------------------------------
void Notes::changeNotesBasedOnClef(int bar)
{
	// if we have a different clef we must offset the notes
	if (clefIndex == 1) {
		bool tacetLocal = false;
		for (unsigned i = 0; i < allNotes[bar].size(); i++) {
			if (!((allNotes[bar].size() == 1) && (allNotes[bar][i][0] == -1))) {
				for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
					if (allNotes[bar][i][j] > -1) {
						allNotes[bar][i][j] -= 2;
						if (allNotes[bar][i][j] < 0) {
							allNotes[bar][i][j] = 7 + allNotes[bar][i][j];
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
void Notes::correctDynamics(int bar, vector<int> dyns)
{
	if (dynamics[bar].size() != dyns.size()) {
		cout << "vectors don't have the same size\n";
		return;
	}
	for (unsigned i = 0; i < dynamics[bar].size(); i++) {
		dynamics[bar][i] = dyns[i];
	}
}

//--------------------------------------------------------------
void Notes::setNotePositions(int bar)
{
	vector<vector<int>> vvi;
	for (auto vvit = allNotes[bar].begin(); vvit != allNotes[bar].end(); ++vvit) {
		vvi.push_back(vector<int>());
		for (auto vit = vvit->begin(); vit != vvit->end(); ++vit) {
			vvi.back().push_back(0);
		}
	}
	vector<vector<float>> vvf;
	for (auto vvit = allNotes[bar].begin(); vvit != allNotes[bar].end(); ++vvit) {
		vvf.push_back(vector<float>());
		for (auto vit = vvit->begin(); vit != vvit->end(); ++vit) {
			vvf.back().push_back(0);
		}
	}
	//vector<vector<bool>> vvb;
	//for (auto vvit = allNotes[bar].begin(); vvit != allNotes[bar].end(); ++vvit) {
	//	vvb.push_back(vector<bool>());
	//	for (auto vit = vvit->begin(); vit != vvit->end(); ++vit) {
	//		vvb.back().push_back(false);
	//	}
	//}
	vector<int> vi1(allNotes[bar].size(), 1);
	vector<int> vi2(allNotes[bar].size(), 0);
	vector<float> vf1(allNotes[bar].size(), 0);
	vector<float> vf2(allNotes[bar].size(), FLT_MIN);
	vector<float> vf3(allNotes[bar].size(), FLT_MAX);
	//vector<bool> vb(allNotes[bar].size(), false);
	stemDirections[bar] = vi1;
	articulXPos[bar] = vf2;
	articulYPos[bar] = vf2;
	actualDurs[bar] = vi2;
	hasStem[bar] = vi2;
	numTails[bar] = vi2;
	numBeams[bar] = vi2;
	allNotesXCoords[bar] = vvf;
	allNotesYCoords1[bar] = vvf;
	allNotesYCoords2[bar] = vf1;
	extraLinesDir[bar] = vvi;
	extraLinesYPos[bar] = vvi;
	numExtraLines[bar] = vvi;
	hasExtraLines[bar] = vvi;
	allNotesMaxYPos[bar] = vf1;
	allNotesMinYPos[bar] = vf3;
	numOctavesOffset[bar] = vi2;
	whichIdxBase[bar] = vi2;
	whichIdxEdge[bar] = vi2;
	isNoteGroupped[bar] = vi2;
	
	float x = 0;
	int beamsCounter = 0;
	bool fullBeat = false;
	bool addedNotesAtFullBeat = false;
	bool fullBeats[allNotes[bar].size()] = {false};
	unsigned prevFullBeatNdx = 0;
	float durAccum = 0;
	tacet[bar] = false;
	beamIndexCounter = 0;
	beamIndexCounter2 = 0;
	vector<int> numNotesEveryFullBeat(allNotes[bar].size());
	int numNotesFullBeatNdx = 0;
	int nonBeamNumNotes = 0;
	// cast to int to avoid cast further down in if tests
	for (int i = 0; i < (int)allNotes[bar].size(); i++) {
		vector<float> yVec(allNotes[bar][i].size());
		if ((allNotes[bar].size() == 1) && (allNotes[bar][i][0] == -1)) {
			tacet[bar] = true;
			minYCoord[bar] = 0;
			maxYCoord[bar] = minYCoord[bar] + (staffDist * 4);
		}
		// get the duration in musical terms, 1 for whole note
		// 2 for half note, 4 for quarter, etc.
		int durWithoutDots = beats;
		while (durWithoutDots > durations[bar][i]) {
			durWithoutDots /= 2;
		}
		actualDurs[bar][i] = (int)(((float)beats / (float)durWithoutDots) + 0.5);
		// detect the beats based on the denominator
		// so eights and shorter notes are groupped properly
		durAccum += ((float)denominator/((float)beats/(float)denominator)) * \
					((float)durations[bar][i]/(float)denominator);
		// if we get a whole beat or more
		if (durAccum >= 1.0) {
			fullBeat = true;
			fullBeats[i] = true;
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
			x += (durations[bar][i-1] * distBetweenBeats[bar]);
		}
		else if (tacet[bar]) {
			x = xLength / 2;
		}
		// accumulate Ys to determine stem direction
		int stemDirAccum = 0;
		for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
			// set y as the C one octave below middle C
			yVec[j] = halfStaffDist * 17;
			float noteYOffset = (allNotes[bar][i][j] * halfStaffDist) + \
								(allOctaves[bar][i][j] * 7 * halfStaffDist);
			yVec[j] -= noteYOffset;
		}
		// run the loop a second time, since the octave offsets have now been updated
		for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
			allNotesYCoords1[bar][i][j] = allNotesYCoords2[bar][i] = yVec[j];
			// initialize all X coords to x
			allNotesXCoords[bar][i][j] = x;
			if (yVec[j] > allNotesMaxYPos[bar][i] && yVec[i] < FLT_MAX) allNotesMaxYPos[bar][i] = yVec[j];
			if (yVec[j] < allNotesMinYPos[bar][i] && yVec[i] > FLT_MIN) allNotesMinYPos[bar][i] = yVec[j];
			// if we're at B in he middle of the staff or above
			if (yVec[j] <= middleOfStaff) {
				stemDirAccum -= 1;
			}
			else {
				stemDirAccum += 1;
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
					numExtraLines[bar][i][j] = (int)(((yVec[j] + yStartPnt[0]) - (yStartPnt[0] + (staffDist * 4))) / staffDist);
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
		// once we've stored the Y coords of the note heads
		// we run a loop to see if there are any note heads close together
		// which means we'll have to change the X coord of one of them
		// e.g. in an <e f g> sequence, only f will shift on the X axis
		for (unsigned j = 1; j < allNotesYCoords1[bar][i].size(); j++) {
			for (unsigned k = 0; k < allNotesYCoords1[bar][i].size(); k++) {
				// don't bother with the same bar
				if (j == k) continue;
				// if two notes are close together on the Y axis
				if (abs(allNotesYCoords1[bar][i][j] - allNotesYCoords1[bar][i][k]) < staffDist) {
					// if k is on top (smaller Y coord)
					if (allNotesYCoords1[bar][i][j] > allNotesYCoords1[bar][i][k]) {
						// if both j and k haven't moved yet
						if ((allNotesXCoords[bar][i][j] == x) && (allNotesXCoords[bar][i][k] == x)) {
							// move the top note
							allNotesXCoords[bar][i][k] += noteWidth;
						}
					}
					else {
						if ((allNotesXCoords[bar][i][k] == x) && (allNotesXCoords[bar][i][j] == x)) {
							allNotesXCoords[bar][i][j] += noteWidth;
						}
					}
				}
			}
		}
		// get the Y coords of the extreme notes of a chord
		float maxVal =  0;
		float minVal = FLT_MAX;
		if (stemDirAccum >= 0) {
			// local stem dir
			// update stemDirections which is used in drawAccidentals()
			stemDirections[bar][i] = 1;
			for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
				if (allNotesYCoords1[bar][i][j] > maxVal) {
					maxVal = allNotesYCoords1[bar][i][j];
					whichIdxBase[bar][i] = j;
				}
				if (allNotesYCoords1[bar][i][j] < minVal) {
					minVal = allNotesYCoords1[bar][i][j];
					whichIdxEdge[bar][i] = j;
				}
			}
		}
		// stems can point down only if this is not a rhythm staff
		else if (!rhythm) {
			stemDirections[bar][i] = -1;
			for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
				if (allNotesYCoords1[bar][i][j] < minVal) {
					minVal = allNotesYCoords1[bar][i][j];
					whichIdxBase[bar][i] = j;
				}
				if (allNotesYCoords1[bar][i][j] > maxVal) {
					maxVal = allNotesYCoords1[bar][i][j];
					whichIdxEdge[bar][i] = j;
				}
			}
		}
		// set the Y coord to be the lowest note (highest Y value)
		// in case of stem up, otherwise the other way round
		float y = (halfStaffDist * 17);
		float yEdge = y;
		float noteYOffsetBase = (allNotes[bar][i][whichIdxBase[bar][i]] * halfStaffDist) + \
								(allOctaves[bar][i][whichIdxBase[bar][i]] * 7 * halfStaffDist);
		float noteYOffsetEdge = (allNotes[bar][i][whichIdxEdge[bar][i]] * halfStaffDist) + \
								(allOctaves[bar][i][whichIdxEdge[bar][i]] * 7 * halfStaffDist);
		y -= noteYOffsetBase;
		yEdge -= noteYOffsetEdge;
		// caclulate the coords of the stems
		if (hasStem[bar][i]) {
			allNotesYCoords2[bar][i] = allNotesYCoords1[bar][i][0] + (noteHeight / 8.0) - (staffDist / 10.0) - ((stemHeight + halfStaffDist) * stemDirections[bar][i]);
			if (y > allNotesMaxYPos[bar][i] && y < FLT_MAX) allNotesMaxYPos[bar][i] = y;
			if (y < allNotesMinYPos[bar][i] && y > FLT_MIN) allNotesMinYPos[bar][i] = y;
		}
		else if (actualDurs[bar][i] == 1) {
			allNotesYCoords2[bar][i] = y;
			if (y > allNotesMaxYPos[bar][i] && y < FLT_MAX) allNotesMaxYPos[bar][i] = y;
			if (y < allNotesMinYPos[bar][i] && y > FLT_MIN) allNotesMinYPos[bar][i] = y;
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
	int j;
	vector<vector<int>> beamsIndexesLocal;
	for (i = 0; i < allNotes[bar].size(); i++) {
		if (fullBeats[i]) {
			vector<int> v;
			beamsIndexesLocal.push_back(v);
			beamsCounter = 0;
			for (j = prevFullBeatNdx; j <= (int)i; j++) {
				if (actualDurs[bar][j] > 4) { // && allNotes[bar][i][0] > -1) {
					if (beamsCounter == 0) {
						beamsIndexesLocal.back().push_back(j);
					}
					beamsCounter++;
				}
			}
			if (beamsCounter > 1) {
				beamsIndexesLocal.back().push_back(j-1);
			}
			else {
				// we mustn't keep empty vectors or vectors with one item only
				beamsIndexesLocal.pop_back();
			}
			prevFullBeatNdx = i + 1;
		}
	}
	// set which note indexes are beamed
	// this is used in drawNotes() to determine whether we'll draw an individual tail or not
	int nonRestsCounter;
	// also set which beam groups have rests so we make sure we don't draw on top of them
	vector<int> beamGroupHasRests(beamsIndexesLocal.size(), 0);
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
	vector<int> hasRestsVecLocal;
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
	vector<int> maxNumLinesLocal;
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
	// copy the local vectors to the maps of the class
	beamsIndexes[bar] = beamsIndexesLocal;
	hasRests[bar] = hasRestsVecLocal;
	maxNumLines[bar] = maxNumLinesLocal;
	grouppedStemDirs[bar] = vector<int>(beamsIndexes[bar].size());
	numNotesWithBeams[bar] = vector<int>(beamsIndexes[bar].size(), 0);
	// to easily separate notes with beams that are groupped from the ones that are not
	// (e.g. in case of a dotted quarter with an eighth note)
	// we first determine how many notes have beams in a group
	// and then we copy the beamIndexesLocal vector to beamsIndexes as a 2D vector
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
			float averageHeight = 0;
			int stemDir;
			for (j = beamsIndexes[bar][i][0]; j <= beamsIndexes[bar][i][1]; j++) {
				if (allNotes[bar][j][0] > -1) {
					averageHeight += (float)stemDirections[bar][j];
				}
				else {
					if (j > beamsIndexes[bar][i][0]) {
						for (int k = j+1; k < beamsIndexes[bar][i][1]; k++) {
							if (allNotes[bar][k][0] > -1) {
								averageHeight += (float)stemDirections[bar][k];
								break;
							}
						}
					}
					else if (j < beamsIndexes[bar][i][1]) {
						for (int k = beamsIndexes[bar][i][1]; k >=0; k--) {
							if (allNotes[bar][k][0] > -1) {
								averageHeight += (float)stemDirections[bar][k];
								break;
							}
						}
					}
					else {
						averageHeight += (float)stemDirections[bar][j];
					}
				}
			}
			averageHeight /= (float)(beamsIndexes[bar][i][1] - beamsIndexes[bar][i][0] + 1);
			if (averageHeight >= 0) {
				stemDir = 1;
			}
			else {
				stemDir = -1;
			}
			// groupped stem directions for groupped notes
			grouppedStemDirs[bar][i] = stemDir;
			for (j = beamsIndexes[bar][i][0]; j <= beamsIndexes[bar][i][1]; j++) {
				// set the second Y coordinate of the note which is the stem length as the direction might have changed
				allNotesYCoords2[bar][j] = allNotesYCoords1[bar][j][0] + (noteHeight / 8.0) - (staffDist / 10.0) - ((stemHeight + halfStaffDist) * grouppedStemDirs[bar][i]);
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
					if (allNotes[bar][j][0] > -1 && allNotesYCoords2[bar][j] < extremes[0]) {
						extremes[0] = allNotesYCoords2[bar][j];
						extremeNdxs[0] = j;
					}
					if (allNotes[bar][j][0] > -1 && allNotesYCoords2[bar][j] > extremes[1]) {
						extremes[1] = allNotesYCoords2[bar][j];
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
						if (allNotesYCoords2[bar][k] < allNotesYCoords2[bar][k-1]) linear = false;
					}
					if (linear) {
						bool allEqual = true;
						for (int k = beamsIndexes[bar][i][0]+1; k <= beamsIndexes[bar][i][1]; k++) {
							if (allNotesYCoords2[bar][k] != allNotesYCoords2[bar][k-1]) allEqual = false;
						}
						if (allEqual) linear = false;
					}
				}
				// if they rise
				else {
					beamDir = -1;
					for (int k = beamsIndexes[bar][i][0]+1; k <= beamsIndexes[bar][i][1]; k++) {
						if (allNotesYCoords2[bar][k] > allNotesYCoords2[bar][k-1]) linear = false;
					}
					if (linear) {
						bool allEqual = true;
						for (int k = beamsIndexes[bar][i][0]+1; k <= beamsIndexes[bar][i][1]; k++) {
							if (allNotesYCoords2[bar][k] != allNotesYCoords2[bar][k-1]) allEqual = false;
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
					float start = allNotesYCoords2[bar][ndx];
					// if the note with the extreme Y coord is a rest, look for a note instead
					if (allNotes[bar][ndx][0] == -1) {
						for (int k = beamsIndexes[bar][i][0]; k <= beamsIndexes[bar][i][1]; k++) {
							if (allNotes[bar][k][0] > -1) {
								start = allNotesYCoords2[bar][k] - (k * (halfStaffDist * beamDir));
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
						allNotesYCoords2[bar][k] = start;
						start += (halfStaffDist * beamDir);
					}
				}
				else {
					// if the notes don't rise of fall linearly, set all stem coords to the same, maximum value
					for (int k = beamsIndexes[bar][i][0]; k <= beamsIndexes[bar][i][1]; k++) {
						if (grouppedStemDirs[bar][i] == 1) allNotesYCoords2[bar][k] = extremes[0];
						else allNotesYCoords2[bar][k] = extremes[1];
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
						if (allNotesYCoords2[bar][lowestStemIndex] + ((maxNumBeams-1) * (staffDist*BEAMDISTCOEFF)) > 0) {
							offset = allNotesYCoords2[bar][lowestStemIndex] + ((maxNumBeams-1) * (staffDist*BEAMDISTCOEFF));
						}
						for (int k = beamsIndexes[bar][i][0]; k <= beamsIndexes[bar][i][1]; k++) {
							allNotesYCoords2[bar][k] -= offset;
							if (allNotesMinYPos[bar][k] > allNotesYCoords2[bar][k]) {
								allNotesMinYPos[bar][k] = allNotesYCoords2[bar][k];
							}
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
						if (allNotesYCoords2[bar][highestStemIndex] - ((maxNumBeams-1) * (staffDist*BEAMDISTCOEFF)) < (staffDist * 4)) {
							offset = (staffDist * 4) - (allNotesYCoords2[bar][highestStemIndex] - ((maxNumBeams-1) * (staffDist*BEAMDISTCOEFF)));
						}
						for (int k = beamsIndexes[bar][i][0]; k <= beamsIndexes[bar][i][1]; k++) {
							allNotesYCoords2[bar][k] += offset;
							if (allNotesMaxYPos[bar][k] < allNotesYCoords2[bar][k]) {
								allNotesMaxYPos[bar][k] = allNotesYCoords2[bar][k];
							}
						}
					}
				}
			}
		}
	}
	storeArticulationsCoords(bar);
	storeSlurCoords(bar);
	storeOttavaCoords(bar);
	storeTextCoords(bar);
	storeDynamicsCoords(bar);
	storeMinMaxY(bar);
}

//--------------------------------------------------------------
// once all bars have their notes and the note positions set
// a function is called that calculates the offset needed in the Y axis
// to make sure the notes, dynamics, etc. of one staff does not collide with another
void Notes::correctYAnchor(float yAnchor1, float yAnchor2)
{
	yStartPnt[0] = yAnchor1;
	yStartPntRecenter[0] = yAnchor1;
	yStartPnt[1] = yAnchor2;
	yStartPntRecenter[1] = yAnchor2;
}

//--------------------------------------------------------------
void Notes::moveScoreX(float numPixels)
{
	xOffset += numPixels;
}

//--------------------------------------------------------------
void Notes::moveScoreY(float numPixels)
{
	yStartPnt[scoreOrientation] += numPixels;
}

//--------------------------------------------------------------
void Notes::recenterScore()
{
	yStartPnt[scoreOrientation] = yStartPntRecenter[scoreOrientation];
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
		if (allArticulations[bar][i] > 0) {
			float xPos = allNotesXCoords[bar][i][whichIdxBase[bar][i]];
			if (stemDirections[bar][i] > 0) xPos -= (noteWidth/4);
			else xPos -= (noteWidth/2);
			float yPos = allNotesYCoords1[bar][i][whichIdxBase[bar][i]];
			if (stemDirections[bar][i] > 0) {
				yPos += staffDist;
				if (yPos > allNotesMaxYPos[bar][i] && yPos < FLT_MAX) allNotesMaxYPos[bar][i] = yPos;
			}
			else {
				yPos -= staffDist;
				if (yPos < allNotesMinYPos[bar][i] && yPos > FLT_MIN) allNotesMinYPos[bar][i] = yPos;
			}
			// check if the note head is on a line, but only inside the stave
			if ((allNotesYCoords1[bar][i][whichIdxBase[bar][i]] > 0) && \
					(allNotesYCoords1[bar][i][whichIdxBase[bar][i]] < (staffDist*5)) && \
					!rhythm) {
				if (((int)(abs(allNotesYCoords1[bar][i][whichIdxBase[bar][i]]) / halfStaffDist) % 2) == 0) {
					if (stemDirections[bar][i] > 0) yPos += halfStaffDist;
					else  yPos -= halfStaffDist;
					if (yPos + (notationFont.stringHeight(articulSyms[allArticulations[bar][i]-1])/2) > allNotesMaxYPos[bar][i] && yPos < FLT_MAX) {
						// articulation symbols start with index 1, because 0 is reserved for no articulation
						// so we subtract 1 from the index
						allNotesMaxYPos[bar][i] = yPos + (notationFont.stringHeight(articulSyms[allArticulations[bar][i]-1])/2);
					}
					if (yPos - (notationFont.stringHeight(articulSyms[allArticulations[bar][i]-1])/2) < allNotesMinYPos[bar][i] && yPos > FLT_MIN) {
						allNotesMinYPos[bar][i] = yPos - (notationFont.stringHeight(articulSyms[allArticulations[bar][i]-1])/2);
					}
				}
			}
			articulXPos[bar][i] = xPos;
			articulYPos[bar][i] = yPos;
			if (allArticulations[bar][i] == 7) {
				float yPosPortando;
				if (stemDirections[bar][i] > 0) {
					// check if the note head is inside the stave
					if (allNotesYCoords1[bar][i][whichIdxBase[bar][i]] < (staffDist*5)) {
						yPosPortando = staffDist * 5;
					}
					else {
						yPosPortando = allNotesYCoords1[bar][i][whichIdxBase[bar][i]] + noteHeight;
					}
				}
				else {
					// check if the note head is inside the stave
					if (allNotesYCoords1[bar][i][whichIdxBase[bar][i]] > 0) {
						yPosPortando = -staffDist;
					}
					else {
						yPosPortando = allNotesYCoords1[bar][i][whichIdxBase[bar][i]] - noteHeight;
					}
				}
				// if case the articulation is a portando, we must get the height as in the line below
				float halfArticulHeight = (notationFont.stringHeight(".") + notationFont.stringHeight("_")) / 2;
				if (yPosPortando + halfArticulHeight > allNotesMaxYPos[bar][i] && yPosPortando < FLT_MAX) {
					allNotesMaxYPos[bar][i] = yPosPortando + halfArticulHeight;
				}
				if (yPosPortando - halfArticulHeight < allNotesMinYPos[bar][i] && yPosPortando > FLT_MIN) {
					allNotesMinYPos[bar][i] = yPosPortando - halfArticulHeight;
				}
				articulYPos[bar][i] = yPosPortando;
			}
		}
	}
}

//--------------------------------------------------------------
void Notes::storeSlurCoords(int bar)
{
	int dirAccum = 0;
	// the vector below is used to calculate the orientation of the middle points of the curve
	vector<int> slurDir(slurBeginnings[bar].size(), 1);
	for (unsigned i = 0; i < slurBeginnings[bar].size(); i++) {
		for (int j = 0; j < (int)allNotes[bar].size(); j++) {
			if (slurBeginnings[bar][i] == j) {
				// find where the slur ends
				unsigned slurEndNdx;
				for (slurEndNdx = slurBeginnings[bar][i]+1; slurEndNdx < slurEndings[bar].size(); slurEndNdx++) {
					if (slurEndings[bar][slurEndNdx] > -1) break;
				}
				// the direction of the slur will depend on the majority of stem directions
				for (int k = slurBeginnings[bar][i]; k <= slurEndings[bar][slurEndNdx]; k++) {
					dirAccum += stemDirections[bar][k];
				}
				// if the accumulation is 0 it means we're even so we chose to go below the staff
				if (dirAccum == 0) {
					slurDir[i] = slurDir[slurEndings[bar][slurEndNdx]] = 1;
				}
				else {
					slurDir[i] = slurDir[slurEndings[bar][slurEndNdx]] = (dirAccum > 0 ? 1 : 0);
				}
			}
		}
	}
	slurStartX[bar] = vector<float>(slurBeginnings[bar].size(), -1);
	slurStartY[bar] = vector<float>(slurBeginnings[bar].size(), -1);
	slurStopX[bar] = vector<float>(slurBeginnings[bar].size(), -1);
	slurStopY[bar] = vector<float>(slurBeginnings[bar].size(), -1);
	slurMiddleX1[bar] = vector<float>(slurBeginnings[bar].size(), -1);
	slurMiddleX2[bar] = vector<float>(slurBeginnings[bar].size(), -1);
	slurMiddleY1[bar] = vector<float>(slurBeginnings[bar].size(), -1);
	slurMiddleY2[bar] = vector<float>(slurBeginnings[bar].size(), -1);
	for (unsigned i = 0; i < slurBeginnings[bar].size(); i++) {
		for (int j = 0; j < (int)allNotes[bar].size(); j++) {
			if (slurBeginnings[bar][i] == j) {
				slurStartX[bar][i] = allNotesXCoords[bar][j][whichIdxBase[bar][j]];
				if (slurDir[i] > 0) {
					if (stemDirections[bar][j] > 0) {
						// if slur and stem directions are the same, we store the Y coord of the note head
						slurStartY[bar][i] = allNotesYCoords1[bar][j][whichIdxBase[bar][j]];
						slurStartY[bar][i] += noteHeight;
						if (slurStartY[bar][i] < allNotesMaxYPos[bar][j] && slurStartY[bar][i] < FLT_MAX) {
							slurStartY[bar][i] = allNotesMaxYPos[bar][j] + noteHeight;
							allNotesMaxYPos[bar][j] = slurStartY[bar][i]; // + (noteHeight/2);
						}
					}
					else {
						// otherwise we store the Y coord of the note stem (allNotesYCoords2)
						slurStartY[bar][i] = allNotesYCoords2[bar][j];
						slurStartY[bar][i] += halfStaffDist;
						if (slurStartY[bar][i] < allNotesMaxYPos[bar][j] && slurStartY[bar][i] < FLT_MAX) {
							slurStartY[bar][i] = allNotesMaxYPos[bar][j] + halfStaffDist;
							allNotesMaxYPos[bar][j] = slurStartY[bar][i]; // + (noteHeight/2);
						}
					}
				}
				else {
					if (stemDirections[bar][j] < 0) {
						slurStartY[bar][i] = allNotesYCoords1[bar][j][whichIdxBase[bar][j]];
						slurStartY[bar][i] -= noteHeight;
						if (slurStartY[bar][i] > allNotesMinYPos[bar][j] && slurStartY[bar][i] > FLT_MIN) {
							slurStartY[bar][i] = allNotesMinYPos[bar][j] - noteHeight;
							allNotesMinYPos[bar][j] = slurStartY[bar][i]; // - (noteHeight/2);
						}
					}
					else {
						slurStartY[bar][i] = allNotesYCoords2[bar][j];
						slurStartY[bar][i] -= halfStaffDist;
						if (slurStartY[bar][i] > allNotesMinYPos[bar][j] && slurStartY[bar][i] > FLT_MIN) {
							slurStartY[bar][i] = allNotesMinYPos[bar][j] - halfStaffDist;
							allNotesMinYPos[bar][j] = slurStartY[bar][i]; // - (noteHeight/2);
						}
					}
				}
			}
		}
	}
	for (unsigned i = 0; i < slurEndings[bar].size(); i++) {
		for (int j = 0; j < (int)allNotes[bar].size(); j++) {
			if (slurEndings[bar][i] == j) {
				slurStopX[bar][i] = allNotesXCoords[bar][j][whichIdxBase[bar][j]];
				if (slurDir[i] > 0) {
					if (stemDirections[bar][j] > 0) {
						slurStopY[bar][i] = allNotesYCoords1[bar][j][whichIdxBase[bar][j]];
						slurStopY[bar][i] += noteHeight;
						if (slurStopY[bar][i] < allNotesMaxYPos[bar][j] && slurStopY[bar][i] < FLT_MAX) {
							slurStopY[bar][i] = allNotesMaxYPos[bar][j] + noteHeight;
							allNotesMaxYPos[bar][j] = slurStopY[bar][i]; // + (noteHeight/2);
						}
					}
					else {
						slurStopY[bar][i] = allNotesYCoords2[bar][j];
						slurStopY[bar][i] += halfStaffDist;
						if (slurStopY[bar][i] < allNotesMaxYPos[bar][j] && slurStopY[bar][i] < FLT_MAX) {
							slurStopY[bar][i] = allNotesMaxYPos[bar][j] + halfStaffDist;
							allNotesMaxYPos[bar][j] = slurStopY[bar][i]; // + (noteHeight/2);
						}
					}
				}
				else {
					if (stemDirections[bar][j] < 0) {
						slurStopY[bar][i] = allNotesYCoords1[bar][j][whichIdxBase[bar][j]];
						slurStopY[bar][i] -= noteHeight;
						if (slurStopY[bar][i] > allNotesMinYPos[bar][j] && slurStopY[bar][i] > FLT_MIN) {
							slurStopY[bar][i] = allNotesMinYPos[bar][j] - noteHeight;
							allNotesMinYPos[bar][j] = slurStopY[bar][i]; // - (noteHeight/2);
						}
					}
					else {
						slurStopY[bar][i] = allNotesYCoords2[bar][j];
						slurStopY[bar][i] -= halfStaffDist;
						if (slurStopY[bar][i] > allNotesMinYPos[bar][j] && slurStopY[bar][i] > FLT_MIN) {
							slurStopY[bar][i] = allNotesMinYPos[bar][j] - halfStaffDist;
							allNotesMinYPos[bar][j] = slurStopY[bar][i]; // - (noteHeight/2);
						}
					}
				}
			}
		}
	}
	for (unsigned i = 0; i < slurBeginnings[bar].size(); i++) {
		if (slurBeginnings[bar][i] > -1) {
			// find the index of the end of slur
			unsigned slurStopNdx;
			for (slurStopNdx = i + 1; slurStopNdx < slurStopX[bar].size(); slurStopNdx++) {
				if (slurStopX[bar][slurStopNdx] > -1) break;
			}
			float curveSegment = (slurStopX[bar][slurStopNdx] - slurStartX[bar][i]) / 3.0;
			float middleX = slurStartX[bar][i] + curveSegment;
			slurMiddleX1[bar][i] = middleX;
			middleX += curveSegment;
			slurMiddleX2[bar][i] = middleX;
			float middleY1;
			if (slurDir[i] > 0) {
				middleY1 = slurStartY[bar][i] > slurStopY[bar][i] ? \
						   slurStartY[bar][i] : slurStopY[bar][i];
			}
			else {
				middleY1 = slurStartY[bar][i] < slurStopY[bar][i] ? \
						   slurStartY[bar][i] : slurStopY[bar][i];
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
			for (int j = slurBeginnings[bar][i]; j < slurEndings[bar][i]; j++) {
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
					if (j > 0 && j < slurEndings[bar][i]-1) {
						if ((allNotesMaxYPos[bar][j] < middleY1 && middleY1 < FLT_MAX) || (allNotesMaxYPos[bar][j] < middleY2 && middleY2 < FLT_MAX)) {
							allNotesMaxYPos[bar][j] = max(middleY1, middleY2); // + (noteHeight/2);
						}
					}
				}
				else {
					if (middleY1 > allNotesMinYPos[bar][j]) {
						middleY1 = allNotesMinYPos[bar][j] + halfStaffDist;
					}
					if (middleY2 > allNotesMinYPos[bar][j]) {
						middleY2 = allNotesMinYPos[bar][j] + halfStaffDist;
					}
					if (j > 0 && j < slurEndings[bar][i]-1) {
						if ((allNotesMinYPos[bar][j] > middleY1 && middleY1 > FLT_MIN) || (allNotesMinYPos[bar][j] < middleY2 && middleY2 > FLT_MIN)) {
							allNotesMinYPos[bar][j] = min(middleY1, middleY2); // - (noteHeight/2);
						}
					}
				}
			}
			slurMiddleY1[bar][i] = middleY1;
			slurMiddleY2[bar][i] = middleY2;
		}
	}
}

//--------------------------------------------------------------
void Notes::storeOttavaCoords(int bar)
{
	allOttavasYCoords[bar] = vector<float>(allOttavas[bar].size(), FLT_MIN);
	int prevOttava = 0;
	int index = -1;
	for (unsigned i = 0; i < allOttavas[bar].size(); i++) {
		if (allOttavas[bar][i] != 0) {
			if (allOttavas[bar][i] != prevOttava) {
				// run the chunk below only if this is not the first ottava symbol
				if (index > -1 && ((int)i - index > 1)) {
					float minVal = FLT_MAX, maxVal = FLT_MIN;
					for (int j = index; j < (int)i; j++) {
						// make sure the horizontal line of the ottava doesn't fall on anything else
						if (allOttavas[bar][j] > 0) {
							if (allOttavasYCoords[bar][j] < allNotesMinYPos[bar][j]) {
								minVal = min(minVal, allNotesMinYPos[bar][j] - noteHeight + halfStaffDist);
								if (minVal < allNotesMinYPos[bar][j] && minVal > FLT_MIN) {
									allNotesMinYPos[bar][j] = minVal;
								}
							}
						}
						else if (allOttavas[bar][j] < 0) {
							if (allOttavasYCoords[bar][j] > allNotesMaxYPos[bar][j]) {
								maxVal = max(maxVal, allNotesMaxYPos[bar][j] - noteHeight + halfStaffDist);
								if (maxVal > allNotesMaxYPos[bar][j] && maxVal < FLT_MAX) {
									allNotesMaxYPos[bar][j] = maxVal;
								}
							}
						}
					}
					for (int j = index; j < (int)i; j++) {
						if (allOttavas[bar][j] > 0) allOttavasYCoords[bar][j] = minVal;
						else if (allOttavas[bar][j] < 0) allOttavasYCoords[bar][j] = maxVal;
					}
				}
				index = (int)i;
				float yPos;
				float textHalfHeight = notationFont.stringHeight(octaveSyms[abs(allOttavas[bar][i])]) / 2;
				if (allOttavas[bar][i] > 0) {
					// the compiler complains if I just compare with 0.0
					// so I create a float here
					float zero = 0.0;
					if (stemDirections[bar][i] > 0) {
						yPos = min(allNotesMinYPos[bar][i], min(allNotesYCoords2[bar][i], zero));
					}
					else {
						yPos = min(allNotesMinYPos[bar][i], min(allNotesYCoords1[bar][i][0], zero));
					}
					yPos -= (noteHeight + halfStaffDist);
					if (yPos - textHalfHeight < allNotesMinYPos[bar][i] && yPos > FLT_MIN) {
						allNotesMinYPos[bar][i] = yPos; // - textHalfHeight;
					}
				}
				else {
					if (stemDirections[bar][i] > 0) {
						yPos = max(allNotesMaxYPos[bar][i], max(allNotesYCoords1[bar][i][0],
									(halfStaffDist*9)));
					}
					else {
						yPos = max(allNotesMaxYPos[bar][i], max(allNotesYCoords2[bar][i],
									(halfStaffDist*9)));
					}
					yPos += (noteHeight + halfStaffDist);
					if (yPos + textHalfHeight > allNotesMaxYPos[bar][i] && yPos < FLT_MAX) {
						allNotesMaxYPos[bar][i] = yPos; // + textHalfHeight;
					}
				}
				allOttavasYCoords[bar][i] = yPos;
				prevOttava = allOttavas[bar][i];
			}
		}
	}
	// then the loop that corrects the Y coords for the ottava symbol and its line for the last ottava symbol
	// this must be done outside of the loop above to work
	if (index > -1 && ((int)allOttavas[bar].size() - index > 1)) {
		float minVal = FLT_MAX, maxVal = FLT_MIN;
		for (int j = index; j < (int)allOttavas[bar].size(); j++) {
			// make sure the horizontal line of the ottava doesn't fall on anything else
			if (allOttavas[bar][j] > 0) {
				if (allOttavasYCoords[bar][j] < allNotesMinYPos[bar][j]) {
					minVal = min(minVal, allNotesMinYPos[bar][j] - noteHeight + halfStaffDist);
					if (minVal < allNotesMinYPos[bar][j] && minVal > FLT_MIN) {
						allNotesMinYPos[bar][j] = minVal;
					}
				}
			}
			else if (allOttavas[bar][j] < 0) {
				if (allOttavasYCoords[bar][j] > allNotesMaxYPos[bar][j]) {
					maxVal = max(maxVal, allNotesMaxYPos[bar][j] - noteHeight + halfStaffDist);
					if (maxVal > allNotesMaxYPos[bar][j] && maxVal < FLT_MAX) {
						allNotesMaxYPos[bar][j] = maxVal;
					}
				}
			}
		}
		for (int j = index; j < (int)allOttavas[bar].size(); j++) {
			if (allOttavas[bar][j] > 0) allOttavasYCoords[bar][j] = minVal;
			else if (allOttavas[bar][j] < 0) allOttavasYCoords[bar][j] = maxVal;
		}
	}
}

//--------------------------------------------------------------
void Notes::storeTextCoords(int bar)
{
	size_t size = 0;
	for (unsigned i = 0; i < allTextsIndexes[bar].size(); i++) {
		if (allTextsIndexes[bar][i] != 0) {
			size++;
		}
	}
	allTextsXCoords[bar] = vector<float>(size);
	allTextsYCoords[bar] = vector<float>(size);
	int index = 0;
	for (unsigned i = 0; i < allTextsIndexes[bar].size(); i++) {
		if (allTextsIndexes[bar][i] != 0) {
			allTextsXCoords[bar][index] = allNotesXCoords[bar][i][whichIdxBase[bar][i]]-(noteWidth/2.0);
			float yPos;
			float textHalfHeight = textFont.stringHeight(allTexts[bar][index]) / 2;
			if (allTextsIndexes[bar][i] > 0) {
				// the compiler complains if I just compare with 0.0
				// so I create a float here
				float zero = 0.0;
				if (stemDirections[bar][i] > 0) {
					yPos = min(allNotesMinYPos[bar][i], min(allNotesYCoords2[bar][i], zero));
				}

				else {
					yPos = min(allNotesMinYPos[bar][i], min(allNotesYCoords1[bar][i][0], zero));
				}
				yPos -= (noteHeight + halfStaffDist);
				if (yPos - textHalfHeight < allNotesMinYPos[bar][i] && yPos > FLT_MIN) {
					allNotesMinYPos[bar][i] = yPos; // - textHalfHeight;
				}
			}
			else {
				if (stemDirections[bar][i] > 0) {
					yPos =  max(allNotesMaxYPos[bar][i], max(allNotesYCoords1[bar][i][0],
								(halfStaffDist*9)));
				}
				else {
					yPos =  max(allNotesMaxYPos[bar][i], max(allNotesYCoords2[bar][i],
								(halfStaffDist*9)));
				}
				yPos += (noteHeight + halfStaffDist);
				if (yPos + textHalfHeight > allNotesMaxYPos[bar][i] && yPos < FLT_MAX) {
					allNotesMaxYPos[bar][i] = yPos; // + textHalfHeight;
				}
			}
			allTextsYCoords[bar][index] = yPos;
			index++;
		}
	}
}

//--------------------------------------------------------------
void Notes::storeDynamicsCoords(int bar)
{
	dynsXCoords[bar] = vector<float>(dynamicsIndexes[bar].size(), 0);
	dynsYCoords[bar] = vector<float>(dynamicsIndexes[bar].size(), 0);
	dynsRampStartXCoords[bar] = vector<float>(dynamicsRampStart[bar].size(), 0);
	dynsRampStartYCoords[bar] = vector<float>(dynamicsRampStart[bar].size(), 0);
	dynsRampEndXCoords[bar] = vector<float>(dynamicsRampEnd[bar].size(), 0);
	dynsRampEndYCoords[bar] = vector<float>(dynamicsRampEnd[bar].size(), 0);
	// find the X Y coordinate for dynamics symbols
	for (unsigned i = 0; i < dynamicsIndexes[bar].size(); i++) {
		// cast to int to avoid cast in if test below
		for (int j = 0; j < (int)allNotesMaxYPos[bar].size(); j++) {
			if (dynamicsIndexes[bar][i] == j) {
				float y = max((staffDist*4), allNotesMaxYPos[bar][j]);
				y += (notationFont.stringHeight(dynSyms[dynamics[bar][i]]) * 1.2); // give a bit of room with 1.2
				if (rhythm) y -= (staffDist * 2);
				if (y > allNotesMaxYPos[bar][j]) allNotesMaxYPos[bar][j] = y;
				dynsXCoords[bar][i] = allNotesXCoords[bar][j][whichIdxBase[bar][j]]-dynSymsWidths[dynamics[bar][i]]/2;
				dynsYCoords[bar][i] = y;
			}
		}
	}
	// then find the X Y coordinates for crescendi and decrescendi
	for (unsigned i = 0; i < dynamicsRampStart[bar].size(); i++) {
		// cast to int to avoid cast in if test below
		for (int j = 0; j < (int)allNotesMaxYPos[bar].size(); j++) {
			if (dynamicsRampStart[bar][i] == j) {
				float y = max((staffDist*4), allNotesMaxYPos[bar][j]);
				if (y > allNotesMaxYPos[bar][j]) allNotesMaxYPos[bar][j] = y;
				dynsRampStartXCoords[bar][i] = allNotesXCoords[bar][j][whichIdxBase[bar][j]];
				dynsRampStartYCoords[bar][i] = y;
			}
		}
	}
	for (unsigned i = 0; i < dynamicsRampEnd[bar].size(); i++) {
		// cast to int to avoid cast in if test below
		for (int j = 0; j < (int)allNotesMaxYPos[bar].size(); j++) {
			if (dynamicsRampEnd[bar][i] == j) {
				float y = max((staffDist*4), allNotesMaxYPos[bar][j]);
				if (y > allNotesMaxYPos[bar][j]) allNotesMaxYPos[bar][j] = y;
				dynsRampEndXCoords[bar][i] = allNotesXCoords[bar][j][whichIdxBase[bar][j]];
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
								dynsRampStartYCoords[bar][i] = max(dynsRampStartYCoords[bar][i], allNotesMaxYPos[bar][k]);
								if (dynsRampStartYCoords[bar][i] > allNotesMaxYPos[bar][k]) {
									allNotesMaxYPos[bar][k] = dynsRampStartYCoords[bar][i];
								}
							}
							// check if the currect ramp ends at the next dynamic
							if (nextRampEndNdx == nextDynNdx) {
								dynsRampEndXCoords[bar][nextRampEndNdx] -= dynSymsWidths[dynamics[bar][nextDynNdx]]/2.0;
								dynsRampStartYCoords[bar][i] = max(dynsRampStartYCoords[bar][i], dynsYCoords[bar][nextDynNdx]);
								dynsRampEndYCoords[bar][nextRampEndNdx] = max(dynsRampStartYCoords[bar][i], dynsRampEndYCoords[bar][nextRampEndNdx]);
								for (int k = dynamicsIndexes[bar][j]; k < dynamicsIndexes[bar][nextDynNdx]; k++) {
									if (dynsRampEndYCoords[bar][nextRampEndNdx] > allNotesMaxYPos[bar][k]) {
										allNotesMaxYPos[bar][k] = dynsRampEndYCoords[bar][nextRampEndNdx];
									}
								}
								// allign the two dynamics as well
								dynsYCoords[bar][j] = dynsYCoords[bar][nextDynNdx] = max(dynsYCoords[bar][j], dynsYCoords[bar][nextDynNdx]);
								for (int k = dynamicsIndexes[bar][j]; k < dynamicsIndexes[bar][nextDynNdx]; k++) {
									if (dynsYCoords[bar][i] > allNotesMaxYPos[bar][k]) {
										allNotesMaxYPos[bar][k] = dynsYCoords[bar][i];
									}
								}
							}
							// otherwise set the start and end of the dynamic to the same Y coord
							else {
								dynsRampStartYCoords[bar][i] = dynsRampEndYCoords[bar][nextRampEndNdx] = max(dynsRampStartYCoords[bar][i], dynsRampEndYCoords[bar][nextRampEndNdx]);
								for (int k = dynamicsIndexes[bar][j]; k < dynamicsIndexes[bar][nextDynNdx]; k++) {
									if (dynsRampStartYCoords[bar][i] > allNotesMaxYPos[bar][k]) {
										allNotesMaxYPos[bar][k] = dynsRampStartYCoords[bar][i];
									}
								}
							}
						}
						// if a ramp ends at the end of the bar and there are no more dynamics
						else {
							dynsRampStartYCoords[bar][i] = dynsRampEndYCoords[bar][nextRampEndNdx] = max(dynsRampStartYCoords[bar][i], dynsRampEndYCoords[bar][nextRampEndNdx]);
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
					unsigned prevDynNdx;
					for (prevDynNdx = j - 1; prevDynNdx >= 0; prevDynNdx--) {
						if (dynamicsIndexes[bar][prevDynNdx] > -1) break;
					}
					unsigned nextDynNdx;
					for (nextDynNdx = j + 1; nextDynNdx < dynamicsIndexes[bar].size(); nextDynNdx++) {
						if (dynamicsIndexes[bar][nextDynNdx] > -1) break;
					}
					// if no other dynamic is found after the end of the current one
					// nextDynNdx will be one greater than the last element of dynamicsIndexes[bar]
					if (nextDynNdx >= dynamicsIndexes[bar].size()) nextDynNdx = dynamicsIndexes[bar].size() - 1;
					// find the previous start of a dynamic ramp
					unsigned prevRampStartNdx;
					for (prevRampStartNdx = i; prevRampStartNdx >= 0; prevRampStartNdx--) {
						if (dynamicsRampStart[bar][prevRampStartNdx] > -1) break;
					}
					if (dynamicsRampEnd[bar][i] == dynamicsIndexes[bar][j]) {
						dynsRampEndXCoords[bar][i] -= dynSymsWidths[dynamics[bar][j]]/2.0;
						// check if the beginning of the ramp doesn't start from a dynamic sign
						if (j > 0) {
							if (dynamicsRampStart[bar][prevRampStartNdx] != dynamicsIndexes[bar][prevDynNdx]) {
								dynsRampEndYCoords[bar][i] = max(dynsRampEndYCoords[bar][i], dynsYCoords[bar][j]);
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
						// if j is 0 it means that we end the ramp at the first dynamic sign
						// so the ramp definitely doesn't start from a dynamic sign
						else {
							dynsRampEndYCoords[bar][i] = max(dynsRampEndYCoords[bar][i], dynsYCoords[bar][j]);
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
}

//--------------------------------------------------------------
void Notes::storeMinMaxY(int bar)
{
	for (unsigned i = 0; i < allNotes[bar].size(); i++) {
		// compare allNotesMaxYPos[bar][i] against FLT_MAX to filter it out
		// we have an eight rest on the downbeat
		if (allNotesMaxYPos[bar][i] > maxYCoord[bar] && allNotesMaxYPos[bar][i] < FLT_MAX && !tacet[bar])
			maxYCoord[bar] = allNotesMaxYPos[bar][i];
		if (allNotesMinYPos[bar][i] < minYCoord[bar] && allNotesMinYPos[bar][i] > FLT_MIN && !tacet[bar])
			minYCoord[bar] = allNotesMinYPos[bar][i];
	}
	if (maxYCoord[bar] < (staffDist*4) && !tacet[bar]) {
		maxYCoord[bar] = (staffDist*4);
	}
	if (minYCoord[bar] > 0 && !tacet[bar]) minYCoord[bar] = 0;
}

//--------------------------------------------------------------
void Notes::copyMelodicLine(int barIndex, int barToCopy)
{
	allNotes[barIndex] = allNotes[barToCopy];
	hasExtraLines[barIndex] = hasExtraLines[barToCopy];
	allNotesXCoords[barIndex] = allNotesXCoords[barToCopy];
	allNotesYCoords1[barIndex] = allNotesYCoords1[barToCopy];
	allNotesYCoords2[barIndex] = allNotesYCoords2[barToCopy];
	extraLinesYPos[barIndex] = extraLinesYPos[barToCopy];
	numExtraLines[barIndex] = numExtraLines[barToCopy];
	extraLinesDir[barIndex] = extraLinesDir[barToCopy];
	actualDurs[barIndex] = actualDurs[barToCopy];
	whichIdxBase[barIndex] = whichIdxBase[barToCopy];
	whichIdxEdge[barIndex] = whichIdxEdge[barToCopy];
	dotIndexes[barIndex] = dotIndexes[barToCopy];
	hasStem[barIndex] = hasStem[barToCopy];
	stemDirections[barIndex] = stemDirections[barToCopy];
	numTails[barIndex] = numTails[barToCopy];
	numNotesWithBeams[barIndex] = numNotesWithBeams[barToCopy];
	numBeams[barIndex] = numBeams[barToCopy];
	beamsIndexes[barIndex] = beamsIndexes[barToCopy];
	grouppedStemDirs[barIndex] = grouppedStemDirs[barToCopy];
	allAccidentals[barIndex] = allAccidentals[barToCopy];
	allGlissandi[barIndex] = allGlissandi[barToCopy];
	allArticulations[barIndex] = allArticulations[barToCopy];
	articulXPos[barIndex] = articulXPos[barToCopy];
	articulYPos[barIndex] = articulYPos[barToCopy];
	allTexts[barIndex] = allTexts[barToCopy];
	allTextsXCoords[barIndex] = allTextsXCoords[barToCopy];
	allTextsYCoords[barIndex] = allTextsYCoords[barToCopy];
	slurStartX[barIndex] = slurStartX[barToCopy];
	slurStartY[barIndex] = slurStartY[barToCopy];
	slurStopX[barIndex] = slurStopX[barToCopy];
	slurStopY[barIndex] = slurStopY[barToCopy];
	slurMiddleX1[barIndex] = slurMiddleX1[barToCopy];
	slurMiddleX2[barIndex] = slurMiddleX2[barToCopy];
	slurMiddleY1[barIndex] = slurMiddleY1[barToCopy];
	slurMiddleY2[barIndex] = slurMiddleY2[barToCopy];
	dynamics[barIndex] = dynamics[barToCopy];
	dynsXCoords[barIndex] = dynsXCoords[barToCopy];
	dynsYCoords[barIndex] = dynsYCoords[barToCopy];
	dynamicsRampStart[barIndex] = dynamicsRampStart[barToCopy];
	dynamicsRampEnd[barIndex] = dynamicsRampEnd[barToCopy];
	dynsRampStartXCoords[barIndex] = dynsRampStartXCoords[barToCopy];
	dynsRampEndXCoords[barIndex] = dynsRampEndXCoords[barToCopy];
	dynsRampStartYCoords[barIndex] = dynsRampStartYCoords[barToCopy];
	dynsRampEndYCoords[barIndex] = dynsRampEndYCoords[barToCopy];
	dynamicsRampDir[barIndex] = dynamicsRampDir[barToCopy];
	minYCoord[barIndex] = minYCoord[barToCopy];
	maxYCoord[barIndex] = maxYCoord[barToCopy];
	durations[barIndex] = durations[barToCopy];
	naturalSignsNotWritten[barIndex] = naturalSignsNotWritten[barToCopy];
	isNoteGroupped[barIndex] = isNoteGroupped[barToCopy];
	allOttavas[barIndex] = allOttavas[barToCopy];
	allOttavasYCoords[barIndex] = allOttavasYCoords[barToCopy];
}

//--------------------------------------------------------------
void Notes::insertNaturalSigns(int bar, int loopNdx, vector<int> *v)
{
	// dummy comment
	// create an array of booleans where the index is the note and the value
	// determines whether this note has already been corrected
	int notesAlreadyCorrected[7] = {0};
	// iterate backwards throuh the bars already displayed to see if we need to add a natural sign
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
void Notes::drawNotes(int bar, int loopNdx, vector<int> *v, float xStartPnt, float yOffset, bool animation, float xCoef)
{
	if (allNotes.find(bar) == allNotes.end()) return;
	float xCoefLocal = xCoef;
	if (scoreOrientation == 0) xCoefLocal = 1;
	if (!mute) {
		// first determine if there is any natural sign that needs to be inserted
		// the second argument is the index within the loop that displays the bars horizontally
		// the third is a pointer to the vector that holds the bar indexes (map keys) of the currently playing loop
		if (loopNdx > 0) insertNaturalSigns(bar, loopNdx, v);
		for (unsigned i = 0; i < allNotes[bar].size(); i++) {
			int restsColor = 0;
			for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
				if (hasExtraLines[bar][i][j]) {
					float xStartLocal = xStartPnt + (allNotesXCoords[bar][i][j] * xCoefLocal);
					float yPos = yStartPnt[scoreOrientation] + extraLinesYPos[bar][i][j] + yOffset;
					// draw the extra lines first so the animation is drawn on top
					for (int k = 0; k < abs(numExtraLines[bar][i][j]); k++) {
						yPos += (staffDist * extraLinesDir[bar][i][j]);
						ofDrawLine(xStartLocal - (noteWidth*EXTRALINECOEFF), yPos, xStartLocal + (noteWidth*EXTRALINECOEFF), yPos);
					}
				}
				if (j == 0) {
					// then set the animation color
					if (animation && animate && whichNote == (int)i) {
						ofSetColor(255 * ((ofApp*)ofGetAppPtr())->brightnessCoeff, 0, 0);
						restsColor = 255 * ((ofApp*)ofGetAppPtr())->brightnessCoeff;
					}
				}
				// then draw all the notes heads and rests
				if (allNotes[bar][i][j] > -1) {
					int noteIdx = (actualDurs[bar][i] < 4 ? actualDurs[bar][i] - 1 : 2);
					float x = xStartPnt + (allNotesXCoords[bar][i][j] * xCoefLocal);
					float y = yStartPnt[scoreOrientation] + allNotesYCoords1[bar][i][j]+(noteHeight/8.0)-(staffDist/10.0) + yOffset;
					notationFont.drawString(notesSyms[noteIdx], x-(noteWidth/2.0), y);
					if (dotIndexes[bar][i] > 0) {
						// if the note is on a line or we have a rest
						if (((int)(((y+yOffset) - yStartPnt[scoreOrientation]) / halfStaffDist) % 2) != scoreOrientation) {
							// add a small offset to the dot
							y -= halfStaffDist;
						}
						ofDrawCircle(x+(noteWidth/1.5), y, staffDist*0.2);
					}
				}
			}
			if (allNotes[bar][i][0] == -1) {
				float x = xStartPnt + (allNotesXCoords[bar][i][whichIdxBase[bar][i]] * xCoefLocal);
				int restIndex = drawRest(bar, actualDurs[bar][i], x, restsColor, yOffset);
				if (dotIndexes[bar][i] == 1) {
					ofDrawCircle(x+(notationFont.stringWidth(restsSyms[restIndex])*1.2), middleOfStaff+yStartPnt[scoreOrientation]+yOffset-halfStaffDist, staffDist*0.2);
				}
			}
			// then draw the stems
			if (hasStem[bar][i]) {
				float x = xStartPnt + (allNotesXCoords[bar][i][0] * xCoefLocal);
				if (stemDirections[bar][i] == 1) x += ((noteWidth / 2.0) - lineWidth);
				else x -= ((noteWidth / 2.0) - (lineWidth / 2.0));
				float y1 = yStartPnt[scoreOrientation] + allNotesYCoords1[bar][i][0]+(noteHeight/8.0)-(staffDist/10.0) + yOffset;
				float y2 = allNotesYCoords2[bar][i] + yStartPnt[scoreOrientation] + yOffset;
				ofDrawLine(x, y1, x, y2);
			}
			// then draw the individual tails
			int numTailsLocal = log(actualDurs[bar][i]) / log(2) - 2;
			if (actualDurs[bar][i] > 4 && !isNoteGroupped[bar][i] && numTailsLocal != 0 && allNotes[bar][i][0] > -1) {
				int symIdx = 3;
				int tailDir = 1;
				float xPos = xStartPnt + (allNotesXCoords[bar][i][0] * xCoefLocal);
				if (stemDirections[bar][i] == 1) xPos += ((noteWidth / 2.0) - lineWidth);
				else xPos -= ((noteWidth / 2.0) - (lineWidth / 2.0));
				float yPos = yStartPnt[scoreOrientation] + allNotesYCoords1[bar][i][0] + (noteHeight/8.0) - \
							 (staffDist/10.0) + yOffset - ((stemHeight + halfStaffDist) * stemDirections[bar][i]);
				// minor corrections on the tails coordinates
				if (stemDirections[bar][i] < 0) {
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
					notationFont.drawString(notesSyms[symIdx], xPos, yPos+((j*(staffDist*BEAMDISTCOEFF))*tailDir));
				}
			}
			// reset the animation color
			if (animation && animate && whichNote == (int)i) {
				ofSetColor(0);
			}
		}
		// then draw the beams
		if (beamsIndexes[bar].size() > 0) {
			for (int i = 0; i < (int)beamsIndexes[bar].size(); i++) {
				// don't draw beams for single notes that are not groupped
				if (numNotesWithBeams[bar][i] > 1) {
					for (int j = beamsIndexes[bar][i][0]; j < beamsIndexes[bar][i][1]; j++) {
						// if there is a rest at the beginning or the end of the beam group, don't draw beams
						if (j == beamsIndexes[bar][i][0] && allNotes[bar][j][0] == -1) continue;
						if (j == beamsIndexes[bar][i][1]-1 && allNotes[bar][j+1][0] == -1) continue;
						float xCoords[3];
						float yCoords[3];
						xCoords[0] = xStartPnt + (allNotesXCoords[bar][j][0] * xCoefLocal);
						xCoords[2] = xStartPnt + (allNotesXCoords[bar][j+1][0] * xCoefLocal);
						yCoords[0] = allNotesYCoords2[bar][j] + yStartPnt[scoreOrientation] + yOffset;
						yCoords[2] = allNotesYCoords2[bar][j+1] + yStartPnt[scoreOrientation] + yOffset;
						if (j == beamsIndexes[bar][i][0]) xCoords[0] -= (lineWidth/2);
						if (j < beamsIndexes[bar][i][1]) xCoords[2] += (lineWidth/2);
						// stems up
						if (grouppedStemDirs[bar][i] == 1) {
							xCoords[0] += ((noteWidth / 2.0) - lineWidth);
							xCoords[2] += ((noteWidth / 2.0) - lineWidth);
							yCoords[0] += (beamsLineWidth / 2);
							yCoords[2] += (beamsLineWidth / 2);
						}
						// stems down
						else {
							xCoords[0] -= ((noteWidth / 2.0) - (lineWidth / 2.0));
							xCoords[2] -= ((noteWidth / 2.0) - (lineWidth / 2.0));
							yCoords[0] -= (beamsLineWidth / 2);
							yCoords[2] -= (beamsLineWidth / 2);
						}
						xCoords[1] = xCoords[0] + ((xCoords[2] - xCoords[0]) / 2.0);
						if (yCoords[0] > yCoords[2]) yCoords[1] = yCoords[2] + ((yCoords[0] - yCoords[2]) / 2.0);
						else yCoords[1] = yCoords[0] + ((yCoords[2] - yCoords[0]) / 2.0);
						// split the drawing of beams to two per connected pair
						// so we can draw separate number of extra beams if for example an eight note
						// is connected to two sixtennths, where the first needs to draw one beam only
						// but the other two need to draw two beams
						for (int k = 0; k < 2; k++) {
							// draw the first beam
							drawBeams(xCoords[k], yCoords[k], xCoords[k+1], yCoords[k+1]);
							// separate variable for correct offsetting in the loop below
							float yCoordOffset1;
							float yCoordOffset2;
							// draw the rest (if any) with a loop
							// but not for rests, unless these are surrounded by notes
							for (int l = 1; l < numBeams[bar][j+k]; l++) {
								bool drawBeam = false;
								// if two connected notes are not rests and they have the same duration
								if (allNotes[bar][j+k][0] > -1 && actualDurs[bar][j] == actualDurs[bar][j+1]) {
									drawBeam = true;
								}
								else {
									if (j+k > beamsIndexes[bar][i][0] && j+k < beamsIndexes[bar][i][1]) {
										// if a rest is surrounded by notes and not rests
										if (numBeams[bar][j+k-1] >= l+1 && numBeams[bar][j+k+1] >= l+1) drawBeam = true;
									}
								}
								if (drawBeam) {
									yCoordOffset1 = yCoords[k] + ((l*(staffDist*BEAMDISTCOEFF)) * grouppedStemDirs[bar][i]);
									yCoordOffset2 = yCoords[k+1] + ((l*(staffDist*BEAMDISTCOEFF)) * grouppedStemDirs[bar][i]);
									drawBeams(xCoords[k], yCoordOffset1, xCoords[k+1], yCoordOffset2);
								}
							}
						}
					}
				}
			}
		}
		drawAccidentals(bar, xStartPnt, yOffset, xCoef);
		drawGlissandi(bar, xStartPnt, yOffset, xCoef);
		drawArticulations(bar, xStartPnt, yOffset, xCoef);
		drawOttavas(bar, xStartPnt, yOffset, xCoef);
		drawText(bar, xStartPnt, yOffset, xCoef);
		drawSlurs(bar, xStartPnt, yOffset, xCoef);
		drawDynamics(bar, xStartPnt, yOffset, xCoef);
	}
	else {
		int restColor = 0;
		float xPos = xStartPnt + (xLength / 2);
		if (animation && animate) restColor = 255;
		drawRest(bar, 1, xPos, restColor, yOffset);
	}
}

//--------------------------------------------------------------
int Notes::drawRest(int bar, int restDur, float x, int color, float yOffset)
{
	ofSetColor(color, 0, 0);
	int indexLocal;
	if (restDur < 4) {
		indexLocal = 0;
		float y = yStartPnt[scoreOrientation] + staffDist + halfStaffDist+((staffDist/2.0)*(restDur-1)) + yOffset;
		if (rhythm && restDur == 1) y += staffDist; 
		notationFont.drawString(restsSyms[indexLocal], x-(noteHeight/2), y);
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
		notationFont.drawString(restsSyms[indexLocal], x-((noteHeight*1.5)/2.0),
				yStartPnt[scoreOrientation]+staffDist+extraOffset+yOffset);
	}
	ofSetColor(0);
	return indexLocal;
}

//--------------------------------------------------------------
void Notes::drawBeams(float x1, float y1, float x2, float y2)
{
	// draw a thick line without affecting the thickness
	// of all the other lines
	// taken from
	// https://forum.openframeworks.cc/t/how-do-i-draw-lines-of-a-
	// reasonable-thickness-on-a-very-large-canvas-given-opengl-
	// constraints/30815/7
	ofPoint a;
	ofPoint b;
	a.x = x1;
	a.y = y1;
	b.x = x2;
	b.y = y2;
	ofPoint diff = (a - b).getNormalized();
	diff.rotate(90, ofPoint(0,0,1));
	ofMesh m;
	m.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);
	m.addVertex(a + diff * beamsLineWidth);
	m.addVertex(a - diff * beamsLineWidth);
	m.addVertex(b + diff * beamsLineWidth);
	m.addVertex(b - diff * beamsLineWidth);
	m.draw();
}

//--------------------------------------------------------------
void Notes::drawAccidentals(int bar, float xStartPnt, float yOffset, float xCoef)
{
	if (tacet[bar]) return;
	float xCoefLocal = xCoef;
	if (scoreOrientation == 0) xCoefLocal = 1;
	for (unsigned i = 0; i < allAccidentals[bar].size(); i++) {
		for (unsigned j = 0; j < allAccidentals[bar][i].size(); j++) {
			// escape dummy accentals
			if ((allAccidentals[bar][i][j]%2) == 0) {
				// escape lack of accidental and rests
				if (allAccidentals[bar][i][j] > -1 && allNotes[bar][i][j] > -1) {
					notationFont.drawString(accSyms[allAccidentals[bar][i][j]],
							(allNotesXCoords[bar][i][j]*xCoefLocal)+xStartPnt-(noteWidth*1.2),
							allNotesYCoords1[bar][i][j]+yStartPnt[scoreOrientation]+yOffset);
				}
			}
			if (naturalSignsNotWritten[bar][i][j]) {
				notationFont.drawString(accSyms[4],
						(allNotesXCoords[bar][i][j]*xCoefLocal)+xStartPnt-(noteWidth*1.2),
						allNotesYCoords1[bar][i][j]+yStartPnt[scoreOrientation]+yOffset);
				naturalSignsNotWritten[bar][i][j] = 0;
			}
		}
	}
}

//--------------------------------------------------------------
void Notes::drawGlissandi(int bar, float xStartPnt, float yOffset, float xCoef)
{
	float xCoefLocal = xCoef;
	if (scoreOrientation == 0) xCoefLocal = 1;
	float xStart, xEnd;
	float yStart, yEnd;
	for (unsigned i = 0; i < allGlissandi[bar].size(); i++) {
		if (allGlissandi[bar][i] > 0) {
			xStart = (allNotesXCoords[bar][i][whichIdxBase[bar][i]]*xCoefLocal) + xStartPnt;
			xEnd = (allNotesXCoords[bar][i+1][whichIdxBase[bar][i]]*xCoefLocal) + xStartPnt - noteWidth;
			yStart = allNotesYCoords1[bar][i][whichIdxBase[bar][i]] + yStartPnt[scoreOrientation] + yOffset;
			yEnd = allNotesYCoords1[bar][i+1][whichIdxBase[bar][i]] + yStartPnt[scoreOrientation] + yOffset;
			// if we have an accidental at the end of the gliss
			if ((allAccidentals[bar][i+1][whichIdxBase[bar][i]]%2) == 0) {
				if (allAccidentals[bar][i+1][whichIdxBase[bar][i]] != 4) {
					// this is taken from the drawAccidentals() function
					xEnd = (allNotesXCoords[bar][i+1][whichIdxBase[bar][i]]*xCoefLocal)-(noteWidth*1.2);
					if (stemDirections[bar][i+1] < 0) {
						xEnd += staffDist;
					}
					xEnd -= (notationFont.stringWidth(accSyms[allAccidentals[bar][i+1][whichIdxBase[bar][i]]])/2.0);
				}
			}
			if (stemDirections[bar][i] > 0) {
				xStart += (noteWidth/2);
			}
			else {
				xStart += noteWidth;
			}
			// check if the start and end notes are on a line
			if (((int)(abs((yStart + yOffset) - yStartPnt[scoreOrientation]) / halfStaffDist) % 2) == 0) {
				float glissLineOffset = staffDist / 4.0;
				if (yStart > yEnd) {
					yStart -= glissLineOffset;
				}
				else {
					yStart += glissLineOffset;
				}
			}
			if (((int)(abs((yEnd + yOffset) - yStartPnt[scoreOrientation]) / halfStaffDist) % 2) == 0) {
				float glissLineOffset = staffDist / 4.0;
				if (yEnd > yStart) {
					yEnd -= glissLineOffset;
				}
				else {
					yEnd += glissLineOffset;
				}
			}
			ofDrawLine(xStart, yStart, xEnd, yEnd);
		}
	}
}

//--------------------------------------------------------------
void Notes::drawArticulations(int bar, float xStartPnt, float yOffset, float xCoef)
{
	float xCoefLocal = xCoef;
	if (scoreOrientation == 0) xCoefLocal = 1;
	for (unsigned i = 0; i < allArticulations[bar].size(); i++) {
		if (allArticulations[bar][i] > 0) {
			if (allArticulations[bar][i] < 7) {
				// we don't have a symbol for no articulation so bar with allArticulations[i]-1
				string articulStr = articulSyms[allArticulations[bar][i]-1];
				if ((allArticulations[bar][i] == 1) && (stemDirections[bar][i] > 0)) {
					// the marcato sign needs to be inverted in case of being projected under the note
					ofPushMatrix();
					ofTranslate((articulXPos[bar][i]*xCoefLocal)+xStartPnt, articulYPos[bar][i]+yStartPnt[scoreOrientation]+yOffset);
					ofRotateXDeg(180);
					notationFont.drawString(articulStr, 0, 0);
					ofPopMatrix();
				}
				else {
					notationFont.drawString(articulStr, (articulXPos[bar][i]*xCoefLocal)+xStartPnt, articulYPos[bar][i]+yStartPnt[scoreOrientation]+yOffset);
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
				notationFont.drawString(".", (articulXPos[bar][i]*xCoefLocal)+xStartPnt+(noteWidth/2), articulYPos[bar][i]+yStartPnt[scoreOrientation]+yOffset);
				notationFont.drawString("_", (articulXPos[bar][i]*xCoefLocal)+xStartPnt, articulYPos[bar][i]+yStartPnt[scoreOrientation]+offset+yOffset);
			}
		}
	}
}

//--------------------------------------------------------------
void Notes::drawOttavas(int bar, float xStartPnt, float yOffset, float xCoef)
{
	float xCoefLocal = xCoef;
	if (scoreOrientation == 0) xCoefLocal = 1;
	int prevOttava = 0;
	int index = -1;
	int lastOttavaIndex = -1;
	for (unsigned i = 0; i < allOttavas[bar].size(); i++) {
		if (allOttavas[bar][i] != 0) {
			if (allOttavas[bar][i] != prevOttava) {
				if (index > -1 && lastOttavaIndex - index > 1) {
					float offsetX = notationFont.stringWidth(octaveSyms[abs(allOttavas[bar][index])]);
					float offsetY = notationFont.stringHeight(octaveSyms[abs(allOttavas[bar][index])]) / 2;
					ofDrawLine(allNotesXCoords[bar][index][0] + offsetX, allOttavasYCoords[bar][index]+ offsetY,
							   allNotesXCoords[bar][lastOttavaIndex][0], allOttavasYCoords[bar][lastOttavaIndex] + offsetY);
				}
				notationFont.drawString(octaveSyms[abs(allOttavas[bar][i])], (allNotesXCoords[bar][i][0]*xCoefLocal)+xStartPnt, allOttavasYCoords[bar][i]+yStartPnt[scoreOrientation]+yOffset);
				prevOttava = allOttavas[bar][i];
				index = (int)i;
			}
			else {
				lastOttavaIndex = (int)i;
			}
		}
	}
	if (index > -1 && lastOttavaIndex - index > 1) {
		float offsetX = notationFont.stringWidth(octaveSyms[abs(allOttavas[bar][index])]);
		float offsetY = notationFont.stringHeight(octaveSyms[abs(allOttavas[bar][index])]) / 2;
		ofDrawLine(allNotesXCoords[bar][index][0] + offsetX, allOttavasYCoords[bar][index] + offsetY,
				   allNotesXCoords[bar][lastOttavaIndex][0], allOttavasYCoords[bar][lastOttavaIndex] + offsetY);
	}
}

//--------------------------------------------------------------
void Notes::drawText(int bar, float xStartPnt, float yOffset, float xCoef)
{
	float xCoefLocal = xCoef;
	if (scoreOrientation == 0) xCoefLocal = 1;
	for (unsigned i = 0; i < allTextsXCoords[bar].size(); i++) {
		textFont.drawString(allTexts[bar][i], (allTextsXCoords[bar][i]*xCoefLocal)+xStartPnt, allTextsYCoords[bar][i]+yStartPnt[scoreOrientation]+yOffset);
	}
}

//--------------------------------------------------------------
void Notes::drawSlurs(int bar, float xStartPnt, float yOffset, float xCoef)
{
	float xCoefLocal = xCoef;
	if (scoreOrientation == 0) xCoefLocal = 1;
	ofNoFill();
	for (unsigned i = 0; i < slurStartX[bar].size(); i++) {
		if (slurStartX[bar][i] > -1) {
			// find index with end of slur
			unsigned stopNdx;
			for (stopNdx = i + 1; stopNdx < slurStopX[bar].size(); stopNdx++) {
				if (slurStopX[bar][stopNdx] > -1) break;
			}
			float x0 = (slurStartX[bar][i]*xCoefLocal) + xStartPnt;
			float y0 = slurStartY[bar][i] + yStartPnt[scoreOrientation] + yOffset;
			float x1 = (slurMiddleX1[bar][i]*xCoefLocal) + xStartPnt;
			float y1 = slurMiddleY1[bar][i] + yStartPnt[scoreOrientation] + yOffset;
			float x2 = (slurMiddleX2[bar][i]*xCoefLocal) + xStartPnt;
			float y2 = slurMiddleY2[bar][i] + yStartPnt[scoreOrientation] + yOffset;
			float x3 = (slurStopX[bar][stopNdx]*xCoefLocal) + xStartPnt;
			float y3 = slurStopY[bar][stopNdx] + yStartPnt[scoreOrientation] + yOffset;
			ofBeginShape();
			ofVertex(x0,y0);
			ofBezierVertex(x1,y1,x2,y2,x3,y3);
			ofEndShape();
		}
	}
	ofFill();
}

//--------------------------------------------------------------
void Notes::drawDynamics(int bar, float xStartPnt, float yOffset, float xCoef)
{
	float xCoefLocal = xCoef;
	if (scoreOrientation == 0) xCoefLocal = 1;
	for (unsigned i = 0; i < dynamics[bar].size(); i++) {
		if (dynamics[bar][i] > -1) {
			notationFont.drawString(dynSyms[dynamics[bar][i]], (dynsXCoords[bar][i]*xCoefLocal)+xStartPnt, dynsYCoords[bar][i]+yStartPnt[scoreOrientation]+yOffset);
		}
	}
	for (unsigned i = 0; i < dynamicsRampStart[bar].size(); i++) {
		if (dynamicsRampStart[bar][i] > -1) {
			// find the next index of dynsRampEndXCoords[bar] that is greater than -1
			// to determine where the dynamic ramp ends
			unsigned endNdx = i;
			for (unsigned j = i; j < dynamicsRampEnd[bar].size(); j++) {
				if (dynamicsRampEnd[bar][j] > -1) {
					endNdx = j;
					break;
				}
			}
			for (int j = 0; j < 2; j++) {
				ofDrawLine((dynsRampStartXCoords[bar][i]*xCoefLocal)+xStartPnt,
						dynsRampStartYCoords[bar][i]+yStartPnt[scoreOrientation]+(staffDist*(((dynamicsRampDir[bar][i]*(-1))+1)*((j*2)-1)))+yOffset,
						(dynsRampEndXCoords[bar][endNdx]*xCoefLocal)+xStartPnt,
						dynsRampEndYCoords[bar][endNdx]+yStartPnt[scoreOrientation]+(staffDist*(dynamicsRampDir[bar][i]*((j*2)-1)))+yOffset);
			}
		}
	}
}

//--------------------------------------------------------------
void Notes::printVector(vector<int> v)
{
	for (unsigned i = 0; i < v.size(); i++) {
		cout << v[i] << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
void Notes::printVector(vector<string> v)
{
	for (unsigned i = 0; i < v.size(); i++) {
		cout << v[i] << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
void Notes::printVector(vector<float> v)
{
	for (unsigned i = 0; i < v.size(); i++) {
		cout << v[i] << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
Notes::~Notes(){}
