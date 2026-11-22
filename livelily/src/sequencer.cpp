#include <string>
#include <map>
#include <vector>
#include <cmath> // to add abs()
#include <utility> // to add pair and make_pair
#include <algorithm>
#include "sequencer.h"
#include "clock.h"
#include <sstream>
#include <iomanip>
#include <climits>
#include "utils.h" // SharedData
// the sequencer calls back into the app (e.g. parseCommand(), storeActiveEditorElement())
// and uses some of its #defines (e.g. GLISSGRAIN, HOST, OFSENDPORT), so it needs ofApp.h too
#include "ofApp.h"

/**************************************************************/
/******************* Sequencer class **************************/
//--------------------------------------------------------------
void Sequencer::setup(SharedData *sData)
{
	sharedData = sData;
	runSequencer = false;
	updateSequencer = false;
	sequencerUpdated = false;
	sequencerRunning = false;
	sequencerStart = false;
	finish = false;
	finished = false;
	mustStop = false;
	mustStopCalled = false;
	sendMidiClock = false;
	muteChecked = false;
	firstIter = false;
	onNextStartMidiByte = 0xFA; // Start MIDI byte
	tickCounter = 0;
	beatCounter = 0;
	sendBeatVizInfoCounter = 0;
	thisBarIndex = 0;
	countdown = false;
	countdownCounter = 0;
	midiTuneVal = 440;
	loopToCheckNdx = 0;
	loopToCheckNumIter = 0;
	checkLoop = false;
	tempBar = -1;
	oscSender->setup(HOST, OFSENDPORT);
	timer.setPeriodicEvent(100000); // 100 us
}

//--------------------------------------------------------------
void Sequencer::checkMute()
{
	// check if an instrument needs to be muted at the beginning
	for (auto instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
		if (instMapIt->second.isToBeMuted()) {
			if (!instMapIt->second.isMuted()) {
				instMapIt->second.setMute(true);
			}
		}
		else if (instMapIt->second.isToBeUnmuted()) {
			if (instMapIt->second.isMuted()) {
				instMapIt->second.setMute(false);
			}
		}
	}
	muteChecked = true;
}

//--------------------------------------------------------------
// a MIDI instrument plays on its channel (voice 0), but the notes of a chord with a glissando play on a channel each (voices from 1)
// so that each can bend on its own, as in MPE, with the channels after the channel of the instrument (with SoundFonts, channels of their own)
int Sequencer::midiVoiceChannel(Instrument& inst, int voice)
{
	if (inst.getSoundFontChannel() >= 0) return sharedData->soundFonts.voiceChannel(inst.getSoundFontChannel(), voice);
	// MIDI channels go from 1 to 16
	return ((inst.getMidiChan() - 1 + voice) % 16) + 1;
}

//--------------------------------------------------------------
void Sequencer::midiNoteOn(Instrument& inst, int note, int velocity, int voice)
{
	inst.midiNoteVoices[note] = voice;
	inst.numMidiVoices = std::max(inst.numMidiVoices, voice + 1);
	int channel = midiVoiceChannel(inst, voice);
	if (logged(midiText(inst, "note on", channel, note, velocity))) return;
	if (inst.getSoundFontChannel() >= 0) sharedData->soundFonts.noteOn(channel, note, velocity);
	else midi->noteOn(inst.getMidiPort(), channel, note, velocity);
}

//--------------------------------------------------------------
void Sequencer::midiNoteOff(Instrument& inst, int note)
{
	// the note off goes to the channel the note was played on
	int voice = 0;
	auto it = inst.midiNoteVoices.find(note);
	if (it != inst.midiNoteVoices.end()) {
		voice = it->second;
		inst.midiNoteVoices.erase(it);
	}
	int channel = midiVoiceChannel(inst, voice);
	if (logged(midiText(inst, "note off", channel, note))) return;
	if (inst.getSoundFontChannel() >= 0) sharedData->soundFonts.noteOff(channel, note);
	else midi->noteOff(inst.getMidiPort(), channel, note, 0);
}

//--------------------------------------------------------------
void Sequencer::midiPitchBend(Instrument& inst, int value, int voice)
{
	int channel = midiVoiceChannel(inst, voice);
	if (logged(midiText(inst, "pitch bend", channel, value))) return;
	if (inst.getSoundFontChannel() >= 0) sharedData->soundFonts.pitchBend(channel, value);
	else midi->pitchBend(inst.getMidiPort(), channel, value);
}

//--------------------------------------------------------------
void Sequencer::midiProgramChange(Instrument& inst, int program)
{
	// articulations are sent as program changes, which on a SoundFont would select another instrument, so they are not sent there
	if (inst.getSoundFontChannel() >= 0) return;
	if (logged(midiText(inst, "program change", inst.getMidiChan(), program))) return;
	midi->programChange(inst.getMidiPort(), inst.getMidiChan(), program);
}

//--------------------------------------------------------------
void Sequencer::midiAllSoundOff(Instrument& inst)
{
	// the sound stops on the channels of all voices of the instrument, which bend back to no bend
	for (int voice = 0; voice < inst.numMidiVoices; voice++) {
		int channel = midiVoiceChannel(inst, voice);
		if (logged(midiText(inst, "all sound off", channel, 0))) {}
		else if (inst.getSoundFontChannel() >= 0) {
			sharedData->soundFonts.allSoundOff(channel);
		}
		else {
			// all sound off is control change 120 with value 0
			midi->controlChange(inst.getMidiPort(), channel, 120, 0);
		}
		if (voice > 0) midiPitchBend(inst, 0, voice);
	}
	inst.midiNoteVoices.clear();
	inst.lastPitchBendSent = INT_MIN;
}

//--------------------------------------------------------------
void Sequencer::silenceInstrument(Instrument& inst)
{
	inst.setGlissandoStart(false);
	inst.dynamicsRampActive = false;
	if (inst.isMidi()) {
		for (auto& note : inst.midiNoteVoices) {
			int channel = midiVoiceChannel(inst, note.second);
			if (logged(midiText(inst, "note off", channel, note.first))) continue;
			if (inst.getSoundFontChannel() >= 0) sharedData->soundFonts.noteOff(channel, note.first);
			else midi->noteOff(inst.getMidiPort(), channel, note.first, 0);
		}
		midiAllSoundOff(inst);
	}
	else {
		OscMessage m;
		m.setAddress("/" + inst.getName() + "/dynamics");
		m.addIntArg(0);
		sendOsc(m);
	}
}

//--------------------------------------------------------------
void Sequencer::setSendMidiClock(bool sendMidiClockState)
{
	sendMidiClock = sendMidiClockState;
}

//--------------------------------------------------------------
void Sequencer::sendAllNotesOff()
{
	for (std::map<int, Instrument>::iterator it = sharedData->instruments.begin(); it != sharedData->instruments.end(); ++it) {
		// an instrument with a delay plays another bar than the score, so its notes that are on are stopped from the notes it has played
		if (it->second.getDelayTime() > 0) {
			silenceInstrument(it->second);
			continue;
		}
		if (it->second.isMidi()) {
			// send the note off message for all current notes that are on
			// the notes of the last step the sequencer played, if it has played (tempBar is -1 before it starts, e.g. when quitting)
			auto barIt = it->second.midiNotes.find(tempBar);
			if (barIt != it->second.midiNotes.end() && it->second.tempBarDataCounter >= 0 && it->second.tempBarDataCounter < (int)barIt->second.size()) {
				for (int note : barIt->second[it->second.tempBarDataCounter]) {
					midiNoteOff(it->second, note);
				}
			}
			// safety message for all notes off
			midiAllSoundOff(it->second);
		}
		else {
			OscMessage m;
			m.setAddress("/" + it->second.getName() + "/dynamics");
			m.addIntArg(0);
			sendOsc(m);
			m.clear();
		}
	}
}

//--------------------------------------------------------------
void Sequencer::start()
{
	// a stop that was asked for before (see stop()) doesn't stop this playback
	mustStop = false;
	runSequencer = true;
	sequencerRunning = false;
	startThread();
}

//--------------------------------------------------------------
void Sequencer::stop()
{
	// the sequencer stops at the end of the bar it plays, so there is nothing to stop when it doesn't play
	// (otherwise the next \score.play would stop at once)
	if (isThreadRunning()) mustStop = true;
}

//--------------------------------------------------------------
void Sequencer::stopNow()
{
	sendAllNotesOff();
	delayedVisuals.clear();
	stopThread();
	if (sendMidiClock) sendMidiByte(0xFC); // Stop MIDI byte
	if (!mustStopCalled) onNextStartMidiByte = 0xFB; // Continue MIDI byte
	else mustStopCalled = false;
	runSequencer = sequencerRunning = false;
	beatCounter = 0;
	sendSequencerStateToParts(false);
}

//--------------------------------------------------------------
void Sequencer::update()
{
	updateSequencer = true;
	sequencerUpdated = false;
}

//--------------------------------------------------------------
bool Sequencer::isUpdated()
{
	return sequencerUpdated;
}

//--------------------------------------------------------------
void Sequencer::setSequencerRunning(bool seqRun)
{
	sequencerRunning = seqRun;
}

//--------------------------------------------------------------
void Sequencer::setFinish(bool finishState)
{
	finish = finishState;
	if (finish) sendFinishToParts(finishState);
}

//--------------------------------------------------------------
void Sequencer::setCountdown(int num)
{
	countdown = true;
	countdownCounter = num;
}

//--------------------------------------------------------------
void Sequencer::setMidiTune(float tuneVal)
{
	midiTuneVal = tuneVal;
}

//--------------------------------------------------------------
void Sequencer::sendBeatVizInfo(int bar)
{
	// calculate the time stamp regardless of whether we're showing the beat
	// or not in the editor, in case we have connected score parts
	sharedData->beatVizTimeStamp = Clock::millis();
	// the click track clicks on the same beats, an octave higher on the first beat of a bar (the beats of a countdown are not accented)
	// with the time of the beat (tickCounter, the start of the beat without the delay of this thread), so the click is sample-accurate
	if (sharedData->clickTrack.isOn() && logged(sharedData->beatCounter == 0 ? "click first beat" : "click")) {}
	else sharedData->clickTrack.trigger(sharedData->beatCounter == 0, tickCounter);
	sharedData->beatVizStepsPerMs = (float)BEATVIZBRIGHTNESS / ((float)sharedData->tempoMs[bar] / 4.0);
	sharedData->beatVizRampStart = sharedData->tempoMs[bar] / 4;
	if (sharedData->beatAnimate) {
		sharedData->beatViz = true;
	}
	// notify all score parts of the time stamp and the beat visualization counter
	OscMessage m;
	m.setAddress("/beatinfo");
	m.addFloatArg(sharedData->beatVizStepsPerMs);
	m.addIntArg((int)sharedData->beatVizRampStart);
	((ofApp*)ofGetAppPtr())->sendToParts(m, false);
	m.clear();
	m.setAddress("/beat");
	m.addIntArg(sharedData->beatCounter);
	((ofApp*)ofGetAppPtr())->sendToParts(m, false);
	m.clear();
}

//--------------------------------------------------------------
float Sequencer::midiToFreq(float midiNote)
{
	return pow(2.0, ((midiNote - 69.0) / 12.0)) * midiTuneVal;
}

//--------------------------------------------------------------
float Sequencer::mapVal(float inVal, float fromLow, float fromHigh, float toLow, float toHigh)
{
	return ((inVal - fromLow) * ((toHigh - toLow) / (fromHigh - fromLow))) + toLow;
}

//--------------------------------------------------------------
int Sequencer::nextBar(int bar)
{
	// an instrument with a delay plays ahead of the score, from its own position in the loops
	if (aheadLoopIndex >= 0) {
		int loopNdx = aheadLoopIndex, barNdx = aheadBarIndex;
		advancePosition(loopNdx, barNdx);
		auto loopIt = sharedData->loopData.find(loopNdx);
		if (loopIt != sharedData->loopData.end() && barNdx < (int)loopIt->second.size()) return loopIt->second[barNdx];
	}
	const std::vector<int>& loop = sharedData->loopData.at(sharedData->loopIndex);
	if (loop.empty()) return bar;
	size_t pos = sharedData->thisBarIndex;
	// the position of the bar in the loop, in case it is not the one playing
	if (pos >= loop.size() || loop[pos] != bar) {
		auto it = std::find(loop.begin(), loop.end(), bar);
		if (it == loop.end()) return loop.front();
		pos = it - loop.begin();
	}
	if (pos + 1 < loop.size()) return loop[pos+1];
	// at the end of the loop, another loop that has been called starts
	if (updateSequencer && !sharedData->loopData.at(sharedData->tempLoopIndex).empty()) return sharedData->loopData.at(sharedData->tempLoopIndex).front();
	return loop.front();
}

//--------------------------------------------------------------
bool Sequencer::tiedAcrossBars(Instrument& inst, int fromBar, int toBar)
{
	return tieAcrossBars(inst, fromBar, toBar) == 1;
}

//--------------------------------------------------------------
bool Sequencer::noteTiedToNext(Instrument& inst, int bar, int noteNdx)
{
	if (noteNdx < (int)inst.notes[bar].size() - 1) return inst.isNoteTied(bar, noteNdx);
	return tiedAcrossBars(inst, bar, nextBar(bar));
}

//--------------------------------------------------------------
std::pair<float, int> Sequencer::noteDynamics(Instrument& inst, int bar)
{
	return noteDynamicsAt(inst, bar, inst.getBarDataCounter());
}

//--------------------------------------------------------------
std::pair<float, int> Sequencer::openRampEnd(Instrument& inst, int bar, int start)
{
	int next = nextBar(bar);
	float startDynamic = inst.dynamics[bar][start];
	int startVel = inst.midiVels[bar][start];
	float targetDynamic = startDynamic;
	int targetVel = startVel;
	if (!inst.dynamics[next].empty() && !inst.midiVels[next].empty()) {
		targetDynamic = inst.dynamics[next].front();
		targetVel = inst.midiVels[next].front();
	}
	auto rampIt = inst.openDynamicsRamp.find(bar);
	int dir = (rampIt != inst.openDynamicsRamp.end() && rampIt->second.second == 1 ? 1 : -1);
	// if the next bar doesn't start with a dynamic in the direction of the ramp (e.g. a bar that plays alone, with a crescendo from its first dynamic)
	// the ramp goes one dynamic further, as a ramp without an end dynamic does inside a bar
	if ((targetVel - startVel) * dir <= 0) {
		targetVel = startVel + (MIDIVELSTEP * dir);
		targetDynamic = startDynamic + (((float)MIDIVELSTEP / 127.0f) * dir);
	}
	return std::make_pair(std::clamp(targetDynamic, 0.0f, 1.0f), std::clamp(targetVel, 0, 127));
}

//--------------------------------------------------------------
std::pair<float, int> Sequencer::noteDynamicsAt(Instrument& inst, int bar, int noteNdx)
{
	if (noteNdx < 0 || noteNdx >= (int)inst.dynamics[bar].size() || noteNdx >= (int)inst.midiVels[bar].size()) {
		return std::make_pair(inst.getDynamic(bar), inst.getMidiVel(bar));
	}
	float dynamic = inst.dynamics[bar][noteNdx];
	int midiVel = inst.midiVels[bar][noteNdx];
	auto rampIt = inst.openDynamicsRamp.find(bar);
	if (rampIt == inst.openDynamicsRamp.end() || rampIt->second.first < 0) return std::make_pair(dynamic, midiVel);
	int start = rampIt->second.first;
	int last = (int)inst.dynamics[bar].size() - 1;
	if (noteNdx < start || start > last) return std::make_pair(dynamic, midiVel);
	// the notes of a ramp left open at the end of the bar go from its start to the dynamic of the first note of the next bar
	// which the ramp reaches at that note, so its last note is one step before it
	float startDynamic = inst.dynamics[bar][start];
	int startVel = inst.midiVels[bar][start];
	std::pair<float, int> target = openRampEnd(inst, bar, start);
	float progress = (float)(noteNdx - start) / (float)(last - start + 1);
	dynamic = std::clamp(startDynamic + ((target.first - startDynamic) * progress), 0.0f, 1.0f);
	midiVel = std::clamp((int)std::lround(startVel + ((target.second - startVel) * progress)), 0, 127);
	return std::make_pair(dynamic, midiVel);
}

//--------------------------------------------------------------
bool Sequencer::dynamicsRampTarget(Instrument& inst, int bar, int noteNdx, std::pair<float, int>& target)
{
	int last = (int)inst.dynamics[bar].size() - 1;
	if (noteNdx < 0 || noteNdx > last) return false;
	// a ramp left open at the end of the bar goes on to the first note of the next bar
	auto rampIt = inst.openDynamicsRamp.find(bar);
	if (rampIt != inst.openDynamicsRamp.end() && rampIt->second.first > -1 && noteNdx >= rampIt->second.first) {
		if (noteNdx < last) target = noteDynamicsAt(inst, bar, noteNdx + 1);
		else target = openRampEnd(inst, bar, rampIt->second.first);
		return true;
	}
	// a ramp inside the bar goes from its start to its end, as the score shows it (see Notes::drawDynamics())
	const std::vector<int>& starts = inst.scoreDynamicsRampStart[bar];
	const std::vector<int>& ends = inst.scoreDynamicsRampEnd[bar];
	for (int i = 0; i <= noteNdx && i < (int)starts.size(); i++) {
		if (starts[i] < 0) continue;
		int end = -1;
		for (int j = i + 1; j < (int)ends.size(); j++) {
			if (ends[j] > -1) {
				end = j;
				break;
			}
		}
		if (end > -1 && noteNdx < end) {
			target = noteDynamicsAt(inst, bar, noteNdx + 1);
			return true;
		}
	}
	return false;
}

//--------------------------------------------------------------
void Sequencer::midiExpression(Instrument& inst, int value)
{
	value = std::clamp(value, 0, 127);
	for (int voice = 0; voice < inst.numMidiVoices; voice++) {
		int channel = midiVoiceChannel(inst, voice);
		if (logged(midiText(inst, "control change 11", channel, value))) continue;
		if (inst.getSoundFontChannel() >= 0) sharedData->soundFonts.controlChange(channel, 11, value);
		else midi->controlChange(inst.getMidiPort(), channel, 11, value);
	}
	inst.lastExpressionSent = value;
}

//--------------------------------------------------------------
void Sequencer::sendDynamic(Instrument& inst, std::pair<float, int> dynamic)
{
	if (inst.isMidi()) {
		// the level of the note is its velocity scaled by the expression controller
		midiExpression(inst, (int)std::lround(127.0 * (double)dynamic.second / (double)std::max(1, inst.dynamicsRampVel)));
	}
	else if (inst.sendToPython()) {
#ifdef USEPYO
		std::string pyoStr = inst.getName() + ".setAmp(" + std::to_string(dynamic.first) + ")";
		execPyo(pyoStr);
#endif
	}
	else {
		OscMessage m;
		m.setAddress(setSeqOscAddress(inst.getName(), "/dynamics", false));
		if (inst.sendMIDI()) m.addIntArg(dynamic.second);
		else m.addFloatArg(dynamic.first);
		sendOsc(m);
	}
}

//--------------------------------------------------------------
int Sequencer::startDynamicsRamp(Instrument& inst, int bar, std::pair<float, int> from, std::pair<float, int> to)
{
	inst.dynamicsRampActive = true;
	inst.dynamicsRampFrom = from;
	inst.dynamicsRampTo = to;
	// the ramp lasts the whole duration of the note
	inst.dynamicsRampDurMs = std::max((int64_t)1, (int64_t)((double)inst.durs[bar][inst.getBarDataCounter()] * sharedData->tempo[bar]));
	inst.dynamicsRampStartMs = inst.dynamicsRampLastMs = Clock::millis();
	if (inst.isMidi()) {
		// the note starts with the velocity of the louder end of the ramp, and the expression controller sets the level of its start
		inst.dynamicsRampVel = std::max(1, std::max(from.second, to.second));
		sendDynamic(inst, from);
		return inst.dynamicsRampVel;
	}
	return from.second;
}

//--------------------------------------------------------------
void Sequencer::runDynamicsRamp(int instNdx)
{
	Instrument& inst = sharedData->instruments.at(instNdx);
	if (!inst.dynamicsRampActive) return;
	int64_t timeStamp = Clock::millis();
	if (timeStamp - inst.dynamicsRampLastMs < GLISSGRAIN) return;
	inst.dynamicsRampLastMs = timeStamp;
	float progress = std::clamp((float)(timeStamp - inst.dynamicsRampStartMs) / (float)inst.dynamicsRampDurMs, 0.0f, 1.0f);
	float dynamic = inst.dynamicsRampFrom.first + ((inst.dynamicsRampTo.first - inst.dynamicsRampFrom.first) * progress);
	int midiVel = (int)std::lround(inst.dynamicsRampFrom.second + ((inst.dynamicsRampTo.second - inst.dynamicsRampFrom.second) * progress));
	sendDynamic(inst, std::make_pair(dynamic, midiVel));
	if (progress >= 1) inst.dynamicsRampActive = false;
}

//--------------------------------------------------------------
// starts a glissando from the current note (or chord) to the next one
// every start note gets its own glissando to an end note (see glissandoPairs() in utils.h)
// MIDI and SoundFont instruments play it with pitch bends, with the notes of a chord on separate channels (see midiVoiceChannel())
// this is called before the notes of the glissando start, so their voices are set here
void Sequencer::startGlissando(int instNdx, int bar, int barDataCounter)
{
	Instrument& inst = sharedData->instruments.at(instNdx);
	inst.setGlissandoStart(false);
	inst.glissStart.clear();
	inst.glissEnd.clear();
	inst.glissMidiNotes.clear();
	inst.glissVoices.clear();
	if (inst.notes.find(bar) == inst.notes.end() || barDataCounter >= (int)inst.notes.at(bar).size()) return;
	// the next note is in this bar, or it is the first note of the next bar of the loop
	int nextNote = barDataCounter + 1;
	int nextBarNdx = bar;
	if (nextNote >= (int)inst.notes.at(bar).size()) {
		// a glissando on the last note goes to the first note of the next bar (the same bar if it plays alone)
		nextNote = 0;
		nextBarNdx = nextBar(bar);
	}
	if (inst.notes.find(nextBarNdx) == inst.notes.end() || inst.notes.at(nextBarNdx).empty()) return;
	const std::vector<float>& startNotes = inst.notes.at(bar).at(barDataCounter);
	const std::vector<float>& endNotes = inst.notes.at(nextBarNdx).at(nextNote);
	// no glissando to a rest
	if (startNotes.empty() || endNotes.empty() || startNotes[0] < 0 || endNotes[0] < 0) return;
	std::vector<int> pairs = glissandoPairs(startNotes, endNotes);
	if (pairs.empty() || pairs[0] < 0) return;
	for (size_t i = 0; i < startNotes.size(); i++) {
		inst.glissStart.push_back(startNotes[i]);
		inst.glissEnd.push_back(endNotes[pairs[i]]);
		inst.glissVoices.push_back(startNotes.size() > 1 ? (int)i : 0);
		const std::vector<int>& midiNotes = inst.midiNotes.at(bar).at(barDataCounter);
		inst.glissMidiNotes.push_back(i < midiNotes.size() ? midiNotes[i] : (int)std::round(startNotes[i]));
	}
	inst.glissDurMs = std::max((int64_t)1, (int64_t)inst.glissDursMs.at(bar).at(barDataCounter));
	inst.glissTimeDiff = GLISSGRAIN;
	inst.glissCounter = 0;
	inst.glissStartTimeStamp = inst.glissTimeStamp = Clock::millis();
	inst.setGlissandoStart(true);
	if (inst.isMidi()) {
		// the bends of the start, before the notes start
		sendGlissando(instNdx, 0);
		// the quarter tone of the first note is part of the bend above, so it is not sent again before the notes start (see update())
		inst.lastPitchBendSent = (int)std::lround(std::clamp((inst.glissStart[0] - inst.glissMidiNotes[0]) * 8192.0f / 12.0f, -8192.0f, 8191.0f));
	}
}

//--------------------------------------------------------------
void Sequencer::sendGlissando(int instNdx, float progress)
{
	Instrument& inst = sharedData->instruments.at(instNdx);
	if (inst.isMidi()) {
		for (size_t i = 0; i < inst.glissStart.size(); i++) {
			float pitch = inst.glissStart[i] + ((inst.glissEnd[i] - inst.glissStart[i]) * progress);
			// the bend is relative to the MIDI note that is played, and it can't go further than 12 semitones
			float bend = std::clamp(pitch - (float)inst.glissMidiNotes[i], -12.0f, 12.0f);
			midiPitchBend(inst, (int)std::lround(std::clamp(bend * 8192.0f / 12.0f, -8192.0f, 8191.0f)), inst.glissVoices[i]);
		}
		return;
	}
	OscMessage m;
	std::string s = "";
	m.setAddress("/" + inst.getName() + "/note");
	if (inst.sendToPython() && inst.glissStart.size() > 1) s += "[";
	for (size_t i = 0; i < inst.glissStart.size(); i++) {
		float midiNote = inst.glissStart[i] + ((inst.glissEnd[i] - inst.glissStart[i]) * progress);
		if (inst.sendToPython()) {
#ifdef USEPYO
			if (i > 0) s += ",";
			s += std::to_string(midiToFreq(midiNote));
#endif
		}
		else {
			if (inst.sendMIDI()) m.addFloatArg(midiNote);
			else m.addFloatArg(midiToFreq(midiNote));
		}
	}
	if (inst.sendToPython()) {
#ifdef USEPYO
		if (s[0] == '[') s += "]";
		std::string pyoStr = inst.getName() + ".setFreq(" + s + ")";
		execPyo(pyoStr);
#endif
	}
	else {
		sendOsc(m);
	}
}

//--------------------------------------------------------------
void Sequencer::runGlissando(int instNdx, int bar)
{
	// here we run a glissando that has been started by the function above, every GLISSGRAIN ms
	// its pitches depend on the time since it started, so they reach the end notes at the end of its duration
	Instrument& inst = sharedData->instruments.at(instNdx);
	if (!inst.getGlissandoStart()) return;
	int64_t timeStamp = Clock::millis();
	if (inst.glissCounter > 0 && timeStamp - inst.glissTimeStamp < inst.glissTimeDiff) return;
	float progress = std::clamp((float)(timeStamp - inst.glissStartTimeStamp) / (float)inst.glissDurMs, 0.0f, 1.0f);
	sendGlissando(instNdx, progress);
	if (inst.isMidi()) {
		// the next note sends its pitch bend for quarter tones again (see update())
		inst.lastPitchBendSent = INT_MIN;
	}
	inst.glissCounter++;
	inst.glissTimeStamp = timeStamp;
	if (progress >= 1) inst.setGlissandoStart(false);
}

//--------------------------------------------------------------
void Sequencer::setCheckLoop(std::string barLoopName, int numIter)
{
	loopToCheckNdx = sharedData->loopsIndexes[barLoopName];
	loopToCheckNumIter = numIter;
	loopToCheckIterCounter = 0;
	loopToCheckBarNdx = 0;
	checkLoop = true;
}

//--------------------------------------------------------------
void Sequencer::sendToParts(OscMessage m, bool delay)
{
	for (auto it = sharedData->grouppedOSCClients.begin(); it != sharedData->grouppedOSCClients.end(); ++it) {
		if (logged("osc part " + sharedData->instruments[*it].getName() + " " + oscText(m))) continue;
		if (sharedData->instruments[*it].hasDelay() && delay) {
			std::pair<OscMessage, unsigned long> p = std::make_pair(m, Clock::millis());
			sharedData->instruments[*it].oscFifo.push_back(p);
		}
		else {
			sharedData->instruments[*it].scorePartSender->send(m);
		}
	}
}

//--------------------------------------------------------------
void Sequencer::sendSequencerStateToParts(bool state)
{
	OscMessage m;
	m.setAddress("/seq");
	m.addBoolArg(state);
	sendToParts(m, false);
	m.clear();
}

//--------------------------------------------------------------
void Sequencer::sendLoopIndexToParts()
{
	OscMessage m;
	m.setAddress("/loopndx");
	m.addIntArg(sharedData->loopIndex);
	sendToParts(m, false);
	m.clear();
}

//--------------------------------------------------------------
void Sequencer::sendStopCountdownToParts()
{
	OscMessage m;
	m.setAddress("/nocountdown");
	m.addIntArg(1); // dummy arg
	sendToParts(m, true);
	m.clear();
}

//--------------------------------------------------------------
void Sequencer::sendCountdownToParts(int countdown)
{
	OscMessage m;
	m.setAddress("/countdown");
	m.addIntArg(countdown);
	sendToParts(m, true);
	m.clear();
}

//--------------------------------------------------------------
void Sequencer::sendFinishToParts(bool finishState)
{
	OscMessage m;
	m.setAddress("/finish");
	m.addBoolArg(finishState);
	sendToParts(m, true);
	m.clear();
}

//--------------------------------------------------------------
std::string Sequencer::setSeqOscAddress(std::string instName, std::string s, bool checking)
{
	std::string address = "/" + instName;
	if (checking) address += "/checking";
	address += s;
	return address;
}

//--------------------------------------------------------------
void Sequencer::fireNote(std::map<int, Instrument>::iterator instMapIt, int bar, uint64_t timeStamp, bool checking)
{
	// an instrument added while the sequencer plays waits for the start of the next bar (see below, where the bar data counters are reset)
	if (!instMapIt->second.timeSynced) return;
	if (instMapIt->second.hasNotesInBar(bar)) {
		if (instMapIt->second.mustFireStep(timeStamp, bar, sharedData->tempo[bar])) {
			// the bar played before this one, also when the sequencer has switched to another loop
			int prevBarNdx = (int)sharedData->thisBarIndex - 1 < 0 ? (int)sharedData->loopData[sharedData->loopIndex].size() - 1 : (int)sharedData->thisBarIndex - 1;
			int prevBar = (previousBar >= 0 ? previousBar : sharedData->loopData[sharedData->loopIndex][prevBarNdx]);
			// the first note of a bar is tied to the last note of the previous bar only if they are the same notes (see tiedAcrossBars())
			bool tiedFromPrevBar = (previousBar >= 0 && !sequencerStart && tiedAcrossBars(instMapIt->second, previousBar, bar));
			// a slur at the end of the previous bar goes on to the first note of this bar
			bool slurredFromPrevBar = (previousBar >= 0 && !sequencerStart && !tiedFromPrevBar &&
					!instMapIt->second.isSlurred[previousBar].empty() && instMapIt->second.isSlurred[previousBar].back());
			// the dynamic of this note (see noteDynamics())
			std::pair<float, int> dynamics = noteDynamics(instMapIt->second, bar);
			// if we have a note, not a rest, and the current instrument is not muted
			if (instMapIt->second.hasNotesInStep(bar) && !mutedForSound(instMapIt->second)) {
				// first determine if we must skip sending notes that have 100% note on duration and are the same as the previous one
				std::vector<bool> isNoteSame(instMapIt->second.midiNotes[bar][instMapIt->second.barDataCounter].size(), false);
				// do the same for previous notes, as in case of different notes, we must also send note off messages for previous notes
				int prevNoteNdx;
				int barLocal;
				// have to check for same notes and rests
				if (instMapIt->second.barDataCounter > 0) {
					barLocal = bar;
					prevNoteNdx = instMapIt->second.barDataCounter - 1;
				}
				else {
					barLocal = prevBar;
					prevNoteNdx = (int)instMapIt->second.midiNotes[prevBar].size() - 1;
				}
				std::vector<bool> isNoteSamePrevious(instMapIt->second.midiNotes[barLocal][prevNoteNdx].size(), false);
				int sameNoteNdx = 0;
				if (instMapIt->second.getDurPercentage(bar) == 100) {
					// store booleans for the current notes
					for (auto it = instMapIt->second.midiNotes[bar][instMapIt->second.barDataCounter].begin(); it != instMapIt->second.midiNotes[bar][instMapIt->second.barDataCounter].end(); ++it) {
						for (auto it2 = instMapIt->second.midiNotes[barLocal][prevNoteNdx].begin(); it2 != instMapIt->second.midiNotes[barLocal][prevNoteNdx].end(); ++it2) {
							if (*it == *it2) {
								isNoteSame[sameNoteNdx] = true;
								break;
							}
						}
						sameNoteNdx++;
					}
					// then do the same for the previous notes
					sameNoteNdx = 0;
					for (auto it = instMapIt->second.midiNotes[barLocal][prevNoteNdx].begin(); it != instMapIt->second.midiNotes[barLocal][prevNoteNdx].end(); ++it) {
						for (auto it2 = instMapIt->second.midiNotes[bar][instMapIt->second.barDataCounter].begin(); it2 != instMapIt->second.midiNotes[bar][instMapIt->second.barDataCounter].end(); ++it2) {
							if (*it == *it2) {
								isNoteSamePrevious[sameNoteNdx] = true;
								break;
							}
						}
						sameNoteNdx++;
					}
				}
				if (!instMapIt->second.isMidi()) { // send OSC message
					// if the toggle is on and the note is not slurred or tied, send a note off
					if (instMapIt->second.getSeqToggle()) {
						if (!instMapIt->second.isNoteSlurred(bar, instMapIt->second.getBarDataCounter()) && \
								!noteTiedToNext(instMapIt->second, bar, instMapIt->second.getBarDataCounter())) {
							if (instMapIt->second.sendToPython()) {
#ifdef USEPYO
								std::string pyoStr = instMapIt->second.getName();
								if (!instMapIt->second.isRhythm()) {
									pyoStr += ".setAmp(0)";
									execPyo(pyoStr);
								}
#endif
							}
							else {
								OscMessage m;
								m.setAddress(setSeqOscAddress(instMapIt->second.getName(), "/dynamics", checking));
								m.addIntArg(0);
								sendOsc(m);
								m.clear();
							}
							visualEditorElement(instMapIt, bar, instMapIt->second.getBarDataCounter(), false);
						}
					}
					else {
						// a glissando that is still running stops when the next note starts, so that its last pitch doesn't bend the new note
						// (its last step can come at the same time as the next note, after it)
						instMapIt->second.setGlissandoStart(false);
						// the same for a crescendo or diminuendo during the previous note
						instMapIt->second.dynamicsRampActive = false;
						bool sendData;
						if (instMapIt->second.getBarDataCounter() > 0) {
							if (!instMapIt->second.isNoteTied(bar, instMapIt->second.getBarDataCounter()-1)) {
								sendData = true;
							}
							else {
								sendData = false;
							}
						}
						else {
							sendData = !tiedFromPrevBar;
						}
						if (instMapIt->second.glissandi.at(bar).at(instMapIt->second.getBarDataCounter()) == 1) {
							startGlissando(instMapIt->first, bar, instMapIt->second.getBarDataCounter());
						}
						if (sendData) {
							OscMessage m;
							m.setAddress(setSeqOscAddress(instMapIt->second.getName(), "/articulation", checking));
							for (auto it = instMapIt->second.articulations[bar][instMapIt->second.getBarDataCounter()].begin(); it != instMapIt->second.articulations[bar][instMapIt->second.getBarDataCounter()].end(); ++it) {
								m.addStringArg(sharedData->articulSyms[*it]);
							}
							sendOsc(m);
							m.clear();
							if (instMapIt->second.hasText(bar)) {
								m.setAddress(setSeqOscAddress(instMapIt->second.getName(), "/text", checking));
								m.addStringArg(instMapIt->second.getText(bar));
								sendOsc(m);
								m.clear();
							}
							if (!instMapIt->second.sendToPython()) {
								m.setAddress(setSeqOscAddress(instMapIt->second.getName(), "/note", checking));
							}
							int i = 0;
							std::string s = "";
							if (instMapIt->second.sendToPython() && instMapIt->second.notes[bar][instMapIt->second.getBarDataCounter()].size() > 1) {
								s += "[";
							}
							for (auto it = instMapIt->second.notes[bar][instMapIt->second.getBarDataCounter()].begin(); it != instMapIt->second.notes[bar][instMapIt->second.getBarDataCounter()].end(); ++it) {
								if (instMapIt->second.sendToPython()) {
									if (i > 0) s += ",";
									s += std::to_string(midiToFreq(*it));
								}
								else {
									if (instMapIt->second.sendMIDI()) m.addFloatArg(*it);
									else m.addFloatArg(midiToFreq(*it));
								}
								i++;
							}
							if (instMapIt->second.sendToPython()) {
#ifdef USEPYO
								if (i > 1) s += "]";
								if (!instMapIt->second.isRhythm()) {
									std::string pyoStr = instMapIt->second.getName() + ".setFreq(" + s + ")";
									execPyo(pyoStr);
								}
#endif
							}
							else {
								sendOsc(m);
								m.clear();
							}
							if (instMapIt->second.sendToPython()) {
#ifdef USEPYO
								std::string pyoStr = instMapIt->second.getName();
								pyoStr += (".setAmp(" + std::to_string(dynamics.first) + ")");
								execPyo(pyoStr);
								if (instMapIt->second.isRhythm()) {
									pyoStr.clear();
									pyoStr = instMapIt->second.getName() + ".play()";
									execPyo(pyoStr);
								}
#endif
							}
							else {
								m.setAddress(setSeqOscAddress(instMapIt->second.getName(), "/dynamics", checking));
								if (instMapIt->second.sendMIDI()) m.addIntArg(dynamics.second);
								else m.addFloatArg(dynamics.first);
								sendOsc(m);
								m.clear();
							}
							// a note in a crescendo or diminuendo changes its dynamic during its duration
							std::pair<float, int> rampTarget;
							if (dynamicsRampTarget(instMapIt->second, bar, instMapIt->second.getBarDataCounter(), rampTarget)) {
								startDynamicsRamp(instMapIt->second, bar, dynamics, rampTarget);
							}
							visualEditorElement(instMapIt, bar, instMapIt->second.getBarDataCounter(), true);
						}
						else {
							// a note tied from the previous note is not played again, but its token is highlighted in the editor
							// in place of the token of the previous note (e.g. 4 in c'4~ 4)
							visualEditorElement(instMapIt, bar, instMapIt->second.getBarDataCounter(), true);
						}
						// if the previous note was slurred, but not tied, deactivate the editor element
						if (instMapIt->second.getBarDataCounter() > 0) {
							if (instMapIt->second.isNoteSlurred(bar, instMapIt->second.getBarDataCounter()-1) && \
									!instMapIt->second.isNoteTied(bar, instMapIt->second.getBarDataCounter()-1)) {
								visualEditorElement(instMapIt, bar, instMapIt->second.getBarDataCounter()-1, false);
							}
						}
						else if (slurredFromPrevBar) {
							visualEditorElement(instMapIt, prevBar, (int)instMapIt->second.notes[prevBar].size()-1, false);
						}
					}
				}
				// sending MIDI
				else {
					if (instMapIt->second.getSeqToggle()) {
						if (!instMapIt->second.isNoteSlurred(bar, instMapIt->second.getBarDataCounter()) && \
								!noteTiedToNext(instMapIt->second, bar, instMapIt->second.getBarDataCounter())) {
							sameNoteNdx = 0;
							for (auto it = instMapIt->second.midiNotes[bar][instMapIt->second.getBarDataCounter()].begin(); it != instMapIt->second.midiNotes[bar][instMapIt->second.getBarDataCounter()].end(); ++it) {
								if (!isNoteSame[sameNoteNdx++]) {
									midiNoteOff(instMapIt->second, *it);
								}
							}
							visualEditorElement(instMapIt, bar, instMapIt->second.getBarDataCounter(), false);
						}
					}
					else {
						// a glissando that is still running stops when the next note starts, so that its last pitch doesn't bend the new note
						// (its last step can come at the same time as the next note, after it)
						instMapIt->second.setGlissandoStart(false);
						// the same for a crescendo or diminuendo during the previous note
						instMapIt->second.dynamicsRampActive = false;
						bool sendData;
						if (instMapIt->second.getBarDataCounter() > 0) {
							if (!instMapIt->second.isNoteTied(bar, instMapIt->second.getBarDataCounter()-1)) {
								sendData = true;
							}
							else {
								sendData = false;
							}
						}
						else {
							sendData = !tiedFromPrevBar;
						}
						if (instMapIt->second.glissandi.at(bar).at(instMapIt->second.getBarDataCounter()) == 1) {
							startGlissando(instMapIt->first, bar, instMapIt->second.getBarDataCounter());
						}
						if (sendData) {
							//std::cout << "bar: " << bar << ", barLocal: " << barLocal << ", prevNoteNdx: " << prevNoteNdx << std::endl;
							//midiOuts[midiPortsMap[instMapIt->second.getMidiPort()]].sendPitchBend(instMapIt->second.getMidiChan(), instMapIt->second.getPitchBendVal(bar));
							for (auto it = instMapIt->second.midiArticulationVals[bar][instMapIt->second.barDataCounter].begin(); it != instMapIt->second.midiArticulationVals[bar][instMapIt->second.barDataCounter].end(); ++it) {
								midiProgramChange(instMapIt->second, *it);
							}
							// check here if we have 100% duration and a different previous note
							// in this case, we must send a note off message for this previous note
							// together with the note on messsage for the current note
							if (!sequencerStart && instMapIt->second.getDurPercentage(bar) == 100) {
								sameNoteNdx = 0;
								for (auto it = instMapIt->second.midiNotes[barLocal][prevNoteNdx].begin(); it != instMapIt->second.midiNotes[barLocal][prevNoteNdx].end(); ++it) {
									if (!isNoteSamePrevious[sameNoteNdx++]) {
										midiNoteOff(instMapIt->second, *it);
									}
								}
							}
							// quarter tones are played with a pitch bend, sent when it changes, before the note on messages
							// a note with a glissando has its bends from startGlissando(), which include its quarter tones
							if (instMapIt->second.isMidi() && instMapIt->second.glissandi.at(bar).at(instMapIt->second.getBarDataCounter()) != 1) {
								int pitchBend = instMapIt->second.getPitchBendVal(bar);
								if (pitchBend != instMapIt->second.lastPitchBendSent) {
									midiPitchBend(instMapIt->second, pitchBend);
									instMapIt->second.lastPitchBendSent = pitchBend;
								}
							}
							// a note in a crescendo or diminuendo starts with the velocity of the louder end of the ramp
							// and the expression controller changes its level during its duration (see startDynamicsRamp())
							// other notes play with full expression
							int noteVel = dynamics.second;
							std::pair<float, int> rampTarget;
							if (dynamicsRampTarget(instMapIt->second, bar, instMapIt->second.getBarDataCounter(), rampTarget)) {
								noteVel = startDynamicsRamp(instMapIt->second, bar, dynamics, rampTarget);
							}
							else if (instMapIt->second.lastExpressionSent != 127) {
								midiExpression(instMapIt->second, 127);
							}
							// then send the note on messages
							// the notes of a chord with a glissando play on a channel each, so each bends on its own (see startGlissando())
							bool chordGlissando = instMapIt->second.glissandi.at(bar).at(instMapIt->second.getBarDataCounter()) == 1 &&
								instMapIt->second.midiNotes[bar][instMapIt->second.barDataCounter].size() > 1;
							sameNoteNdx = 0;
							for (auto it = instMapIt->second.midiNotes[bar][instMapIt->second.barDataCounter].begin(); it != instMapIt->second.midiNotes[bar][instMapIt->second.barDataCounter].end(); ++it) {
								int voice = (chordGlissando ? sameNoteNdx : 0);
								if (!isNoteSame[sameNoteNdx++] || sequencerStart) {
									midiNoteOn(instMapIt->second, *it, noteVel, voice);
								}
							}
							visualEditorElement(instMapIt, bar, instMapIt->second.getBarDataCounter(), true);
						}
						else {
							// a note tied from the previous note is not played again, but its token is highlighted in the editor
							// in place of the token of the previous note (e.g. 4 in c'4~ 4)
							visualEditorElement(instMapIt, bar, instMapIt->second.getBarDataCounter(), true);
						}
						// if the previous note was slurred, but not tied, send the note off message after the note on of the new note
						if (instMapIt->second.getBarDataCounter() > 0) {
							if (instMapIt->second.isNoteSlurred(bar, instMapIt->second.getBarDataCounter()-1) && \
									!instMapIt->second.isNoteTied(bar, instMapIt->second.getBarDataCounter()-1)) {
								sameNoteNdx = 0;
								for (auto it = instMapIt->second.midiNotes[bar][instMapIt->second.getBarDataCounter()-1].begin(); it != instMapIt->second.midiNotes[bar][instMapIt->second.getBarDataCounter()-1].end(); ++it) {
									if (!isNoteSamePrevious[sameNoteNdx++]) {
										midiNoteOff(instMapIt->second, *it);
									}
								}
								visualEditorElement(instMapIt, bar, instMapIt->second.getBarDataCounter()-1, false);
							}
						}
						// the same for a slur at the end of the previous bar
						else if (slurredFromPrevBar) {
							sameNoteNdx = 0;
							for (auto it = instMapIt->second.midiNotes[prevBar].back().begin(); it != instMapIt->second.midiNotes[prevBar].back().end(); ++it) {
								if (!isNoteSamePrevious[sameNoteNdx++]) {
									midiNoteOff(instMapIt->second, *it);
								}
							}
							visualEditorElement(instMapIt, prevBar, (int)instMapIt->second.notes[prevBar].size()-1, false);
						}
					}
				}
			}
			else {
				// a glissando that is still running stops at a rest too
				instMapIt->second.setGlissandoStart(false);
				instMapIt->second.dynamicsRampActive = false;
				// a rest is highlighted in the editor for its whole duration, if it is written as a rest (see ofApp::getActiveEditorNoteSpans())
				// so it is set when it starts, and the next note or rest replaces it
				// this branch is also called for the notes of a muted instrument, which are not highlighted, and stop the highlight of a rest
				// a muted instrument highlights nothing, also not its rests
				if (!instMapIt->second.getSeqToggle()) {
					if (!instMapIt->second.hasNotesInStep(bar) && !mutedForSound(instMapIt->second)) {
						visualEditorRest(instMapIt, bar, instMapIt->second.getBarDataCounter());
					}
					else {
						visualEditorClear(instMapIt);
					}
				}
				if (!instMapIt->second.isMidi()) {
					// in case of a rest, send a 0 dynamic to make sure the instMapInstrument will stop
					OscMessage m;
					m.setAddress(setSeqOscAddress(instMapIt->second.getName(), "/dynamics", checking));
					m.addIntArg(0);
					sendOsc(m);
					m.clear();
				}
				else {
					// send all sound off with CC #120 with value 0
					midiAllSoundOff(instMapIt->second);
				}
			}
			if (sharedData->animate && !instMapIt->second.getSeqToggle()) {
				visualActiveNote(instMapIt);
			}
			if (instMapIt->second.getSeqToggle()) {
				// collect the names of the functions to call while holding the lock
				// and call them after releasing it, as calling a function locks it again
				std::vector<std::string> funcsToCall;
				{
					std::lock_guard<std::mutex> lock(sharedData->functionsMutex);
					for (auto func = sharedData->functions.begin(); func != sharedData->functions.end(); ++func) {
						if (func->second.getBoundInst() == instMapIt->first) {
							if (instMapIt->second.getBarDataCounter() == 0) {
								// this will not reset the first time a function is bound to an instrument
								// so that we can set a calling step to not be the first one
								// and when we bind the function, the first time it will be triggered at the
								// desired step, then it will wrap around the number of steps of the instrument
								func->second.resetCallingStep();
							}
							if (func->second.getCallingStep() == instMapIt->second.getBarDataCounter()) {
								func->second.addStepIncrement(instMapIt->second.getBarDataCounter());
								funcsToCall.push_back(func->second.getName());
							}
						}
					}
				}
				for (const std::string& funcName : funcsToCall) {
					//((ofApp*)ofGetAppPtr())->parseCommand(((ofApp*)ofGetAppPtr())->genCmdInput(funcName), 1, 1); // dummy 2nd and 3rd arguments
					visualBoundFunction(instMapIt, funcName);
				}
			}
			// toggleSeqToggle() takes care to increment the barDataCounter of the class
			// but before doing that, store the current data counter to a temp var
			// this is used for sending all notes off, which is nessary for a suddent stop of the sequencer
			// or for stopping the sequencer when the notes duration is set to 100%
			instMapIt->second.storeTempBarDataCounter();
			instMapIt->second.toggleSeqToggle(bar);
		}
	}
}

//--------------------------------------------------------------
void Sequencer::clearDelayedVisuals()
{
	delayedVisuals.clear();
}

//--------------------------------------------------------------
void Sequencer::runDelayedVisuals(int64_t now)
{
	while (!delayedVisuals.empty() && delayedVisuals.begin()->first <= now) {
		std::function<void()> fn = delayedVisuals.begin()->second;
		delayedVisuals.erase(delayedVisuals.begin());
		fn();
	}
}

//--------------------------------------------------------------
void Sequencer::visual(Instrument& inst, std::function<void()> fn)
{
	if (aheadPass && inst.getDelayTime() > 0) delayedVisuals.emplace((int64_t)Clock::micros() + inst.getDelayTime(), fn);
	else fn();
}

//--------------------------------------------------------------
void Sequencer::visualEditorElement(std::map<int, Instrument>::iterator instMapIt, int bar, int ndx, bool active)
{
	int instNdx = instMapIt->first;
	visual(instMapIt->second, [this, instNdx, bar, ndx, active]() { frontend->notePlaying(instNdx, bar, ndx, active); });
}

//--------------------------------------------------------------
void Sequencer::visualEditorRest(std::map<int, Instrument>::iterator instMapIt, int bar, int ndx)
{
	int instNdx = instMapIt->first;
	visual(instMapIt->second, [this, instNdx, bar, ndx]() { frontend->restPlaying(instNdx, bar, ndx); });
}

//--------------------------------------------------------------
void Sequencer::visualEditorClear(std::map<int, Instrument>::iterator instMapIt)
{
	int instNdx = instMapIt->first;
	visual(instMapIt->second, [this, instNdx]() { frontend->nothingPlaying(instNdx); });
}

//--------------------------------------------------------------
void Sequencer::visualActiveNote(std::map<int, Instrument>::iterator instMapIt)
{
	int instNdx = instMapIt->first;
	int ndx = instMapIt->second.getBarDataCounter();
	SharedData *sData = sharedData;
	visual(instMapIt->second, [sData, instNdx, ndx]() {
		auto it = sData->instruments.find(instNdx);
		if (it != sData->instruments.end()) it->second.setActiveNote(ndx);
	});
}

//--------------------------------------------------------------
void Sequencer::visualBoundFunction(std::map<int, Instrument>::iterator instMapIt, std::string funcName)
{
	visual(instMapIt->second, [this, funcName]() { callBoundFunction(funcName); });
}

//--------------------------------------------------------------
bool Sequencer::mutedForSound(Instrument& inst)
{
	if (inst.soundMuteState >= 0) return inst.soundMuteState == 1;
	return inst.isMuted();
}

//--------------------------------------------------------------
int64_t Sequencer::barDurUs(int bar)
{
	if (sharedData->numBeats.find(bar) == sharedData->numBeats.end() || sharedData->tempo.find(bar) == sharedData->tempo.end()) return 0;
	return (int64_t)((double)sharedData->numBeats[bar] * sharedData->tempo[bar] * 1000.0);
}

//--------------------------------------------------------------
void Sequencer::advancePosition(int& loopNdx, int& barNdx)
{
	auto loopIt = sharedData->loopData.find(loopNdx);
	if (loopIt != sharedData->loopData.end() && barNdx + 1 < (int)loopIt->second.size()) {
		barNdx++;
		return;
	}
	// at the end of the loop, another loop that has been called starts, as in threadedFunction()
	if (updateSequencer && !sharedData->loopData[sharedData->tempLoopIndex].empty()) loopNdx = sharedData->tempLoopIndex;
	barNdx = 0;
}

//--------------------------------------------------------------
void Sequencer::syncDelayedInstrument(std::map<int, Instrument>::iterator instMapIt, int loopNdx, int barNdx, int64_t scoreBarStart, int64_t now, bool onlyIfChanged)
{
	Instrument& inst = instMapIt->second;
	int64_t delay = inst.getDelayTime();
	// the instrument plays now what the score plays after its delay
	// so we find the bar of the score at that time, from the bar of the score and the time it starts
	int64_t barStart = scoreBarStart;
	int64_t target = now + delay;
	for (int i = 0; i < 10000; i++) {
		auto loopIt = sharedData->loopData.find(loopNdx);
		if (loopIt == sharedData->loopData.end() || barNdx >= (int)loopIt->second.size()) return;
		int64_t dur = barDurUs(loopIt->second[barNdx]);
		if (dur <= 0 || barStart + dur > target) break;
		barStart += dur;
		advancePosition(loopNdx, barNdx);
	}
	if (onlyIfChanged && inst.soundSynced && inst.soundLoopIndex == loopNdx && inst.soundBarIndex == barNdx) return;
	// notes that sound from another bar are stopped
	if (inst.soundSynced) silenceInstrument(inst);
	int bar = sharedData->loopData[loopNdx][barNdx];
	// the instrument starts this bar its delay before the score does
	int64_t realBarStart = barStart - delay;
	// and plays from its first step that hasn't started yet
	int step = 0;
	int64_t stepStart = 0;
	if (now > realBarStart && sharedData->tempo.find(bar) != sharedData->tempo.end()) {
		int numSteps = (inst.durs.find(bar) != inst.durs.end() ? (int)inst.durs[bar].size() : 0);
		while (step < numSteps && realBarStart + inst.stepStartUs(bar, step, sharedData->tempo[bar]) < now) step++;
		stepStart = inst.stepStartUs(bar, step, sharedData->tempo[bar]);
	}
	inst.soundLoopIndex = loopNdx;
	inst.soundBarIndex = barNdx;
	inst.soundBar = bar;
	inst.soundBarStart = realBarStart;
	inst.soundPrevBar = -1;
	inst.soundFirstStep = true;
	inst.setSeqStep(step, realBarStart + stepStart);
	inst.setGlissandoStart(false);
	inst.dynamicsRampActive = false;
	inst.soundSynced = true;
}

//--------------------------------------------------------------
void Sequencer::playDelayedInstrument(std::map<int, Instrument>::iterator instMapIt, int64_t now)
{
	Instrument& inst = instMapIt->second;
	if (!inst.soundSynced || now < inst.soundBarStart) return;
	// the instrument moves on to its next bar by itself, before the score
	int64_t dur = barDurUs(inst.soundBar);
	while (dur > 0 && now >= inst.soundBarStart + dur) {
		// but not after the last bar, when the sequencer stops at the end of the bar, or finishes at the end of the loop
		auto loopIt = sharedData->loopData.find(inst.soundLoopIndex);
		if (mustStop || finished || loopIt == sharedData->loopData.end()) return;
		if (finish && inst.soundBarIndex >= (int)loopIt->second.size() - 1) return;
		int prevBar = inst.soundBar;
		advancePosition(inst.soundLoopIndex, inst.soundBarIndex);
		loopIt = sharedData->loopData.find(inst.soundLoopIndex);
		if (loopIt == sharedData->loopData.end() || inst.soundBarIndex >= (int)loopIt->second.size()) return;
		inst.soundBar = loopIt->second[inst.soundBarIndex];
		inst.soundBarStart += dur;
		inst.soundPrevBar = prevBar;
		inst.setSeqStep(0, inst.soundBarStart);
		// \mute and \unmute apply at the start of the next bar, which this instrument plays before the score
		// the score shows it at the start of its bar (see checkMute())
		if (inst.isToBeMuted()) inst.soundMuteState = 1;
		else if (inst.isToBeUnmuted()) inst.soundMuteState = 0;
		dur = barDurUs(inst.soundBar);
	}
	// the functions that play the notes use the bar of the instrument, and its own position for the bar that comes next
	int savedPreviousBar = previousBar;
	bool savedSequencerStart = sequencerStart;
	previousBar = inst.soundPrevBar;
	sequencerStart = inst.soundFirstStep;
	aheadPass = true;
	aheadLoopIndex = inst.soundLoopIndex;
	aheadBarIndex = inst.soundBarIndex;
	int stepBefore = inst.barDataCounter;
	int toggleBefore = inst.getSeqToggle();
	runGlissando(instMapIt->first, inst.soundBar);
	runDynamicsRamp(instMapIt->first);
	fireNote(instMapIt, inst.soundBar, (uint64_t)now, false);
	if (inst.barDataCounter != stepBefore || inst.getSeqToggle() != toggleBefore) inst.soundFirstStep = false;
	aheadPass = false;
	aheadLoopIndex = -1;
	previousBar = savedPreviousBar;
	sequencerStart = savedSequencerStart;
}

//--------------------------------------------------------------
void Sequencer::threadedFunction()
{
	while (isThreadRunning()) {
		timer.waitNext();
		if (!step()) return;
	}
}

//--------------------------------------------------------------
bool Sequencer::step()
{
	{
		// instruments are not added or removed while the sequencer goes through them in this step (see ofApp::removeInstrument())
		std::lock_guard<std::recursive_mutex> instrumentsLock(sharedData->instrumentsMutex);
		if (runSequencer && !sequencerRunning) {
			int bar = sharedData->loopData[sharedData->loopIndex][sharedData->thisBarIndex];
			// in here we calculate the number of beats on a microsecond basis
			numBeats = sharedData->numBeats[bar] * sharedData->tempo[bar];
			if (sharedData->showNotes) {
				if (sharedData->setAnimation) {
					for (auto instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
						instMapIt->second.setAnimation(true);
					}
					sharedData->animate = true;
					sharedData->setAnimation = false;
				}
				if (sharedData->setBeatAnimation) {
					sharedData->beatViz = true;
					sharedData->beatAnimate = true;
					sharedData->setBeatAnimation = false;
				}
			}
			// calculate the following two in case we connect to a score part
			sharedData->beatVizStepsPerMs = (float)BEATVIZBRIGHTNESS / ((float)sharedData->tempoMs[bar] / 4.0);
			sharedData->beatVizRampStart = sharedData->tempoMs[bar] / 4;
			sharedData->beatAtValCounter = 0;
			sendFinishToParts(false);
			OscMessage m;
			m.setAddress("/beatinfo");
			m.addFloatArg(sharedData->beatVizStepsPerMs);
			m.addIntArg(sharedData->beatVizRampStart);
			sendToParts(m, true);
			m.clear();
			sequencerRunning = true;
			sequencerStart = true;
			runSequencer = false;
			if (sendMidiClock) sendMidiByte(onNextStartMidiByte);
			firstIter = true;
			endOfBar = false;
			sendBeatVizInfoCounter = 0;
			for (auto instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
				instMapIt->second.initSeqToggle();
				instMapIt->second.setFirstIter(true);
			}
			thisBarIndex = sharedData->thisBarIndex;
			sendSequencerStateToParts(true);
			// the score starts after the longest delay of the instruments, and each instrument with a delay starts its delay before the score
			// so the instrument with the longest delay starts now
			int64_t now = (int64_t)Clock::micros();
			int64_t maxDelay = 0;
			for (auto instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
				maxDelay = std::max(maxDelay, instMapIt->second.getDelayTime());
			}
			scoreStartTime = now + maxDelay;
			delayedVisuals.clear();
			// with a countdown, the first bar starts after the beats of the countdown
			int64_t firstBarStart = scoreStartTime + (countdown ? (int64_t)countdownCounter * sharedData->tempoMs[bar] * 1000 : 0);
			for (auto instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
				instMapIt->second.soundSynced = false;
				instMapIt->second.soundMuteState = -1;
				if (instMapIt->second.getDelayTime() > 0) {
					syncDelayedInstrument(instMapIt, sharedData->loopIndex, sharedData->thisBarIndex, firstBarStart, now, false);
				}
			}
		}

		if (sequencerRunning) {
			uint64_t timeStamp = Clock::micros();
			// the highlights of instruments with a delay are shown after their delay, when their notes sound
			runDelayedVisuals((int64_t)timeStamp);
			// before the score starts, only the instruments with a delay play (see the start of the sequencer above)
			if ((int64_t)timeStamp < scoreStartTime) {
				for (auto instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
					if (instMapIt->second.getDelayTime() > 0) playDelayedInstrument(instMapIt, (int64_t)timeStamp);
				}
				return true;
			}
			int bar = sharedData->loopData[sharedData->loopIndex][sharedData->thisBarIndex];
			// check if we're at the beginning of the loop
			if (sharedData->thisBarIndex == 0) {
				if (!finished) {
					// notify scores that this is the first bar of a loop
					// if the first instItrument has been notified, then all instItruments will
					// so we don't bother to check every instItrument explicitly
					// this information concerns the score
					auto instMapIt = sharedData->instruments.begin();
					if (!instMapIt->second.isLoopStart()) {
						for (instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
							instMapIt->second.setLoopStart(true);
						}
					}
					if (checkLoop) {
						loopToCheckBarNdx = 0;
					}
				}
			}
			else {
				// if the last instItrument has its isLoopStart boolean to true
				// set all instItruments booleans to false
				instMapRevIt = sharedData->instruments.rbegin();
				if (instMapRevIt->second.isLoopStart()) {
					for (auto instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
						instMapIt->second.setLoopStart(false);
					}
				}
			}
			// check if we're at the end of the loop
			if (sharedData->thisBarIndex == sharedData->loopData[sharedData->loopIndex].size()-1) {
				// first check if we're finishing
				if (finish) {
					// falsify both isLoopStart and isLoopEnd for all staffs first
					// and notify all staffs of the finale
					for (auto instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
						instMapIt->second.setLoopStart(false);
						instMapIt->second.setLoopEnd(false);
						instMapIt->second.setScoreEnd(true);
					}
				}
				else {
					// notify scores that this is the last bar of a loop
					auto instMapIt = sharedData->instruments.begin();
					if (!instMapIt->second.isLoopEnd()) {
						for (instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
							instMapIt->second.setLoopEnd(true);
						}
					}
				}
			}
			else {
				instMapRevIt = sharedData->instruments.rbegin();
				if (instMapRevIt->second.isLoopEnd()) {
					for (auto instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
						instMapIt->second.setLoopEnd(false);
					}
				}
			}
			// get the start of each beat
			int diff = (int)(timeStamp - tickCounter);
			int compareVal = sharedData->tempoMs[bar] * 1000; // in microseconds
			if (diff >= compareVal || firstIter) {
				if (finished) {
					std::vector<std::string> funcsToCall;
					{
						std::lock_guard<std::mutex> lock(sharedData->functionsMutex);
						for (auto func = sharedData->functions.begin(); func != sharedData->functions.end(); ++func) {
							// binding a function to the number of instruments + 2, binds to the end
							if (func->second.getBoundInst() == (int)sharedData->instruments.size() + 2 && sharedData->beatCounter == 0) {
								funcsToCall.push_back(func->second.getName());
							}
						}
					}
					for (const std::string& funcName : funcsToCall) {
						//((ofApp*)ofGetAppPtr())->parseCommand(((ofApp*)ofGetAppPtr())->genCmdInput(funcName), 1, 1);
						callBoundFunction(funcName);
					}
					finished = false;
					stopNow();
					return false;
				}
				if (firstIter) tickCounter = timeStamp;
				else tickCounter = timeStamp - (uint64_t)(diff - compareVal);
				// reset all instrument note durations at the start of each bar
				//for (auto it = sharedData->instruments.begin(); it != sharedData->instruments.end(); ++it) {
				//	it->second.resetNoteDur();
				//}
				if (endOfBar || firstIter) {
					// the bar that has just finished, so that ties and slurs at its end go on to the first notes of the next bar (see fireNote())
					previousBar = (firstIter ? -1 : bar);
					sharedData->thisBarIndex = thisBarIndex;
					bar = sharedData->loopData[sharedData->loopIndex][sharedData->thisBarIndex];
					// \mute, \unmute, and \solo apply at the start of the next bar (\mutenow, \unmutenow, and \solonow apply at once)
					// this was checked on every step of the first bar of the loop, which applied them at once in that bar
					checkMute();
					OscMessage m;
					m.setAddress("/thisloopndx");
					m.addIntArg(sharedData->thisBarIndex);
					sendToParts(m, true);
					m.clear();
					// sync the time stamp of each instrument to that of the sequencer when the latter starts
					if (firstIter) {
						for (auto it = sharedData->instruments.begin(); it != sharedData->instruments.end(); ++it) {
							// instruments with a delay have started already (see syncDelayedInstrument())
							if (it->second.getDelayTime() > 0) continue;
							it->second.setTimeStamp(timeStamp);
						}
					}
					if (updateSequencer && (sharedData->thisBarIndex == 0)) {
						// update the loop index first
						sharedData->loopIndex = sharedData->tempLoopIndex;
						// then update the bar index
						bar = sharedData->loopData[sharedData->loopIndex][sharedData->thisBarIndex];
						sendLoopIndexToParts();
						// zero noteDur for all instruments so the first step of a new loop fires immediately
						//for (auto it = sharedData->instruments.begin(); it != sharedData->instruments.end(); ++it) {
						//	it->second.zeroNoteDur();
						//}
						updateSequencer = false;
						sequencerUpdated = true;
						sharedData->drawLoopStartEnd = true;
						sharedData->barCounter = 0;
						// update the previous number of bars displayed and the previous position to properly display single bars in horizontal view
						sharedData->prevNumBars = sharedData->numBars;
						sharedData->prevPosition = sharedData->thisPosition;
						// reset the counter for the beat durations
						sharedData->beatAtValCounter = 0;
					}
					// instruments with a delay play ahead of the score, and must be in the bar the score plays after their delay
					// which they are not after calling a loop, or going to another bar of the loop, while they were already in their next bar
					// or when their delay has changed (see Instrument::setDelay())
					if (!countdown) {
						for (auto it = sharedData->instruments.begin(); it != sharedData->instruments.end(); ++it) {
							if (it->second.getDelayTime() > 0) {
								syncDelayedInstrument(it, sharedData->loopIndex, sharedData->thisBarIndex, (int64_t)tickCounter, (int64_t)timeStamp, true);
							}
						}
					}
					// after checking if we must update the indexes, increment them
					if (!countdown) {
						thisBarIndex++;
						if (thisBarIndex >= sharedData->loopData[sharedData->loopIndex].size()) {
							thisBarIndex = 0;
							// when we start a loop from its beginning, we must check if an instrument needs to be muted
							muteChecked = false;
						}
						// do the same for bars that are set to be checked
						if (checkLoop) {
							loopToCheckBarNdx++;
							if (loopToCheckBarNdx >= (int)sharedData->loopData[loopToCheckNdx].size()) {
								loopToCheckIterCounter++;
								if (loopToCheckIterCounter >= loopToCheckNumIter) {
									checkLoop = false;
								}
								loopToCheckBarNdx = 0;
							}
						}
						// increment the bar counter which is used for displaying the score only
						sharedData->barCounter++;
						endOfBar = false;
						if (firstIter) sendStopCountdownToParts();
					}
				}
				firstIter = false;
				if (countdown) {
					sendCountdownToParts(countdownCounter);
					countdownCounter--;
					sharedData->beatCounter = -1;
					std::vector<std::string> funcsToCall;
					{
						std::lock_guard<std::mutex> lock(sharedData->functionsMutex);
						for (auto func = sharedData->functions.begin(); func != sharedData->functions.end(); ++func) {
							// binding to the number of instruments, binds a function to the pulse of the beat
							if (func->second.getBoundInst() == (int)sharedData->instruments.size()) {
								funcsToCall.push_back(func->second.getName());
							}
						}
					}
					for (const std::string& funcName : funcsToCall) {
						//((ofApp*)ofGetAppPtr())->parseCommand(((ofApp*)ofGetAppPtr())->genCmdInput(funcName), 1, 1);
						callBoundFunction(funcName);
					}
					if (!(sharedData->beatCounter == 0 && mustStop)) sendBeatVizInfo(bar);
					endOfBar = true;
					if (countdownCounter == 0) {
						countdown = false;
						sendCountdownToParts(countdownCounter);
					}
				}
				else {
					sharedData->beatCounter = beatCounter;
					beatCounter++;
					if (beatCounter >= sharedData->numerator[bar]) {
						beatCounter = 0;
					}
					// the beat visualization pulses on the beats set with \beatat (or \tempo), which can be longer than the beats of the meter
					// e.g. "\beatat 4. 4" in 5/8 gives the values 3 and 2, so it pulses on the first and the fourth eighth of the bar
					// every bar starts with a pulse and with its first value, regardless of how the previous bar ended
					if (sharedData->beatCounter == 0) {
						sendBeatVizInfoCounter = 0;
						sharedData->beatAtValCounter = 0;
					}
					if (sendBeatVizInfoCounter == 0) {
						if (!(sharedData->beatCounter == 0 && mustStop)) sendBeatVizInfo(bar);
					}
					if (sharedData->beatAtDifferentThanDivisor[bar] && !sharedData->beatAtValues[bar].empty()) {
						if (sharedData->beatAtValCounter >= sharedData->beatAtValues[bar].size()) {
							sharedData->beatAtValCounter = 0;
						}
						// the number of meter beats until the next pulse
						int beatLength = sharedData->beatAtValues[bar][sharedData->beatAtValCounter];
						if (beatLength < 1) beatLength = 1;
						sendBeatVizInfoCounter++;
						// move to the next value only once the current one has been counted in full, and not on every meter beat
						if (sendBeatVizInfoCounter >= beatLength) {
							sendBeatVizInfoCounter = 0;
							sharedData->beatAtValCounter++;
							if (sharedData->beatAtValCounter >= sharedData->beatAtValues[bar].size()) {
								sharedData->beatAtValCounter = 0;
							}
						}
					}
					std::vector<std::string> funcsToCall;
					{
						std::lock_guard<std::mutex> lock(sharedData->functionsMutex);
						for (auto func = sharedData->functions.begin(); func != sharedData->functions.end(); ++func) {
							// binding a function to the number of instruments + 1, binds to the start of each bar
							if (func->second.getBoundInst() == (int)sharedData->instruments.size() + 1 && sharedData->beatCounter == 0) {
								funcsToCall.push_back(func->second.getName());
							}
							// binding to the number of instruments, binds a function to the pulse of the beat
							else if (func->second.getBoundInst() == (int)sharedData->instruments.size()) {
								funcsToCall.push_back(func->second.getName());
							}
							// binding to the number of instruments + 3, binds a function to the start of the loop
							else if (func->second.getBoundInst() == (int)sharedData->instruments.size() + 3 && \
									sharedData->thisBarIndex == 0 && sharedData->beatCounter == 0) {
								funcsToCall.push_back(func->second.getName());
							}
						}
					}
					for (const std::string& funcName : funcsToCall) {
						//((ofApp*)ofGetAppPtr())->parseCommand(((ofApp*)ofGetAppPtr())->genCmdInput(funcName), 1, 1);
						callBoundFunction(funcName);
					}
				}
				sharedData->PPQNCounter = 0;
				// check if we're at the beginning of a bar so we must reset the barDataCounter of each instrument
				if (sharedData->beatCounter == 0) {
					for (auto it = sharedData->instruments.begin(); it != sharedData->instruments.end(); ++it) {
						// instruments with a delay move on to their next bar by themselves (see playDelayedInstrument())
						if (it->second.getDelayTime() > 0) continue;
						it->second.resetBarDataCounter();
						// an instrument added while the sequencer plays starts here, at the start of a bar, with the other instruments
						if (!it->second.timeSynced) it->second.setTimeStamp(timeStamp);
					}
					// we must also reset the time stamp of the piano roll
					sharedData->pianoRollTimeStamp = Clock::millis();
					if (mustStop) {
						if (sharedData->animate) {
							for (auto it = sharedData->instruments.begin(); it != sharedData->instruments.end(); ++it) {
								it->second.setAnimation(false);
							}
							sharedData->animate = false;
							sharedData->setAnimation = true;
						}
						onNextStartMidiByte = 0xFA; // Start MIDI command
						mustStop = false;
						mustStopCalled = true;
						stopNow();
						return false;
					}
				}
			}
			if (!countdown) {
				// send MIDI clock, if set
				// need to calculate the number of steps
				uint64_t midiClockTimeStamp = Clock::micros();
				if (sendMidiClock && \
						(midiClockTimeStamp - sharedData->PPQNTimeStamp) >= sharedData->PPQNPerUs[bar] && \
						sharedData->PPQNCounter < sharedData->PPQN) {
					sendMidiByte(0xF8);
					sharedData->PPQNCounter++;
					sharedData->PPQNTimeStamp = midiClockTimeStamp;
				}
				for (auto instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
					// instruments with a delay are played below
					if (instMapIt->second.getDelayTime() > 0) continue;
					if (instMapIt->second.getBarDataCounter() == 0) {
						if (instMapIt->second.mustUpdateTempo()) {
							instMapIt->second.setUpdateTempo(false);
						}
					}
					runGlissando(instMapIt->first, bar); // this function determines whethere there are any active glissandi
					runDynamicsRamp(instMapIt->first); // the same for crescendi and diminuendi during notes
					fireNote(instMapIt, bar, timeStamp, false); // last argument is for loops that are being checked
					//if (checkLoop) {
					//	int barToCheck = sharedData->loopData[loopToCheckNdx][loopToCheckBarNdx];
					//	int prevBarToCheck = sharedData->loopData[loopToCheckNdx][((int)loopToCheckBarNdx - 1 < 0 ? (int)sharedData->loopData[loopToCheckNdx].size()-1 : loopToCheckBarNdx-1)];
					//	fireNote(instMapIt, barToCheck, prevBarToCheck, timeStamp, true);
					//}
				}
			}
			// check if we're at the end of the bar
			if (sharedData->beatCounter == sharedData->numerator[bar] - 1 && !endOfBar) {
				endOfBar = true;
				if (sharedData->thisBarIndex == sharedData->loopData[sharedData->loopIndex].size()-1 && finish) {
					// if we're finishing
					finish = false;
					finished = true;
					runSequencer = false;
					updateSequencer = false;
					sequencerUpdated = false;
					onNextStartMidiByte = 0xFA; // Start MIDI command
					//for (auto instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
					//	instMapIt->second.setAnimation(false);
					//}
					//sharedData->beatAnimate = false;
					//sharedData->animate = false;
					//sharedData->setAnimation = true;
				}
			}
			// update the boolean below so that it is true only for the very first iteration of the sequncer
			sequencerStart = false;
			// store a temporary variable with the current bar
			// this is used when sending note off messages on sequencer quit
			tempBar = bar;
			// the instruments with a delay, ahead of the score
			for (auto instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
				if (instMapIt->second.getDelayTime() > 0) playDelayedInstrument(instMapIt, (int64_t)Clock::micros());
			}
		}
	}
	return true;
}

//--------------------------------------------------------------
std::vector<std::string> Sequencer::dryRun()
{
	std::vector<std::string> log;
	if (isThreadRunning() || sharedData->instruments.empty()) return log;
	auto loopIt = sharedData->loopData.find(sharedData->loopIndex);
	if (loopIt == sharedData->loopData.end() || loopIt->second.empty()) return log;
	// how long it plays: the loop that plays twice, after the longest delay and the countdown, and at most one minute
	int64_t duration = 0;
	for (int bar : loopIt->second) duration += barDurUs(bar);
	duration *= 2;
	int64_t maxDelay = 0;
	for (auto& inst : sharedData->instruments) maxDelay = std::max(maxDelay, inst.second.getDelayTime());
	duration += maxDelay;
	if (countdown) duration += (int64_t)countdownCounter * sharedData->tempoMs[loopIt->second.front()] * 1000;
	duration = std::min(duration, (int64_t)60000000);
	Clock::simulate(true);
	dryRunLog = &log;
	dryRunStart = Clock::micros();
	// as start() does
	mustStop = false;
	runSequencer = true;
	sequencerRunning = false;
	// the steps of the thread, every 100 microseconds of the simulated clock (see setup())
	while ((int64_t)(Clock::micros() - dryRunStart) < duration) {
		if (!step()) break;
		Clock::advance(100);
	}
	// the sequencer stops, with its notes off, as when it is stopped
	if (sequencerRunning) stopNow();
	dryRunLog = nullptr;
	Clock::simulate(false);
	return log;
}

//--------------------------------------------------------------
bool Sequencer::logged(const std::string& message)
{
	if (!dryRunLog) return false;
	std::ostringstream line;
	line << std::fixed << std::setprecision(1) << std::setw(9) << (double)(Clock::micros() - dryRunStart) / 1000.0 << " ms  " << message;
	dryRunLog->push_back(line.str());
	return true;
}

//--------------------------------------------------------------
void Sequencer::sendOsc(OscMessage& m)
{
	if (logged("osc " + oscText(m))) return;
	oscSender->send(m);
}

//--------------------------------------------------------------
void Sequencer::execPyo(const std::string& code)
{
	if (logged("pyo " + code)) return;
#ifdef USEPYO
	sharedData->pyo.exec(code.c_str());
#endif
}

//--------------------------------------------------------------
void Sequencer::sendMidiByte(int byte)
{
	std::ostringstream hex;
	hex << "midi byte 0x" << std::uppercase << std::hex << byte;
	if (logged(hex.str())) return;
	std::lock_guard<std::mutex> lock(genericMidiPortsMutex);
	for (int port : genericMidiPorts) midi->sendByte(port, (unsigned char)byte);
}

//--------------------------------------------------------------
void Sequencer::addGenericMidiPort(int port)
{
	std::lock_guard<std::mutex> lock(genericMidiPortsMutex);
	genericMidiPorts.insert(port);
}

//--------------------------------------------------------------
bool Sequencer::isGenericMidiPort(int port)
{
	std::lock_guard<std::mutex> lock(genericMidiPortsMutex);
	return genericMidiPorts.count(port) > 0;
}

//--------------------------------------------------------------
bool Sequencer::hasGenericMidiPorts()
{
	std::lock_guard<std::mutex> lock(genericMidiPortsMutex);
	return !genericMidiPorts.empty();
}

//--------------------------------------------------------------
void Sequencer::callBoundFunction(const std::string& funcName)
{
	// in a dry run the function is called too, as what it does (e.g. changing the loop) is part of the playback
	logged("function " + funcName);
	((ofApp*)ofGetAppPtr())->callBoundFunction(funcName);
}

//--------------------------------------------------------------
std::string Sequencer::oscText(OscMessage& m)
{
	std::ostringstream text;
	text << m.getAddress() << std::fixed << std::setprecision(4);
	for (size_t i = 0; i < m.getNumArgs(); i++) {
		switch (m.getArgType(i)) {
			case OscArgType::Int32: text << " " << m.getArgAsInt32(i); break;
			case OscArgType::Int64: text << " " << m.getArgAsInt64(i); break;
			case OscArgType::Float: text << " " << m.getArgAsFloat(i); break;
			case OscArgType::Double: text << " " << m.getArgAsDouble(i); break;
			case OscArgType::String: text << " \"" << m.getArgAsString(i) << "\""; break;
			case OscArgType::True: text << " true"; break;
			case OscArgType::False: text << " false"; break;
			default: text << " ?"; break;
		}
	}
	return text.str();
}

//--------------------------------------------------------------
std::string Sequencer::midiText(Instrument& inst, const std::string& kind, int channel, int a, int b)
{
	std::string text = "midi " + inst.getName() + (inst.getSoundFontChannel() >= 0 ? " soundfont " : " ") + kind + " ch " + std::to_string(channel) + " " + std::to_string(a);
	if (b >= 0) text += " " + std::to_string(b);
	return text;
}
