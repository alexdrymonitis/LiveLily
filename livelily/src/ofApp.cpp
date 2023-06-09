#include "ofApp.h"
#include <string.h>
#include <sstream>
#include <ctype.h> // to add isdigit()
#include <algorithm> // for replacing characters in a string

/******************* Sequencer class **************************/
//--------------------------------------------------------------
void Sequencer::setup(SequencerVectors *sVec){
	seqVec = sVec;
	sequencerRunning = false;
	finish = false;
	finished = false;
	oscSender.setup(HOST, OFSENDPORT);
	timer.setPeriodicEvent(1000000); // 1 ms
}

//--------------------------------------------------------------
int Sequencer::setPatternIndex(bool increment){
	int patternIndexLocal = 0;
	if (increment) {
		seqVec->thisPatternLoopIndex++;
		if (seqVec->thisPatternLoopIndex >= seqVec->patternLoopIndexes[seqVec->patternLoopIndex].size()) {
			seqVec->thisPatternLoopIndex = 0;
		}
	}
	patternIndexLocal = seqVec->patternLoopIndexes[seqVec->patternLoopIndex][seqVec->thisPatternLoopIndex];
	return patternIndexLocal;
}

//--------------------------------------------------------------
void Sequencer::checkMute(){
	// check if an instrument needs to be muted at the beginning
	if (toMute.size() > 0) {
		for (unsigned i = 0; i < toMute.size(); i++) {
			seqVec->notesVec[toMute[i]].mute = true;
		}
		toMute.clear();
	}
	// and check if an instrument needs to be unmuted
	if (toUnmute.size() > 0) {
		for (unsigned i = 0; i < toUnmute.size(); i++) {
			seqVec->notesVec[toUnmute[i]].mute = false;
		}
		toUnmute.clear();
	}
}

//--------------------------------------------------------------
void Sequencer::sendAllNotesOff(){
	for (unsigned i = 0; i < seqVec->instruments.size(); i++) {
		if (seqVec->isMidi[i]) {
			for (unsigned j = 0; j < notesOn[i].size(); j++) {
				midiOut.sendNoteOff(seqVec->midiChans[i], notesOn[i][j], 0);
			}
			notesOn[i].clear();
		}
	}
}

//--------------------------------------------------------------
void Sequencer::threadedFunction(){
	while (isThreadRunning()) {
		timer.waitNext();
		if (runSequencer && !sequencerRunning) {
			int patternIndexLocal = setPatternIndex(false);
			int bar = seqVec->patternData[patternIndexLocal][0];
			seqVec->tempo = seqVec->newTempo;
			for (unsigned i = 2; i < seqVec->patternData[patternIndexLocal].size(); i++) {
				int inst = seqVec->patternData[patternIndexLocal][i];
				// initialize all beat counter to the first number of beats in durs
				// so that every line kicks off immediately
				int noteDur;
				if (seqVec->isMidi[inst]) noteDur = seqVec->midiDursWithoutSlurs[inst][bar][seqVec->barDataCounters[inst]];
				else noteDur = seqVec->dursWithoutSlurs[inst][bar][seqVec->barDataCounters[inst]];
				seqVec->beatCounters[inst] = noteDur * seqVec->tempo;
			}
			sequencerRunning = true;
			runSequencer = false;
			// in here we calculate the number of beats on a millisecond basis
			numBeats = seqVec->patternData[patternIndexLocal][1] * seqVec->tempo;
			if (seqVec->showScore) {
				if (seqVec->setAnimation) {
					for (unsigned i = 0; i < seqVec->notesVec.size(); i++) {
						seqVec->notesVec[i].setAnimation(true);
					}
					seqVec->animate = true;
					seqVec->setAnimation = false;
				}
				if (seqVec->setBeatAnimation) {
					// seqVec->beatVizStepsPerMs = (float)BEATVIZBRIGHTNESS / ((float)seqVec->tempoMs / 4.0);
					// seqVec->beatVizRampStart = seqVec->tempoMs / 4;
					seqVec->beatViz = true;
					seqVec->beatAnimate = true;
					seqVec->setBeatAnimation = false;
				}
			}
			// calculate the following two in case we connect to a score part
			seqVec->beatVizStepsPerMs = (float)BEATVIZBRIGHTNESS / ((float)seqVec->tempoMs / 4.0);
			seqVec->beatVizRampStart = seqVec->tempoMs / 4;
			ofxOscMessage m;
			m.setAddress("/beatinfo");
			m.addFloatArg(seqVec->beatVizStepsPerMs);
			m.addInt64Arg(seqVec->beatVizRampStart);
			for (unsigned i = 0; i < seqVec->sendsToPart.size(); i++) {
				if (seqVec->sendsToPart[i]) {
					seqVec->scorePartSenders[i].sendMessage(m, false);
				}
			}
		}

		if (sequencerRunning) {
			int patternIndexLocal = setPatternIndex(false);
			int bar = seqVec->patternData[patternIndexLocal][0];
			// check if we're at the beginning of the loop
			if (seqVec->thisPatternLoopIndex == 0) {
				// if we have finished, we just silence the instruments
				if (finished) {
					for (unsigned i = 2; i < seqVec->patternData[patternIndexLocal].size(); i++) {
						int inst = seqVec->patternData[patternIndexLocal][i];
						ofxOscMessage m;
						m.setAddress("/" + seqVec->instruments[inst] + "/dynamics");
						m.addIntArg(0);
						m.addIntArg(0);
						oscSender.sendMessage(m, false);
						m.clear();
					}
					finished = false;
					sequencerRunning = false;
					stopThread();
					return;
				}
				else {
					// notify scores that this is the first bar of a loop
					// if the first staff has been notified, then all staffs will
					// so we don't bother
					if (!seqVec->staffs[0].isLoopStart) {
						for (unsigned i = 0; i < seqVec->notesVec.size(); i++) {
							seqVec->staffs[i].isLoopStart = true;
						}
					}
				}
			}
			else {
				// if the last staff has its isLoopStart boolean to true
				// set all staffs booleans to false
				if (seqVec->staffs.back().isLoopStart) {
					for (unsigned i = 0; i < seqVec->notesVec.size(); i++) {
						seqVec->staffs[i].isLoopStart = false;
					}
				}
			}
			// check if we're at the end of the loop
			if (seqVec->thisPatternLoopIndex == seqVec->patternLoopIndexes[seqVec->patternLoopIndex].size()-1) {
				// first check if we're finishing
				if (finish) {
					// falsify both isLoopStart and isLoopEnd for all staffs first
					// and notify all staffs of the finale
					for (unsigned i = 0; i < seqVec->notesVec.size(); i++) {
						seqVec->staffs[i].isLoopStart = false;
						seqVec->staffs[i].isLoopEnd = false;
						seqVec->staffs[i].isScoreEnd = true;
					}					
				}
				else {
					// notify scores that this is the last bar of a loop
					if (!seqVec->staffs[0].isLoopEnd) {
						for (unsigned i = 0; i < seqVec->notesVec.size(); i++) {
							seqVec->staffs[i].isLoopEnd = true;
						}
					}
				}
			}
			else {
				if (seqVec->staffs.back().isLoopEnd) {
					for (unsigned i = 0; i < seqVec->notesVec.size(); i++) {
						seqVec->staffs[i].isLoopEnd = false;
					}
				}
			}
			seqVec->stepDone = false;
			for (unsigned i = 2; i < seqVec->patternData[patternIndexLocal].size(); i++) {
				int inst = seqVec->patternData[patternIndexLocal][i];
				// the first time in each iteration we must fire based on the
				// first index of the bar data counter
				// but all other occurances must fire based on the previous index
				// e.g. for a sequence of 2 2 1 1 2 the first time we fire immediately
				// and the second time we wait for the first number of beats to conclude
				// (the first 2 in the sequence) and so on till the end of the sequence
				int counterLocal = 0;
				if (seqVec->barDataCounters[inst] > 0) {
					counterLocal = seqVec->barDataCounters[inst] - 1;
				}
				else {
					if (updateTempo) {
						// re-initialize the beat counters so there is no stalling after a tempo change
						if (!seqVec->isMidi[inst]) {
							seqVec->beatCounters[inst] = seqVec->dursWithoutSlurs[inst][bar][seqVec->barDataCounters[inst]] * seqVec->tempo;
						}
						else {
							seqVec->beatCounters[inst] = seqVec->midiDursWithoutSlurs[inst][bar][seqVec->barDataCounters[inst]] * seqVec->tempo;
						}
						updateTempo = false;
					}
					// update the score at the beginning of every bar
					if (seqVec->showScore) {
						seqVec->currentScoreIndex = patternIndexLocal;
						if (seqVec->updatePatternLoop && (seqVec->thisPatternLoopIndex == 0)) {
							seqVec->updatePatternLoop = false;
						}
						// if (seqVec->beatAnimate) {
						seqVec->beatVizStepsPerMs = (float)BEATVIZBRIGHTNESS / ((float)seqVec->tempoMs / 4.0);
						seqVec->beatVizRampStart = seqVec->tempoMs / 4;
						// }
						// notify all score parts
						ofxOscMessage m;
						m.setAddress("/beatinfo");
						m.addFloatArg(seqVec->beatVizStepsPerMs);
						m.addInt64Arg(seqVec->beatVizRampStart);
						for (unsigned i = 0; i < seqVec->sendsToPart.size(); i++) {
							if (seqVec->sendsToPart[i]) {
								seqVec->scorePartSenders[i].sendMessage(m, false);
							}
						}
					}
					if (seqVec->thisPatternLoopIndex == 0) {
						// at the beginning of every loop, we check if instruments need to be muted
						checkMute();
					}
				}
				if (!seqVec->updatingSharedMem) {
					if (seqVec->notes[inst][bar].size() > 0) {
						// since MIDI data has double the notes, durs, and all
						// so it can send a noteOff message without sending a duration
						// we have to specify certain variables to not get a crash
						// due to indexing beyond the bounds of a vector
						int noteDur;
						if (seqVec->isMidi[inst]) noteDur = seqVec->midiDursWithoutSlurs[inst][bar][counterLocal];
						else noteDur = seqVec->dursWithoutSlurs[inst][bar][counterLocal];
						bool hasNote = true;
						if (seqVec->isMidi[inst]) {
							if (seqVec->midiNotes[inst][bar][counterLocal][0] >= 0) {
								hasNote = true;
							}
							else hasNote = false;
						}
						else {
							if (seqVec->notes[inst][bar][counterLocal][0] >= 0) {
								hasNote = true;
							}
							else hasNote = false;
						}
						if (seqVec->beatCounters[inst] == (int)((float)noteDur * seqVec->tempo)) {
							// if we have a note, not a rest, and the current instrument is not muted
							if (hasNote && !seqVec->notesVec[inst].mute) {
								if (!seqVec->isMidi[inst]) {
									// send OSC message
									ofxOscMessage m;
									m.setAddress("/" + seqVec->instruments[inst] + "/dynamics");
									// if we're at the first beat we have to send the first dynamic without a ramp
									// because in case a ramp starts at the beginning, if we don't send this
									// the respective instrument will ramp from its last value (the last beat)
									// to the next value, but it must ramp from its current value to the next
									if (globalBeatCount == 0) {
										m.addIntArg(seqVec->dynamics[inst][bar][0]);
										m.addIntArg(0);
										oscSender.sendMessage(m, false);
										m.clear();
										m.setAddress("/" + seqVec->instruments[inst] + "/dynamics");
									}
									if (seqVec->dynamicsRamps[inst][bar][seqVec->barDataCounters[inst]] == 0) {
										m.addIntArg(seqVec->dynamics[inst][bar][seqVec->barDataCounters[inst]]);
										m.addIntArg(0);
									}
									else {
										m.addIntArg(seqVec->dynamicsRamps[inst][bar][seqVec->barDataCounters[inst]]);
										m.addIntArg(seqVec->dursWithoutSlurs[inst][bar][seqVec->barDataCounters[inst]] * seqVec->tempo);
									}
									// if we want to finish send an additional 0 amplitude to silence the instrument
									if ((seqVec->barDataCounters[inst] == 0) && finish) {
										m.addIntArg(0);
										m.addIntArg(0);
									}
									oscSender.sendMessage(m, false);
									m.clear();
									m.setAddress("/" + seqVec->instruments[inst] + "/articulation");
									m.addStringArg(seqVec->articulSyms[seqVec->articulations[inst][bar][seqVec->barDataCounters[inst]]]);
									oscSender.sendMessage(m, false);
									m.clear();
									if (seqVec->textIndexes[inst][bar][seqVec->barDataCounters[inst]] != 0) {
										m.setAddress("/" + seqVec->instruments[inst] + "/text");
										m.addStringArg(seqVec->text[inst][bar][seqVec->barDataCounters[inst]]);
										oscSender.sendMessage(m, false);
										m.clear();
									}
									m.setAddress("/" + seqVec->instruments[inst] + "/note");
									for (unsigned j = 0; j < seqVec->notes[inst][bar][seqVec->barDataCounters[inst]].size(); j++) {
										m.addFloatArg(seqVec->notes[inst][bar][seqVec->barDataCounters[inst]][j]);
									}
									m.addIntArg(0);
									oscSender.sendMessage(m, false);
									m.clear();
									// if we have a glissando send the next note and the duration
									// so we can first go to the current note and then ramp to the next
									if (seqVec->glissandi[inst][bar][seqVec->barDataCounters[inst]] == 1) {
										// safety test to make sure there is one more datum in the notes
										if ((int)seqVec->notes[inst][bar].size() >= (seqVec->barDataCounters[inst]+1)) {
											m.setAddress("/" + seqVec->instruments[inst] + "/note");
											for (unsigned j = 0; j < seqVec->notes[inst][bar][seqVec->barDataCounters[inst]+1].size(); j++) {
												m.addFloatArg(seqVec->notes[inst][bar][seqVec->barDataCounters[inst]+1][j]);
											}
											m.addIntArg(seqVec->dursWithoutSlurs[inst][bar][seqVec->barDataCounters[inst]] * seqVec->tempo);
											oscSender.sendMessage(m, false);
											m.clear();
										}
									}
									if (seqVec->durs[inst][bar][seqVec->barDataCounters[inst]] > 0) {
										m.setAddress("/" + seqVec->instruments[inst] + "/duration");
										m.addIntArg(seqVec->durs[inst][bar][seqVec->barDataCounters[inst]]*seqVec->tempo);
										oscSender.sendMessage(m, false);
										m.clear();
									}
								}
								// sending MIDI
								else {
									// in MIDI we have two beats per note for noteOn and noteOff
									// but we also need a count for noteOn messages only
									if (seqVec->barDataCounters[inst] > 0 && (seqVec->barDataCounters[inst] % 2) == 0) {
										midiSequencerCounters[inst]++;
									}
									else if (seqVec->barDataCounters[inst] == 0) midiSequencerCounters[inst] = 0;
									// separate noteOn from noteOff messages
									if (seqVec->midiVels[inst][bar][seqVec->barDataCounters[inst]] > 0) {
										for (unsigned j = 0; j < seqVec->midiNotes[inst][bar][seqVec->barDataCounters[inst]].size(); j++) {
											midiOut.sendNoteOn(seqVec->midiChans[inst],
															   seqVec->midiNotes[inst][bar][seqVec->barDataCounters[inst]][j],
															   seqVec->midiVels[inst][bar][seqVec->barDataCounters[inst]]);
											notesOn[inst].push_back(seqVec->midiNotes[inst][bar][seqVec->barDataCounters[inst]][j]);
										}
										// midiOut.sendPitchBend(seqVec->midiChans[inst], seqVec->pitchBendVals[inst][bar][midiSequencerCounter]);
										// midiOut.sendProgramChange(seqVec->midiChans[inst], seqVec->midiArticulationVals[inst][bar][midiSequencerCounter]);
										// // if we have a dynamics ramp, send the ramp time in beat steps and the next velocity value
										// if (seqVec->midiDynamicsRampDurs[inst][bar][midiSequencerCounter] > 0) {
										// 	midiOut.sendControlChange(seqVec->midiChans[inst], 1, seqVec->midiDynamicsRampDurs[inst][bar][midiSequencerCounter]);
										// 	midiOut.sendControlChange(seqVec->midiChans[inst], 2, seqVec->midiVels[inst][bar][seqVec->barDataCounters[inst]+1]);
										// }
										// // if we have a glissando, send the glissando time in beat steps and all the next notes
										// if (seqVec->midiGlissDurs[inst][bar][midiSequencerCounter] > 0) {
										// 	midiOut.sendControlChange(seqVec->midiChans[inst], 3, seqVec->midiGlissDurs[inst][bar][midiSequencerCounter]);
										// 	for (unsigned j = 0; j < seqVec->midiNotes[inst][bar][seqVec->barDataCounters[inst]+1].size(); j++) {
										// 		midiOut.sendControlChange(seqVec->midiChans[inst], 4, seqVec->midiNotes[inst][bar][seqVec->barDataCounters[inst]+1][j]);
										// 	}
											
										// }
									}
									else {
										for (unsigned j = 0; j < seqVec->midiNotes[inst][bar][seqVec->barDataCounters[inst]].size(); j++) {
											midiOut.sendNoteOff(seqVec->midiChans[inst],
																seqVec->midiNotes[inst][bar][seqVec->barDataCounters[inst]][j],
																seqVec->midiVels[inst][bar][seqVec->barDataCounters[inst]]);
											notesOn[inst].erase(std::remove(notesOn[inst].begin(), notesOn[inst].end(), j), notesOn[inst].end());
										}
									}
								}
							}
							else {
								if (!seqVec->isMidi[inst]) {
									// in case of a rest, send a 0 dynamic to make sure the instrument will stop
									ofxOscMessage m;
									m.setAddress("/" + seqVec->instruments[inst] + "/dynamics");
									m.addIntArg(0);
									m.addIntArg(0);
									oscSender.sendMessage(m, false);
									m.clear();
								}
								else {
									midiOut.sendNoteOff(seqVec->midiChans[inst], 0, 0);
								}
							}
							if (seqVec->animate) {
								if (seqVec->isMidi[inst]) {
									seqVec->notesVec[inst].setActiveNote(midiSequencerCounters[inst]);
								}
								else {
									seqVec->notesVec[inst].setActiveNote(seqVec->barDataCounters[inst]);
								}
							}
							// reset the beat counter
							seqVec->beatCounters[inst] = 0;
							// increment the data counter so next time we point to the next datum
							seqVec->barDataCounters[inst]++;
						}
					}
				}
				seqVec->beatCounters[inst]++;
			}
			seqVec->stepDone = true;
			globalBeatCount++;
			// notify the global beat visualization
			if ((globalBeatCount % (numBeats / seqVec->numerator)) == 0) {
				// calculate the time stamp regardless of whether we're showing the beat
				// or not in the editor, in case we have connected score parts
				seqVec->beatVizTimeStamp = ofGetElapsedTimeMillis();
				seqVec->beatVizCounter++;
				if (seqVec->beatAnimate) {
					seqVec->beatViz = true;
					// seqVec->beatVizTimeStamp = ofGetElapsedTimeMillis();
					// seqVec->beatVizCounter++;
				}
				// notify all score parts of the time stamp and the beat visualization counter
				ofxOscMessage m;
				m.setAddress("/beat");
				m.addInt64Arg(seqVec->beatVizTimeStamp);
				m.addIntArg(seqVec->beatVizCounter);
				for (unsigned i = 0; i < seqVec->sendsToPart.size(); i++) {
					if (seqVec->sendsToPart[i]) {
						seqVec->scorePartSenders[i].sendMessage(m, false);
					}
				}
			}
			// check if we're at the end of the bar
			if (globalBeatCount >= numBeats) {
				if (seqVec->thisPatternLoopIndex == seqVec->patternLoopIndexes[seqVec->patternLoopIndex].size()-1 && finish) {
					// if we're finishing
					finish = false;
					finished = true;
					runSequencer = false;
					for (unsigned i = 0; i < seqVec->notesVec.size(); i++) {
						seqVec->notesVec[i].setAnimation(false);
					}
					seqVec->beatAnimate = false;
					seqVec->animate = false;
					seqVec->setAnimation = true;
				}
				else {
					int patternIndexLocal = setPatternIndex(true);
					int bar = seqVec->patternData[patternIndexLocal][0];
					for (unsigned i = 2; i < seqVec->patternData[patternIndexLocal].size(); i++) {
						int inst = seqVec->patternData[patternIndexLocal][i];
						seqVec->barDataCounters[inst] = 0;
						// again set the beat counter to the value of the first duration
						// so it fires immediately at the next iteration of the bar
						int noteDur;
						if (seqVec->isMidi[inst]) noteDur = seqVec->midiDursWithoutSlurs[inst][bar][seqVec->barDataCounters[inst]];
						else noteDur = seqVec->dursWithoutSlurs[inst][bar][seqVec->barDataCounters[inst]];
						seqVec->beatCounters[inst] = noteDur * seqVec->tempo;
					}
					globalBeatCount = 0;
					seqVec->beatVizCounter = 0;
					// if a command is sent while the sequencer is running
					// we must wait until a full bar or loop finishes before we insert
					// the new information
					if (runSequencer && seqVec->updatePatternLoop && (seqVec->thisPatternLoopIndex == 0)) {
						seqVec->patternLoopIndex = seqVec->tempPatternLoopIndex;
						// update the pattern data
						int patternIndexLocal = setPatternIndex(false);
						int bar = seqVec->patternData[patternIndexLocal][0];
						for (unsigned i = 2; i < seqVec->patternData[patternIndexLocal].size(); i++) {
							int inst = seqVec->patternData[patternIndexLocal][i];
							// again set the beat counter to the value of the first duration
							// so it fires immediately at the next iteration of the bar
							int noteDur;
							if (seqVec->isMidi[inst]) noteDur = seqVec->midiDursWithoutSlurs[inst][bar][seqVec->barDataCounters[inst]];
							else noteDur = seqVec->dursWithoutSlurs[inst][bar][seqVec->barDataCounters[inst]];
							seqVec->beatCounters[inst] = noteDur * seqVec->tempo;
						}
						// notify the score parts
						ofxOscMessage m;
						m.setAddress("/loopndx");
						m.addIntArg(seqVec->patternLoopIndex);
						m.addIntArg(seqVec->currentScoreIndex);
						for (unsigned i = 0; i < seqVec->sendsToPart.size(); i++) {
							if (seqVec->sendsToPart[i]) {
								seqVec->scorePartSenders[i].sendMessage(m, false);
							}
						}
						runSequencer = false;
					}
					if (updateTempo) {
						seqVec->tempo = seqVec->newTempo;
						numBeats = seqVec->patternData[patternIndexLocal][1] * seqVec->tempo;
					}
				}
			}
		}
	}
}

/******************* PosCalculator class **********************/
//--------------------------------------------------------------
void PosCalculator::setup(SequencerVectors *sVec){
	seqVec = sVec;
	sequencerRunning = false;
}

//--------------------------------------------------------------
void PosCalculator::setLoopIndex(int loopIdx){
	loopIndex = loopIdx;
	correctAllStaffLoopIndex = loopIdx;
}

//--------------------------------------------------------------
void PosCalculator::calculateAllStaffPositions(){
	// no need to calculate any coordinates for one instrument only
	if (seqVec->activeInstruments.size() == 1) {
		return;
	}
	float maxHeightSum = 0;
	float minVsAnchor = 0;
	vector<float> maxPositionsVec;
	vector<float> minPositionsVec;
	vector<float> minVsAnchorVec;
	int prevBar = -1;
	bool mustCorrectAllStaffs = false;
	bool mustCopyPos = false;
	for (unsigned i = 0; i < seqVec->patternLoopIndexes[loopIndex].size(); i++) {
		int bar = seqVec->patternData[seqVec->patternLoopIndexes[loopIndex][i]][0];
		// don't run the loop for the same bar more than once
		if (bar == prevBar) {
			prevBar = bar;
			continue;
		}
		prevBar = bar;
		// initialize the coordinates of all seqVec.notes first and then caclulate
		for (unsigned j = 2; j < seqVec->patternData[seqVec->patternLoopIndexes[loopIndex][i]].size(); j++) {
			int inst = seqVec->patternData[seqVec->patternLoopIndexes[loopIndex][i]][j];
			maxPositionsVec.push_back(seqVec->notesVec[inst].getMaxYPos(bar));
			minPositionsVec.push_back(seqVec->notesVec[inst].getMinYPos(bar));
			minVsAnchorVec.push_back(seqVec->notesVec[inst].getMinVsAnchor(loopIndex, bar));
			float diff = maxPositionsVec.back() - minPositionsVec.back();
			if (diff > maxHeightSum) {
				maxHeightSum = diff;
				seqVec->maxStaffScorePos[loopIndex] = maxPositionsVec.back();
				seqVec->minStaffScorePos[loopIndex] = minPositionsVec.back();
				minVsAnchor = minVsAnchorVec.back();
			}
		}
	}
	if (maxHeightSum >= seqVec->allStaffDiffs) {
		for (unsigned i = 0; i < seqVec->maxStaffScorePos.size(); i++) {
			seqVec->maxStaffScorePos[i] = seqVec->maxStaffScorePos[loopIndex];
			seqVec->minStaffScorePos[i] = seqVec->minStaffScorePos[loopIndex];
			mustCorrectAllStaffs = true;
		}
		seqVec->allStaffDiffs = maxHeightSum;
	}
	else {
		// if there is a greater difference in some other pattern
		// just copy the difference from the first pattern with !loopIndex
		seqVec->maxStaffScorePos[loopIndex] = seqVec->maxStaffScorePos[!loopIndex];
		seqVec->minStaffScorePos[loopIndex] = seqVec->minStaffScorePos[!loopIndex];
		// this though might occur even if we have only one pattern
		// so we must make sure we have more than one patterns
		if (seqVec->patternFirstStaffAnchor.size() > 1) {
			seqVec->patternFirstStaffAnchor[loopIndex] = seqVec->patternFirstStaffAnchor[!loopIndex];
			mustCopyPos = true;
		}
	}
	float firstStaff;
	if (!mustCopyPos) {
		float totalDiff = maxPositionsVec.back() - minPositionsVec[0];
		// if all staffs fit in the screen
		if (totalDiff < seqVec->tracebackYCoord) {
			//firstStaff = seqVec->middleOfScreenY - (totalDiff / 2.0);
			int numInsts = seqVec->patternData[seqVec->patternLoopIndexes[loopIndex][0]].size() - 2;
			firstStaff = (seqVec->tracebackYCoord/2) - (((seqVec->allStaffDiffs * numInsts) + 5) / 2.0);
			// make sure anything that sticks out of the staff doesn't go below 0 in the Y axis
			if ((firstStaff - minVsAnchor) < 0) {
				firstStaff += minVsAnchor;
			}
		}
		else {
			// if the full score exceeds the screen just place it 10 pixels below the top part of the window
			firstStaff = minVsAnchor + 10;
		}
		//seqVec->patternFirstStaffAnchor[loopIndex] = firstStaff;
		for (unsigned i = 0; i < seqVec->patternFirstStaffAnchor.size(); i++) {
			seqVec->patternFirstStaffAnchor[i] = firstStaff;
		}
		if (firstStaff > seqVec->maxPatternFirstStaffAnchor) {
			// for (unsigned i = 0; i < seqVec->patternFirstStaffAnchor.size(); i++) {
			// 	seqVec->patternFirstStaffAnchor[i] = firstStaff;
			// }
			seqVec->maxPatternFirstStaffAnchor = firstStaff;
			mustCorrectAllStaffs = true;
		}
	}
	if (mustCorrectAllStaffs) {
		for (unsigned i = 0; i < seqVec->patternLoopIndexes.size(); i++) {
			if ((int)i == loopIndex) {
				// the current loopIndex will be corrected anyway, so we can omit it here
				continue;
			}
			correctAllStaffLoopIndex = i;
			correctAllStaffPositions();
		}
		correctAllStaffLoopIndex = loopIndex;
	}
}

//--------------------------------------------------------------
void PosCalculator::correctAllStaffPositions(){
	if (seqVec->activeInstruments.size() == 1) {
		seqVec->patternFirstStaffAnchor[correctAllStaffLoopIndex] = (seqVec->tracebackYCoord/2) - (seqVec->staffLinesDist * 2);
	}
	float xPos = seqVec->staffXOffset+seqVec->longestInstNameWidth+(seqVec->blankSpace*1.5);
	float xEdge = seqVec->staffWidth-seqVec->blankSpace-seqVec->staffLinesDist;
	int prevBar = -1; // there are no negative bars so init with -1
	for (unsigned i = 0; i < seqVec->patternLoopIndexes[correctAllStaffLoopIndex].size(); i++) {
		int bar = seqVec->patternData[seqVec->patternLoopIndexes[correctAllStaffLoopIndex][i]][0];
		int numBeatsLocal = seqVec->patternData[seqVec->patternLoopIndexes[correctAllStaffLoopIndex][i]][1];
		// don't run the loop for the same bar more than once
		if (bar == prevBar) {
			prevBar = bar;
			continue;
		}
		prevBar = bar;
		// the first element of each seqVec.patternData slot is the bar correctAllStaffLoopIndex
		// so we start the loop below from 1 instead of 0
		for (unsigned j = 2; j < seqVec->patternData[seqVec->patternLoopIndexes[correctAllStaffLoopIndex][i]].size(); j++) {
			int inst = seqVec->patternData[seqVec->patternLoopIndexes[correctAllStaffLoopIndex][i]][j];
			float yAnchor = seqVec->patternFirstStaffAnchor[correctAllStaffLoopIndex];
			// since the parent loop starts at 2 we must start at 2 below as well
			for (unsigned k = 2; k < j; k++) {
				yAnchor += (seqVec->allStaffDiffs + (seqVec->staffLinesDist * 2));
			}
			seqVec->scoreYAnchors[inst][correctAllStaffLoopIndex] = yAnchor;
			seqVec->notesXStartPnt = 0;
			seqVec->staffs[inst].setCoords(correctAllStaffLoopIndex, xPos, yAnchor, xEdge,
										   seqVec->staffLinesDist, seqVec->numerator, seqVec->denominator);
			float xStartPnt = seqVec->staffs[inst].getXPos();
			if (xStartPnt > seqVec->notesXStartPnt) seqVec->notesXStartPnt = xStartPnt;
			// give the appropriate offset so that notes are not too close to the loop symbols
			float xStart = seqVec->notesXStartPnt + seqVec->staffLinesDist;
			seqVec->notesVec[inst].setCoords(bar, correctAllStaffLoopIndex, xStart, yAnchor, xEdge,
											 seqVec->staffLinesDist, seqVec->scoreFontSize, numBeatsLocal);
			seqVec->notesVec[inst].setNotePositions(bar, correctAllStaffLoopIndex);
		}
	}
	// get the distance between whole beats (e.g. quarter seqVec.notes for 4/4)
	// so we can display the 2nd version of beat highlighting properly
	seqVec->distBetweenBeats[correctAllStaffLoopIndex] = ((seqVec->staffWidth-seqVec->blankSpace) - seqVec->notesXStartPnt) / seqVec->numBeats;
	seqVec->distBetweenBeats[correctAllStaffLoopIndex] *= (seqVec->numBeats / seqVec->denominator);
	if (seqVec->activeInstruments.size() > 0) {
		seqVec->noteWidth = seqVec->notesVec[seqVec->activeInstruments[0]].getNoteWidth();
		seqVec->noteHeight = seqVec->notesVec[seqVec->activeInstruments[0]].getNoteHeight();
	}
}

//--------------------------------------------------------------
void PosCalculator::threadedFunction(){
	if (multipleBarsPatternIndexes.size() > 0) {
		for (unsigned i = 0; i < multipleBarsPatternIndexes.size(); i++) {
			setLoopIndex(multipleBarsPatternIndexes[i]);
			calculateAllStaffPositions();
			correctAllStaffPositions();
		}
		// when done, clear because this runs in a different thread
		// and the main thread doesn't know when it is actually done
		multipleBarsPatternIndexes.clear();
	}
	else {
		calculateAllStaffPositions();
		correctAllStaffPositions();
	}
}

/********************* Main OF class **************************/
//--------------------------------------------------------------
void ofApp::setup(){
	ofBackground(BACKGROUND);

	unsigned int framerate = 60;

	sequencer.setup(&seqVec);
	posCalculator.setup(&seqVec);

	brightness = 220;
	backGround = BACKGROUND;
	notesID = 0;
	barIndexGlobal = 0;
	parsedMelody = false;
	parsingCommand = false;
	parsingLoopPattern = false;
	storingFunction = false;
	lastFunctionIndex = 0;
	fullScreen = false;
	isWindowResized = false;
	windowResizeTimeStamp = 0;

	ofSetFrameRate(framerate);
	ofSetLineWidth(lineWidth);

	ofSetWindowTitle("LiveLily");

	oscReceiver.setup(OSCPORT);

	midiPortOpen = false;

	seqVec.screenWidth = ofGetWindowWidth();
	seqVec.screenHeight = ofGetWindowHeight();
	seqVec.middleOfScreenX = seqVec.screenWidth / 2;
	seqVec.middleOfScreenY = seqVec.screenHeight / 2;
	seqVec.staffXOffset = seqVec.middleOfScreenX;
	seqVec.staffWidth = seqVec.screenWidth;

	// mutex simulation
	seqVec.stepDone = false;
	seqVec.updatingSharedMem = false;

	shiftPressed = false;
	ctlPressed = false;
	altPressed = false;

	typedInstrument = false;

	ofTrueTypeFont::setGlobalDpi(72);
	fontSize = 18;
	numHorizontalEditors = 1;
	numVerticalEditors.push_back(1);
	whichEditor = 0;
	Editor editor;
	editors.push_back(editor);
	editors.back().isActive = true;
	editors.back().setID(0);
	changeFontSizeForAllEditors();
	screenSplitHorizontal = seqVec.screenWidth;

	lineWidth = 2;

	tempNumInstruments = 0;
	seqVec.tempoMs = 500;
	seqVec.numerator = 4;
	seqVec.denominator = 4;
	seqVec.numBeats = 4;
	seqVec.tempNumBeats = 4;
	// 64ths are the minimum note duration
	// so we use 4 steps per this duration as a default
	minDur = 256;

	patternIndex = 0;
	numParse = 1;
	whenToStore = 1;
	storePattern = false;
	inserting = false;
	storingMultipleBars = false;

	seqVec.patternLoopIndex = 0;
	seqVec.tempPatternLoopIndex = 0;
	seqVec.thisPatternLoopIndex = 0;
	seqVec.updatePatternLoop = false;
	strayMelodicLine = true;
	storingNewPattern = false;
	seqVec.currentScoreIndex = 0;
	calculateOnNextStep = false;
	patternIndexToCalculate = 0;

	seqVec.showScore = false;
	seqVec.longestInstNameWidth = 0;
	seqVec.staffLinesDist = 10.0;
	seqVec.scoreFontSize = 35;
	instFontSize = seqVec.scoreFontSize/3.5;
	instFont.load("Monaco.ttf", instFontSize);
	seqVec.notesXStartPnt = 0;
	seqVec.blankSpace = 10;
	seqVec.animate = false;
	seqVec.setAnimation = false;
	seqVec.beatAnimate = false;
	seqVec.setBeatAnimation = false;
	seqVec.beatViz = false;
	seqVec.beatTypeCommand = false;
	seqVec.beatVizCounter = 0;
	seqVec.beatVizType = 1;
	seqVec.beatVizDegrade = BEATVIZBRIGHTNESS;
	seqVec.beatVizTimeStamp = 0;
	seqVec.beatVizStepsPerMs = (float)BEATVIZBRIGHTNESS / ((float)seqVec.tempoMs / 4.0);
	seqVec.beatVizRampStart = seqVec.tempoMs / 4;
	seqVec.noteWidth = 0;
	seqVec.noteHeight = 0;
	seqVec.allStaffDiffs = 0;
	seqVec.maxPatternFirstStaffAnchor = 0;

	// set the notes chars
	for (int i = 2; i < 9; i++) {
		noteChars[i-2] = char((i%7)+97);
	}
	noteChars[7] = char(114); // "r" for rest

	// store the articulation names strings
	string articulSymsLocal[8] = {"none", "marcato", "trill", "tenuto", \
																"staccatissimo", "accent", "staccato", "portando"};
	for (int i = 0; i < 8; i++) {
		seqVec.articulSyms[i] = articulSymsLocal[i];
	}
}

//--------------------------------------------------------------
void ofApp::update(){
	while (oscReceiver.hasWaitingMessages()) {
		ofxOscMessage m;
		oscReceiver.getNextMessage(m);
		string address = m.getAddress();

		for (unsigned i = 0; i < fromOscAddr.size(); i++) {
			if (address.substr(0, fromOscAddr[i].size()) == fromOscAddr[i]) {
				if (m.getArgType(0) == OFXOSC_TYPE_STRING) {
					string oscStr = m.getArgAsString(0);
					for (unsigned j = 0; j < oscStr.size(); j++) {
						if (address.find("/press") != string::npos) {
							editors[fromOsc[i]].fromOscPress((int)oscStr.at(j));
						}
						else if (address.find("/release") != string::npos) {
							editors[fromOsc[i]].fromOscRelease((int)oscStr.at(j));
						}
					}
				}
				else if (m.getArgType(0) == OFXOSC_TYPE_INT32) {
					size_t numArgs = m.getNumArgs();
					for (size_t j = 0; j < numArgs; j++) {
						int oscVal = (int)m.getArgAsInt32(j);
						if (address.find("/press") != string::npos) {
							editors[fromOsc[i]].fromOscPress(oscVal);
						}
						else if (address.find("/release") != string::npos) {
							editors[fromOsc[i]].fromOscRelease(oscVal);
						}
					}
				}
				else if (m.getArgType(0) == OFXOSC_TYPE_INT64) {
					size_t numArgs = m.getNumArgs();
					for (size_t j = 0; j < numArgs; j++) {
						int oscVal = (int)m.getArgAsInt64(j);
						if (address.find("/press") != string::npos) {
							editors[fromOsc[i]].fromOscPress(oscVal);
						}
						else if (address.find("/release") != string::npos) {
							editors[fromOsc[i]].fromOscRelease(oscVal);
						}
					}
				}
				else if (m.getArgType(0) == OFXOSC_TYPE_FLOAT) {
					size_t numArgs = m.getNumArgs();
					for (size_t j = 0; j < numArgs; j++) {
						int oscVal = (int)m.getArgAsFloat(j);
						if (address.find("/press") != string::npos) {
							editors[fromOsc[i]].fromOscPress(oscVal);
						}
						else if (address.find("/release") != string::npos) {
							editors[fromOsc[i]].fromOscRelease(oscVal);
						}
					}
				}
				else {
					cout << "unknown OSC message type\n";
				}
			}
		}
	}

	if (isWindowResized) {
		if ((ofGetElapsedTimeMillis() - windowResizeTimeStamp) > WINDOW_RESIZE_GAP) {
			isWindowResized = false;
			resizeWindow();
		}
	}
}

//--------------------------------------------------------------
void ofApp::draw(){
	for (unsigned i = 0; i < editors.size(); i++) {
		editors[i].drawText();
	}
	// draw the lines that separate the different editors
	if ((numHorizontalEditors > 1) || (numVerticalEditors[0] > 1)) {
		ofSetColor(brightness);
		for (int i = 0; i < numHorizontalEditors; i++) {
			int index = 0;
			for (int j = 0; j < numVerticalEditors[i] - 1; j++) {
				if (i) index = (i * numVerticalEditors[i-1]) + j;
				else index = i + j;
				ofDrawLine(editors[index].frameXOffset,
						   editors[index].frameHeight+editors[index].frameYOffset,
						   editors[index].frameXOffset+editors[index].frameWidth,
						   editors[index].frameHeight+editors[index].frameYOffset);
			}
			if (i) {
				ofDrawLine(i*screenSplitHorizontal, 0, i*screenSplitHorizontal, seqVec.tracebackYCoord);
			}
		}
		ofSetLineWidth(lineWidth);
	}
	if (seqVec.showScore) {
		drawScore();
	}
	drawTraceback();
}

//--------------------------------------------------------------
void ofApp::drawTraceback(){
	static int tracebackNumLinesStatic = 1;
	int maxXCoord = seqVec.screenWidth;
	//if (seqVec.showScore) maxXCoord = seqVec.screenWidth / 2;
	ofSetColor(BACKGROUND*1.5);
	ofDrawRectangle(0, seqVec.tracebackYCoord, maxXCoord, seqVec.screenHeight-seqVec.tracebackYCoord);
	ofSetColor(brightness);
	ofSetLineWidth(lineWidth*2);
	ofDrawLine(0, seqVec.tracebackYCoord, maxXCoord, seqVec.tracebackYCoord);
	ofSetLineWidth(lineWidth);
	ofSetColor(255, 0, 0);
	int tracebackCounter = 0;
	if (editors[whichEditor].tracebackTimeStamp.size() > 0) {
		int tracebackNumLines = 0;
		for (unsigned i = 0; i < editors[whichEditor].tracebackTimeStamp.size(); i++) {
			if ((ofGetElapsedTimeMillis() - editors[whichEditor].tracebackTimeStamp[i]) < editors[whichEditor].executionRampStart) {
				if (editors[whichEditor].cursorLineIndex != editors[whichEditor].tracebackTempIndex[i]) {
					switch (editors[whichEditor].tracebackColor[editors[whichEditor].tracebackTempIndex[i]]) {
						case 0:
							ofSetColor(255);
							break;
						case 1:
							ofSetColor(255, 255, 0);
							break;
						case 2:
							ofSetColor(255, 0, 0);
							break;
						default:
							ofSetColor(255);
							break;
					}
					// accumulate the height of the traceback space
					tracebackNumLines += (editors[whichEditor].tracebackNumLines[editors[whichEditor].tracebackTempIndex[i]]+2);
					int tracebackYCoord = seqVec.screenHeight - (cursorHeight * tracebackNumLines) + (letterHeightDiff*2) + lineWidth;
					seqVec.tracebackYCoord = min(seqVec.tracebackYCoord, tracebackYCoord);
					// if the number of lines in the traceback has changed, change the maximum number of lines in the editor
					if (tracebackNumLines != tracebackNumLinesStatic) {
						editors[whichEditor].setMaxNumLines(tracebackNumLines, true);
						tracebackNumLinesStatic = tracebackNumLines;
					}
					// used to prepend the line number with
					// string tracebackStrLocal = "line "+ofToString(editors[whichEditor].tracebackTempIndex[i]+1)+": "+
					string tracebackStrLocal = editors[whichEditor].tracebackStr[editors[whichEditor].tracebackTempIndex[i]];
					font.drawString(tracebackStrLocal, editors[whichEditor].halfCharacterWidth, seqVec.tracebackYCoord+(editors[whichEditor].cursorHeight*(tracebackCounter+1)));
					tracebackCounter++;
				}
			}
			else {
				editors[whichEditor].tracebackTimeStamp.erase(editors[whichEditor].tracebackTimeStamp.begin()+i);
				editors[whichEditor].tracebackTempIndex.erase(editors[whichEditor].tracebackTempIndex.begin()+i);
			}
		}
	}
	if (editors[whichEditor].tracebackStr[editors[whichEditor].cursorLineIndex].size() > 0) {
		// similar to above, used to prepend the line number
		string tracebackStrLocal = editors[whichEditor].tracebackStr[editors[whichEditor].cursorLineIndex];
		switch (editors[whichEditor].tracebackColor[editors[whichEditor].cursorLineIndex]) {
			case 0:
				ofSetColor(255);
				break;
			case 1:
				ofSetColor(255, 255, 0);
				break;
			case 2:
				ofSetColor(255, 0, 0);
				break;
			default:
				ofSetColor(255);
				break;
		}
		// as above, if the number of lines of the traceback changes, change the maximum number of lines in the editor
		if (editors[whichEditor].tracebackNumLines[editors[whichEditor].cursorLineIndex] != tracebackNumLinesStatic) {
			tracebackNumLinesStatic = editors[whichEditor].tracebackNumLines[editors[whichEditor].cursorLineIndex];
			editors[whichEditor].setMaxNumLines(tracebackNumLinesStatic, true);
		}
		// set the height of the traceback space
		seqVec.tracebackYCoord = seqVec.screenHeight - (cursorHeight * (editors[whichEditor].tracebackNumLines[editors[whichEditor].cursorLineIndex]+2)) + (letterHeightDiff*2) + lineWidth;
		font.drawString(tracebackStrLocal, editors[whichEditor].halfCharacterWidth, seqVec.tracebackYCoord+(editors[whichEditor].cursorHeight*(tracebackCounter+1)));
	}
	else if (editors[whichEditor].tracebackTimeStamp.size() == 0) {
		// if the number of lines of the traceback changes, reset the maximum number of lines of the editor
		if (seqVec.tracebackYCoord != seqVec.tracebackYCoordAnchor) {
			seqVec.tracebackYCoord = seqVec.tracebackYCoordAnchor;
			editors[whichEditor].setMaxNumLines(1, true);
			tracebackNumLinesStatic = 1;
		}
	}
}

//--------------------------------------------------------------
void ofApp::drawScore(){
	ofSetColor(brightness);
	ofDrawRectangle(seqVec.staffXOffset, 0, seqVec.screenWidth/2, seqVec.tracebackYCoord);
	// show the animated beat
	if (seqVec.beatAnimate) {
		float xAnimationPos = 0;
		float yAnimationMaxPos = 0;
		float yAnimationMinPos = FLT_MAX;
		if (seqVec.activeInstruments.size() > 0) {
			// for (unsigned i = 0; i < seqVec.notesVec.size(); i++) {
			// 	float yMinPos = seqVec.notesVec[i].getMinYPos(0);
			// 	float yMaxPos = seqVec.notesVec[i].getMaxYPos(0);
			// 	if (yMinPos < yAnimationMinPos) yAnimationMinPos = yMinPos;
			// 	if (yMaxPos > yAnimationMaxPos) yAnimationMaxPos = yMaxPos;
			// }
			yAnimationMinPos = seqVec.notesVec[0].getMinYPos(0) - seqVec.noteHeight;
			yAnimationMaxPos = seqVec.notesVec.back().getMaxYPos(0) + seqVec.noteHeight;
		}
		int yAnimationSize = yAnimationMaxPos - yAnimationMinPos;
		if (seqVec.beatViz) {
			ofSetColor(50, 200, 0, seqVec.beatVizDegrade);
			int xSize = 0;
			// seqVec.blankSpace * 2 because the initial Y position is yPos-seqVec.blankSpace
			//int ySize = (yEnd + (seqVec.staffLinesDist * 4) + (seqVec.blankSpace * 2)) - yPos;
			switch (seqVec.beatVizType) {
				case 1:
					xAnimationPos = seqVec.staffXOffset+seqVec.longestInstNameWidth+(seqVec.blankSpace*1.5);
					xSize = (seqVec.screenWidth - xAnimationPos - seqVec.blankSpace);
					break;
				case 2:
					xAnimationPos = seqVec.notesXStartPnt + (seqVec.distBetweenBeats[patternIndex] * seqVec.beatVizCounter);
					xSize = seqVec.noteWidth * 2;
					break;
				default:
					break;
			}
			ofDrawRectangle(xAnimationPos-(seqVec.blankSpace/2), yAnimationMinPos, xSize, yAnimationSize);
			if ((ofGetElapsedTimeMillis() - seqVec.beatVizTimeStamp) >= seqVec.beatVizRampStart) {
				// calculate how many steps the brightness has to dim, depending on the elapsed time
				// and the steps per millisecond
				int brightnessDegrade = (int)((ofGetElapsedTimeMillis()-(seqVec.beatVizTimeStamp+seqVec.beatVizRampStart))*seqVec.beatVizStepsPerMs);
				seqVec.beatVizDegrade = BEATVIZBRIGHTNESS - brightnessDegrade;
				if (seqVec.beatVizDegrade < 0) {
					seqVec.beatVizDegrade = 0;
					seqVec.beatViz = false;
					seqVec.beatVizDegrade = BEATVIZBRIGHTNESS;
				}
			}
		}
	}
	// then write the name of each intsrument before its staff
	// and get the longest name
	ofSetColor(0);
	int xPos = seqVec.staffXOffset+seqVec.longestInstNameWidth+(seqVec.blankSpace*1.5);
	// draw one line at each side of the seqVec.staffs to connect them
	if (seqVec.instruments.size() > 1) {
		float y1 = seqVec.scoreYAnchors[0][seqVec.currentScoreIndex];
		float y2 = seqVec.scoreYAnchors.back()[seqVec.currentScoreIndex];
		ofDrawLine(xPos, y1, xPos, y2);
		ofDrawLine(seqVec.staffWidth-seqVec.blankSpace-seqVec.staffLinesDist, y1,
				   seqVec.staffWidth-seqVec.blankSpace-seqVec.staffLinesDist, y2);
	}
	// write the names of the instruments
	for (unsigned i = 0; i < seqVec.instruments.size(); i++) {
		int xPos = seqVec.staffXOffset+seqVec.blankSpace+seqVec.longestInstNameWidth-instNameWidths[i];
		instFont.drawString(seqVec.instruments[i], xPos, seqVec.scoreYAnchors[i][seqVec.currentScoreIndex]+(seqVec.staffLinesDist*2.5));
	}
	// then we display the score
	for (unsigned i = 0; i < seqVec.staffs.size(); i++) {
		seqVec.staffs[i].drawStaff(seqVec.currentScoreIndex);
	}
	// we draw the staffs and notes separately so we can display
	// empty staffs, before we assign any notes to them
	if ((int)seqVec.patternData.size() > seqVec.currentScoreIndex) {
		if (seqVec.patternData[seqVec.currentScoreIndex].size() > 0) {
			int bar = seqVec.patternData[seqVec.currentScoreIndex][0];
			for (unsigned i = 2; i < seqVec.patternData[seqVec.currentScoreIndex].size(); i++) {
				int inst = seqVec.patternData[seqVec.currentScoreIndex][i];
				seqVec.notesVec[inst].drawNotes(bar, seqVec.currentScoreIndex);
			}
		}
	}
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key, int thisEditor){
	int whichEditorBackup = whichEditor;
	// change the currently active editor temporarily
	// because this overloaded function is called from an editor
	// when it receives input from OSC
	// this way, even if an editor is out of focus
	// we're still able to type in it properly
	whichEditor = thisEditor;
	keyPressed(key);
	whichEditor = whichEditorBackup;
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key, int thisEditor){
	// the same applies to releasing a key
	int whichEditorBackup = whichEditor;
	whichEditor = thisEditor;
	keyReleased(key);
	whichEditor = whichEditorBackup;
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
	switch (key) {
		case 1:
			for (unsigned i = 0; i < editors.size(); i++) {
				editors[i].shiftPressed = true;
			}
			//editors[whichEditor].shiftPressed = true;
			shiftPressed = true;
			break;
		case 2:
			for (unsigned i = 0; i < editors.size(); i++) {
				editors[i].ctlPressed = true;
			}
			//editor[whichEditor].ctlPressed = true;
			ctlPressed = true;
			break;
		case 4:
			for (unsigned i = 0; i < editors.size(); i++) {
				editors[i].altPressed = true;
			}
			//editors[whichEditor].altPressed = true;
			altPressed = true;
			break;
		// below are codes of the Ctl, Alt, and Shift keys
		// that are sent together with the values above
		// left ctl
		case 3680:
			break;
		// right ctl
		case 3683:
			break;
		// left alt
		case 3684:
			break;
		// right alt
		case 3685:
			break;
		// left shift
		case 3682:
			break;
		// right shift
		case 3681:
			break;
		case 57357:
			editors[whichEditor].upArrow(-1); // -1 for single increment
			break;
		case 57359:
			editors[whichEditor].downArrow(-1); // -1 for single decrement
			break;
		case 57358:
			editors[whichEditor].rightArrow();
			break;
		case 57356:
			editors[whichEditor].leftArrow();
			break;
		case 57360:
			editors[whichEditor].pageUp();
			break;
		case 57361:
			editors[whichEditor].pageDown();
			break;
		default:
			if (key == 61 && ctlPressed) { // +
				fontSize += 2;
				changeFontSizeForAllEditors();
			}
			else if (key == 45 && ctlPressed) { // -
				fontSize -= 2;
				if (fontSize == 0) fontSize = 2;
				changeFontSizeForAllEditors();
			}
			else if (key == 43 && ctlPressed) { // + with shift pressed
				seqVec.scoreFontSize += 5;
				instFontSize = seqVec.scoreFontSize / 3.5;
				instFont.load("Monaco.ttf", instFontSize);
				setScoreSizes();
			}
			else if (key == 95 && ctlPressed) { // - with shift pressed
				seqVec.scoreFontSize -= 5;
				if (seqVec.scoreFontSize < 5) seqVec.scoreFontSize = 5;
				instFontSize = seqVec.scoreFontSize / 3.5;
				instFont.load("Monaco.ttf", instFontSize);
				setScoreSizes();
			}
			else if (key == 102 && ctlPressed) { // f
				ofToggleFullscreen();
				fullScreen = !fullScreen;
			}
			else if ((char(key) == 'q' || char(key) == 'w') && ctlPressed) {
				exit();
				ofExit();
			}
			// add editors. V for vertical, H for horizontal (with shift pressed)
			else if (((key == 86) || (key == 72)) && ctlPressed) {
				Editor editor;
				editor.setID(whichEditor+1);
				editors.insert(editors.begin()+whichEditor+1, editor);
				if (key == 72) { // H
					int counter = 0;
					bool foundEditor = false;
					for (unsigned i = 0; i < numVerticalEditors.size(); i++) {
						if (whichEditor <= counter) {
							numVerticalEditors[i]++;
							foundEditor = true;
							break;
						}
						counter++;
					}
					if (!foundEditor) {
						numVerticalEditors[numHorizontalEditors-1]++;
					}
				}
				else {
					numHorizontalEditors++;
					int counter = 0;
					bool foundEditor = false;
					for (unsigned i = 0; i < numVerticalEditors.size(); i++) {
						if (whichEditor < counter) {
							numVerticalEditors.insert(numVerticalEditors.begin()+i, 1);
							foundEditor = true;
							break;
						}
						counter++;
					}
					if (!foundEditor) {
						numVerticalEditors.push_back(1);
					}
				}
				editors[whichEditor].isActive = false;
				whichEditor++;
				editors[whichEditor].isActive = true;
				changeFontSizeForAllEditors();
			}
			// remove editors. v for vertical, h for horizontal (without shift)
			else if (((key == 118) || (key == 104)) && ctlPressed && altPressed) {
				if (editors.size() > 1) {
					// find which editor is highlighted
					int index = 0;
					for (int i = 0; i < numHorizontalEditors; i++) {
						bool indexFound = false;
						for (int j = 0; j < numVerticalEditors[i]; j++) {
							if (i) index = (i * numVerticalEditors[i-1]) + j;
							else index = i + j;
							if (index == whichEditor) {
								if (numVerticalEditors[i] > 0) numVerticalEditors[i]--;
								else numHorizontalEditors--;
								if (numVerticalEditors[i] == 0) {
									numVerticalEditors.erase(numVerticalEditors.begin()+i);
									numHorizontalEditors--;
								}
								indexFound = true;
								break;
							}
						}
						if (indexFound) break;
					}
					editors.erase(editors.begin()+index);
					if (whichEditor < (int)(editors.size()-1)) {
						editors[whichEditor].isActive = false;
					}
					whichEditor--;
					if (whichEditor < 0) whichEditor = 0;
					editors[whichEditor].isActive = true;
					changeFontSizeForAllEditors();
				}
			}
			// set which editor is active
			else if ((key >= 49) && (key <= 57) && ctlPressed) {
				if ((key - 49) < (int)editors.size()) {
					editors[whichEditor].isActive = false;
					whichEditor = key - 49;
					editors[whichEditor].isActive = true;
				}
			}
			// swap score position with Ctl+P for horizontal
			// and Ctl+Shift+P for vertical
			else if ((key == 112) && ctlPressed) {
				swapScorePosition(0);
			}
			else if ((key == 80) && ctlPressed) {
				swapScorePosition(1);
			}
			else {
				editors[whichEditor].allOtherKeys(key);
			}
			break;
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){
	switch (key) {
		case 1:
			for (unsigned i = 0; i < editors.size(); i++) {
				editors[i].shiftPressed = false;
			}
			//editors[whichEditor].shiftPressed = false;
			shiftPressed = false;
			break;
		case 2:
			for (unsigned i = 0; i < editors.size(); i++) {
				editors[i].ctlPressed = false;
			}
			//editors[whichEditor].ctlPressed = false;
			ctlPressed = false;
			break;
		case 4:
			for (unsigned i = 0; i < editors.size(); i++) {
				editors[i].altPressed = false;
			}
			//editors[whichEditor].altPressed = false;
			altPressed = false;
			break;
	}
}

//--------------------------------------------------------------
void ofApp::sendToParts(ofxOscMessage m){
	for (unsigned i = 0; i < seqVec.sendsToPart.size(); i++) {
		if (seqVec.sendsToPart[i]) {
			seqVec.scorePartSenders[i].sendMessage(m, false);
		}
	}
}

//--------------------------------------------------------------
void ofApp::printVector(vector<int> v){
	for (unsigned i = 0; i < v.size(); i++) {
		cout << v[i] << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
void ofApp::printVector(vector<string> v){
	for (unsigned i = 0; i < v.size(); i++) {
		cout << v[i] << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
void ofApp::printVector(vector<float> v){
	for (unsigned i = 0; i < v.size(); i++) {
		cout << v[i] << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
bool ofApp::startsWith(string a, string b){
	string aLocal(1, a[0]);
	if (aLocal.compare(b) == 0) return true;
  return false;
}

//--------------------------------------------------------------
bool ofApp::endsWith(string a, string b){
	string aLocal(1, a[a.size()-1]);
	if (aLocal.compare(b) == 0) return true;
	return false;
}

//--------------------------------------------------------------
bool ofApp::isDigit(string str){
	for (int i = 0; i < (int)str.length(); i++) {
		if (!isdigit(str[i])) {
			return false;
		}
	}
	return true;
}

//-------------------------------------------------------------
vector<int> ofApp::findRepetitionInt(string str, int multIndex){
	vector<int> numRepNumDitigs;
	int index = findNextStrCharIdx(str, " ", multIndex);
	int numDigits = index - multIndex - 1;
	if (isDigit(str.substr(multIndex+1, numDigits))) {
		numRepNumDitigs.push_back(stoi(str.substr(multIndex+1, numDigits)));
		numRepNumDitigs.push_back(numDigits);
	}
	return numRepNumDitigs;
}

//--------------------------------------------------------------
string ofApp::detectRepetitionsSubStr(string str) {
	// this function detects repetitions within a nested repetition scheme
	// otherwise it is called for the entire string
	string strToProcess = "";
	vector<string> strToRepeat;
	vector<int> numRepetitions;
	vector<int> startOfRepetitions;
	vector<int> numMultDigits;
	vector<int> multIndexes = findIndexesOfCharInStr(str, "*");
	for (unsigned i = 0; i < multIndexes.size(); i++) {
		vector<int> numRepNumDitigs = findRepetitionInt(str, multIndexes[i]);
		numRepetitions.push_back(numRepNumDitigs[0]);
		numMultDigits.push_back(numRepNumDitigs[1]);
		string charToCompare = " ";
		int idxOffset = 0;
		startOfRepetitions.push_back(multIndexes[i]-idxOffset);
		int index = startOfRepetitions.back();
		while ((str.substr(index, 1).compare(charToCompare) != 0) && \
			   startOfRepetitions.back() >= 0) {
			// in case we repeat a chord we change the compared char
			if (str.substr(index, 1).compare(">") == 0) {
				charToCompare = "<";
			}
			// in case we have a quote, detect the pairing quote
			else if (str.substr(index, 1).compare("\"") == 0) {
				bool foundChar = false;
				for (int i = startOfRepetitions.back(); i >= 0; i--) {
					// a pairing quote might have been forgotten, so we also check for a white space
					if (str.substr(i, 1).compare("\"") == 0 || \
						str.substr(i, 1).compare(charToCompare) == 0) {
						startOfRepetitions.back() -= i;
						foundChar = true;
						break;
					}
				}
				// add one because we're subtracting one below
				if (foundChar) startOfRepetitions.back()++;
			}
			startOfRepetitions.back()--;
			// use a local index so we can safely keep above negative values
			// without being unable to exit the while loop
			index = startOfRepetitions.back();
			if (index < 0) index = 0;
		}
		// if the starting index reaches the end, it will go to -1 so we rest to 0
		if (startOfRepetitions.back() < 0) startOfRepetitions.back() = 0;
		int startOffset = 1;
		int endOffset = 1;
		if ((startOfRepetitions.back() == 0) || (charToCompare.compare("<") == 0)) {
			startOffset = 0;
			endOffset = 0;
		}
		strToRepeat.push_back(str.substr(startOfRepetitions.back()+startOffset, (multIndexes[i]-startOfRepetitions.back()-endOffset)));
	}
	int index = 0;
	for (unsigned i = 0; i < multIndexes.size(); i++) {
		int substrLen = startOfRepetitions[i]-index;
		if (substrLen < 0) substrLen = 0;
		strToProcess += str.substr(index, substrLen);
		if ((substrLen > 0) || (i > 0)) strToProcess += " ";
		for (int j = 0; j < numRepetitions[i]; j++) {
			strToProcess += strToRepeat[i];
			strToProcess += " ";
		}
		// remove the extra white space that is added in the last iteration above
		if ((substrLen > 0) || (numRepetitions[i] > 0)) {
			strToProcess.pop_back();
		}
		index = multIndexes[i] + numMultDigits[i] + 1;
	}
	if (index <= (int)str.size()) {
		strToProcess += str.substr(index, str.size()-index);
	}
	// sometimes we end up with double spaces so we replace them with single ones
	strToProcess = replaceCharInStr(strToProcess, "  ", " ");
	return strToProcess;
}

//--------------------------------------------------------------
int ofApp::findNextStrCharIdx(string str, string compareStr, int index){
	while (index < (int)str.size()) {
		if (str.substr(index, 1).compare(compareStr) == 0) {
			break;
		}
		index++;
	}
	return index;
}

//--------------------------------------------------------------
string ofApp::detectRepetitions(string str){
	// we'll replace the string from allStrings vector with the string below
	// as we'll duplicate seqVec.notes or groups of seqVec.notes that are repeated with *
	string strToProcess = "";
	// but first we'll check for nested repetitions
	vector<int> squareBracketsOpenIdx = findIndexesOfCharInStr(str, "[");
	vector<int> squareBracketsCloseIdx = findIndexesOfCharInStr(str, "]");
	vector<int> multIndexes = findIndexesOfCharInStr(str, "*");
	vector<vector<vector<int>>> squareBracketsNest;
	for (unsigned i = 0; i < squareBracketsCloseIdx.size(); i++) {
		for (unsigned j = squareBracketsOpenIdx.size()-1; j >= 0; j--) {
			// as soon as we find an index of an opening bracket that's smaller
			// then the index of the closing one, then we have a pair
			if (squareBracketsOpenIdx[j] < squareBracketsCloseIdx[i]) {
				if (i > 0) {
					// if the index of the current opening bracket is greater than that
					// of the previous closing bracket, we have a new nest
					if (squareBracketsOpenIdx[j] > squareBracketsCloseIdx[i-1]) {
						squareBracketsNest.resize(squareBracketsNest.size()+1);
					}
				}
				// create a new slot for the beginning of the loop
				else {
					squareBracketsNest.resize(squareBracketsNest.size()+1);
				}
				squareBracketsNest.back().push_back({squareBracketsOpenIdx[j], squareBracketsCloseIdx[i]});
				// once we store the pair, we delete the opening bracket element
				// so that in the next iteration of the parent loop it's not there
				// as if it were there it would be detected before the bracket of the nest
				// e.g. [c'4 [d' e'] f'] [g' d']
				// indexes are 0, 5, 17 for opening and 11, 15, and 23
				// as soon as we detect 5 as a pair for 11, if we don't erase it
				// when we look up 15, 5 will again be smaller, so it must be deleted
				// in order for 0 to be paired with 15
				squareBracketsOpenIdx.erase(squareBracketsOpenIdx.begin()+j);
				break;
			}
		}
	}
	// remove possible beam grouppings which are also denoted with []
	// iterate backwards so that indexing is not affected as the loop progresses
	int firstSquareBracketIndex = (int)str.size();
	for (unsigned i = 0; i < squareBracketsNest.size(); i++) {
		for (unsigned j = squareBracketsNest[i].size()-1; j >= 0; j--) {
			// the loop iterates one time too many so we're making sure we're within bounds
			if (j > (squareBracketsNest[i].size()) || (j < 0)) {
				break;
			}
			// safety test
			if ((int)str.size() > (squareBracketsNest[i][j][1]+1)) {
				if (str.substr(squareBracketsNest[i][j][1]+1, 1).compare("*") != 0) {
					squareBracketsNest[i].erase(squareBracketsNest[i].begin()+j);
				}
			}
			// since we iterate over the indexes, we store the index of the first opening square bracket
			if (squareBracketsNest[i].size() > j) {
				if (squareBracketsNest[i][j][0] < firstSquareBracketIndex) {
					firstSquareBracketIndex = squareBracketsNest[i][j][0];
				}
			}
		}
	}
	if (multIndexes.size() == 0) {
		return str;
	}
	if (squareBracketsNest.size() == 0) {
		return detectRepetitionsSubStr(str);
	}
	else {
		// if there are charecters before the nest
		if (firstSquareBracketIndex > 0) {
			strToProcess += detectRepetitionsSubStr(str.substr(0, firstSquareBracketIndex-1));
			strToProcess += " ";
		}
		for (unsigned i = 0; i < squareBracketsNest.size(); i++) {
			string nestStrFinal;
			int numRepetitions = 0;
			int numDigits = 0;
			// extract the outter most string in the nest
			int outterStrStart = squareBracketsNest[i].back()[0];
			int strLen = squareBracketsNest[i].back()[1] - outterStrStart + 1;
			string nestStrOutter = str.substr(squareBracketsNest[i].back()[0], strLen);
			// variable that will hold the difference in length in the child string
			// before and after it has been processed
			int strLenDiff = 0;
			for (unsigned j = 0; j < squareBracketsNest[i].size(); j++) {
				// if we're inside a nest
				if ((squareBracketsNest[i].size()-1) > j) {
					int strStart = squareBracketsNest[i][j+1][0] - outterStrStart;
					strLen = squareBracketsNest[i][j+1][1] - outterStrStart - strStart + strLenDiff + 1;
					// store the parent nest string first
					string nestStrParent = nestStrOutter.substr(strStart, strLen);
					// save the size for replacing the parent in the outter later on
					unsigned parentSize = nestStrParent.size();
					// add 1 to escape the "["
					strStart = squareBracketsNest[i][j][0] - outterStrStart + 1;
					strLen = squareBracketsNest[i][j][1] + strLenDiff - outterStrStart - strStart;
					int multIndex = squareBracketsNest[i][j][1]+strLenDiff+1-outterStrStart;
					vector<int> numRepNumDitigs = findRepetitionInt(nestStrOutter, multIndex);
					if (numRepNumDitigs.size() > 0) {
						numRepetitions = numRepNumDitigs[0];
						numDigits = numRepNumDitigs[1];
					}
					// then create a string by processing the child nest
					string nestStrChild = detectRepetitionsSubStr(nestStrOutter.substr(strStart, strLen));
					// and repeat as many times as the repetition coefficient
					// -1 because the first instance is already copied
					if (numRepetitions > 0) numRepetitions--;
					// copy the string because if we use it intact inside the loop below
					// it will grow exponentially
					string nestStrChildCopy = nestStrChild;
					for (int k = 0; k < numRepetitions; k++) {
						nestStrChild += " ";
						nestStrChild += nestStrChildCopy;
					}
					// remove the extra white space
					if (nestStrChild.back() == ' ') nestStrChild.pop_back();
					// and replace the processed string within the parent nest
					strStart -= squareBracketsNest[i][j+1][0] - outterStrStart;
					nestStrParent.erase(strStart-1, strLen+numDigits+3);
					nestStrParent.insert(strStart-1, nestStrChild);
					if (nestStrParent.back() == ' ') nestStrParent.pop_back();
					// calculate the difference between the child string before and after processing
					strLenDiff += ((int)nestStrChild.size() - strLen - numDigits - 3);
					// replace the parent string in the corresponding place in the outter string
					nestStrOutter.erase(squareBracketsNest[i][j+1][0]-outterStrStart, parentSize);
					nestStrOutter.insert(squareBracketsNest[i][j+1][0]-outterStrStart, nestStrParent);
					if (nestStrOutter.back() == ' ') nestStrOutter.pop_back();
				}
				else {
					// if we're at the outter part of the nest we process the whole nest
					int multIndex = squareBracketsNest[i][j][1]+1;
					vector<int> numRepNumDitigs = findRepetitionInt(str, multIndex);
					if (numRepNumDitigs.size() > 0) {
						numRepetitions = numRepNumDitigs[0];
						numDigits = numRepNumDitigs[1];
					}
					int strStart = squareBracketsNest[i][j][0] - outterStrStart + 1;
					int strLen = (int)nestStrOutter.size() - 2;
					nestStrFinal = detectRepetitionsSubStr(nestStrOutter.substr(strStart, strLen));
					if (numRepetitions > 0) numRepetitions--;
					string nestStrFinalCopy = nestStrFinal;
					for (int k = 0; k < numRepetitions; k++) {
						nestStrFinal += " ";
						nestStrFinal += nestStrFinalCopy;
					}
					if (nestStrFinal.back() == ' ') nestStrFinal.pop_back();
				}
			}
			strToProcess += nestStrFinal;
			// check if there's text in between nests
			if (squareBracketsNest.size() > (i+1)) {
				int whiteSpaceIndex = findNextStrCharIdx(str, " ", squareBracketsNest[i].back()[1]+1);
				// if there are more than one characters in between nests
				// it means there's more than a white space there so we must store that text
				if ((squareBracketsNest[i+1][0][0] - whiteSpaceIndex) > 1) {
					// subtract 2 to get rid of the white spaces at the beginning and end of the substring
					int strLen = squareBracketsNest[i+1][0][0] - whiteSpaceIndex - 2;
					strToProcess += " ";
					// add 1 to whiteSpaceIndex to get rid of the actual white space
					strToProcess += detectRepetitionsSubStr(str.substr(whiteSpaceIndex+1, strLen));
					strToProcess += " ";
				}
			}
			// if we're at the end of the brackets check if there's more text left
			else if ((int)str.size() > (squareBracketsNest.back().back()[1]+numDigits+3)) {
				int whiteSpaceIndex = findNextStrCharIdx(str, " ", squareBracketsNest.back().back()[1]+1);
				strToProcess += " ";
				strToProcess += detectRepetitionsSubStr(str.substr(whiteSpaceIndex+1));
			}
		}
		if (strToProcess.back() == ' ') strToProcess.pop_back();
		return strToProcess;
	}
}

//---------------------------------------------------------------
vector<int> ofApp::findIndexesOfCharInStr(string str, string charToFind){
	vector<int> tokensIndexes;
	size_t pos = str.find(charToFind, 0);
	while (pos != string::npos) {
    tokensIndexes.push_back((int)pos);
    pos = str.find(charToFind, pos+1);
	}
	return tokensIndexes;
}

//---------------------------------------------------------------
string ofApp::replaceCharInStr(string str, string a, string b){
	auto it = str.find(a);
  while (it != string::npos) {
		str.replace(it, a.size(), b);
    it = str.find(a);
  }
	return str;
}

//---------------------------------------------------------------
vector<string> ofApp::tokenizeString(string str, string delimiter){
	size_t pos = 0;
	string token;
	vector<string> tokens;
	while ((pos = str.find(delimiter)) != string::npos) {
		token = str.substr(0, pos);
		tokens.push_back(token);
		str.erase(0, pos + delimiter.length());
	}
	tokens.push_back(str);
	return tokens;
}

//--------------------------------------------------------------
void ofApp::resetEditorStrings(int index, int numLines){
	for (unsigned i = 1; i < numLines-1; i++) {
		editors[whichEditor].allStrings[i+index] = tempLines[i-1];
	}
	tempLines.clear();
	allBars.clear();
}

//--------------------------------------------------------------
void ofApp::parseStrings(int index, int numLines){
	//static int counter = 0;
	// if we parse melodic lines, we parse each line twice
	// first to make sure there are no errors, and then, if there are no errors
	// we parse again to store the melodic/rhythmic lines
	vector<string> errors;
	bool noErrors = true;
	for (int i = index; i < (index+numLines); i++) {
		// parseString() returns an empty string on success otherwise an error string
		errors.push_back(parseString(editors[whichEditor].allStrings[i], i, numLines));
	}
	for (unsigned i = 0; i < errors.size(); i++) {
		if (errors[i].size() > 0) {
			parsedMelody = false;
			strayMelodicLine = true;
			noErrors = false;
			numParse = 1;
			if (storingMultipleBars) {
				resetEditorStrings(index, numLines);
				storingMultipleBars = false;
			}
		}
	}
	// if we have parsed at least one melodic line
	if ((parsedMelody && !strayMelodicLine && storePattern) || parsingLoopPattern) {
		if (!inserting && !parsingLoopPattern) {
			// first add a rest for any instrument that is not included in the pattern
			fillInMissingInsts();
			// then sort the instument indexes
			sort(seqVec.patternData[patternIndex].begin(), seqVec.patternData[patternIndex].end());
			// insert the bar index at the beginning of the patternData vector at index patternIndex
			seqVec.patternData[patternIndex].insert(seqVec.patternData[patternIndex].begin(),barIndexGlobal);
			// insert the number of beats after the bar index in the patternData vector
			seqVec.patternData[patternIndex].insert(seqVec.patternData[patternIndex].begin()+1,seqVec.tempNumBeats);
			// insert the patternIndex at the current index of the pattern loops indexes
			seqVec.patternLoopIndexes[patternIndex].push_back(patternIndex);
			// update the score parts
			ofxOscMessage m;
			m.setAddress("/patidx");
			m.addIntArg(patternIndex);
			sendToParts(m);
		}
		if (!parsingLoopPattern) {
			// zero the first staff coord at current index
			seqVec.patternFirstStaffAnchor[patternIndex] = 0;
			// if (!storingMultipleBars) {
			// 	cout << "finished storing " << counter << endl;
			// 	counter++;
			// }
			setScoreNotes(patternIndex);
			if (!storingMultipleBars) { // || (storingMultipleBars && numParse == 1)) {
				posCalculator.setLoopIndex(patternIndex);
				posCalculator.startThread();
				ofxOscMessage m;
				m.setAddress("/poscalc");
				m.addIntArg(patternIndex);
				sendToParts(m);
			}
		}
		if (sequencer.isThreadRunning() && !storingMultipleBars) {
			sequencer.runSequencer = true;
		}
		else if (!parsingLoopPattern && !storingMultipleBars) {
			seqVec.patternLoopIndex = seqVec.tempPatternLoopIndex;
			seqVec.currentScoreIndex = seqVec.patternLoopIndexes[patternIndex][0];
			ofxOscMessage m;
			m.setAddress("/loopndx");
			m.addIntArg(seqVec.patternLoopIndex);
			m.addIntArg(seqVec.currentScoreIndex);
			sendToParts(m);
		}
		if (!inserting && !parsingLoopPattern) {
			barIndexGlobal++;
		}
		parsedMelody = false;
		strayMelodicLine = true;
		storingNewPattern = false;
		typedInstrument = false;
		storePattern = false;
		inserting = false;
		parsingLoopPattern = false;
	}
	else if (storingFunction) {
		storingFunction = false;
	}
	instrumentsPassed.clear();
	// when parsing melodic lines, we parse once to make sure there are no errors
	// and then, if there are no errors, we parse again to store the new patterns
	if (noErrors) {
		if (storingMultipleBars && numParse == 1) {
			// before we clear the temporary string vectors we must create a loop
			// with all the bars in the \bars command
			string createLoopStr = "";
			for (unsigned i = 0; i < allBars[0].size(); i++) {
				createLoopStr += (barsName + "-" + to_string(i+1));
				if (i < allBars[0].size()-1) {
					createLoopStr += " ";
				}
			}
			// store the loop
			pushNewPattern(barsName);
			// call parsePatternLoop() to create the loop
			string patternLoopStr = parsePatternLoop(createLoopStr, index, 1);
			// then reset the strings of the editor and clear the temporary ones
			resetEditorStrings(index, numLines);
			storingMultipleBars = false;
			barIndexGlobal++;
			// finally, calculate their positions, this time only by starting the posCalculator tread
			// as the multipleBarsPatternIndexes vector has already been populated and posCalculator
			// checks its size, and if it's greater than 0, then it sets the loop index internally
			posCalculator.startThread();
		}
		while (numParse > 1) {
			numParse--;
			if (numParse <= whenToStore) storePattern = true;
			parseStrings(index, numLines);
		}
		storePattern = false;
	}
	// if we're creating a pattern out of patterns
	// the posCalculator.calculateAllStaffPositions() function
	// is called from parseString() below, after the block that checks if
	// a line starts with a quotation mark
}

//--------------------------------------------------------------
string ofApp::parseString(string str, int lineNum, int numLines){
	if (str.length() == 0) return "";
	// strip white spaces
	while (startsWith(str, " ")) {
		str = str.substr(1);
	}
	// then check again if the string is empty
	if (str.length() == 0) return "";
	// after we've stripped all white spaces, remove any possible comments
	size_t commentIndex = str.find("%");
	if (commentIndex != string::npos) {
		str = str.substr(0, commentIndex);
	}
	// check if there's a trailing white space after removing a comment
	if (endsWith(str, " ")) {
		str = str.substr(0, str.size()-1);
	}
	// if we're left with a closing bracket only
	if (str.compare("}") == 0) return "";
	if (startsWith(str, "{")) {
		str = str.substr(1);
	}
	else if (endsWith(str, "}")) {
		str = str.substr(0, str.size()-1);
	}
	// check if the string starts with a closing bracket but has more stuff after it
	if (startsWith(str, "}")) {
		str = str.substr(1);
		// and check for a trailing white space
		if (startsWith(str, " ")) {
			str = str.substr(1);
		}
	}
	// last check if string is empty
	if (str.length() == 0) return "";
	if (startsWith(str, "\\")) {
		if (storingFunction) {
			functionBodies[lastFunctionIndex].push_back(str);
		}
		else {
			parsingCommand = true;
			string commandTracebackStr = parseCommand(str, lineNum, numLines);
			// a command might be an instrument with a melodic line following
			// in which case we should set the traceback from further down in this function
			// in the else part of this if / else if / else chain
			// if this is the case, parsingCommand is turned to false in parseCommand()
			if (parsingCommand) {
				if (commandTracebackStr.size() > 0) {
					editors[whichEditor].setTraceback(commandTracebackStr, lineNum);
				}
				else {
					editors[whichEditor].releaseTraceback(lineNum);
				}
			}
			return commandTracebackStr;
		}
	}
	// ignore comments
	else if (startsWith(str, "%")) {
		editors[whichEditor].releaseTraceback(lineNum);
		return "";
	}
	else {
		if (storingFunction) {
			functionBodies[lastFunctionIndex].push_back(str);
		}
		else if (parsingLoopPattern) {
			string tracebackStr = parsePatternLoop(str, lineNum, numLines);
			if (tracebackStr.size() > 0) {
				editors[whichEditor].setTraceback(tracebackStr, lineNum);
				popNewPattern();
			}
			else {
				editors[whichEditor].releaseTraceback(lineNum);
				if (sequencer.isThreadRunning()) {
					sequencer.runSequencer = true;
					patternIndexToCalculate = patternIndex;
				}
				else {
					seqVec.patternLoopIndex = patternIndex;
				}
			}
			return tracebackStr;
		}
		else if (typedInstrument) {
			string tracebackStr;
			bool parsingMelody = false;
			// check if we're parsing a melodic line by testing against the octave
			// characters which are likely to be present and are fobiden in bar and loop names
			string octaveChars[2] = {"'", ","};
			for (int i = 0; i < 2; i++) {
				if (str.find(octaveChars[i]) != string::npos) {
					parsingMelody = true;
					break;
				}
			}
			// if the octave symbols are not present, check for a note duration
			if (!parsingMelody) {
				string durs[7] = {"1", "2", "4", "8", "16", "32", "64"};
				string noteNames[7] = {"a", "b", "c", "d", "e", "f", "g"};
				string accidentalNames[4] = {"is", "isis", "es", "eses"};
				bool foundDur = false;
				int startNdx = 0;
				int durNdx;
				for (int i = 0; i < 7; i++) {
					durNdx = str.find(durs[i]);
					if (durNdx != string::npos) {
						// check for repetition or chord symbols
						// and set the start index of the substring we're querying accordingly
						if (str.substr(0, 1).compare("<") == 0) {
							startNdx++;
						}
						else if (str.substr(0, 1).compare("[") == 0) {
							startNdx++;
							// if we start with a repetition symbol, a chord symbol might be following
							if (str.substr(0, 1).compare("<") == 0) {
								startNdx++;
							}
						}
						foundDur = true;
						break;
					}
				}
				if (foundDur) {
					// if we do find a number, we must check if the naming matches a note
					for (int i = 0; i < 7; i++) {
						if (str.substr(startNdx, durNdx).compare(noteNames[i]) == 0) {
							parsingMelody = true;
							break;
						}
						else if (str.substr(startNdx, 1).compare(noteNames[i]) == 0) {
							// if the note name contains accidentals we must check it explicitly
							for (int j = 0; j < 4; j++) {
								// set the length of the substring with ((i%2)+1)*2 so we check
								// for 2 and 4 characters alternatingly
								if (str.substr(startNdx+1, ((i%2)+1)*2).compare(accidentalNames[j]) == 0) {
									parsingMelody = true;
									break;
								}
							}
						}
						// if we have an accidental, the inner loop of else if will
						// break, so we have to break the outter one too
						if (parsingMelody) break;
					}
				}
			}
			if (parsingMelody) {
				tracebackStr = parseMelodicLine(str);
			}
			else {
				tracebackStr = stripLineFromPattern(str);
			}
			if (tracebackStr.size() > 0) {
				editors[whichEditor].setTraceback(tracebackStr, lineNum);
			}
			else {
				if (parsingMelody) parsedMelody = true;
				editors[whichEditor].releaseTraceback(lineNum);
			}
			typedInstrument = false;
			return tracebackStr;
		}
	}
	return "";
}

//--------------------------------------------------------------
void ofApp::fillInMissingInsts(){
	vector<int> missingInsts;
	for (unsigned i = 0; i < seqVec.instruments.size(); i++) {
		missingInsts.push_back(i);
		for (unsigned j = 0; j < instrumentsPassed.size(); j++) {
			if ((int)i == instrumentsPassed[j]) {
				missingInsts.pop_back();
				break;
			}
		}
	}
	if (storePattern) {
		for (unsigned i = 0; i < missingInsts.size(); i++) {
			createEmptyMelody(missingInsts[i]);
			// update the pattern with this instrument
			seqVec.patternData.back().push_back(missingInsts[i]);
			updateActiveInstruments(missingInsts[i]);
		}
	}
}

//--------------------------------------------------------------
void ofApp::createEmptyMelody(int index){
	seqVec.notes[index].push_back({{-1.0}});
	seqVec.midiNotes[index].push_back({{-1}});
	seqVec.durs[index].push_back({minDur});
	seqVec.dursWithoutSlurs[index].push_back({minDur});
	seqVec.midiDursWithoutSlurs[index].push_back({minDur});
	seqVec.pitchBendVals[index].push_back({0});
	// default dynamics value so if we create a line after an empty one
	// we'll get the previous dynamic, which should be the default and not 0
	seqVec.dynamics[index].push_back({(100-((100-MINDB)/2))});
	// MIDI velocities need two notes, a noteOn and an noteOff
	seqVec.midiVels[index].push_back({72, 0});
	seqVec.dynamicsRamps[index].push_back({0});
	seqVec.glissandi[index].push_back({0});
	seqVec.midiGlissDurs[index].push_back({0});
	seqVec.midiDynamicsRampDurs[index].push_back({0});
	seqVec.articulations[index].push_back({0});
	seqVec.midiArticulationVals[index].push_back({0});
	seqVec.text[index].push_back({""});
	seqVec.textIndexes[index].push_back({0});
	slurBeginnings[index].push_back({-1});
	slurEndings[index].push_back({-1});
	scoreNotes[index].push_back({{-1}});
	scoreDurs[index].push_back({minDur});
	scoreDotIndexes[index].push_back({0});;
	scoreAccidentals[index].push_back({{4}});
	scoreOctaves[index].push_back({{0}});
	scoreGlissandi[index].push_back({0});
	scoreArticulations[index].push_back({0});
	scoreDynamics[index].push_back({-1});
	scoreDynamicsIndexes[index].push_back({-1});
	scoreDynamicsRampStart[index].push_back({-1});
	scoreDynamicsRampEnd[index].push_back({-1});
	scoreDynamicsRampDir[index].push_back({-1});
	scoreSlurBeginnings[index].push_back({-1});
	scoreSlurEndings[index].push_back({-1});
	scoreTexts[index].push_back({""});
}

//--------------------------------------------------------------
void ofApp::resizePatternVectors(){
	seqVec.patternData.resize(patternNames.size());
	seqVec.patternLoopIndexes.resize(patternLoopNames.size());
	seqVec.patternFirstStaffAnchor.resize(patternLoopNames.size());
	seqVec.maxStaffScorePos.resize(patternLoopNames.size());
	seqVec.minStaffScorePos.resize(patternLoopNames.size());
	seqVec.distBetweenBeats.resize(patternLoopNames.size());
	patternIndex = patternNames.size()-1;
	seqVec.tempPatternLoopIndex = patternIndex;
}

//--------------------------------------------------------------
void ofApp::sendPushPopPattern(){
	ofxOscMessage m;
	m.setAddress("/push_pop_pat");
	m.addIntArg(patternIndex);
	sendToParts(m);
}

//--------------------------------------------------------------
void ofApp::pushNewPattern(string patternName){
	patternNames.push_back(patternName);
	patternLoopNames.push_back(patternName);
	resizePatternVectors();
	storingNewPattern = true;
	ofxOscMessage m;
	sendPushPopPattern();
}

//--------------------------------------------------------------
void ofApp::popNewPattern(){
	patternNames.pop_back();
	patternLoopNames.pop_back();
	resizePatternVectors();
	storingNewPattern = false;
	sendPushPopPattern();
}

//--------------------------------------------------------------
string ofApp::parseCommand(string str, int lineNum, int numLines){
	vector<string> commands = tokenizeString(str.substr(1), " ");
	int BPMTempo = 0;
	string errorStr = "";

	if (commands[0].compare("time") == 0) {
		if (commands.size() > 2) {
			return "error: \\time command takes one argument only";
		}
		else if (commands.size() < 2) {
			return "error: \\time command needs one arguments";
		}
		int divisionSym = commands[1].find("/");
		string numeratorStr = commands[1].substr(0, divisionSym);
		string denominatorStr = commands[1].substr(divisionSym+1);
		int numeratorLocal = 0;
		int denominatorLocal = 0;
		if (isDigit(numeratorStr)) {
			numeratorLocal = stoi(numeratorStr);
		}
		else {
			return (string)"error: " + numeratorStr + (string)" is not an int";
		}
		if (isDigit(denominatorStr)) {
			denominatorLocal = stoi(denominatorStr);
		}
		else {
			return(string) "error: " + denominatorStr + (string)" is not an int";
		}
		seqVec.numerator = numeratorLocal;
		seqVec.denominator = denominatorLocal;
	}

	else if (commands[0].compare("tempo") == 0) {
		if (commands.size() > 2) {
			return "error: \\tempo command takes one argument only";
		}
		else if (commands.size() < 2) {
			return "error: \\tempo command needs one argument";
		}
		if (isDigit(commands[1])) {
			BPMTempo = stoi(commands[1]);
			// convert BPM to ms
			seqVec.tempoMs = (int)(1000.0 / ((float)BPMTempo / 60.0));
			// get the tempo of the minimum duration
			seqVec.newTempo = seqVec.tempoMs / (minDur / seqVec.denominator);
		}
		else {
			return "error: " + commands[1]+" is not an int";
		}
		if (sequencer.isThreadRunning()) {
			sequencer.updateTempo = true;
		}
	}

	else if (commands[0].compare("play") == 0) {
		if (commands.size() > 1) {
			return "error: \\play command takes no arguments";
		}
		// reset animation in case it is on
		if (seqVec.setAnimation) {
			for (unsigned i = 0; i < seqVec.notesVec.size(); i++) {
				seqVec.notesVec[i].setAnimation(true);
			}
			seqVec.animate = true;
		}
		sequencer.runSequencer = true;
		sequencer.startThread();
	}

	else if (commands[0].compare("stop") == 0) {
		if (commands.size() > 1) {
			return "error: \\stop command takes no arguments";
		}
		sequencer.sendAllNotesOff();
		sequencer.stopThread();
		if (seqVec.animate) {
			for (unsigned i = 0; i < seqVec.notesVec.size(); i++) {
				seqVec.notesVec[i].setAnimation(false);
			}
			seqVec.animate = false;
			seqVec.setAnimation = true;
		}
		ofxOscMessage m;
		m.setAddress("/play");
		m.addIntArg(0);
		sendToParts(m);
	}

	else if (commands[0].compare("reset") == 0) {
		if (commands.size() > 1) {
			return "error: \\reset command takes no arguments";
		}
		int ndx = seqVec.patternData[seqVec.patternLoopIndex][0];
		int bar = seqVec.patternData[ndx][0];
		for (unsigned i = 2; i < seqVec.patternData[ndx].size(); i++) {
			int inst = seqVec.patternData[ndx][i];
			// reset all bar data counters
			seqVec.barDataCounters[inst] = 0;
			int noteDur = seqVec.dursWithoutSlurs[inst][bar][seqVec.barDataCounters[inst]];
			if (seqVec.isMidi[inst]) noteDur = seqVec.midiDursWithoutSlurs[inst][bar][seqVec.barDataCounters[inst]];
			seqVec.beatCounters[inst] = noteDur * seqVec.tempo;
		}
		sequencer.sequencerRunning = false;
	}

	else if (commands[0].compare("finish") == 0) {
		if (commands.size() > 1) {
			return "error: \\finish command takes no arguments";
		}
		sequencer.finish = true;
	}

	else if (commands[0].compare("clef") == 0) {
		if (commands.size() < 2) {
			return "error: \\clef command takes a clef name as an argument";
		}
		else if (commands.size() > 2) {
			return "error: \\clef command takes one argument only";
		}
		if (commands[1].compare("treble") == 0) {
			seqVec.staffs[lastInstrumentIndex].setClef(0);
			seqVec.notesVec[lastInstrumentIndex].setClef(0);
		}
		else if (commands[1].compare("bass") == 0) {
			seqVec.staffs[lastInstrumentIndex].setClef(1);
			seqVec.notesVec[lastInstrumentIndex].setClef(1);
		}
		else if (commands[1].compare("alto") == 0) {
			seqVec.staffs[lastInstrumentIndex].setClef(2);
			seqVec.notesVec[lastInstrumentIndex].setClef(2);
		}
		else if (commands[1].compare("perc") == 0 || commands[1].compare("percussion") == 0) {
			seqVec.staffs[lastInstrumentIndex].setClef(3);
			seqVec.notesVec[lastInstrumentIndex].setClef(3);
		}
		else {
			return "error: unknown clef";
		}
	}

	else if (commands[0].compare("insts") == 0) {
		if (commands.size() < 2) {
			return "error: insts command takes at least one argument";
		}
		for (unsigned i = 1; i < commands.size(); i++) {
			bool instrumentExists = false;
			// isolate string from quotation marks
			//string instName = commands[i].substr(1, commands[i].size()-2);
			for (unsigned j = 0; j < seqVec.instruments.size(); j++) {
				if (commands[i].compare(seqVec.instruments[j]) == 0) {
					instrumentExists = true;
					lastInstrumentIndex = j;
					break;
				}
			}
			if (!instrumentExists) {
				initializeInstrument(commands[i]);
				createStaff();
				createNotes();
				// when we create a Staff object we send a dummy patternIndex argument
				// later on, when we'll set the coordinates, this argument will be meaningful
				initStaffCoords(patternIndex);
				//initNotesCoords(seqVec.tempNumBeats);
			}
			lastInstrument = commands[i];
		}
	}

	else if (commands[0].compare("score") == 0) {
		if (commands[1].compare("show") == 0) {
			seqVec.showScore = true;
			editors[whichEditor].setMaxCharactersPerString();
		}
		else if (commands[1].compare("hide") == 0) {
			seqVec.showScore = false;
			editors[whichEditor].setMaxCharactersPerString();
		}
		else {
			for (unsigned i = 1; i < commands.size(); i++) {
				string commandFound = setAnimationState(commands[i]);
				if (commandFound.size() > 0) {
					return (string)"error: " + commandFound;
				}
			}
		}
	}

	else if (commands[0].compare("cursor") == 0) {
		if ((commands.size() == 1) || (commands.size() > 2)) {
			return "error: \\cursor takes one argument";
		}
		// for some reason the OF functions are inverted
		if (commands[1].compare("hide")) {
			ofShowCursor();
		}
		else if (commands[1].compare("show")) {
			ofHideCursor();
		}
		else {
			return "error: unknown argument for \\cursor";
		}
	}

	else if (commands[0].compare("bar") == 0 || commands[0].compare("loop") == 0) {
		bool patternExists = false;
		string patternName;
		if (commands.size() < 2) {
			if (commands[0].compare("bar") == 0) {
				return "error: \\bar command takes a name as an argument";
			}
			else if (commands[0].compare("loop")) {
				return "error: \\loop command takes a name as an argument";
			}
		}
		patternName = commands[1];
		for (unsigned i = 0; i < patternNames.size(); i++) {
			if (patternName.compare(patternNames[i]) == 0) {
				return "warning: bar/loop/bars already exists";
			}
		}
		strayMelodicLine = false;
		if (commands[0].compare("bar") == 0 && numParse == 1 && !storePattern) {
			// we're always storing when half of the numbers of parsing iterations are done
			numParse = 2;
			whenToStore = numParse / 2;
			storePattern = false;
		}
		else {
			if (commands[0].compare("loop") == 0) {
				parsingLoopPattern = true;
			}
			pushNewPattern(patternName);
		}
		// since patterns include simple melodic lines in their definition
		// we need to know this when we parse these lines
		if (commands.size() > 2) {
			string restOfCommand = "";
			for (unsigned i = 2; i < commands.size()-1; i++) {
				restOfCommand += commands[i];
				restOfCommand += " ";
			}
			restOfCommand += commands[commands.size()-1];
			errorStr = parseString(restOfCommand, lineNum, numLines);
			if (parsingLoopPattern && errorStr.size() > 0) {
				parsingLoopPattern = false;
			}
		}
	}

	else if (commands[0].compare("bars") == 0) {
		bool patternExist = false;
		string patternName;
		if (commands.size() < 2) {
			return "error: \\bars command takes a name for a set of bars as an argument";
		}
		patternName = commands[1];
		for (unsigned i = 0; i < patternNames.size(); i++) {
			if (patternName.compare(patternNames[i]) == 0) {
				return "warning: bar/loop/bars already exists";
			}
		}
		strayMelodicLine = false;
		if (numParse == 1 && !storePattern) {
			// first parse the strings to isolate every bar
			if (numLines > 1) {
				for (int i = 1; i < numLines-1; i++) {
					string str = editors[whichEditor].allStrings[i+lineNum];
					allBars.push_back(tokenizeString(editors[whichEditor].allStrings[i+lineNum], "|"));
				}
			}
			// once all strings have been split based on the upright slash
			// we must place the instrument name to each bar
			for (unsigned i = 0; i < allBars.size(); i++) {
				string inst;
				bool instFound = false;
				// first erase the white spaces at the beginning of the first bar
				while (startsWith(allBars[i][0], " ")) {
					allBars[i][0] = allBars[i][0].substr(1);
				}
				size_t pos = allBars[i][0].find(" ");
				// if the first string has one token only
				if (pos == string::npos) {
					// check if that token is the name of an instrument
					for (unsigned j = 0; j < seqVec.instruments.size(); j++) {
						// if this is the case
						if (allBars[i][0].compare("\\"+seqVec.instruments[j]) == 0) { 
							inst = seqVec.instruments[j];
							instFound = true;
							break;
						}
					}
				}
				// if the string has more than one tokens
				else {
					// check if the first token is the instrument
					for (unsigned j = 0; j < seqVec.instruments.size(); j++) {
						if (allBars[i][0].substr(0, pos).compare("\\"+seqVec.instruments[j]) == 0) {
							inst = seqVec.instruments[j];
							instFound = true;
							break;
						}
					}
				}
				// if the first bar has no instrument assigned
				// assign the lastInstrument to it
				if (!instFound) {
					inst = lastInstrument;
					allBars[i][0] = "\\" + inst + " " + allBars[i][0];
				}
				// then iterate over the other bars
				for (unsigned j = 1; j < allBars[i].size(); j++) {
					// first erase any white spaces at the beginning
					while (startsWith(allBars[i][j], " ")) {
						allBars[i][j] = allBars[i][j].substr(1);
					}
					// then assign the found instrument to all other bars
					allBars[i][j] = "\\" + inst + " " + allBars[i][j];
				}
			}
			// once we get all the bars as separate strings with instruments assigned to them
			// we must make sure all lines have the same number of bars
			unsigned numBars;
			for (unsigned i = 0; i < allBars.size(); i++) {
				if (!i) numBars = allBars[i].size();
				else if (allBars[i].size() != numBars) {
					allBars.clear();
					return "error: all instruments must have the same number of bars";
				}
			}
			// then we set the number of string parsing to be the number of bars
			// times 2, so we first iterate and make sure there are no mistakes
			// in the melodic patterns, and then we iterate again and store the patterns
			numParse = (int)numBars * 2;
			// we're always storing when half of the parsing iterations are done
			whenToStore = numParse / 2;
			storePattern = false; // storePattern is switched to true in parseStrings()
			storingMultipleBars = true;
			// then we store the actual lines of the editor to a temporary vector
			// and replace the editor strings with the split bar strings, one at a time
			if (numLines > 1) {
				for (int i = 1; i < numLines-1; i++) {
					tempLines.push_back(editors[whichEditor].allStrings[i+lineNum]);
					// in the first run, we store the first bar, then we'll store the others
					editors[whichEditor].allStrings[i+lineNum] = allBars[i-1][0];
				}
			}
		}
		else if (numParse > whenToStore) {
			// assign the next bar line to the editor strings
			if (numLines > 1) {
				for (int i = 1; i < numLines-1; i++) {
					editors[whichEditor].allStrings[i+lineNum] = allBars[i-1][(whenToStore*2)-numParse];
				}
			}
		}
		else {
			// once we're done parsing to test if everything is ok
			// we start storing each bar separately with the "-x" suffix
			// where "x" is the bar number within this bar group
			// "x" is 1-based, hence +1 at the end of the line below
			patternName = commands[1] + "-" + to_string(whenToStore-numParse+1);
			pushNewPattern(patternName);
			// patternIndex is updated through the pushNewPattern() function
			// so we can safely use it in the line below
			// we push every index of the bars defined with \bars
			// and once the posCalculator is done, it clears the multipleBarsPatternIndexes vector itself
			posCalculator.multipleBarsPatternIndexes.push_back(patternIndex);
			if (numParse == whenToStore) {
				barsName = commands[1];
			}
			// assign the next bar line to the editor strings
			// this time compared to whenToStore, not whenToStore*2
			if (numLines > 1) {
				for (int i = 1; i < numLines-1; i++) {
					editors[whichEditor].allStrings[i+lineNum] = allBars[i-1][whenToStore-numParse];
				}
			}
		}
	}

	else if (commands[0].compare("change") == 0) {
		bool patternExist = false;
		string patternName;
		if (commands.size() < 2) {
			return "error: \\change command takes a bar name as an argument";
		}
		patternName = commands[1];
		for (unsigned i = 0; i < patternNames.size(); i++) {
			if (patternName.compare(patternNames[i]) == 0) {
				patternExist = true;
				patternIndex = i;
				break;
			}
		}
		if (!patternExist) {
			return "error: bar doesn't exist";
		}
		strayMelodicLine = false;
		if (numParse == 1 && !storePattern) {
			numParse = 2;
			whenToStore = numParse / 2;
			storePattern = false;
			inserting = true;
		}
		// since patterns include simple melodic lines in their definition
		// we need to know this when we parse these lines
		if (commands.size() > 2) {
			string restOfCommand = "";
			for (unsigned i = 2; i < commands.size()-1; i++) {
				restOfCommand += commands[i];
				restOfCommand += " ";
			}
			restOfCommand += commands[commands.size()-1];
			errorStr = parseString(restOfCommand, lineNum, numLines);
			if (parsingLoopPattern && errorStr.size() > 0) {
				parsingLoopPattern = false;
			}
		}
	}

	else if (commands[0].compare("fromosc") == 0) {
		bool editorIndexExists = false;
		for (unsigned i = 0; i < fromOsc.size(); i++) {
			if (whichEditor == fromOsc[i]) {
				editorIndexExists = true;
				if (commands.size() > 1) {
					fromOscAddr[i] = commands[1];
				}
				else {
					// default address is "/livelily1" for 1st editor etc.
					fromOscAddr[i] = "/livelily" + to_string(whichEditor+1);
				}
				editorIndexExists = true;
				break;
			}
		}
		if (!editorIndexExists) {
			fromOsc.push_back(whichEditor);
			if (commands.size() > 2) {
				fromOsc.pop_back();
				return "error: \\fromosc commands takes maximum one argument";
			}
			else if (commands.size() > 1) {
				if (!startsWith(commands[1], "/")) {
					fromOsc.pop_back();
					return "error: OSC address must start with /";
				}
				fromOscAddr.push_back(commands[1]);
			}
			else {
				fromOscAddr.push_back("/livelily" + to_string(whichEditor+1));
			}
		}
	}

	else if (commands[0].compare("rest") == 0) {
		bool foundInst = false;
		for (unsigned i = 0; i < seqVec.instruments.size(); i++) {
			for (unsigned j = 0; j < instrumentsPassed.size(); j++) {
				if (instrumentsPassed[j] == (int)i) {
					foundInst = true;
					break;
				}
			}
			if (foundInst) {
				foundInst = false;
				continue;
			}
			// store the pattern passed to the "\rest" command
			// for undefined instruments in the current bar
			// we need to define lastInstrumentIndex because it is used
			// in copyMelodicLine() which is called by strinLineFromPattern()
			lastInstrumentIndex = i;
			// the boolean below is necessary so a melodic line is actually copied
			typedInstrument = true;
			// once we store the current instrument, we parse the string
			// with the rest of the command, which is the bar or loop to copy from
			// so the melodic line can be copied with stringLineFromPattern()
			if (commands.size() > 1) {
				string restOfCommand = "";
				for (unsigned i = 2; i < commands.size()-1; i++) {
					restOfCommand += commands[i];
					restOfCommand += " ";
				}
				restOfCommand += commands[commands.size()-1];
				errorStr = parseString(restOfCommand, lineNum, numLines);
			}
			else {
				return "error: \\rest command takes one argument";
			}
		}
	}

	else if (commands[0].compare("all") == 0) {
		if (commands.size() < 2) {
			return "error: \\all command takes at least one argument";
		}
		string restOfCommand = "";
		for (unsigned i = 2; i < commands.size()-1; i++) {
			restOfCommand += commands[i];
			restOfCommand += " ";
		}
		restOfCommand += commands[commands.size()-1];
		for (unsigned i = 0; i < seqVec.instruments.size(); i++) {
			lastInstrumentIndex = i;
			errorStr = parseString(restOfCommand, lineNum, numLines);
			if (errorStr.size() > 0) {
				break;
			}
		}
	}

	else if (commands[0].compare("solo") == 0) {
		if (commands.size() < 2) {
			return "error: \\solo commands takes at least one argument";
		}
		vector<string> soloInsts;
		for (unsigned i = 1; i < commands.size(); i++) {
			if (find(seqVec.instruments.begin(), seqVec.instruments.end(), commands[i]) >= seqVec.instruments.end()) {
				return (string)"error: " + commands[i] + (string)": unknown instrument";
			}
			else {
				soloInsts.push_back(commands[i]);
			}
		}
		if (soloInsts.size() == seqVec.instruments.size()) {
			return "warning: no point in calling \\solo for all instruments";
		}
		// mute all instruments but the solo ones
		for (unsigned i = 0; i < seqVec.instruments.size(); i++) {
			if (find(soloInsts.begin(), soloInsts.end(), seqVec.instruments[i]) >= soloInsts.end()) {
				sequencer.toMute.push_back(i);
			}
		}
	}

	else if (commands[0].compare("mute") == 0) {
		if (commands.size() < 2) {
			return "error: \\mute command takes at least one argument";
		}
		if (commands[1].compare("all") == 0) {
			for (unsigned i = 0; i < seqVec.instruments.size(); i++) {
				sequencer.toMute.push_back(i);
			}
		}
		else {
			for (unsigned i = 1; i < commands.size(); i++) {
				if (find(seqVec.instruments.begin(), seqVec.instruments.end(), commands[i]) >= seqVec.instruments.end()) {
					return commands[i] + (string)": unknown instrument";
				}
				else {
					for (unsigned j = 0; j < seqVec.instruments.size(); j++) {
						if (commands[i].compare(seqVec.instruments[j]) == 0) {
							sequencer.toMute.push_back(j);
							break;
						}
					}
				}
			}
		}
	}

	else if (commands[0].compare("unmute") == 0) {
		if (commands.size() < 2) {
			return "error: \\unmute command takes at least one argument";
		}
		if (commands[1].compare("all") == 0) {
			for (unsigned i = 0; i < seqVec.instruments.size(); i++) {
				sequencer.toUnmute.push_back(i);
			}
		}
		else {
			for (unsigned i = 1; i < commands.size(); i++) {
				if (find(seqVec.instruments.begin(), seqVec.instruments.end(), commands[i]) >= seqVec.instruments.end()) {
					return commands[i] + (string)": unknown instrument";
				}
				else {
					for (unsigned j = 0; j < seqVec.instruments.size(); j++) {
						if (commands[i].compare(seqVec.instruments[j]) == 0) {
							sequencer.toUnmute.push_back(j);
							break;
						}
					}
				}
			}
		}
	}

	else if (commands[0].compare("mutenow") == 0) {
		if (commands.size() < 2) {
			return "error: \\mutenow command takes at least one argument";
		}
		if (commands[1].compare("all") == 0) {
			for (unsigned i = 0; i < seqVec.instruments.size(); i++) {
				seqVec.notesVec[i].mute = true;
			}
		}
		else {
			for (unsigned i = 1; i < commands.size(); i++) {
				if (find(seqVec.instruments.begin(), seqVec.instruments.end(), commands[i]) >= seqVec.instruments.end()) {
					return commands[i] + (string)": unknown instrument";
				}
				else {
					for (unsigned j = 0; j < seqVec.instruments.size(); j++) {
						if (commands[i].compare(seqVec.instruments[j]) == 0) {
							seqVec.notesVec[j].mute = true;
							break;
						}
					}
				}
			}
		}
	}

	else if (commands[0].compare("unmutenow") == 0) {
		if (commands.size() < 2) {
			return "error: \\unmutenow command takes at least one argument";
		}
		if (commands[1].compare("all") == 0) {
			for (unsigned i = 0; i < seqVec.instruments.size(); i++) {
				seqVec.notesVec[i].mute = false;
			}
		}
		else {
			for (unsigned i = 1; i < commands.size(); i++) {
				if (find(seqVec.instruments.begin(), seqVec.instruments.end(), commands[i]) >= seqVec.instruments.end()) {
					return commands[i] + (string)": unknown instrument";
				}
				else {
					for (unsigned j = 0; j < seqVec.instruments.size(); j++) {
						if (commands[i].compare(seqVec.instruments[j]) == 0) {
							seqVec.notesVec[j].mute = false;
							break;
						}
					}
				}
			}
		}
	}

	else if (commands[0].compare("function") == 0) {
		bool functionExists = false;
		if (commands.size() < 2) {
			return "error: \\function needs to be given a name";
		}
		for (unsigned i = 0; i < functionNames.size(); i++) {
			if (commands[1].compare(functionNames[i]) == 0) {
				functionExists = true;
				lastFunctionIndex = i;
				functionBodies[lastFunctionIndex].clear();
				storingFunction = true;
				break;
			}
		}
		if (!functionExists) {
			functionNames.push_back(commands[1]);
			functionBodies.resize(functionNames.size());
			storingFunction = true;
			lastFunctionIndex = (unsigned)functionNames.size()-1;
		}
		string restOfCommand = "";
		for (unsigned i = 2; i < commands.size()-1; i++) {
			restOfCommand += commands[i];
			restOfCommand += " ";
		}
		restOfCommand += commands[commands.size()-1];
		errorStr = parseString(restOfCommand, lineNum, numLines);
		if (errorStr.size() > 0) {
			storingFunction = false;
		}
	}

	else if (commands[0].compare("sendto") == 0) {
		if (commands.size() < 2) {
			return "error: \\sendto command takes at least one argument, host IP or port";
		}
		else if (commands.size() > 3) {
			return "error: \\sendto command takes maximum two arguments, host IP and port";
		}
		string host;
		int port;
		if (commands.size() > 2) {
			host = commands[1];
			port = stoi(commands[2]);
		}
		else {
			if (isDigit(commands[1])) {
				host = "localhost";
				port = stoi(commands[1]);
			}
			else {
				host = commands[1];
				port = SCOREPARTPORT;
			}
		}
		seqVec.sendsToPart[lastInstrumentIndex] = true;
		seqVec.scorePartSenders[lastInstrumentIndex].setup(host, port);
		ofxOscMessage m;
		m.setAddress("/initinsts");
		m.addStringArg(seqVec.instruments[lastInstrumentIndex]);
		seqVec.scorePartSenders[lastInstrumentIndex].sendMessage(m, false);
	}

	else if (commands[0].compare("listmidiports") == 0) {
		if (commands.size() > 1) {
			return "error: \\listmidiports command takes no arguments";
		}
		vector<string> outPorts = sequencer.midiOut.getOutPortList();
		string outPortsOneStr = "note: ";
		for (unsigned i = 0; i < outPorts.size()-1; i++) {
			outPortsOneStr += (string)"MIDI out port " += to_string(i) += (string)": " += outPorts[i] += "\n";
		}
		// append the last port separately to not include the newline
		outPortsOneStr += (string)"MIDI out port " += to_string(outPorts.size()-1) += (string)": " += outPorts.back();
		return outPortsOneStr;
	}

	else if (commands[0].compare("openmidiport") == 0) {
		if (commands.size() > 2) {
			return "error: \\openmidiport takes one argument only";
		}
		else if (commands.size() == 1) {
			return "error: no midi port number provided";
		}
		if (!isDigit(commands[1])) {
			return "error: midi port argument must be a number";
		}
		sequencer.midiOut.openPort(stoi(commands[1]));
		midiPortOpen = true;
		return (string)"note: opened MIDI port " + commands[1];;
	}

	else if (commands[0].compare("midichan")== 0) {
		if (commands.size() == 1) {
			return "error: no MIDI channel set";
		}
		if (commands.size() > 2) {
			return "error: \\midichan command takes one argument only";
		}
		if (!isDigit(commands[1])) {
			return "error: MIDI channel must be a number";
		}
		seqVec.midiChans[lastInstrumentIndex] = stoi(commands[1]);
		seqVec.isMidi[lastInstrumentIndex] = true;
	}

	else if (commands[0].compare("mididur") == 0) {
		if (commands.size() == 1) {
			return "error: no MIDI duration set";
		}
		if (commands.size() > 2) {
			return "error: \\mididur command takes one argument only";
		}
		if (!isDigit(commands[1])) {
			return "error: MIDI duration must be a number";
		}
		seqVec.midiDurs[lastInstrumentIndex] = (float)stoi(commands[1])/10.0;
	}

	else if (commands[0].compare("midistaccato") == 0) {
		if (commands.size() == 1) {
			return "error: no percentage value provided";
		}
		if (commands.size() > 2) {
			return "error: \\midistaccato command takes one argument only";
		}
		if (!isDigit(commands[1])) {
			return "error: staccato percentage must be a number";
		}
		int stacPercent = stoi(commands[1]);
		if (stacPercent < 0) {
			return "error: staccato percentage can't be below 0";
		}
		if (stacPercent == 0) {
			return "error: staccato percentage can't be 0";
		}
		if (stacPercent > 100) {
			return "error: staccato percentage can't be over 100%";
		}
		seqVec.midiStaccato[lastInstrumentIndex] = stacPercent;
		if (stacPercent == 100) {
			return "warning: staccato set to 100% of Note On duration";
		}
	}

	else if (commands[0].compare("midistaccatissimo") == 0) {
		if (commands.size() == 1) {
			return "error: no percentage value provided";
		}
		if (commands.size() > 2) {
			return "error: \\midistaccatissimo command takes one argument only";
		}
		if (!isDigit(commands[1])) {
			return "error: staccatissimo percentage must be a number";
		}
		int stacPercent = stoi(commands[1]);
		if (stacPercent < 0) {
			return "error: staccatissimo percentage can't be below 0";
		}
		if (stacPercent == 0) {
			return "error: staccatissimo percentage can't be 0";
		}
		if (stacPercent > 100) {
			return "error: staccatissimo percentage can't be over 100%";
		}
		seqVec.midiStaccatissimo[lastInstrumentIndex] = stacPercent;
		if (stacPercent == 100) {
			return "warning: staccatissimo set to 100% of Note On duration";
		}
	}

	else if (commands[0].compare("miditenuto") == 0) {
		if (commands.size() == 1) {
			return "error: no percentage value provided";
		}
		if (commands.size() > 2) {
			return "error: \\miditenuto command takes one argument only";
		}
		if (!isDigit(commands[1])) {
			return "error: tenuto percentage must be a number";
		}
		int tenutoPercent = stoi(commands[1]);
		if (tenutoPercent < 0) {
			return "error: tenuto percentage can't be below 0";
		}
		if (tenutoPercent == 0) {
			return "error: tenuto percentage can't be 0";
		}
		if (tenutoPercent > 100) {
			return "error: tenuto percentage can't be over 100%";
		}
		seqVec.midiTenuto[lastInstrumentIndex] = tenutoPercent;
		if (tenutoPercent == 100) {
			return "warning: tenuto set to 100% of Note On duration";
		}
	}

	else if (commands[0].compare("vec") == 0) {
		printVector(seqVec.midiDursWithoutSlurs[lastInstrumentIndex][stoi(commands[1])]);
	}

	else {
		bool instrumentExists = false;
		bool patternLoopExists = false;
		bool functionExists = false;
		for (unsigned i = 0; i < seqVec.instruments.size(); i++) {
			if (commands[0].compare(seqVec.instruments[i]) == 0) {
				instrumentExists = true;
				lastInstrument = commands[0];
				lastInstrumentIndex = i;
				break;
			}
		}
		if (!instrumentExists) {
			for (unsigned i = 0; i < patternLoopNames.size(); i++) {
				if (commands[0].compare(patternLoopNames[i]) == 0) {
					patternLoopExists = true;
					seqVec.tempPatternLoopIndex = i; // seqVec.patternLoopIndexes[i][0]; // i;
					break;
				}
			}
		}
		if (!instrumentExists && !patternLoopExists) {
			for (unsigned i = 0; i < functionNames.size(); i++) {
				if (commands[0].compare(functionNames[i]) == 0) {
					functionExists = true;
					lastFunctionIndex = i;
					if (commands.size() > 1) {
						return "error: calling a function takes no arguments";
					}
					break;
				}
			}
		}
		if (!instrumentExists && !patternLoopExists && !functionExists) {
			return (string)"error: " + commands[0] + (string)": unknown command";
		}
		else if (patternLoopExists) {
			seqVec.updatePatternLoop = true;
			if (sequencer.isThreadRunning()) {
				sequencer.runSequencer = true;
			}
			else {
				seqVec.patternLoopIndex = seqVec.tempPatternLoopIndex;
				seqVec.currentScoreIndex = seqVec.patternLoopIndexes[seqVec.patternLoopIndex][0];
				ofxOscMessage m;
				m.setAddress("/loopndx");
				m.addIntArg(seqVec.patternLoopIndex);
				m.addIntArg(seqVec.currentScoreIndex);
				sendToParts(m);
			}
		}
		else if (instrumentExists) {
			// boolean needed if we're retrieving a melodic line from a previous pattern
			typedInstrument = true;
			// the melodic line or another command of an instrument
			// can be in the same line with the instrument
			if (commands.size() > 1) {
				errorStr = parseString(str.substr(str.find(" ")+1), lineNum, numLines);
				// if we get an error in a melodic line, the traceback has to be set
				// from there, instead of the command parsing
				// all this happens in parseString()
				if (errorStr.size() > 0) parsingCommand = false;
			}
		}
		else if (functionExists) {
			for (unsigned i = 0; i < functionBodies[lastFunctionIndex].size(); i++) {
				errorStr = parseString(functionBodies[lastFunctionIndex][i], lineNum, numLines);
				if (errorStr.size() > 0) {
					break;
				}
			}
		}
	}

	return errorStr;
}

//--------------------------------------------------------------
string ofApp::stripLineFromPattern(string str){
	string patternName = str;
	bool patternLoopExists = false;
	// the following boolean must be true so that patterns are stored
	// even if all instruments strip lines from existing bars
	parsedMelody = true;
	int whichPattern = 0;
	for (unsigned i = 0; i < patternLoopNames.size(); i++) {
		if (patternName.compare(patternLoopNames[i]) == 0) {
			patternLoopExists = true;
			whichPattern = i;
			break;
		}
	}
	if (!patternLoopExists) {
		return "error: pattern doesn't exist";
	}
	if (storePattern) {
		for (unsigned i = 0; i < seqVec.patternLoopIndexes[whichPattern].size(); i++) {
			int bar = seqVec.patternData[seqVec.patternLoopIndexes[whichPattern][i]][0];
			// copy the number of beats from the pattern we strip the line
			seqVec.tempNumBeats = seqVec.patternData[seqVec.patternLoopIndexes[whichPattern][i]][1];
			copyMelodicLine(bar);
		}
		seqVec.patternData[patternIndex].push_back(lastInstrumentIndex);
		instrumentsPassed.push_back(lastInstrumentIndex);
	}
	return "";
}

//--------------------------------------------------------------
void ofApp::copyMelodicLine(int bar){
	if (storePattern) {
		seqVec.notes[lastInstrumentIndex].push_back(seqVec.notes[lastInstrumentIndex][bar]);
		seqVec.midiNotes[lastInstrumentIndex].push_back(seqVec.midiNotes[lastInstrumentIndex][bar]);
		seqVec.durs[lastInstrumentIndex].push_back(seqVec.durs[lastInstrumentIndex][bar]);
		seqVec.dursWithoutSlurs[lastInstrumentIndex].push_back(seqVec.dursWithoutSlurs[lastInstrumentIndex][bar]);
		seqVec.midiDursWithoutSlurs[lastInstrumentIndex].push_back(seqVec.midiDursWithoutSlurs[lastInstrumentIndex][bar]);
		seqVec.pitchBendVals[lastInstrumentIndex].push_back(seqVec.pitchBendVals[lastInstrumentIndex][bar]);
		seqVec.dynamics[lastInstrumentIndex].push_back(seqVec.dynamics[lastInstrumentIndex][bar]);
		seqVec.midiVels[lastInstrumentIndex].push_back(seqVec.midiVels[lastInstrumentIndex][bar]);
		seqVec.dynamicsRamps[lastInstrumentIndex].push_back(seqVec.dynamicsRamps[lastInstrumentIndex][bar]);
		seqVec.glissandi[lastInstrumentIndex].push_back(seqVec.glissandi[lastInstrumentIndex][bar]);
		seqVec.midiGlissDurs[lastInstrumentIndex].push_back(seqVec.midiGlissDurs[lastInstrumentIndex][bar]);
		seqVec.midiDynamicsRampDurs[lastInstrumentIndex].push_back(seqVec.midiDynamicsRampDurs[lastInstrumentIndex][bar]);
		seqVec.articulations[lastInstrumentIndex].push_back(seqVec.articulations[lastInstrumentIndex][bar]);
		seqVec.midiArticulationVals[lastInstrumentIndex].push_back(seqVec.midiArticulationVals[lastInstrumentIndex][bar]);
		seqVec.text[lastInstrumentIndex].push_back(seqVec.text[lastInstrumentIndex][bar]);
		seqVec.textIndexes[lastInstrumentIndex].push_back(seqVec.textIndexes[lastInstrumentIndex][bar]);
		slurBeginnings[lastInstrumentIndex].push_back(slurBeginnings[lastInstrumentIndex][bar]);
		slurEndings[lastInstrumentIndex].push_back(slurEndings[lastInstrumentIndex][bar]);
		scoreNotes[lastInstrumentIndex].push_back(scoreNotes[lastInstrumentIndex][bar]);
		scoreDurs[lastInstrumentIndex].push_back(scoreDurs[lastInstrumentIndex][bar]);
		scoreDotIndexes[lastInstrumentIndex].push_back(scoreDotIndexes[lastInstrumentIndex][bar]);
		scoreAccidentals[lastInstrumentIndex].push_back(scoreAccidentals[lastInstrumentIndex][bar]);
		scoreOctaves[lastInstrumentIndex].push_back(scoreOctaves[lastInstrumentIndex][bar]);
		scoreGlissandi[lastInstrumentIndex].push_back(scoreGlissandi[lastInstrumentIndex][bar]);
		scoreArticulations[lastInstrumentIndex].push_back(scoreArticulations[lastInstrumentIndex][bar]);
		scoreDynamics[lastInstrumentIndex].push_back(scoreDynamics[lastInstrumentIndex][bar]);
		scoreDynamicsIndexes[lastInstrumentIndex].push_back(scoreDynamicsIndexes[lastInstrumentIndex][bar]);
		scoreDynamicsRampStart[lastInstrumentIndex].push_back(scoreDynamicsRampStart[lastInstrumentIndex][bar]);
		scoreDynamicsRampEnd[lastInstrumentIndex].push_back(scoreDynamicsRampEnd[lastInstrumentIndex][bar]);
		scoreDynamicsRampDir[lastInstrumentIndex].push_back(scoreDynamicsRampDir[lastInstrumentIndex][bar]);
		scoreSlurBeginnings[lastInstrumentIndex].push_back(scoreSlurBeginnings[lastInstrumentIndex][bar]);
		scoreSlurEndings[lastInstrumentIndex].push_back(scoreSlurEndings[lastInstrumentIndex][bar]);
		scoreTexts[lastInstrumentIndex].push_back(scoreTexts[lastInstrumentIndex][bar]);
	}
}

//--------------------------------------------------------------
string ofApp::parsePatternLoop(string str, int lineNum, int numLines){
	// first check if there is anything after the closing bracket
	size_t closingBracketIndex = str.find("}");
	string restOfCommand = "";
	// storing anything after a closing bracket enables us
	// to call a loop in the same line we define it, like this
	// \loop name {bar1 bar2} \name
	if (closingBracketIndex != string::npos) {
		restOfCommand = str.substr(closingBracketIndex);
		str = str.substr(0, closingBracketIndex);
	}
	vector<string> names = tokenizeString(str, " ");
	for (unsigned i = 0; i < names.size(); i++) {
		bool foundPatternLoop = false;
		size_t multIndex = names[i].find("*");
		string thisPatternLoop;
		int howManyTimes = 1;
		if (multIndex != string::npos) {
			thisPatternLoop = names[i].substr(0, multIndex);
			if (!isDigit(names[i].substr(multIndex+1))) {
				return "error: repetition coefficient not an int";
			}
			howManyTimes = stoi(names[i].substr(multIndex+1));
		}
		else {
			thisPatternLoop = names[i];
		}
		// first check for patternLoops because when we define a pattern with data
		// and not with combinations of other patterns, we create a patternLoop with the same name
		for (unsigned j = 0; j < patternLoopNames.size(); j++) {
			if (thisPatternLoop.compare(patternLoopNames[j]) == 0) {
				for (int k = 0; k < howManyTimes; k++) {
					for (unsigned l = 0; l < seqVec.patternLoopIndexes[j].size(); l++) {
						seqVec.patternLoopIndexes[seqVec.tempPatternLoopIndex].push_back(seqVec.patternLoopIndexes[j][l]);
					}
				}
				foundPatternLoop = true;
				break;
			}
		}
		if (!foundPatternLoop) {
			parsingCommand = false;
			return "error: pattern doesn't exist";
		}
	}
	if (restOfCommand.size() > 0) {
		parseString(restOfCommand, lineNum, numLines);
	}
	return "";
}

//--------------------------------------------------------------
string ofApp::parseMelodicLine(string str){
	if (strayMelodicLine) {
		return "error: stray melodic line";
	}
	string strToProcess = detectRepetitions(str);
	bool dynAtFirstNote = false;
	static int tempDur = 0;
	// the variable below will store an incrementing value
	// when we get a note and pass that to durIndexes
	// when we get a duration
	// this is necessary because repeated durations can be
	// omitted in Lilypond notation, but it's easier to store
	// them all here so the sequencer can work properly
	// e.g. c4' d' e8' f' g4' is valid Lilypond notation
	int noteIndex = 0;
	vector<int> durIndexes;
	vector<int> dynamicsIndexes;
	vector<vector<float>> notesData;
	vector<vector<int>> midiNotesData;
	vector<int> midiVels;
	// the following array is used to map the dynamic values
	// to the corresponding MIDI velocity values
	int dynsArr[8] = {-9, -6, -3, -1, 1, 3, 6, 9};
	// an array to test against durations
	int dursArr[7] = {1, 2, 4, 8, 16, 32, 64};
	vector<int> midiPitchBendVals;
	vector<int> dursData;
	vector<int> dursDataWithoutSlurs;
	vector<int> midiDursDataWithoutSlurs;
	vector<int> pitchBendVals;
	vector<int> dotIndexes;
	vector<int> dynamicsData;
	vector<int> dynamicsRampData;
	vector<int> slurBeginningsIndexes;
	vector<int> slurEndingsIndexes;
	vector<int> textIndexesLocal;
	vector<string> texts;
	bool mezzo = false;
	vector<vector<int>> notesForScore;
	vector<int> dursForScore;
	vector<vector<int>> accidentalsForScore;
	vector<vector<int>> octavesForScore;
	vector<int> dynamicsForScore;
	vector<int> dynamicsForScoreIndexes;
	vector<int> dynamicsRampStart;
	vector<int> dynamicsRampEnd;
	vector<int> midiDynamicsRampDurs;
	vector<int> glissandiIndexes;
	vector<int> midiGlissDurs;
	vector<int> articulationIndexes;
	vector<int> midiArticulationVals;
	vector<int> dynamicsRampStartForScore;
	vector<int> dynamicsRampEndForScore;
	vector<int> dynamicsRampDirForScore;
	vector<int> slurBeginningsIndexesForScore;
	vector<int> slurEndingsIndexesForScore;
	vector<string> textsForScore;
	bool dynamicsRampStarted = false;
	// boolean to notify if there is a duration at the end
	// of the string
	bool durAtEndOfString = false;
	// booleans to determine whether we're writing a chord or not
	bool chordStarted = false;
	bool firstChordNote = false;
	// index that counts for a new note or group of notes
	// necessary as chords are white space separated and tokenized below
	// so we need an index that will not increment within chords
	// start from -1 because we increment immediately
	int ndx = -1;
	vector<string> tokens = tokenizeString(strToProcess, " ");
	// first detect if there are any quotes, which might include one or more white spaces
	for (unsigned i = 0; i < tokens.size(); i++) {
		// first determine if we have both quotes in the same token
		vector<int> quoteNdx = findIndexesOfCharInStr(tokens[i], "\"");
		if (quoteNdx.size() > 1) {
			continue;
		}
		// if we find a quote and we're not at the last token
		if (tokens[i].find("\"") != string::npos && i < (tokens.size()-1)) {
			int ndx = i+1;
			while (ndx < tokens.size()) {
				// if we find the pairing quote, concatenate the strings into one token
				if (tokens[ndx].find("\"") != string::npos) {
					tokens[i] += (" " + tokens[ndx]);
					tokens.erase(tokens.begin()+ndx);
					break;
				}
			}
		}
	}
	// then iterate over all the tokes and parse them
	for (unsigned i = 0; i < tokens.size(); i++) {
		bool foundNote = false;
		bool foundDynamic = false;
		bool foundAccidental = false;
		bool foundOctave = false;
		bool foundArticulation = false;
		float accidental = 0.0;
		int octaveCounter = 0;
		unsigned firstArticulIndex = 0;
		// variable to determine which character we start looking at
		// useful in case we start a bar with a chord
		int firstChar = 0;
		// first check if the token is a comment so we exit the loop
		if (startsWith(tokens[i], "%")) continue;
		// the first element of each token is the note
		// so we first check for it in a separate loops
		// and then for the rest of the stuff
	parseStart:
		for (int j = 0; j < 8; j++) {
			if (tokens[i][firstChar] == noteChars[j]) {
				int midiNote = -1;
				int naturalScaleNote;
				if (j < 7) {
					midiNote = j * 2;
					// after D we have a semitone so we must subtract 1
					if (j > 2) midiNote -= 1;
					// we start one octave below the middle C
					midiNote += 48;
					naturalScaleNote = j;
				}
				else {
					// the last element of the noteChars array is the rest
					midiNote = -1;
					naturalScaleNote = -1;
				}
				if (!chordStarted || (chordStarted && firstChordNote)) {
					ndx++; // increment ndx only for each new group of notes
					notesData.push_back({(float)midiNote});
					midiNotesData.push_back({midiNote});
					notesForScore.push_back({naturalScaleNote});
					// store a zero which will be overriden in case of an accidental
					accidentalsForScore.push_back({0});
					// do the same for the octaves which will be calculated later
					octavesForScore.push_back({0});
					dotIndexes.push_back(0);
					glissandiIndexes.push_back(0);
					midiGlissDurs.push_back(0);
					midiDynamicsRampDurs.push_back(0);
					articulationIndexes.push_back(0);
					midiArticulationVals.push_back(1);
					pitchBendVals.push_back(0);
					textIndexesLocal.push_back(0);
					texts.push_back("");
					noteIndex++;
				}
				else if (chordStarted && !firstChordNote) {
					notesData.back().push_back((float)midiNote);
					midiNotesData.back().push_back(midiNote);
					notesForScore.back().push_back(naturalScaleNote);
					accidentalsForScore.back().push_back(0);
					octavesForScore.back().push_back(0);
				}
				else {
					return "error: something went wrong";
				}
				foundNote = true;
				break;
			}
		}
		if (!foundNote) {
			if (tokens[i][firstChar] == char(60)) { // <
				chordStarted = true;
				firstChordNote = true;
				firstChar = 1;
				goto parseStart; // reiterate if starting with a chord symbol
			}
			else {
				return "error: first character must be a note";
			}
		}
		// then we have accidentals, if any
		unsigned accIndex = firstChar + 1;
		while (accIndex < tokens[i].size()) {
			if (tokens[i][accIndex] == char(101)) { // 101 is "e"
				if (accIndex < tokens[i].size()-1) {
					// if the character after "e" is "s" or "h" we have an accidental
					if (tokens[i][accIndex+1] == char(115)) { // 115 is "s"
						accidental += -1.0; // in which case we subtract one semitone
						foundAccidental = true;
					}
					else if (tokens[i][accIndex+1] == char(104)) { // 104 is "h"
						accidental += -0.5;
						foundAccidental = true;
					}
					else {
						return (string)"error: " + tokens[i][accIndex+1] + (string)": unknown accidental character";
					}
				}
				else {
					return "error: \"e\" must be followed by \"s\" or \"h\"";
				}
			}
			else if (tokens[i][accIndex] == char(105)) { // 105 is "i"
				if (accIndex < tokens[i].size()-1) {
					if (tokens[i][accIndex+1] == char(115)) { // 115 is "s"
						accidental += 1.0; // in which case we add one semitone
						foundAccidental = true;
					}
					else if (tokens[i][accIndex+1] == char(104)) { // 104 is "h"
						accidental += 0.5;
						foundAccidental = true;
					}
					else {
						return (string)"error: " + tokens[i][accIndex+1] + (string)": unknown accidental character";
					}
				}
				else {
					return "error: \"i\" must be followed by \"s\" or \"h\"";
				}
			}
			// we ignore "s" and "h" as we have checked them above
			else if (tokens[i][accIndex] == char(115) || tokens[i][accIndex] == char(104)) {
				accIndex++;
				continue;
			}
			// when the accidentals characters are over we move on
			else {
				break;
			}
			accIndex++;
		}
		if (foundAccidental) {
			notesData.back().back() += accidental;
			midiNotesData.back().back() += (int)accidental;
			// accidentals can go up to a whole tone, which is accidental=2.0
			// but in case it's one and a half tone, one tome is already added above
			// so we need the half tone only, which we get with accidental-(int)accidental
			pitchBendVals.back() = (int)((accidental-(int)accidental) * 8191);
			// store accidentals in the following order with the following indexes
			// 0 - double flat, 1 - flat, 2 - half flat,
			// 3 - natural, 4 - half sharp, 5 - sharp, 6 - double sharp
			// these indexes are achieved by incrementing with += below
			int accidentalSymIdx = (int)accidental;
			if (accidental < 0.0) accidentalSymIdx--;
			else if (accidental > 0.0) accidentalSymIdx++;
			accidentalsForScore.back().back() += accidentalSymIdx;
			foundAccidental = false;
			accIndex--; // go back one character
		}
		// now check for the rest of the characters of the token, so start with j = accIndex
		for (unsigned j = accIndex; j < tokens[i].size(); j++) {
			if (tokens[i][j] == char(92)) { // back slash
				unsigned index = j;
				int dynamic = 0;
				bool foundDynamicLocal = false;
				bool foundGlissandoLocal = false;
				if (index > (tokens[i].size()-1)) {
					return "error: a backslash must be followed by a command";
				}
				// loop till you find all dynamics or other commands
				while (index < tokens[i].size()) {
					if (tokens[i][index+1] == char(102)) { // f for forte
						if (mezzo) {
							dynamic++;
							mezzo = false;
						}
						else {
							dynamic += 3;
						}
						dynamicsIndexes.push_back(noteIndex-1);
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							// subtract one because noteIndex increments every time we get a note
							dynamicsRampEnd.push_back(noteIndex-1);
							dynamicsRampEndForScore.push_back(noteIndex-1);
						}
						foundDynamic = true;
						foundDynamicLocal = true;
					}
					else if (tokens[i][index+1] == char(112)) { // p for piano
						if (mezzo) {
							dynamic--;
							mezzo = false;
						}
						else {
							dynamic -= 3;
						}
						dynamicsIndexes.push_back(noteIndex-1);
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							// subtract one because noteIndex increments every time we get a note
							dynamicsRampEnd.push_back(noteIndex-1);
							dynamicsRampEndForScore.push_back(noteIndex-1);
						}
						foundDynamic = true;
						foundDynamicLocal = true;
					}
					else if (tokens[i][index+1] == char(109)) { // m for mezzo
						mezzo = true;
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							// subtract one because noteIndex increments every time we get a note
							dynamicsRampEnd.push_back(noteIndex-1);
							dynamicsRampEndForScore.push_back(noteIndex-1);
						}
					}
					else if (tokens[i][index+1] == char(60)) { // <
						if (!dynamicsRampStarted) {
							dynamicsRampStarted = true;
							// subtract one because noteIndex increments every time we get a note
							dynamicsRampStart.push_back(noteIndex-1);
							dynamicsRampStartForScore.push_back(noteIndex-1);
							dynamicsRampDirForScore.push_back(1);
						}
					}
					else if (tokens[i][index+1] == char(62)) { // >
						if (!dynamicsRampStarted) {
							dynamicsRampStarted = true;
							// subtract one because noteIndex increments every time we get a note
							dynamicsRampStart.push_back(noteIndex-1);
							dynamicsRampStartForScore.push_back(noteIndex-1);
							dynamicsRampDirForScore.push_back(0);
						}
					}
					else if (tokens[i][index+1] == char(33)) { // exclamation mark
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							// subtract one because noteIndex increments every time we get a note
							dynamicsRampEnd.push_back(noteIndex-1);
							dynamicsRampEndForScore.push_back(noteIndex-1);
						}
					}
					else if (tokens[i][index+1] == char(103)) { // g for gliss
						if ((tokens[i].substr(index+1,5).compare("gliss") == 0) ||
							(tokens[i].substr(index+1,9).compare("glissando") == 0)) {
							glissandiIndexes[noteIndex-1] = 1;
							foundGlissandoLocal = true;
						}
					}
					else {
						if (foundDynamicLocal) {
							// seqVec.dynamics are stored like this:
							// ppp = -9, pp = -6, p = -3, mp = -1, mf = 1, f = 3, ff = 6, fff = 9
							// so to map this range to 60-100 (dB) we must first add 9 (subtract -9)
							// multiply by 40 (100 - 60) / (9 - (-9)) = 40 / 18
							// and add 60 (the lowest value)
							if (dynamic < -9) dynamic = -9;
							else if (dynamic > 9) dynamic = 9;
							int scaledDynamic = (int)((float)(dynamic + 9) * ((100.0-(float)MINDB) / 18.0)) + MINDB;
							dynamicsData.push_back(scaledDynamic);
							// to get the dynamics in MIDI velocity values, we map ppp to 16, and fff to 127
							// based on this website https://www.hedsound.com/p/midi-velocity-db-dynamics-db-and.html
							// first find the index of the dynamic value in the array {-9, -6, -3, -1, 1, 3, 6, 9}
							int midiVelIndex = std::distance(dynsArr, std::find(dynsArr, dynsArr+8, dynamic));
							midiVels.push_back((midiVelIndex+1)*16);
							if (midiVels.back() > 127) midiVels.back() = 127;
						}
						// if we haven't found a dynamic and the ramp vectors are empty
						// it means that we have received some unknown character
						else if ((dynamicsRampStart.size() == 0) && !foundGlissandoLocal) {
							return (string)"error: " + tokens[i][index+1] + (string)": unknown character";
						}
						break;
					}
					index++;
				}
				j = index;
				continue;
			}

			else if (tokens[i][j] == char(45)) { // - for articulation symbols
				if (j > (tokens[i].size()-1)) {
					return "error: a hyphen must be followed by an articulation symbol";
				}
				if (!foundArticulation) {
					firstArticulIndex = j;
				}
				// since the hyphen is used both for indicating the command for articulation
				// but also for the tenuto symbol, we have to iterate through these symbols
				// every two, otherwise, if we write a tenuto, the else if (tokens[i][j] == char(45)) test
				// will be successful not only for the articulation command, but also for the symbol
				else if ((j - firstArticulIndex) < 2) {
					continue;
				}
				foundArticulation = true;
				int articulChar = char(tokens[i][j+1]);
				switch (articulChar) {
					// articulation symbol indexes start from 1 because 0 is reserved for no articulation
					case 94: // ^ for marcato
						articulationIndexes[noteIndex-1] = 1;
						midiArticulationVals[noteIndex-1] = 10;
						break;
					case 43: // + for trill
						articulationIndexes[noteIndex-1] = 2;
						midiArticulationVals[noteIndex-1] = 11;
						break;
					case 45: // - for tenuto
						articulationIndexes[noteIndex-1] = 3;
						midiArticulationVals[noteIndex-1] = 12;
						break;
					case 33: // ! for staccatissimo
						articulationIndexes[noteIndex-1] = 4;
						midiArticulationVals[noteIndex-1] = 13;
						break;
					case 62: // > for accent
						articulationIndexes[noteIndex-1] = 5;
						midiArticulationVals[noteIndex-1] = 14;
						break;
					case 46: // . for staccato
						articulationIndexes[noteIndex-1] = 6;
						midiArticulationVals[noteIndex-1] = 15;
						break;
					case 95: // _ for portando
						articulationIndexes[noteIndex-1] = 7;
						midiArticulationVals[noteIndex-1] = 16;
						break;
					default:
						return "error: unknown articulation symbol";
				}
				j++;
				continue;
			}

			// ^ or _ for adding text above or below the note
			else if ((tokens[i][j] == char(94)) || (tokens[i][j] == char(95))) {
				if (j > (tokens[i].size()-1)) {
					if (tokens[i][j] == char(94)) {
						return "error: a carret must be followed by text in quotes";
					}
					else {
						return "error: an undescore must be followed by text in quotes";
					}
				}
				if (tokens[i][j+1] != char(34)) { // "
					if (tokens[i][j] == char(94)) {
						return "error: a carret must be followed by a bouble quote sign";
					}
					else {
						return "error: an underscore must be followed by a bouble quote sign";
					}
				}
				int openQuote = j+2;
				int index = j+2;
				int closeQuote = j+2;
				while (index < (int)tokens[i].size()) {
					if (tokens[i][index] == char(34)) {
						closeQuote = index;
						break;
					}
					index++;
				}
				if (closeQuote <= openQuote) {
					return "error: text must be between two double quote signs";
				}
				// use ndx because it doesn't increment within a chord
				texts[ndx] = tokens[i].substr(openQuote, closeQuote-openQuote);
				textsForScore.push_back(texts[ndx]);
				//textsForScore.push_back(texts[i]);
				if (tokens[i][j] == char(94)) {
					textIndexesLocal[ndx] = 1;
				}
				else if (tokens[i][j] == char(95)) {
					textIndexesLocal[ndx] = -1;
				}
				j = closeQuote;
			}

			else if (tokens[i][j] == char(40)) { // ( for beginning of slur
				slurBeginningsIndexes.push_back(noteIndex-1);
				// separate vector for score so we can push back a -1
				// in case of no slurs
				slurBeginningsIndexesForScore.push_back(noteIndex-1);
			}
			else if (tokens[i][j] == char(41)) { // ) for end of slur
				slurEndingsIndexes.push_back(noteIndex-1);
				slurEndingsIndexesForScore.push_back(noteIndex-1);
			}

			else if (tokens[i][j] == char(46)) { // . for dotted note
				dotIndexes.back() = 1;
			}

			else if (tokens[i][j] == char(62)) { // > for chord end
				chordStarted = false;
			}

			else {
				if (isdigit(tokens[i][j])) {
					tempDur = tempDur * 10 + int(tokens[i][j]) - 48;
					if (noteIndex == 1) {
						dynAtFirstNote = true;
					}
					else if (i == tokens.size()-1) {
						// because of the parent if of this if
						// the else below won't run if this duration is at the
						// end of the string, therefore we need to know this
						durAtEndOfString = true;
					}
					if ((noteIndex > 1) && !dynAtFirstNote) {
						return "error: first note doesn't have a duration";
					}
				}

				else {
					if (int(tokens[i][j]) == 39) {
						notesData.back().back() += 12.0;
						midiNotesData.back().back() += 12;
						octavesForScore.back().back()++;
						foundOctave = true;
					}

					else if (int(tokens[i][j]) == 44) {
						notesData.back().back() -= 12.0;
						midiNotesData.back().back() -= 12;
						octavesForScore.back().back()--;
						foundOctave = true;
					}
				}
			}
			if (!foundNote && !foundDynamic && !foundAccidental && \
				!foundOctave && !dynAtFirstNote) {
				return (string)"error: " + tokens[i][j] + (string)": unknown character";
			}
		}
		// store durations at the end of each token only if tempDur is greater than 0
		if (tempDur > 0) {
			if (std::find(std::begin(dursArr), std::end(dursArr), tempDur) == std::end(dursArr)) {
				return (string)"error: " + to_string(tempDur) + (string)" is not a valid duration";
			}
			dursData.push_back(tempDur);
			// subtract one because noteIndex increments every time we get a note
			durIndexes.push_back(noteIndex-1);
			//if (tempDur > minDurLocal) minDurLocal = tempDur;
			tempDur = 0;
		}
		// if we have started a chord, check whether we must falsify the first chord note
		if (chordStarted && firstChordNote) firstChordNote = false;
	}

	// in case there's a duration at the end of the string
	// we store it here
	if (durAtEndOfString) {
		// store durations only if tempDur is greater than 0
		if (tempDur > 0) {
			if (std::find(std::begin(dursArr), std::end(dursArr), tempDur) == std::end(dursArr)) {
				return (string)"error: " + to_string(tempDur) + (string)" is not a valid duration";
			}
			dursData.push_back(tempDur);
			// subtract one because noteIndex increments every time we get a note
			durIndexes.push_back(noteIndex-1);
			tempDur = 0;
		}
	}
	if (dursData.size() == 0) {
		return "error: no durations added";
	}

	// correct the accidentals
	for (unsigned i = 0; i < accidentalsForScore.size(); i++) {
		for (unsigned j = 0; j < accidentalsForScore[i].size(); j++) {
			accidentalsForScore[i][j] += 4;
		}
	}

	// fill in possible empty slots of durations
	bool foundDur = false;
	for (int i = 0; i < noteIndex; i++) {
		for (unsigned j = 0; j < durIndexes.size(); j++) {
			if (i == durIndexes[j]) {
				foundDur = true;
				break;
			}
		}
		// if a duration has not been stored, fill in the last
		// stored duration in this empty slot
		if (!foundDur) {
			dursData.insert(dursData.begin()+i, dursData[i-1]);
		}
		else {
			foundDur = false;
		}
	}
	// fill in possible empty slots of dynamics
	bool foundDyn = false;
	if (dynamicsData.size() == 0) {
		for (int i = 0; i < noteIndex; i++) {
			// if this is not the very first pattern get the last value of the previous pattern
			if (seqVec.patternData.size() > 1) {
				int instIndex = 0;
				for (unsigned i = 0; i < seqVec.instruments.size(); i++) {
					if (lastInstrument.compare(seqVec.instruments[i]) == 0) {
						instIndex = i;
						break;
					}
				}
				dynamicsData.push_back(seqVec.dynamics[instIndex].back().back());
				// for the MIDI velocities, get the prelast element, as the last one is always 0
				midiVels.push_back(seqVec.midiVels[instIndex].back().end()[-2]);
			}
			else {
				// fill in a default value of halfway from min to max dB
				dynamicsData.push_back(100-((100-MINDB)/2));
				midiVels.push_back(72); // half way between mp and mf in MIDI velocity
			}
		}
		// -1 will be filtered inside each Notes object
		// we only store one value, its enough
		dynamicsForScore.push_back(-1);
		dynamicsForScoreIndexes.push_back(-1);
	}
	else {
		for (int i = 0; i < noteIndex; i++) {
			for (unsigned j = 0; j < dynamicsIndexes.size(); j++) {
				if (i == dynamicsIndexes[j]) {
					// convert the range MINDB to 100 to indexes from 0 to 7
					// subtract the minimum dB value from the dynamic, multipled by (7/dB-range)
					// add 0.5 and get the int to round the float properly
					int thisDynamic = (int)((((float)dynamicsData[i] - \
									  (float)MINDB) * (7.0 / (100.0-(float)MINDB))) + 0.5);
					// check if a dynamic is already there, and test if it is the same
					if (dynamicsForScore.size() > 0) {
						// add the new dynamic only if it is different
						if (dynamicsForScore.back() != thisDynamic) {
							dynamicsForScoreIndexes.push_back(i);
							dynamicsForScore.push_back(thisDynamic);
							foundDyn = true;
						}
					}
					else {
						dynamicsForScoreIndexes.push_back(i);
						dynamicsForScore.push_back(thisDynamic);
						foundDyn = true;
					}
				}
				if (foundDyn) break;
			}
			// if a dynamic has not been stored, fill in the last
			// stored dynamic in this empty slot
			if (!foundDyn) {
				// we don't want to add data for the score, only for the sequencer
				dynamicsData.insert(dynamicsData.begin()+i, dynamicsData[i-1]);
				midiVels.insert(midiVels.begin()+i, midiVels[i-1]);
			}
			else {
				foundDyn = false;
			}
		}
	}
	// correct dynamics in case of crescendi or decrescendi
	for (unsigned i = 0; i < dynamicsRampStart.size(); i++) {
		// a dynamic ramp start MUST be followd by a dynamic ramp end
		int stepsDiff = dynamicsRampEnd[i] - dynamicsRampStart[i];
		int valsDiff = dynamicsData[dynamicsRampEnd[i]] - dynamicsData[dynamicsRampStart[i]];
		int midiVelsDiff = midiVels[dynamicsRampEnd[i]] - midiVels[dynamicsRampStart[i]];
		int step = valsDiff / stepsDiff;
		int midiVelsStep = midiVelsDiff / stepsDiff;
		int index = 1;
		for (int j = dynamicsRampStart[i]+1; j < dynamicsRampEnd[i]; j++) {
			dynamicsData[j] += (step * index);
			midiVels[j] += (midiVelsStep * index);
			index++;
		}
	}
	// store the next dynamic in case of the dynamics ramps
	// first fill in all values with 0s in case we don't have any ramps
	for (int i = 0; i < noteIndex; i++) {
		dynamicsRampData.push_back(0);
	}
	// then replace all values within a ramp with the dynamic value of the next index
	for (unsigned i = 0; i < dynamicsRampStart.size(); i++) {
		for (int j = dynamicsRampStart[i]; j < dynamicsRampEnd[i]; j++) {
			// if a ramp ends at the end of the bar without setting a new dynamic
			// go back to the pre-last dynamic
			if (j == (int)notesData.size()-1) {
				// if there's only one ramp in the bar store the first dynamic
				if (i == 0) {
					dynamicsRampData[j] = dynamicsData[0];
				}
				else {
					dynamicsRampData[j] = dynamicsData[dynamicsRampStart[i-1]];
				}
			}
			// otherwise store the next dynamic so that the receiving program
			// ramps from its current dynamic to the next one in the duration
			// of the current note (retrieved by the seqVec.durs vector)
			else {
				dynamicsRampData[j] = dynamicsData[j+1];
			}
		}
	}
	// set a default value to the following vectors in case they're empty
	if (dotIndexes.size() == 0) {
		dotIndexes.push_back(-1);
	}
	if (dynamicsRampStartForScore.size() == 0) {
		dynamicsRampStartForScore.push_back(-1);
	}
	if (dynamicsRampEndForScore.size() == 0) {
		dynamicsRampEndForScore.push_back(-1);
	}
	if (dynamicsRampDirForScore.size() == 0) {
		dynamicsRampDirForScore.push_back(-1);
	}
	// set a default value for slurs, in case they're empty
	if (slurBeginningsIndexes.size() == 0) {
		slurBeginningsIndexesForScore.push_back(-1);
	}
	if (slurEndingsIndexes.size() == 0) {
		slurEndingsIndexesForScore.push_back(-1);
	}
	// get the tempo of the minimum duration
	seqVec.newTempo = (float)seqVec.tempoMs / ((float)minDur / (float)seqVec.denominator);
	seqVec.tempNumBeats = seqVec.numerator * (minDur / seqVec.denominator);
	int dursAccum = 0;
	// convert durations to number of beats with respect to minimum duration
	for (unsigned i = 0; i < dursData.size(); i++) {
		dursData[i] = minDur / dursData[i];
		if (dotIndexes[i] == 1) {
			dursData[i] += (dursData[i] / 2);
		}
		dursAccum += dursData[i];
		dursForScore.push_back(dursData[i]);
		dursDataWithoutSlurs.push_back(dursData[i]);
	}
	if (dursAccum < seqVec.tempNumBeats) {
		return "error: durations sum is less than bar duration";
	}
	if (dursAccum > seqVec.tempNumBeats) {
		return "error: durations sum is greater than bar duration";
	}
	// then check if we have slurs and set the durations accordingly
	// don't do this for the durations sent to the score
	if (slurBeginningsIndexes.size() > 0) {
		for (unsigned i = 0; i < slurBeginningsIndexes.size(); i++) {
			int durAccumIndex = slurBeginningsIndexes[i];
			for (int j = slurBeginningsIndexes[i]+1; j <= slurEndingsIndexes[i]; j++) {
				// accumulate the duration of the slurred notes in the first index
				dursData[durAccumIndex] += dursData[j];
				// zero the durations of the rest of the slurred notes
				dursData[j] = 0;
			}
		}
	}
	// push the new data only after all strings have been parsed and no errors have occured
	int activeInstrument = 0;
	if (storePattern) {
		for (unsigned i = 0; i < seqVec.instruments.size(); i++) {
			if (lastInstrument.compare(seqVec.instruments[i]) == 0) {
				// first map durations to MIDI duartions based on the percentage
				// value for noteOn and noteOff
				for (unsigned j = 0; j < dursDataWithoutSlurs.size(); j++) {
					int percentage = seqVec.midiDurs[i];
					if (midiArticulationVals[j] >= 10) {
						switch (midiArticulationVals[j]) {
							case 12:
								if (seqVec.midiTenuto[i] > 0) {
									percentage = seqVec.midiTenuto[i];
								}
								break;
							case 13:
								if (seqVec.midiStaccatissimo[i] > 0) {
									percentage = seqVec.midiStaccatissimo[i];
								}
								break;
							case 15:
								if (seqVec.midiStaccato[i] > 0) {
									percentage = seqVec.midiStaccato[i];
								}
								break;
							default:
								break;
						}
					}
					// first store the percetage for the noteOn
					midiDursDataWithoutSlurs.push_back((percentage * dursDataWithoutSlurs[j]) / 100);
					// and the remaining number of beats for the noteOff
					midiDursDataWithoutSlurs.push_back(dursDataWithoutSlurs[j] - midiDursDataWithoutSlurs.back());
				}
				// once done, we must add 0 velocities for the noteOff parts
				// we iterate over the midiVels vector backwards to easily insert data
				for (unsigned j = midiVels.size(); j > 0; j--) {
					midiVels.insert(midiVels.begin()+j, 0);
				}
				// we also double the notes so we have a note for every noteOn and noteOff message
				for (unsigned j = midiNotesData.size(); j > 0; j--) {
					midiNotesData.insert(midiNotesData.begin()+j, midiNotesData[j-1]);
				}
				// get the duration of glissandi in number of beats and store them as MIDI control messages
				for (unsigned j = 0; j < glissandiIndexes.size(); j++) {
					if (glissandiIndexes[j]) {
						// minimum duration is 1/4 of a 64th note, so we divide by 16
						// to get the duration of a 16th note
						int glissDur = dursData[j] / 16;
						if (glissDur > 127) glissDur = 127;
						midiGlissDurs[j] = glissDur;
					}
				}
				// and the duration of crescendi and diminuendi in number of beats
				int dynRampDur = 0;
				for (unsigned j = 0; j < dynamicsRampStart.size(); j++) {
					for (int k = dynamicsRampStart[j]; k < dynamicsRampEnd[j]; k++) {
						dynRampDur += dursData[k];
					}
					// again divide by 16
					dynRampDur /= 16;
					midiDynamicsRampDurs[dynamicsRampStart[j]] = dynRampDur;
					dynRampDur = 0;
				}
				if (inserting) {
					// simulate a mutex lock by notifying the sequencer that we're updating running memory
					seqVec.updatingSharedMem = true;
					if (sequencer.isThreadRunning()) {
						// seqVec.stepDone is updated by the sequencer
						while (!seqVec.stepDone); // wait for the current step to end
					}
					seqVec.notes[i].erase(seqVec.notes[i].begin()+patternIndex);
					seqVec.midiNotes[i].erase(seqVec.midiNotes[i].begin()+patternIndex);
					seqVec.durs[i].erase(seqVec.durs[i].begin()+patternIndex);
					seqVec.dursWithoutSlurs[i].erase(seqVec.dursWithoutSlurs[i].begin()+patternIndex);
					seqVec.midiDursWithoutSlurs[i].erase(seqVec.midiDursWithoutSlurs[i].begin()+patternIndex);
					seqVec.pitchBendVals[i].erase(seqVec.pitchBendVals[i].begin()+patternIndex);
					seqVec.dynamics[i].erase(seqVec.dynamics[i].begin()+patternIndex);
					seqVec.midiVels[i].erase(seqVec.midiVels[i].begin()+patternIndex);
					seqVec.dynamicsRamps[i].erase(seqVec.dynamicsRamps[i].begin()+patternIndex);
					seqVec.glissandi[i].erase(seqVec.glissandi[i].begin()+patternIndex);
					seqVec.midiGlissDurs[i].erase(seqVec.midiGlissDurs[i].begin()+patternIndex);
					seqVec.midiDynamicsRampDurs[i].erase(seqVec.midiDynamicsRampDurs[i].begin()+patternIndex);
					seqVec.articulations[i].erase(seqVec.articulations[i].begin()+patternIndex);
					seqVec.midiArticulationVals[i].erase(seqVec.midiArticulationVals[i].begin()+patternIndex);
					seqVec.text[i].erase(seqVec.text[i].begin()+patternIndex);
					seqVec.textIndexes[i].erase(seqVec.textIndexes[i].begin()+patternIndex);
					slurBeginnings[i].erase(slurBeginnings[i].begin()+patternIndex);
					slurEndings[i].erase(slurEndings[i].begin()+patternIndex);
					scoreNotes[i].erase(scoreNotes[i].begin()+patternIndex);
					scoreDurs[i].erase(scoreDurs[i].begin()+patternIndex);
					scoreDotIndexes[i].erase(scoreDotIndexes[i].begin()+patternIndex);
					scoreAccidentals[i].erase(scoreAccidentals[i].begin()+patternIndex);
					scoreOctaves[i].erase(scoreOctaves[i].begin()+patternIndex);
					scoreGlissandi[i].erase(scoreGlissandi[i].begin()+patternIndex);
					scoreArticulations[i].erase(scoreArticulations[i].begin()+patternIndex);
					scoreDynamics[i].erase(scoreDynamics[i].begin()+patternIndex);
					scoreDynamicsIndexes[i].erase(scoreDynamicsIndexes[i].begin()+patternIndex);
					scoreDynamicsRampStart[i].erase(scoreDynamicsRampStart[i].begin()+patternIndex);
					scoreDynamicsRampEnd[i].erase(scoreDynamicsRampEnd[i].begin()+patternIndex);
					scoreDynamicsRampDir[i].erase(scoreDynamicsRampDir[i].begin()+patternIndex);
					scoreSlurBeginnings[i].erase(scoreSlurBeginnings[i].begin()+patternIndex);
					scoreSlurEndings[i].erase(scoreSlurEndings[i].begin()+patternIndex);
					scoreTexts[i].erase(scoreTexts[i].begin()+patternIndex);

					seqVec.notes[i].insert(seqVec.notes[i].begin()+patternIndex, notesData);
					seqVec.midiNotes[i].insert(seqVec.midiNotes[i].begin()+patternIndex, midiNotesData);
					seqVec.durs[i].insert(seqVec.durs[i].begin()+patternIndex, dursData);
					seqVec.dursWithoutSlurs[i].insert(seqVec.dursWithoutSlurs[i].begin()+patternIndex, dursDataWithoutSlurs);
					seqVec.midiDursWithoutSlurs[i].insert(seqVec.midiDursWithoutSlurs[i].begin()+patternIndex, midiDursDataWithoutSlurs);
					seqVec.pitchBendVals[i].insert(seqVec.pitchBendVals[i].begin()+patternIndex, pitchBendVals);
					seqVec.dynamics[i].insert(seqVec.dynamics[i].begin()+patternIndex, dynamicsData);
					seqVec.midiVels[i].insert(seqVec.midiVels[i].begin()+patternIndex, midiVels);
					seqVec.dynamicsRamps[i].insert(seqVec.dynamicsRamps[i].begin()+patternIndex, dynamicsRampData);
					seqVec.glissandi[i].insert(seqVec.glissandi[i].begin()+patternIndex, glissandiIndexes);
					seqVec.midiGlissDurs[i].insert(seqVec.midiGlissDurs[i].begin()+patternIndex, midiGlissDurs);
					seqVec.midiDynamicsRampDurs[i].insert(seqVec.midiDynamicsRampDurs[i].begin()+patternIndex, midiDynamicsRampDurs);
					seqVec.articulations[i].insert(seqVec.articulations[i].begin()+patternIndex, articulationIndexes);
					seqVec.midiArticulationVals[i].insert(seqVec.midiArticulationVals[i].begin()+patternIndex, midiArticulationVals);
					seqVec.text[i].insert(seqVec.text[i].begin()+patternIndex, texts);
					seqVec.textIndexes[i].insert(seqVec.textIndexes[i].begin()+patternIndex, textIndexesLocal);
					slurBeginnings[i].insert(slurBeginnings[i].begin()+patternIndex, slurBeginningsIndexes);
					slurEndings[i].insert(slurEndings[i].begin()+patternIndex, slurEndingsIndexes);
					scoreNotes[i].insert(scoreNotes[i].begin()+patternIndex, notesForScore);
					scoreDurs[i].insert(scoreDurs[i].begin()+patternIndex, dursForScore);
					scoreDotIndexes[i].insert(scoreDotIndexes[i].begin()+patternIndex, dotIndexes);
					scoreAccidentals[i].insert(scoreAccidentals[i].begin()+patternIndex, accidentalsForScore);
					scoreOctaves[i].insert(scoreOctaves[i].begin()+patternIndex, octavesForScore);
					scoreGlissandi[i].insert(scoreGlissandi[i].begin()+patternIndex, glissandiIndexes);
					scoreArticulations[i].insert(scoreArticulations[i].begin()+patternIndex, articulationIndexes);
					scoreDynamics[i].insert(scoreDynamics[i].begin()+patternIndex, dynamicsForScore);
					scoreDynamicsIndexes[i].insert(scoreDynamicsIndexes[i].begin()+patternIndex, dynamicsForScoreIndexes);
					scoreDynamicsRampStart[i].insert(scoreDynamicsRampStart[i].begin()+patternIndex, dynamicsRampStartForScore);
					scoreDynamicsRampEnd[i].insert(scoreDynamicsRampEnd[i].begin()+patternIndex, dynamicsRampEndForScore);
					scoreDynamicsRampDir[i].insert(scoreDynamicsRampDir[i].begin()+patternIndex, dynamicsRampDirForScore);
					scoreSlurBeginnings[i].insert(scoreSlurBeginnings[i].begin()+patternIndex, slurBeginningsIndexesForScore);
					scoreSlurEndings[i].insert(scoreSlurEndings[i].begin()+patternIndex, slurEndingsIndexesForScore);
					scoreTexts[i].insert(scoreTexts[i].begin()+patternIndex, textsForScore);
					seqVec.updatingSharedMem = false;
				}
				else {
					seqVec.notes[i].push_back(notesData);
					seqVec.midiNotes[i].push_back(midiNotesData);
					seqVec.durs[i].push_back(dursData);
					seqVec.dursWithoutSlurs[i].push_back(dursDataWithoutSlurs);
					seqVec.midiDursWithoutSlurs[i].push_back(midiDursDataWithoutSlurs);
					seqVec.dynamics[i].push_back(dynamicsData);
					seqVec.midiVels[i].push_back(midiVels);
					seqVec.pitchBendVals[i].push_back(pitchBendVals);
					seqVec.dynamicsRamps[i].push_back(dynamicsRampData);
					seqVec.glissandi[i].push_back(glissandiIndexes);
					seqVec.midiGlissDurs[i].push_back(midiGlissDurs);
					seqVec.midiDynamicsRampDurs[i].push_back(midiDynamicsRampDurs);
					seqVec.articulations[i].push_back(articulationIndexes);
					seqVec.midiArticulationVals[i].push_back(midiArticulationVals);
					seqVec.text[i].push_back(texts);
					seqVec.textIndexes[i].push_back(textIndexesLocal);
					slurBeginnings[i].push_back(slurBeginningsIndexes);
					slurEndings[i].push_back(slurEndingsIndexes);
					scoreNotes[i].push_back(notesForScore);
					scoreDurs[i].push_back(dursForScore);
					scoreDotIndexes[i].push_back(dotIndexes);
					scoreAccidentals[i].push_back(accidentalsForScore);
					scoreOctaves[i].push_back(octavesForScore);
					scoreGlissandi[i].push_back(glissandiIndexes);
					scoreArticulations[i].push_back(articulationIndexes);
					scoreDynamics[i].push_back(dynamicsForScore);
					scoreDynamicsIndexes[i].push_back(dynamicsForScoreIndexes);
					scoreDynamicsRampStart[i].push_back(dynamicsRampStartForScore);
					scoreDynamicsRampEnd[i].push_back(dynamicsRampEndForScore);
					scoreDynamicsRampDir[i].push_back(dynamicsRampDirForScore);
					scoreSlurBeginnings[i].push_back(slurBeginningsIndexesForScore);
					scoreSlurEndings[i].push_back(slurEndingsIndexesForScore);
					scoreTexts[i].push_back(textsForScore);
				}
				activeInstrument = i;
				if (!inserting) {
					// address the vectors above like this:
					// int bar = seqVec.patternData[seqVec.patternLoopIndexes[seqVec.patternLoopIndex]][0]
					// int inst = seqVec.patternData[seqVec.patternLoopIndexes[seqVec.patternLoopIndex]][1]
					// seqVec.notes[inst][bar]
					// instruments are stored after the bar, so they are accessed through
					// a loop that start from 1 and ends at the end of the current seqVec.patternData size
					seqVec.patternData[patternIndex].push_back(i);
					// store the index of the instrument, in case there is an error in the pattern
					// in which case we want to pop_baclk() the vectors of any instrument before the error
					// we also use this in case we don't include all instruments in a pattern
					// in which case we'll set a rest for the missing instruments
					instrumentsPassed.push_back(i);
				}
				if (seqVec.sendsToPart[i]) {
					ofxOscMessage m;
					// send all data to the corresponding part
					m.setAddress("/patdata");
					m.addIntArg(barIndexGlobal);
					m.addIntArg(seqVec.tempNumBeats);
					seqVec.scorePartSenders[i].sendMessage(m, false);
					m.clear();
					for (unsigned j = 0; j < notesForScore.size(); j++) {
						m.setAddress("/notes");
						// add the instrument to support multi inst merging in one part
						m.addIntArg(i);
						for (unsigned k = 0; k < notesForScore[i].size(); k++) {
							m.addIntArg(notesForScore[j][k]);
						}
						seqVec.scorePartSenders[i].sendMessage(m, false);
						m.clear();
					}
					for (unsigned j = 0; j < accidentalsForScore.size(); j++) {
						m.setAddress("/acc");
						m.addIntArg(i);
						for (unsigned k = 0; k < accidentalsForScore[j].size(); k++) {
							m.addIntArg(accidentalsForScore[j][k]);
						}
						seqVec.scorePartSenders[i].sendMessage(m, false);
						m.clear();
					}
					for (unsigned j = 0; j < octavesForScore.size(); j++) {
						m.setAddress("/oct");
						m.addIntArg(i);
						for (unsigned k = 0; k < octavesForScore[j].size(); k++) {
							m.addIntArg(octavesForScore[j][k]);
						}
						seqVec.scorePartSenders[i].sendMessage(m, false);
						m.clear();
					}
					m.setAddress("/durs");
					m.addIntArg(i);
					for (unsigned j = 0; j < dursForScore.size(); j++) {
						m.addIntArg(dursForScore[j]);
					}
					seqVec.scorePartSenders[i].sendMessage(m, false);
					m.clear();
					m.setAddress("/dots");
					m.addIntArg(i);
					for (unsigned j = 0; j < dotIndexes.size(); j++) {
						m.addIntArg(dotIndexes[j]);
					}
					seqVec.scorePartSenders[i].sendMessage(m, false);
					m.clear();
					m.setAddress("/gliss");
					m.addIntArg(i);
					for (unsigned j = 0; j < glissandiIndexes.size(); j++) {
						m.addIntArg(glissandiIndexes[j]);
					}
					seqVec.scorePartSenders[i].sendMessage(m, false);
					m.clear();
					m.setAddress("/artic");
					m.addIntArg(i);
					for (unsigned j = 0; j < articulationIndexes.size(); j++) {
						m.addIntArg(articulationIndexes[j]);
					}
					seqVec.scorePartSenders[i].sendMessage(m, false);
					m.clear();
					m.setAddress("/dyn");
					m.addIntArg(i);
					for (unsigned j = 0; j < dynamicsForScore.size(); j++) {
						m.addIntArg(dynamicsForScore[j]);
					}
					seqVec.scorePartSenders[i].sendMessage(m, false);
					m.clear();
					m.setAddress("/dynIdx");
					m.addIntArg(i);
					for (unsigned j = 0; j < dynamicsForScoreIndexes.size(); j++) {
						m.addIntArg(dynamicsForScoreIndexes[j]);
					}
					seqVec.scorePartSenders[i].sendMessage(m, false);
					m.clear();
					m.setAddress("/dynramp_start");
					m.addIntArg(i);
					for (unsigned j = 0; j < dynamicsRampStartForScore.size(); j++) {
						m.addIntArg(dynamicsRampStartForScore[j]);
					}
					seqVec.scorePartSenders[i].sendMessage(m, false);
					m.clear();
					m.setAddress("/dynramp_end");
					m.addIntArg(i);
					for (unsigned j = 0; j < dynamicsRampEndForScore.size(); j++) {
						m.addIntArg(dynamicsRampEndForScore[j]);
					}
					seqVec.scorePartSenders[i].sendMessage(m, false);
					m.clear();
					m.setAddress("/dynramp_dir");
					m.addIntArg(i);
					for (unsigned j = 0; j < dynamicsRampDirForScore.size(); j++) {
						m.addIntArg(dynamicsRampDirForScore[j]);
					}
					seqVec.scorePartSenders[i].sendMessage(m, false);
					m.clear();
					m.setAddress("/slurbegin");
					m.addIntArg(i);
					for (unsigned j = 0; j < slurBeginningsIndexesForScore.size(); j++) {
						m.addIntArg(slurBeginningsIndexesForScore[j]);
					}
					seqVec.scorePartSenders[i].sendMessage(m, false);
					m.clear();
					m.setAddress("/slurend");
					m.addIntArg(i);
					for (unsigned j = 0; j < slurEndingsIndexesForScore.size(); j++) {
						m.addIntArg(slurEndingsIndexesForScore[j]);
					}
					seqVec.scorePartSenders[i].sendMessage(m, false);
					m.clear();
					m.setAddress("/texts");
					m.addIntArg(i);
					for (unsigned j = 0; j < textsForScore.size(); j++) {
						m.addStringArg(textsForScore[j]);
					}
					seqVec.scorePartSenders[i].sendMessage(m, false);
					m.clear();
					m.setAddress("/textIdx");
					m.addIntArg(i);
					for (unsigned j = 0; j < textIndexesLocal.size(); j++) {
						m.addIntArg(textIndexesLocal[j]);
					}
					seqVec.scorePartSenders[i].sendMessage(m, false);
				}
				break;
			}
		}
		if (!inserting) {
			updateActiveInstruments(activeInstrument);
		}
	}
	return "";
}

//--------------------------------------------------------------
void ofApp::updateActiveInstruments(int activeInstrument){
	// update seqVec.activeInstruments so the sequencer plays all instruments with seqVec.notes
	if (find(seqVec.activeInstruments.begin(), seqVec.activeInstruments.end(), activeInstrument) >= seqVec.activeInstruments.end()) {
		seqVec.activeInstruments.push_back(activeInstrument);
	}
}

//--------------------------------------------------------------
void ofApp::setEditorCoords(){
	seqVec.tracebackYCoord = seqVec.screenHeight - (cursorHeight * 3) + (letterHeightDiff*2) + lineWidth;
	seqVec.middleOfScreenY = seqVec.tracebackYCoord / 2;
	// we need the anchor so we can change the traceback height in case the traceback
	// has more than two lines
	seqVec.tracebackYCoordAnchor = seqVec.tracebackYCoord;
	for (int i = 0; i < numHorizontalEditors; i++) {
		int index = 0;
		for (int j = 0; j < numVerticalEditors[i]; j++) {
			if (i) index = (i * numVerticalEditors[i-1]) + j;
			else index = i + j;
			editors[index].frameHeight = (seqVec.tracebackYCoord / numVerticalEditors[i]) - lineWidth;
			editors[index].frameYOffset = editors[index].frameHeight * j;
			editors[index].frameWidth = (seqVec.screenWidth / numHorizontalEditors) - (lineWidth / 2);
			editors[index].frameXOffset = editors[index].frameWidth * i;
		}
	}
	screenSplitHorizontal = seqVec.screenWidth / numHorizontalEditors;
	for (unsigned i = 0; i < editors.size(); i++) {
		editors[i].setMaxNumLines(1, true);
		editors[i].setMaxCharactersPerString();
	}
}

//--------------------------------------------------------------
void ofApp::setFontSize(){
	font.load("DroidSansMono.ttf", fontSize);
	cursorHeight = font.stringHeight("q");
	letterHeightDiff = font.stringHeight("j") - font.stringHeight("l");
	letterHeightDiff /= 2;
	setEditorCoords();
}

//--------------------------------------------------------------
void ofApp::changeFontSizeForAllEditors(){
	for (unsigned i = 0; i < editors.size(); i++) {
		editors[i].setFontSize(fontSize);
	}
	setFontSize();
}

//--------------------------------------------------------------
string ofApp::setAnimationState(string command){
	if (command.compare("animate") == 0) {
		if (sequencer.isThreadRunning()) {
			for (unsigned i = 0; i < seqVec.notesVec.size(); i++) {
				seqVec.notesVec[i].setAnimation(true);
			}
			seqVec.animate = true;
		}
		else {
			seqVec.setAnimation = true;
		}
		return "";
	}
	else if (command.compare("inanimate") == 0) {
		for (unsigned i = 0; i < seqVec.notesVec.size(); i++) {
			seqVec.notesVec[i].setAnimation(false);
		}
		seqVec.animate = false;
		seqVec.setAnimation = false;
		return "";
	}
	else if (command.compare("showbeat") == 0) {
		if (sequencer.isThreadRunning()) {
			seqVec.beatAnimate = true;
		}
		else {
			seqVec.setBeatAnimation = true;
		}
		return "";
	}
	else if (command.compare("hidebeat") == 0) {
		seqVec.beatAnimate = false;
		seqVec.setBeatAnimation = false;
		return "";
	}
	else if (command.compare("beattype") == 0) {
		seqVec.beatTypeCommand = true;
		return "";
	}
	else if (isDigit(command)) {
		if (seqVec.beatTypeCommand) {
			seqVec.beatVizType = stoi(command);
			seqVec.beatTypeCommand = false;
			if ((seqVec.beatVizType < 1) || (seqVec.beatVizType > 2)) {
				return "unknown beat vizualization type";
			}
			else {
				return "";
			}
		}
		else {
			return "unkown command: integers for \\score command must be preceded by 'beattype'";
		}
	}
	else {
		return command + ": unknown command";
	}
}

//--------------------------------------------------------------
void ofApp::initializeInstrument(string instName){
	seqVec.notes.resize(seqVec.notes.size()+1);
	seqVec.midiNotes.resize(seqVec.midiNotes.size()+1);
	seqVec.durs.resize(seqVec.durs.size()+1);
	seqVec.dursWithoutSlurs.resize(seqVec.dursWithoutSlurs.size()+1);
	seqVec.midiDursWithoutSlurs.resize(seqVec.midiDursWithoutSlurs.size()+1);
	seqVec.pitchBendVals.resize(seqVec.pitchBendVals.size()+1);
	seqVec.dynamics.resize(seqVec.dynamics.size()+1);
	seqVec.midiVels.resize(seqVec.midiVels.size()+1);
	seqVec.dynamicsRamps.resize(seqVec.dynamicsRamps.size()+1);
	seqVec.glissandi.resize(seqVec.glissandi.size()+1);
	seqVec.midiGlissDurs.resize(seqVec.midiGlissDurs.size()+1);
	seqVec.midiDynamicsRampDurs.resize(seqVec.midiDynamicsRampDurs.size()+1);
	seqVec.articulations.resize(seqVec.articulations.size()+1);
	seqVec.midiArticulationVals.resize(seqVec.midiArticulationVals.size()+1);
	seqVec.text.resize(seqVec.text.size()+1);
	seqVec.textIndexes.resize(seqVec.textIndexes.size()+1);
	slurBeginnings.resize(slurBeginnings.size()+1);
	slurEndings.resize(slurEndings.size()+1);
	scoreNotes.resize(scoreNotes.size()+1);
	scoreDurs.resize(scoreDurs.size()+1);
	scoreDotIndexes.resize(scoreDotIndexes.size()+1);
	scoreAccidentals.resize(scoreAccidentals.size()+1);
	scoreOctaves.resize(scoreOctaves.size()+1);
	scoreGlissandi.resize(scoreGlissandi.size()+1);
	scoreArticulations.resize(scoreArticulations.size()+1);
	scoreDynamics.resize(scoreDynamics.size()+1);
	scoreDynamicsIndexes.resize(scoreDynamicsIndexes.size()+1);
	scoreDynamicsRampStart.resize(scoreDynamicsRampStart.size()+1);
	scoreDynamicsRampEnd.resize(scoreDynamicsRampEnd.size()+1);
	scoreDynamicsRampDir.resize(scoreDynamicsRampDir.size()+1);
	scoreSlurBeginnings.resize(scoreSlurBeginnings.size()+1);
	scoreSlurEndings.resize(scoreSlurEndings.size()+1);
	scoreTexts.resize(scoreTexts.size()+1);

	seqVec.instruments.resize(seqVec.instruments.size()+1);
	seqVec.beatCounters.resize(seqVec.beatCounters.size()+1);
	seqVec.barDataCounters.resize(seqVec.barDataCounters.size()+1);
	instNameWidths.resize(instNameWidths.size()+1);
	seqVec.scoreYAnchors.resize(seqVec.scoreYAnchors.size()+1);

	// score part OSC stuff
	seqVec.sendsToPart.resize(seqVec.sendsToPart.size()+1);
	seqVec.sendsToPart.back() = false;
	seqVec.scorePartSenders.resize(seqVec.scorePartSenders.size()+1);

	// MIDI stuff
	seqVec.midiChans.resize(seqVec.midiChans.size()+1);
	seqVec.midiDurs.resize(seqVec.midiDurs.size()+1);
	seqVec.isMidi.resize(seqVec.isMidi.size()+1);
	seqVec.midiStaccato.resize(seqVec.midiStaccato.size()+1);
	seqVec.midiStaccatissimo.resize(seqVec.midiStaccatissimo.size()+1);
	seqVec.midiTenuto.resize(seqVec.midiTenuto.size()+1);
	// the sequencer keeps track of all notesOn so it can send
	// the equivalent noteOff messages when it stops
	sequencer.notesOn.resize(sequencer.notesOn.size()+1);
	// it also has one counter for noteOn messages only, per instrument
	sequencer.midiSequencerCounters.resize(sequencer.midiSequencerCounters.size()+1);
	sequencer.midiSequencerCounters.back() = 0;
	seqVec.midiChans.back() = 1;
	seqVec.midiDurs.back() = 75;
	seqVec.isMidi.back() = false;

	seqVec.instruments.back() = instName;
	seqVec.beatCounters.back() = 0;
	seqVec.barDataCounters.back() = 0;
	tempNumInstruments = (int)seqVec.instruments.size();
	int thisInstNameWidth = instFont.stringWidth(instName);
	instNameWidths.back() = thisInstNameWidth;
	if (thisInstNameWidth > seqVec.longestInstNameWidth) {
		seqVec.longestInstNameWidth = thisInstNameWidth;
	}
	seqVec.scoreYAnchors.back().push_back({0});
	// if an instrument is initialized after we have already created patterns
	// we create an empty melodic line for every existing pattern
	// if (seqVec.patternData.size() > 0) {
	// 	for (unsigned i = 0; i < seqVec.patternData.size(); i++) {
	// 		createEmptyMelody(seqVec.notes.size()-1);
	// 		if (i != patternIndex) {
	// 			// update every existing pattern with the new instrument
	// 			seqVec.patternData[i].push_back(seqVec.patternData[i].size()-3);
	// 			setScoreNotes(i);
	// 			posCalculator.setLoopIndex(i);
	// 			posCalculator.startThread();
	// 		}
	// 	}
	// }
}

//--------------------------------------------------------------
void ofApp::updateTempoBeatsInsts(int ptrnIdx){
	numInstruments = seqVec.activeInstruments.size();
	seqVec.tempo = seqVec.newTempo;
	seqVec.numBeats = seqVec.tempNumBeats;
}

//--------------------------------------------------------------
void ofApp::createStaff(){
	float xPos = seqVec.staffXOffset+seqVec.longestInstNameWidth+(seqVec.blankSpace*1.5);
	float xEdge = seqVec.staffWidth-seqVec.blankSpace;
	// place yPos at the center of the screen in the Y axis
	float yPos = seqVec.middleOfScreenY - (seqVec.staffLinesDist * 2);
	// and give an offset according to the number of seqVec.staffs
	// so they are all centered
	if (seqVec.instruments.size() > 1) {
		yPos -= ((seqVec.instruments.size()/2) * (seqVec.staffLinesDist * 10));
		// if the number of seqVec.staffs is even, we must subtract half the distance
		// between each staff
		if ((seqVec.instruments.size() % 2) == 0) {
			yPos += (seqVec.staffLinesDist * 5);
		}
	}
	Staff newStaff(xPos, yPos+((seqVec.instruments.size()-1)*(seqVec.staffLinesDist*10)),
	 			   xEdge, seqVec.staffLinesDist, seqVec.numerator, seqVec.denominator);
	newStaff.setSize(seqVec.scoreFontSize);
	seqVec.staffs.push_back(newStaff);
	float xStartPnt = seqVec.staffs[seqVec.staffs.size()-1].getXPos();
	if (xStartPnt > seqVec.notesXStartPnt) seqVec.notesXStartPnt = xStartPnt;
}

//--------------------------------------------------------------
void ofApp::initStaffCoords(int loopIndex){
	float xPos = seqVec.staffXOffset+seqVec.longestInstNameWidth+(seqVec.blankSpace*1.5);
	float xEdge = seqVec.staffWidth-seqVec.blankSpace-seqVec.staffLinesDist;
	// place yPos at the center of the screen in the Y axis
	float yPos = (seqVec.tracebackYCoord/2) - (seqVec.staffLinesDist * 2);
	// and give an offset according to the number of seqVec.staffs
	// so they are all centered
	if (seqVec.instruments.size() > 1) {
		yPos -= ((seqVec.instruments.size()/2) * (seqVec.staffLinesDist * 10));
		// if the number of seqVec.staffs is even, we must subtract half the distance
		// between each staff
		if ((seqVec.instruments.size() % 2) == 0) {
			yPos += (seqVec.staffLinesDist * 5);
		}
	}
	seqVec.notesXStartPnt = 0;
	for (unsigned i = 0; i < seqVec.instruments.size(); i++) {
		seqVec.scoreYAnchors[i][loopIndex] = yPos+(i*(seqVec.staffLinesDist*10));
		seqVec.staffs[i].setCoords(loopIndex, xPos, seqVec.scoreYAnchors[i][loopIndex], xEdge,
								   seqVec.staffLinesDist, seqVec.numerator, seqVec.denominator);
		int xStartPnt = seqVec.staffs[i].getXPos();
		// get the maximum start point so all scores are alligned on the X axis
		if (xStartPnt > seqVec.notesXStartPnt) seqVec.notesXStartPnt = xStartPnt;
	}
}

//--------------------------------------------------------------
void ofApp::createNotes(){
	Notes newNotes(seqVec.denominator, seqVec.scoreFontSize, notesID);
	seqVec.notesVec.push_back(newNotes);
	notesID++;
}

//--------------------------------------------------------------
void ofApp::initNotesCoords(int ptrnIdx){
	// send a message to any connected score part app
	ofxOscMessage m;
	m.setAddress("/initcoords");
	m.addIntArg(ptrnIdx);
	for (unsigned i = 0; i < seqVec.sendsToPart.size(); i++) {
		if (seqVec.sendsToPart[i]) seqVec.scorePartSenders[i].sendMessage(m, false);
	}
	// give the appropriate offset so that notes are not too close to the loop symbols
	float xStart = seqVec.notesXStartPnt + seqVec.staffLinesDist;
	float xEdge = seqVec.screenWidth - seqVec.blankSpace - (seqVec.staffLinesDist*2);
	// place yPos at the center of the screen in the Y axis
	float yPos = (seqVec.tracebackYCoord/2) - (seqVec.staffLinesDist * 2);
	// and give an offset according to the number of seqVec.staffs
	// so they are all centered
	if (seqVec.instruments.size() > 1) {
		yPos -= ((seqVec.instruments.size()/2) * (seqVec.staffLinesDist * 10));
		// if the number of seqVec.staffs is even, we must subtract half the distance
		// between each staff
		if ((seqVec.instruments.size() % 2) == 0) {
			yPos += (seqVec.staffLinesDist * 5);
		}
	}
	int bar = seqVec.patternData[ptrnIdx][0];
	int numBeats = seqVec.patternData[ptrnIdx][1];
	for (unsigned i = 2; i < seqVec.patternData[ptrnIdx].size(); i++) {
		int inst = seqVec.patternData[ptrnIdx][i];
		// we call this function whenever we type a new instrument name because
		// if the new name is longer, the seqVec.staffs will shrink, and the seqVec.notes must
		// take new positions, otherwise they'll go out of bounds (they'll enter
		// the seqVec.tempo and clef space)
		seqVec.notesVec[inst].setCoords(bar, ptrnIdx, xStart, yPos+(inst*(seqVec.staffLinesDist*10)),
										xEdge, seqVec.staffLinesDist, seqVec.scoreFontSize, numBeats);
	}
	// if we have at least one instrument in our pattern (which should be the case)
	if (seqVec.patternData[ptrnIdx].size() > 2) {
		// get the note width from the first instrument, as they all have the same size
		seqVec.noteWidth = seqVec.notesVec[seqVec.patternData[ptrnIdx][2]].getNoteWidth();
		seqVec.noteHeight = seqVec.notesVec[seqVec.patternData[ptrnIdx][2]].getNoteHeight();
	}
}

//--------------------------------------------------------------
void ofApp::setScoreNotes(int ptrnIdx){
	// this version of the overloaded setScoreNotes() function is called only
	// when we create a pattern by writing seqVec.notes, and not by using other patterns
	// so patternIndex is essensially the same as seqVec.patternLoopIndex
	int bar = seqVec.patternData[ptrnIdx][0];
	int numBeats = seqVec.patternData[ptrnIdx][1];
	initNotesCoords(ptrnIdx);
	// send a message to any connected score part app
	ofxOscMessage m;
	m.setAddress("/setnotes");
	m.addIntArg(ptrnIdx);
	for (unsigned i = 0; i < seqVec.sendsToPart.size(); i++) {
		if (seqVec.sendsToPart[i]) seqVec.scorePartSenders[i].sendMessage(m, false);
	}
	for (unsigned i = 2; i < seqVec.patternData[ptrnIdx].size(); i++) {
		int inst = seqVec.patternData[ptrnIdx][i];
		seqVec.notesVec[inst].setNotes(bar, ptrnIdx,
									   scoreNotes[inst][bar],
									   scoreAccidentals[inst][bar],
									   scoreOctaves[inst][bar],
									   scoreDurs[inst][bar],
									   scoreDotIndexes[inst][bar],
									   scoreGlissandi[inst][bar],
									   scoreArticulations[inst][bar],
									   scoreDynamics[inst][bar],
									   scoreDynamicsIndexes[inst][bar],
									   scoreDynamicsRampStart[inst][bar],
									   scoreDynamicsRampEnd[inst][bar],
									   scoreDynamicsRampDir[inst][bar],
									   scoreSlurBeginnings[inst][bar],
									   scoreSlurEndings[inst][bar],
									   scoreTexts[inst][bar],
									   seqVec.textIndexes[inst][bar],
									   numBeats);
	}
}

//--------------------------------------------------------------
void ofApp::setScoreNotes(int bar, int loopIndex, int inst, int beats){
	seqVec.notesVec[inst].setNotes(bar, loopIndex, scoreDurs[inst][bar], beats);
}

//--------------------------------------------------------------
void ofApp::swapScorePosition(int orientation){
	switch (orientation) {
		case 0:
			if (seqVec.staffXOffset == seqVec.middleOfScreenX) {
				seqVec.staffXOffset = 0;
				seqVec.staffWidth = seqVec.screenWidth / 2;
			}
			else {
				seqVec.staffXOffset = seqVec.middleOfScreenX;
				seqVec.staffWidth = seqVec.screenWidth;
			}
			setScoreSizes();
			break;
		case 1:
			break;
		default:
			break;
	}
}

//--------------------------------------------------------------
void ofApp::setScoreSizes(){
	// for a 35 font size the staff lines distance is 10
	seqVec.staffLinesDist = 10 * ((float)seqVec.scoreFontSize / 35.0);
	if (seqVec.patternLoopIndexes.size() > 0) {
		for (unsigned i = 0; i < seqVec.patternLoopIndexes.size(); i++) {
			for (unsigned j = 0; j < seqVec.patternLoopIndexes[i].size(); j++) {
				int bar = seqVec.patternData[seqVec.patternLoopIndexes[i][j]][0];
				int numBeats = seqVec.patternData[seqVec.patternLoopIndexes[i][j]][1];
				initStaffCoords(seqVec.patternLoopIndexes[i][j]);
				initNotesCoords(seqVec.patternLoopIndexes[i][j]);
				for (unsigned k = 2; k < seqVec.patternData[seqVec.patternLoopIndexes[i][j]].size(); k++) {
					int inst = seqVec.patternData[seqVec.patternLoopIndexes[i][j]][k];
					int loopIndex = seqVec.patternLoopIndexes[i][j];
					seqVec.staffs[inst].setSize(seqVec.scoreFontSize);
					seqVec.notesVec[inst].setFontSize(seqVec.scoreFontSize);
					setScoreNotes(bar, loopIndex, inst, numBeats);
				}
			}
			posCalculator.setLoopIndex(seqVec.patternLoopIndex);
			posCalculator.startThread();
		}
	}
	else {
		initStaffCoords(0);
	}
}

//--------------------------------------------------------------
void ofApp::exit(){
	sequencer.stopThread();
	posCalculator.stopThread();
	ofxOscMessage m;
	m.setAddress("/exit");
	m.addIntArg(1);
	sendToParts(m);
	if (midiPortOpen) sequencer.midiOut.closePort();
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::resizeWindow() {
	setEditorCoords();
	setScoreSizes();
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){
	// hold the previous middle of screen value so the score is also properly updated
	int prevMiddleOfScreenX = seqVec.middleOfScreenX;
	seqVec.screenWidth = w;
	seqVec.screenHeight = h;
	seqVec.middleOfScreenX = seqVec.screenWidth / 2;
	seqVec.middleOfScreenY = seqVec.screenHeight / 2;
	// once the screen coordinates have been updated, update the score coordinates too
	if (seqVec.staffXOffset == prevMiddleOfScreenX) {
		seqVec.staffXOffset = 0;
		seqVec.staffWidth = seqVec.screenWidth / 2;
	}
	else {
		seqVec.staffXOffset = seqVec.middleOfScreenX;
		seqVec.staffWidth = seqVec.screenWidth;
	}
	windowResizeTimeStamp = ofGetElapsedTimeMillis();
	isWindowResized = true;
	// setEditorCoords();
	// setScoreSizes();
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){

}
