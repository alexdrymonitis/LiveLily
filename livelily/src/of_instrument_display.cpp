// what is drawn of an instrument with openFrameworks (see of_instrument_display.h), moved from the Instrument class
#include "of_instrument_display.h"

//--------------------------------------------------------------
OfInstrumentDisplay::OfInstrumentDisplay()
{
	staff.staffColor = ofColor::black;
	notesObj.notesColor = ofColor::black;
	notesObj.activeNotesColor = ofColor::red;
}

//--------------------------------------------------------------
void OfInstrumentDisplay::setNumBarsToDisplay(int numBars)
{
	staff.setNumBarsToDisplay(numBars);
}

//--------------------------------------------------------------
float OfInstrumentDisplay::getXCoef()
{
	return staff.getXCoef();
}

//--------------------------------------------------------------
std::pair<int, int> OfInstrumentDisplay::isLinked(int bar)
{
	return notesObj.isBarLinked(bar);
}

//--------------------------------------------------------------
std::pair<int, int> OfInstrumentDisplay::getMeter(int bar)
{
	return staff.getMeter(bar);
}

//--------------------------------------------------------------
void OfInstrumentDisplay::setScoreNotes(Instrument& inst, int bar, int numerator, int denominator, int numBeats,
		int BPMTempo, int beatAtValue, bool hasDot, int BPMMultiplier)
{
	//setMeter(bar, numerator, denominator, numBeats);
	staff.setTempo(bar, BPMTempo, beatAtValue, hasDot);
	notesObj.setNotes(bar,
			inst.scoreNotes[bar],
			inst.scoreAccidentals[bar],
			inst.scoreNaturalSignsNotWritten[bar],
			inst.scoreOctaves[bar],
			inst.scoreOttavas[bar],
			inst.scoreDurs[bar],
			inst.scoreDotIndexes[bar],
			inst.scoreDotsCounter[bar],
			inst.scoreGlissandi[bar],
			inst.articulations[bar],
			inst.scoreDynamics[bar],
			inst.scoreDynamicsIndexes[bar],
			inst.scoreDynamicsRampStart[bar],
			inst.scoreDynamicsRampEnd[bar],
			inst.scoreDynamicsRampDir[bar],
			inst.slurIndexes[bar],
			inst.tieIndexes[bar],
			inst.isWholeBarSlurred[bar],
			inst.scoreTupletRatios[bar],
			inst.scoreTupletStartStop[bar],
			inst.scoreTexts[bar],
			inst.textIndexes[bar],
			BPMMultiplier);
	auto rampIt = inst.openDynamicsRamp.find(bar);
	notesObj.setDynamicsRampOpenAtEnd(bar, rampIt != inst.openDynamicsRamp.end() && rampIt->second.first > -1);
}

//--------------------------------------------------------------
void OfInstrumentDisplay::setNotePositions(int bar)
{
	notesObj.setNotePositions(bar);
	notesObj.storeArticulationsCoords(bar);
	notesObj.storeTupletCoords(bar);
	notesObj.storeSlurCoords(bar);
	notesObj.storeOttavaCoords(bar);
	notesObj.storeTextCoords(bar);
	notesObj.storeDynamicsCoords(bar);
	notesObj.storeMinMaxY(bar);
}

//--------------------------------------------------------------
void OfInstrumentDisplay::setNotePositions(int bar, int numBars)
{
	for (int i = 0; i < numBars; i++) {
		notesObj.setNotePositions(bar+i);
		notesObj.storeMinMaxY(bar+i);
	}
	for (int i = 0; i < numBars; i++) {
		notesObj.storeArticulationsCoords(bar+i);
		notesObj.storeMinMaxY(bar+i);
	}
	for (int i = 0; i < numBars; i++) {
		notesObj.storeTupletCoords(bar+i);
		notesObj.storeMinMaxY(bar+i);
	}
	for (int i = 0; i < numBars; i++) {
		notesObj.storeSlurCoords(bar+i);
		notesObj.storeMinMaxY(bar+i);
	}
	for (int i = 0; i < numBars; i++) {
		notesObj.storeOttavaCoords(bar+i);
		notesObj.storeMinMaxY(bar+i);
	}
	for (int i = 0; i < numBars; i++) {
		notesObj.storeTextCoords(bar+i);
		notesObj.storeMinMaxY(bar+i);
	}
	for (int i = 0; i < numBars; i++) {
		notesObj.storeDynamicsCoords(bar+i);
		notesObj.storeMinMaxY(bar+i);
	}
}

//--------------------------------------------------------------
void OfInstrumentDisplay::setNoteCoords(float xLen, float staffLineDist, int fontSize)
{
	notesObj.setCoords(xLen, staffLineDist, fontSize);
}

//--------------------------------------------------------------
void OfInstrumentDisplay::setScoreOrientation(int orientation)
{
	staff.setOrientation(orientation);
	notesObj.setOrientation(orientation);
	notesObj.setOrientationForDots(orientation);
}

//--------------------------------------------------------------
void OfInstrumentDisplay::setStaffColor(Color color)
{
	staff.staffColor = toOf(color);
}

//--------------------------------------------------------------
void OfInstrumentDisplay::setStaffColor(int color, int val)
{
	switch (color) {
		case 0:
			staff.staffColor.r = val;
			break;
		case 1:
			staff.staffColor.g = val;
			break;
		case 2:
			staff.staffColor.b = val;
			break;
		default:
			break;
	}
}

//--------------------------------------------------------------
void OfInstrumentDisplay::setStaffColor(int r, int g, int b)
{
	staff.staffColor.r = r;
	staff.staffColor.g = g;
	staff.staffColor.b = b;
}

//--------------------------------------------------------------
Color OfInstrumentDisplay::getStaffColor()
{
	return toColor(staff.staffColor);
}

//--------------------------------------------------------------
void OfInstrumentDisplay::setNotesColor(Color color)
{
	notesObj.notesColor = toOf(color);
}

//--------------------------------------------------------------
void OfInstrumentDisplay::setNotesColor(int r, int g, int b)
{
	notesObj.notesColor.r = r;
	notesObj.notesColor.g = g;
	notesObj.notesColor.b = b;
}

//--------------------------------------------------------------
void OfInstrumentDisplay::setNotesColor(int color, int val)
{
	switch (color) {
		case 0:
			notesObj.notesColor.r = val;
			break;
		case 1:
			notesObj.notesColor.g = val;
			break;
		case 2:
			notesObj.notesColor.b = val;
			break;
		default:
			break;
	}
}

//--------------------------------------------------------------
Color OfInstrumentDisplay::getNotesColor()
{
	return toColor(notesObj.notesColor);
}

//--------------------------------------------------------------
void OfInstrumentDisplay::setActiveNotesColor(Color color)
{
	notesObj.activeNotesColor = toOf(color);
}

//--------------------------------------------------------------
void OfInstrumentDisplay::setActiveNotesColor(int color, int val)
{
	switch (color) {
		case 0:
			notesObj.activeNotesColor.r = val;
			break;
		case 1:
			notesObj.activeNotesColor.g = val;
			break;
		case 2:
			notesObj.activeNotesColor.b = val;
			break;
		default:
			break;
	}
}

//--------------------------------------------------------------
void OfInstrumentDisplay::setActiveNotesColor(int r, int g, int b)
{
	notesObj.activeNotesColor.r = r;
	notesObj.activeNotesColor.g = g;
	notesObj.activeNotesColor.b = b;
}

//--------------------------------------------------------------
Color OfInstrumentDisplay::getActiveNotesColor()
{
	return toColor(notesObj.activeNotesColor);
}

//--------------------------------------------------------------
void OfInstrumentDisplay::invertColor()
{
	staff.staffColor.r = 255 - staff.staffColor.r;
	staff.staffColor.g = 255 - staff.staffColor.g;
	staff.staffColor.b = 255 - staff.staffColor.b;
	notesObj.notesColor.r = 255 - notesObj.notesColor.r;
	notesObj.notesColor.g = 255 - notesObj.notesColor.g;
	notesObj.notesColor.b = 255 - notesObj.notesColor.b;
	notesObj.activeNotesColor.r = 255 - notesObj.activeNotesColor.r;
	notesObj.activeNotesColor.g = 255 - notesObj.activeNotesColor.g;
	notesObj.activeNotesColor.b = 255 - notesObj.activeNotesColor.b;
}

//--------------------------------------------------------------
void OfInstrumentDisplay::moveScoreX(int numPixels)
{
	staff.moveScoreX(numPixels);
	notesObj.moveScoreX(numPixels);
}

//--------------------------------------------------------------
void OfInstrumentDisplay::moveScoreY(int numPixels)
{
	staff.moveScoreY(numPixels);
	notesObj.moveScoreY(numPixels);
}

//--------------------------------------------------------------
void OfInstrumentDisplay::recenterScore()
{
	staff.recenterScore();
	notesObj.recenterScore();
}

//--------------------------------------------------------------
float OfInstrumentDisplay::getStaffXStartOffset()
{
	return staff.getXStartOffset();
}

//--------------------------------------------------------------
float OfInstrumentDisplay::getStaffXLength()
{
	return staff.getXLength();
}

//--------------------------------------------------------------
float OfInstrumentDisplay::getNoteWidth()
{
	return notesObj.getNoteWidth();
}

//--------------------------------------------------------------
float OfInstrumentDisplay::getNoteHeight()
{
	return notesObj.getNoteHeight();
}

//--------------------------------------------------------------
void OfInstrumentDisplay::setStaffCoords(float xStartPnt, float staffLineDist)
{
	staff.setCoords(xStartPnt, staffLineDist);
}

//--------------------------------------------------------------
void OfInstrumentDisplay::setNotesFontSize(int fontSize, float staffLinesDist)
{
	staff.setSize(fontSize, staffLinesDist);
	notesObj.setFontSize(fontSize, staffLinesDist);
}

//--------------------------------------------------------------
float OfInstrumentDisplay::getMinVsAnchor(int bar)
{
	return notesObj.getMinVsAnchor(bar);
}

//--------------------------------------------------------------
float OfInstrumentDisplay::getMaxYPos(int bar)
{
	return notesObj.getMaxYPos(bar);
}

//--------------------------------------------------------------
float OfInstrumentDisplay::getMinYPos(int bar)
{
	return notesObj.getMinYPos(bar);
}

//--------------------------------------------------------------
float OfInstrumentDisplay::getClefXOffset()
{
	return staff.getClefXOffset();
}

//--------------------------------------------------------------
float OfInstrumentDisplay::getMeterXOffset()
{
	return staff.getMeterXOffset();
}

//--------------------------------------------------------------
void OfInstrumentDisplay::drawStaff(int bar, float xOffset, float yStartPnt, float yOffset, bool drawClef,
		bool drawMeter, bool drawLoopStartEnd, bool drawTempo)
{
	staff.drawStaff(bar, xOffset, yStartPnt, yOffset, drawClef, drawMeter, drawLoopStartEnd, drawTempo);
}

//--------------------------------------------------------------
void OfInstrumentDisplay::drawNotes(int bar, int loopNdx,  std::vector<int> *v, float xOffset, float yStartPnt, float yOffset, bool animate, float xCoef, int nextBar)
{
	notesObj.drawNotes(bar, loopNdx, v, xOffset, yStartPnt, yOffset, animate, xCoef, nextBar);
}
