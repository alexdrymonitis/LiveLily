#include "instrument.h"

//--------------------------------------------------------------
Instrument::Instrument()
{
	midiChan = 1;
	isMidiBool = false;
	staccatoDur = 0.5;
	staccatissimoDur = 0.25;
	tenutoDur = 0.9;
	// we're using the articulations map to index durPercetanges
	// which is a map<int, vector<int>>
	// index 0 is not articulation
	// index 1 is marcato
	// index 2 is trill
	// index 5 is accent
	// index 7 is portando
	// for these indexes we use the default duration
	durPercentages[0] = 0.75;
	durPercentages[1] = 0.75;
	durPercentages[2] = 0.75;
	durPercentages[3] = tenutoDur;
	durPercentages[4] = staccatissimoDur;
	durPercentages[5] = 0.75;
	durPercentages[6] = staccatoDur;
	durPercentages[7] = 0.75;

	beatCounter = 0;
	barDataCounter = 0;
	barDataCounterReset = false;
	seqToggle = 0;
	noteDur = 0;

	muteState = false;
	toMute = false;
	toUnmute = false;

	passed = false;
	firstIter = false;

	multiBarsDone = true;
	multiBarsStrBegin = 0;

	hasNewStepBool = false;
	newStep = 0;

	isRhythmBool = false;
	transposition = 0;
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
	staff.setID(id);
	notesObj.setID(id);
}

//--------------------------------------------------------------
void Instrument::setRhythm(bool isRhythm)
{
	isRhythmBool = isRhythm;
	staff.setRhythm(isRhythm);
	notesObj.setRhythm(isRhythm);
	if (isRhythm) setClef(0, 3);
}

//--------------------------------------------------------------
bool Instrument::isRhythm()
{
	return isRhythmBool;
}

//--------------------------------------------------------------
void Instrument::setTransposition(int transpo)
{
	transposition = transpo;
}

//--------------------------------------------------------------
int Instrument::getTransposition()
{
	return transposition;
}

//--------------------------------------------------------------
void Instrument::setDefaultDur(float dur)
{
	durPercentages[0] = dur;
	durPercentages[1] = dur;
	durPercentages[2] = dur;
	durPercentages[5] = dur;
	durPercentages[7] = dur;
}

//--------------------------------------------------------------
void Instrument::setStaccatoDur(float dur)
{
	durPercentages[6] = dur;
}

//--------------------------------------------------------------
void Instrument::setStaccatissimoDur(float dur)
{
	durPercentages[4] = dur;
}

//--------------------------------------------------------------
void Instrument::setTenutoDur(float dur)
{
	durPercentages[3] = dur;
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
void Instrument::setPassed(bool passedBool)
{
	passed = passedBool;
}

//--------------------------------------------------------------
bool Instrument::hasPassed()
{
	return passed;
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
bool Instrument::hasNewStep()
{
	return hasNewStepBool;
}

//--------------------------------------------------------------
intPair Instrument::isLinked(int bar)
{
	return notesObj.isBarLinked(bar);
}

//--------------------------------------------------------------
void Instrument::setNewStep(bool stepState)
{
	hasNewStepBool = stepState;
}

//--------------------------------------------------------------
bool Instrument::getMultiBarsDone()
{
	return multiBarsDone;
}

//--------------------------------------------------------------
void Instrument::setMultiBarsDone(bool done)
{
	multiBarsDone = done;
}

//--------------------------------------------------------------
size_t Instrument::getMultiBarsStrBegin()
{
	return multiBarsStrBegin;
}

//--------------------------------------------------------------
void Instrument::setMultiBarsStrBegin(size_t pos)
{
	multiBarsStrBegin = pos;
}

//--------------------------------------------------------------
void Instrument::setClef(int bar, int clefIdx)
{
	staff.setClef(bar, clefIdx);
	notesObj.setClef(bar, clefIdx);
}

//--------------------------------------------------------------
int Instrument::getClef(int bar)
{
	return staff.getClef(bar);
}

//--------------------------------------------------------------
void Instrument::copyMelodicLine(int barIndex)
{
	notes[barIndex] = notes[copyNdxs[barIndex]];
	midiNotes[barIndex] = midiNotes[copyNdxs[barIndex]];
	durs[barIndex] = durs[copyNdxs[barIndex]];
	dursWithoutSlurs[barIndex] = dursWithoutSlurs[copyNdxs[barIndex]];
	midiDursWithoutSlurs[barIndex] = midiDursWithoutSlurs[copyNdxs[barIndex]];
	pitchBendVals[barIndex] = pitchBendVals[copyNdxs[barIndex]];
	dynamics[barIndex] = dynamics[copyNdxs[barIndex]];
	midiVels[barIndex] = midiVels[copyNdxs[barIndex]];
	dynamicsRamps[barIndex] = dynamicsRamps[copyNdxs[barIndex]];
	glissandi[barIndex] = glissandi[copyNdxs[barIndex]];
	midiGlissDurs[barIndex] = midiGlissDurs[copyNdxs[barIndex]];
	midiDynamicsRampDurs[barIndex] = midiDynamicsRampDurs[copyNdxs[barIndex]];
	articulations[barIndex] = articulations[copyNdxs[barIndex]];
	midiArticulationVals[barIndex] = midiArticulationVals[copyNdxs[barIndex]];
	isSlurred[barIndex] = isSlurred[copyNdxs[barIndex]];
	text[barIndex] = text[copyNdxs[barIndex]];
	textIndexes[barIndex] = textIndexes[copyNdxs[barIndex]];
	//slurBeginnings[barIndex] = slurBeginnings[copyNdxs[barIndex]];
	//slurEndings[barIndex] = slurEndings[copyNdxs[barIndex]];
	slurIndexes[barIndex] = slurIndexes[copyNdxs[barIndex]];
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
	isWholeBarSlurred[barIndex] = isWholeBarSlurred[copyNdxs[barIndex]];
	scoreTexts[barIndex] = scoreTexts[copyNdxs[barIndex]];
	scoreNaturalSignsNotWritten[barIndex] = scoreNaturalSignsNotWritten[copyNdxs[barIndex]];
	staff.copyMelodicLine(barIndex, copyNdxs[barIndex]);
	notesObj.copyMelodicLine(barIndex, copyNdxs[barIndex]);
}

//--------------------------------------------------------------
void Instrument::createEmptyMelody(int barIndex)
{
	notes[barIndex] = vector<vector<float>>(1, vector<float>(1, -1.0));
	midiNotes[barIndex] = vector<vector<int>>(1, vector<int>(1, -1));
	durs[barIndex] = vector<int>(1, MINDUR);
	dursWithoutSlurs[barIndex] = vector<int>(1, MINDUR);
	midiDursWithoutSlurs[barIndex] = vector<int>(1, MINDUR);
	pitchBendVals[barIndex] = vector<int>(1, 0);
	// default dynamics value so if we create a line after an empty one
	// we'll get the previous dynamic, which should be the default and not 0
	dynamics[barIndex] = vector<int>(1, (100-((100-MINDB)/2)));
	midiVels[barIndex] = vector<int>(1, 72);
	dynamicsRamps[barIndex] = vector<int>(1, 0);
	glissandi[barIndex] = vector<int>(1, 0);
	midiGlissDurs[barIndex] = vector<int>(1, 0);
	midiDynamicsRampDurs[barIndex] = vector<int>(1, 0);
	articulations[barIndex] = vector<vector<int>>(1, vector<int>(1, 0));
	midiArticulationVals[barIndex] = vector<vector<int>>(1, vector<int>(1, 0));
	text[barIndex] = vector<string>(1, "");
	textIndexes[barIndex] = vector<int>(1, 0);
	isWholeBarSlurred[barIndex] = false;
	intPair p;
	p.first = p.second = -1;
	slurIndexes[barIndex] = vector<intPair>(1, p);
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
	map<int, intPair> m1;
	scoreTupRatios[barIndex] = m1;
	map<int, uintPair> m2;
	scoreTupStartStop[barIndex] = m2;
	scoreTexts[barIndex] = vector<string>(1, "");
}

//--------------------------------------------------------------
void Instrument::setMeter(int bar, int numerator, int denominator, int numBeats)
{
	staff.setMeter(bar, numerator, denominator);
	notesObj.setMeter(bar, numerator, denominator, numBeats);
}

//--------------------------------------------------------------
intPair Instrument::getMeter(int bar)
{
	return staff.getMeter(bar);
}

//--------------------------------------------------------------
void Instrument::setScoreNotes(int bar, int numerator, int denominator, int numBeats,
		int BPMTempo, int beatAtValue, bool hasDot)
{
	//setMeter(bar, numerator, denominator, numBeats);
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
			slurIndexes[bar],
			isWholeBarSlurred[bar],
			scoreTupRatios[bar],
			scoreTupStartStop[bar],
			scoreTexts[bar],
			textIndexes[bar]);
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
void Instrument::setNotePositions(int bar, int numBars)
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
void Instrument::setNoteCoords(float xLen, float yPos1, float yPos2, float staffLineDist, int fontSize)
{
	notesObj.setCoords(xLen, yPos1, yPos2, staffLineDist, fontSize);
}

//--------------------------------------------------------------
void Instrument::correctScoreYAnchor(float yAnchor1, float yAnchor2)
{
	staff.correctYAnchor(yAnchor1, yAnchor2);
	notesObj.correctYAnchor(yAnchor1, yAnchor2);
}

//--------------------------------------------------------------
void Instrument::setScoreOrientation(int orientation)
{
	staff.setOrientation(orientation);
	notesObj.setOrientation(orientation);
	notesObj.setOrientationForDots(orientation);
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
void Instrument::setStaffCoords(float xStartPnt, float yAnchor1, float yAnchor2, float staffLineDist)
{
	staff.setCoords(xStartPnt, yAnchor1, yAnchor2, staffLineDist);
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
void Instrument::initSeqToggle()
{
	seqToggle = 0;
}

//--------------------------------------------------------------
void Instrument::setFirstIter(bool iter)
{
	firstIter = iter;
}

//--------------------------------------------------------------
void Instrument::setToMute(bool mute)
{
	toMuteState = mute;
}

//--------------------------------------------------------------
bool Instrument::isToBeMuted()
{
	return toMuteState;
}

//--------------------------------------------------------------
void Instrument::setToUnmute(bool unmute)
{
	toUnmuteState = unmute;
}

//--------------------------------------------------------------
bool Instrument::isToBeUnmuted()
{
	return toUnmuteState;
}

//--------------------------------------------------------------
void Instrument::setMute(bool mute)
{
	muteState = mute;
	notesObj.setMute(mute);
	toMuteState = false;
	toUnmuteState = false;
}

//--------------------------------------------------------------
bool Instrument::isMuted()
{
	return muteState;
}

//--------------------------------------------------------------
void Instrument::setMidi(bool isMidiBool)
{
	Instrument::isMidiBool = isMidiBool;
}

//--------------------------------------------------------------
bool Instrument::isMidi()
{
	return isMidiBool;
}

//--------------------------------------------------------------
void Instrument::setTimeStamp(uint64_t stamp)
{
	if (noteDur > 0) noteDur -= (int64_t)(stamp - timeStamp);
	timeStamp = stamp;
}

//--------------------------------------------------------------
void Instrument::resetNoteDur()
{
	if (barDataCounter == 0) noteDur = 0;
}

//--------------------------------------------------------------
void Instrument::zeroNoteDur()
{
	noteDur = 0;
}

//--------------------------------------------------------------
bool Instrument::mustFireStep(uint64_t stamp, int bar, float tempo)
{
	if (barDataCounter >= (int)notes[bar].size()) {
		return false;
	}
	int64_t diff = (int64_t)((int64_t)stamp - (int64_t)timeStamp);
	if (diff >= noteDur) {
		if (noteDur == 0) {
			timeStamp = stamp;
		}
		else {
			timeStamp = stamp - (uint64_t)(diff - (noteDur));
		}
		setNoteDur(bar, tempo);
		return true;
	}
	return false;
}

//--------------------------------------------------------------
void Instrument::setNoteDur(int bar, float tempo)
{
	int64_t durMs = (int64_t)((double)durs[bar][barDataCounter] * (tempo * 1000));
	int64_t noteOn = (int64_t)((double)durMs * durPercentages[articulations[bar][barDataCounter][0]]);
	int64_t noteOff = (int64_t)((double)durMs * (1.0 - durPercentages[articulations[bar][barDataCounter][0]]));
	// we must make sure the sum of the note on and note off durations
	// equals the sum of the note duration of the score
	// this might not be the case because of floating point impressision
	int64_t remaining = durMs - (noteOn + noteOff);
	noteOn += remaining;
	switch (seqToggle) {
		case 0:
			noteDur = noteOn;
			break;
		case 1:
			noteDur = noteOff;
			break;
		default:
			break;
	}
}

//--------------------------------------------------------------
bool Instrument::isNoteSlurred(int bar, int dataCounter)
{
	// explicitly query with barDataCounter sent as an argument
	// so we can test if the previous note was slurred
	// which is useful when handling MIDI, where we send the note off
	// of the previous note after the note on of the current note is sent
	// in case the previous note is slurred
	// otherwise, instead of dataCounter we could just use barDataCounter here
	// without needing the second argument to this function
	return isSlurred[bar][dataCounter];
}

//--------------------------------------------------------------
bool Instrument::mustUpdateTempo()
{
	return updateTempo;
}

//--------------------------------------------------------------
void Instrument::setUpdateTempo(bool tempoUpdate)
{
	updateTempo = tempoUpdate;
}

//--------------------------------------------------------------
bool Instrument::hasNotesInBar(int bar)
{
	return (midiNotes[bar].size() > 0 ? true : false); 
}

//--------------------------------------------------------------
bool Instrument::hasNotesInStep(int bar)
{
	if (barDataCounter >= (int)midiNotes[bar].size()) return false;
	// a rest is stored as a -1, so a value from 0 and above is a note
	return (midiNotes[bar][barDataCounter][0] >= 0 ? true : false); 
}

//--------------------------------------------------------------
void Instrument::setMidiChan(int chan)
{
	midiChan = chan;
}

//--------------------------------------------------------------
int Instrument::getMidiChan()
{
	return midiChan;
}

//--------------------------------------------------------------
void Instrument::setActiveNote()
{
	notesObj.setActiveNote(barDataCounter);
}

//--------------------------------------------------------------
void Instrument::resetBarDataCounter()
{
	barDataCounter = 0;
	zeroNoteDur();
}

//--------------------------------------------------------------
int Instrument::getBarDataCounter()
{
	return barDataCounter;
}

//--------------------------------------------------------------
void Instrument::toggleSeqToggle(int bar)
{
	seqToggle++;
	if (seqToggle > 1) {
		seqToggle = 0;
		barDataCounter++;
		hasNewStepBool = true;
	}
}

//--------------------------------------------------------------
int Instrument::getSeqToggle()
{
	return seqToggle;
}

//--------------------------------------------------------------
bool Instrument::hasText(int bar)
{
	return ((int)text[bar].size() > barDataCounter ? true : false);
}

//--------------------------------------------------------------
string Instrument::getText(int bar)
{
	return text[bar][barDataCounter];
}

//--------------------------------------------------------------
int Instrument::getPitchBendVal(int bar)
{
	return pitchBendVals[bar][barDataCounter];
}

//--------------------------------------------------------------
//int Instrument::getProgramChangeVal(int bar)
//{
//	return midiArticulationVals[bar][barDataCounter];
//}

//--------------------------------------------------------------
int Instrument::getMidiVel(int bar)
{
	return midiVels[bar][barDataCounter];
}

//--------------------------------------------------------------
//int Instrument::getArticulationIndex(int bar)
//{
//	return articulations[bar][barDataCounter];
//}

//--------------------------------------------------------------
int Instrument::getDynamic(int bar)
{
	return dynamics[bar][barDataCounter];
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
void Instrument::drawStaff(int bar, float xOffset, float yOffset, bool drawClef,
		bool drawMeter, bool drawLoopStartEnd, bool drawTempo)
{
	staff.drawStaff(bar, xOffset, yOffset, drawClef, drawMeter, drawLoopStartEnd, drawTempo);
}

//--------------------------------------------------------------
void Instrument::drawNotes(int bar, int loopNdx, vector<int> *v, float xOffset, float yOffset, bool animate, float xCoef)
{
	notesObj.drawNotes(bar, loopNdx, v, xOffset, yOffset, animate, xCoef);
}

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
