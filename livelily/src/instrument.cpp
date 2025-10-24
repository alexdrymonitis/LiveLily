#include "instrument.h"

//--------------------------------------------------------------
Instrument::Instrument()
{
	midiChan = 1;
	isMidiBool = false;
	staccatoDur = 0.5;
	staccatissimoDur = 0.25;
	tenutoDur = 0.9;
	// we're using the articulations std::map to index durPercetanges
	// which is a std::map<int, std::vector<int>>
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
	barCounter = 0;
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
	sendMIDIBool = false;
	sendToPythonBool = false;
	transposition = 0;

	startGliss = false;

	delayState = false;
	delayTime = 0;

	groupID = -1;
	midiPort = -1;

	setStaffColor(ofColor::black);
	setNotesColor(ofColor::black);
	setActiveNotesColor(ofColor::red);
}

//--------------------------------------------------------------
void Instrument::setName(std::string name)
{
	Instrument::name = name;
}

//--------------------------------------------------------------
std::string Instrument::getName()
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
void Instrument::setSendToPython(bool b)
{
	sendToPythonBool = b;
}

//--------------------------------------------------------------
bool Instrument::sendToPython()
{
	return sendToPythonBool;
}

//--------------------------------------------------------------
void Instrument::setDelay(int64_t dur)
{
	delayTime = dur * 1000;
	if (dur > 0) delayState = true;
	else delayState = false;
}

//--------------------------------------------------------------
bool Instrument::hasDelay()
{
	return delayState;
}

//--------------------------------------------------------------
int64_t Instrument::getDelayTime()
{
	return delayTime;
}

//--------------------------------------------------------------
void Instrument::setSendMIDI(bool sendMIDI)
{
	sendMIDIBool = sendMIDI;
}

//--------------------------------------------------------------
bool Instrument::sendMIDI()
{
	return sendMIDIBool;
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
int Instrument::setDuration(std::string subcommand, float dur)
{
	if (subcommand.compare("normal") == 0) {
		setDefaultDur(dur);
		return 0;
	}
	else if (subcommand.compare("staccato") == 0) {
		setStaccatoDur(dur);
		return 0;
	}
	else if (subcommand.compare("staccatissimo") == 0) {
		setStaccatissimoDur(dur);
		return 0;
	}
	else if (subcommand.compare("tenuro") == 0) {
		setTenutoDur(dur);
		return 0;
	}
	return 1;
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
std::pair<int, int> Instrument::isLinked(int bar)
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
void Instrument::createEmptyMelody(int barIndex)
{
	notes[barIndex] = std::vector<std::vector<float>>(1, std::vector<float>(1, -1.0));
	midiNotes[barIndex] = std::vector<std::vector<int>>(1, std::vector<int>(1, -1));
	durs[barIndex] = std::vector<int>(1, MINDUR);
	dursWithoutSlurs[barIndex] = std::vector<int>(1, MINDUR);
	midiDursWithoutSlurs[barIndex] = std::vector<int>(1, MINDUR);
	pitchBendVals[barIndex] = std::vector<int>(1, 0);
	// default dynamics value so if we create a line after an empty one
	// we'll get the previous dynamic, which should be the default and not 0
	// this is half way between min dB (which is 82, equivalent to MIDI velocity 16)
	// and max dB (which is 100), which is 72
	// but converted to RMS
	dynamics[barIndex] = std::vector<float>(1, 0.567);
	midiVels[barIndex] = std::vector<int>(1, 72);
	dynamicsRamps[barIndex] = std::vector<float>(1, 0);
	glissandi[barIndex] = std::vector<int>(1, 0);
	midiGlissDurs[barIndex] = std::vector<int>(1, 0);
	midiDynamicsRampDurs[barIndex] = std::vector<int>(1, 0);
	articulations[barIndex] = std::vector<std::vector<int>>(1, std::vector<int>(1, 0));
	midiArticulationVals[barIndex] = std::vector<std::vector<int>>(1, std::vector<int>(1, 0));
	isSlurred[barIndex] = std::vector<bool>(1, false);
	text[barIndex] = std::vector<std::string>(1, "");
	textIndexes[barIndex] = std::vector<std::vector<int>>(1, std::vector<int>(1, 0));
	slurIndexes[barIndex] = std::vector<std::pair<int, int>>(1, std::make_pair(-1, -1));
	isWholeBarSlurred[barIndex] = false;
	tieIndexes[barIndex] = std::vector<int> (1, -1);
	scoreNotes[barIndex] = std::vector<std::vector<int>>(1, std::vector<int>(1, -1));
	scoreDurs[barIndex] = std::vector<int>(1, MINDUR);
	scoreDotIndexes[barIndex] = std::vector<int>(1, 0);
	scoreDotsCounter[barIndex] = std::vector<unsigned>(1, 0);
	scoreAccidentals[barIndex] = std::vector<std::vector<int>>(1, std::vector<int>(1, 4));
	scoreNaturalSignsNotWritten[barIndex] = std::vector<std::vector<int>>(1, std::vector<int>(1, 0));
	scoreOctaves[barIndex] = std::vector<std::vector<int>>(1, std::vector<int>(1, 0));
	scoreGlissandi[barIndex] = std::vector<int>(1, 0);
	scoreArticulations[barIndex] = std::vector<std::vector<int>>(1, std::vector<int>(1, 0));
	scoreDynamics[barIndex] = std::vector<int>(1, -1);
	scoreDynamicsIndexes[barIndex] = std::vector<int>(1, -1);
	scoreDynamicsRampStart[barIndex] = std::vector<int>(1, -1);
	scoreDynamicsRampEnd[barIndex] = std::vector<int>(1, -1);
	scoreDynamicsRampDir[barIndex] = std::vector<int>(1, -1);
	std::map<int, std::pair<int, int>> m1;
	scoreTupletRatios[barIndex] = m1;
	std::map<int, std::pair<unsigned, unsigned>> m2;
	scoreTupletStartStop[barIndex] = m2;
	scoreTexts[barIndex] = std::vector<std::string>(1, "");
}

//--------------------------------------------------------------
void Instrument::deleteNotesBar(int bar)
{
	notesObj.deleteBar(bar);
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
	staff.setTempo(bar, BPMTempo, beatAtValue, hasDot);
	notesObj.setNotes(bar,
			scoreNotes[bar],
			scoreAccidentals[bar],
			scoreNaturalSignsNotWritten[bar],
			scoreOctaves[bar],
			scoreOttavas[bar],
			scoreDurs[bar],
			scoreDotIndexes[bar],
			scoreDotsCounter[bar],
			scoreGlissandi[bar],
			articulations[bar],
			scoreDynamics[bar],
			scoreDynamicsIndexes[bar],
			scoreDynamicsRampStart[bar],
			scoreDynamicsRampEnd[bar],
			scoreDynamicsRampDir[bar],
			slurIndexes[bar],
			tieIndexes[bar],
			isWholeBarSlurred[bar],
			scoreTupletRatios[bar],
			scoreTupletStartStop[bar],
			scoreTexts[bar],
			textIndexes[bar],
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
void Instrument::setScoreOrientation(int orientation)
{
	staff.setOrientation(orientation);
	notesObj.setOrientation(orientation);
	notesObj.setOrientationForDots(orientation);
}

//--------------------------------------------------------------
void Instrument::setStaffColor(ofColor color)
{
	staff.staffColor = color;
}

//--------------------------------------------------------------
void Instrument::setStaffColor(int color, int val)
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
void Instrument::setStaffColor(int r, int g, int b)
{
	staff.staffColor.r = r;
	staff.staffColor.g = g;
	staff.staffColor.b = b;
}

//--------------------------------------------------------------
ofColor Instrument::getStaffColor()
{
	return staff.staffColor;
}

//--------------------------------------------------------------
void Instrument::setNotesColor(ofColor color)
{
	notesObj.notesColor = color;
}

//--------------------------------------------------------------
void Instrument::setNotesColor(int r, int g, int b)
{
	notesObj.notesColor.r = r;
	notesObj.notesColor.g = g;
	notesObj.notesColor.b = b;
}

//--------------------------------------------------------------
void Instrument::setNotesColor(int color, int val)
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
ofColor Instrument::getNotesColor()
{
	return notesObj.notesColor;
}

//--------------------------------------------------------------
void Instrument::setActiveNotesColor(ofColor color)
{
	notesObj.activeNotesColor = color;
}

//--------------------------------------------------------------
void Instrument::setActiveNotesColor(int color, int val)
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
void Instrument::setActiveNotesColor(int r, int g, int b)
{
	notesObj.activeNotesColor.r = r;
	notesObj.activeNotesColor.g = g;
	notesObj.activeNotesColor.b = b;
}

//--------------------------------------------------------------
ofColor Instrument::getActiveNotesColor()
{
	return notesObj.activeNotesColor;
}

//--------------------------------------------------------------
void Instrument::setGlissandoStart(bool startGlissBool)
{
	startGliss = startGlissBool;
}

//--------------------------------------------------------------
bool Instrument::getGlissandoStart()
{
	return startGliss;
}

//--------------------------------------------------------------
void Instrument::invertColor()
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
	if (diff >= noteDur + delayTime) {
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
bool Instrument::isNoteTied(int bar, int dataCounter)
{
	// explicitly query with barDataCounter sent as an argument
	// so we can test if the previous note was tied
	// which is useful when handling MIDI, where we send the note off
	// of the previous note after the note on of the current note is sent
	// in case the previous note is tied
	// otherwise, instead of dataCounter we could just use barDataCounter here
	// without needing the second argument to this function
	return (tieIndexes[bar][dataCounter] > -1 ? true : false);
}

//--------------------------------------------------------------
bool Instrument::isLastNoteTied(int bar)
{
	return (tieIndexes[bar].back() > -1 ? true : false);
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
void Instrument::setMidiPort(int port)
{
	midiPort = port;
}

//--------------------------------------------------------------
int Instrument::getMidiPort()
{
	return midiPort;
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
std::string Instrument::getText(int bar)
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
float Instrument::getDynamic(int bar)
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
void Instrument::drawStaff(int bar, float xOffset, float yStartPnt, float yOffset, bool drawClef,
		bool drawMeter, bool drawLoopStartEnd, bool drawTempo)
{
	staff.drawStaff(bar, xOffset, yStartPnt, yOffset, drawClef, drawMeter, drawLoopStartEnd, drawTempo);
}

//--------------------------------------------------------------
void Instrument::drawNotes(int bar, int loopNdx,  std::vector<int> *v, float xOffset, float yStartPnt, float yOffset, bool animate, float xCoef)
{
	notesObj.drawNotes(bar, loopNdx, v, xOffset, yStartPnt, yOffset, animate, xCoef);
}

void Instrument::printVector(std::vector<int> v)
{
	for (int elem : v) {
		cout << elem << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
void Instrument::printVector(std::vector<std::string> v)
{
	for (std::string elem : v) {
		cout << elem << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
void Instrument::printVector(std::vector<float> v)
{
	for (float elem : v) {
		cout << elem << " ";
	}
	cout << endl;
}
