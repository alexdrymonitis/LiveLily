#include "instrument.h"

//--------------------------------------------------------------
Instrument::Instrument()
{
	staff.setOrientation(1);
	notesObj.setOrientation(1);
	notesObj.setOrientationForDots(0);
	clef = 0;
	groupID = -1;
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
int Instrument::getID()
{
	return objID;
}

//--------------------------------------------------------------
void Instrument::setGroup(int groupID)
{
	Instrument::groupID = groupID;
	staff.setGroup(groupID);
}

//--------------------------------------------------------------
int Instrument::getGroup()
{
	return groupID;
}

//--------------------------------------------------------------
void Instrument::setRhythm(bool isRhythm)
{
	staff.setRhythm(isRhythm);
	notesObj.setRhythm(isRhythm);
	if (isRhythm) setClef(0, 3);
}

//--------------------------------------------------------------
void Instrument::setNumBarsToDisplay(int numBars)
{
	staff.setNumBarsToDisplay(numBars);
}

//--------------------------------------------------------------
float Instrument::getXCoef()
{
	return staff.getXCoef();
}

//--------------------------------------------------------------
void Instrument::setClef(int bar, int clefIdx)
{
	staff.setClef(bar, clefIdx);
	notesObj.setClef(bar, clefIdx);
	isClefSet[bar] = true;
	clef = clefIdx;
}

//--------------------------------------------------------------
int Instrument::getClef(int bar)
{
	return staff.getClef(bar);
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
	// the vectors in vector are separated by 1000
	// so we unpack the 1D vector and convert to 2D based on 1000
	scoreNotes[bar] = packIntVector(v, 1000);
}

//--------------------------------------------------------------
void Instrument::setAccidentals(int bar, vector<int> v)
{
	// the vectors in vector are separated by 1000
	// so we unpack the 1D vector and convert to 2D based on 1000
	scoreAccidentals[bar] = packIntVector(v, 1000);
}

//--------------------------------------------------------------
void Instrument::setNaturalSignsNotWritten(int bar, vector<int> v)
{
	// the vectors in vector are separated by 1000
	// so we unpack the 1D vector and convert to 2D based on 1000
	scoreNaturalSignsNotWritten[bar] = packIntVector(v, 1000);
}

//--------------------------------------------------------------
void Instrument::setOctaves(int bar, vector<int> v)
{
	// the vectors in vector are separated by 1000
	// so we unpack the 1D vector and convert to 2D based on 1000
	scoreOctaves[bar] = packIntVector(v, 1000);
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
	// the vectors in vector are separated by 1000
	// so we unpack the 1D vector and convert to 2D based on 1000
	scoreArticulations[bar] = packIntVector(v, 1000);
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
void Instrument::setSlurIndexes(int bar, vector<int> v)
{
	int first = 0;
	for (unsigned i = 0; i < v.size(); i++) {
		if (i%2 == 0) first = v[i];
		else {
			scoreSlurIndexes[bar].push_back(std::make_pair(first, v[i]));
		}
	}
}

//--------------------------------------------------------------
void Instrument::setTies(int bar, vector<int> v)
{
	tieIndexes[bar] = v;
}

//--------------------------------------------------------------
void Instrument::setWholeBarSlurred(int bar, bool b)
{
	isWholeBarSlurred[bar] = b;
}

//--------------------------------------------------------------
void Instrument::setTupRatios(int bar, vector<int> v)
{
	int key = 0, first = 0;
	for (unsigned i = 0; i < v.size(); i++) {
		if (i%3 == 0) key = v[i];
		else if (i%3 == 1) first = v[i];
		else {
			scoreTupRatios[bar][key] = std::make_pair(first, v[i]);
		}
	}
}

//--------------------------------------------------------------
void Instrument::setTupStartStop(int bar, vector<int> v)
{
	int key = 0, first = 0;
	for (unsigned i = 0; i < v.size(); i++) {
		if (i%3 == 0) key = v[i];
		else if (i%3 == 1) first = v[i];
		else {
			scoreTupStartStop[bar][key] = std::make_pair((unsigned)first, (unsigned)v[i]);
		}
	}
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
	//scoreSlurBeginnings[barIndex] = scoreSlurBeginnings[copyNdxs[barIndex]];
	//scoreSlurEndings[barIndex] = scoreSlurEndings[copyNdxs[barIndex]];
	scoreSlurIndexes[barIndex] = scoreSlurIndexes[copyNdxs[barIndex]];
	isWholeBarSlurred[barIndex] = isWholeBarSlurred[copyNdxs[barIndex]];
	tieIndexes[barIndex] = tieIndexes[copyNdxs[barIndex]];
	scoreTupRatios[barIndex] = scoreTupRatios[copyNdxs[barIndex]];
	scoreTupStartStop[barIndex] = scoreTupStartStop[copyNdxs[barIndex]];
	scoreTexts[barIndex] = scoreTexts[copyNdxs[barIndex]];
	scoreTextIndexes[barIndex] = scoreTextIndexes[copyNdxs[barIndex]];
	scoreNaturalSignsNotWritten[barIndex] = scoreNaturalSignsNotWritten[copyNdxs[barIndex]];
	staff.copyMelodicLine(barIndex, copyNdxs[barIndex]);
	notesObj.copyMelodicLine(barIndex, copyNdxs[barIndex]);
}

//--------------------------------------------------------------
void Instrument::createEmptyMelody(int barIndex)
{
	scoreNotes[barIndex] = vector<vector<int>>(1, vector<int>(1, -1));
	scoreDurs[barIndex] = vector<int>(1, MINDUR);
	scoreDotIndexes[barIndex] = vector<int>(1, 0);
	scoreAccidentals[barIndex] = vector<vector<int>>(1, vector<int>(1, 4));
	scoreNaturalSignsNotWritten[barIndex] = vector<vector<int>>(1, vector<int>(1, 0));
	scoreOctaves[barIndex] = vector<vector<int>>(1, vector<int>(1, 0));
	scoreGlissandi[barIndex] = vector<int>(1, 0);
	scoreArticulations[barIndex] = vector<vector<int>>(1, vector<int>(1, 0));
	scoreDynamics[barIndex] = vector<int>(1, -1);
	scoreDynamicsIndexes[barIndex] = vector<int>(1, -1);
	scoreDynamicsRampStart[barIndex] = vector<int>(1, -1);
	scoreDynamicsRampEnd[barIndex] = vector<int>(1, -1);
	scoreDynamicsRampDir[barIndex] = vector<int>(1, -1);
	isWholeBarSlurred[barIndex] = false;
	tieIndexes[barIndex] = vector<int>(1, -1);
	scoreSlurIndexes[barIndex] = vector<std::pair<int, int>>(1, std::make_pair(-1, -1));
	map<int, std::pair<int, int>> m1;
	scoreTupRatios[barIndex] = m1;
	map<int, std::pair<unsigned, unsigned>> m2;
	scoreTupStartStop[barIndex] = m2;
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
std::pair<int, int> Instrument::getMeter(int bar)
{
	return staff.getMeter(bar);
}

//--------------------------------------------------------------
void Instrument::setScoreNotes(int bar, int numerator, int denominator, int numBeats,
		int BPMTempo, int beatAtValue, bool hasDot, int BPMMultiplier)
{
	//setMeter(bar, numerator, denominator, numBeats);
	if (isClefSet.find(bar) != isClefSet.end()) {
		if (!isClefSet[bar]) setClef(bar, clef);
	}
	else {
		setClef(bar, clef);
	}
	staff.setTempo(bar, BPMTempo, beatAtValue, hasDot);
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
			scoreSlurIndexes[bar],
			tieIndexes[bar],
			isWholeBarSlurred[bar],
			scoreTupRatios[bar],
			scoreTupStartStop[bar],
			scoreTexts[bar],
			scoreTextIndexes[bar],
			BPMMultiplier);
}

//--------------------------------------------------------------
void Instrument::setNotePositions(int bar)
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
void Instrument::setNoteCoords(float xLen, float staffLineDist, int fontSize)
{
	notesObj.setCoords(xLen, staffLineDist, fontSize);
}

//--------------------------------------------------------------
void Instrument::setAccidentalsOffsetCoef(float coef)
{
	notesObj.setAccidentalsOffsetCoef(coef);
}

//--------------------------------------------------------------
void Instrument::moveScoreX(int numPixels)
{
	//staff.moveScoreX(numPixels);
	//notesObj.moveScoreX(numPixels);
}

//--------------------------------------------------------------
void Instrument::moveScoreY(int numPixels)
{
	//staff.moveScoreY(numPixels);
	//notesObj.moveScoreY(numPixels);
}

//--------------------------------------------------------------
void Instrument::recenterScore()
{
	//staff.recenterScore();
	//notesObj.recenterScore();
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
void Instrument::setStaffCoords(float xStartPnt, float staffLineDist)
{
	staff.setCoords(xStartPnt, staffLineDist);
}

//--------------------------------------------------------------
void Instrument::setNotesFontSize(int fontSize, float staffLinesDist)
{
	staff.setSize(fontSize, staffLinesDist);
	notesObj.setFontSize(fontSize, staffLinesDist);
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
void Instrument::drawStaff(int bar, float xOffset, float yStartPnt, float yOffset, bool drawClef, bool drawMeter, bool drawLoopStartEnd, bool drawTempo)
{
	staff.drawStaff(bar, xOffset, yStartPnt, yOffset, drawClef, drawMeter, drawLoopStartEnd, drawTempo);
}

//--------------------------------------------------------------
void Instrument::drawNotes(int bar, int loopNdx, vector<int> *v, float xOffset, float yStartPnt, float yOffset, bool animate, float xCoef)
{
	notesObj.drawNotes(bar, loopNdx, v, xOffset, yStartPnt, yOffset, animate, xCoef);
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
