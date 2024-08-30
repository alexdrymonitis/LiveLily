#include "instrument.h"

//--------------------------------------------------------------
Instrument::Instrument()
{
	staff.setOrientation(1);
	notesObj.setOrientation(1);
}

//--------------------------------------------------------------
void Instrument::setName(string name)
{
	Instrument::name = name;
}

//--------------------------------------------------------------
string Instrument::getName()
{
	return name;
}

//--------------------------------------------------------------
void Instrument::setID(int id)
{
	objID = id;
	notesObj.setID(id);
}

//--------------------------------------------------------------
void Instrument::setRhythm(bool isRhythm)
{
	staff.setRhythm(isRhythm);
	notesObj.setRhythm(isRhythm);
	if (isRhythm) setClef(3);
}

//--------------------------------------------------------------
void Instrument::setNumBarsToDisplay(int numBars)
{
	staff.setNumBarsToDisplay(numBars);
}

//--------------------------------------------------------------
void Instrument::setClef(int clefIdx)
{
	staff.setClef(clefIdx);
	notesObj.setClef(clefIdx);
}

//--------------------------------------------------------------
void Instrument::setCopied(int barIndex, bool copyState)
{
	copyStates[barIndex] = copyState;
}

//--------------------------------------------------------------
bool Instrument::getCopied(int barIndex)
{
	return copyStates[barIndex];
}

//--------------------------------------------------------------
void Instrument::setCopyNdx(int barIndex, int barToCopy)
{
	copyNdxs[barIndex] = barToCopy;
}

//--------------------------------------------------------------
int Instrument::getCopyNdx(int barIndex)
{
	return copyNdxs[barIndex];
}

//--------------------------------------------------------------
vector<vector<int>> Instrument::packIntVector(vector<int> v, int val)
{
	auto it = find(v.begin(), v.end(), val);
	vector<int> ndxs;
	while (it != v.end()) {
		ndxs.push_back(it - v.begin());
		it = find(it + 1, v.end(), val);
	}
	vector<vector<int>> vv;
	for (unsigned i = 0; i < ndxs.size(); i++) {
		vv.push_back(vector<int>());
		int j = 0;
		if (i > 0) j = ndxs[i-1] + 1;
		for (int k = j; k < ndxs[i]; k++) {
			vv.back().push_back(v[k]);
		}
	}
	return vv;
}

//--------------------------------------------------------------
void Instrument::setNotes(int bar, vector<int> v)
{
	// the vectors in vector are separated by -2, as -1 is used for rests
	// so we unpack the 1D vector and convert to 2D based on -2
	scoreNotes[bar] = packIntVector(v, -2);
}

//--------------------------------------------------------------
void Instrument::setAccidentals(int bar, vector<int> v)
{
	// the vectors in vector are separated by -2, as -1 is used for rests
	// so we unpack the 1D vector and convert to 2D based on -2
	scoreAccidentals[bar] = packIntVector(v, -2);
}

//--------------------------------------------------------------
void Instrument::setNaturalSignsNotWritten(int bar, vector<int> v)
{
	// the vectors in vector are separated by -2, as -1 is used for rests
	// so we unpack the 1D vector and convert to 2D based on -2
	scoreNaturalSignsNotWritten[bar] = packIntVector(v, -2);
}

//--------------------------------------------------------------
void Instrument::setOctaves(int bar, vector<int> v)
{
	// the vectors in vector are separated by -2, as -1 is used for rests
	// so we unpack the 1D vector and convert to 2D based on -2
	scoreOctaves[bar] = packIntVector(v, -2);
}

//--------------------------------------------------------------
void Instrument::setOttavas(int bar, vector<int> v)
{
	scoreOttavas[bar] = v;
}

//--------------------------------------------------------------
void Instrument::setDurs(int bar, vector<int> v)
{
	scoreDurs[bar] = v;
}

//--------------------------------------------------------------
void Instrument::setDotIndexes(int bar, vector<int> v)
{
	scoreDotIndexes[bar] = v;
}

//--------------------------------------------------------------
void Instrument::setGlissandi(int bar, vector<int> v)
{
	scoreGlissandi[bar] = v;
}

//--------------------------------------------------------------
void Instrument::setArticulations(int bar, vector<int> v)
{
	scoreArticulations[bar] = v;
}

//--------------------------------------------------------------
void Instrument::setDynamics(int bar, vector<int> v)
{
	scoreDynamics[bar] = v;
}

//--------------------------------------------------------------
void Instrument::setDynamicsIndexes(int bar, vector<int> v)
{
	scoreDynamicsIndexes[bar] = v;
}

//--------------------------------------------------------------
void Instrument::setDynamicsRampStart(int bar, vector<int> v)
{
	scoreDynamicsRampStart[bar] = v;
}

//--------------------------------------------------------------
void Instrument::setDynamicsRampEnd(int bar, vector<int> v)
{
	scoreDynamicsRampEnd[bar] = v;
}

//--------------------------------------------------------------
void Instrument::setDynamicsRampDir(int bar, vector<int> v)
{
	scoreDynamicsRampDir[bar] = v;
}

//--------------------------------------------------------------
void Instrument::setSlurBeginnings(int bar, vector<int> v)
{
	scoreSlurBeginnings[bar] = v;
}

//--------------------------------------------------------------
void Instrument::setSlurEndings(int bar, vector<int> v)
{
	scoreSlurEndings[bar] = v;
}

//--------------------------------------------------------------
void Instrument::setTexts(int bar, vector<string> v)
{
	scoreTexts[bar] = v;
}

//--------------------------------------------------------------
void Instrument::setTextIndexes(int bar, vector<int> v)
{
	scoreTextIndexes[bar] = v;
}

//--------------------------------------------------------------
bool Instrument::checkVecSizesForEquality(int bar)
{
	size_t size = scoreNotes[bar].size();
	if (scoreAccidentals[bar].size() != size) return false;
	if (scoreOctaves[bar].size() != size) return false;
	if (scoreDurs[bar].size() != size) return false;
	if (scoreDotIndexes[bar].size() != size) return false;
	if (scoreGlissandi[bar].size() != size) return false;
	if (scoreArticulations[bar].size() != size) return false;
	if (scoreDynamics[bar].size() != size) return false;
	if (scoreDynamicsIndexes[bar].size() != size) return false;
	if (scoreDynamicsRampStart[bar].size() != size) return false;
	if (scoreDynamicsRampEnd[bar].size() != size) return false;
	if (scoreDynamicsRampDir[bar].size() != size) return false;
	if (scoreSlurBeginnings[bar].size() != size) return false;
	if (scoreSlurEndings[bar].size() != size) return false;
	return true;
}

//--------------------------------------------------------------
void Instrument::copyMelodicLine(int barIndex)
{
	scoreNotes[barIndex] = scoreNotes[copyNdxs[barIndex]];
	scoreDurs[barIndex] = scoreDurs[copyNdxs[barIndex]];
	scoreDotIndexes[barIndex] = scoreDotIndexes[copyNdxs[barIndex]];
	scoreAccidentals[barIndex] = scoreAccidentals[copyNdxs[barIndex]];
	scoreOctaves[barIndex] = scoreOctaves[copyNdxs[barIndex]];
	scoreOttavas[barIndex] = scoreOttavas[copyNdxs[barIndex]];
	scoreGlissandi[barIndex] = scoreGlissandi[copyNdxs[barIndex]];
	scoreArticulations[barIndex] = scoreArticulations[copyNdxs[barIndex]];
	scoreDynamics[barIndex] = scoreDynamics[copyNdxs[barIndex]];
	scoreDynamicsIndexes[barIndex] = scoreDynamicsIndexes[copyNdxs[barIndex]];
	scoreDynamicsRampStart[barIndex] = scoreDynamicsRampStart[copyNdxs[barIndex]];
	scoreDynamicsRampEnd[barIndex] = scoreDynamicsRampEnd[copyNdxs[barIndex]];
	scoreDynamicsRampDir[barIndex] = scoreDynamicsRampDir[copyNdxs[barIndex]];
	scoreSlurBeginnings[barIndex] = scoreSlurBeginnings[copyNdxs[barIndex]];
	scoreSlurEndings[barIndex] = scoreSlurEndings[copyNdxs[barIndex]];
	scoreTexts[barIndex] = scoreTexts[copyNdxs[barIndex]];
	scoreTextIndexes[barIndex] = scoreTextIndexes[copyNdxs[barIndex]];
	scoreNaturalSignsNotWritten[barIndex] = scoreNaturalSignsNotWritten[copyNdxs[barIndex]];
	// we only need to copy vectors for the notes object
	// as the staff object needs to copy only the numerator and denominator of the meter
	// but that is done in setMeter() below, which is called by setScoreNotes()
	// which in turn is called by the main program
	notesObj.copyMelodicLine(barIndex, copyNdxs[barIndex]);
}

//--------------------------------------------------------------
void Instrument::createEmptyMelody(int barIndex)
{
	scoreNotes[barIndex] = vector<vector<int>>(1, vector<int>(1, -1));
	scoreDurs[barIndex] = vector<int>(1, MINDUR);
	scoreDotIndexes[barIndex] = vector<int>(1, 0);
	scoreAccidentals[barIndex] = vector<vector<int>>(1, vector<int>(1, 4));
	scoreOctaves[barIndex] = vector<vector<int>>(1, vector<int>(1, 0));
	scoreGlissandi[barIndex] = vector<int>(1, 0);
	scoreArticulations[barIndex] = vector<int>(1, 0);
	scoreDynamics[barIndex] = vector<int>(1, -1);
	scoreDynamicsIndexes[barIndex] = vector<int>(1, -1);
	scoreDynamicsRampStart[barIndex] = vector<int>(1, -1);
	scoreDynamicsRampEnd[barIndex] = vector<int>(1, -1);
	scoreDynamicsRampDir[barIndex] = vector<int>(1, -1);
	scoreSlurBeginnings[barIndex] = vector<int>(1, -1);
	scoreSlurEndings[barIndex] = vector<int>(1, -1);
	scoreTexts[barIndex] = vector<string>(1, "");
	scoreTextIndexes[barIndex] = vector<int>(1, 0);
}

//--------------------------------------------------------------
void Instrument::setMeter(int bar, int numerator, int denominator, int numBeats)
{
	staff.setMeter(bar, numerator, denominator);
	notesObj.setMeter(bar, numerator, denominator, numBeats);
}

//--------------------------------------------------------------
void Instrument::setScoreNotes(int bar, int numerator, int denominator, int numBeats)
{
	setMeter(bar, numerator, denominator, numBeats);
	notesObj.setNotes(bar,
			scoreNotes[bar],
			scoreAccidentals[bar],
			scoreNaturalSignsNotWritten[bar],
			scoreOctaves[bar],
			scoreOttavas[bar],
			scoreDurs[bar],
			scoreDotIndexes[bar],
			scoreGlissandi[bar],
			scoreArticulations[bar],
			scoreDynamics[bar],
			scoreDynamicsIndexes[bar],
			scoreDynamicsRampStart[bar],
			scoreDynamicsRampEnd[bar],
			scoreDynamicsRampDir[bar],
			scoreSlurBeginnings[bar],
			scoreSlurEndings[bar],
			scoreTexts[bar],
			scoreTextIndexes[bar]);
}

//--------------------------------------------------------------
void Instrument::setNoteCoords(float xLen, float yPos1, float yPos2, float staffLineDist, int fontSize)
{
	notesObj.setCoords(xLen, yPos1, yPos2, staffLineDist, fontSize);
}

//--------------------------------------------------------------
void Instrument::setNotePositions(int bar)
{
	notesObj.setNotePositions(bar);
}

//--------------------------------------------------------------
void Instrument::correctScoreYAnchor(float yAnchor1, float yAnchor2)
{
	staff.correctYAnchor(yAnchor1, yAnchor2);
	notesObj.correctYAnchor(yAnchor1, yAnchor2);
}

//--------------------------------------------------------------
void Instrument::moveScoreX(int numPixels)
{
	staff.moveScoreX(numPixels);
	notesObj.moveScoreX(numPixels);
}

//--------------------------------------------------------------
void Instrument::moveScoreY(int numPixels)
{
	staff.moveScoreY(numPixels);
	notesObj.moveScoreY(numPixels);
}

//--------------------------------------------------------------
void Instrument::recenterScore()
{
	staff.recenterScore();
	notesObj.recenterScore();
}

//--------------------------------------------------------------
float Instrument::getStaffXLength()
{
	return staff.getXLength();
}

//--------------------------------------------------------------
float Instrument::getNoteWidth()
{
	return notesObj.getNoteWidth();
}

//--------------------------------------------------------------
float Instrument::getNoteHeight()
{
	return notesObj.getNoteHeight();
}

//--------------------------------------------------------------
float Instrument::getStaffYAnchor()
{
	return staff.getYAnchor();
}

//--------------------------------------------------------------
void Instrument::setStaffSize(int fontSize)
{
	staff.setSize(fontSize);
}

//--------------------------------------------------------------
void Instrument::setStaffCoords(float xStartPnt, float yAnchor1, float yAnchor2, float staffLineDist)
{
	staff.setCoords(xStartPnt, yAnchor1, yAnchor2, staffLineDist);
}

//--------------------------------------------------------------
void Instrument::setNotesFontSize(int fontSize)
{
	notesObj.setFontSize(fontSize);
}

//--------------------------------------------------------------
void Instrument::setAnimation(bool animationState)
{
	notesObj.setAnimation(animationState);

}

//--------------------------------------------------------------
void Instrument::setLoopStart(bool loopStartState)
{
	staff.isLoopStart = loopStartState;
}

//--------------------------------------------------------------
void Instrument::setLoopEnd(bool loopEndState)
{
	staff.isLoopEnd = loopEndState;
}

//--------------------------------------------------------------
void Instrument::setScoreEnd(bool scoreEndState)
{
	staff.isScoreEnd = scoreEndState;
}

//--------------------------------------------------------------
bool Instrument::isLoopStart()
{
	return staff.isLoopStart;
}

//--------------------------------------------------------------
bool Instrument::isLoopEnd()
{
	return staff.isLoopEnd;
}

//--------------------------------------------------------------
void Instrument::setActiveNote(int note)
{
	notesObj.setActiveNote(note);
}

//--------------------------------------------------------------
float Instrument::getMinVsAnchor(int bar)
{
	return notesObj.getMinVsAnchor(bar);
}

//--------------------------------------------------------------
float Instrument::getMaxYPos(int bar)
{
	return notesObj.getMaxYPos(bar);
}

//--------------------------------------------------------------
float Instrument::getMinYPos(int bar)
{
	return notesObj.getMinYPos(bar);
}

//--------------------------------------------------------------
float Instrument::getClefXOffset()
{
	return staff.getClefXOffset();
}

//--------------------------------------------------------------
float Instrument::getMeterXOffset()
{
	return staff.getMeterXOffset();
}

//--------------------------------------------------------------
void Instrument::drawStaff(int bar, float xOffset, float yOffset, bool drawClef, bool drawMeter, bool drawLoopStartEnd)
{
	staff.drawStaff(bar, xOffset, yOffset, drawClef, drawMeter, drawLoopStartEnd);
}

//--------------------------------------------------------------
void Instrument::drawNotes(int bar, int loopNdx, vector<int> *v, float xOffset, float yOffset, bool animate, float xCoef)
{
	notesObj.drawNotes(bar, loopNdx, v, xOffset, yOffset, animate, xCoef);
}

//--------------------------------------------------------------
void Instrument::printVector(vector<int> v)
{
	for (int elem : v) {
		cout << elem << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
void Instrument::printVector(vector<string> v)
{
	for (string elem : v) {
		cout << elem << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
void Instrument::printVector(vector<float> v)
{
	for (float elem : v) {
		cout << elem << " ";
	}
	cout << endl;
}
