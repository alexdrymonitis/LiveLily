#include "ofApp.h"
#include <string.h>
#include <time.h> // to give random seed to srand()
#include <sstream>
#include <ctype.h> // to add isdigit()
#include <algorithm> // for replacing characters in a string
#include <cmath> // to add abs()
#include <utility> // to add pair and make_pair
#include <sstream> // to create a string from vector<string>

/******************* Sequencer class **************************/
//--------------------------------------------------------------
void Sequencer::setup(SharedData *sData)
{
	sharedData = sData;
	runSequencer = false;
	updateSequencer = false;
	sequencerRunning = false;
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
	thisLoopIndex = 0;
	countdown = false;
	countdownCounter = 0;
	midiTuneVal = 440;
	oscSender.setup(HOST, OFSENDPORT);
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
void Sequencer::setSendMidiClock(bool sendMidiClockState)
{
	sendMidiClock = sendMidiClockState;
}

//--------------------------------------------------------------
void Sequencer::sendAllNotesOff()
{
	for (map<int, Instrument>::iterator it = sharedData->instruments.begin(); it != sharedData->instruments.end(); ++it) {
		if (it->second.isMidi()) midiOut.sendControlChange(it->second.getMidiChan(), 120, 0);
		else {
			ofxOscMessage m;
			m.setAddress("/" + it->second.getName() + "/dynamics");
			m.addIntArg(0);
			oscSender.sendMessage(m, false);
			m.clear();
		}
	}
}

//--------------------------------------------------------------
void Sequencer::start()
{
	runSequencer = true;
	sequencerRunning = false;
	startThread();
}

//--------------------------------------------------------------
void Sequencer::stop()
{
	mustStop = true;
}

//--------------------------------------------------------------
void Sequencer::stopNow()
{
	sendAllNotesOff();
	stopThread();
	if (sendMidiClock) midiOut.sendMidiByte(0xFC); // Stop MIDI byte
	if (!mustStopCalled) onNextStartMidiByte = 0xFB; // Continue MIDI byte
	else mustStopCalled = false;
	runSequencer = sequencerRunning = false;
	beatCounter = 0;
	((ofApp*)ofGetAppPtr())->sendSequencerStateToParts(false);
}

//--------------------------------------------------------------
void Sequencer::update()
{
	updateSequencer = true;
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
	if (finish) ((ofApp*)ofGetAppPtr())->sendFinishToParts(finishState);
}

//--------------------------------------------------------------
void Sequencer::setCountdown(int num)
{
	countdown = true;
	countdownCounter = num;
}

//--------------------------------------------------------------
void Sequencer::setMidiTune(int tuneVal)
{
	midiTuneVal = tuneVal;
}

//--------------------------------------------------------------
void Sequencer::sendBeatVizInfo(int bar)
{
	// calculate the time stamp regardless of whether we're showing the beat
	// or not in the editor, in case we have connected score parts
	sharedData->beatVizTimeStamp = ofGetElapsedTimeMillis();
	sharedData->beatVizStepsPerMs = (float)BEATVIZBRIGHTNESS / ((float)sharedData->tempoMs[bar] / 4.0);
	sharedData->beatVizRampStart = sharedData->tempoMs[bar] / 4;
	if (sharedData->beatAnimate) {
		sharedData->beatViz = true;
	}
	// notify all score parts of the time stamp and the beat visualization counter
	ofxOscMessage m;
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
float Sequencer::midiToFreq(int midiNote)
{
	return pow(2.0, ((midiNote - 69.0) / 12.0)) * midiTuneVal;
}

//--------------------------------------------------------------
void Sequencer::startGlissando(int intNdx, int bar, int barDataCounter)
{
//	sharedData->instruments.at(instNdx).setGlissandoStart(true);
//	// if the isntrument sends MIDI, then we have to determine the pitch bend value we need to cover
//	// but we should bare in mind that the user might have set a specific tuning
//	// in which case we have to compensate for that
//	auto itBegin = sharedData->instruments.at(instNdx).notes.at(bar).at(sharedData->instruments.at(instNdx).getBarDataCounter()).begin();
//	auto itEnd = sharedData->instruments.at(instNdx).notes.at(bar).at(sharedData->instruments.at(instNdx).getBarDataCounter()).end();
//	for (auto it = itBegin; it != itEnd; ++it) {
//		sharedData->instruments.at(instNdx).glissVecStart.push_back(*it);
//	}
//	int nextNote = sharedData->instruments.at(instNdx).getBarDataCounter() + 1;
//	int nextBar = sharedData->thisLoopIndex;
//	if (nextNote >= (int)sharedData->instruments.at(instNdx).notes.at(bar).size()) {
//		nextNote = 0;
//		nextBar = sharedData->thisLoopIndex + 1;
//		if (nextBar >= (int)sharedData->loopData.at(sharedData->loopIndex).size()) {
//			nextBar = 0;
//		}
//	}
//	itBegin = sharedData->instruments.at(instNdx).notes.at(nextBar).at(nextNote).begin();
//	itEnd = sharedData->instruments.at(instNdx).notes.at(nextBar).at(nextNote).end();
//	for (auto it = itBegin; it != itEnd; ++it) {
//		sharedData->instruments.at(instNdx).glissVecEnd.push_back(*it);
//	}
//	// check if the two vectors dont't have the same size
//	if (sharedData->instruments.at(instNdx).glissStart.size() != sharedData->instruments.at(instNdx).glissEnd.size()) {
//		
//	}
}

//--------------------------------------------------------------
void Sequencer::runGlissando(int instNdx, int bar)
{
//	// here we run a glissando that has been started by the function above
//	// with a time grain of 100um (the sequencer's time grain)
//	// based on the current tempo
//	if (sharedData->instruments.at(instNdx).getGlissandoStart()) {
//		// here we run the glissando, and when we reach its target, we set the glissandoStart to false
//		if (reachedTargetPitch) {
//			sharedData->instruments.at(instNdx).setGlissandoStart(false);
//		}
//	}
}

//--------------------------------------------------------------
void Sequencer::threadedFunction()
{
	while (isThreadRunning()) {
		timer.waitNext();
		if (runSequencer && !sequencerRunning) {
			//sharedData->mutex.lock();
			int bar = sharedData->loopData[sharedData->loopIndex][sharedData->thisLoopIndex];
			// in here we calculate the number of beats on a microsecond basis
			numBeats = sharedData->numBeats[bar] * sharedData->tempo[bar];
			if (sharedData->showScore) {
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
			((ofApp*)ofGetAppPtr())->sendFinishToParts(false);
			ofxOscMessage m;
			m.setAddress("/beatinfo");
			m.addFloatArg(sharedData->beatVizStepsPerMs);
			m.addIntArg(sharedData->beatVizRampStart);
			((ofApp*)ofGetAppPtr())->sendToParts(m, true);
			m.clear();
			sequencerRunning = true;
			runSequencer = false;
			if (sendMidiClock) midiOut.sendMidiByte(onNextStartMidiByte);
			firstIter = true;
			endOfBar = false;
			sendBeatVizInfoCounter = 0;
			for (auto instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
				instMapIt->second.initSeqToggle();
				instMapIt->second.setFirstIter(true);
			}
			thisLoopIndex = sharedData->thisLoopIndex;
			((ofApp*)ofGetAppPtr())->sendSequencerStateToParts(true);
			//sharedData->mutex.unlock();
		}
	
		if (sequencerRunning) {
			//sharedData->mutex.lock();
			uint64_t timeStamp = ofGetElapsedTimeMicros();
			int bar = sharedData->loopData[sharedData->loopIndex][sharedData->thisLoopIndex];
			int prevBar = sharedData->loopData[sharedData->loopIndex][((int)sharedData->thisLoopIndex - 1 < 0 ? (int)sharedData->loopData[sharedData->loopIndex].size()-1 : sharedData->thisLoopIndex-1)];
			// check if we're at the beginning of the loop
			if (sharedData->thisLoopIndex == 0) {
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
					checkMute();
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
			if (sharedData->thisLoopIndex == sharedData->loopData[sharedData->loopIndex].size()-1) {
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
					for (auto func = sharedData->functions.begin(); func != sharedData->functions.end(); ++func) {
						// binding a function to the number of instruments + 2, binds to the end
						if (func->second.getBoundInst() == (int)sharedData->instruments.size() + 2 && sharedData->beatCounter == 0) {
							((ofApp*)ofGetAppPtr())->parseCommand(((ofApp*)ofGetAppPtr())->genCmdInput(func->second.getName()), 1, 1);
						}
					}
					finished = false;
					stopNow();
					//sharedData->mutex.unlock();
					return;
				}
				if (firstIter) tickCounter = timeStamp;
				else tickCounter = timeStamp - (uint64_t)(diff - compareVal);
				// reset all instrument note durations at the start of each bar
				//for (auto it = sharedData->instruments.begin(); it != sharedData->instruments.end(); ++it) {
				//	it->second.resetNoteDur();
				//}
				if (endOfBar || firstIter) {
					sharedData->thisLoopIndex = thisLoopIndex;
					bar = sharedData->loopData[sharedData->loopIndex][sharedData->thisLoopIndex];
					ofxOscMessage m;
					m.setAddress("/thisloopndx");
					m.addIntArg(sharedData->thisLoopIndex);
					((ofApp*)ofGetAppPtr())->sendToParts(m, true);
					m.clear();
					// sync the time stamp of each instrument to that of the sequencer when the latter starts
					if (firstIter) {
						for (auto it = sharedData->instruments.begin(); it != sharedData->instruments.end(); ++it) {
							it->second.setTimeStamp(timeStamp);
						}
					}
					if (updateSequencer && (sharedData->thisLoopIndex == 0)) {
						// update the loop index first
						sharedData->loopIndex = sharedData->tempBarLoopIndex;
						// then update the bar index
						bar = sharedData->loopData[sharedData->loopIndex][sharedData->thisLoopIndex];
						((ofApp*)ofGetAppPtr())->sendLoopIndexToParts();
						// zero noteDur for all instruments so the first step of a new loop fires immediately
						//for (auto it = sharedData->instruments.begin(); it != sharedData->instruments.end(); ++it) {
						//	it->second.zeroNoteDur();
						//}
						updateSequencer = false;
						sharedData->drawLoopStartEnd = true;
						sharedData->barCounter = 0;
						// update the previous number of bars displayed and the previous position to properly display single bars in horizontal view
						sharedData->prevNumBars = sharedData->numBars;
						sharedData->prevPosition = sharedData->thisPosition;
					}
					// after checking if we must update the indexes, increment them
					if (!countdown) {
						thisLoopIndex++;
						if (thisLoopIndex >= sharedData->loopData[sharedData->loopIndex].size()) {
							thisLoopIndex = 0;
							// when we start a loop from its beginning, we must check if an instrument needs to be muted
							muteChecked = false;
						}
						// increment the bar counter which is used for displaying the score only
						sharedData->barCounter++;
						endOfBar = false;
						if (firstIter) ((ofApp*)ofGetAppPtr())->sendStopCountdownToParts();
					}
				}
				firstIter = false;
				if (countdown) {
					((ofApp*)ofGetAppPtr())->sendCountdownToParts(countdownCounter);
					countdownCounter--;
					sharedData->beatCounter = -1;
					for (auto func = sharedData->functions.begin(); func != sharedData->functions.end(); ++func) {
						// binding to the number of instruments, binds a function to the pulse of the beat
						if (func->second.getBoundInst() == (int)sharedData->instruments.size()) { 
							((ofApp*)ofGetAppPtr())->parseCommand(((ofApp*)ofGetAppPtr())->genCmdInput(func->second.getName()), 1, 1);
						}
					}
					if (!(sharedData->beatCounter == 0 && mustStop)) sendBeatVizInfo(bar);
					endOfBar = true;
					if (countdownCounter == 0) {
						countdown = false;
						sharedData->beatCounter = 0;
						((ofApp*)ofGetAppPtr())->sendCountdownToParts(countdownCounter);
					}
				}
				else {
					sharedData->beatCounter = beatCounter;
					beatCounter++;
					if (beatCounter >= sharedData->numerator[bar]) {
						beatCounter = 0;
					}
					if (sendBeatVizInfoCounter == 0) {
						if (!(sharedData->beatCounter == 0 && mustStop)) sendBeatVizInfo(bar);
					}
					if (sharedData->beatAtDifferentThanDivisor[bar]) {
						sendBeatVizInfoCounter++;
						int modulo = (sharedData->beatAtValues[bar] > 0 ? sharedData->beatAtValues[bar] : 1);
						sendBeatVizInfoCounter %= modulo;
					}
					for (auto func = sharedData->functions.begin(); func != sharedData->functions.end(); ++func) {
						// binding a function to the number of instruments + 1, binds to the start of each bar
						if (func->second.getBoundInst() == (int)sharedData->instruments.size() + 1 && sharedData->beatCounter == 0) {
							((ofApp*)ofGetAppPtr())->parseCommand(((ofApp*)ofGetAppPtr())->genCmdInput(func->second.getName()), 1, 1);
						}
						// binding to the number of instruments, binds a function to the pulse of the beat
						else if (func->second.getBoundInst() == (int)sharedData->instruments.size()) { 
							((ofApp*)ofGetAppPtr())->parseCommand(((ofApp*)ofGetAppPtr())->genCmdInput(func->second.getName()), 1, 1);
						}
						// binding to the number of instruments + 3, binds a function to the start of the loop
						else if (func->second.getBoundInst() == (int)sharedData->instruments.size() + 3 && \
								sharedData->thisLoopIndex == 0 && sharedData->beatCounter == 0) { 
							((ofApp*)ofGetAppPtr())->parseCommand(((ofApp*)ofGetAppPtr())->genCmdInput(func->second.getName()), 1, 1);
						}
					}
				}
				sharedData->PPQNCounter = 0;
				// check if we're at the beginning of a bar so we must reset the barDataCounter of each instrument
				if (sharedData->beatCounter == 0) {
					for (auto it = sharedData->instruments.begin(); it != sharedData->instruments.end(); ++it) {
						it->second.resetBarDataCounter();
					}
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
						//sharedData->mutex.unlock();
						return;
					}
				}
			}
			if (!countdown) {
				// send MIDI clock, if set
				// need to calculate the number of steps
				uint64_t midiClockTimeStamp = ofGetElapsedTimeMicros();
				if (sendMidiClock && \
						(midiClockTimeStamp - sharedData->PPQNTimeStamp) >= sharedData->PPQNPerUs[bar] && \
						sharedData->PPQNCounter < sharedData->PPQN) {
					midiOut.sendMidiByte(0xF8);
					sharedData->PPQNCounter++;
					sharedData->PPQNTimeStamp = midiClockTimeStamp;
				}
				for (auto instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
					if (instMapIt->second.getBarDataCounter() == 0) {
						if (instMapIt->second.mustUpdateTempo()) {
							instMapIt->second.setUpdateTempo(false);
						}
					}
					//runGlissando(instMapIt->first, bar); // this function determines whethere there are any glissandi active
					if (instMapIt->second.hasNotesInBar(bar)) {
						if (instMapIt->second.mustFireStep(timeStamp, bar, sharedData->tempo[bar])) {
							// if we have a note, not a rest, and the current instrument is not muted
							if (instMapIt->second.hasNotesInStep(bar) && !instMapIt->second.isMuted()) {
								if (!instMapIt->second.isMidi()) { // send OSC message
									// if the toggle is on and the note is not slurred or tied, send a note off
									if (instMapIt->second.getSeqToggle()) {
										if (!instMapIt->second.isNoteSlurred(bar, instMapIt->second.getBarDataCounter()) && \
												!instMapIt->second.isNoteTied(bar, instMapIt->second.getBarDataCounter())) {
											ofxOscMessage m;
											m.setAddress("/" + instMapIt->second.getName() + "/dynamics");
											m.addIntArg(0);
											oscSender.sendMessage(m, false);
											m.clear();
										}
									}
									else {
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
											if (prevBar != bar) {
												if (!instMapIt->second.isLastNoteTied(prevBar)) {
													sendData = true;
												}
												else {
													sendData = false;
												}
											}
											else {
												sendData = true;
											}
										}
										//if (instMapIt->second.glissandi.at(bar).at(instMapIt->second.getBarDataCounter()) == 1) {
										//	startGlissando(instMapIt->first, bar, instMapIt->second.getBarDataCounter());
										//}
										if (sendData) {
											ofxOscMessage m;
											m.setAddress("/" + instMapIt->second.getName() + "/articulation");
											for (auto it = instMapIt->second.articulations[bar][instMapIt->second.getBarDataCounter()].begin(); it != instMapIt->second.articulations[bar][instMapIt->second.getBarDataCounter()].end(); ++it) {
												m.addStringArg(sharedData->articulSyms[*it]);
											}
											oscSender.sendMessage(m, false);
											m.clear();

											if (instMapIt->second.hasText(bar)) {
												m.setAddress("/" + instMapIt->second.getName() + "/text");
												m.addStringArg(instMapIt->second.getText(bar));
												oscSender.sendMessage(m, false);
												m.clear();
											}

											m.setAddress("/" + instMapIt->second.getName() + "/note");
											for (auto it = instMapIt->second.notes[bar][instMapIt->second.getBarDataCounter()].begin(); it != instMapIt->second.notes[bar][instMapIt->second.getBarDataCounter()].end(); ++it) {
												if (instMapIt->second.sendMIDI()) m.addFloatArg(*it);
												else m.addFloatArg(midiToFreq(*it));
											}
											oscSender.sendMessage(m, false);
											m.clear();
											
											m.setAddress("/" + instMapIt->second.getName() + "/dynamics");
											if (instMapIt->second.sendMIDI()) m.addIntArg(instMapIt->second.getMidiVel(bar));
											else m.addIntArg(instMapIt->second.getDynamic(bar));
											oscSender.sendMessage(m, false);
											m.clear();
										}
									}
								}
								// sending MIDI
								else {
									if (instMapIt->second.getSeqToggle()) {
										if (!instMapIt->second.isNoteSlurred(bar, instMapIt->second.getBarDataCounter()) && \
												!instMapIt->second.isNoteTied(bar, instMapIt->second.getBarDataCounter())) {
											for (auto it = instMapIt->second.midiNotes[bar][instMapIt->second.getBarDataCounter()].begin(); it != instMapIt->second.midiNotes[bar][instMapIt->second.getBarDataCounter()].end(); ++it) {
												midiOuts[midiPortsMap[instMapIt->second.getMidiPort()]].sendNoteOff(instMapIt->second.getMidiChan(), *it, 0);
											}
										}
									}
									else {
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
											if (prevBar != bar) {
												if (!instMapIt->second.isLastNoteTied(prevBar)) {
													sendData = true;
												}
												else {
													sendData = false;
												}
											}
											else {
												sendData = true;
											}
										}
										if (sendData) {
											midiOuts[midiPortsMap[instMapIt->second.getMidiPort()]].sendPitchBend(instMapIt->second.getMidiChan(), instMapIt->second.getPitchBendVal(bar));
											for (auto it = instMapIt->second.midiArticulationVals[bar][instMapIt->second.barDataCounter].begin(); it != instMapIt->second.midiArticulationVals[bar][instMapIt->second.barDataCounter].end(); ++it) {
												midiOuts[midiPortsMap[instMapIt->second.getMidiPort()]].sendProgramChange(instMapIt->second.getMidiChan(), *it);
											}
											for (auto it = instMapIt->second.midiNotes[bar][instMapIt->second.barDataCounter].begin(); it != instMapIt->second.midiNotes[bar][instMapIt->second.barDataCounter].end(); ++it) {
												midiOuts[midiPortsMap[instMapIt->second.getMidiPort()]].sendNoteOn(instMapIt->second.getMidiChan(), *it, instMapIt->second.getMidiVel(bar));
											}
										}
										// if the previous note was slurred, but not tied, send the note off message after the note on of the new note
										if (instMapIt->second.getBarDataCounter() > 0) {
											if (instMapIt->second.isNoteSlurred(bar, instMapIt->second.getBarDataCounter()-1) && \
													!instMapIt->second.isNoteTied(bar, instMapIt->second.getBarDataCounter()-1)) {
												for (auto it = instMapIt->second.midiNotes[bar][instMapIt->second.getBarDataCounter()-1].begin(); it != instMapIt->second.midiNotes[bar][instMapIt->second.getBarDataCounter()-1].end(); ++it) {
													midiOuts[midiPortsMap[instMapIt->second.getMidiPort()]].sendNoteOff(instMapIt->second.getMidiChan(), *it, 0);
												}
											}
										}
									}
								}
							}
							else {
								if (!instMapIt->second.isMidi()) {
									// in case of a rest, send a 0 dynamic to make sure the instMapInstrument will stop
									ofxOscMessage m;
									m.setAddress("/" + instMapIt->second.getName() + "/dynamics");
									m.addIntArg(0);
									oscSender.sendMessage(m, false);
									m.clear();
								}
								else {
									// send all sound off with CC #120 with value 0
									midiOut.sendControlChange(instMapIt->second.getMidiChan(), 120, 0);
								}
							}
							if (sharedData->animate && !instMapIt->second.getSeqToggle()) {
								instMapIt->second.setActiveNote();
							}
							if (instMapIt->second.getSeqToggle()) {
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
											((ofApp*)ofGetAppPtr())->parseCommand(((ofApp*)ofGetAppPtr())->genCmdInput(func->second.getName()), 1, 1); // dummy 2nd and 3rd arguments
										}
									}
								}
							}
							// toggleSeqToggle() takes care to increment the barDataCounter of the class
							instMapIt->second.toggleSeqToggle(bar);
						}
					}
				}
			}
			// check if we're at the end of the bar
			if (sharedData->beatCounter == sharedData->numerator[bar] - 1 && !endOfBar) {
				endOfBar = true;
				if (sharedData->thisLoopIndex == sharedData->loopData[sharedData->loopIndex].size()-1 && finish) {
					// if we're finishing
					finish = false;
					finished = true;
					runSequencer = false;
					updateSequencer = false;
					onNextStartMidiByte = 0xFA; // Start MIDI command
					//for (auto instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
					//	instMapIt->second.setAnimation(false);
					//}
					//sharedData->beatAnimate = false;
					//sharedData->animate = false;
					//sharedData->setAnimation = true;
				}
			}
			//sharedData->mutex.unlock();
		}
	}
}

/********************* Main OF class **************************/
//--------------------------------------------------------------
void ofApp::setup()
{
	backgroundColor = ofColor::black;
	ofBackground(backgroundColor);

	// give a random seed to srand()
	srand(time(NULL));
	
	unsigned int framerate = 60;
	
	sequencer.setup(&sharedData);

	startSequencer = false;

	brightnessCoeff = 0.85;
	brightness = (int)(255 * brightnessCoeff);
	notesID = 0;
	parsingBar = false;
	parsingBars = false;
	parsingLoop = false;
	storingFunction = false;
	storingList = false;
	barsIterCounter = 0;
	numBarsToParse = 0;
	numBarsToParseSet = false;
	firstInstForBarsIndex = 0;
	firstInstForBarsSet = false;
	checkIfAllPartsReceivedBarData = false;
	lastFunctionIndex = 0;
	fullScreen = false;
	isWindowResized = false;
	windowResizeTimeStamp = 0;
	
	ofSetFrameRate(framerate);
	lineWidth = 2;
	ofSetLineWidth(lineWidth);
	
	ofSetWindowTitle("LiveLily");
	
	//oscReceiver.setup(OSCPORT);
	oscReceiverIsSet = false;
	// waiting time to receive a response from a score part server is one frame at 60 FPS
	scorePartResponseTime = (uint64_t)((1000.0 / 60.0) + 0.5);
	
	sharedData.screenWidth = ofGetWindowWidth();
	sharedData.screenHeight = ofGetWindowHeight();
	sharedData.middleOfScreenX = sharedData.screenWidth / 2;
	sharedData.middleOfScreenY = sharedData.screenHeight / 2;
	sharedData.staffXOffset = sharedData.middleOfScreenX;
	sharedData.staffWidth = sharedData.screenWidth;
	numBarsToDisplay = 2;

	sharedData.numInstruments = 0;
	
	shiftPressed = false;
	ctrlPressed = false;
	altPressed = false;
	
	ofTrueTypeFont::setGlobalDpi(72);
	fontSize = 18;
	whichPane = 0;
	Editor editor;
	editors[0] = editor;
	editors[0].setActivity(true);
	editors[0].setID(0);
	editors[0].setPaneRow(0);
	editors[0].setPaneCol(1);
	numPanes[0] = 1;
	paneSplitOrientation = 0;
	// the function below calls setFontSize() which in turn sets various values
	// including the tracebackBase variable that is used to determine the coordinates and offsets of the panes
	setFontSize();
	screenSplitHorizontal = sharedData.screenWidth;
	
	sharedData.tempoMs[0] = 500;
	sharedData.BPMMultiplier[0] = 1;
	sharedData.beatAtDifferentThanDivisor[0] = false;
	sharedData.numerator[0] = 4;
	sharedData.denominator[0] = 4;
	sharedData.numBeats[0] = 4;
	
	barError = false;
	inserting = false;
	
	sharedData.loopIndex = 0;
	sharedData.tempBarLoopIndex = 0;
	sharedData.thisLoopIndex = 0;
	sharedData.beatCounter = 0; // this is controlled by the sequencer
	sharedData.barCounter = 0; // also controlled by the sequencer
	sharedData.numBars = sharedData.prevNumBars = 1;
	sharedData.thisPosition = sharedData.prevPosition = 0;
	showBarCount = false;
	showTempo = false;
	
	sharedData.showScore = false;
	sharedData.longestInstNameWidth = 0;
	sharedData.staffLinesDist = 10.0;
	sharedData.scoreFontSize = 35;
	instFontSize = sharedData.scoreFontSize/3.5;
	instFont.load("Monaco.ttf", instFontSize);
	sharedData.notesXStartPnt = 0;
	sharedData.blankSpace = 10;
	sharedData.animate = false;
	sharedData.setAnimation = false;
	sharedData.beatAnimate = false;
	sharedData.setBeatAnimation = false;
	sharedData.beatViz = false;
	sharedData.beatTypeCommand = false;
	sharedData.beatVizType = 1;
	sharedData.beatVizDegrade = BEATVIZBRIGHTNESS;
	sharedData.beatVizTimeStamp = 0;
	sharedData.beatVizStepsPerMs = (float)BEATVIZBRIGHTNESS / ((float)sharedData.tempoMs[0] / 4.0);
	sharedData.beatVizRampStart = sharedData.tempoMs[0] / 4;
	sharedData.beatUpdated = false;
	sharedData.noteWidth = 0;
	sharedData.noteHeight = 0;
	sharedData.allStaffDiffs = 0;
	sharedData.maxBarFirstStaffAnchor = 0;

	scoreOrientation = 0;
	scoreXOffset = sharedData.screenWidth / 2;
	scoreYOffset = 0;
	scoreXStartPnt = 0;
	scoreBackgroundWidth = sharedData.screenWidth/2;
	scoreBackgroundHeight = sharedData.tracebackBase - cursorHeight;
	scoreMoveXCommand = false;
	scoreMoveYCommand = false;
	mustUpdateScore = false;
	scoreUpdated = false;
	scoreChangeOnLastBar = false;

	maestroToggleSet = false;
	receivingMaestro = false;
	maestroValNdx = 0;
	maestroInitialized = false;
	maestroValThresh = 7.5;
	maestroTimeStamp = ofGetElapsedTimeMillis();

	correctOnSameOctaveOnly = true;

	serialPortOpen = false;
	
	sharedData.PPQN = 24;
	sharedData.PPQNCounter = 0;
	// get the ms duration of the PPQM
	sharedData.PPQNPerUs[0] = (uint64_t)(sharedData.tempoMs[0] / (float)sharedData.PPQN) * 1000;
	
	// set the notes chars
	for (int i = 2; i < 9; i++) {
		noteChars[i-2] = char((i%7)+97);
	}
	noteChars[7] = char(114); // "r" for rest
	
	// store the articulation names strings
	string articulSymsLocal[8] = {"none", "marcato", "trill", "tenuto", \
								  "staccatissimo", "accent", "staccato", "portando"};
	for (int i = 0; i < 8; i++) {
		sharedData.articulSyms[i] = articulSymsLocal[i];
	}

	// create the beat rectangle transparency cosine table
	for (int i = 0; i <= BEATVIZBRIGHTNESS; i++) {
		sharedData.beatVizCosTab[i] = (int)(((cos(((float)i/(float)(BEATVIZBRIGHTNESS+1))*M_PI) * -0.5) + 0.5) * 255);
	}

	/* Colors used for syntax highlighting
	   first level commands -- fuchsia
	   second level commands - violet
	   arguments ------------- skyBlue
	   instruments ----------- greenYellow
	   bars ------------------ orchid
	   loops ----------------- lightSeaGreen
	   functions ------------- turqoise
	   groups ---------------- lightSteelBlue
	   command execution ----- cyan
	   errors ---------------- red
	   warnings -------------- orange
	   brackets pairing ------ magenta
	   comments -------------- gray
	   digits ---------------- gold
	   text highlight box ---- goldenRod
	   strings --------------- aqua
	   -- paleGreen is used for the pulse of the beat
	*/
	// first level commands
	commandsMap[livelily]["\\bar"] = ofColor::fuchsia;
	commandsMap[livelily]["\\loop"] = ofColor::fuchsia;
	commandsMap[livelily]["\\bars"] = ofColor::fuchsia;
	commandsMap[livelily]["\\function"] = ofColor::fuchsia;
	commandsMap[livelily]["\\tempo"] = ofColor::fuchsia;
	commandsMap[livelily]["\\solo"] = ofColor::fuchsia;
	commandsMap[livelily]["\\solonow"] = ofColor::fuchsia;
	commandsMap[livelily]["\\mute"] =  ofColor::fuchsia;
	commandsMap[livelily]["\\unmute"] =  ofColor::fuchsia;
	commandsMap[livelily]["\\mutenow"] =  ofColor::fuchsia;
	commandsMap[livelily]["\\unmutenow"] = ofColor::fuchsia;
	commandsMap[livelily]["\\change"] = ofColor::fuchsia;
	commandsMap[livelily]["\\remaining"] =  ofColor::fuchsia;
	commandsMap[livelily]["\\cursor"] =  ofColor::fuchsia;
	commandsMap[livelily]["\\score"] =  ofColor::fuchsia;
	commandsMap[livelily]["\\insts"] =  ofColor::fuchsia;
	commandsMap[livelily]["\\finish"] =  ofColor::fuchsia;
	commandsMap[livelily]["\\reset"] =  ofColor::fuchsia;
	commandsMap[livelily]["\\fromosc"] =  ofColor::fuchsia;
	commandsMap[livelily]["\\listmidiports"] = ofColor::fuchsia;
	commandsMap[livelily]["\\openmidiport"] = ofColor::fuchsia;
	commandsMap[livelily]["\\ctlchange"] = ofColor::fuchsia;
	commandsMap[livelily]["\\pgmchange"] = ofColor::fuchsia;
	commandsMap[livelily]["\\midiclock"] = ofColor::fuchsia;
	commandsMap[livelily]["\\dur"] = ofColor::fuchsia;
	commandsMap[livelily]["\\staccato"] = ofColor::fuchsia;
	commandsMap[livelily]["\\staccatissimo"] = ofColor::fuchsia;
	commandsMap[livelily]["\\tenuto"] = ofColor::fuchsia;
	commandsMap[livelily]["\\listserialports"] = ofColor::fuchsia;
	commandsMap[livelily]["\\openserialport"] = ofColor::fuchsia;
	commandsMap[livelily]["\\closeserialport"] = ofColor::fuchsia;
	commandsMap[livelily]["\\serialwrite"] = ofColor::fuchsia;
	commandsMap[livelily]["\\serialprint"] = ofColor::fuchsia;
	commandsMap[livelily]["\\time"] = ofColor::fuchsia;
	commandsMap[livelily]["\\print"] = ofColor::fuchsia;
	commandsMap[livelily]["\\osc"] = ofColor::fuchsia;
	commandsMap[livelily]["\\oscsend"] = ofColor::fuchsia;
	commandsMap[livelily]["\\beatcount"] = ofColor::fuchsia;
	commandsMap[livelily]["\\random"] = ofColor::fuchsia;
	commandsMap[livelily]["\\barnum"] = ofColor::fuchsia;
	commandsMap[livelily]["\\list"] = ofColor::fuchsia;
	commandsMap[livelily]["\\ottava"] = ofColor::fuchsia;
	commandsMap[livelily]["\\ott"] = ofColor::fuchsia;
	commandsMap[livelily]["\\beatsin"] = ofColor::fuchsia;
	commandsMap[livelily]["\\instorder"] = ofColor::fuchsia;
	commandsMap[livelily]["\\miditune"] = ofColor::fuchsia;
	commandsMap[livelily]["\\tuplet"] = ofColor::fuchsia;
	commandsMap[livelily]["\\tup"] = ofColor::fuchsia;
	commandsMap[livelily]["\\system"] = ofColor::fuchsia;
	commandsMap[livelily]["\\beatat"] = ofColor::fuchsia;
	commandsMap[livelily]["\\group"] = ofColor::fuchsia;
	commandsMap[livelily]["\\accoffset"] = ofColor::fuchsia;
	commandsMap[livelily]["\\autobrackets"] = ofColor::fuchsia;
	commandsMap[livelily]["\\active"] = ofColor::fuchsia;
	commandsMap[livelily]["\\inactive"] = ofColor::fuchsia;
	commandsMap[livelily]["\\sendkeys"] = ofColor::fuchsia;
	commandsMap[livelily]["\\sendlines"] = ofColor::fuchsia;
	commandsMap[livelily]["\\maestro"] = ofColor::fuchsia;
	commandsMap[livelily]["\\framerate"] = ofColor::fuchsia;
	commandsMap[livelily]["\\barlines"] = ofColor::fuchsia;
	commandsMap[livelily]["\\clef"] = ofColor::fuchsia;

	// second leve commands
	commandsMap[livelily]["play"] = ofColor::violet;
	commandsMap[livelily]["stop"] =  ofColor::violet;
	commandsMap[livelily]["stopnow"] =  ofColor::violet;
	commandsMap[livelily]["rhythm"] = ofColor::violet;
	commandsMap[livelily]["bind"] = ofColor::violet;
	commandsMap[livelily]["unbind"] = ofColor::violet;
	commandsMap[livelily]["onrelease"] = ofColor::violet;
	commandsMap[livelily]["release"] = ofColor::violet;
	commandsMap[livelily]["sendto"] = ofColor::violet;
	commandsMap[livelily]["midiport"] = ofColor::violet;
	commandsMap[livelily]["midichan"] = ofColor::violet;
	commandsMap[livelily]["show"] = ofColor::violet;
	commandsMap[livelily]["hide"] = ofColor::violet;
	commandsMap[livelily]["animate"] = ofColor::violet;
	commandsMap[livelily]["inanimate"] = ofColor::violet;
	commandsMap[livelily]["showbeat"] = ofColor::violet;
	commandsMap[livelily]["hidebeat"] = ofColor::violet;
	commandsMap[livelily]["beattype"] = ofColor::violet;
	commandsMap[livelily]["recenter"] = ofColor::violet;
	commandsMap[livelily]["numbars"] = ofColor::violet;
	commandsMap[livelily]["traverse"] = ofColor::violet;
	commandsMap[livelily]["fullscreen"] = ofColor::violet;
	commandsMap[livelily]["cursor"] = ofColor::violet;
	commandsMap[livelily]["transpose"] = ofColor::violet;
	commandsMap[livelily]["update"] = ofColor::violet;
	commandsMap[livelily]["correct"] = ofColor::violet;
	commandsMap[livelily]["goto"] = ofColor::violet;
	commandsMap[livelily]["locate"] = ofColor::violet;
	commandsMap[livelily]["sendmidi"] = ofColor::violet;
	commandsMap[livelily]["accoffset"] = ofColor::violet;
	commandsMap[livelily]["delay"] = ofColor::violet;
	commandsMap[livelily]["setargs"] = ofColor::violet;
	commandsMap[livelily]["send"] = ofColor::violet;
	commandsMap[livelily]["setup"] = ofColor::violet;
	commandsMap[livelily]["address"] = ofColor::violet;
	commandsMap[livelily]["valndx"] = ofColor::violet;
	commandsMap[livelily]["toggle"] = ofColor::violet;
	commandsMap[livelily]["reset"] = ofColor::violet;
	commandsMap[livelily]["valthresh"] = ofColor::violet;
	commandsMap[livelily]["init"] = ofColor::violet;
	commandsMap[livelily]["add"] = ofColor::violet;

	// arguments
	commandsMap[livelily]["barcount"] = ofColor::skyBlue;
	commandsMap[livelily]["tempo"] = ofColor::skyBlue;
	commandsMap[livelily]["beat"] = ofColor::skyBlue;
	commandsMap[livelily]["barstart"] = ofColor::skyBlue;
	commandsMap[livelily]["loopstart"] = ofColor::skyBlue;
	commandsMap[livelily]["all"] = ofColor::skyBlue;
	commandsMap[livelily]["framerate"] = ofColor::skyBlue;
	commandsMap[livelily]["fr"] = ofColor::skyBlue;
	commandsMap[livelily]["onlast"] = ofColor::skyBlue;
	commandsMap[livelily]["immediately"] = ofColor::skyBlue;
	commandsMap[livelily]["onoctave"] = ofColor::skyBlue;
	commandsMap[livelily]["on"] = ofColor::skyBlue;
	commandsMap[livelily]["off"] = ofColor::skyBlue;
	commandsMap[livelily]["bass"] = ofColor::skyBlue;
	commandsMap[livelily]["treble"] = ofColor::skyBlue;
	commandsMap[livelily]["alto"] = ofColor::skyBlue;
	commandsMap[livelily]["perc"] = ofColor::skyBlue;
	commandsMap[livelily]["percussion"] = ofColor::skyBlue;

	// push commands that won't let the editor tokenize based on digits
	commandsToNotTokenize.push_back("\\insts");
	commandsToNotTokenize.push_back("\\bar");
	commandsToNotTokenize.push_back("\\bars");
	commandsToNotTokenize.push_back("\\loop");
	commandsToNotTokenize.push_back("\\function");
	commandsToNotTokenize.push_back("\\list");
	commandsToNotTokenize.push_back("\\group");
}

//--------------------------------------------------------------
void ofApp::update()
{
	//sharedData.mutex.lock();
	//bool mutexLocked = true;
    // check for OSC messages
	static float prevVal = 0;
	while (oscReceiver.hasWaitingMessages()) {
		ofxOscMessage m;
		oscReceiver.getNextMessage(m);
		string address = m.getAddress();
		if (address == maestroAddress) {
			if (!sequencer.isThreadRunning() && sharedData.setBeatAnimation) {
				sharedData.beatAnimate = true;
				sharedData.setBeatAnimation = false;
			}
			float val = m.getArgAsFloat(maestroValNdx);
			if (receivingMaestro) {
				if (val > maestroValThresh) {
					if (!sharedData.beatUpdated && ofGetElapsedTimeMillis() - maestroTimeStamp > MAESTROTHRESH) {
						if (maestroInitialized) {
							int bar = getPlayingBarIndex();
							sharedData.beatCounter++;
							if (sharedData.beatCounter >= sharedData.numerator[bar]) {
								sharedData.beatCounter = 0;
								sharedData.thisLoopIndex++;
								if (sharedData.thisLoopIndex >= sharedData.loopData[sharedData.loopIndex].size()) {
									sharedData.thisLoopIndex = 0;
								}
								bar = getPlayingBarIndex();
							}
							sharedData.BPMTempi[bar] = msToBPM(ofGetElapsedTimeMillis() - maestroTimeStamp);
							sendBeatVizInfo(bar);
						}
						else {
							maestroInitialized = true;
						}
						sharedData.beatUpdated = true;
						maestroTimeStamp = ofGetElapsedTimeMillis();
						cout << sharedData.beatCounter << endl;
						printVector(maestroVec);
						float accum = 0;
						for (auto it = maestroVec.begin(); it != maestroVec.end(); ++it) {
							accum += *it;
						}
						accum /= (float)maestroVec.size();
						cout << "accum: " << accum << endl;
						maestroVec.clear();
					}
				}
				else {
					sharedData.beatUpdated = false;
					if (prevVal != 0) {
						maestroVec.push_back(abs(val - prevVal));
					}
					prevVal = val;
				}
			}
		}
		else if (address == maestroToggleAddress) {
			if (m.getArgType(0) == OFXOSC_TYPE_INT32) {
				int val = m.getArgAsInt32(0);
				if (val) receivingMaestro = true;
				else receivingMaestro = false;
			}
			else if (m.getArgType(0) == OFXOSC_TYPE_FLOAT) {
				int val = (int)m.getArgAsFloat(0);
				if (val) receivingMaestro = true;
				else receivingMaestro = false;
			}
			else if (m.getArgType(0) == OFXOSC_TYPE_TRUE || m.getArgType(0) == OFXOSC_TYPE_FALSE) {
				receivingMaestro = m.getArgAsBool(0);
			}
			if (!receivingMaestro) maestroInitialized = false;
			else sharedData.beatCounter = -1;
		}
		else if (address == maestroResetAddress) {
			int val = m.getArgAsInt32(0);
			if (val) sharedData.beatCounter = 0;
		}
		else {
			for (map<int, string>::iterator it = fromOscAddr.begin(); it != fromOscAddr.end(); ++it) {
				if (address.substr(0, it->second.size()) == it->second) {
					int whichPaneOsc = it->first;
					// unlock the mutex here because the function that handles data received from OSC locks it
					//sharedData.mutex.unlock();
					//mutexLocked = false;
					map<int, Editor>::iterator editIt = editors.find(whichPaneOsc);
					if (editIt != editors.end()) {
						if (m.getArgType(0) == OFXOSC_TYPE_STRING) {
							string oscStr = m.getArgAsString(0);
							for (unsigned i = 0; i < oscStr.size(); i++) {
								if (address.find("/press") != string::npos) {
									editIt->second.fromOscPress((int)oscStr.at(i));
								}
								else if (address.find("/release") != string::npos) {
									editIt->second.fromOscRelease((int)oscStr.at(i));
								}
							}
						}
						else if (m.getArgType(0) == OFXOSC_TYPE_INT32) {
							size_t numArgs = m.getNumArgs();
							for (size_t j = 0; j < numArgs; j++) {
								int oscVal = (int)m.getArgAsInt32(j);
								if (address.find("/press") != string::npos) {
									editIt->second.fromOscPress(oscVal);
								}
								else if (address.find("/release") != string::npos) {
									editIt->second.fromOscRelease(oscVal);
								}
							}
						}
						else if (m.getArgType(0) == OFXOSC_TYPE_INT64) {
							size_t numArgs = m.getNumArgs();
							for (size_t j = 0; j < numArgs; j++) {
								int oscVal = (int)m.getArgAsInt64(j);
								if (address.find("/press") != string::npos) {
									editIt->second.fromOscPress(oscVal);
								}
								else if (address.find("/release") != string::npos) {
									editIt->second.fromOscRelease(oscVal);
								}
							}
						}
						else if (m.getArgType(0) == OFXOSC_TYPE_FLOAT) {
							size_t numArgs = m.getNumArgs();
							for (size_t j = 0; j < numArgs; j++) {
								int oscVal = (int)m.getArgAsFloat(j);
								if (address.find("/press") != string::npos) {
									editIt->second.fromOscPress(oscVal);
								}
								else if (address.find("/release") != string::npos) {
									editIt->second.fromOscRelease(oscVal);
								}
							}
						}
						else {
							cout << "unknown OSC message type\n";
						}
					}
					break;
				}
			}
		}
	}


	//if (!mutexLocked) sharedData.mutex.lock();
	// check if any instrument has a delay for receiving OSC messages
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		if (it->second.oscFifo.size() > 0) {
			uint64_t stamp = ofGetElapsedTimeMillis();
			if (stamp >= it->second.oscFifo.back().second) {
				it->second.scorePartSender.sendMessage(it->second.oscFifo.back().first, false);
				it->second.oscFifo.pop_back();
			}
		}
	}
	for (auto func = sharedData.functions.begin(); func != sharedData.functions.end(); ++func) {
		// binding a function to the number of instruments + 2, binds to the framerate
		if (func->second.getBoundInst() == (int)sharedData.instruments.size() + 2) {
			parseCommand(genCmdInput(func->second.getName()), 1, 1);
		}
	}
	//sharedData.mutex.unlock();

	if (isWindowResized) {
		if ((ofGetElapsedTimeMillis() - windowResizeTimeStamp) > WINDOW_RESIZE_GAP) {
			isWindowResized = false;
			resizeWindow();
		}
	}
	if (startSequencer) {
		startSequencer = false;
		if (!sequencer.isThreadRunning()) {
			sequencer.start();
		}
	}
}

//--------------------------------------------------------------
void ofApp::draw()
{
	//sharedData.mutex.lock();
	for (map<int, Editor>::iterator it = editors.begin(); it != editors.end(); ++it) {
		it->second.drawText();
		// draw the pane separator for panes that do not touch the traceback printing area
		if ((paneSplitOrientation == 0 && numPanes.find(it->second.getPaneRow()+1) != numPanes.end()) || \
				(paneSplitOrientation == 1 && it->second.getPaneCol() < numPanes[it->second.getPaneRow()])) {
			it->second.drawPaneSeparator();
		}
	}
	ofSetColor(brightness);
	// draw the lines that separate the different editors
	// initializing to 0 to avoid warning messages during compilation
	float paneWidth = 0, paneHeight = 0;
	// depending on the orientation, we must first create either the height or the width of the editors
	if (paneSplitOrientation == 0) {
		paneHeight = sharedData.tracebackBase / (float)numPanes.size();
	}
	else {
		paneWidth = (float)sharedData.screenWidth / (float)numPanes.size();
	}
	for (map<int, int>::iterator it = numPanes.begin(); it != numPanes.end(); ++it) {
		if (paneSplitOrientation == 0) {
			paneWidth = (float)sharedData.screenWidth / (float)it->second;
			// we start the following loop from 1 because if we have one column only
			// we don't need to draw any separating lines
			for (int col = 1; col < it->second; col++) {
				ofDrawLine(paneWidth*(float)col, paneHeight*(float)it->first, paneWidth*(float)col, paneHeight*(float)(it->first+1));
			}
		}
		else {
			// if the orientation is switched, we treat rows as columns and vice versa
			if (it->first > 0) {
				ofDrawLine(paneWidth*(float)it->first, 0, paneWidth*(float)it->first, sharedData.tracebackYCoord);
			}
			paneHeight = sharedData.tracebackBase / (float)it->second;
		}
	}
	ofSetLineWidth(lineWidth);
	if (sharedData.showScore) {
		drawScore();
	}
	// draw the pane separator for panes that do touch the traceback printing area
	for (map<int, Editor>::iterator it = editors.begin(); it != editors.end(); ++it) {
		if ((paneSplitOrientation == 0 && numPanes.find(it->second.getPaneRow()+1) == numPanes.end()) || \
				(paneSplitOrientation == 1 && it->second.getPaneCol() >= numPanes[it->second.getPaneRow()])) {
			it->second.drawPaneSeparator();
		}
	}
	// fill the traceback printing area with the background color
	float tracebackBackgroundYCoord;
	int paneNdx;
	// first determine the index of the pane we need to get the correct Y coordinates from
	if (paneSplitOrientation == 0) {
		// with a 0 pane split orientation, the index is the last row
		auto lastRowPane = prev(numPanes.end());
		paneNdx = lastRowPane->first;
	}
	else {
		// with a 1 pane split orientation, it's the column that matters, so we just use the first index
		paneNdx = 0;
	}
	// the line below is copied from Editor::drawPaneSeparator() where we calculate the Y coord
	tracebackBackgroundYCoord = (editors[paneNdx].getMaxNumLines() * editors[paneNdx].getCursorHeight()) + editors[paneNdx].getFrameYOffset();
	// in this case though we need an extra offset equal to the height of the cursor
	tracebackBackgroundYCoord += editors[paneNdx].getCursorHeight();
	ofSetColor(backgroundColor);
	ofDrawRectangle(0, tracebackBackgroundYCoord, sharedData.screenWidth, sharedData.screenHeight-tracebackBackgroundYCoord);
	if (editors[whichPane].showingCommand()) {
		drawCommand();
	}
	else{
		drawTraceback();
	}
	//sharedData.mutex.unlock();
}

//--------------------------------------------------------------
int ofApp::msToBPM(unsigned long ms)
{
	return (int)((1000 / ms) * 60);
}

//--------------------------------------------------------------
void ofApp::sendBeatVizInfo(int bar)
{
	// calculate the time stamp regardless of whether we're showing the beat
	// or not in the editor, in case we have connected score parts
	sharedData.beatVizTimeStamp = ofGetElapsedTimeMillis();
	sharedData.beatVizStepsPerMs = (float)BEATVIZBRIGHTNESS / ((float)sharedData.tempoMs[bar] / 4.0);
	sharedData.beatVizRampStart = sharedData.tempoMs[bar] / 4;
	if (sharedData.beatAnimate) {
		sharedData.beatViz = true;
	}
	// notify all score parts of the time stamp and the beat visualization counter
	ofxOscMessage m;
	m.setAddress("/beatinfo");
	m.addFloatArg(sharedData.beatVizStepsPerMs);
	m.addIntArg((int)sharedData.beatVizRampStart);
	sendToParts(m, false);
	m.clear();
	m.setAddress("/beat");
	m.addIntArg(sharedData.beatCounter);
	sendToParts(m, false);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::drawTraceback()
{
	static int tracebackNumLinesStatic = MINTRACEBACKLINES;
	int tracebackCounter = 0;
	uint64_t stamps[editors[whichPane].tracebackTimeStamps.size()];
	int ndxs[editors[whichPane].tracebackTimeStamps.size()];
	int i = 0;
	int paneNdx = 0;
	//for (auto it = numPanes.begin(); it != numPanes.end(); ++it) {
	//	for (i = 0; i < it->second; i++) {
	//		editors[paneNdx++].resetMaxNumLines();
	//	}
	//}
	// we must first determine which traceback string was initiated first, so we can print this on top
	for (auto it = editors[whichPane].getTracebackTimeStampsBegin(); it != editors[whichPane].getTracebackTimeStampsEnd(); ++it) {
		if ((ofGetElapsedTimeMillis() - it->second) < editors[whichPane].getTracebackDur() && editors[whichPane].getTracebackStr(it->first).size() > 0) {
			// exclude the line of the cursor as this is check below
			if (it->first != editors[whichPane].getCursorLineIndex()) {
				ndxs[i] = it->first;
				stamps[i] = it->second;
				i++;
			}
		}
		else {
			it->second = 0;
		}
	}
	// sort indexes based on time stamps (the smaller the time stamp, the higher it should be in the traceback)
	quickSort(stamps, ndxs, 0, i-1);
	int numTracebackLines = 0;
	bool numTracebackLinesChanged = false;
	for (int j = 0; j < i; j++) {
		numTracebackLines += editors[whichPane].getTracebackNumLines(ndxs[j]);
	}
	if (numTracebackLines > 0) {
		numTracebackLinesChanged = true;
	}
	// once we check the traceback messages of lines other than that where the cursor is
	// we check the line of the cursor too
	if (editors[whichPane].tracebackStr[editors[whichPane].getCursorLineIndex()].size() > 0) {
		// if there was something in the traceback, accumulate
		if (numTracebackLinesChanged) {
			numTracebackLines += editors[whichPane].getTracebackNumLines(editors[whichPane].getCursorLineIndex());
		}
		// otherwise just set this number of lines
		else {
			numTracebackLines = editors[whichPane].getTracebackNumLines(editors[whichPane].getCursorLineIndex());
		}
		// add the line index of the cursor to ndxs
		ndxs[i] = editors[whichPane].getCursorLineIndex();
		// increment i so the loop below includes the line of the cursor as well
		i++;
	}
	// minimum number of lines is 2
	// so the line that separates the panes from the traceback can fit one line of text
	numTracebackLines = max(numTracebackLines, MINTRACEBACKLINES);
	// if the number of lines in the traceback has changed, change the maximum number of lines in the editor
	if (numTracebackLines != tracebackNumLinesStatic) {
		tracebackNumLinesStatic = numTracebackLines;
		// if the number of lines of the traceback changes, reset the maximum number of lines of the editor
		// need to check which editors go till the bottom of the window
		paneNdx = 0;
		for (auto it = numPanes.begin(); it != numPanes.end(); ++it) {
			for (int i = 0; i < it->second; i++) {
				// with split orientation set to 0, we need to offset the number of lines
				// for the editors on the last row, so the last key of the numPanes map
				// with split orientation set to 1, we need to offset the number of lines
				// for the editors that are in the last column of every row
				int leftOperand = (paneSplitOrientation == 0 ? it->first : i);
				int rightOperand = (paneSplitOrientation == 0 ? (int)numPanes.size() : it->second);
				if (leftOperand == rightOperand - 1) {
					if (numTracebackLines > MINTRACEBACKLINES) {
						editors[paneNdx].offsetMaxNumLines(-(numTracebackLines-MINTRACEBACKLINES));
					}
					else {
						editors[paneNdx].resetMaxNumLines();
					}
				}
				paneNdx++;
			}
		}
	}
	if (numTracebackLines > MINTRACEBACKLINES) {
		sharedData.tracebackYCoord = sharedData.tracebackBase - (cursorHeight * (numTracebackLines - MINTRACEBACKLINES));
		sharedData.tracebackYCoord = min(sharedData.tracebackYCoord, sharedData.tracebackBase);
	}
	else {
		sharedData.tracebackYCoord = sharedData.tracebackBase;
	}
	for (int j = 0; j < i; j++) {
		switch (editors[whichPane].getTracebackColor(ndxs[j])) {
			case 0:
				ofSetColor(ofColor::white);
				break;
			case 1:
				ofSetColor(ofColor::orange);
				break;
			case 2:
				ofSetColor(ofColor::red);
				break;
			default:
				ofSetColor(ofColor::white);
				break;
		}
		string tracebackStr;
		// if the stderr message is a warning or error, prepend the line number
		if (editors[whichPane].getTracebackColor(ndxs[j]) > 0) {
			tracebackStr = "line " + ofToString(ndxs[j]+1) + ": ";
		}
		tracebackStr += editors[whichPane].getTracebackStr(ndxs[j]);
		font.drawString(tracebackStr, editors[whichPane].getHalfCharacterWidth(), sharedData.tracebackYCoord+(editors[whichPane].getCursorHeight()*(tracebackCounter+1)));
		tracebackCounter += editors[whichPane].getTracebackNumLines(ndxs[j]);
	}
}

//--------------------------------------------------------------
void ofApp::drawCommand()
{
	static int commandNumLinesStatic = MINTRACEBACKLINES;
	vector<string> commandSplit = tokenizeString(editors[whichPane].getCommandStr(), "\n");
	int numCommandLines = (int)commandSplit.size();
	// minimum number of lines is 2
	// so the line that separates the panes from the traceback can fit one line of text
	numCommandLines = max(numCommandLines, MINTRACEBACKLINES);
	// if the number of lines in the traceback has changed, change the maximum number of lines in the editor
	if (numCommandLines != commandNumLinesStatic) {
		commandNumLinesStatic = numCommandLines;
		// if the number of lines of the traceback changes, reset the maximum number of lines of the editor
		// need to check which editors go till the bottom of the window
		int paneNdx = 0;
		for (auto it = numPanes.begin(); it != numPanes.end(); ++it) {
			for (int i = 0; i < it->second; i++) {
				// with split orientation set to 0, we need to offset the number of lines
				// for the editors on the last row, so the last key of the numPanes map
				// with split orientation set to 1, we need to offset the number of lines
				// for the editors that are in the last column of every row
				int leftOperand = (paneSplitOrientation == 0 ? it->first : i);
				int rightOperand = (paneSplitOrientation == 0 ? (int)numPanes.size() : it->second);
				if (leftOperand == rightOperand - 1) {
					if (numCommandLines > MINTRACEBACKLINES) {
						editors[paneNdx].offsetMaxNumLines(-(numCommandLines-MINTRACEBACKLINES));
					}
					else {
						editors[paneNdx].resetMaxNumLines();
					}
				}
				paneNdx++;
			}
		}
	}
	ofSetColor(editors[whichPane].getCommandStrColor());
	string commandStr = editors[whichPane].getCommandStr();
	float charWidth = editors[whichPane].oneCharacterWidth;
	float cursorHeight = editors[whichPane].getCursorHeight();
	float x = editors[whichPane].getHalfCharacterWidth();
	float y = sharedData.tracebackYCoord+cursorHeight;
	if (numCommandLines > 2) y -= ((numCommandLines - 2) * cursorHeight);
	font.drawString(commandStr, x, y);
	if (editors[whichPane].isTypingCommand()) {
		ofDrawRectangle(font.stringWidth(commandStr)+(charWidth*0.5), y-(cursorHeight*0.85), charWidth, cursorHeight);
	}
}

//--------------------------------------------------------------
void ofApp::drawScore()
{
	ofSetColor(brightness);
	ofDrawRectangle(scoreXOffset, scoreYOffset, scoreBackgroundWidth, scoreBackgroundHeight);
	// safety test to avoid crashes when changing the window size before creating any bars
	if (sharedData.loopIndex >= (int)sharedData.loopData.size()) return;
	//------------- variables for horizontal score view ------------------
	//int numBars = 1;
	bool drawLoopStartEnd = true;
	int bar; //, prevBar = 0;
	int ndx = sharedData.thisLoopIndex;
	if (scoreOrientation > 0) {
		// numBars is part of the ofApp class and it's used to be compared to the previous number of bars displayed
		// which is necessary to properly position single bars in horizontal view
		sharedData.numBars = min(numBarsToDisplay, (int)sharedData.loopData[sharedData.loopIndex].size());
		drawLoopStartEnd = false;
		// the equation below yields the first bar to display
		// the rest will be calculated by adding the loop variable i
		// e.g. for an 8-bar loop with two bars being displayed
		// the line below yields 0, 2, 4, 6, every other index (it actually ignores the odd indexes)
		ndx = ((sharedData.thisLoopIndex - (sharedData.thisLoopIndex % sharedData.numBars)) / sharedData.numBars) * sharedData.numBars;
		// in the if test below we set the number of bars to be displayed to fit the remaining bars
		// e.g. in a six-bar loop, when the first four bars have been played, we need to display two bars only
		// not four bars which is the maximum number of bars to display (these numbers are hypothetical)
		if (ndx > 0) {
			sharedData.numBars -= ((int)sharedData.loopData[sharedData.loopIndex].size() - ndx);
		}
	}
	else {
		sharedData.numBars = 1;
	}
	//--------------- end of horizontal score view variables --------------
	//---------------- variables for the beat visualization ---------------
	float beatPulseStartX = 0, beatPulseEndX = 0, noteSize, beatPulseSizeX;
	float beatPulsePosX = 0;
	float beatPulseMaxPosY = 0;
	float beatPulseMinPosY = FLT_MAX;
	float beatPulseSizeY = 0;
	//------------ end of variables for the beat visualization ------------
	// show the animated beat
	ofSetColor(0);
	if (sharedData.instruments.size() > 0) {
		if (sharedData.beatAnimate) {
			beatPulseMinPosY = sharedData.yStartPnts[scoreOrientation] - sharedData.noteHeight;
			beatPulseMaxPosY = sharedData.yStartPnts[scoreOrientation] + (sharedData.staffLinesDist * 4);
			for (unsigned i = 1; i < sharedData.instruments.size(); i++) {
				beatPulseMaxPosY += (sharedData.allStaffDiffs + (sharedData.staffLinesDist * 2));
			}
			beatPulseMaxPosY += sharedData.noteHeight;
			beatPulseSizeY = beatPulseMaxPosY - beatPulseMinPosY;
			if (sharedData.beatViz) {
				switch (sharedData.beatVizType) {
					case 1:
						beatPulseStartX = scoreXStartPnt + scoreXOffset;
						beatPulseEndX = scoreXStartPnt + scoreXOffset + sharedData.instruments.begin()->second.getStaffXLength();
						if (scoreOrientation == 1) {
							beatPulseEndX += (sharedData.instruments.begin()->second.getStaffXLength() * (sharedData.numBars-1));
							beatPulseEndX -= (sharedData.instruments.begin()->second.getClefXOffset() * (sharedData.numBars-1));
							beatPulseEndX -= (sharedData.instruments.begin()->second.getMeterXOffset() * (sharedData.numBars-1));
						}
						beatPulsePosX = beatPulseStartX - sharedData.staffLinesDist;
						beatPulseSizeX = beatPulseEndX - beatPulseStartX;
						beatPulseSizeX += sharedData.staffLinesDist;
						beatPulseMinPosY += scoreYOffset;
						ofSetColor(ofColor::paleGreen.r*brightnessCoeff,
								   ofColor::paleGreen.g*brightnessCoeff,
								   ofColor::paleGreen.b*brightnessCoeff,
								   sharedData.beatVizCosTab[sharedData.beatVizDegrade] * BEATVIZCOEFF);
						ofDrawRectangle(beatPulsePosX, beatPulseMinPosY, beatPulseSizeX, beatPulseSizeY);
						ofSetColor(0);
						break;
					default:
						break;
				}
				// regardless of the beattype, we calculate the alpha value here
				if ((ofGetElapsedTimeMillis() - sharedData.beatVizTimeStamp) >= sharedData.beatVizRampStart) {
					// calculate how many steps the brightness has to dim, depending on the elapsed time and the steps per millisecond
					int brightnessDegrade = (int)((ofGetElapsedTimeMillis()-(sharedData.beatVizTimeStamp+sharedData.beatVizRampStart))*sharedData.beatVizStepsPerMs);
					sharedData.beatVizDegrade = BEATVIZBRIGHTNESS - brightnessDegrade;
					if (sharedData.beatVizDegrade < 0) {
						sharedData.beatVizDegrade = 0;
						sharedData.beatViz = false;
						sharedData.beatVizDegrade = BEATVIZBRIGHTNESS;
					}
				}
			}
		}
		// then we display the score
		float staffOffsetX = scoreXStartPnt + scoreXOffset;
		float notesOffsetX = staffOffsetX;
		//notesOffsetX += sharedData.instruments.begin()->second.getClefXOffset();
		//notesOffsetX += sharedData.instruments.begin()->second.getMeterXOffset();
		notesOffsetX += (sharedData.staffLinesDist * NOTESXOFFSETCOEF);
		//float prevNotesOffsetX = notesOffsetX;
		//float notesXCoef = (sharedData.instruments.begin()->second.getStaffXLength() + staffOffsetX - notesOffsetX) / notesLength;
		if (showBarCount) {
			string barCountStr = "(" + to_string(sharedData.thisLoopIndex+1) + "/" + to_string(sharedData.loopData[sharedData.loopIndex].size()) + ") " + to_string(sharedData.barCounter);
			font.drawString(barCountStr, scoreXOffset+scoreBackgroundWidth-font.stringWidth(barCountStr)-20, scoreYOffset+20);
		}
		std::pair prevMeter = std::make_pair(4, 4);
		std::pair prevTempo = std::make_pair(0, 0);
		vector<int> prevClefs (sharedData.instruments.size(), 0); 
		bool prevDrawClef = false, prevDrawMeter = false;
		for (int i = 0; i < sharedData.numBars; i++) {
			bool drawClef = false, drawMeter = false, animate = false;
			bool drawTempo = false;
			bool showBar = true;
			// safety test to avoid division by zero, in case something went wrong when setting a loop
			if (sharedData.loopData[sharedData.loopIndex].size() == 0) break;
			int barNdx = (ndx + i) % (int)sharedData.loopData[sharedData.loopIndex].size();
			bar = sharedData.loopData[sharedData.loopIndex][barNdx];
			if (i == 0) {
				drawClef = true;
				for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
					prevClefs[it->first] = it->second.getClef(bar);
				}
			}
			else {
				drawClef = false;
				for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
					if (prevClefs[it->first] != it->second.getClef(bar)) {
						drawClef = true;
						break;
					}
				}
				for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
					prevClefs[it->first] = it->second.getClef(bar);
				}
			}
			// get the meter of this bar to determine if it has changed and thus it has be to written
			std::pair<int, int> thisMeter = sharedData.instruments.begin()->second.getMeter(bar);
			std::pair<int, int> thisTempo = std::make_pair(sharedData.tempoBaseForScore[bar], sharedData.BPMTempi[bar]);
			if (i > 0 && scoreOrientation > 0) {
				if (prevMeter.first != thisMeter.first || prevMeter.second != thisMeter.second) {
					drawMeter = true;
				}
				else {
					drawMeter = false;
				}
			}
			else if (i == 0) {
				drawMeter = true;
			}
			if (showTempo) {
				if (prevTempo.first != thisTempo.first || prevTempo.second != thisTempo.second) {
					drawTempo = true;
				}
			}
			if (barNdx == (int)sharedData.thisLoopIndex) animate = true;
			// the index below is used in insertNaturalSigns() in Notes() in score.cpp to deterimine if we must
			// insert a natural sign at the beginning of a bar in case the same note appears
			// in a previous bar with an accidental (this concerns only the visible bars, e.g. 4 bars in one row)
			// mainly used in LiveLily Part
			int insertNaturalsNdx = sharedData.loopIndex;
			// determine which notes all bars but the last must display in case of horizontal view
			// this is so that when the last bar of the displayed line is played, in case there are performers following the score
			// they should be able to look at the left side of the score to see what notes come right after the current playing bar
			// if we are calling a new loop, all bars but the last should display the first bars of the next loop
			// unless we set scoreChangeOnLast to false, where on the last run (e.g. an eight-bar loop will run twice with four bars being display)
			// every bar that finishes playing must display the respective bar of the upcoming loop
			int threshold = (int)sharedData.loopData[sharedData.loopIndex].size() - sharedData.numBars + i;
			if (scoreChangeOnLastBar) threshold = (int)sharedData.loopData[sharedData.loopIndex].size() - 2;
			if (mustUpdateScore && (int)sharedData.thisLoopIndex > threshold && i < sharedData.numBars - 1) {
				if (i >= (int)sharedData.loopData[sharedData.tempBarLoopIndex].size()) {
					showBar = false;
				}
				else {
					bar = sharedData.loopData[sharedData.tempBarLoopIndex][i];
					insertNaturalsNdx = sharedData.tempBarLoopIndex;
				}
				scoreUpdated = true;
			}
			else if (mustUpdateScore && scoreUpdated && sharedData.thisLoopIndex == 0) {
				mustUpdateScore = scoreUpdated = false;
			}
			// if we're staying in the same loop, when the currently playing bar is displayed at the right-most staff
			// all the other bars must display the next bars of the loop (if there are any)
			else if (((int)sharedData.thisLoopIndex % sharedData.numBars) == sharedData.numBars - 1 && i < sharedData.numBars - 1) {
				bar = sharedData.loopData[sharedData.loopIndex][(barNdx+sharedData.numBars)%(int)sharedData.loopData[sharedData.loopIndex].size()];
				// the calculations below determine whether the remaining bars are less than sharedData.numBars - 1
				// (we subtract one because in the last slot, we display the bar of the current loop chunk)
				// in which case, we should leave the remaining slots empty
				// for example, in a 6-bar loop with sharedData.numBars = 4, when we enter the second chunk of the loop
				// we need to display two bars
				// when at the last bar of the first chunk, we display the 5th and 6th bar on the left side
				// and the 4th bar on the right side, with the 3rd slot being blank
				int loopIndexLocal = sharedData.thisLoopIndex + 1;
				if (loopIndexLocal >= (int)sharedData.loopData[sharedData.loopIndex].size()) loopIndexLocal = 0;
				int ndxLocal = (((loopIndexLocal) - ((loopIndexLocal) % sharedData.numBars)) / sharedData.numBars) * sharedData.numBars;
				int numBarsLocal = (ndxLocal > 0 ? sharedData.numBars - ((int)sharedData.loopData[sharedData.loopIndex].size() - ndxLocal) : sharedData.numBars);
				if (i >= numBarsLocal) showBar = false;
				else showBar = true;
			}
			// like with the beat visualization, accumulate X offsets for all bars but the first
			if (i > 0) {
				// only the notes need to go back some pixels, in case the clef or meter is not drawn
				// so we use a separate value for the X coordinate
				staffOffsetX += sharedData.instruments.begin()->second.getStaffXLength();
				notesOffsetX += sharedData.instruments.begin()->second.getStaffXLength();
			}
			if (prevDrawClef != drawClef) {
				if (drawClef) notesOffsetX += sharedData.instruments.begin()->second.getClefXOffset();
				else notesOffsetX -= sharedData.instruments.begin()->second.getClefXOffset();
			}
			if (prevDrawMeter != drawMeter) {
				if (drawMeter) notesOffsetX += sharedData.instruments.begin()->second.getMeterXOffset();
				else notesOffsetX -= sharedData.instruments.begin()->second.getMeterXOffset();
			}
			// set position of single bars based on the position of the bars of the previous loop
			if (sharedData.numBars == 1 && scoreOrientation > 0) {
				// initially display the signle bar at the second position
				staffOffsetX += sharedData.instruments.begin()->second.getStaffXLength();
				notesOffsetX += sharedData.instruments.begin()->second.getStaffXLength();
				if ((sharedData.prevNumBars > 1 || (sharedData.prevNumBars == 1 && sharedData.prevPosition == 0)) && sequencer.isThreadRunning()) {
					// but the previous loop was a single bar at the second position, display the current single bar at the third position
					staffOffsetX += sharedData.instruments.begin()->second.getStaffXLength();
					notesOffsetX += sharedData.instruments.begin()->second.getStaffXLength();
					sharedData.thisPosition = 1;
				}
				else {
					sharedData.thisPosition = 0;
				}
			}
			float notesXCoef = notesLength * sharedData.instruments.begin()->second.getXCoef(); // sharedData.instruments.begin()->second.getStaffXLength();
			if (drawClef) notesXCoef -= sharedData.instruments.begin()->second.getClefXOffset();
			if (drawMeter) notesXCoef -= sharedData.instruments.begin()->second.getMeterXOffset();
			notesXCoef /= notesLength; // (notesLength * sharedData.instruments.begin()->second.getXCoef());
			// if we're displaying the beat pulse on each beat instead of a rectangle that encloses the whole score
			// we need to display it here, in case we display the score horizontally, where we need to give
			// an offset to the pulse so it is displayed on top of the currently playing bar
			if (sharedData.beatAnimate && sharedData.beatViz && sharedData.beatVizType == 2) {
				// draw only for the currenlty playing bar, whichever that is in the horizontal line
				if (barNdx == (int)sharedData.thisLoopIndex) {
					beatPulseStartX = notesOffsetX + ((notesLength  / sharedData.numerator[getPlayingBarIndex()]) * sharedData.beatCounter) * notesXCoef;
					//beatPulseStartX = ((notesLength * notesXCoef)  / sharedData.numerator[getPlayingBarIndex()]) * sharedData.beatCounter;
					//beatPulseStartX *= notesXCoef;
					//beatPulseStartX += notesOffsetX;
					if (sharedData.beatCounter == -1) beatPulseStartX = notesOffsetX - sharedData.instruments.begin()->second.getClefXOffset();
					noteSize = sharedData.instruments.begin()->second.getNoteWidth();
					beatPulsePosX = beatPulseStartX - noteSize;
					beatPulseSizeX = noteSize * 2;
					beatPulseMinPosY += scoreYOffset;
					ofSetColor(ofColor::paleGreen.r*brightnessCoeff,
							   ofColor::paleGreen.g*brightnessCoeff,
							   ofColor::paleGreen.b*brightnessCoeff,
							   sharedData.beatVizCosTab[sharedData.beatVizDegrade] * BEATVIZCOEFF);
					ofDrawRectangle(beatPulsePosX, beatPulseMinPosY, beatPulseSizeX, beatPulseSizeY);
					ofSetColor(0);
				}
			}
			float yStartPnt = sharedData.yStartPnts[scoreOrientation];
			unsigned j = 0;
			map<int, Instrument>::iterator prevInst;
			for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				if (i == 0) {
					// write the names of the instruments without the backslash and get the longest name
					// only at the first iteration of the bars loop
					float xPos = scoreXOffset + (sharedData.blankSpace * 0.75) + (sharedData.longestInstNameWidth - instNameWidths[it->first]);
					float yPos = yStartPnt + (sharedData.staffLinesDist*2.5) + scoreYOffset;
					instFont.drawString(it->second.getName(), xPos, yPos);
				}
				// then draw one line at each side of the staffs to connect them
				// we draw the first one here, and the rest inside the for loop below
				float connectingLineX = scoreXStartPnt + scoreXOffset;
				float connectingLineY2 = yStartPnt - sharedData.allStaffDiffs + (sharedData.staffLinesDist * 2);
				if (i > 0) {
					connectingLineX += sharedData.instruments.begin()->second.getStaffXLength();
				}
				if (j > 0) {
					ofDrawLine(connectingLineX, yStartPnt, connectingLineX, connectingLineY2);
					// if this instrument is groupped with the previous one, draw a connecting line at the end of the bar too
					if (it->second.getGroup() == prevInst->second.getID() && it->second.getGroup() > -1) {
						connectingLineX += sharedData.instruments.begin()->second.getStaffXLength();
						ofDrawLine(connectingLineX, yStartPnt, connectingLineX, connectingLineY2);
					}
				}
				bool drawTempoLocal = (showTempo && drawTempo ? true : false);
				if (showBar) {
					it->second.drawStaff(bar, staffOffsetX, yStartPnt, scoreYOffset, drawClef, drawMeter, drawLoopStartEnd, drawTempoLocal);
					it->second.drawNotes(bar, i, &sharedData.loopData[insertNaturalsNdx], notesOffsetX, yStartPnt, scoreYOffset, animate, notesXCoef);
				}
				yStartPnt += (sharedData.allStaffDiffs + (sharedData.staffLinesDist * 2));
				j++;
				prevInst = it;
			}
			prevMeter.first = thisMeter.first;
			prevMeter.second = thisMeter.second;
			prevTempo.first = thisTempo.first;
			prevTempo.second = thisTempo.second;
			prevDrawClef = drawClef;
			prevDrawMeter = drawMeter;
			//prevNotesOffsetX = notesOffsetX;
			//prevBar = bar;
		}
	}
}

//--------------------------------------------------------------
// the function that is called when a key is pressed
// or when a key press is simulated from OSC
void ofApp::executeKeyPressed(int key)
{
	switch (key) {
		case 1:
			editors[whichPane].setShiftPressed(true);
			shiftPressed = true;
	    	break;
	    case 2:
			editors[whichPane].setCtrlPressed(true);
			ctrlPressed = true;
	    	break;
	    case 4:
			editors[whichPane].setAltPressed(true);
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
	    	editors[whichPane].upArrow(-1); // -1 for single increment
	    	break;
	    case 57359:
	    	editors[whichPane].downArrow(-1); // -1 for single decrement
	    	break;
	    case 57358:
	    	editors[whichPane].rightArrow();
	    	break;
	    case 57356:
	    	editors[whichPane].leftArrow();
	    	break;
	    case 57360:
	    	editors[whichPane].pageUp();
	    	break;
	    case 57361:
	    	editors[whichPane].pageDown();
	    	break;
		// esc
		case 27:
			editors[whichPane].setInserting(false);
			editors[whichPane].commandStr = "";
			editors[whichPane].resetMaxNumLines();
	    default:
	    	if (key == 61 && ctrlPressed && !altPressed) { // +
	    		fontSize += 2;
				setFontSize();
	    	}
	    	else if (key == 45 && ctrlPressed && !altPressed) { // -
	    		fontSize -= 2;
	    		if (fontSize == 0) fontSize = 2;
				setFontSize();
	    	}
	    	else if (key == 43 && ctrlPressed) { // + with shift pressed
				sharedData.staffLinesDist += 2;
				sharedData.scoreFontSize = (int)(35.0 * sharedData.staffLinesDist / 10.0);
	    		instFontSize = sharedData.scoreFontSize / 3.5;
	    		instFont.load("Monaco.ttf", instFontSize);
	    		setScoreSizes();
	    	}
	    	else if (key == 95 && ctrlPressed) { // - with shift pressed
				sharedData.staffLinesDist -= 2;
	    		if (sharedData.staffLinesDist < 4) sharedData.staffLinesDist = 4;
				sharedData.scoreFontSize = (int)(35.0 * sharedData.staffLinesDist / 10.0);
	    		instFontSize = sharedData.scoreFontSize / 3.5;
	    		instFont.load("Monaco.ttf", instFontSize);
	    		setScoreSizes();
	    	}
	    	else if (key == 102 && ctrlPressed) { // f
	    		ofToggleFullscreen();
	    		fullScreen = !fullScreen;
	    	}
	    	else if (char(key) == 'q' && ctrlPressed) {
	    		exit();
	    	}
	    	// add panes. V for vertical, H for horizontal (with shift pressed)
	    	else if (((key == 86) || (key == 72)) && ctrlPressed) {
				addPane(key);
			}
	    	// remove panes. Ctrl+w, where if only one pane, we quit 
	    	else if (key == 119 && ctrlPressed) {
				removePane();
	    	}
	    	// set which editor is active
	    	else if ((key >= 49) && (key <= 57) && altPressed) {
	    		if ((key - 49) < (int)editors.size()) {
	    			editors[whichPane].setActivity(false);
					// set all hot keys to false
					editors[whichPane].setShiftPressed(false);
					editors[whichPane].setCtrlPressed(false);
					editors[whichPane].setAltPressed(false);
	    			whichPane = key - 49;
	    			editors[whichPane].setActivity(true);
	    		}
	    	}
	    	// swap score position with Ctrl+P for horizontal
	    	// Ctrl+Shift+P for vertical
			// Ctrl+Alt+Shift+P for full screen
	    	else if ((key == 112) && ctrlPressed) {
	    		swapScorePosition(0);
	    	}
	    	else if ((key == 80) && ctrlPressed && !altPressed) {
	    		swapScorePosition(1);
	    	}
			else if ((key == 80) && ctrlPressed && altPressed) {
				swapScorePosition(2);
			}
			else if (key == 105 && !editors[whichPane].getInserting() && !editors[whichPane].isTypingCommand()) {
				editors[whichPane].setInserting(true);
				editors[whichPane].commandStr = "";
				editors[whichPane].resetMaxNumLines();
			}
			else if ((key == 106 || key == 74) && !editors[whichPane].getInserting() && !editors[whichPane].isTypingCommand()) {
				editors[whichPane].downArrow(-1); // -1 for single increment
			}
			else if ((key == 107 || key == 75) && !editors[whichPane].getInserting() && !editors[whichPane].isTypingCommand()) {
				editors[whichPane].upArrow(-1); // -1 for single decrement
			}
			else if ((key == 104 || key == 72) && !editors[whichPane].getInserting() && !editors[whichPane].isTypingCommand()) {
				editors[whichPane].leftArrow();
			}
			else if ((key == 108 || key == 76) && !editors[whichPane].getInserting() && !editors[whichPane].isTypingCommand()) {
				editors[whichPane].rightArrow();
			}
			else if ((key == 102 && ctrlPressed) && !editors[whichPane].getInserting() && !editors[whichPane].isTypingCommand()) {
				editors[whichPane].pageDown();
			}
			else if ((key == 98 && ctrlPressed) && !editors[whichPane].getInserting() && !editors[whichPane].isTypingCommand()) {
				editors[whichPane].pageUp();
			}
			else if (key == 121 && !editors[whichPane].getInserting() && !editors[whichPane].isTypingCommand()) {
				editors[whichPane].yankString();
			}
			else if (key == 112 && !editors[whichPane].getInserting() && !editors[whichPane].isTypingCommand()) {
				editors[whichPane].pasteYankedString();
			}
	    	else if (editors[whichPane].getInserting()) {
				editors[whichPane].allOtherKeys(key);
			}
			else if (!editors[whichPane].getInserting() && (key == 58 || editors[whichPane].isTypingCommand())) {
				editors[whichPane].typeCommand(key);
			}
			break;
	}
}

//--------------------------------------------------------------
// the function that is called when a key is released
// or when a key release is simulated from OSC
void ofApp::executeKeyReleased(int key)
{
	switch (key) {
	    case 1:
	    	editors[whichPane].setShiftPressed(false);
	    	shiftPressed = false;
	    	break;
	    case 2:
	    	editors[whichPane].setCtrlPressed(false);
	    	ctrlPressed = false;
	    	break;
	    case 4:
	    	editors[whichPane].setAltPressed(false);
	    	altPressed = false;
	    	break;
	}
}

//--------------------------------------------------------------
// overloaded keyPressed() function called from OSC
void ofApp::keyPressedOsc(int key, int thisEditor)
{
	//sharedData.mutex.lock();
	int whichPaneBackup = whichPane;
	// change the currently active editor temporarily
	// because this overloaded function is called from an editor
	// when it receives input from OSC
	// this way, even if an editor is out of focus
	// we're still able to type in it properly
	whichPane = thisEditor;
	executeKeyPressed(key);
	whichPane = whichPaneBackup;
	//sharedData.mutex.unlock();
}

//--------------------------------------------------------------
// overloaded keyReleased() function called from OSC
void ofApp::keyReleasedOsc(int key, int thisEditor)
{
	// the same applies to releasing a key
	//sharedData.mutex.lock();
	int whichPaneBackup = whichPane;
	whichPane = thisEditor;
	executeKeyReleased(key);
	whichPane = whichPaneBackup;
	//sharedData.mutex.unlock();
}

//--------------------------------------------------------------
// OF's default keyPressed() function
void ofApp::keyPressed(int key)
{
	//sharedData.mutex.lock();
	if (editors[whichPane].getSendKeys()) {
		ofxOscMessage m;
		m.setAddress("/livelily"+to_string(editors[whichPane].getSendKeysPaneNdx())+"/press");
		m.addIntArg(key);
		editors[whichPane].oscKeys.sendMessage(m, false);
	}
	executeKeyPressed(key);
	//sharedData.mutex.unlock();
}

//--------------------------------------------------------------
// OF's default keyReleased() function
void ofApp::keyReleased(int key)
{
	//sharedData.mutex.lock();
	if (editors[whichPane].getSendKeys()) {
		ofxOscMessage m;
		m.setAddress("/livelily"+to_string(editors[whichPane].getSendKeysPaneNdx())+"/release");
		m.addIntArg(key);
		editors[whichPane].oscKeys.sendMessage(m, false);
	}
	executeKeyReleased(key);
	//sharedData.mutex.unlock();
}

//--------------------------------------------------------------
void ofApp::addPane(int key)
{
	Editor editor;
	editor.setID(whichPane+1);
	bool firstAddition = false;
	if (editors.size() == 1) firstAddition = true;
	// first move the keys of all the editors
	for (map<int, Editor>::reverse_iterator editRevIt = editors.rbegin(); editRevIt != editors.rend(); ++editRevIt) {
		if (editRevIt->first > whichPane) {
			int editKey = editRevIt->first;
			auto editNodeHolder = editors.extract(editKey);
			editNodeHolder.key() = editKey + 1;
			editors.insert(std::move(editNodeHolder));
			editors[editKey+1].setID(editRevIt->second.getID()+1);
		}
	}
	editors[whichPane+1] = editor;
	setFontSize();
	if (firstAddition) {
		if (key == 86) paneSplitOrientation = 1;
		else paneSplitOrientation = 0;
	}
	if ((paneSplitOrientation == 0 && key == 86) || (paneSplitOrientation == 1 && key == 72)) {
		int paneRowWithCursor = editors[whichPane].getPaneRow();
		numPanes[paneRowWithCursor]++;
		editors[whichPane+1].setPaneRow(paneRowWithCursor);
		//cout << "created a new pane in row " << editors[whichPane+1].getPaneRow() << endl;
		editors[whichPane+1].setPaneCol(numPanes[paneRowWithCursor]);
		//cout << "added a pane column in row " << paneRowWithCursor << endl;
	}
	else {
		int paneRowWithCursor = editors[whichPane].getPaneRow();
		map<int, int>::iterator numPanesIt = numPanes.find(paneRowWithCursor);
		map<int, int>::reverse_iterator numPanesRevIt = numPanes.rbegin();
		//cout << "testing " << numPanesIt->first << " against " << numPanesRevIt->first << endl;
		if (numPanesIt != numPanes.end()) {
			// first check if we are at the last row of editors
			if (numPanesIt->first == numPanesRevIt->first) {
				// in this case, we add a new row that now contains one column
				numPanes[numPanesIt->first+1] = 1;
				editors[whichPane+1].setPaneRow(numPanesIt->first+1);
				editors[whichPane+1].setPaneCol(1);
			}
			else {
				// otherwise we insert a row somewhere in the middle
				// so we first have to change the keys of all rows after the current one
				for (numPanesRevIt = numPanes.rbegin(); numPanesRevIt->first > paneRowWithCursor; ++numPanesRevIt) {
					int key = numPanesRevIt->first;
					auto nodeHolder = numPanes.extract(key);
					nodeHolder.key() = key + 1;
					numPanes.insert(std::move(nodeHolder));
					// find which editors are contained in the panes that change rows
					for (map<int, Editor>::reverse_iterator editRevIt = editors.rbegin(); editRevIt != editors.rend(); ++editRevIt) {
						if (editRevIt->second.getPaneRow() == key) {
							editRevIt->second.setPaneRow(key+1);
						}
					}
				}
				numPanes[paneRowWithCursor+1] = 1;
				editors[whichPane+1].setPaneRow(paneRowWithCursor+1);
				editors[whichPane+1].setPaneCol(1);
			}
		}
	}
	editors[whichPane].setActivity(false);
	// release the Ctrl key from the previous pane before moving to the new one
	editors[whichPane].setCtrlPressed(false);
	// for safety, set also the shift and alt keys to released
	editors[whichPane].setShiftPressed(false);
	editors[whichPane].setAltPressed(false);
	whichPane++;
	editors[whichPane].setActivity(true);
	setPaneCoords();
}

//--------------------------------------------------------------
void ofApp::removePane()
{
	if (numPanes.size() == 1) {
		exit();
	}
	else {
		int paneRowWithCursor = editors[whichPane].getPaneRow();
		//cout << "will remove pane from row " << paneRowWithCursor << endl;
		// check if we're erasing the last pane, to determine which pane will become active
		bool isLastPane = (editors.rbegin()->first == whichPane ? true : false);
		editors.erase(whichPane);
		// if the current row has only one pane, remove it from the map
		if (numPanes[paneRowWithCursor] == 1) {
			numPanes.erase(paneRowWithCursor);
			// changes the keys of all panes with greater index than the one we just erased
			for (map<int, int>::iterator numPanesIt = numPanes.find(paneRowWithCursor+1); numPanesIt != numPanes.end(); ++numPanesIt) {
				int key = numPanesIt->first;
				auto nodeHolder = numPanes.extract(key);
				nodeHolder.key() = key-1;
				numPanes.insert(std::move(nodeHolder));
				// find all the editors located in this row and change it as well as their keys
				for (map<int, Editor>::iterator editIt = editors.begin(); editIt != editors.end(); ++editIt) {
					if (editIt->second.getPaneRow() == key) {
						editIt->second.setPaneRow(key-1);
						int editKey = editIt->first;
						auto editNodeHolder = editors.extract(editKey);
						editNodeHolder.key() = editKey - 1;
						editors.insert(std::move(editNodeHolder));
					}
				}
			}
		}
		else {
			// find the editors in the same row that are above the column we are erasing
			// or editors that are in greater rows than the one where we are erasing a column
			for (map<int, Editor>::iterator editIt = editors.begin(); editIt != editors.end(); ++editIt) {
				if (editIt->second.getPaneRow() >= paneRowWithCursor) {
					// for the editors in panes of the same row where we erase a column
					// change their column number
					if ((editIt->second.getPaneRow() == paneRowWithCursor && editIt->second.getPaneCol() > numPanes[paneRowWithCursor]) ||
							editIt->second.getPaneRow() > paneRowWithCursor) {
						if (editIt->second.getPaneRow() == paneRowWithCursor) {
							editIt->second.setPaneCol(numPanes[paneRowWithCursor]-1);
						}
						// for all editors in the same row and greater column or in a greater row
						// change their index
						int editKey = editIt->first;
						auto editNodeHolder = editors.extract(editKey);
						editNodeHolder.key() = editKey - 1;
						editors.insert(std::move(editNodeHolder));
					}
				}
			}
			numPanes[paneRowWithCursor]--;
		}
		// if we deleted the last pane we must decrement the pane index
		// otherwise it stays as it is, and the next pane becomes active
		if (isLastPane) whichPane--;
		editors[whichPane].setActivity(true);
		// if we're left with one pane only, reset paneSplitOrientation
		if (numPanes.size() == 1) paneSplitOrientation = 0;
		setPaneCoords();
	}
}

//--------------------------------------------------------------
// the following two functions are used in drawTraceback to sort the indexes based on the
// time stamps. it is copied from https://www.geeksforgeeks.org/quick-sort/ 
int ofApp::partition(uint64_t arr[], int arr2[], int low,int high)
{
	//choose the pivot
	uint64_t pivot = arr[high];
	//Index of smaller element and Indicate
	//the right position of pivot found so far
	int i = low-1;

	for(int j = low; j <= high; j++) {
		//If current element is smaller than the pivot
		if (arr[j] < pivot) {
		//Increment index of smaller element
		i++;
		swap(arr[i], arr[j]);
		swap(arr2[i], arr2[j]);
		}
	}
	swap(arr[i+1], arr[high]);
	swap(arr2[i+1], arr2[high]);
	return (i+1);
}

// The Quicksort function Implementation
void ofApp::quickSort(uint64_t arr[], int arr2[], int low, int high)
{
	// when low is less than high
	if(low<high)
	{
		// pi is the partition return index of pivot
		int pi = partition(arr, arr2, low, high);
		//Recursion Call
		//smaller element than pivot goes left and
		//higher element goes right
		quickSort(arr, arr2, low, pi-1);
		quickSort(arr, arr2, pi+1, high);
	}
}

//--------------------------------------------------------------
void ofApp::sendToParts(ofxOscMessage m, bool delay)
{
	for (auto it = grouppedOSCClients.begin(); it != grouppedOSCClients.end(); ++it) {
		if (sharedData.instruments[*it].hasDelay() && delay) {
			std::pair<ofxOscMessage, unsigned long> p = std::make_pair(m, ofGetElapsedTimeMillis());
			sharedData.instruments[*it].oscFifo.push_back(p);
		}
		else {
			sharedData.instruments[*it].scorePartSender.sendMessage(m, false);
		}
	}
}

//--------------------------------------------------------------
void ofApp::sendBarToParts(int barIndex)
{
	ofxOscMessage m;
	m.setAddress("/bar");
	m.addStringArg(sharedData.barsOrdered[barIndex]);
	sendToParts(m, false);
	m.clear();
	for (auto inst = sharedData.instruments.begin(); inst != sharedData.instruments.end(); ++inst) {
		if (inst->second.sendToPart) {
			m.setAddress("/line");
			m.addStringArg(inst->second.barLines[barIndex]);
			inst->second.scorePartSender.sendMessage(m, false);
			m.clear();
		}
	}
	m.setAddress("/bar");
	m.addIntArg(0); // add an int argument to separate it from any possible bar name
	sendToParts(m, false);
}

void ofApp::sendLoopToParts()
{
	int lastLoopNdx = getLastLoopIndex();
	ofxOscMessage m;
	m.setAddress("/loop");
	m.addStringArg(sharedData.loopsOrdered[lastLoopNdx]);
	m.addIntArg(lastLoopNdx);
	for (unsigned i = 0; i < sharedData.loopData[lastLoopNdx].size(); i++) {
		m.addIntArg(sharedData.loopData[lastLoopNdx].at(i));
	}
	sendToParts(m, false);
}

//--------------------------------------------------------------
void ofApp::sendCountdownToParts(int countdown)
{
	ofxOscMessage m;
	m.setAddress("/countdown");
	m.addIntArg(countdown);
	sendToParts(m, true);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendStopCountdownToParts()
{
	ofxOscMessage m;
	m.setAddress("/nocountdown");
	m.addIntArg(1); // dummy arg
	sendToParts(m, true);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendSizeToPart(int instNdx, int size)
{
	ofxOscMessage m;
	m.setAddress("/size");
	m.addIntArg(size);
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
}

//--------------------------------------------------------------
void ofApp::sendNumBarsToPart(int instNdx, int numBars)
{
	ofxOscMessage m;
	m.setAddress("/numbars");
	m.addIntArg(numBars);
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
}

//--------------------------------------------------------------
void ofApp::sendAccOffsetToPart(int instNdx, float accOffset)
{
	ofxOscMessage m;
	m.setAddress("/accoffset");
	m.addFloatArg(accOffset);
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
}

//--------------------------------------------------------------
void ofApp::sendLoopIndexToParts()
{
	ofxOscMessage m;
	m.setAddress("/loopndx");
	m.addIntArg(sharedData.loopIndex);
	sendToParts(m, false);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendSequencerStateToParts(bool state)
{
	ofxOscMessage m;
	m.setAddress("/seq");
	m.addBoolArg(state);
	sendToParts(m, false);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendScoreChangeToPart(int instNdx, bool scoreChange)
{
	ofxOscMessage m;
	m.setAddress("/scorechange");
	m.addBoolArg(scoreChange);
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendChangeBeatColorToPart(int instNdx, bool changeBeatColor)
{
	ofxOscMessage m;
	m.setAddress("/beatcolor");
	m.addBoolArg(changeBeatColor);
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendFullscreenToPart(int instNdx, bool fullscreen)
{
	ofxOscMessage m;
	m.setAddress("/fullscreen");
	m.addBoolArg(fullscreen);
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendCursorToPart(int instNdx, bool cursor)
{
	ofxOscMessage m;
	m.setAddress("/cursor");
	m.addBoolArg(cursor);
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendFinishToParts(bool finishState)
{
	ofxOscMessage m;
	m.setAddress("/finish");
	m.addBoolArg(finishState);
	sendToParts(m, true);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::printVector(vector<int> v)
{
	for (int elem : v) {
		cout << elem << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
void ofApp::printVector(vector<string> v)
{
	for (string elem : v) {
		cout << elem << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
void ofApp::printVector(vector<float> v)
{
	for (float elem : v) {
		cout << elem << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
bool ofApp::startsWith(string a, string b)
{
	if (a.size() < b.size()) return false;
	string aLocal = a.substr(0, b.size());
	if (aLocal.compare(b) == 0) return true;
	return false;
}

//--------------------------------------------------------------
bool ofApp::endsWith(string a, string b)
{
	if (a.size() < b.size()) return false;
	string aLocal = a.substr(a.size() - b.size(), b.size());
	if (aLocal.compare(b) == 0) return true;
	return false;
}

//--------------------------------------------------------------
bool ofApp::isNumber(string str)
{
	if (str.empty()) return false;
	// first check if there is a hyphen in the beginning
	int loopStart = 0;
	if (str[0] == '-') loopStart = 1;
	if (str.size() == 0) return false;
	for (int i = loopStart; i < (int)str.length(); i++) {
		if (!isdigit(str[i])) {
			return false;
		}
	}
	return true;
}

//--------------------------------------------------------------
bool ofApp::isFloat(string str)
{
	if (str.empty()) return false;
	// first check if there is a hyphen in the beginning
	int loopStart = 0;
	int dotCounter = 0;
	if (str[0] == '-') loopStart = 1;
	for (int i = loopStart; i < (int)str.length(); i++) {
		if (!isdigit(str[i])) {
			if (str[i] == '.') {
				if (dotCounter == 0) dotCounter++;
				else return false;
			}
			else return false;
		}
	}
	return true;
}

//-------------------------------------------------------------
vector<int> ofApp::findRepetitionInt(string str, int multIndex)
{
	vector<int> numRepNumDigits(2);
	int index = findNextStrCharIdx(str, " ", multIndex);
	int numDigits = index - multIndex - 1;
	if (isNumber(str.substr(multIndex+1, numDigits))) {
		numRepNumDigits[0] = stoi(str.substr(multIndex+1, numDigits));
		numRepNumDigits[1] = numDigits;
	}
	return numRepNumDigits;
}

//--------------------------------------------------------------
int ofApp::findNextStrCharIdx(string str, string compareStr, int index)
{
	while (index < (int)str.size()) {
		if (str.substr(index, 1).compare(compareStr) == 0) {
			break;
		}
		index++;
	}
	return index;
}

//--------------------------------------------------------------
// the following two functions are inspired by
// https://www.geeksforgeeks.org/check-for-balanced-parentheses-in-an-expression/
bool ofApp::areBracketsBalanced(string str)
{
	// Declare a stack to hold the previous brackets.
	size_t i;
	stack<char> temp;
	for (i = 0; i < str.length(); i++) {
		if (str[i] == '{' || str[i] == '[') {
			temp.push(str[i]);
		}
		else if (str[i] == '}' || str[i] == ']') {
			if (temp.empty()) return false;
			if ((temp.top() == '{' && str[i] == '}')
				|| (temp.top() == '[' && str[i] == ']')) {
				temp.pop();
			}
		}
	}
	if (temp.empty()) {     
		// If stack is empty return true
		return true;
	}
	return false;
}

//--------------------------------------------------------------
bool ofApp::areParenthesesBalanced(string str)
{
	// Declare a stack to hold the previous brackets.
	size_t i;
	stack<char> temp;
	for (i = 0; i < str.length(); i++) {
		if (str[i] == '(') {
			temp.push(str[i]);
		}
		else if (str[i] == ')') {
			if (temp.empty()) return false;
			if (temp.top() == '(') {
				temp.pop();
			}
		}
	}
	if (temp.empty()) {     
		// If stack is empty return true
		return true;
	}
	return false;
}

//--------------------------------------------------------------
std::pair<int, string> ofApp::expandStringBasedOnBrackets(const string& str)
{
	int i = 0;
	stack<int> braceNdx;
	// we stack the opening square brackets and as soon as we find a closing one
	// we are in the deepest nest, if brackets are nested
	// we extract the inner part of this deepest nest and repeat it as many times as we define
	// with the multiplication, then we return the string and this function is called again
	// until all opening square brackets have been exhausted
	for (char c : str) {
		if (c == '[') {
			braceNdx.push(i);
		}
		else if (c == ']') {
			int closingBraceNdx = i;
			if (!braceNdx.empty()) {
				int openingBraceNdx = braceNdx.top();
				size_t strLength = closingBraceNdx - openingBraceNdx - 1;
				auto revit = str.substr(openingBraceNdx+1, strLength).rbegin();
				// a string to be expanded might have a white space before the closing square bracket
				// so we need to determine if this is the case and not repeat this space
				int revOffset = (revit[0] == ' ' ? 1 : 0);
				// the same stands for the beginning of the string
				auto it = str.substr(openingBraceNdx+1, strLength).begin();
				int offset = (it[0] == ' ' ? 1 : 0);
				string enclosedString = str.substr(openingBraceNdx+1+offset, strLength-revOffset);
				int multiplier = 0;
				int digitNdx = 0;
				if ((int)str.size() > closingBraceNdx + 2) {
					digitNdx = closingBraceNdx + 2;
					if (!isdigit(str[digitNdx])) {
						std::pair p (1, str);
						return p;
					}
					while (digitNdx > 0 && isdigit(str[digitNdx])) {
						multiplier = multiplier * 10 + str[digitNdx] - '0';
						digitNdx++;
					}
				}
				else {
					std::pair p (1, str);
					return p;
				}
				string enclosedCopy = enclosedString;
				for (int j = 1; j < multiplier; j++) {
					enclosedCopy += " ";
					enclosedCopy += enclosedString;
				}
				if (openingBraceNdx == 0) {
					if (str.substr(closingBraceNdx+2).size() > 0) {
						enclosedCopy += str.substr(digitNdx);
					}
					if (endsWith(enclosedCopy, "|")) enclosedCopy = enclosedCopy.substr(0, enclosedCopy.size()-1);
					if (endsWith(enclosedCopy, " ")) enclosedCopy = enclosedCopy.substr(0, enclosedCopy.size()-1);
					std::pair p (0, enclosedCopy);
					return p;
				}
				string expandedString = str.substr(0, openingBraceNdx);
				expandedString += enclosedCopy;
				if (str.substr(digitNdx).size() > 0) {
					expandedString += str.substr(digitNdx);
				}
				if (endsWith(expandedString, "|")) expandedString = expandedString.substr(0, expandedString.size()-1);
				if (endsWith(expandedString, " ")) expandedString = expandedString.substr(0, expandedString.size()-1);
				std::pair p (0, expandedString);
				return p;
			}
			else {
				std::pair p (1, str);
				return p;
			}
		}
		i++;
	}
	// in case no brackets have been found
	std::pair p (0, str);
	return p;
}

//--------------------------------------------------------------
std::pair <int ,string> ofApp::expandChordsInString(const string& firstPart, const string& str)
{
	// in contrast to expandStringBasedOnBrackets() and expandSingleChars()
	// here we are looking for angle brackets, which are not removed after expansion
	// for this reason, the int in the returning pair is not a 0 or 1 error code
	// but the size of the substring after expansion
	// what happens is that this function looks for the first chord to be expanded
	// and once this is done, it returns this together with anything that precedes or follows it
	// and in detectRepetitions() we run a loop where we are looking for the next opening angle brace
	// in the substring after the returned expanded string
	unsigned i = 0;
	stack<unsigned> braceNdx;
	for (char c : str) {
		if (c == '<') {
			if (i == 0) {
				braceNdx.push(i);
			}
			else if (str[i-1] != '\\') {
				braceNdx.push(i);
			}
		}
		else if (c == '>') {
			if (str[i-1] == '\\') continue;
			bool chordRepeats = true;
			unsigned closingBraceNdx = i;
			if (!braceNdx.empty()) {
				unsigned openingBraceNdx = braceNdx.top();
				string firstPartLocal = str.substr(0, openingBraceNdx);
				while (closingBraceNdx < str.size()) {
					if (str[closingBraceNdx] == '*') break;
					closingBraceNdx++;
				}
				// a chord that does not repeat might already exist
				// so we need to check how far the closing index has moved
				// since the shortest duration is 64, we can only have two digits
				if (closingBraceNdx > i+2) {
					while (!braceNdx.empty()) braceNdx.pop();
					chordRepeats = false;
				}
				if (chordRepeats) {
					size_t strLength = closingBraceNdx - openingBraceNdx;
					auto it = str.substr(openingBraceNdx, strLength).rbegin();
					int offset = (it[0] == ' ' ? 1 : 0);
					int offset2 = (it[0] == ' ' ? 1 : 0);
					string enclosedString = str.substr(openingBraceNdx+offset2, strLength-offset);
					int multiplier = 0;
					unsigned digitNdx = 0;
					if (str.size() > closingBraceNdx + 1) {
						digitNdx = closingBraceNdx + 1;
						if (!isdigit(str[digitNdx])) {
							std::pair p ((int)str.size(), str);
							return p;
						}
						while (digitNdx > 0 && digitNdx < str.size()) {
							if (!isdigit(str[digitNdx])) break;
							multiplier = multiplier * 10 + str[digitNdx] - '0';
							digitNdx++;
						}
					}
					else {
						std::pair p ((int)str.size(), str);
						return p;
					}
					int j;
					string enclosedCopy = enclosedString;
					for (j = 1; j < multiplier; j++) {
						enclosedCopy += " ";
						enclosedCopy += enclosedString;
					}
					string whole = firstPart + firstPartLocal + enclosedCopy;
					int endOfStr = (int)whole.size();
					if (str.substr(digitNdx).size() > 0) {
						whole += str.substr(digitNdx);
					}
					std::pair p (endOfStr, whole);
					return p;
				}
				else {
					std::pair p ((int)(firstPart.size() + str.size()), firstPart+str);
					return p;
				}
			}
			else {
				std::pair p ((int)str.size(), str);
				return p;
			}
		}
		i++;
	}
	// in case no brackets have been found
	std::pair p ((int)str.size(), str);
	return p;
}

//--------------------------------------------------------------
std::pair<int, string> ofApp::expandSingleChars(string str)
{
	size_t multNdx = str.find("*");
	size_t whiteSpaceBefore = str.substr(0, multNdx).find_last_of(" ");
	size_t quoteIndex = str.substr(0, multNdx).find_last_of("\"");
	int offset = 1;	
	string firstPart = "";
	if (whiteSpaceBefore != string::npos) {
		// if there's a quote sign after the last white space
		if ((quoteIndex != string::npos && quoteIndex > whiteSpaceBefore)) {
			// spot the quote sign before the one we already found (which is the opening quote sing)
			quoteIndex = str.substr(0, quoteIndex).find_last_of("\"");
			// and move the white space index accordingly
			whiteSpaceBefore = str.substr(0, quoteIndex).find_last_of(" ");
			if (whiteSpaceBefore != string::npos) {
				firstPart = str.substr(0, whiteSpaceBefore) + " ";
			}
			else {
				whiteSpaceBefore = offset = 0;
			}
		}
		else {
			firstPart = str.substr(0, whiteSpaceBefore) + " ";
		}
	}
	else {
		whiteSpaceBefore = offset = 0;
	}
	size_t strToRepeatLength = multNdx - whiteSpaceBefore - offset;
	string strToRepeat = str.substr(whiteSpaceBefore+offset, strToRepeatLength);
	int multiplier = 0;
	size_t digitNdx = multNdx + 1;
	if (digitNdx >= str.size()) {
		std::pair p (1, str);
		return p;
	}
	if (!isdigit(str[digitNdx])) {
		std::pair p (1, str);
		return p;
	}
	while (digitNdx > 0 && digitNdx < str.size()) {
		if (!isdigit(str[digitNdx])) break;
		multiplier = multiplier * 10 + str[digitNdx] - '0';
		digitNdx++;
	}
	int i;
	string strToRepeatCopy = strToRepeat;
	for (i = 1; i < multiplier; i++) {
		strToRepeatCopy += " ";
		strToRepeatCopy += strToRepeat;
	}
	string wholeString = firstPart + strToRepeatCopy;
	if (str.length() >= digitNdx+1) {
		wholeString += " ";
		wholeString += str.substr(digitNdx+1);
	}
	std::pair p (0, wholeString);
	return p;
}

//--------------------------------------------------------------
vector<string> ofApp::detectRepetitions(vector<string> tokens)
{
	vector<string> output;
	size_t openingSquareBracketNdx, closingSquareBracketNdx;
	bool foundOpeningBracket = false;
	for (size_t i = 0; i < tokens.size(); i++) {
		size_t closingSquareBracket = tokens[i].find("]");
		if (startsWith(tokens[i], "[")) {
			openingSquareBracketNdx = i;
			foundOpeningBracket = true;
		}
		if (!foundOpeningBracket) {
			size_t multNdx = tokens[i].find("*");
			if (multNdx != string::npos) {
				if (multNdx < tokens[i].size()-1 && isNumber(tokens[i].substr(multNdx+1))) {
					int numRepetitions = stoi(tokens[i].substr(multNdx+1));
					for (int j = 0; j < numRepetitions; j++) {
						output.push_back(tokens[i].substr(0, multNdx));
					}
				}
			}
			else {
				output.push_back(tokens[i]);
			}
		}
		if (closingSquareBracket != string::npos) {
			closingSquareBracketNdx = i;
			if (closingSquareBracket < tokens[i].size()-1 && tokens[i][closingSquareBracket+1] == '*') {
				if (closingSquareBracket < tokens[i].size()-2 && isNumber(tokens[i].substr(closingSquareBracket+2))) {
					int numRepetitions = stoi(tokens[i].substr(closingSquareBracket+2));
					for (int i = 0; i < numRepetitions; i++) {
						for (size_t j = openingSquareBracketNdx; j <= closingSquareBracketNdx; j++) {
							if (j == openingSquareBracketNdx) output.push_back(tokens[j].substr(1));
							else if (j == closingSquareBracketNdx) output.push_back(tokens[j].substr(0, closingSquareBracket));
							else output.push_back(tokens[j]);
						}
					}
					foundOpeningBracket = false;
				}
				else {
					for (size_t j = openingSquareBracketNdx; j <= closingSquareBracketNdx; j++) {
						output.push_back(tokens[j]);
					}
				}
			}
			else {
				for (size_t j = openingSquareBracketNdx; j <= closingSquareBracketNdx; j++) {
					output.push_back(tokens[j]);
				}
			}
		}
	}
	return output;
}

//---------------------------------------------------------------
vector<int> ofApp::findIndexesOfCharInStr(string str, string charToFind)
{
	size_t pos = str.find(charToFind, 0);
	vector<int> tokensIndexes;
	while (pos != string::npos) {
        tokensIndexes.push_back((int)pos);
        pos = str.find(charToFind, pos+1);
	}
	return tokensIndexes;
}

//---------------------------------------------------------------
string ofApp::replaceCharInStr(string str, string a, string b)
{
	auto it = str.find(a);
	while (it != string::npos) {
		str.replace(it, a.size(), b);
    it = str.find(a);
	}
	return str;
}

//---------------------------------------------------------------
vector<string> ofApp::tokenizeString(string str, string delimiter)
{
	size_t start = 0;
	size_t end = str.find(delimiter);
	vector<string> tokens;
	while (end != string::npos) {
		tokens.push_back(str.substr(start, end));
		start += end + 1;
		end = str.substr(start).find(delimiter);
	}
	// the last token is not extracted in the loop above because end has reached string::npos
	// so we extract it here by simply passing a substring from the last start point to the end
	tokens.push_back(str.substr(start));
	return tokens;
}

//---------------------------------------------------------------
map<size_t, string> ofApp::tokenizeStringWithNdxs(string str, string delimiter)
{
	size_t start = 0;
	size_t end = str.find(delimiter);
	map<size_t, string> m;
	vector<string> tokens;
	vector<size_t> ndxs;
	while (end != string::npos) {
		m[start] = str.substr(start,end);
		start += end + 1;
		end = str.substr(start).find(delimiter);
	}
	// the last token is not extracted in the loop above because end has reached string::npos
	// so we extract it here by simply passing a substring from the last start point to the end
	m[start] = str.substr(start);
	return m;
}

//--------------------------------------------------------------
int ofApp::findMatchingBrace(const std::string& s, size_t openPos)
{
	int depth = 0;
	for (size_t i = openPos; i < s.length(); ++i) {
		if (s[i] == '{') ++depth;
		else if (s[i] == '}') {
			--depth;
			if (depth == 0) return i;
		}
	}
	return -1;
}

//--------------------------------------------------------------
void ofApp::parseStrings(int index, int numLines)
{
	bool noErrors = true;
	barError = false;
	vector<std::pair<int, string>> errors;
	for (int i = index; i < (index+numLines); i++) {
		// parseString() returns a pair of int and string, where the int determines the error type
		// 0 for nothing, 1 for note, 2 for warning, and 3 for error
		if (editors[whichPane].getSessionActivity()) {
			errors.push_back(parseString(editors[whichPane].allStrings[i], i, numLines));
			if (editors[whichPane].getSendLines()) {
				ofxOscMessage m;
				m.setAddress("/livelily"+to_string(editors[whichPane].getSendLinesPaneNdx())+"/press");
				m.addStringArg(editors[whichPane].allStrings[i]);
				editors[whichPane].oscKeys.sendMessage(m, false);
			}
		}
		else {
			if (editors[whichPane].allStrings[i] == "\\active") {
				errors.push_back(parseString(editors[whichPane].allStrings[i], i, numLines));
			}
		}
	}
	for (auto error : errors) {
		if (error.first == 3) {
			if (parsingBar) {
				// must clear bars from all passed instruments
				// and entries in barsIndexes and loopsIndexes maps
				if (parsingBars) {
					for (int i = 0; i < barsIterCounter; i++) {
						// when an error occurs not in the first bar of all the bars in the \bars command
						// only in the first iteration of deleting the last bar we must leave the passed
						// booleans of the instruments intact
						// for all the other iterations, we must set them to true, as they have all passed
						// all the previous bars, up until the error occured
						if (i > 0) {
							for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
								it->second.setPassed(true);
							}
						}
						deleteLastBar();
					}
					barsIterCounter = 0;
					numBarsToParse = 0;
					numBarsToParseSet = false;
					parsingBars = false;
					firstInstForBarsSet = false;
				}
				else {
					deleteLastBar();
				}
				parsingBar = false;
				checkIfAllPartsReceivedBarData = false;
			}
			if (parsingLoop) {
				parsingLoop = false;
			}
			if (parsingBars) {
				parsingBars = false;
				firstInstForBarsSet = false;
			}
			if (storingList) storingList = false;
			noErrors = false;
		}
		if (!noErrors) break;
	}
	// if we have parsed at least one melodic line
	if (noErrors && (parsingBar || parsingLoop)) {
		// the bar index has already been stored with the \bar command
		int barIndex = getLastLoopIndex();
		// first send the parsed lines to any connected instruments
		if (parsingBar && !parsingBars) {
			sendBarToParts(barIndex);
		}
		if (!parsingLoop) {
			// store the closing curly bracket in the bar lines map
			// sharedData.barLines is a map<string, string> so we get the key with
			// sharedData.barsOrdered[barIndex]
			sharedData.barLines[sharedData.barsOrdered[barIndex]] += "}";
			// then add a rest for any instrument that is not included in the bar
			fillInMissingInsts(barIndex);
			// if we create a bar, create a loop with this bar only
			sharedData.loopData[barIndex] = {barIndex};
			// check if no time has been specified and set the default 4/4
			if (sharedData.numerator.find(barIndex) == sharedData.numerator.end()) {
				sharedData.numerator[barIndex] = 4;
				sharedData.denominator[barIndex] = 4;
			}
			setScoreNotes(barIndex);
			if (!parsingBars) {
				// then set the note positions and calculate the staff positions
				setNotePositions(barIndex);
				calculateStaffPositions(barIndex, false);
			}
			barError = false;
			for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				it->second.setPassed(false);
			}
		}
		if (!sequencer.isThreadRunning()) {
			sharedData.loopIndex = sharedData.tempBarLoopIndex;
			//sendLoopIndexToParts();
		}
		inserting = false;
		parsingBar = false;
		parsingLoop = false;
	}
	if (storingFunction) {
		storingFunction = false;
		functionBodyOffset = 0;
	}
	if (storingList) storingList = false;
	if (traversingList) traversingList = false;
	if (noErrors && parsingBars) {
		// check if all instruments are done parsing all their bars
		bool parsingBarsDone = true;
		for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			if (!it->second.getMultiBarsDone()) {
				parsingBarsDone = false;
				break;
			}
		}
		// call this function recursively, until all instruments are done parsing their bars
		if (!parsingBarsDone) {
			parseStrings(index, numLines);
		}
		else {
			int barIndex = sharedData.barsIndexes["\\"+multiBarsName+"-1"];
			setNotePositions(barIndex, barsIterCounter);
			for (int i = 0; i < barsIterCounter; i++) {
				barIndex = sharedData.barsIndexes["\\"+multiBarsName+"-"+to_string(i+1)];
				sendBarToParts(barIndex);
				calculateStaffPositions(barIndex, false);
			}
			parsingBars = false;
			firstInstForBarsSet = false;
			// once done, create a string to create a loop with all the separate bars
			string multiBarsLoop = "\\loop " + multiBarsName + " {";
			for (int i = 0; i < barsIterCounter-1; i++) {
				multiBarsLoop += ("\\" + multiBarsName + "-" + to_string(i+1) + " ");
			}
			// last iteration outside of the loop so that we don't add the extra white space
			// and we close the curly brackets
			multiBarsLoop += ("\\" + multiBarsName + "-" + to_string(barsIterCounter) + "}");
			parseCommand(genCmdInput(multiBarsLoop), index, 1);
			// after we create the loop of the bars we have created, we send the line with the loop command to the parts
			sendLoopToParts();
			barsIterCounter = 0;
		}
	}
	// if we're creating a pattern out of patterns
	// the posCalculator.calculateAllStaffPositions() function
	// is called from parseString() below, after the block that checks if
	// a line starts with a quotation mark
}

//--------------------------------------------------------------
std::pair<int, string> ofApp::parseString(string str, int lineNum, int numLines)
{
	if (str.length() == 0) return std::make_pair(0, "");
	if (startsWith(str, "%")) return std::make_pair(0, "");
	// strip white spaces
	while (startsWith(str, " ")) {
		str = str.substr(1);
	}
	// then check again if the string is empty
	if (str.length() == 0) return std::make_pair(0, "");
	size_t commentIndex = str.find("%");
	if (commentIndex != string::npos) {
		str = str.substr(0, commentIndex);
	}
	// check if there's a trailing white space
	if (endsWith(str, " ")) {
		str = str.substr(0, str.size()-1);
	}
	// if we're left with a closing bracket only
	if (str.compare("}") == 0) return std::make_pair(0, "");
	if (startsWith(str, "{")) {
		str = str.substr(1);
		// again strip white spaces
		while (startsWith(str, " ")) {
			str = str.substr(1);
		}
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
	if (str.length() == 0) return std::make_pair(0, "");
	// ignore comments
	if (startsWith(str, "%")) {
		editors[whichPane].releaseTraceback(lineNum);
		return std::make_pair(0, "");
	}
	else {
		if (parsingLoop) {
			std::pair<int, string> error = parseBarLoop(str, lineNum, numLines);
			if (error.first > 0) {
				editors[whichPane].setTraceback(error.first, error.second, lineNum);
				deleteLastLoop();
			}
			else {
				editors[whichPane].releaseTraceback(lineNum);
				if (!sequencer.isThreadRunning()) {
					sharedData.loopIndex = sharedData.tempBarLoopIndex;
					sendLoopIndexToParts();
				}
			}
			return error;
		}
		else if (storingFunction) {
			int offset = 0;
			int tabSize = editors[whichPane].getTabSize();
			if ((int)str.size() >= tabSize) {
				if (str.substr(0, tabSize).compare(editors[whichPane].tabStr) == 0) {
					offset = tabSize;
				}
			}
			if (functionBodyOffset == 0) sharedData.functions[lastFunctionIndex].copyStr(str.substr(offset), functionBodyOffset);
			else sharedData.functions[lastFunctionIndex].copyStr("\n"+str.substr(offset), functionBodyOffset);
			functionBodyOffset += str.substr(tabSize).size()+ 1; // + 1 for the newline
		}
		else if (storingList) {
			storeList(str);
			return std::make_pair(0, "");
		}
		else if (traversingList) {
			CmdOutput cmdOutput = traverseList(str, lineNum, numLines); 
			if (cmdOutput.errorCode > 0) {
				editors[whichPane].setTraceback(cmdOutput.errorCode, cmdOutput.errorStr, lineNum);
			}
			else {
				editors[whichPane].releaseTraceback(lineNum);
			}
			return std::make_pair(cmdOutput.errorCode, cmdOutput.errorStr);
		}
		else {
			parsingCommand = true;
			// first replace any command inside the string with the actual output of the command
			CmdOutput cmdOutput = expandCommands(str, lineNum, numLines);
			if (cmdOutput.errorCode > 0) {
				editors[whichPane].setTraceback(cmdOutput.errorCode, cmdOutput.errorStr, lineNum);
			}
			if (cmdOutput.errorCode < 3) {
				CmdInput cmdInput = {cmdOutput.outputVec, vector<string>(), false, true};
				cmdOutput = parseCommand(cmdInput, lineNum, numLines);
				if (cmdOutput.errorCode > 0) {
					editors[whichPane].setTraceback(cmdOutput.errorCode, cmdOutput.errorStr, lineNum);
				}
				else {
					editors[whichPane].releaseTraceback(lineNum);
				}
			}
			else if (cmdOutput.errorCode == 0) {
				editors[whichPane].releaseTraceback(lineNum);
			}
			return std::make_pair(cmdOutput.errorCode, cmdOutput.errorStr);
		}
	}
	return std::make_pair(0, "");
}

//--------------------------------------------------------------
CmdOutput ofApp::expandCommands(const string& input, int lineNum, int numLines)
{
	vector<string> tokens = tokenizeExpandedCommands(input);
	CmdOutput cmdOutput = CmdOutput();
	vector<string> output;
	if (tokens.empty()) {
		return cmdOutput;
	}
	else if (find(nonExpandableCommands.begin(), nonExpandableCommands.end(), tokens[0]) != nonExpandableCommands.end() || parsingBar) {
		cmdOutput.outputVec = tokens;
		return cmdOutput;
	}
	cmdOutput = parseExpandedCommands(tokens, lineNum, numLines);
	return cmdOutput;
}

//--------------------------------------------------------------
vector<string> ofApp::tokenizeExpandedCommands(const string& input)
{
	bool isString = false;
	bool isChord = false;
	int quotesCounter = 0;
	vector<string> tokens;
	string token;
	for (size_t i = 0; i < input.size(); i++) {
		if (isspace(input[i])) {
			if (!token.empty()) {
				if (!isString && !isChord) {
					tokens.push_back(token);
					token.clear();
				}
				else {
					token += input[i];
				}
			}
		}
		else if (input[i] == '{' || input[i] == '}') {
			if (!token.empty() && !isString) {
				tokens.push_back(token);
				token.clear();
			}
			tokens.emplace_back(1, input[i]);
		}
		else {
			token += input[i];
			if (input[i] == '"') {
				quotesCounter++;
				if (quotesCounter == 1) {
					isString = true;
				}
				else {
					isString = false;
					quotesCounter = 0;
				}
			}
			else if (input[i] == '<' && (i < input.size()-1 && !isspace(input[i+1]))) {
				isChord = true;
			}
			else if (input[i] == '>') {
				isChord = false;
			}
		}
	}
	if (!token.empty()) tokens.push_back(token);
	return tokens;
}

//--------------------------------------------------------------
CmdOutput ofApp::parseExpandedCommands(const vector<string>& tokens, int lineNum, int numLines)
{
	size_t index = tokens.size() - 1;
	size_t firstCmdNdx = 0;
	bool hasBrackets = false;
	// initialize a CmdOutput structure with the first command of the string
	// (all executable lines in LiveLily start with a string)
	CmdOutput cmdOutput = CmdOutput();
	if (tokens.empty()) return cmdOutput;
	// check if there are white spaces (probably a horizontal tab) at the beginning of the input vector
	for (string s : tokens) {
		if (s.empty() || s == " ") firstCmdNdx++;
		else break;
	}
	cmdOutput.outputVec = {tokens[firstCmdNdx]};
	deque<string> outputDeque;
	
	while (index > firstCmdNdx) {
		string token = tokens[index];
		// if the token is a command
		if (!token.empty() && token[firstCmdNdx] == '\\') {
			size_t dequeNdx = 0;
			vector<string> cmd;
			cmd.push_back(token); // this is the command taken from the original tokens vector
			if (dequeNdx >= outputDeque.size()) {
				CmdInput cmdInput = {cmd, vector<string>(), false, false};
				CmdOutput cmdOutputLocal = parseCommand(cmdInput, lineNum, numLines);
				if (cmdOutputLocal.errorCode == 3) return cmdOutputLocal;
				for (size_t i = 0; i < cmdOutputLocal.toPop; i++) {
					outputDeque.pop_front();
				}
				outputDeque.insert(outputDeque.begin(), cmdOutputLocal.outputVec.begin(), cmdOutputLocal.outputVec.end());
				index--;
				continue;
			}

			// check if the first argument is outside curly brackets
			if (outputDeque[dequeNdx] != "{") {
				// extract the first argument of the command
				// this is extracted from the outputDeque deque instead of the tokens vector
				cmd.push_back(outputDeque[dequeNdx++]);
			}
			// then check if there are more arguments inside curly brackets
			if (dequeNdx < outputDeque.size() && outputDeque[dequeNdx] == "{") {
				hasBrackets = true;
				dequeNdx++; // skip open curly bracket
				while (dequeNdx < outputDeque.size() && outputDeque[dequeNdx] != "}") {
					cmd.push_back(outputDeque[dequeNdx++]);
				}
			}
			if (hasBrackets) dequeNdx++; // move to point to the item after the closing bracket
			// the second argument to the data structure initialization below
			// is the tokens after the command and its argumets
			CmdInput cmdInput = {cmd, {outputDeque.begin()+dequeNdx, outputDeque.end()}, hasBrackets, false};
			CmdOutput cmdOutputLocal = parseCommand(cmdInput, lineNum, numLines);
			if (cmdOutputLocal.errorCode == 3) return cmdOutputLocal;
			for (size_t i = 0; i < cmdOutputLocal.toPop; i++) {
				outputDeque.pop_front();
			}
			if (hasBrackets) hasBrackets = false;
			outputDeque.insert(outputDeque.begin(), cmdOutputLocal.outputVec.begin(), cmdOutputLocal.outputVec.end());
		}
		else {
			outputDeque.push_front(token);
		}
		index--;
	}
	cmdOutput.outputVec.insert(cmdOutput.outputVec.end(), outputDeque.begin(), outputDeque.end());
	return cmdOutput;
}

//--------------------------------------------------------------
string ofApp::genStrFromVec(const vector<string>& vec)
{
    std::ostringstream oss;
    for (const auto& str : vec) {
        oss << str << " "; // Add a space after each string
    }
    string result = oss.str();
    return result.empty() ? result : result.substr(0, result.size() - 1); // Remove the trailing space
}

//--------------------------------------------------------------
CmdOutput ofApp::traverseList(string str, int lineNum, int numLines)
{
	unsigned i;
	CmdOutput cmdOutput = CmdOutput();
	string lineWithArgs = str;
	if (lineWithArgs.compare("{") == 0) return cmdOutput;
	if (lineWithArgs.compare("}") == 0) return cmdOutput;
	if (startsWith(lineWithArgs, "{")) lineWithArgs = lineWithArgs.substr(1);
	if (endsWith(lineWithArgs, "}")) lineWithArgs = lineWithArgs.substr(0, lineWithArgs.size()-1);
	vector<int> argNdxs = findIndexesOfCharInStr(lineWithArgs, "$");
	for (auto it = listMap[lastListIndex].begin(); it != listMap[lastListIndex].end(); ++it) {
		for (i = 0; i < argNdxs.size(); i++) {
			string strToExecute = replaceCharInStr(lineWithArgs, lineWithArgs.substr(argNdxs[i],1), *it);
			cmdOutput = parseCommand(genCmdInput(strToExecute), lineNum, numLines);
			if (cmdOutput.errorCode == 3) return cmdOutput;
		}
	}
	return cmdOutput;
}

//--------------------------------------------------------------
void ofApp::storeList(string str)
{
	vector<string> items = tokenizeString(str, " ");
	for (string item : items) {
		if (item.compare("{") == 0) continue;
		if (item.compare("}") == 0) continue;
		if (startsWith(item, "{")) listMap[lastListIndex].push_back(item.substr(1));
		else if (endsWith(item, "}")) listMap[lastListIndex].push_back(item.substr(0, item.size()-1));
		else listMap[lastListIndex].push_back(item);
	}
}

//--------------------------------------------------------------
void ofApp::fillInMissingInsts(int barIndex)
{
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		if (!it->second.hasPassed()) {
			it->second.createEmptyMelody(barIndex);
			it->second.setPassed(true);
			// since all instruments need to set the boolean below to true
			// for parseStrings() to know that ;all bars of a \bars commands have finished
			// it's ok to set it to true for missing instruments right from the start
			if (parsingBars) it->second.setMultiBarsDone(true);
		}
	}
}

//--------------------------------------------------------------
int ofApp::storeNewBar(string barName)
{
	int barIndex = getLastLoopIndex() + 1;
	if (sharedData.numerator.find(barIndex) == sharedData.numerator.end()) {
		sharedData.numerator[barIndex] = 4;
		sharedData.denominator[barIndex] = 4;
	}
	sharedData.barsIndexes[barName] = barIndex;
	sharedData.loopsIndexes[barName] = barIndex;
	sharedData.loopsOrdered[barIndex] = barName;
	sharedData.barsOrdered[barIndex] = barName;
	sharedData.loopsVariants[barIndex] = 0;
	sharedData.tempBarLoopIndex = barIndex;
	int prevBarIndex = getPrevBarIndex();
	if (prevBarIndex < 0) {
		sharedData.numerator[barIndex] = 4;
		sharedData.denominator[barIndex] = 4;
		sharedData.numBeats[barIndex] = sharedData.numerator[barIndex] * (MINDUR / sharedData.denominator[barIndex]);
		sharedData.tempoMs[barIndex] = 500;
		sharedData.BPMTempi[barIndex] = 120;
		sharedData.BPMMultiplier[barIndex] = 1;
		sharedData.beatAtDifferentThanDivisor[barIndex] = false;
		sharedData.beatAtValues[barIndex] = 1;
		sharedData.tempoBaseForScore[barIndex] = 4;
		sharedData.BPMDisplayHasDot[barIndex] = false;
		sharedData.tempo[barIndex] = sharedData.tempoMs[barIndex] / (MINDUR / sharedData.denominator[barIndex]);
		sharedData.PPQNPerUs[barIndex] = (uint64_t)(sharedData.tempoMs[barIndex] / (float)sharedData.PPQN) * 1000;
	}
	else {
		sharedData.numerator[barIndex] = sharedData.numerator[prevBarIndex];
		sharedData.denominator[barIndex] = sharedData.denominator[prevBarIndex];
		sharedData.numBeats[barIndex] = sharedData.numBeats[prevBarIndex];
		sharedData.tempoMs[barIndex] = sharedData.tempoMs[prevBarIndex];
		sharedData.BPMTempi[barIndex] = sharedData.BPMTempi[prevBarIndex];
		sharedData.BPMMultiplier[barIndex] = sharedData.BPMMultiplier[prevBarIndex];
		sharedData.beatAtDifferentThanDivisor[barIndex] = sharedData.beatAtDifferentThanDivisor[prevBarIndex];
		sharedData.beatAtValues[barIndex] = sharedData.beatAtValues[prevBarIndex];
		sharedData.tempoBaseForScore[barIndex] = sharedData.tempoBaseForScore[prevBarIndex];
		sharedData.BPMDisplayHasDot[barIndex] = sharedData.BPMDisplayHasDot[prevBarIndex];
		sharedData.tempo[barIndex] = sharedData.tempo[prevBarIndex];
		sharedData.PPQNPerUs[barIndex] = sharedData.PPQNPerUs[prevBarIndex];
	}
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		it->second.setMeter(barIndex, sharedData.numerator[barIndex],
				sharedData.denominator[barIndex], sharedData.numBeats[barIndex]);
	}
	keywords.push_back(barName);
	// store the bar name
	// the rest of the bar data will be stored in parseMelodicLine()
	// sharedData.barLines is a map of string and string, so the key is the bar name
	sharedData.barLines[barName] = "\\bar " + barName + " {\n";
	return barIndex;
}

//--------------------------------------------------------------
void ofApp::storeNewLoop(string loopName)
{
	int loopIndex = getLastLoopIndex() + 1;
	sharedData.loopsIndexes[loopName] = loopIndex;
	sharedData.loopsOrdered[loopIndex] = loopName;
	sharedData.loopsVariants[loopIndex] = 0;
	sharedData.tempBarLoopIndex = loopIndex;
	partsReceivedOKCounters[loopIndex] = 0;
	keywords.push_back(loopName);
}

//--------------------------------------------------------------
int ofApp::getBaseDurValue(string str, int denominator)
{
	int base = 0;
	int val;
	if (str.find(".") != string::npos) {
		base = stoi(str.substr(0, str.find(".")));
		val = denominator / base;
		// get the number of dots, copied from
		// https://stackoverflow.com/questions/3867890/count-character-occurrences-in-a-string-in-c#3871346
		string::difference_type n = std::count(str.begin(), str.end(), '.');
		int half = val / 2;
		for (int i = 0; i < (int)n; i++) {
			val += half;
			half /= 2;
		}
	}
	else {
		base = stoi(str);
		val = denominator / base;
	}
	return val;
}

//--------------------------------------------------------------
std::pair<int, string> ofApp::getBaseDurError(string str)
{
	if (!isNumber(str)) {
		if (str.find(".") == string::npos) {
			return std::make_pair(3, str + " is not a number");
		}
		else {
			if (!isNumber(str.substr(0, str.find(".")))) {
				return std::make_pair(3, str + " is not a number");
			}
		}
	}
	return std::make_pair(0, "");
}

//--------------------------------------------------------------
CmdOutput ofApp::genError(string str)
{
	CmdOutput error = {3, str, 0, vector<string>()};
	return error;
}

//--------------------------------------------------------------
CmdOutput ofApp::genWarning(string str)
{
	CmdOutput warning = {2, str, 0, vector<string>()};
	return warning;
}

//--------------------------------------------------------------
CmdOutput ofApp::genNote(string str)
{
	CmdOutput note = {1, str, 0, vector<string>()};
	return note;
}

//--------------------------------------------------------------
CmdOutput ofApp::genOutput(string str)
{
	CmdOutput output = {0, "", 0, vector<string>({str})};
	return output;
}

//--------------------------------------------------------------
CmdOutput ofApp::genOutput(vector<string> v)
{
	CmdOutput output = {0, "", 0, v};
	return output;
}

//--------------------------------------------------------------
CmdOutput ofApp::genOutput(string str, size_t toPop)
{
	CmdOutput output = {0, "", toPop, vector<string>({str})};
	return output;
}

//--------------------------------------------------------------
CmdOutput ofApp::genOutput(vector<string> v, size_t toPop)
{
	CmdOutput output = {0, "", toPop, v};
	return output;
}

//--------------------------------------------------------------
CmdOutput ofApp::genOutputFuncs(std::pair<int, string> p)
{
	switch (p.first) {
		case 0:
			return genOutput(p.second);
		case 1:
			return genNote(p.second);
		case 2:
			return genWarning(p.second);
		case 3:
			return genError(p.second);
		default:
			CmdOutput cmdOutput = CmdOutput();
			return cmdOutput;
	}
}

//--------------------------------------------------------------
CmdInput ofApp::genCmdInput(string str)
{
	vector<string> v = tokenizeString(str, " ");
	CmdInput cmdInput = {v, vector<string>(), false, true};
	return cmdInput;
}

//--------------------------------------------------------------
CmdInput ofApp::genCmdInput(vector<string> v)
{
	CmdInput cmdInput = {v, vector<string>(), false, true};
	return cmdInput;
}

//--------------------------------------------------------------
CmdOutput ofApp::parseCommand(CmdInput cmdInput, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();

	if (cmdInput.inputVec[0].compare("\\time") == 0) {
		if (barError) return cmdOutput;
		if (!parsingBar && !parsingBars) {
			return genError("\\time command must be placed inside a bar(s) definition");
		}
		if (cmdInput.inputVec.size() > 2) {
			return genError("\\time command takes one argument only");
		}
		else if (cmdInput.inputVec.size() < 2) {
			return genError("\\time command needs one arguments");
		}
		int divisionSym = cmdInput.inputVec[1].find("/");
		string numeratorStr = cmdInput.inputVec[1].substr(0, divisionSym);
		string denominatorStr = cmdInput.inputVec[1].substr(divisionSym+1);
		int numeratorLocal = 0;
		int denominatorLocal = 0;
		if (isNumber(numeratorStr)) {
			numeratorLocal = stoi(numeratorStr);
		}
		else {
			return genError(numeratorStr + " is not an int");
		}
		if (isNumber(denominatorStr)) {
			denominatorLocal = stoi(denominatorStr);
		}
		else {
			return genError(denominatorStr + " is not an int");
		}
		// a \time command must be placed inside a bar definition
		// and the bar index has already been stored, so we can safely query the last bar index
		int bar = getLastBarIndex();
		sharedData.numerator[bar] = numeratorLocal;
		sharedData.denominator[bar] = denominatorLocal;
		sharedData.numBeats[bar] = sharedData.numerator[bar] * (MINDUR / sharedData.denominator[bar]);
		if (sharedData.BPMMultiplier.find(bar) == sharedData.BPMMultiplier.end()) {
			sharedData.BPMMultiplier[bar] = 1;
			sharedData.beatAtValues[bar] = 1;
			sharedData.beatAtDifferentThanDivisor[bar] = true;
		}
		for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			it->second.setMeter(bar, sharedData.numerator[bar],
					sharedData.denominator[bar], sharedData.numBeats[bar]);
			//if (it->second.sendToPart) {
			//	sendBarIndexBeatMeterToPart(it->first, bar);
			//}
			ofxOscMessage m;
			m.setAddress("/time");
			m.addIntArg(bar);
			m.addIntArg(sharedData.numerator[bar]);
			m.addIntArg(sharedData.denominator[bar]);
			m.addIntArg(sharedData.numBeats[bar]);
			sendToParts(m, false);
		}
		return cmdOutput;
	}

	else if (cmdInput.inputVec[0].compare("\\tempo") == 0) {
		if (barError) return cmdOutput;
		if (!parsingBar && !parsingBars) {
			return genError("\\tempo command must be placed inside a bar(s) definition");
		}
		if (cmdInput.inputVec.size() > 4) {
			return genError("\\tempo command takes maximum three arguments");
		}
		if (cmdInput.inputVec.size() < 2) {
			return genError("\\tempo command needs at least one argument");
		}
		// a \tempo command must be placed inside a bar definition
		// and the bar index has already been stored, so we can safely query the last bar index
		int bar = getLastBarIndex();
		if (cmdInput.inputVec.size() == 4) {
			if (cmdInput.inputVec[2].compare("=") != 0) {
				return genError("\\tempo command takes maximum three arguments");
			}
			else {
				std::pair<int, string> err = getBaseDurError(cmdInput.inputVec[1]);
				if (err.first > 0) {
					// using the array to function pointers that generate output here
					// because we don't know the error code returned by geBaseDurError()
					return genOutputFuncs(err);
				}
				if (!isNumber(cmdInput.inputVec[3])) {
					return genError(cmdInput.inputVec[3] + " is not a number");
				}
				sharedData.BPMMultiplier[bar] = getBaseDurValue(cmdInput.inputVec[1], sharedData.denominator[bar]);
				sharedData.beatAtValues[bar] = sharedData.BPMMultiplier[bar];
				sharedData.beatAtDifferentThanDivisor[bar] = true;
				sharedData.BPMTempi[bar] = stoi(cmdInput.inputVec[3]);
				// convert BPM to ms
				sharedData.tempoMs[bar] = 1000.0 / ((float)(sharedData.BPMTempi[bar]  * sharedData.BPMMultiplier[bar]) / 60.0);
				// get the tempo of the minimum duration
				sharedData.tempo[bar] = sharedData.tempoMs[bar] / (MINDUR / sharedData.denominator[bar]);
				// get the usec duration of the PPQM
				sharedData.PPQNPerUs[bar] = (uint64_t)(sharedData.tempoMs[bar] / (float)sharedData.PPQN) * 1000;
				if (cmdInput.inputVec[1].find(".") != string::npos) {
					sharedData.tempoBaseForScore[bar] = stoi(cmdInput.inputVec[1].substr(0, cmdInput.inputVec[1].find(".")));
					sharedData.BPMDisplayHasDot[bar] = true;
				}
				else {
					sharedData.tempoBaseForScore[bar] = stoi(cmdInput.inputVec[1]);
					sharedData.BPMDisplayHasDot[bar] = false;
				}
			}
		}
		else {
			if (isNumber(cmdInput.inputVec[1])) {
				sharedData.BPMTempi[bar] = stoi(cmdInput.inputVec[1]);
				// convert BPM to ms
				sharedData.tempoMs[bar] = 1000.0 / ((float)sharedData.BPMTempi[bar] / 60.0);
				// get the tempo of the minimum duration
				sharedData.tempo[bar] = sharedData.tempoMs[bar] / (MINDUR / sharedData.denominator[bar]);
				// get the usec duration of the PPQM
				sharedData.PPQNPerUs[bar] = (uint64_t)(sharedData.tempoMs[bar] / (float)sharedData.PPQN) * 1000;
				sharedData.BPMMultiplier[bar] = sharedData.beatAtValues[bar] = 1;
				sharedData.beatAtDifferentThanDivisor[bar] = true;
				sharedData.tempoBaseForScore[bar] = 4;
				sharedData.BPMDisplayHasDot[bar] = false;
			}
			else {
				return genError(cmdInput.inputVec[1] + " is not a number");
			}
		}
		return cmdOutput;
	}

	else if (cmdInput.inputVec[0].compare("\\beatat") == 0) {
		if (barError) return cmdOutput;
		if (!parsingBar && !parsingBars) {
			return genError("\\beatat command must be placed inside a bar(s) definition");
		}
		if (cmdInput.inputVec.size() > 2) {
			return genError("\\beatat command takes one argument only");
		}
		if (cmdInput.inputVec.size() == 1) {
			return genError("\\beatat command takes one argument");
		}
		std::pair<int, string> err = getBaseDurError(cmdInput.inputVec[1]);
		if (err.first > 0) {
			// using the array to function pointers that generate output here
			// because we don't know the error code returned by geBaseDurError()
			return genOutputFuncs(err);
		}
		int bar = getLastBarIndex();
		sharedData.beatAtValues[bar] = getBaseDurValue(cmdInput.inputVec[1], sharedData.denominator[bar]);
		sharedData.beatAtDifferentThanDivisor[bar] = true;
		return cmdOutput;
	}

	else if (cmdInput.inputVec[0].compare("\\tuplet") == 0 || cmdInput.inputVec[0].compare("\\tup") == 0) {
		if (cmdInput.isMainCmd) {
			return genError("stray tuplet");
		}
		if (cmdInput.inputVec.size() < 2 || !cmdInput.hasBrackets) {
			return genError("badly formatted \\tuplet");
		}
		if (cmdInput.inputVec[1].find("/") == string::npos) {
			return genError("badly formatted tuplet ratio");
		}
		size_t divisionNdx = cmdInput.inputVec[1].find("/");
		size_t remainingStr = cmdInput.inputVec[1].size() - divisionNdx - 1;
		if (!isNumber(cmdInput.inputVec[1].substr(0, divisionNdx)) || !isNumber(cmdInput.inputVec[1].substr(divisionNdx+1, remainingStr))) {
			return genError("numerator or denominator of tuplet ratio is not a number");
		}
		string toInsert = "/" + cmdInput.inputVec[1];
		vector<char> notes = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'r'};
		for (size_t i = 2; i < cmdInput.inputVec.size(); i++) {
			string strToChange = cmdInput.inputVec[i];
			bool tupletInserted = false;
			string changedStr = "";
			for (size_t j = 0; j < cmdInput.inputVec[i].size(); j++) {
				if (find(notes.begin(), notes.end(), strToChange[j]) != notes.end() || (strToChange[j] == '\'' || strToChange[j] == ',' || strToChange[j] == '<' || strToChange[j] == '>')) {
					changedStr += strToChange[j];
					if ((strToChange[j] == 'e' || strToChange[j] == 'i') && j + 1 < strToChange.size()) {
						if (strToChange[j+1] == 's') {
							changedStr += strToChange[j+1];
							j++;
						}
					}
					continue;
				}
				if (strToChange[j] == 'o' && j + 1 < strToChange.size()) {
					changedStr += strToChange[j];
					if (strToChange[j+1] == '-' && j + 2 < strToChange.size()) {
						changedStr += strToChange[j+1];
						j++;
					}
					changedStr += strToChange[j+1];
					j++;
					continue;
				}
				if (std::isdigit(strToChange[j]) || std::isspace(strToChange[j])) {
					changedStr += strToChange[j];
					continue;
				}
				if (!tupletInserted) {
					changedStr += toInsert;
					tupletInserted = true;
				}
				changedStr += strToChange[j];
			}
			if (!tupletInserted) {
				changedStr += toInsert;
			}
			cmdOutput.outputVec.push_back(changedStr);
		}
		// the command vector doesn't include the curly brackets, only the command name and its arguments
		// in case of a 3/2 tuplet, the command vector will have 5 items, e.g. \tuplet 3/2 c d e
		// but we need to pop_front 6 elements, 3/2 c d e and the curly brackets
		cmdOutput.toPop = cmdInput.inputVec.size() + 1;
		return cmdOutput;
	}

	else if (cmdInput.inputVec[0].compare("\\ottava") == 0 || cmdInput.inputVec[0].compare("\\ott") == 0) {
		if (cmdInput.isMainCmd || !parsingBar) {
			return genError("stray ottava");
		}
		if (cmdInput.inputVec.size() < 2) {
			return genError("no argument to \\ottava");
		}
		if (cmdInput.afterCmdVec.empty()) {
			return genError("no item to apply \\ottava to");
		}
		string arg = cmdInput.inputVec[1];
		if (arg.size() > 2) {
			return genError("unkown argument to \\ottava: " + arg);
		}
		if (arg.size() == 2 && (arg[0] != '-' || !isdigit(arg[1]))) {
			return genError("unkown argument to \\ottava: " + arg);
		}
		if (arg.size() == 1 && !isdigit(arg[0])) {
			return genError("argument to \\ottava must be a number between -2 and 2");
		}
		int intArg = stoi(arg);
		if (intArg < -2 || intArg > 2) {
			return genError("argument to \\ottava must be between -2 and 2");
		}
		string strToChange = cmdInput.afterCmdVec[0];
		bool octaveInserted = false;
		vector<char> notes = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'r', 'i'};
		string changedStr = "";
		for (size_t i = 0; i < strToChange.size(); i++) {
			if (find(notes.begin(), notes.end(), strToChange[i]) != notes.end() || (strToChange[i] == '\'' || strToChange[i] == ',' || strToChange[i] == '<')) {
				changedStr += strToChange[i];
				if ((strToChange[i] == 'e' || strToChange[i] == 'i') && (i+1 < strToChange.size() && strToChange[i+1] == 's')) {
					changedStr += strToChange[i+1];
					i++;
				}
				continue;
			}
			if (!octaveInserted) {
				changedStr += "o" + arg;
				octaveInserted = true;
			}
			changedStr += strToChange[i];
		}
		if (!octaveInserted) {
			changedStr += "o" + arg;
		}
		cmdOutput.outputVec.push_back(changedStr);
		cmdOutput.toPop = 2;
		return cmdOutput;
	}

	if (cmdInput.inputVec[0].compare("\\clef") == 0) {
		if (!parsingBar) {
			return genError("stray \\clef command");
		}
		if (cmdInput.inputVec.size() != 2) {
			return genError("\\clef command takes one argument, a clef name");
		}
		int clefNdx;
		if (cmdInput.inputVec[1].compare("treble") == 0) {
			sharedData.instruments[lastInstrumentIndex].setClef(getLastBarIndex(), 0);
			clefNdx = 0;
		}
		else if (cmdInput.inputVec[1].compare("bass") == 0) {
			sharedData.instruments[lastInstrumentIndex].setClef(getLastBarIndex(), 1);
			clefNdx = 1;
		}
		else if (cmdInput.inputVec[1].compare("alto") == 0) {
			sharedData.instruments[lastInstrumentIndex].setClef(getLastBarIndex(), 2);
			clefNdx = 2;
		}
		else if (cmdInput.inputVec[1].compare("perc") == 0 || cmdInput.inputVec[1].compare("percussion") == 0) {
			sharedData.instruments[lastInstrumentIndex].setClef(getLastBarIndex(), 3);
			clefNdx = 3;
		}
		else {
			return genError("unknown clef");
		}
		if (sharedData.instruments[lastInstrumentIndex].sendToPart) {
			ofxOscMessage m;
			m.setAddress("/clef");
			m.addStringArg("\\"+sharedData.instruments[lastInstrumentIndex].getName());
			m.addIntArg(clefNdx);
			sharedData.instruments[lastInstrumentIndex].scorePartSender.sendMessage(m, false);
			m.clear();
		}
		cmdOutput.toPop = 1;
	}

	else if (cmdInput.inputVec[0].compare("\\ppqn") == 0) {
		if (cmdInput.inputVec.size() < 2) {
			return genError("\\ppqn command needs the PPQN number as an argument");
		}
		if (cmdInput.inputVec.size() > 2) {
			return genError("\\ppqn command takes one argument only");
		}
		if (!isNumber(cmdInput.inputVec[1])) {
			return genError("the argument to \\ppqn must be a number");
		}
		int PPQNLocal = stoi(cmdInput.inputVec[1]);
		if (PPQNLocal != 24 && PPQNLocal != 48 && PPQNLocal != 96) {
			return genError("PPQN value can be 24, 48, or 96");
		}
		sharedData.PPQN = PPQNLocal;
	}

	else if (cmdInput.inputVec[0].compare("\\midiclock") == 0) {
		if (cmdInput.inputVec.size() < 2) {
			return genError("\\midiclock takes one argument");
		}
		if (cmdInput.inputVec[1].compare("on") == 0) {
			sequencer.setSendMidiClock(true);
			sharedData.PPQNTimeStamp = ofGetElapsedTimeMicros();
		}
		else if (cmdInput.inputVec[1].compare("off") == 0) {
			sequencer.setSendMidiClock(false);
		}
		else {
			return genError(cmdInput.inputVec[1] + ": unknown argument, \\midiclock takes on or off");
		}
	}

	else if (cmdInput.inputVec[0].compare("\\instorder") == 0) {
		if (cmdInput.inputVec.size() != sharedData.instruments.size()+1) {
			return genError("instrument list must be equal to number of instruments");
		}
		for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
			sharedData.instrumentIndexesOrdered[i-1] = sharedData.instrumentIndexes[cmdInput.inputVec[i]];
		}
	}

	else if (cmdInput.inputVec[0].compare("\\print") == 0) {
		//string restOfCommand = getRestOfCommand(commands, 1);
		//std::pair<int, string> error = parseString(restOfCommand, lineNum, numLines);
		//if (error.first == 0) return std::make_pair(1, error.second);
		//else return error;
		if (!cmdInput.isMainCmd) {
			return genError("\\print can only be a top command");
		}
		vector<string> v = {cmdInput.inputVec.begin()+1, cmdInput.inputVec.end()};
		return genNote(genStrFromVec(v));
	}

	else if (cmdInput.inputVec[0].compare("\\osc") == 0) {
		if (cmdInput.inputVec.size() < 2) {
			return genError("\\osc command takes one argument, the name of the OSC client");
		}
		if (oscClients.find(cmdInput.inputVec[1]) != oscClients.end()) {
			return genError("OSC client already exists");
		}
		string oscClientName = "\\" + cmdInput.inputVec[1];
		oscClients[oscClientName] = ofxOscSender();
		commandsMap[livelily][oscClientName] = ofColor::lightSeaGreen;
	}
	
	else if (cmdInput.inputVec[0].compare("\\barnum") == 0) {
		if (cmdInput.inputVec.size() > 1) {
			return genError("\\barnum takes no arguments");
		}
		if (!cmdInput.isMainCmd) {
			// since \barnum takes no arguments we don't need to pop any deque items
			cmdOutput.outputVec.push_back(to_string(getPlayingBarIndex()));
			return cmdOutput;
		}
	}

	else if (cmdInput.inputVec[0].compare("\\beatcount") == 0) {
		if (cmdInput.inputVec.size() > 1) {
			return genError("\\beatcount takes no arguments");
		}
		// livelily is 1-based, but C++ is 0-based, so we add one to the beat counter
		// this is not needed with the \barnum command, because livelily creates the first bar
		// with index 0 upon creating the instruments, so the first bar created by the user
		// will have index 1
		if (!cmdInput.isMainCmd) {
			// since \barnum takes no arguments we don't need to pop any deque items
			return genOutput(to_string(sharedData.beatCounter + 1));
		}
	}

	else if (cmdInput.inputVec[0].compare("\\insts") == 0 || startsWith(cmdInput.inputVec[0], "\\insts.")) {
		if (cmdInput.inputVec[0].compare("\\insts") == 0 && cmdInput.inputVec.size() < 2) {
			return genError("the \\insts command does nothing by itself, you must call one of its second level commands");
		}
		else if (cmdInput.inputVec[0].compare("\\insts") == 0 && cmdInput.inputVec.size() >= 2) {
			return genError("the \\insts command takes only second level commands, not arguments");
		}
		if (cmdInput.inputVec.size() < 2) {
			return genError("second level commands of \\insts command take at least one argument");
		}
		vector<string> commands = tokenizeString(cmdInput.inputVec[0], ".");
		commands.insert(commands.end(), cmdInput.inputVec.begin()+1, cmdInput.inputVec.end());
		if (commands[1] == "init" && sharedData.numInstruments > 0) {
			return genError("instruments have already been initialized, can only add now");
		}
		if (commands[1].compare("init") != 0 && commands[1].compare("add") != 0) {
			return genError("\\insts command takes only \"init\" or \"add\" second leve commands");
		}
		// check the arguments to make sure they don't start with a backslash
		for (auto it = commands.begin()+2; it != commands.end(); ++it) {
			if (startsWith(*it, "\\")) {
				return genError("instrument names can't start with a backslash");
			}
		}
		// create the instruments of the arguments
		for (unsigned i = 2; i < commands.size(); i++) {
			bool instrumentExists = false;
			for (auto it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				if (commands[i].compare(it->first) == 0) {
					instrumentExists = true;
					lastInstrumentIndex = it->second;
					break;
				}
			}
			if (!instrumentExists) {
				initializeInstrument("\\" + commands[i]);
				commandsMap[livelily]["\\" + commands[i]] = ofColor::greenYellow;
			}
			lastInstrument = commands[i];
		}
		if (commands[1].compare("init") == 0) {
			// create a default empty bar with a -1 bar number
			// by recursively calling parseCommand() like below
			CmdInput cmdInput = {{"\\bar", "-1"}, vector<string>(), false, true};
			parseCommand(cmdInput, 0, 1); // dummy line number and number of lines arguments
		}
		else if (commands[1].compare("add") == 0) {
			// create an empty melodic line for all existing bars for each new instrument
			for (auto barIt = sharedData.barsIndexes.begin(); barIt != sharedData.barsIndexes.end(); ++barIt) {
				for (auto instIt = commands.begin()+2; instIt != commands.end(); ++instIt) {
					sharedData.instruments[sharedData.instrumentIndexes[*instIt]].createEmptyMelody(barIt->second);
				}
			}
		}
	}

	else if (cmdInput.inputVec[0].compare("\\group") == 0) {
		if (cmdInput.inputVec.size() < 3) {
			return genError("\\group command takes at least two arguments");
		}
		if (cmdInput.inputVec.size() > sharedData.instruments.size() + 1) {
			return genError("too many arguments to \\group");
		}
		string groupName;
		for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
			if (sharedData.instrumentIndexes.find(cmdInput.inputVec[i]) == sharedData.instrumentIndexes.end()) {
				// if we provide two arguments (3 with the command name) then we can't provide a name for the group
				// otherwise, if we're not at the last argument, again we raise an error as only the last argument
				// can be the name of the group, the previous arguments must be instrument names
				if (cmdInput.inputVec.size() == 3 || i < cmdInput.inputVec.size() - 1) {
					return genError(cmdInput.inputVec[i] + ": unknown instrument");
				}
				else if (i == cmdInput.inputVec.size()-1) {
					groupName = "\\" + cmdInput.inputVec[i];
				}
			}
		}
		for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
			if (i > 1 && sharedData.instrumentIndexes.find(cmdInput.inputVec[i]) != sharedData.instrumentIndexes.end()) {
				if (sharedData.instruments[sharedData.instrumentIndexes[cmdInput.inputVec[i]]].getID() - sharedData.instruments[sharedData.instrumentIndexes[cmdInput.inputVec[i-1]]].getID() != 1) {
					return genError("can't group instruments that are not adjacent");
				}
			}
		}
		unsigned loopIter = (groupName.empty() ? cmdInput.inputVec.size() : cmdInput.inputVec.size()-1);
		for (unsigned i = 2; i < loopIter; i++) {
			sharedData.instruments[sharedData.instrumentIndexes[cmdInput.inputVec[i]]].setGroup(sharedData.instruments[sharedData.instrumentIndexes[cmdInput.inputVec[i-1]]].getID());
		}
		ofxOscMessage m;
		m.setAddress("/group");
		for (unsigned i = 1; i < loopIter; i++) {
			m.addStringArg(cmdInput.inputVec[i]);
		}
		sendToParts(m, false);
		if (!groupName.empty()) {
			instGroups[groupName] = {cmdInput.inputVec.begin()+1, cmdInput.inputVec.end()-1};
			commandsMap[livelily][groupName] = ofColor::lightSteelBlue;
		}
	}

	else if (cmdInput.inputVec[0].compare("\\score") == 0 || startsWith(cmdInput.inputVec[0], "\\score.")) {
		if (cmdInput.inputVec[0].compare("\\score") == 0 && cmdInput.inputVec.size() < 2) {
			return genError("the \\score command does nothing by itself, you must call one of its second level commands");
		}
		else if (cmdInput.inputVec[0].compare("\\score") == 0 && cmdInput.inputVec.size() >= 2) {
			return genError("the \\score command takes only second level commands, not arguments");
		}
		else {
			return scoreCommands(cmdInput.inputVec, lineNum, numLines);
		}
	}

	else if (cmdInput.inputVec[0].compare("\\cursor") == 0) {
		if ((cmdInput.inputVec.size() == 1) || (cmdInput.inputVec.size() > 2)) {
			return genError("\\cursor takes one argument");
		}
		// for some reason the OF functions are inverted
		if (cmdInput.inputVec[1].compare("hide")) {
			ofShowCursor();
		}
		else if (cmdInput.inputVec[1].compare("show")) {
			ofHideCursor();
		}
		else {
			return genError("unknown argument for \\cursor");
		}
	}

	else if (cmdInput.inputVec[0].compare("\\bar") == 0) {
		if (cmdInput.inputVec.size() < 2) {
			barError = true;
			return genError("\\bar command takes a name as an argument");
		}
		if (cmdInput.inputVec.size() >= 2 && cmdInput.inputVec[1].compare("{") == 0) {
			barError = true;
			return genError("\\bar command takes a name as an argument");
		}
		string barName = "\\" + cmdInput.inputVec[1];
		auto barExists = sharedData.barsIndexes.find(barName);
		if (barExists != sharedData.barsIndexes.end()) {
			barError = true;
			return genError((string)"bar " + barName.substr(1) + (string)" already exists");
		}
		parsingBar = true;
		int barIndex = storeNewBar(barName);
		//cout << "will parse bar " << sharedData.loopsOrdered[barIndex] << " with index " << barIndex << endl;
		// first set all instruments to not passed and not copied
		for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			it->second.setPassed(false);
			it->second.setCopied(barIndex, false);
			// if this is not the first bar, set a default clef similar to the last one
			if (barIndex > 0) it->second.setClef(barIndex, it->second.getClef(getPrevBarIndex()));
		}
		commandsMap[livelily][barName] = ofColor::orchid;
		// since bars include simple melodic lines in their definition
		// we need to know this when we parse these lines
		if (cmdInput.inputVec.size() > 2) {
			vector<string> v = {cmdInput.inputVec.begin()+2, cmdInput.inputVec.end()};
			std::pair<int, string> p = parseString(genStrFromVec(v), lineNum, numLines);
			return genOutputFuncs(p);
		}
	}

	else if (cmdInput.inputVec[0].compare("\\loop") == 0) {
		if (cmdInput.inputVec.size() < 2) {
			return genError("\\loop command takes a name as an argument");
		}
		if (cmdInput.inputVec.size() > 1 && startsWith(cmdInput.inputVec[1], "{")) {
			return genError("\\loop command must take a name as an argument");
		}
		string loopName = "\\" + cmdInput.inputVec[1];
		auto loopExists = sharedData.loopsIndexes.find(loopName);
		if (loopExists != sharedData.loopsIndexes.end()) {
			return genError((string)"loop " + loopName.substr(1) + (string)" already exists");
		}
		parsingLoop = true;
		storeNewLoop(loopName);
		commandsMap[livelily][loopName] = ofColor::lightSeaGreen;
		// loops are most likely defined in one-liners, so we parse the rest of the line here
		if (cmdInput.inputVec.size() > 2) {
			//string restOfCommand = getRestOfCommand(commands, 2);
			string restOfCommand = genStrFromVec({cmdInput.inputVec.begin()+2, cmdInput.inputVec.end()});
			std::pair<int, string> error = parseString(restOfCommand, lineNum, numLines);
			if (error.first > 0) {
				parsingLoop = false;
				return genOutputFuncs(error);
			}
			lastLoopCommand = genStrFromVec(cmdInput.inputVec);
		}
	}

	else if (cmdInput.inputVec[0].compare("\\bars") == 0) {
		if (cmdInput.inputVec.size() < 2) {
			barError = true;
			return genError("\\bars command takes a name for a set of bars as an argument");
		}
		if (barsIterCounter == 0) {
			for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				it->second.setMultiBarsDone(false);
				it->second.setMultiBarsStrBegin(0);
			}
			parsingBars = true;
		}
		multiBarsName = cmdInput.inputVec[1];
		auto barsExist = sharedData.loopsIndexes.find("\\" + multiBarsName);
		if (barsExist != sharedData.loopsIndexes.end()) {
			return genError((string)"bars " + multiBarsName + (string)" already exist");
		}
		barsIterCounter++;
		numBarsToParse = 0;
		numBarsToParseSet = false;
		// store each bar separately with the "-x" suffix, where x is the counter
		// of the iterations, until all instruments have had all their separate bars parsed
		cmdOutput.outputVec.insert(cmdOutput.outputVec.end(), {"\\bar", multiBarsName+"-"+to_string(barsIterCounter)});
		CmdInput cmdInputLocal = {cmdOutput.outputVec, vector<string>(), false, true};
		parseCommand(cmdInputLocal, lineNum, numLines);
	}

	else if (cmdInput.inputVec[0].compare("\\random") == 0 || cmdInput.inputVec[0].compare("\\rand") == 0) {
		int start = 0, end = 0, diff = 0, randVal;
		bool symbols = false;
		if (cmdInput.inputVec.size() < 2) {
			return genError(cmdInput.inputVec[0] + " takes at least one argument");
		}
		if (cmdInput.inputVec.size() == 2) {
			if (isNumber(cmdInput.inputVec[1])) {
				end = stoi(cmdInput.inputVec[1]);
			}
			else {
				return genError("if only one argument is provided to " + cmdInput.inputVec[0] + ", it must be a number");
			}
		}
		else {
			if (cmdInput.inputVec.size() == 3) {
				if ((isNumber(cmdInput.inputVec[1]) && !isNumber(cmdInput.inputVec[2])) || (!isNumber(cmdInput.inputVec[1]) && isNumber(cmdInput.inputVec[2]))) {
					return genError("if " + cmdInput.inputVec[0] + " receives two arguments, they should both be of the same type, int or string");
				}
				if (isNumber(cmdInput.inputVec[1])) {
					start = stoi(cmdInput.inputVec[1]);
					end = stoi(cmdInput.inputVec[2]);
				}
				else {
					end = (int)cmdInput.inputVec.size() - 1;
					symbols = true;
				}
			}
			else {
				end = (int)cmdInput.inputVec.size() - 1;
				symbols = true;
			}
		}
		diff = end - start;
		randVal = rand() % diff + start;
		if (!cmdInput.isMainCmd) {
			cmdOutput.toPop = cmdInput.inputVec.size();
			if (symbols) {
				return genOutput(cmdInput.inputVec[randVal+1]);
			}
			else {
				return genOutput(to_string(randVal));
			}
		}
	}

	else if (cmdInput.inputVec[0].compare("\\list") == 0) {
		if (cmdInput.inputVec.size() < 2) {
			return genError("\\list takes a name as an argument");
		}
		if (cmdInput.inputVec.size() < 3) {
			return genError("\\list must be assigned at least one item, inside curly brackets");
		}
		string listName = "\\" + cmdInput.inputVec[1];
		bool firstList = true;
		if (listIndexes.size() > 0) {
			firstList = false;
			if (listIndexes.find(listName) != listIndexes.end()) {
				return genError("list already exists");
			}
		}
		storingList = true;
		if (firstList) lastListIndex = 0;
		else lastListIndex = listIndexes.rbegin()->second + 1;
		listIndexes[listName] = lastListIndex;
		commandsMap[livelily][listName] = ofColor::lightSeaGreen;
		string restOfCommand = genStrFromVec({cmdInput.inputVec.begin()+2, cmdInput.inputVec.end()});
		std::pair<int, string> error = parseString(restOfCommand, lineNum, numLines);
		if (error.first == 3) {
			storingList = false;
			return genError(error.second);
		}
		keywords.push_back(listName);
	}

	else if (cmdInput.inputVec[0].compare("\\fromosc") == 0) {
		if (cmdInput.inputVec.size() > 2) {
			return genError("\\fromosc commands takes maximum one argument");
		}
		if (cmdInput.inputVec.size() > 1) {
			if (!startsWith(cmdInput.inputVec[1], "/")) {
				return genError("OSC address must start with /");
			}
			fromOscAddr[whichPane] = cmdInput.inputVec[1];
		}
		else {
			fromOscAddr[whichPane] = "/livelily" + to_string(whichPane+1);
		}
		fromOsc[whichPane] = true;
		if (!oscReceiverIsSet) {
			oscReceiver.setup(OSCPORT);
			oscReceiverIsSet = true;
		}
	}

	else if (cmdInput.inputVec[0].compare("\\remaining") == 0) {
		if (!parsingBar) {
			return genError("\\remaining can only be called inside a bar definition");
		}
		int numFoundInsts = 0;
		if (cmdInput.inputVec.size() == 2) {
			if (sharedData.loopsIndexes.find(cmdInput.inputVec[1]) == sharedData.loopsIndexes.end()) {
				return genError("bar " + cmdInput.inputVec[1] + " doesn't exist");
			}
		}
		else {
			return genError("\\remaining takes one argument");
		}
		for (map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			if (!it->second.hasPassed()) {
				numFoundInsts++;
				break;
			}
		}
		if (numFoundInsts == 0) {
			return genWarning("all instruments have been defined, nothing left for \\remaining");
		}
		for (map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			if (!it->second.hasPassed()) {
				vector<string> v = {"\\" + it->second.getName()};
				v.insert(v.end(), cmdInput.inputVec.begin()+1, cmdInput.inputVec.end());
				// isInstrument returns a pair of bool and CmdOutput
				// this is because in parseCommand() the bool is used to determine whether the first command is indeed an instrument
				// if it is, parseCommand() will end at calling isInstrument()
				// otherwise it goes on
				// here we know that the first item in the v vector is an instrument
				// but we need to comply with what isInstrument() returns
				std::pair<bool, CmdOutput> p = isInstrument(v, lineNum, numLines);
				if (p.second.errorCode > 0) {
					return genOutputFuncs(std::make_pair(p.second.errorCode, p.second.errorStr));
				}
			}
		}
	}

	else if (cmdInput.inputVec[0].compare("\\solo") == 0) {
		if (cmdInput.inputVec.size() < 2) {
			return genError("\\solo commands takes at least one argument");
		}
		if (cmdInput.inputVec.size() - 1 > sharedData.instrumentIndexes.size()) {
			return genError("too many arguments for \\solo command");
		}
		if (cmdInput.inputVec.size() == 2 && cmdInput.inputVec[1].compare("\\all") == 0) {
			for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				sharedData.instruments.at(it->second).setToUnmute(true);
			}
		}
		else {
			vector<string> soloInsts;
			for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
				if (sharedData.instrumentIndexes.find(cmdInput.inputVec[i]) == sharedData.instrumentIndexes.end()) {
					return genError(cmdInput.inputVec[i] + ": unknown instrument");
				}
				else {
					soloInsts.push_back(cmdInput.inputVec[i]);
				}
			}
			// mute all instruments but the solo ones
			for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				if (find(soloInsts.begin(), soloInsts.end(), it->first) >= soloInsts.end()) {
					sharedData.instruments.at(it->second).setToMute(true);
				}
			}
		}
	}

	else if (cmdInput.inputVec[0].compare("\\solonow") == 0) {
		if (cmdInput.inputVec.size() < 2) {
			return genError("\\solonow commands takes at least one argument");
		}
		// below we test if the number of tokens is greater than the number of instruments
		// and not greater than or equal to, because the first token in the \solonow command
		if (cmdInput.inputVec.size() > sharedData.instrumentIndexes.size()) {
			return genError("too many arguments for \\solonow command");
		}
		if (cmdInput.inputVec.size() == 2 && cmdInput.inputVec[1].compare("\\all") == 0) {
			for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				sharedData.instruments.at(it->second).setMute(true);
			}
		}
		else {
			vector<string> soloInsts;
			for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
				if (sharedData.instrumentIndexes.find(cmdInput.inputVec[i]) == sharedData.instrumentIndexes.end()) {
					return genError(cmdInput.inputVec[i] + ": unknown instrument");
				}
				else {
					soloInsts.push_back(cmdInput.inputVec[i]);
				}
			}
			// mute all instruments but the solo ones
			for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				if (find(soloInsts.begin(), soloInsts.end(), it->first) >= soloInsts.end()) {
					sharedData.instruments.at(it->second).setMute(true);
				}
			}
		}
	}

	else if (cmdInput.inputVec[0].compare("\\mute") == 0) {
		if (cmdInput.inputVec.size() < 2) {
			return genError("\\mute command takes at least one argument");
		}
		// again test if greater than and not if greater than or equal to
		if (cmdInput.inputVec.size() - 1 > sharedData.instruments.size()) {
			return genError("too many arguments for \\mute command");
		}
		if (cmdInput.inputVec[1].compare("\\all") == 0) {
			for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				sharedData.instruments.at(it->second).setToMute(true);
			}
		}
		else {
			vector<string> muteInsts;
			for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
				if (sharedData.instrumentIndexes.find(cmdInput.inputVec[i]) == sharedData.instrumentIndexes.end()) {
					return genError(cmdInput.inputVec[i] + ": unknown instrument");
				}
				else {
					muteInsts.push_back(cmdInput.inputVec[i]);
				}
			}
			for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				if (find(muteInsts.begin(), muteInsts.end(), it->first) != muteInsts.end()) {
					sharedData.instruments.at(it->second).setToMute(true);
				}
			}
		}
	}

	else if (cmdInput.inputVec[0].compare("\\unmute") == 0) {
		if (cmdInput.inputVec.size() < 2) {
			return genError("\\unmute command takes at least one argument");
		}
		// again test if greater than and not if greater than or equal to
		if (cmdInput.inputVec.size() > sharedData.instruments.size()) {
			return genError("too many arguments for \\unmute command");
		}
		if (cmdInput.inputVec[1].compare("\\all") == 0) {
			for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				sharedData.instruments[it->second].setToUnmute(true);
			}
		}
		else {
			vector<string> unmuteInsts;
			for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
				if (sharedData.instrumentIndexes.find(cmdInput.inputVec[i]) == sharedData.instrumentIndexes.end()) {
					return genError(cmdInput.inputVec[i] + ": unknown instrument");
				}
				else {
					unmuteInsts.push_back(cmdInput.inputVec[i]);
				}
			}
			for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				if (find(unmuteInsts.begin(), unmuteInsts.end(), it->first) != unmuteInsts.end()) {
					sharedData.instruments.at(it->second).setToUnmute(true);
				}
			}
		}
	}

	else if (cmdInput.inputVec[0].compare("\\mutenow") == 0) {
		if (cmdInput.inputVec.size() < 2) {
			return genError("\\mutenow command takes at least one argument");
		}
		// again test if greater than and not if greater than or equal to
		if (cmdInput.inputVec.size() > sharedData.instruments.size()) {
			return genError("too many arguments for \\mutenow command");
		}
		if (cmdInput.inputVec[1].compare("\\all") == 0) {
			for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				sharedData.instruments[it->second].setMute(true);
			}
		}
		else {
			vector<string> muteInsts;
			for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
				if (sharedData.instrumentIndexes.find(cmdInput.inputVec[i]) == sharedData.instrumentIndexes.end()) {
					return genError(cmdInput.inputVec[i] + ": unknown instrument");
				}
				else {
					muteInsts.push_back(cmdInput.inputVec[i]);
				}
			}
			for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				if (find(muteInsts.begin(), muteInsts.end(), it->first) != muteInsts.end()) {
					sharedData.instruments.at(it->second).setMute(true);
				}
			}
		}
	}

	else if (cmdInput.inputVec[0].compare("\\unmutenow") == 0) {
		if (cmdInput.inputVec.size() < 2) {
			return genError("\\unmutenow command takes at least one argument");
		}
		// again test if greater than and not if greater than or equal to
		if (cmdInput.inputVec.size() > sharedData.instruments.size()) {
			return genError("too many arguments for \\unmutenow command");
		}
		if (cmdInput.inputVec[1].compare("\\all") == 0) {
			for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				sharedData.instruments[it->second].setMute(false);
			}
		}
		else {
			vector<string> unmuteInsts;
			for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
				if (sharedData.instrumentIndexes.find(cmdInput.inputVec[i]) == sharedData.instrumentIndexes.end()) {
					return genError(cmdInput.inputVec[i] + ": unknown instrument");
				}
				else {
					unmuteInsts.push_back(cmdInput.inputVec[i]);
				}
			}
			for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				if (find(unmuteInsts.begin(), unmuteInsts.end(), it->first) != unmuteInsts.end()) {
					sharedData.instruments.at(it->second).setMute(false);
				}
			}
		}
	}

	else if (cmdInput.inputVec[0].compare("\\function") == 0) {
		if (cmdInput.inputVec.size() < 2) {
			return genError("\\function needs to be given a name");
		}
		if (startsWith(cmdInput.inputVec[1], "{")) {
			return genError("\\function needs to be given a name");
		}
		if (cmdInput.inputVec.size() > 2) {
			if (!startsWith(cmdInput.inputVec[2], "{")) {
				return genError("after the function name, the function body must be enclosed in curly brackets");
			}
		}
		if (functionIndexes.find("\\"+cmdInput.inputVec[1]) != functionIndexes.end()) {
			lastFunctionIndex = functionIndexes["\\"+cmdInput.inputVec[1]];
			sharedData.functions[lastFunctionIndex].clear();
			storingFunction = true;
		}
		else {
			int functionNdx = 0;
			string functionName = "\\"+cmdInput.inputVec[1];
			if (functionIndexes.size() > 0) {
				map<string, int>::reverse_iterator it = functionIndexes.rbegin();
				functionNdx = it->second + 1;
			}
			functionIndexes[functionName] = functionNdx;
			Function function;
			sharedData.functions[functionNdx] = function;
			sharedData.functions[functionNdx].setName(functionName);
			if (sharedData.functions[functionNdx].getNameError()) {
				return genError("function name too long");
			}
			commandsMap[livelily][functionName] = ofColor::turquoise;
			storingFunction = true;
			lastFunctionIndex = functionNdx;
			keywords.push_back(functionName);
		}
		// check if there are more characters in this line
		// and then get all the strings in all executing lines
		// to allocate memory for the char array of functions
		size_t functionBodySize = 0;
		if (cmdInput.inputVec.size() > 2) {
			// if the line of the function definition includes more than the opening bracket
			if (cmdInput.inputVec[2].compare("{") != 0) {
				functionBodySize += cmdInput.inputVec[2].substr(1).size();
				// add one character for the new line
				functionBodySize++;
			}
		}
		for (int i = lineNum+1; i < lineNum+numLines; i++) {
			string line = editors[whichPane].getLine(i);
			int offset = 0;
			int tabSize = editors[whichPane].getTabSize();
			if ((int)line.size() >= tabSize) {
				if (line.substr(0, tabSize).compare(editors[whichPane].tabStr) == 0) {
					offset = tabSize;
				}
			}
			functionBodySize += line.substr(offset).size();
			if (endsWith(line, "}")) functionBodySize--;
			// add one for the new line character
			functionBodySize++;
		}
		// even though there is no newline in the last line, we don't subtract one from
		// the size to be allocated for the null terminating character
		sharedData.functions[lastFunctionIndex].allocate(functionBodySize);
		functionBodyOffset = 0;
		string restOfCommand = genStrFromVec({cmdInput.inputVec.begin()+2, cmdInput.inputVec.end()});
		std::pair<int, string> error = parseString(restOfCommand, lineNum, numLines);
		if (error.first > 0) {
			cmdOutput.errorCode = error.first;
			cmdOutput.errorStr = error.second;
			storingFunction = false;
		}
	}

	else if (cmdInput.inputVec[0].compare("\\miditune") == 0) {
		if (cmdInput.inputVec.size() != 2) {
			return genError("\\miditune takes one argument, the MIDI tuning frequency");
		}
		if (!isNumber(cmdInput.inputVec[1])) {
			return genError("the argument to \\miditune must be a number");
		}
		int midiTune = stoi(cmdInput.inputVec[1]);
		if (midiTune < 0) {
			return genError("MIDI tuning frequency must be positive");
		}
		sequencer.setMidiTune(midiTune);
	}

	else if (cmdInput.inputVec[0].compare("\\listmidiports") == 0) {
		if (cmdInput.inputVec.size() > 1) {
			return genError("\\listmidiports command takes no arguments");
		}
		sequencer.midiOutPorts = sequencer.midiOut.getOutPortList();
		string noteStr = "";
		for (unsigned i = 0; i < sequencer.midiOutPorts.size()-1; i++) {
			noteStr += "MIDI out port " + to_string(i) + ": " + sequencer.midiOutPorts[i] += "\n";
		}
		// append the last port separately to not include the newline
		noteStr += "MIDI out port " + to_string(sequencer.midiOutPorts.size()-1) + ": " + sequencer.midiOutPorts.back();
		return genNote(noteStr);
	}

	else if (cmdInput.inputVec[0].compare("\\openmidiport") == 0) {
		if (cmdInput.inputVec.size() != 2) {
			return genError("\\openmidiport  takes one argument, the MIDI port to open");
		}
		if (!isNumber(cmdInput.inputVec[1])) {
			return genError("MIDI port must be a number");
		}
		int midiPort = stoi(cmdInput.inputVec[1]);
		if (midiPort < 0) {
			return genError("MIDI port must be positive");
		}
		if (midiPort >= (int)sequencer.midiOutPorts.size()) {
			return genError("MIDI port doesn't exist");
		}
		if (sequencer.midiPortsMap.find(midiPort) == sequencer.midiPortsMap.end()) {
			ofxMidiOut midiOut;
			midiOut.openPort(midiPort);
			sequencer.midiOuts.push_back(midiOut);
			sequencer.midiPortsMap[midiPort] = (int)sequencer.midiOuts.size()-1;
			return genNote("opened MIDI port " + cmdInput.inputVec[1]);
		}
		else {
			return genError("MIDI port " + cmdInput.inputVec[1] + " is already open");
		}
	}
	
	else if (cmdInput.inputVec[0].compare("\\ctlchange") == 0) {
		if (cmdInput.inputVec.size() != 5) {
			return genError("\\ctlchange takes four arguments, port, channel, controller nr, and value");
		}
		if (!isNumber(cmdInput.inputVec[1]) || !isNumber(cmdInput.inputVec[2]) || !isNumber(cmdInput.inputVec[3]) || !isNumber(cmdInput.inputVec[4])) {
			return genError("all arguments to \\ctlchange must be integers");
		}
		int port = stoi(cmdInput.inputVec[1]);
		if (sequencer.midiPortsMap.find(port) == sequencer.midiPortsMap.end()) {
			return genError("selected MIDI port is not open");
		}
		int channel = stoi(cmdInput.inputVec[2]);
		int control = stoi(cmdInput.inputVec[3]);
		int val = stoi(cmdInput.inputVec[4]);
		if (channel < 1 || channel > 16) {
			return genError("MIDI channel must be between 1 and 16");
		}
		if (control < 0) {
			return genError("control number must be positive");
		}
		if (val < 0 || val > 127) {
			return genError("MIDI value must be between 0 and 127");
		}
		sequencer.midiOuts[sequencer.midiPortsMap[port]].sendControlChange(channel, control, val);
	}
	
	else if (cmdInput.inputVec[0].compare("\\pgmchange") == 0) {
		if (cmdInput.inputVec.size() != 4) {
			return genError("\\pgmchange takes three arguments, port, channel, and value");
		}
		if (!isNumber(cmdInput.inputVec[1]) || !isNumber(cmdInput.inputVec[2]) || !isNumber(cmdInput.inputVec[3])) {
			return genError("all arguments to \\pgmchange must be integers");
		}
		int port = stoi(cmdInput.inputVec[1]);
		if (sequencer.midiPortsMap.find(port) == sequencer.midiPortsMap.end()) {
			return genError("selected MIDI port is not open");
		}
		int channel = stoi(cmdInput.inputVec[2]);
		int val = stoi(cmdInput.inputVec[3]);
		if (channel < 1 || channel > 16) {
			return genError("MIDI channel must be between 1 and 16");
		}
		if (val < 0 || val > 127) {
			return genError("MIDI value must be between 0 and 127");
		}
		sequencer.midiOuts[sequencer.midiPortsMap[port]].sendProgramChange(channel, val);
	}

	// the test below includes all functions that set duration percetanges for notes
	// currently default Note On duration, staccato, staccatissimo, and tenuto
	else if (cmdInput.inputVec[0].compare("\\dur") == 0 || startsWith(cmdInput.inputVec[0], "\\dur.")) {
		if (cmdInput.inputVec[0].compare("\\dur") == 0 && cmdInput.inputVec.size() < 2) {
			return genError("the \\dur command does nothing by itself, you must call one of its second level commands");
		}
		else if (cmdInput.inputVec[0].compare("\\dur") == 0 && cmdInput.inputVec.size() >= 2) {
			return genError("the \\dur command takes only second level commands, not arguments");
		}
		string subcommand = cmdInput.inputVec[0].substr(cmdInput.inputVec[0].find(".")+1);
		if (cmdInput.inputVec.size() == 1) {
			return genError("no percentage value provided");
		}
		if (cmdInput.inputVec.size() > 2) {
			return genError("\\dur." + subcommand + " command takes one argument only");
		}
		if (!isNumber(cmdInput.inputVec[1])) {
			return genError("percentage to \\dur." + subcommand + " must be an integer");
		}
		int percentage = stoi(cmdInput.inputVec[1]);
		if (percentage < 0) {
			return genError("percentage to \\dur." + subcommand + " can't be below 0");
		}
		if (percentage == 0) {
			return genError("percentage to \\dur." + subcommand + " can't be 0");
		}
		if (percentage > 100) {
			return genError("percentage to \\dur." + subcommand + " can't be over 100\%");
		}
		for (unsigned i = 0; i < sharedData.instruments.size(); i++) {
			int error = sharedData.instruments[i].setDuration(subcommand, (float)percentage/10.0);
			if (error) return genError("unknown duration second level command");
		}
		if (percentage == 100) {
			return genWarning("percentage to \\dur." + subcommand + " set to 100\% of Note On duration");
		}
	}

	else if (cmdInput.inputVec[0].compare("\\listserialports") == 0) {
		if (cmdInput.inputVec.size() > 1) {
			return genError("\\listserialports command takes no arguments");
		}
		serial.listDevices();
		serialDeviceList = serial.getDeviceList();
		string outPortsOneStr = "";
		for (unsigned i = 0; i < serialDeviceList.size()-1; i++) {
			outPortsOneStr += "Serial port " + ofToString(serialDeviceList[i].getDeviceID()) + \
							  ": " + serialDeviceList[i].getDevicePath() + "\n";
		}
		// append last port separately to not include the newline
		outPortsOneStr += "Serial port " + ofToString(serialDeviceList.back().getDeviceID()) + \
						  ": " + serialDeviceList.back().getDevicePath();
		return genNote(outPortsOneStr);
	}

	else if (cmdInput.inputVec[0].compare("\\openserialport") == 0) {
		if (cmdInput.inputVec.size() < 3 || cmdInput.inputVec.size() > 3) {
			return genError("\\openserialport takes two arguments, port path and baud rate");
		}
		int serialPort;
		bool serialPortNumSet = false;
		if (isNumber(cmdInput.inputVec[1])) {
			serialPort = stoi(cmdInput.inputVec[1]);
			if (serialPort < 0) {
				return genError("serial port list number must be positive");
			}
			if (serialPort >= (int)serialDeviceList.size()) {
				return genError("serial port list number out of range");
			}
			serialPortNumSet = true;
		}
		if (!isNumber(cmdInput.inputVec[2])) {
			return genError("second argument must be baud rate");
		}
		if (serialPortNumSet) {
			serial.setup(serialDeviceList[serialPort].getDevicePath(), stoi(cmdInput.inputVec[2]));
		}
		else {
			bool serialPortPathFound = false;
			for (unsigned i = 0; i < serialDeviceList.size(); i++) {
				if (serialDeviceList[i].getDevicePath().compare(cmdInput.inputVec[1]) == 0) {
					serialPortPathFound = true;
					break;
				}
			}
			if (serialPortPathFound) {
				serial.setup(cmdInput.inputVec[1], stoi(cmdInput.inputVec[2]));
			}
			else {
				return genError("serial port path \"" + cmdInput.inputVec[1] + "\" doesn't exist");
			}
		}
		serialPortOpen = true;
		return genNote("opened " + cmdInput.inputVec[1] + " at baud " + cmdInput.inputVec[2]);
	}

	else if (cmdInput.inputVec[0].compare("\\serialwrite") == 0) {
		if (cmdInput.inputVec.size() == 1) {
			return genError("\\serialwrite takes at least one argument, a byte to send over serial");
		}
		if (!serial.isInitialized()) {
			return genError("serial port is not open");
		}
		for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
			if (!isNumber(cmdInput.inputVec[i])) {
				return genError("\\serialwrite takes numbers from 0 to 255 only");
			}
			int value = stoi(cmdInput.inputVec[i]);
			if (value < 0 || value > 255) {
				return genError("\\serialwrite takes numbers from 0 to 255");
			}
			serial.writeByte((char)value);
		}
	}

	else if (cmdInput.inputVec[0].compare("\\serialprint") == 0) {
		if (cmdInput.inputVec.size() == 1) {
			return genError("\\serialprint takes at least one argument, a character to send over serial");
		}
		if (!serial.isInitialized()) {
			return genError("serial port is not open");
		}
		size_t charSize = 0;
		for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
			for (unsigned j = 0; j < cmdInput.inputVec[i].size(); j++) {
				charSize++;
			}
			charSize++;
		}
		// the last addition happens one time two many, so we subtract one here
		charSize--;
		unsigned char buf[charSize];
		int index = 0;
		for (unsigned i = 1; i < cmdInput.inputVec.size(); i++) {
			for (unsigned j = 0; j < cmdInput.inputVec[i].size(); j++) {
				buf[index++] = cmdInput.inputVec[i][j];
			}
			if (i < cmdInput.inputVec.size() - 1) buf[index++] = ' ';
		}
		serial.writeBytes(&buf[0], charSize);
	}

	else if (cmdInput.inputVec[0].compare("\\barlines") == 0) {
		if (cmdInput.inputVec.size() != 2) {
			return genError("\\barlines takes one argument, the name of the bar to get its lines");
		}
		if (sharedData.barLines.find(cmdInput.inputVec[1]) == sharedData.barLines.end()) {
			return genError((string)"bar name \"" + cmdInput.inputVec[1] + (string)"\" doesn't exist");
		}
		return genOutput(sharedData.barLines[cmdInput.inputVec[1]]);
	}

	else if (cmdInput.inputVec[0].compare("\\closeserialport") == 0) {
		if (serial.isInitialized()) {
			serial.close();
		}
	}

	else if (cmdInput.inputVec[0].compare("\\system") == 0) {
		if (cmdInput.inputVec.size() == 1) {
			return genWarning("no command added to \\system");
		}
		unsigned i;
		string sysCommand = "";
		for (i = 1; i < cmdInput.inputVec.size(); i++) {
			if (i > 1) sysCommand += " ";
			sysCommand += cmdInput.inputVec[i];
		}
		return genOutput(ofSystem(sysCommand), cmdInput.inputVec.size()-1);
	}

	else if (cmdInput.inputVec[0].compare("\\autobrackets") == 0) {
		if (cmdInput.inputVec.size() != 2) {
			return genError("\\autobrackets takes one argument only, \"on\" or \"off\"");
		}
		if (cmdInput.inputVec[1].compare("on") != 0 && cmdInput.inputVec[1].compare("off") != 0) {
			return genError(" argument to \\autobrackets must be \"on\" or \"off\"");
		}
		if (cmdInput.inputVec[1].compare("on") == 0) {
			editors[whichPane].setAutocomplete(true);
		}
		else {
			editors[whichPane].setAutocomplete(false);
		}
	}

	else if (cmdInput.inputVec[0].compare("\\active") == 0) {
		if (cmdInput.inputVec.size() > 1) {
			return genError("\\active takes no arguments");
		}
		editors[whichPane].setSessionActivity(true);
	}

	else if (cmdInput.inputVec[0].compare("\\inactive") == 0) {
		if (cmdInput.inputVec.size() > 1) {
			return genError("\\inactive takes no arguments");
		}
		editors[whichPane].setSessionActivity(false);
	}

	else if (cmdInput.inputVec[0].compare("\\sendkeys") == 0) {
		if (cmdInput.inputVec.size() != 3) {
			return genError("\\sendkeys takes two arguments, host IP and pane index");
		}
		string hostIP = cmdInput.inputVec[1];
		int paneNdx;
		if (!isNumber(cmdInput.inputVec[2])) {
			return genError("pane index of host is not a number");
		}
		paneNdx = stoi(cmdInput.inputVec[2]);
		editors[whichPane].oscKeys.setup(hostIP, OSCPORT);
		editors[whichPane].setSendKeys(paneNdx);
	}
		
	else if (cmdInput.inputVec[0].compare("\\sendlines") == 0) {
		if (cmdInput.inputVec.size() != 3) {
			return genError("\\sendlines takes two arguments, host IP and pane index");
		}
		string hostIP = cmdInput.inputVec[1];
		int paneNdx;
		if (!isNumber(cmdInput.inputVec[2])) {
			return genError("pane index of host is not a number");
		}
		paneNdx = stoi(cmdInput.inputVec[2]);
		editors[whichPane].oscKeys.setup(hostIP, OSCPORT);
		editors[whichPane].setSendLines(paneNdx);
	}

	else if (cmdInput.inputVec[0].compare("\\maestro") == 0) {
		return maestroCommands(cmdInput.inputVec, lineNum, numLines);
	}

	else if (cmdInput.inputVec[0].compare("\\framerate") == 0) {
		if (cmdInput.inputVec.size() == 1) {
			return genOutput(ofToString(ofGetFrameRate()));
		}
		if (cmdInput.inputVec.size() != 2) {
			return genError("\\framerate takes one argument, the framerate");
		}
		if (!isNumber(cmdInput.inputVec[1])) {
			return genError("argument to \\framerate must be an integer");
		}
		int framerate = stoi(cmdInput.inputVec[1]);
		if (framerate <= 0) {
			return genError("framerate must be positive and higher than 0");
		}
		ofSetFrameRate(framerate);
	}

	else if (cmdInput.inputVec[0].compare("\\livelily") == 0) {
		if (cmdInput.inputVec.size() > 1) {
			return genError("language command takes no arguments");
		}
		editors[whichPane].setLanguage(livelily);
	}
	
	else if (cmdInput.inputVec[0].compare("\\python") == 0) {
		if (cmdInput.inputVec.size() > 1) {
			return genError("language command takes no arguments");
		}
		editors[whichPane].setLanguage(python);
	}
	
	else if (cmdInput.inputVec[0].compare("\\lua") == 0) {
		if (cmdInput.inputVec.size() > 1) {
			return genError("language command takes no arguments");
		}
		editors[whichPane].setLanguage(lua);
	}

	else if (cmdInput.inputVec[0].compare("\\accoffset") == 0) {
		if (cmdInput.inputVec.size() != 2) {
			return genError("\\accoffset command takes one argument, the offset for accidentals that are close together");
		}
		if (!isFloat(cmdInput.inputVec[1])) {
			return genError("argument to \\accoffset is not a number");
		}
		float coef = std::stof(cmdInput.inputVec[1]);
		for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			it->second.setAccidentalsOffsetCoef(coef);
		}
	}
	
	else {
		// the functions return a pair with a boolean stating if an instrument, loop, function, or list is found
		// and an error string
		// if the boolean is true, we just return the error string, otherwise we go on to the next function
		std::pair<bool, CmdOutput> p;
		p = isInstrument(cmdInput.inputVec, lineNum, numLines);
		if (p.first) return p.second;
		p = isBarLoop(cmdInput.inputVec, cmdInput.isMainCmd, lineNum, numLines);
		if (p.first) return p.second;
		p = isFunction(cmdInput.inputVec, lineNum, numLines);
		if (p.first) return p.second;
		p = isList(cmdInput.inputVec, lineNum, numLines);
		if (p.first) return p.second;
		p = isOscClient(cmdInput.inputVec, lineNum, numLines);
		if (p.first) return p.second;
		p = isGroup(cmdInput.inputVec, lineNum, numLines);
		if (p.first) return p.second;
		return genError(cmdInput.inputVec[0] + ": unknown command");
	}
	return cmdOutput;
}

//--------------------------------------------------------------
std::pair<bool, CmdOutput> ofApp::isInstrument(vector<string>& originalCommands, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	bool hasDot = false;
	vector<string> commands;
	// the initial command is \instname which might be followed by a second level command
	// to separate the second level command we create a new vector that will copy the originalCommands vector
	// except from the first item where we trim the \instname. part
	for (unsigned i = 0; i < originalCommands.size(); i++) {
		if (!i) {
			// separate the name of the instrument from a possible second level command
			vector<string> tokens = tokenizeString(originalCommands[i], ".");
			for (unsigned j = 0; j < tokens.size(); j++) {
				commands.push_back(tokens[j]);
			}
			if (tokens.size() > 1) hasDot = true;
		}
		else {
			commands.push_back(originalCommands[i]);
		}
	}
	bool instrumentExists = false;
	unsigned commandNdxOffset = 1;
	std::pair<int, string> error = std::make_pair(0, "");
	if (sharedData.instruments.size() > 0) {
		if (sharedData.instrumentIndexes.find(commands[0]) != sharedData.instrumentIndexes.end()) {
			instrumentExists = true;
			lastInstrument = commands[0];
			lastInstrumentIndex = sharedData.instrumentIndexes[commands[0]];
			if (commands.size() > 1) {
				//if (commands[1].compare("\\clef") == 0) {
				//	if (hasDot) {
				//		return std::make_pair(instrumentExists, genError("\\clef is a first level command, should not be concatenated to instrument name with a dot"));
				//	}
				//	if (commands.size() < 3) {
				//		return std::make_pair(instrumentExists, genError("\\clef command takes a clef name as an argument"));
				//	}
				//	else if (commands.size() > 3 && !(parsingBar || parsingBars)) {
				//		return std::make_pair(instrumentExists, genError("\\clef command takes one argument only"));
				//	}
				//	if (commands[2].compare("treble") == 0) {
				//		sharedData.instruments[lastInstrumentIndex].setClef(getLastBarIndex(), 0);
				//	}
				//	else if (commands[2].compare("bass") == 0) {
				//		sharedData.instruments[lastInstrumentIndex].setClef(getLastBarIndex(), 1);
				//	}
				//	else if (commands[2].compare("alto") == 0) {
				//		sharedData.instruments[lastInstrumentIndex].setClef(getLastBarIndex(), 2);
				//	}
				//	else if (commands[2].compare("perc") == 0 || commands[2].compare("percussion") == 0) {
				//		sharedData.instruments[lastInstrumentIndex].setClef(getLastBarIndex(), 3);
				//	}
				//	else {
				//		return std::make_pair(instrumentExists, genError("unknown clef"));
				//	}
				//	if (commands.size() > 3) {
				//		commandNdxOffset = 3;
				//		goto parseMelody;
				//	}
				//	// the \clef command must be included inside a melodic line definition which is expanded before fully interpreted
				//	// that's why we generate an empty string output here
				//	return std::make_pair(instrumentExists, genOutput(""));
				//}
				if (commands[1].compare("rhythm") == 0) {
					if (hasDot) {
						return std::make_pair(instrumentExists, genError("\"rhythm\" is an arguement, not a second level command, should not be concatenated to instrument name with a dot"));
					}
					if (commands.size() > 2) {
						return std::make_pair(instrumentExists, genError("\"rhythm\" argument takes no further arguments"));
					}
					sharedData.instruments[lastInstrumentIndex].setRhythm(true);
				}
				else if (commands[1].compare("transpose") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"transpose\" is a second level command, must be concatenated to instrument name with a dot"));
					}
					if (commands.size() != 3) {
						return std::make_pair(instrumentExists, genError("\"transpose\" command takes one argument"));
					}
					if (!isNumber(commands[2])) {
						return std::make_pair(instrumentExists, genError("argument to \"transpose\" command must be a number"));
					}
					int transposition = stoi(commands[2]);
					if (transposition < -11 || transposition > 11) {
						return std::make_pair(instrumentExists, genError("argument to \"transpose\" command must be between -11 and 11"));
					}
					sharedData.instruments[lastInstrumentIndex].setTransposition(transposition);
					if (transposition == 0) {
						return std::make_pair(instrumentExists, genWarning("0 \"transpose\" has no effect"));
					}
				}
				else if (commands[1].compare("sendmidi") == 0) {
					if (hasDot) {
						return std::make_pair(instrumentExists, genError("\"sendmidi\" is an arguement, not a second level command, should not be concatenated to instrument name with a dot"));
					}
					if (commands.size() > 2) {
						return std::make_pair(instrumentExists, genError("\"sendmidi\" takes no arguments"));
					}
					sharedData.instruments[lastInstrumentIndex].setSendMIDI(true);
				}
				else if (commands[1].compare("sendto") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"sendto\" is a second level command, must be concatenated to instrument name with a dot"));
					}
					int sendNotice = 0;
					if (commands.size() > 4) {
						return std::make_pair(instrumentExists, genError("\"sendto\" takes zero to two arguments, remote IP and/or port"));
					}
					string remoteIP;
					int port;
					if (commands.size() < 3) {
						remoteIP = "127.0.0.1";
						port = SCOREPARTPORT;
						sendNotice = 1;
					}
					else if (commands.size() > 2) {
						if (commands.size() == 4) {
							if (isNumber(commands[2]) || !isNumber(commands[3])) {
								return std::make_pair(instrumentExists, genError("if two arguments are provided, first argument must be remote IP and second must be port number"));
							}
							remoteIP = commands[2];
							port = stoi(commands[3]);
						}
						else {
							if (isNumber(commands[2])) {
								remoteIP = "127.0.0.1";
								port = stoi(commands[2]);
								sendNotice = 2;
							}
							else {
								remoteIP = commands[2];
								port = SCOREPARTPORT;
								sendNotice = 3;
							}
						}
					}
					std::pair<string, int> IPAndPort = std::make_pair(remoteIP, port);
					bool clientExists = false;
					for (auto it = instrumentOSCHostPorts.begin(); it != instrumentOSCHostPorts.end(); ++it) {
						if (IPAndPort == it->second) {
							// if there is already a client that sends to the same server
							// ignore this instrument, as the already existing client will send
							// the generic OSC messages
							clientExists = true;
							break;
						}
					}
					if (!clientExists) {
						grouppedOSCClients.push_back(lastInstrumentIndex);
					}
					instrumentOSCHostPorts[lastInstrumentIndex] = IPAndPort;
					sharedData.instruments[lastInstrumentIndex].sendToPart = true;
					sharedData.instruments[lastInstrumentIndex].scorePartSender.setup(remoteIP, port);
					numScorePartSenders++;
					ofxOscMessage m;
					m.setAddress("/initinst");
					m.addIntArg(lastInstrumentIndex);
					m.addStringArg(sharedData.instruments[lastInstrumentIndex].getName());
					sharedData.instruments[lastInstrumentIndex].scorePartSender.sendMessage(m, false);
					m.clear();
					if (sendNotice > 0) {
						switch (sendNotice) {
							case 1:
								cmdOutput = genNote("setting localhost and port 9000");
								break;
							case 2:
								cmdOutput = genNote("setting localhost");
								break;
							case 3:
								cmdOutput = genNote("setting port 9000");
								break;
							default:
								break;
						}
						return std::make_pair(instrumentExists, cmdOutput);
					}
				}
				else if (commands[1].compare("fullscreen") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"fullscreen\" is a second level command, must be concatenated to instrument name with a dot"));
					}
					if (!sharedData.instruments[lastInstrumentIndex].sendToPart) {
						return std::make_pair(instrumentExists, genError("instrument doesn't have OSC set"));
					}
					if (commands.size() < 3) {
						return std::make_pair(instrumentExists, genError("\"fullscreen\" takes one argument, on or off"));
					}
					if (commands.size() > 3) {
						return std::make_pair(instrumentExists, genError("\"fullscreen\" takes one argument only"));
					}
					if (commands[2].compare("on") == 0) {
						sendFullscreenToPart(lastInstrumentIndex, true);
					}
					else if (commands[2].compare("off") == 0) {
						sendFullscreenToPart(lastInstrumentIndex, false);
					}
					else {
						return std::make_pair(instrumentExists, genError("unknown argument to \"fullscreen\""));
					}
				}
				else if (commands[1].compare("cursor") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"cursor\" is a second level command, must be concatenated to instrument name with a dot"));
					}
					if (!sharedData.instruments[lastInstrumentIndex].sendToPart) {
						return std::make_pair(instrumentExists, genError("instrument doesn't have OSC set"));
					}
					if (commands.size() < 3) {
						return std::make_pair(instrumentExists, genError("\"cursor\" takes one argument, show or hide "));
					}
					if (commands.size() > 3) {
						return std::make_pair(instrumentExists, genError("\"cursor\" takes one argument only"));
					}
					if (commands[2].compare("show") == 0) {
						sendCursorToPart(lastInstrumentIndex, true);
					}
					else if (commands[2].compare("hide") == 0) {
						sendCursorToPart(lastInstrumentIndex, false);
					}
					else {
						return std::make_pair(instrumentExists, genError("unknown argument to \"cursor\""));
					}
				}
				else if (commands[1].compare("update") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"update\" is a second level command, must be concatenated to instrument name with a dot"));
					}
					if (commands.size() < 3) {
						return std::make_pair(instrumentExists, genError("\"update\" takes one argument, \"onlast\" or \"immediately\""));
					}
					if (commands[2].compare("onlast") == 0) {
						sendScoreChangeToPart(lastInstrumentIndex, true);
					}
					else if (commands[2].compare("immediately") == 0) {
						sendScoreChangeToPart(lastInstrumentIndex, false);
					}
					else {
						return std::make_pair(instrumentExists, genError(commands[2] + (string)"unknown argument to \"update\", must be \"onlast\" or \"immediately\""));
					}
				}
				else if (commands[1].compare("beatcolor") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"beatcolor\" is a second level command, must be concatenated to instrument name with a dot"));
					}
					if (commands.size() < 3) {
						return std::make_pair(instrumentExists, genError("\"beatcolor\" takes one argument, \"change\" or \"keep\""));
					}
					if (commands[2].compare("change") == 0) {
						sendChangeBeatColorToPart(lastInstrumentIndex, true);
					}
					else if (commands[2].compare("keep") == 0) {
						sendChangeBeatColorToPart(lastInstrumentIndex, false);
					}
					else {
						return std::make_pair(instrumentExists, genError(commands[2] + (string)"unknown argument to \"beatcolor\", must be \"change\" or \"keep\""));
					}
				}
				else if (commands[1].compare("midiport") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"midiport\" is a second level command, must be concatenated to instrument name with a dot"));
					}
					if (commands.size() != 3) {
						return std::make_pair(instrumentExists, genError("\"midiport\"  takes one argument, the MIDI port to open"));
					}
					if (!isNumber(commands[2])) {
						return std::make_pair(instrumentExists, genError("MIDI port must be a number"));
					}
					int midiPort = stoi(commands[2]);
					if (midiPort < 0) {
						return std::make_pair(instrumentExists, genError("MIDI port must be positive"));
					}
					if (midiPort >= (int)sequencer.midiOutPorts.size()) {
						return std::make_pair(instrumentExists, genError("MIDI port doesn't exist"));
					}
					bool openedMidiPort = false;
					if (sequencer.midiPortsMap.find(midiPort) == sequencer.midiPortsMap.end()) {
						ofxMidiOut midiOut;
						midiOut.openPort(midiPort);
						sequencer.midiOuts.push_back(midiOut);
						sequencer.midiPortsMap[midiPort] = (int)sequencer.midiOuts.size()-1;
						openedMidiPort = true;
					}
					sharedData.instruments[lastInstrumentIndex].setMidiPort(midiPort);
					if (openedMidiPort) {
						return std::make_pair(instrumentExists, genNote("opened MIDI port " + commands[2]));
					}
				}
				else if (commands[1].compare("midichan") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"midichan\" is a second level command, must be concatenated to instrument name with a dot"));
					}
					if (commands.size() == 2) {
						return std::make_pair(instrumentExists, genError("no MIDI channel set"));
					}
					if (commands.size() > 3) {
						return std::make_pair(instrumentExists, genError("\"midichan\"  takes one argument only"));
					}
					if (!isNumber(commands[2])) {
						return std::make_pair(instrumentExists, genError("MIDI channel must be a number"));
					}
					if (sharedData.instruments[lastInstrumentIndex].getMidiPort() < 0) {
						return std::make_pair(instrumentExists, genError("no MIDI port has been set for this instrument"));
					}
					int midiChan = stoi(commands[2]);
					if (midiChan < 1 || midiChan > 16) {
						return std::make_pair(instrumentExists, genError("MIDI must be between 1 and 16"));
					}
					sharedData.instruments[lastInstrumentIndex].setMidiChan(midiChan);
					sharedData.instruments[lastInstrumentIndex].setMidi(true);
				}
				else if (commands[1].compare("size") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"size\" is a second level command, must be concatenated to instrument name with a dot"));
					}
					if (!sharedData.instruments[lastInstrumentIndex].sendToPart) {
						return std::make_pair(instrumentExists, genError("instrument doesn't have OSC set"));
					}
					if (commands.size() != 3) {
						return std::make_pair(instrumentExists, genError("\"size\" command takes one argument, the size value"));
					}
					if (!isNumber(commands[2])) {
						return std::make_pair(instrumentExists, genError(commands[2] + " is not a number"));
					}
					sendSizeToPart(lastInstrumentIndex, stoi(commands[2]));
				}
				else if (commands[1].compare("numbars") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"numbars\" is a second level command, must be concatenated to instrument name with a dot"));
					}
					if (!sharedData.instruments[lastInstrumentIndex].sendToPart) {
						return std::make_pair(instrumentExists, genError("instrument doesn't have OSC set"));
					}
					if (commands.size() != 3) {
						return std::make_pair(instrumentExists, genError("\"numbars\" command takes one argument, the number of bars to display"));
					}
					if (!isNumber(commands[2])) {
						return std::make_pair(instrumentExists, genError(commands[2] + " is not a number"));
					}
					sendNumBarsToPart(lastInstrumentIndex, stoi(commands[2]));
				}
				else if (commands[1].compare("accoffset") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"accoffset\" is a second level command, must be concatenated to instrument name with a dot"));
					}
					if (!sharedData.instruments[lastInstrumentIndex].sendToPart) {
						return std::make_pair(instrumentExists, genError("instrument doesn't have OSC set"));
					}
					if (commands.size() != 3) {
						return std::make_pair(instrumentExists, genError("\"accoffset\" command takes one argument, the offset for accidentals that are close together"));
					}
					if (!isFloat(commands[2])) {
						return std::make_pair(instrumentExists, genError(commands[2] + " is not a number"));
					}
					sendAccOffsetToPart(lastInstrumentIndex, std::stof(commands[2]));
					
				}
				else if (commands[1].compare("delay") == 0) {
					if (!hasDot) {
						return std::make_pair(instrumentExists, genError("\"delay\" is a second level command, must be concatenated to instrument name with a dot"));
					}
					if (commands.size() != 3) {
						return std::make_pair(instrumentExists, genError("\"delay\" command takes one argument, the delay to send messages in milliseconds"));
					}
					if (!isNumber(commands[2])) {
						return std::make_pair(instrumentExists, genError(commands[2] + " is not a number"));
					}
					int delayTime = stoi(commands[2]);
					sharedData.instruments[lastInstrumentIndex].setDelay((int64_t)delayTime);
				}
				else {
					if (parsingBars && barsIterCounter == 1 && !firstInstForBarsSet) {
						firstInstForBarsIndex = lastInstrumentIndex;
						firstInstForBarsSet = true;
					}
					std::pair<int, string> p = parseMelodicLine({commands.begin()+commandNdxOffset, commands.end()}, lineNum, numLines);
					if (p.first > 0) return std::make_pair(instrumentExists, genOutputFuncs(p));
					else return std::make_pair(instrumentExists, genOutput(p.second));
				}
			}
		}
	}
	return std::make_pair(instrumentExists, cmdOutput);
}

//--------------------------------------------------------------
std::pair<bool, CmdOutput> ofApp::isBarLoop(vector<string>& originalCommands, bool isMainCmd, int lineNum, int numLines)
{
	bool barLoopExists = false;
	CmdOutput cmdOutput = CmdOutput();
	if (sharedData.loopsIndexes.size() > 0) {
		string barLoopName;
		if (originalCommands[0].find(".") != string::npos) barLoopName = originalCommands[0].substr(0, originalCommands[0].find("."));
		else barLoopName = originalCommands[0];
		if (sharedData.loopsIndexes.find(barLoopName) != sharedData.loopsIndexes.end()) {
			barLoopExists = true;
			int indexLocal = sharedData.loopsIndexes[barLoopName];
			// the initial command is \barloopname which might be followed by a second level command
			// to separate the second level command we create a new vector that will copy the originalCommands vector
			// except from the first item where we trim the \barloopname. part
			bool hasDot = false;
			vector<string> commands;
			for (unsigned i = 0; i < originalCommands.size(); i++) {
				if (!i) {
					// separate the name of the bar/loop from the second level command
					vector<string> tokens = tokenizeString(originalCommands[i], ".");
					for (unsigned j = 0; j < tokens.size(); j++) {
						commands.push_back(tokens[j]);
					}
					if (tokens.size() > 1) hasDot = true;
				}
				else {
					commands.push_back(originalCommands[i]);
				}
			}
			if (commands.size() > 1) {
				if (!hasDot) {
					return std::make_pair(barLoopExists, genError("second level commands must be concatenated to the bar/loop name with a dot"));
				}
				if (commands.size() > 3) {
					return std::make_pair(barLoopExists, genError("if arguments are provided to a loop, these either \"locate [bar/loop-name]\", \"goto bar/loop-name/index\", or name of instrument"));
				}
				if (commands[1].compare("goto") != 0 && commands[1].compare("locate") != 0 && \
						sharedData.instrumentIndexes.find(commands[1]) == sharedData.instrumentIndexes.end()) {
					return std::make_pair(barLoopExists, genError("if arguments are provided to a loop, first must be \"goto\", \"locate\", or name of instrument"));
				}
				if (commands.size() < 3 && commands[1].compare("goto") == 0) {
					return std::make_pair(barLoopExists, genError("\"goto\" takes one argument, the bar/loop-name/index to go to"));
				}
				if (commands.size() == 3) {
					if (sharedData.instrumentIndexes.find(commands[1]) == sharedData.instrumentIndexes.end()) {
						return std::make_pair(barLoopExists, genError("a bar/loop with an instrument name as a second level command takes no arguments"));
					}
					auto it = find(sharedData.loopData[indexLocal].begin(), sharedData.loopData[indexLocal].end(), sharedData.loopsIndexes[commands[2]]);
					int gotoIndexOrBarName = 0;
					if (it == sharedData.loopData[indexLocal].end()) {
						if (isNumber(commands[2])) {
							size_t indexToGoto = stoi(commands[2]);
							if (indexToGoto < 1) {
								return std::make_pair(barLoopExists, genError("index to \"goto\" must be greater than 0"));
							} 
							if (indexToGoto <= sharedData.loopData[indexLocal].size()) {
								sharedData.thisLoopIndex = indexToGoto - 1; // argument is 1-based
								gotoIndexOrBarName = 1;
							}
							else {
								return std::make_pair(barLoopExists, genError("index to \"goto\" is greater than loop's size"));
							}
						}
						else {
							return std::make_pair(barLoopExists, genError(commands[2] + " doesn't exist in " + sharedData.loopsOrdered[indexLocal]));
						}
					}
					if (commands[1].compare("goto") == 0) {
						if (gotoIndexOrBarName == 0) sharedData.thisLoopIndex = it - sharedData.loopData[indexLocal].begin();
						ofxOscMessage m;
						m.setAddress("/thisloopndx");
						m.addIntArg(sharedData.thisLoopIndex);
						sendToParts(m, false);
						m.clear();
					}
					else {
						return std::make_pair(barLoopExists, genOutput(to_string(it - sharedData.loopData[indexLocal].begin() + 1)));
					}
				}
				else if (commands.size() == 2) {
					if (sharedData.instrumentIndexes.find(commands[1]) == sharedData.instrumentIndexes.end()) {
						return std::make_pair(barLoopExists, genError("unkown second level command to " + commands[0]));
					}
					// first check if this is a loop and not a bar
					if (sharedData.barsIndexes.find(barLoopName) == sharedData.barsIndexes.end()) {
						vector<string> v;
						for (int i : sharedData.loopData[sharedData.loopsIndexes[barLoopName]]) {
							v.push_back(sharedData.loopsOrdered.at(i) + "." + commands[1]);
							v.push_back("|");
						}
						// remove the last slash
						v.pop_back();
						return std::make_pair(barLoopExists, genOutput(v));
					}
					vector<string> barLinesTokens = tokenizeString(sharedData.barLines[barLoopName], "\n");
					for (auto it = barLinesTokens.begin(); it != barLinesTokens.end(); ++it) {
						vector<string> instBarLinesTokens = tokenizeString(*it, " ");
						size_t instNameNdx = 0;
						// bar definitions have their lines indented by a horizontal tab written as single white spaces
						// these white spaces have been left out when the string is tokenized
						// below we determine which token in the vector to look at, which must be the first after these spaces
						while (instBarLinesTokens.size() > 0 && instBarLinesTokens[instNameNdx].empty()) instNameNdx++;
						if (instBarLinesTokens.size() > 0 && instBarLinesTokens[instNameNdx] == commands[1]) {
							vector<string> v = {instBarLinesTokens.begin()+instNameNdx, instBarLinesTokens.end()};
							return std::make_pair(barLoopExists, genOutput(v));
						}
					}
					return std::make_pair(barLoopExists, genError("couldn't find " + commands[1] + " in " + barLoopName));
				}
			}
			else {
				if (isMainCmd) {
					// without any arguments and as a main command we're calling a loop to be played
					sharedData.tempBarLoopIndex = sharedData.loopsIndexes[commands[0]];
					if (sequencer.isThreadRunning()) {
						sequencer.update();
						mustUpdateScore = true;
						ofxOscMessage m;
						m.setAddress("/update");
						m.addIntArg(sharedData.tempBarLoopIndex);
						sendToParts(m, true);
						m.clear();
					}
					else {
						sharedData.loopIndex = sharedData.tempBarLoopIndex;
						sendLoopIndexToParts();
						// update the previous number of bars displayed and the previous position to properly display single bars in horizontal view
						sharedData.prevNumBars = sharedData.numBars;
						sharedData.prevPosition = sharedData.thisPosition;
					}
				}
				else  {
					vector<string> v;
					// first check if this is a loop and not a bar
					if (sharedData.barsIndexes.find(barLoopName) == sharedData.barsIndexes.end()) {
						for (int i : sharedData.loopData[sharedData.loopsIndexes[barLoopName]]) {
							v.push_back(sharedData.loopsOrdered.at(i));
							v.push_back("|");
						}
						// remove the last slash
						v.pop_back();
					}
					else {
						v = tokenizeString(sharedData.barLines[barLoopName], " ");
					}
					return std::make_pair(barLoopExists, genOutput(v));
				}
			}
		}
	}
	return std::make_pair(barLoopExists, cmdOutput);
}

//--------------------------------------------------------------
std::pair<bool, CmdOutput> ofApp::isFunction(vector<string>& commands, int lineNum, int numLines)
{
	bool functionExists = false;
	bool executeFunction = false;
	CmdOutput cmdOutput = CmdOutput();
	if (functionIndexes.size() > 0) {
		string functionName;
		if (commands[0].find(".") != string::npos) functionName = commands[0].substr(0, commands[0].find("."));
		else functionName = commands[0];
		if (functionIndexes.find(functionName) != functionIndexes.end()) {
			functionExists = true;
			executeFunction = true;
			lastFunctionIndex = functionIndexes[functionName];
			cmdOutput = functionFuncs(commands);
			if (cmdOutput.errorCode > 0) return std::make_pair(functionExists, cmdOutput);
			if (cmdOutput.outputVec[0].compare("noexec") == 0) executeFunction = false;
		}
	}
	if (functionExists && executeFunction) {
		vector<string> functionLines = tokenizeString(sharedData.functions[lastFunctionIndex].printStr(), "\n");
		size_t counter = 0;
		for (string line : functionLines) {
			// for some reason, the first line has an extra character, which we need to remove
			if (counter == 0) line = line.substr(0, line.size()-1);
			counter++;
			// detect $ args and replace them with actual args
			vector<int> argNdxs = findIndexesOfCharInStr(line, "$");
			size_t nextWhiteSpaceNdx;
			int dollarVal;
			unsigned i;
			string lineWithArgs = line;
			for (i = 0; i < argNdxs.size(); i++) {
				nextWhiteSpaceNdx = lineWithArgs.substr(argNdxs[i]).find(" ");
				if (nextWhiteSpaceNdx == string::npos) nextWhiteSpaceNdx = 2;
				if (isNumber(lineWithArgs.substr(argNdxs[i]+1, nextWhiteSpaceNdx-1))) {
					dollarVal = stoi(lineWithArgs.substr(argNdxs[i]+1, nextWhiteSpaceNdx-1));
					if (dollarVal <= sharedData.functions[lastFunctionIndex].getNumArgs()) {
						lineWithArgs = replaceCharInStr(lineWithArgs,
										lineWithArgs.substr(argNdxs[i], nextWhiteSpaceNdx),
										sharedData.functions[lastFunctionIndex].getArgument(dollarVal-1));
						for (unsigned j = i; j < argNdxs.size(); j++) {
							argNdxs[j] += (2 - (int)sharedData.functions[lastFunctionIndex].getArgument(dollarVal-1).size());
						}
					}
					else {
						if (i + 1 < commands.size()) {
							lineWithArgs = replaceCharInStr(lineWithArgs,
								lineWithArgs.substr(argNdxs[i], nextWhiteSpaceNdx),
								commands[i+1]);
						}
						else {
							lineWithArgs = replaceCharInStr(lineWithArgs,
								lineWithArgs.substr(argNdxs[i], nextWhiteSpaceNdx),
								"0");
						}
						for (unsigned j = i; j < argNdxs.size(); j++) {
							argNdxs[j]--;
						}
					}
				}
			}
			std::pair<int, string> p = parseString(lineWithArgs, lineNum, numLines);
			if (p.first == 3) { // on error, return
				return std::make_pair(functionExists, genError(p.second));
			}
		}
		int onUnbindFuncNdx = sharedData.functions[lastFunctionIndex].incrementCallCounter();
		if (onUnbindFuncNdx > -1) {
			CmdInput cmdInput = {{sharedData.functions[onUnbindFuncNdx].getName()}, vector<string>(), false, true};
			return std::make_pair(functionExists, parseCommand(cmdInput, 1, 1));
		}
	}
	return std::make_pair(functionExists, cmdOutput);
}

//--------------------------------------------------------------
std::pair<bool, CmdOutput> ofApp::isList(vector<string>& originalCommands, int lineNum, int numLines)
{
	bool listExists = false;
	CmdOutput cmdOutput = CmdOutput();
	if (listIndexes.size() > 0) {
		string listName;
		if (originalCommands[0].find(".") != string::npos) listName = originalCommands[0].substr(0, originalCommands[0].find("."));
		else listName = originalCommands[0];
		if (listIndexes.find(listName) != listIndexes.end()) {
			listExists = true;
			lastListIndex = listIndexes[listName];
			// the initial command is \listname which might be followed by a second level command
			// to separate the second level command we create a new vector that will copy the originalCommands vector
			// except from the first item where we trim the \listname. part
			bool hasDot = false;
			vector<string> commands;
			for (unsigned i = 0; i < originalCommands.size(); i++) {
				if (!i) {
					// separate the name of the list from the second level command
					vector<string> tokens = tokenizeString(originalCommands[i], ".");
					for (unsigned j = 0; j < tokens.size(); j++) {
						commands.push_back(tokens[j]);
					}
					if (tokens.size() > 1) hasDot = true;
				}
				else {
					commands.push_back(originalCommands[i]);
				}
			}
			if (commands.size() < 2) {
				return std::make_pair(listExists, cmdOutput);
			}
			if (commands[1].compare("traverse") == 0) {
				if (!hasDot) {
					return std::make_pair(listExists, genError("second level commands must be concatenated to the list name with a dot"));
				}
				traversingList = true;
				listIndexCounter = 0;
				CmdInput cmdInput = {{commands.begin()+2, commands.end()}, vector<string>(), false, true};
				cmdOutput = traverseList(genStrFromVec(cmdInput.inputVec), lineNum, numLines);
				if (cmdOutput.errorCode == 3) {
					traversingList = false;
					return std::make_pair(listExists, cmdOutput);
				}
			}
			else if (isNumber(commands[1])) {
				std::pair<int, string> p = listItemExists(stoi(commands[1]));
				if (p.first > 0) return std::make_pair(listExists, genOutputFuncs(p));
				else return std::make_pair(listExists, genOutput(""));
			}
			else {
				vector<string> v = {commands.begin()+1, commands.end()};
				cmdOutput = parseCommand(genCmdInput(v), lineNum, numLines);
				if (cmdOutput.errorCode > 0) {
					return std::make_pair(listExists, cmdOutput);
				}
				else {
					if (!isNumber(cmdOutput.outputVec[0])) {
						return std::make_pair(listExists, genError("list can only be indexed with an integer"));
					}
					size_t listItemNdx = stoi(cmdOutput.outputVec[0]);
					std::pair<int, string> p = listItemExists(listItemNdx);
					if (p.first > 0) return std::make_pair(listExists, genOutputFuncs(p));
					else return std::make_pair(listExists, genOutput(""));
				}
			}
		}
	}
	return std::make_pair(listExists, cmdOutput);
}

//--------------------------------------------------------------
std::pair<int, string> ofApp::listItemExists(size_t listItemNdx)
{
	if (listItemNdx < 1) {
		return std::make_pair(3, "lists are 1-base indexed");
	}
	if (listItemNdx > listMap[lastListIndex].size()) {
		return std::make_pair(3, "list index out of range");
	}
	listItemNdx--; // LiveLily is 1-based counting
	auto lit = listMap[lastListIndex].begin();
	std::advance(lit, listItemNdx);
	return std::make_pair(0, *lit);
}

//--------------------------------------------------------------
std::pair<bool, CmdOutput> ofApp::isOscClient(vector<string>& originalCommands, int lineNum, int numLines)
{
	bool oscClientExists = false;
	CmdOutput cmdOutput = CmdOutput();
	if (oscClients.size() > 0) {
		string oscClientName;
		if (originalCommands[0].find(".") != string::npos) oscClientName = originalCommands[0].substr(0, originalCommands[0].find("."));
		else oscClientName = originalCommands[0];
		if (oscClients.find(oscClientName) != oscClients.end()) {
			oscClientExists = true;
			// the initial command is \oscclientname which might be followed by a second level command
			// to separate the second level command we create a new vector that will copy the originalCommands vector
			// except from the first item where we trim the \oscclientname. part
			bool hasDot = false;
			vector<string> commands;
			for (unsigned i = 0; i < originalCommands.size(); i++) {
				if (!i) {
					// separate the name of the OSC client from the second level command
					vector<string> tokens = tokenizeString(originalCommands[i], ".");
					for (unsigned j = 0; j < tokens.size(); j++) {
						commands.push_back(tokens[j]);
					}
					if (tokens.size() > 1) hasDot = true;
				}
				else {
					commands.push_back(originalCommands[i]);
				}
			}
			if (!hasDot) {
				return std::make_pair(oscClientExists, genError("OSC client name and second level command must be concatenated with a dot"));
			}
			if (commands.size() < 2) {
				return std::make_pair(oscClientExists, genWarning("no second level commands provided"));
			}
			for (unsigned i = 1; i < commands.size(); i++) {
				if (commands[i].compare("setup") == 0) {
					if (i > 1) {
						return std::make_pair(oscClientExists, genError("\"setup\" can't be combined with other commands"));
					}
					if (commands.size() > i+3) {
						return std::make_pair(oscClientExists, genError("\"setup\" takes two arguments maximum, host IP and port"));
					}
					string host;
					int port;
					if (commands.size() > i+2) {
						if (isNumber(commands[i+1]) || !isNumber(commands[i+2])) {
							return std::make_pair(oscClientExists, genError("when two arguments are provided to \"setup\", the first must be the IP address"));
						}
						host = commands[i+1];
						port = stoi(commands[i+2]);
					}
					else {
						if (isNumber(commands[i+1])) {
							host = "localhost";
							port = stoi(commands[i+1]);
						}
						else {
							return std::make_pair(oscClientExists, genError("if one argument is provided to \"setup\", this must be the port number"));
						}
					}
					oscClients[commands[0]].setup(host, port);
					return std::make_pair(oscClientExists, cmdOutput);
				}
				else if (commands[i].compare("send") == 0) {
					if (i > 1) {
						return std::make_pair(oscClientExists, genError("\"send\" can't be combined with other commands"));
					}
					if (commands.size() < i+1) {
						return std::make_pair(oscClientExists, genWarning("no data sepcified, not sending message"));
					}
					if (startsWith(commands[i+1], "/")) {
						if (commands.size() < i+2) {
							return std::make_pair(oscClientExists, genWarning("no data sepcified, not sending message"));
						}
						ofxOscMessage m;
						m.setAddress(commands[i+1]);
						// create an empty string to concatenate all vector items that are not numbers
						// but are not defined as strings either, in case they are missing quotes
						string oscStr = "";
						for (unsigned j = i+2; j < commands.size(); j++) {
							if (isNumber(commands[j])) {
								// when encountering a number item in the arguments vector
								// check if the string is not empty, and if that's true
								// store the number as a string argument instead of an int
								if (!oscStr.empty()) {
									oscStr += commands[j];
								}
								else {
									m.addIntArg(stoi(commands[j]));
								}
							}
							else if (isFloat(commands[j])) {
								// the same applies for floats
								if (!oscStr.empty()) {
									oscStr += commands[j];
								}
								else {
									m.addFloatArg(stof(commands[j]));
								}
							}
							else if (startsWith(commands[j], "\"") && endsWith(commands[j], "\"")) {
								// is we get a string with quotes, store the oscStr and clear it
								// and then store the quoted string separatelly
								if (!oscStr.empty()) {
									m.addStringArg(oscStr);
									oscStr.clear();
								}
								m.addStringArg(commands[j].substr(1, commands[j].size()-2));
							}
							else {
								if (!oscStr.empty()) oscStr += " ";
								oscStr += commands[j];
							}
						}
						// after running through all vector items, check if the string has been populated
						// if it has, store it
						if (!oscStr.empty()) m.addStringArg(oscStr);
						oscClients[commands[0]].sendMessage(m, false);
						m.clear();
					}
					else {
						return std::make_pair(oscClientExists, genError("no OSC address specified, must begin with \"/\""));
					}
				}
			}
		}
	}
	return std::make_pair(oscClientExists, cmdOutput);
}

//--------------------------------------------------------------
std::pair<bool, CmdOutput> ofApp::isGroup(vector<string>& originalCommands, int lineNum, int numLines)
{
	bool groupExists = false;
	CmdOutput cmdOutput = CmdOutput();
	if (instGroups.size() > 0) {
		string groupName;
		if (originalCommands[0].find(".") != string::npos) groupName = originalCommands[0].substr(0, originalCommands[0].find("."));
		else groupName = originalCommands[0];
		if (instGroups.find(groupName) != instGroups.end()) {
			groupExists = true;
			// the initial command is \grouptname which might be followed by a second level command
			// to separate the second level command we create a new vector that will copy the originalCommands vector
			// except from the first item where we trim the \groupname. part
			bool hasDot = false;
			vector<string> commands;
			for (unsigned i = 0; i < originalCommands.size(); i++) {
				if (!i) {
					// separate the name of the group from the second level command
					vector<string> tokens = tokenizeString(originalCommands[i], ".");
					for (unsigned j = 0; j < tokens.size(); j++) {
						commands.push_back(tokens[j]);
					}
					if (tokens.size() > 1) hasDot = true;
				}
				else {
					commands.push_back(originalCommands[i]);
				}
			}
			if (commands.size() == 1) {
				return std::make_pair(groupExists, genWarning("group called with no second level commands or arguments"));
			}
			for (string inst : instGroups[groupName]) {
				size_t cmdOffset = 1;
				vector<string> v;
				if (hasDot) {
					v.push_back(inst + "." + commands[1]);
					cmdOffset = 2;
				}
				else {
					v.push_back(inst);
				}
				if (cmdOffset < commands.size()) {
					v.insert(v.end(), commands.begin()+cmdOffset, commands.end());
				}
				std::pair<bool, CmdOutput> p = isInstrument(v, lineNum, numLines);
				if (p.second.errorCode == 3) {
					return p;
				}
				if (p.second.errorCode > cmdOutput.errorCode) {
					cmdOutput.errorCode = p.second.errorCode;
					cmdOutput.errorStr = p.second.errorStr;
				}
			}
		}
	}
	return std::make_pair(groupExists, cmdOutput);
}

//--------------------------------------------------------------
CmdOutput ofApp::functionFuncs(vector<string>& originalCommands)
{
	// the initial command is \functionname which might be followed by a second level command
	// to separate the second level command we create a new vector that will copy the originalCommands vector
	// except from the first item where we trim the \functionname. part
	bool hasDot = false;
	CmdOutput cmdOutput = CmdOutput();
	vector<string> commands;
	for (unsigned i = 0; i < originalCommands.size(); i++) {
		if (!i) {
			// separate the name of the function from a possible second level command
			vector<string> tokens = tokenizeString(originalCommands[i], ".");
			for (unsigned j = 0; j < tokens.size(); j++) {
				commands.push_back(tokens[j]);
			}
			if (tokens.size() > 1) hasDot = true;
		}
		else {
			commands.push_back(originalCommands[i]);
		}
	}
	if (!hasDot) return genError("function name and second level command must be concatenated with a dot");
	for (unsigned i = 1; i < commands.size(); i++) {
		if (commands[i].compare("setargs") == 0) {
			sharedData.functions[lastFunctionIndex].resetArgumentIndex();
			for (unsigned j = i+1; j < commands.size(); j++) {
				sharedData.functions[lastFunctionIndex].setArgument(commands[j]);
				if (sharedData.functions[lastFunctionIndex].getArgError() > 0) {
					switch (sharedData.functions[lastFunctionIndex].getArgError()) {
						case 1:
							return genError("too many arguments");
						case 2:
							return genError("argument size too big");
						default:
							return genError("something went wrong");
					}
				}
			}
			return genOutput("noexec");
		}
		else if (commands[i].compare("bind") == 0) {
			if (commands.size() < i+2) {
				return genError("\\bind command takes at least one argument");
			}
			if (sharedData.instrumentIndexes.find(commands[i+1]) == sharedData.instrumentIndexes.end() && \
				!(commands[i+1].compare("beat") == 0 || commands[i+1].compare("barstart") == 0 || \
				  commands[i+1].compare("framerate") == 0 || commands[i+1].compare("fr") == 0 || \
				  commands[i+1].compare("loopstart") == 0)) {
				return genError(commands[i+1] + (string)" is not a defined instrument");
			}
			int instNdx = -1;
			int callingStep = 1;
			int everyOtherStep = 0;
			int numTimes = 0;
			bool callingStepSet = false;
			bool settingEvery = false;
			bool settingTimes = false;
			bool sendValWarning = false;
			if (commands.size() >= i+2) {
				if (commands[i+1].compare("beat") == 0) {
					instNdx = (int)sharedData.instruments.size();
				}
				else if (commands[i+1].compare("barstart") == 0) {
					instNdx = (int)sharedData.instruments.size() + 1;
				}
				else if (commands[i+1].compare("framerate") == 0 || commands[i+1].compare("fr") == 0) {
					instNdx = (int)sharedData.instruments.size() + 2;
				}
				else if (commands[i+1].compare("loopstart") == 0) {
					instNdx = (int)sharedData.instruments.size() + 3;
				}
				// if only a function and an instrument are defined
				// then we trigger the function at every step
				else if (commands.size() == i+2){
					instNdx = sharedData.instrumentIndexes[commands[i+1]];
					everyOtherStep = 1;
				}
			}
			if (commands.size() > i+2) {
				for (unsigned j = i+2; j < commands.size(); j++) {
					if (isNumber(commands[j])) {
						int val = stoi(commands[j]);
						if (val < 0) return genError("value can't be negative");
						if (val == 0) sendValWarning = true;
						if (settingEvery) {
							everyOtherStep = val;
							settingEvery = false;
						}
						else if (settingTimes) {
							numTimes = val;
							settingTimes = false;
						}
						else {
							callingStep = val;
							callingStepSet = true;
						}
					}
					else if (commands[j].compare("every") == 0) {
						if (commands.size() < j + 2) {
							return genError("argument \"every\" to \"bind\" takes a number after it");
						}
						if (!isNumber(commands[j+1])) {
							return genError("argument \"every\" to \"bind\" takes a number after it");
						}
						if (!callingStepSet) callingStep = 1;
						settingEvery = true;
					}
					else if (commands[j].compare("times") == 0) {
						if (commands.size() < j + 2) {
							return genError("argument \"times\" to \"bind\" takes a number after it");
						}
						if (!isNumber(commands[j+1])) {
							return genError("argument \"times\" to \"bind\" takes a number after it");
						}
						if (!callingStepSet) callingStep = 1;
						settingTimes = true;
					}
					else {
						return genError(commands[j] + ": unkonw argument");
					}
				}
			}
			sharedData.functions[lastFunctionIndex].setBind(instNdx, callingStep, everyOtherStep, numTimes);
			if (sendValWarning) {
				return genWarning("starting step must be 1 or greater, setting to 1");
			}
			else return genOutput("noexec");
		}
		else if (commands[i].compare("unbind") == 0) {
			if (commands.size() > i+1) {
				return genError("\"unbind\" command takes no arguments");
			}
			sharedData.functions[lastFunctionIndex].releaseBind();
			return genOutput("");
		}
		else if (commands[i].compare("onrelease") == 0) {
			if (commands.size() < i+2) {
				return genError("\"onrelease\" takes at least one argument");
			}
			if (isNumber(commands[i+1])) {
				sharedData.functions[lastFunctionIndex].onUnbindFunc(stoi(commands[i+1]));
			} 
			else if (commands[i+1].compare("release") == 0) {
				sharedData.functions[lastFunctionIndex].onUnbindFunc(-1);
			}
			else if (functionIndexes.find(commands[i+1]) == functionIndexes.end()) {
				// read the line and store a function with it
			}
			else {
				sharedData.functions[lastFunctionIndex].onUnbindFunc(functionIndexes[commands[i+1]]);
			}
			return genOutput("noexec");
		}
	}
	return cmdOutput;
}

//--------------------------------------------------------------
string ofApp::getRestOfCommand(vector<string>& commands, unsigned startNdx)
{
	string restOfCommand;
	for (unsigned i = startNdx; i < commands.size(); i++) {
		restOfCommand += commands[i];
		if (i < commands.size() - 1) restOfCommand += " ";
	}
	return restOfCommand;
}

//--------------------------------------------------------------
int ofApp::getLastLoopIndex()
{
	int loopIndex = -1;
	// since every bar is copied to the loop maps, but loops are not copied to the bar map
	// we query the loopsIndexes map instead of the barIndexes map
	// so that the keys between bars and loops are the same
	if (sharedData.loopsOrdered.size() > 0) {
		map<int, string>::reverse_iterator it = sharedData.loopsOrdered.rbegin();
		loopIndex = it->first;
	}
	return loopIndex;
}

//--------------------------------------------------------------
int ofApp::getLastBarIndex()
{
	int barIndex = -1;
	int lastLoopIndex = getLastLoopIndex();
	// since loop indexes are not stored as bar indexes, once we get the last loop index
	// we query whether this is also a bar index, and if not, we subtract one until we find a bar index
	if (lastLoopIndex > 0) {
		while (sharedData.barsIndexes.find(sharedData.loopsOrdered[lastLoopIndex]) == sharedData.barsIndexes.end()) {
			lastLoopIndex--;
		}
		barIndex = lastLoopIndex;
	}
	return barIndex;
}

//--------------------------------------------------------------
int ofApp::getPrevBarIndex()
{
	int prevBarIndex = getLastBarIndex();
	prevBarIndex--;
	if (prevBarIndex > 0) {
		while (sharedData.barsIndexes.find(sharedData.loopsOrdered[prevBarIndex]) == sharedData.barsIndexes.end()) {
			prevBarIndex--;
		}
	}
	return prevBarIndex;
}

//--------------------------------------------------------------
int ofApp::getPlayingBarIndex()
{
	if (sharedData.loopData.find(sharedData.loopIndex) == sharedData.loopData.end()) {
		return -1;
	}
	if (sharedData.thisLoopIndex >= sharedData.loopData[sharedData.loopIndex].size()) {
		return -1;
	}
	return sharedData.loopData[sharedData.loopIndex][sharedData.thisLoopIndex];
}

//--------------------------------------------------------------
std::pair<int, string> ofApp::stripLineFromBar(string str)
{
	auto barToCopyIt = sharedData.loopsIndexes.find(str);
	int barIndex = getLastLoopIndex();
	if (barToCopyIt == sharedData.loopsIndexes.end()) {
		return std::make_pair(3, (string)"bar/loop " + str + (string)" doesn't exist");
	}
	barToCopy = barToCopyIt->second;
	//sharedData.numBeats[barIndex] = sharedData.numBeats[barToCopy];
	sharedData.instruments[lastInstrumentIndex].setPassed(true);
	sharedData.instruments[lastInstrumentIndex].setCopied(barIndex, true);
	sharedData.instruments[lastInstrumentIndex].setCopyNdx(barIndex, barToCopy);
	sharedData.instruments[lastInstrumentIndex].copyMelodicLine(barIndex);
	return std::make_pair(0, "");
}

//--------------------------------------------------------------
void ofApp::deleteLastBar()
{
	//if (barError) return; // if a bar error has been raised, nothing has been stored
	int bar = getLastBarIndex();
	for (map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		if (it->second.hasPassed() && it->second.notes.find(bar) != it->second.notes.end()) {
			it->second.notes.erase(bar);
			it->second.midiNotes.erase(bar);
			it->second.durs.erase(bar);
			it->second.dursWithoutSlurs.erase(bar);
			it->second.midiDursWithoutSlurs.erase(bar);
			it->second.pitchBendVals.erase(bar);
			it->second.dynamics.erase(bar);
			it->second.midiVels.erase(bar);
			it->second.dynamicsRamps.erase(bar);
			it->second.glissandi.erase(bar);
			it->second.midiGlissDurs.erase(bar);
			it->second.midiDynamicsRampDurs.erase(bar);
			it->second.articulations.erase(bar);
			it->second.midiArticulationVals.erase(bar);
			it->second.isSlurred.erase(bar);
			it->second.text.erase(bar);
			it->second.textIndexes.erase(bar);
			it->second.slurIndexes.erase(bar);
			it->second.isWholeBarSlurred.erase(bar);
			it->second.tieIndexes.erase(bar);
			it->second.scoreNotes.erase(bar);
			it->second.scoreDurs.erase(bar);
			it->second.scoreDotIndexes.erase(bar);
			it->second.scoreDotsCounter.erase(bar);
			it->second.scoreAccidentals.erase(bar);
			it->second.scoreNaturalSignsNotWritten.erase(bar);
			it->second.scoreOctaves.erase(bar);
			it->second.scoreOttavas.erase(bar);
			it->second.scoreGlissandi.erase(bar);
			it->second.scoreArticulations.erase(bar);
			it->second.scoreDynamics.erase(bar);
			it->second.scoreDynamicsIndexes.erase(bar);
			it->second.scoreDynamicsRampStart.erase(bar);
			it->second.scoreDynamicsRampEnd.erase(bar);
			it->second.scoreDynamicsRampDir.erase(bar);
			it->second.scoreTupletRatios.erase(bar);
			it->second.scoreTupletStartStop.erase(bar);
			it->second.scoreTexts.erase(bar);
			it->second.passed = false;
			it->second.deleteNotesBar(bar);
		}
	}
	if (sharedData.barsIndexes.find(sharedData.loopsOrdered[bar]) != sharedData.barsIndexes.end()) {
		sharedData.barsIndexes.erase(sharedData.loopsOrdered[bar]);
	}
	if (sharedData.loopsIndexes.find(sharedData.loopsOrdered[bar]) != sharedData.loopsIndexes.end()) {
		sharedData.loopsIndexes.erase(sharedData.loopsOrdered[bar]);
	}
	if (sharedData.numerator.find(bar) != sharedData.numerator.end()) {
		sharedData.numerator.erase(bar);
		sharedData.denominator.erase(bar);
	}
	commandsMap[livelily].erase(sharedData.loopsOrdered[bar]);
	sharedData.loopsOrdered.erase(bar);
	sharedData.loopsVariants.erase(bar);
	// sharedData.barLines is a map<string, string> so we get the correct key
	// from the sharedData.barsOrdered map using the bar index as the key
	sharedData.barLines.erase(sharedData.barsOrdered[bar]);
}

//--------------------------------------------------------------
void ofApp::deleteLastLoop()
{
	map<int, string>::reverse_iterator it = sharedData.loopsOrdered.rbegin();
	commandsMap[livelily].erase(it->second);
	sharedData.loopsIndexes.erase(it->second);
	sharedData.loopsOrdered.erase(it->first);
	sharedData.loopsVariants.erase(it->first);
	//sendPushPopPattern();
}

//--------------------------------------------------------------
std::pair<int, string> ofApp::parseBarLoop(string str, int lineNum, int numLines)
{
	// first check if there is anything after the closing bracket
	size_t closingBracketIndex = str.find("}");
	if (str.substr(closingBracketIndex-1, 1).compare(" ") == 0) {
		str = str.substr(0, closingBracketIndex-1) + str.substr(closingBracketIndex);
	}
	size_t strLength = str.size();
	string restOfCommand = "";
	// storing anything after a closing bracket enables us
	// to call a loop in the same line we define it, like this
	// \loop name {\bar1 \bar2} \name
	if (closingBracketIndex != string::npos) {
		restOfCommand = str.substr(closingBracketIndex);
		strLength = closingBracketIndex;
	}
	string wildCardStr = (endsWith(str, "}") ? str.substr(0, str.size()-1) : str);
	// if we get only an asterisk, it means we want to create a loop with all the bars
	if (wildCardStr.compare("*") == 0) {
		vector<int> thisLoopIndexes;
		for (auto it = sharedData.barsOrdered.begin(); it != sharedData.barsOrdered.end(); ++it) {
			// the first, default bar (a tacet) has index 0, which we must ignore here
			if (it->first > 0) thisLoopIndexes.push_back(it->first);
		}
		// find the last index of the stored loops and store the vector we just created to the value of loopData
		int loopNdx = getLastLoopIndex();
		sharedData.loopData[loopNdx] = thisLoopIndexes;
		return std::make_pair(0, "");
	}
	size_t multIndex = wildCardStr.find("*");
	vector<string> names;
	if (multIndex != string::npos) {
		if (!isNumber(wildCardStr.substr(multIndex+1))) {
			// look for file names that include the characters of the wild card
			vector<int> multNdxs = findIndexesOfCharInStr(wildCardStr, "*");
			vector<string> strParts(multNdxs.size(), "");
			int prevMultNdx = 0;
			for (unsigned i = 0; i < multNdxs.size(); i++) {
				if (wildCardStr.substr(prevMultNdx, multNdxs[i]-prevMultNdx).size() > 0) {
					strParts[i] = wildCardStr.substr(prevMultNdx, multNdxs[i]-prevMultNdx);
				}
				prevMultNdx = multNdxs[i]+1;
			}
			if (wildCardStr.substr(multNdxs.back()+1).size() > 0) {
				strParts.back() = wildCardStr.substr(multNdxs.back()+1);
			}
			for (auto it = sharedData.loopsOrdered.begin(); it != sharedData.loopsOrdered.end(); ++it) {
				if (sharedData.barsIndexes.find(it->second) == sharedData.barsIndexes.end()) {
					// look for bars only, not for loops
					continue;
				}
				bool matches = true;
				for (auto it2 = strParts.begin(); it2 != strParts.end(); ++it2) {
					// in case the pattern doesn't match
					if (it->second.find(*it2) == string::npos) {
						matches = false;
						break;
					}
					// a wildcard might include a 1, which will result in including the default first bar
					// so we make sure not to include it with the test below
					if (it->second.compare("\\-1") == 0) {
						matches = false;
						break;
					}
				}
				if (matches) {
					names.push_back(it->second);
				}
			}
		}
		else {
			names = tokenizeString(str.substr(0, strLength), " ");
		}
	}
	else {
		names = tokenizeString(str.substr(0, strLength), " ");
	}
	//vector<string> names = tokenizeString(str.substr(0, strLength), " ");
	// initialize all variables used in the loop below, so we can initialize the growing vector
	// after them, to ensure it is at the top of the stack
	//size_t multIndex;
	size_t nameLength;
	int howManyTimes;
	map<string, int>::iterator thisBarLoopIndexIt;
	map<int, vector<int>>::iterator thisBarLoopDataIt;
	vector<int>::iterator barIndex;
	int i;
	vector<int> thisLoopIndexes;
	for (string name : names) {
		nameLength = name.size();
		multIndex = name.find("*");
		howManyTimes = 1;
		if (multIndex != string::npos) {
			nameLength = multIndex;
			if (!isNumber(name.substr(multIndex+1))) {
				return std::make_pair(3, "repetition coefficient not an int");
			}
			else if (multIndex == 0) {
				// the multiplication symbol cannot be the first symbol in the token
				return std::make_pair(3, "the multiplication character must be concatenated to the bar/loop name");
			}
			else if (multIndex == name.size()-1) {
				// the multiplication symbol must be followed by a number
				return std::make_pair(3, "the multiplication symbol must be concatenated to a number");
			}
			howManyTimes = stoi(name.substr(multIndex+1));
		}
		// first check for barLoops because when we define a bar with data
		// and not with combinations of other bars, we create a barLoop with the same name
		thisBarLoopIndexIt = sharedData.loopsIndexes.find(name.substr(0, nameLength));
		if (thisBarLoopIndexIt == sharedData.loopsIndexes.end()) {
			// check if the substring ends with a closing curly bracket, in which case we must remove it
			if (endsWith(name.substr(0, nameLength), "}")) {
				thisBarLoopIndexIt = sharedData.loopsIndexes.find(name.substr(0, nameLength-1));
			}
			else {
				return std::make_pair(3, name.substr(0, nameLength) + (string)" doesn't exist");
			}
		}
		// find the map of the loop with the vector that contains indexes of bars
		thisBarLoopDataIt = sharedData.loopData.find(thisBarLoopIndexIt->second);
		// then iterate over the number of repetitions of this loop
		for (i = 0; i < howManyTimes; i++) {
			// and push back every item in the vector that contains the indexes of bars
			for (barIndex = thisBarLoopDataIt->second.begin(); barIndex != thisBarLoopDataIt->second.end(); ++barIndex) {
				thisLoopIndexes.push_back(*barIndex);
			}
		}
	}
	// once all the bar indexes have been pushed, we check if any of the bars in this loop is linked
	// to a bar that is not sequential within the loop we're trying to create
	for (i = 0; i < (int)thisLoopIndexes.size(); i++) {
		for (auto instIt = sharedData.instruments.begin(); instIt != sharedData.instruments.end(); ++instIt) {
			std::pair<int, int> barLinks = instIt->second.isLinked(thisLoopIndexes[i]);
			if (barLinks.first < 0 || barLinks.second > 0) {
				// if we find a link, first determine whether the linked bar(s) is not present in the loop
				if (abs(barLinks.first) > i) goto linkError;
				if (barLinks.second > (int)thisLoopIndexes.size() - i - 1) goto linkError;
				for (int j = i; j > i-abs(barLinks.first); j--) {
					if (thisLoopIndexes[j-1] > thisLoopIndexes[j]) goto linkError;
					if (thisLoopIndexes[j] - thisLoopIndexes[j-1] > 1) goto linkError;
				}
				if (i+barLinks.second > (int)thisLoopIndexes.size()) goto linkError;
				for (int j = i; j < i + barLinks.second; j++) {
					if (thisLoopIndexes[j] > thisLoopIndexes[j+1]) goto linkError;
					if (thisLoopIndexes[j+1] - thisLoopIndexes[j] > 1) goto linkError;
				}
				continue; // in case goto is not invoked
			linkError:
				int linkLevel = max(abs(barLinks.first), barLinks.second);
				string err = (string)"bar " + sharedData.loopsOrdered[thisLoopIndexes[i]] + " links ";
				if (linkLevel > 1) {
					err += "through ";
				}
				err += "to ";
				if (abs(barLinks.first) > barLinks.second) {
					err += sharedData.loopsOrdered[thisLoopIndexes[i]-linkLevel];
				}
				else {
					err += sharedData.loopsOrdered[thisLoopIndexes[i]+linkLevel];
				}
				return std::make_pair(3, err);
			}
		}
	}
	// find the last index of the stored loops and store the vector we just created to the value of loopData
	int loopNdx = getLastLoopIndex();
	sharedData.loopData[loopNdx] = thisLoopIndexes;
	if (restOfCommand.size() > 0) {
		// falsify this so that the name of the loop is treated properly in parseString()
		// otherwise, parseString() will again call this function
		parsingLoop = false;
		parseString(restOfCommand, lineNum, numLines);
	}
	if (!sequencer.isThreadRunning()) {
		sharedData.loopIndex = sharedData.tempBarLoopIndex;
		//sendLoopIndexToParts();
	}
	sendLoopToParts();
	return std::make_pair(0, "");
}

//--------------------------------------------------------------
vector<string> ofApp::tokenizeChord(string input, bool includeAngleBrackets)
{
	bool isString = false;
	int quotesCounter = 0;
	vector<string> tokens;
	string token;
	for (size_t i = 0; i < input.size(); i++) {
		if (isspace(input[i])) {
			if (!token.empty() && !isString) {
				tokens.push_back(token);
				token.clear();
			}
			else {
				token += input[i];
			}
		}
		// we continue when we run into the chord opening and closing characters
		// so that they are not concatenated to the token
		else if (input[i] == '<' && ((i < input.size()-1 && !isspace(input[i+1])) || (i > 0 && input[i-1] != '\\'))) {
			if (includeAngleBrackets) token += input[i];
			continue;
		}
		else if (input[i] == '>' && (i > 0 && input[i-1] != '\\' && input[i-1] != '-')) {
			if (includeAngleBrackets) token += input[i];
			continue;
		}
		else {
			token += input[i];
			if (input[i] == '"') {
				quotesCounter++;
				if (quotesCounter == 1) {
					isString = true;
				}
				else {
					isString = false;
					quotesCounter = 0;
				}
			}
		}
	}
	if (!token.empty()) tokens.push_back(token);
	return tokens;
}

//--------------------------------------------------------------
std::pair<int, string> ofApp::parseMelodicLine(vector<string> tokens, int lineNum, int numLines)
{
	if (barError) {
		// if a barError occurs, it concerns the line where the bar name is defined
		// in this case, we don't proceed to parse the melodic lines defined in the bar
		// but we also don't raise an error here, as this has been raised in parseCommand()
		return std::make_pair(0, "");
	}
	if (!parsingBar) {
		return std::make_pair(3, "stray melodic line");
	}
	//if (!areBracketsBalanced(str)) {
	//	return std::make_pair(3, "brackets are not balanced");
	//}
	//if (!areParenthesesBalanced(str)) {
	//	return std::make_pair(3, "parentheses are not balanced");
	//}
	//bool parsingBarsFromLoop = false;
	//int ndx;
	//string parsingBarsFromLoopName;
	//if (parsingBars && startsWith(str, "\\") && str.find("|") == string::npos) {
	//	if (sharedData.barsIndexes.find(str) == sharedData.barsIndexes.end()) {
	//		if (sharedData.loopsIndexes.find(str) != sharedData.loopsIndexes.end()) {
	//			parsingBarsFromLoop = true;
	//			parsingBarsFromLoopName = str;
	//		}
	//		else {
	//			return std::make_pair(3, (string)"bar/loop " + str + (string)" doesn't exist");
	//		}
	//	}
	//}
	//size_t strBegin, strLen;
	//string strToProcess = detectRepetitions(str);
	tokens = detectRepetitions(tokens);
	if (parsingBars) {
		if (startsWith(tokens[0], "\\")) {
			// first determine whether there is an instrument name as a second level command
			size_t dotNdx = tokens[0].find_last_of(".");
			bool foundDot = (dotNdx != string::npos ? true : false);
			string barName;
			if (foundDot) barName = tokens[0].substr(0, dotNdx);
			else barName = tokens[0];
			// determine if this is a single bar or a loop
			// if it is a loop, replace the tokens vector with the output of parseExpandedCommands()
			if (sharedData.loopsIndexes.find(barName) != sharedData.loopsIndexes.end() && sharedData.barsIndexes.find(barName) == sharedData.barsIndexes.end()) {
				CmdInput cmdInput = {tokens, vector<string>(), false, false};
				CmdOutput cmdOutput = parseCommand(cmdInput, lineNum, numLines);
				if (cmdOutput.errorCode == 3) return std::make_pair(3, cmdOutput.errorStr);
				tokens.clear();
				tokens = cmdOutput.outputVec;
			}
		}
		if (lastInstrumentIndex == firstInstForBarsIndex && !numBarsToParseSet) {
			// if this is the first instrument, determine the number of bars to compare to other instruments
			numBarsToParse = 0;
			for (string s : tokens) if (s == "|") numBarsToParse++;
			numBarsToParse++;
			// check in case the user ends bars definition with |
			if (tokens.back() == "|") numBarsToParse--;
			numBarsToParseSet = true;
		}
		int numBarsToParseLocal = 0;
		for (string s : tokens) if (s == "|") numBarsToParseLocal++;
		numBarsToParseLocal++; // have to double check in case the user ends bars definition with |
		if (lastInstrumentIndex != firstInstForBarsIndex && numBarsToParseLocal != numBarsToParse) {
			return std::make_pair(3, (string)"parsed " + to_string(numBarsToParseLocal) + (string)" bars, while first instrument parsed " + to_string(numBarsToParse));
		}
		// the int below is compared to barsIterCounter which increments to 1 as soon as we start parsing many bars
		int verticalBarsCounter = 1;
		vector<string> newTokens;
		for (string s : tokens) {
			if (s == "|") verticalBarsCounter++;
			if (verticalBarsCounter == barsIterCounter && s != "|") {
				newTokens.push_back(s);
			}
			if (verticalBarsCounter > barsIterCounter) {
				// reset verticalBarsCounter so that the test agaist numBarsToParse below works
				// otherwise it will be true after the first iteration
				verticalBarsCounter = barsIterCounter;
				break;
			}
		}
		if (verticalBarsCounter == numBarsToParse) sharedData.instruments[lastInstrumentIndex].setMultiBarsDone(true);
		tokens.clear();
		tokens = newTokens;
	}
	// if we are parsing many bars and in one of those we define an earlier bar from the same bars definition
	// we need to isolate the bar number from the loop name, which is loopname-barnumber (e.g. \1-4-1 for the first bar from loop \1-4)
	// so we can use it further down in the else if (tokens[0] == "\\bars") test
	int verticalBarsNdx = 1;
	// as we might ask for a previous bar of the same bars definition but from another instrument
	// we need to detect if this is the case and store the name of the desired instrument
	// the default instrument name though is the one we're parsing the current line for
	string instFromBars = "\\" + sharedData.instruments[lastInstrumentIndex].getName();
	if (parsingBars && startsWith(tokens[0], "\\")) {
		size_t lastHyphenNdx = tokens[0].find_last_of("-");
		size_t dotNdx = tokens[0].find_last_of(".");
		bool foundDot = (dotNdx != string::npos ? true : false);
		size_t endOfSubstr = (foundDot ? dotNdx : tokens[0].size());
		string barName;
		if (foundDot) barName = tokens[0].substr(0, dotNdx);
		else barName = tokens[0];
		if (sharedData.barsIndexes.find(barName) != sharedData.barsIndexes.end()) {
			if (lastHyphenNdx != string::npos && isNumber(tokens[0].substr(lastHyphenNdx+1, endOfSubstr-lastHyphenNdx-1))) {
				verticalBarsNdx = stoi(tokens[0].substr(lastHyphenNdx+1, endOfSubstr-lastHyphenNdx-1));
			}
			if (foundDot) {
				if (sharedData.instrumentIndexes.find(tokens[0].substr(dotNdx+1)) != sharedData.instrumentIndexes.end()) {
					instFromBars = tokens[0].substr(dotNdx+1);
				}
			}
		}
	}
	// before we parse the input vector to detect commands, we store it as a string
	// which will be the string of the currect bar
	string barLine = genStrFromVec(tokens);
	// parse the string to detect any containing commands like \ottava, \tuplet, or a bar or loop name
	// first insert the instrument name as this has been striped
	vector<string> v = {"\\" + sharedData.instruments[lastInstrumentIndex].getName()};
	// then append the rest of the input vector
	v.insert(v.end(), tokens.begin(), tokens.end());
	CmdOutput cmdOutput = parseExpandedCommands(v, lineNum, numLines);
	if (cmdOutput.errorCode == 3) return std::make_pair(3, cmdOutput.errorStr);
	tokens.clear();
	tokens = {cmdOutput.outputVec.begin()+1, cmdOutput.outputVec.end()};
	// the boolean below is used to determine if we need to update the barLine string
	bool barLineChanged = false;
	if (tokens[0] == "\\bars") {
		// the algorithm of this if test is similar to the if (parsingBars) above
		// but here we need a boolean too because we get the entire bars definition strings e.g.
		// \bars 1-4 {
		//     \inst1 c''2 d'' | e''2 f'' | \1-4-1 | g''1
		//     \inst2 c'2 d' | e'2 f' | g'1 | \1-4-2
		// }
		// so we need to determine where to start pushing back to the newTokens vector
		int verticalBarsCounter = 1;
		bool startInserting = false;
		vector<string> newTokens;
		for (string s : tokens) {
			if (s == "|" && startInserting) {
				verticalBarsCounter++;
			}
			if (verticalBarsCounter == verticalBarsNdx && s != "|" && startInserting) {
				newTokens.push_back(s);
			}
			if (instFromBars == s) {
				newTokens.push_back(s);
				startInserting = true;
			}
			if (verticalBarsCounter > verticalBarsNdx) {
				break;
			}
		}
		CmdOutput cmdOutput = parseExpandedCommands(newTokens, lineNum, numLines);
		if (cmdOutput.errorCode == 3) return std::make_pair(3, cmdOutput.errorStr);
		barLine = genStrFromVec(newTokens);
		tokens.clear();
		// check for possible newline characters that might be inserted when inputing a loop name in a bars definition
		for (auto it = cmdOutput.outputVec.begin()+1; it != cmdOutput.outputVec.end(); ++it) {
			size_t newlineNdx = (*it).find('\n');
			if (newlineNdx != string::npos) tokens.push_back((*it).substr(0, newlineNdx));
			else tokens.push_back(*it);
		}
		//barLineChanged = true;
	}
	// if we provide a bar name, the tokens vector will contain the bar lines
	// so below we check if the output starts with \bar
	if (tokens[0] == "\\bar") {
		// the output of calling a bar is a vector of strings with single tokens
		// so we run through it to determine which strings we'll use to assemble a new string vector
		bool startPushing = false;
		vector<string> newTokens;
		for (auto it = tokens.begin(); it != tokens.end(); ++it) {
			auto newlineNdx = (*it).find("\n");
			auto openCurlyBracketNdx = (*it).find("{");
			auto closeCurlyBracketNdx = (*it).find("}");
			if (startPushing) {
				// don't include the newline character
				if (newlineNdx != string::npos) {
					newTokens.push_back((*it).substr(0, newlineNdx));
					startPushing = false;
					break;
				}
				else if (closeCurlyBracketNdx != string::npos) {
					newTokens.push_back((*it).substr(0, closeCurlyBracketNdx));
					startPushing = false;
					break;
				}
				else {
					newTokens.push_back(*it);
				}
			}
			if (openCurlyBracketNdx != string::npos && (*it).substr(openCurlyBracketNdx) == "\\" + sharedData.instruments[lastInstrumentIndex].getName()) {
				startPushing = true;
				newTokens.push_back((*it).substr(openCurlyBracketNdx));
			}
			else if (*it == "\\" + sharedData.instruments[lastInstrumentIndex].getName()) {
				startPushing = true;
				newTokens.push_back(*it);
			}
		}
		CmdOutput cmdOutput = parseExpandedCommands(newTokens, lineNum, numLines);
		if (cmdOutput.errorCode == 3) return std::make_pair(3, cmdOutput.errorStr);
		barLine = genStrFromVec(newTokens);
		tokens.clear();
		// the first item of the CmdOutput structure returned by parseExpandedCommands()
		// is the name of the instrument the line of which we're extracting here
		// so we discard it and keep the rest of the information
		tokens = {cmdOutput.outputVec.begin()+1, cmdOutput.outputVec.end()};
		cout << "from bar: \"" << genStrFromVec(tokens) << "\"\n";
		//barLineChanged = true;
	}
	// if we provide a bar name and an instrument name as a second level command
	// then the tokens vector will contain the melodic line of this instrument at this bar
	// so we check if the first item of the tokens vector is an instrument name
	// the else if test below can also be true if we parse many bars and for one of them
	// we ask for a previous bar of this bars definition but for another instrument
	// in this case, we'll get the instrument name and the entire line with all the bars
	else if (sharedData.instrumentIndexes.find(tokens[0]) != sharedData.instrumentIndexes.end()){
		CmdOutput cmdOutput = parseExpandedCommands(tokens, lineNum, numLines);
		tokens.clear();
		// the first item of the CmdOutput structure returned by parseExpandedCommands()
		// is the name of the instrument the line of which we're extracting here
		// so we discard it and keep the rest of the information
		tokens = {cmdOutput.outputVec.begin()+1, cmdOutput.outputVec.end()};
		barLineChanged = true;
	}
	// now we'll store the parsed tokens as a string to the sharedData.barLines map
	// the line is supposed to start with a horizontal tab, so we store that first
	// the bar index has already been stored with the \bar command
	// so we can retrieve it with geLastLoopIndex()
	int barIndex = getLastLoopIndex();
	string barName = sharedData.barsOrdered[barIndex];
	sharedData.barLines[barName] += editors[0].tabStr + "\\" + sharedData.instruments[lastInstrumentIndex].getName() + " ";
	if (barLineChanged) {
		sharedData.barLines[barName] += genStrFromVec(tokens);
	}
	else {
		sharedData.barLines[barName] += barLine;
	}
	sharedData.barLines[barName] += "\n";

	// now that the tokens have been expanded, we can save it, in case this instrument sends to an OSC server
	if (instrumentOSCHostPorts.find(lastInstrumentIndex) != instrumentOSCHostPorts.end()) {
		string forOSCPart = "\\" + sharedData.instruments[lastInstrumentIndex].getName() + " " + genStrFromVec(tokens);
		sharedData.instruments[lastInstrumentIndex].barLines[barIndex] = forOSCPart;
	}

	// once the line has been parsed to expand commands and stored as a string we can move on and create the melodic line
	int tempDur = 0;
	// the following array is used to map the dynamic values
	// to the corresponding MIDI velocity values
	int dynsArr[8] = {-9, -6, -3, -1, 1, 3, 6, 9};
	// an array to test against durations
	vector<int> dursArr{1, 2, 4, 8, 16, 32, 64};
	// the notes of the natural scale in MIDI values
	int midiNotes[7] = {0, 2, 4, 5, 7, 9, 11};
	bool dynamicsRampStarted = false;
	// booleans to determine whether we're writing a chord or not
	//bool chordStarted = false;
	bool firstChordNote = false;
	
	bool foundArticulation = false;
	// boolean for dynamics, whether it's a mezzo forte or mezzo piano
	bool mezzo = false;
	// index variables for loop so that they are initialized before vectors with unkown size
	unsigned i = 0, j = 0, k = 0; //  initialize to avoid compiler warnings
	// then detect any chords, as the size of most vectors equals the number of notes vertically
	// and not single notes within chords
	unsigned numNotesVertical = tokens.size();

	map<int, std::pair<int, int>> tupletRatios;
	map<int, std::pair<unsigned, unsigned>> tupletStartStop;

	// once we define the number of tokens and vertical notes, we can create the vectors with known size
	vector<int> durIndexes(tokens.size(), -1);
	vector<int> dynamicsIndexes(tokens.size(), -1);
	vector<int> midiVels(tokens.size(), 0);
	vector<int> midiPitchBendVals(tokens.size(), 0);
	vector<int> dursData(tokens.size(), 0);
	vector<int> dursDataWithoutSlurs(tokens.size(), 0);
	vector<int> midiDursDataWithoutSlurs(tokens.size(), 0);
	// pitch bend values are calculated by the sequencer
	// so here we store the microtones only
	vector<int> pitchBendVals(tokens.size(), 0);
	vector<int> dotIndexes(tokens.size(), 0);
	vector<unsigned> dotsCounter(tokens.size(), 0);
	vector<int> dynamicsData(tokens.size(), 0);
	vector<int> dynamicsRampData(tokens.size(), 0);
	vector<int> slurBeginningsIndexes(tokens.size(), -1);
	vector<int> slurEndingsIndexes(tokens.size(), -1);
	vector<int> tieIndexes(tokens.size(), -1);
	//vector<int> textIndexesLocal(tokens.size(), 0);
	vector<int> dursForScore(tokens.size(), 0);
	vector<int> dynamicsForScore(tokens.size(), -1);
	vector<int> dynamicsForScoreIndexes(tokens.size(), -1);
	vector<int> dynamicsRampStart(tokens.size(), 0);
	vector<int> dynamicsRampEnd(tokens.size(), 0);
	vector<pair<unsigned, unsigned>> dynamicsRampIndexes(tokens.size());
	vector<int> midiDynamicsRampDurs(tokens.size(), 0);
	vector<int> glissandiIndexes(tokens.size(), 0);
	vector<int> glissandiIndexesForScore(tokens.size(), 0);
	vector<int> midiGlissDurs(tokens.size(), 0);
	vector<int> dynamicsRampStartForScore(tokens.size(), -1);
	vector<int> dynamicsRampEndForScore(tokens.size(), -1);
	vector<int> dynamicsRampDirForScore(tokens.size(), -1);
	vector<bool> isSlurred (tokens.size(), false);
	vector<int> notesCounter(tokens.size(), 0);
	vector<int> verticalNotesIndexes(tokens.size()+1, 0); // plus one to easily break out of loops in case of chords
	vector<int> beginningOfChords(tokens.size(), 0);
	vector<int> endingOfChords(tokens.size(), 0);
	vector<unsigned> accidentalIndexes(tokens.size());
	vector<unsigned> chordNotesIndexes(tokens.size(), 0);
	vector<int> ottavas(tokens.size(), 0);
	vector<bool> foundNotes(tokens.size());
	size_t transposedVecSize = 0;
	for (i = 0; i < tokens.size(); i++) {
		vector<string> subtokens = tokenizeChord(tokens[i]);
		transposedVecSize += subtokens.size();
	}
	vector<int> transposedOctaves(transposedVecSize, 0);
	vector<int> transposedAccidentals(transposedVecSize, 0);
	// variable to determine which character we start looking at
	// useful in case we start a bar with a chord
	vector<unsigned> firstChar(tokens.size(), 0);
	vector<unsigned> firstCharOffset(tokens.size(), 1); // this is zeroed in case of a rhythm staff with durations only
	int lastMidiNote = -1, lastNaturalScaleNote = -1; // these are used for tied notes
	// boolean used to set the values to isSlurred vector
	bool slurStarted = false;
	bool isLastNoteTied = false;
	// a counter for the number of notes in each chord
	unsigned index1 = 0, index2 = 0; // variables to index various data
	// create an unpopullated vector of vector of pairs of the notes as MIDI and natural scale
	// after all vectors with known size and all single variables have been created
	vector<vector<std::pair<int, int>>> notePairs;
	//vector<vector<intPair>> notePairs;
	// then iterate over all the tokens and parse them to store the notes
	for (i = 0; i < tokens.size(); i++) {
		firstChar.at(i) = 0;
		// first check if the token is a comment so we exit the loop
		if (startsWith(tokens.at(i), "%")) continue;
		// further tokenize the token to detect chords
		vector<string> subtokens = tokenizeChord(tokens.at(i));
		foundNotes.at(i) = false;
		transposedOctaves.at(index1) = 0;
		transposedAccidentals.at(index1) = 0;
		// the first element of each token is the note
		// so we first check for it in a separate loop
		// and then we check for the rest of the stuff
		for (j = 0; j < subtokens.size(); j++) {
			int midiNote = -1, naturalScaleNote = -1; // the second is for the score
			for (k = 0; k < 8; k++) {
				if (subtokens.at(j).at(0) == noteChars[k]) {
					midiNote = -1;
					if (k < 7) {
						midiNote = midiNotes[k];
						// we start one octave below the middle C
						midiNote += 48;
						naturalScaleNote = k;
						if (sharedData.instruments.at(lastInstrumentIndex).isRhythm()) {
							if (j > 0) return std::make_pair(3, "chords are not allowed in rhythm staffs");
							midiNote = 59;
							naturalScaleNote = 6;
						}
						//else if (sharedData.instruments.at(lastInstrumentIndex).getTransposition() != 0) {
						//	int transposedMidiNote = midiNotes[k] + sharedData.instruments.at(lastInstrumentIndex).getTransposition();
						//	// testing against the midiNote it is easier to determine whether we need to add an accidental
						//	if (transposedMidiNote < 0) {
						//		transposedMidiNote = 12 + midiNotes[k] + sharedData.instruments.at(lastInstrumentIndex).getTransposition();
						//	}
						//	else if (transposedMidiNote > 11) {
						//		transposedMidiNote %= 12;
						//	}
						//	if (midiNotes[k] < 4) {
						//		transposedOctaves.at(index1) -= 1;
						//	}
						//	if (transposedMidiNote > 4 && (transposedMidiNote % 2) == 0) {
						//		transposedAccidentals.at(index1) = 2;
						//	}
						//	else if (transposedMidiNote <= 4 && (transposedMidiNote % 2) == 1) {
						//		transposedAccidentals.at(index1) = 2;
						//	}
						//	naturalScaleNote = distance(midiNotes, find(begin(midiNotes), end(midiNotes), (transposedMidiNote-(transposedAccidentals.at(index1)/2))));
						//}
					}
					else {
						// the last element of the noteChars array is the rest
						if (j > 0) return std::make_pair(3, "rests can't be included in chords");
						midiNote = naturalScaleNote = -1;
					}
				storeNote:
					lastMidiNote = midiNote;
					lastNaturalScaleNote = naturalScaleNote;
					if (j == 0) {
						// create a new vector for each single note or a group of notes of a chord
						std::pair p = std::make_pair(midiNote, naturalScaleNote);
						std::vector<std::pair<int, int>> aVector(1, p);
						notePairs.push_back(std::move(aVector));
						chordNotesIndexes.at(i) = 0;
        	            notesCounter.at(i)++;
						dotIndexes.at(i) = 0;
						glissandiIndexes.at(i) = 0;
						glissandiIndexesForScore.at(i) = 0;
						midiGlissDurs.at(i) = 0;
						midiDynamicsRampDurs.at(i) = 0;
						pitchBendVals.at(i) = 0;
						//textIndexesLocal.at(index2) = 0;
						index2++;
						if (firstChordNote) firstChordNote = false;
					}
					else {
						// if we have a chord, push this note to the current vector
						notePairs.back().push_back(std::make_pair(midiNote, naturalScaleNote));
						// increment the counter of the chord notes
						// so we can set the index for each note in a chord properly
						chordNotesIndexes.at(i)++;
        	            notesCounter.at(i)++;
					}
					foundNotes.at(i) = true;
					break;
				}
			}
			if (!foundNotes.at(i)) {
				if (sharedData.instruments.at(lastInstrumentIndex).isRhythm() || isLastNoteTied) {
					// to be able to write a duration without a note, for rhythm staffs
					// we need to check if the first character is a number
					int dur = 0;
					if (subtokens.at(j).size() > 1) {
						if (isdigit(subtokens.at(j).at(0)) && isdigit(subtokens.at(j).at(1))) {
							dur = int(subtokens.at(j).at(0)+subtokens.at(j).at(1));
						}
						else if (isdigit(subtokens.at(j).at(0))) {
							dur = int(subtokens.at(j).at(0));
						}
						else {
							return std::make_pair(3, (string)"first character must be a note or a chord opening symbol (<), not \"" + subtokens.at(j).at(0) + (string)"\"");
						}
					}
					else if (isdigit(subtokens.at(j).at(0))) {
						dur = int(subtokens.at(j).at(0));
					}
					else {
						return std::make_pair(3, (string)"first character must be a note or a chord opening symbol (<), not \"" + subtokens.at(j).at(0) +(string)"\"");
					}
					if (std::find(std::begin(dursArr), std::end(dursArr), dur) == std::end(dursArr)) {
						if (isLastNoteTied) {
							midiNote = lastMidiNote;
							naturalScaleNote = lastNaturalScaleNote;
						}
						else {
							midiNote = 59;
							naturalScaleNote = 6;
						}
						// we need to assign 0 to firstCharOffset which defaults to 1
						// because further down it is added to firstChar
						// but, in this case, we want to check the token from its beginning, index 0
						firstCharOffset.at(i) = 0;
						goto storeNote;
					}
					else {
						return std::make_pair(3, to_string(dur) + ": wrong duration");
					}
				}
				else {
					return std::make_pair(3, (string)"first character must be a note or a chord opening symbol (<), not \"" + subtokens.at(j).at(0) + (string)"\"");
				}
			}
			for (k = 0; k < subtokens.at(j).size(); k++) {
				// if we have a tilde, it means that this note is tide to the next one
				if (subtokens.at(j).at(k) == 126) {
					isLastNoteTied = true;
					break;
				}
			}
		}
		index1++;
	}

	// now run through the entire subtoken to check if we have a character that is forbidden inside a chord
	for (i = 0; i < tokens.size(); i++) {
		// here we need to include the angle brackets to make sure we check for invalid characters inside a chord and not outside of it
		vector<string> subtokens = tokenizeChord(tokens.at(i), true);
		vector<char> forbiddenCharsInChord{'(', ')', '-', '.', '\\', '^', '_', '{', '}'};
		bool outsideChord = true;
		for (j = 0; j < subtokens.size(); j++) {
			if (subtokens.at(j).size() == 0) break;
			if (j == 0 && subtokens.at(j).at(0) == '<') outsideChord = false;
			for (k = 0; k < subtokens.at(j).size(); k++) {
				if (j == subtokens.size()-1 && subtokens.at(j).at(k) == '>') {
					outsideChord = true;
					break;
				}
				if (j > 0 && find(forbiddenCharsInChord.begin(), forbiddenCharsInChord.end(), subtokens.at(j).at(k)) != forbiddenCharsInChord.end()) {
					// first check if we have a backslash, in which case we should check if it's actually a dynamic
					if (subtokens.at(j).at(k) == 92) {
						if (k < subtokens.at(j).size()) {
							// compare with "m", "p", "f", "<", ">", and "!"
							if (subtokens.at(j).at(k+1) == char(109) || subtokens.at(j).at(k+1) == char(112) || subtokens.at(j).at(k+1) == char(102) || \
									subtokens.at(j).at(k+1) == char(60) || subtokens.at(j).at(k+1) == char(62) || subtokens.at(j).at(k+1) == char(33)) {
								return std::make_pair(3, "dynamic can't be included within a chord, only outside of it");
							}
							return std::make_pair(3, char(subtokens.at(j).at(k)) + (string)" can't be included within a chord, only outside of it");
						}
						return std::make_pair(3, char(subtokens.at(j).at(k)) + (string)" can't be included within a chord, only outside of it");
					}
					return std::make_pair(3, char(subtokens.at(j).at(k)) + (string)" can't be included within a chord, only outside of it");
				}
			}
			// stop checking after exiting the chord
			if (outsideChord) break;
		}
	}
	
	// reset the indexes
	index1 = index2 = 0;

	// now we have enough information to allocate memory for the rest of the vectors
	// but we do it one by one, to allocate the required memory only
	// as each note might be a chord or not
	vector<vector<int>> midiNotesData;
	for (i = 0; i < notePairs.size(); i++) {
		int value = notePairs.at(i).at(0).first;
		std::vector<int> aVector(1, value);
		midiNotesData.push_back(std::move(aVector));
		for (j = 1; j < notePairs.at(i).size(); j++) {
			midiNotesData.back().push_back(notePairs.at(i).at(j).first);
		}
	}
	vector<vector<float>> notesData;
	for (i = 0; i < midiNotesData.size(); i++) {
		float value = midiNotesData.at(i).at(0);
		std::vector<float> aVector(1, value);
		notesData.push_back(std::move(aVector));
		for (j = 1; j < midiNotesData.at(i).size(); j++) {
			notesData.back().push_back((float)midiNotesData.at(i).at(j));
		}
	}
	vector<vector<int>> notesForScore;
	for (i = 0; i < notePairs.size(); i++) {
		int value = notePairs.at(i).at(0).second;
		std::vector<int> aVector(1, value);
		notesForScore.push_back(std::move(aVector));
		for (j = 1; j < notePairs.at(i).size(); j++) {
			notesForScore.back().push_back(notePairs.at(i).at(j).second);
		}
	}
	vector<vector<int>> accidentalsForScore;
	for (i = 0; i < midiNotesData.size(); i++) {
		std::vector<int> aVector(1, -1);
		accidentalsForScore.push_back(std::move(aVector));
		for (j = 1; j < midiNotesData.at(i).size(); j++) {
			accidentalsForScore.back().push_back(-1);
		}
	}
	// the 2D vector below is used here only for memory allocation
	// its contents are updated in drawNotes() of the Notes() class
	vector<vector<int>> naturalSignsNotWrittenForScore;
	for (i = 0; i < midiNotesData.size(); i++) {
		std::vector<int> aVector(1, 0);
		naturalSignsNotWrittenForScore.push_back(std::move(aVector));
		for (j = 1; j < midiNotesData.at(i).size(); j++) {
			naturalSignsNotWrittenForScore.back().push_back(0);
		}
	}
	int transposedIndex = 0;
	vector<vector<int>> octavesForScore;
	for (i = 0; i < midiNotesData.size(); i++) {
		if (sharedData.instruments.at(lastInstrumentIndex).isRhythm()) {
			std::vector<int> aVector(1, 1);
			octavesForScore.push_back(std::move(aVector));
		}
		else {
			std::vector<int> aVector(1, transposedOctaves[transposedIndex++]);
			octavesForScore.push_back(std::move(aVector));
		}
		for (j = 1; j < midiNotesData.at(i).size(); j++) {
			octavesForScore.back().push_back(transposedOctaves[transposedIndex++]);
		}
	}

	// before we move on to the rest of the data, we need to store the texts
	// because this is dynamically allocated memory too
	int textsCounter;
	vector<vector<int>> textIndexesLocal;
	for (i = 0; i < tokens.size(); i++) {
		// check if we have a comment, so we exit the loop
		if (startsWith(tokens.at(i), "%")) break;
		// create an empty entry for every note/chord
		if (textIndexesLocal.size() <= i) {
			textIndexesLocal.push_back({0});
		}
		textsCounter = 0;
		// then check for carrets or underscores
		for (j = 0; j < tokens.at(i).size(); j++) {
			if (tokens.at(i).at(j) == (94)) {
				if (j > 0) {
					// a carret is also used for articulations, so we must check if the previous character is not a hyphen
					if (tokens.at(i).at(j-1) != '-') {
						if (textsCounter == 0) {
							textIndexesLocal.back().back() = 1;
						}
						else {
							textIndexesLocal.back().push_back(1);
						}
						textsCounter++;
					}
				}
				else {
					if (textsCounter == 0) {
						textIndexesLocal.back().back() = 1;
					}
					else {
						textIndexesLocal.back().push_back(1);
					}
					textsCounter++;
				}
			}
			else if (tokens.at(i).at(j) == 95) {
				if (tokens.at(i).at(j-1) != '-') {
					if (textsCounter == 0) {
						textIndexesLocal.back().back() = -1;
					}
					else {
						textIndexesLocal.back().push_back(-1);
					}
					textsCounter++;
				}
			}
		}
	}
	// once we have sorted out the indexes of the texts, we can sort out the actual texts
	unsigned openQuote, closeQuote;
	vector<string> texts;
	for (i = 0; i < tokens.size(); i++) {
		// again, check if we have a comment, so we exit the loop
		if (startsWith(tokens.at(i), "%")) break;
		for (j = 0; j < tokens.at(i).size(); j++) {
			// ^ or _ for adding text above or below the note
			if ((tokens.at(i).at(j) == char(94)) || (tokens.at(i).at(j) == char(95))) {
				if (j > 0) {
					if (tokens.at(i).at(j-1) == '-') foundArticulation = true;
				}
				if (j >= (tokens.at(i).size()-1) && !foundArticulation) {
					if (tokens.at(i).at(j) == char(94)) {
						return std::make_pair(3, "a carret must be followed by text in quotes");
					}
					else {
						return std::make_pair(3, "an undescore must be followed by text in quotes");
					}
				}
				if (j < tokens.at(i).size()-1) {
					if (tokens.at(i).at(j+1) != char(34) && !foundArticulation) { // "
						if (tokens.at(i).at(j) == char(94)) {
							return std::make_pair(3, "a carret must be followed by a bouble quote sign");
						}
						else {
							return std::make_pair(3, "an underscore must be followed by a bouble quote sign");
						}
					}
				}
				if (!foundArticulation) {
					openQuote = j+2;
					index2 = j+2;
					closeQuote = j+2;
					while (index2 < tokens.at(i).size()) {
						if (tokens.at(i).at(index2) == char(34)) {
							closeQuote = index2;
							break;
						}
						index2++;
					}
					if (closeQuote <= openQuote) {
						return std::make_pair(3, "text must be between two double quote signs");
					}
					texts.push_back(tokens.at(i).substr(openQuote, closeQuote-openQuote));
					//if (tokens.at(i).at(j) == char(94)) {
					//	textIndexesLocal.at(i) = 1;
					//}
					//else if (tokens.at(i).at(j) == char(95)) {
					//	textIndexesLocal.at(i) = -1;
					//}
					j = closeQuote;
				}
			}
		}
		foundArticulation = false;
	}
	// then copy the texts to textsForScore
	vector<string> textsForScore;
	for (i = 0; i < texts.size(); i++) {
		textsForScore.push_back(texts.at(i));
	}
	// store articulation symbols, as these have to be allocated dynamically too
	int quotesCounter = 0; // so we can ignore anything that is passed as text
	vector<vector<int>> articulationIndexes;
	for (i = 0; i < tokens.size(); i++) {
		foundArticulation = false;
		unsigned firstArticulIndex = 0;
		if (articulationIndexes.size() <= i) {
			articulationIndexes.push_back({0});
		}
		vector<string> subtokens = tokenizeChord(tokens.at(i));
		for (j = 0; j < subtokens.back().size(); j++) {
			if (int(subtokens.back().at(j)) == 34 && quotesCounter >= 1) {
				quotesCounter++;
				if (quotesCounter > 2) quotesCounter = 0;
			}
			if (quotesCounter > 0) continue; // if we're inside text, ignore
			if (subtokens.back().at(j) == char(45) && (j > 0 && subtokens.back().at(j-1) != 'o')) { // - for articulation symbols
				if (j >= (subtokens.back().size()-1)) {
					return std::make_pair(3, "a hyphen must be followed by an articulation symbol");
				}
				if (!foundArticulation) {
					firstArticulIndex = j;
				}
				// since the hyphen is used both for indicating the command for articulation
				// but also for the tenuto symbol, we have to iterate through these symbols
				// every two, otherwise, if we write a tenuto, the else if (tokens.at(i).at(j) == char(45)) test
				// will be successful not only for the articulation command, but also for the symbol
				else if ((j - firstArticulIndex) < 2) {
					continue;
				}
				foundArticulation = true;
				int articulChar = char(subtokens.back().at(j+1));
				int articulIndex = 0;
				switch (articulChar) {
					// articulation symbol indexes start from 1 because 0 is reserved for no articulation
					case 94: // ^ for marcato
						articulIndex = 1;
						break;
					case 43: // + for trill
						articulIndex = 2;
						break;
					case 45: // - for tenuto
						articulIndex = 3;
						break;
					case 33: // ! for staccatissimo
						articulIndex = 4;
						break;
					case 62: // > for accent
						articulIndex = 5;
						break;
					case 46: // . for staccato
						articulIndex = 6;
						break;
					case 95: // _ for portando
						articulIndex = 7;
						break;
					default:
						return std::make_pair(3, "unknown articulation symbol");
				}
				if (articulationIndexes.at(i).size() == 1 && articulationIndexes.at(i).at(0) == 0) {
					articulationIndexes.at(i).at(0) = articulIndex;
				}
				else {
					articulationIndexes.at(i).push_back(articulIndex);
				}
				j++;
				continue;
			}
		}
	}
	// copy the articulation indexes with an offset for MIDI
	vector<vector<int>> midiArticulationVals;
	for (i = 0; i < articulationIndexes.size(); i++) {
		vector<int> aVector(1, (articulationIndexes.at(i).at(0) > 0 ? articulationIndexes.at(i).at(0)+9 : 1));
		midiArticulationVals.push_back(std::move(aVector));
		for (j = 1; j < articulationIndexes.at(i).size(); j++) {
			midiArticulationVals.back().push_back(articulationIndexes.at(i).at(j)+9);
		}
	}

	// now check for tuplets
	int tupletNdx = 0;
	int tupletNumerator = 1, tupletDenominator = 1;
	vector<char> allowedChars = {'c', 'd', 'e', 'f', 'g', 'a', 'b', 'r', 'i', 's', 'h', '\'', ',', 'o', '-'};
	for (i = 0; i < tokens.size(); i++) {
		vector<string> subtokens = tokenizeChord(tokens.at(i));
		bool isTuplet = true;
		// tuplets are converted to a forward slash and the tuplet ration after it
		// for example, \tuplet 3/2 {c''8 d'' e''} is converted to c''8/3/2 d''/3/2 e''/3/2
		// so we need to look for digits after a note character, an octave symbol (, or ') or a duration digit
		size_t slashNdx = subtokens.back().find('/');
		if (slashNdx != string::npos) {
			for (j = 0; j < slashNdx; j++) {
				// if we find a character that is not allowed
				if (find(allowedChars.begin(), allowedChars.end(), subtokens.back().at(j)) == allowedChars.end() && !isdigit(subtokens.back().at(j))) {
					// this is not a tuplet, move on
					isTuplet = false;
					break;
				}
			}
			if (isTuplet) {
				size_t secondSlashNdx = subtokens.back().substr(slashNdx+1).find('/');
				if (secondSlashNdx != string::npos) {
					if (isNumber(subtokens.back().substr(slashNdx+1, secondSlashNdx))) {
						tupletNumerator = stoi(subtokens.back().substr(slashNdx+1, secondSlashNdx));
						k = slashNdx + secondSlashNdx + 2;
						bool foundDenominator = false;
						while (k < subtokens.back().size()) {
							if (!isdigit(subtokens.back().at(k))) {
								if (isNumber(subtokens.back().substr(slashNdx+secondSlashNdx+1, k-slashNdx-secondSlashNdx-1))) {
									tupletDenominator = stoi(subtokens.back().substr(slashNdx+secondSlashNdx+1, k-slashNdx-secondSlashNdx-1));
									foundDenominator = true;
								}
								else {
									isTuplet = false;
								}
								break;
							}
							k++;
						}
						if (!foundDenominator && isdigit(subtokens.back().at(k-1))) {
							tupletDenominator = stoi(subtokens.back().substr(k-1, 1));
							isTuplet = true;
						}
						else {
							isTuplet = false;
						}
					}
					else {
						isTuplet = false;
					}
				}
				else {
					isTuplet = false;
				}
			}
		}
		else {
			isTuplet = false;
		}
		if (isTuplet) {
			tupletStartStop[tupletNdx] = std::make_pair(i, i+tupletNumerator-1);
			tupletRatios[tupletNdx++] = std::make_pair(tupletNumerator, tupletDenominator);
			// move the i index to test notes beyond the tuplet we just stored
			i += tupletNumerator;
			i--; // because i is incremented before testing if the loop must continue
		}
	}

	// we are now done with dynamically allocating memory, so we can move on to the rest of the data
	// we can now create more variables without worrying about memory

	// the vector below is used later to check if we encounter some unkown character
	vector<int> foundAccidentals(tokens.size(), 0);
	float accidental = 0.0;
	// various counters
	unsigned numDurs = 0;
	unsigned numDynamics = 0;
	unsigned dynamicsRampCounter = 0;
	unsigned slursCounter = 0;
	for (i = 0; i < tokens.size(); i++) {
		// first check for accidentals, if any
		vector<string> subtokens = tokenizeChord(tokens.at(i));
		for (j = 0; j < subtokens.size(); j++) {
			bool foundAccidental = false;
			// the first character of a note token is the actual note, so we start from 1 in the loop below
			for (k = 1; k < subtokens.at(j).size(); k++) {
				if (subtokens.at(j).at(k) == char(101)) { // 101 is "e"
					if (k < subtokens.at(j).size()-1) {
						// if the character after "e" is "s" or "h" we have an accidental
						if (subtokens.at(j).at(k+1) == char(115)) { // 115 is "s"
							accidental -= 1.0; // in which case we subtract one semitone
							foundAccidental = true;
							foundAccidentals.at(i) = 1;
						}
						else if (subtokens.at(j).at(k+1) == char(104)) { // 104 is "h"
							accidental -= 0.5;
							foundAccidental = true;
							foundAccidentals.at(i) = 1;
						}
						else {
							return std::make_pair(3, subtokens.at(j).at(k+1) + (string)": unknown accidental character");
						}
					}
					else {
						return std::make_pair(3, "\"e\" must be followed by \"s\" or \"h\"");
					}
				}
				else if (subtokens.at(j).at(k) == char(105)) { // 105 is "i"
					if (k < subtokens.at(j).size()-1) {
						if (subtokens.at(j).at(k+1) == char(115)) { // 115 is "s"
							accidental += 1.0; // in which case we add one semitone
							foundAccidental = true;
							foundAccidentals.at(i) = 1;
						}
						else if (subtokens.at(j).at(k+1) == char(104)) { // 104 is "h"
							accidental += 0.5;
							foundAccidental = true;
							foundAccidentals.at(i) = 1;
						}
						else {
							return std::make_pair(3, subtokens.at(j).at(k+1) + (string)": unknown accidental character");
						}
					}
					else {
						return std::make_pair(3, "\"i\" must be followed by \"s\" or \"h\"");
					}
				}
				// we ignore "s" and "h" as we have checked them above
				else if (subtokens.at(j).at(k) == char(115) || subtokens.at(j).at(k) == char(104)) {
					accidentalIndexes.at(i)++;
					continue;
				}
				// when the accidentals characters are over we move on
				else {
					break;
				}
				accidentalIndexes.at(i)++;
			}
			if (foundAccidental) {
				notesData.at(i).at(j) += accidental;
				midiNotesData.at(i).at(j) += (int)accidental;
				// accidentals can go up to a whole tone, which is accidental=2.0
				// but in case it's one and a half tone, one tone is already added above
				// so we need the half tone only, which we get with accidental-(int)accidental
				pitchBendVals.at(i) = (abs(accidental)-abs((int)accidental));
				if (accidental < 0.0) pitchBendVals.at(i) *= -1.0;
				// store accidentals in the following order with the following indexes
				// 0 - double flat, 1 - one and half flat, 2 - flat, 3 - half flat,
				// 4 - natural, 5 - half sharp, 6 - sharp, 7 - one and half sharp, 8 - double sharp
				// to map the -2 to 2 range of the accidental float variable to a 0 to 8 range
				// we use the map function below
				// this is subtracting the fromLow value (-2) which results in adding 2
				// and mutliplying by the division between the difference between toHigh - toLow and fromHigh - fromLow
				// which again results to 2, since (8 - 0) / (2 - -2) = 8 / 4 = 2
				// a proper mapping function then adds the toLow value, but here it's 0 so we omit it
				int mapped = (accidental + 2) * 2; 
				accidentalsForScore.at(i).at(j) = mapped;
				//accidentalsForScore.at(i).at(j) += transposedAccidentals.at(i);
				// go back one character because of the last accidentalIndexes.at(i)++ above this if chunk
				accidentalIndexes.at(i)--;
			}
			else {
				accidentalsForScore.at(i).at(j) = -1;
			}
			// add the accidentals based on transposition (if set)
			if (accidentalsForScore.at(i).at(j) == -1) {
				// if we have no accidental, add the transposition to the natural sign indexes with 4
				accidentalsForScore.at(i).at(j) = 4 + transposedAccidentals.at(i);
			}
			else {
				accidentalsForScore.at(i).at(j) += transposedAccidentals.at(i);
			}
			// if the transposed accidental results in natural, assign -1 instead of 4 so as to not display the natural sign
			// if the natural sign is needed to be displayed, this will be taken care of at the end of the last loop
			// that iterates through the tokens of the string we parse in this function
			if (accidentalsForScore.at(i).at(j) == 4) {
				accidentalsForScore.at(i).at(j) = -1;
			}
			accidental = 0;
		}
	}
	
	vector<int> foundDynamics(tokens.size(), 0);
	vector<int> foundOctaves(tokens.size(), 0);
	bool dynAtFirstNote = false;
	unsigned beginningOfChordIndex = 0;
	bool tokenInsideChord = false;
	int prevScoreDynamic = -1;
	int ottava = 0;
	quotesCounter = 0;
	for (i = 0; i < tokens.size(); i++) {
		foundArticulation = false;
		if (beginningOfChords.at(i)) {
			beginningOfChordIndex = i;
			tokenInsideChord = true;
		}
		vector<string> subtokens = tokenizeChord(tokens.at(i));
		// first check for octaves and ottavas
		for (j = 0; j < subtokens.size(); j++) {
			for (k = 0; k < subtokens.at(j).size(); k++) {
				if (int(subtokens.at(j).at(k)) == 34 && quotesCounter >= 1) {
					quotesCounter++;
					if (quotesCounter > 2) quotesCounter = 0;
				}
				if (quotesCounter > 0) continue; // if we're inside text, ignore
				if (int(subtokens.at(j).at(k)) == 39) {
					if (!sharedData.instruments.at(lastInstrumentIndex).isRhythm()) {
						notesData.at(i).at(j) += 12;
						midiNotesData.at(i).at(j) += 12;
						octavesForScore.at(i).at(j)++;
					}
					foundOctaves.at(i) = 1;
				}
				else if (int(subtokens.at(j).at(k)) == 44) {
					if (!sharedData.instruments.at(lastInstrumentIndex).isRhythm()) {
						notesData.at(i).at(j) -= 12;
						midiNotesData.at(i).at(j) -= 12;
						octavesForScore.at(i).at(j)--;
					}
					foundOctaves.at(i) = 1;
				}
				else if (subtokens.at(j).at(k) == 'o') {
					if (k < subtokens.at(j).size()-1) {
						if (isNumber(string(1, subtokens.at(j).at(k+1)))) {
							int ndxOffset = 1;
							if (subtokens.at(j).at(k+1) == '-') ndxOffset++;
							ottava = stoi(string(1, subtokens.at(j).at(k+ndxOffset)));
							if (ndxOffset == 2) ottava *= -1;
							// we have already checked the argument to \ottava in parseCommand()
							// but we need to check here too, in case the user writes in "raw" LiveLily
							if (ottava < -2 || ottava > 2) {
								return std::make_pair(3, "argument to \\ottava must be between -2 and 2");
							}
						}
					}
				}
			}
		}
		ottavas.at(i) = ottava;
		// then check for the rest of the characters
		// we don't check inside chords, as the characters we look for in this loop
		// are not permitted inside chords, so we check only at the last chord note
		bool tupletDigits = false;
		for (j = 0; j < subtokens.back().size(); j++) {
			if (int(subtokens.back().at(j)) == 34 && quotesCounter >= 1) {
				quotesCounter++;
				if (quotesCounter > 2) quotesCounter = 0;
			}
			if (quotesCounter > 0) continue; // if we're inside text, ignore
			if (subtokens.back().at(j) == '/') {
				// a forward slash after duration digits means that the digits that come after it belong to a tuplet definition
				tupletDigits = true;
			}
			if (isdigit(subtokens.back().at(j))) {
				// make sure this is not an ottava digit or a tuplet
				if (j > 0 && subtokens.back().at(j-1) != 'o' && subtokens.back().at(j-1) != '-' && !tupletDigits) {
					// assemble the value from its ASCII characters
					tempDur = tempDur * 10 + int(subtokens.back().at(j)) - 48;
					if (i == 0) {
						dynAtFirstNote = true;
					}
					if (i > 0 && !tokenInsideChord && !dynAtFirstNote) {
						return std::make_pair(3, "first note doesn't have a duration");
					}
				}
			}
			
			else if (subtokens.back().at(j) == char(92)) { // back slash
				index2 = j;
				int dynamic = 0;
				bool foundDynamicLocal = false;
				bool foundGlissandoLocal = false;
				if (index2 > (subtokens.back().size()-1)) {
					return std::make_pair(3, "a backslash must be followed by a command");
				}
				// loop till you find all dynamics or other commands
				while (index2 < subtokens.back().size()) {
					if (index2+1 == subtokens.back().size()) {
						goto foundDynamicTest;
					}
					if (subtokens.back().at(index2+1) == char(102)) { // f for forte
						if (mezzo) {
							dynamic++;
							mezzo = false;
						}
						else {
							dynamic += 3;
						}
						dynamicsIndexes.at(i) = i;
						dynamicsForScoreIndexes.at(i) = i;
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							dynamicsRampEnd.at(i) = i;
							dynamicsRampEndForScore.at(i) = i;
							dynamicsRampIndexes.at(dynamicsRampCounter-1).second = i;
							if (dynamicsRampStart.at(i) == dynamicsRampEnd.at(i)) {
								std::pair<int, string> p = std::make_pair(3, "can't start and end a crescendo/diminuendo on the same note");
								return p;
							}
						}
						foundDynamics.at(i) = 1;
						foundDynamicLocal = true;
					}
					else if (subtokens.back().at(index2+1) == char(112)) { // p for piano
						if (mezzo) {
							dynamic--;
							mezzo = false;
						}
						else {
							dynamic -= 3;
						}
						dynamicsIndexes.at(i) = i;
						dynamicsForScoreIndexes.at(i) = i;
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							dynamicsRampEnd.at(i) = i;
							dynamicsRampEndForScore.at(i) = i;
							dynamicsRampIndexes.at(dynamicsRampCounter-1).second = i;
							if (dynamicsRampStart.at(i) == dynamicsRampEnd.at(i)) {
								std::pair<int, string> p = std::make_pair(3, "can't start and end a crescendo/diminuendo on the same note");
								return p;
							}
						}
						foundDynamics.at(i) = 1;
						foundDynamicLocal = true;
					}
					else if (subtokens.back().at(index2+1) == char(109)) { // m for mezzo
						mezzo = true;
						//if (dynamicsRampStarted) {
						//	dynamicsRampStarted = false;
						//	dynamicsRampEnd.at(i) = i;
						//	dynamicsRampEndForScore.at(i) = i;
						//	dynamicsRampIndexes.at(dynamicsRampCounter-1).second = i;
						//	if (dynamicsRampStart.at(i) == dynamicsRampEnd.at(i)) {
						//		std::pair<int, string> p = std::make_pair(3, "can't start and end a crescendo/diminuendo on the same note");
						//		return p;
						//	}
						//}
					}
					else if (subtokens.back().at(index2+1) == char(60)) { // <
						if (!dynamicsRampStarted) {
							dynamicsRampStarted = true;
							dynamicsRampStart.at(i) = i;
							dynamicsRampStartForScore.at(i) = i;
							dynamicsRampDirForScore.at(i) = 1;
							dynamicsRampIndexes.at(dynamicsRampCounter).first = i;
							dynamicsRampCounter++;
						}
					}
					else if (subtokens.back().at(index2+1) == char(62)) { // >
						if (!dynamicsRampStarted) {
							dynamicsRampStarted = true;
							dynamicsRampStart.at(i) = i;
							dynamicsRampStartForScore.at(i) = i;
							dynamicsRampDirForScore.at(i) = 0;
							dynamicsRampIndexes.at(dynamicsRampCounter).first = i;
							dynamicsRampCounter++;
						}
					}
					else if (subtokens.back().at(index2+1) == char(33)) { // exclamation mark
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							dynamicsRampEnd.at(i) = i;
							dynamicsRampEndForScore.at(i) = i;
							dynamicsRampIndexes.at(dynamicsRampCounter-1).second = i;
							if (dynamicsRampStart.at(i) == dynamicsRampEnd.at(i)) {
								std::pair<int, string> p = std::make_pair(3, "can't start and end a crescendo/diminuendo on the same note");
								return p;
							}
						}
						else {
							return std::make_pair(3, "no crescendo or diminuendo initiated");
						}
					}
					else if (subtokens.back().at(index2+1) == char(103)) { // g for gliss
						if ((subtokens.back().substr(index2+1,5).compare("gliss") == 0) ||
							(subtokens.back().substr(index2+1,9).compare("glissando") == 0)) {
							glissandiIndexes.at(i) = 1;
							glissandiIndexesForScore.at(i) = 1;
							foundGlissandoLocal = true;
						}
					}
					else {
					foundDynamicTest:
						if (foundDynamicLocal) {
							// dynamics are stored like this:
							// ppp = -9, pp = -6, p = -3, mp = -1, mf = 1, f = 3, ff = 6, fff = 9
							// so to map this range to 60-100 (dB) we must first add 9 (subtract -9)
							// multiply by 40 (100 - 60) / (9 - (-9)) = 40 / 18
							// and add 60 (the lowest value)
							if (dynamic < -9) dynamic = -9;
							else if (dynamic > 9) dynamic = 9;
							int scaledDynamic = (int)((float)(dynamic + 9) * ((100.0-(float)MINDB) / 18.0)) + MINDB;
							dynamicsData.at(i) = scaledDynamic;
							// convert the range MINDB to 100 to indexes from 0 to 7
							// subtract the minimum dB value from the dynamic, multipled by (7/dB-range)
							// add 0.5 and get the int to round the float properly
							int dynamicForScore = (int)((((float)dynamicsData.at(i)-(float)MINDB)*(7.0/(100.0-(float)MINDB)))+0.5);
							if (dynamicForScore != prevScoreDynamic) {
								dynamicsForScore.at(i) = dynamicForScore;
								prevScoreDynamic = dynamicForScore;
							}
							else {
								// if it's the same dynamic, we must invalidate this index
								dynamicsForScoreIndexes.at(i) = -1;
							}
							numDynamics++;
							// to get the dynamics in MIDI velocity values, we map ppp to 16, and fff to 127
							// based on this website https://www.hedsound.com/p/midi-velocity-db-dynamics-db-and.html
							// first find the index of the dynamic value in the array {-9, -6, -3, -1, 1, 3, 6, 9}
							int midiVelIndex = std::distance(dynsArr, std::find(dynsArr, dynsArr+8, dynamic));
							midiVels.at(i) = (midiVelIndex+1)*16;
							if (midiVels.at(i) > 127) midiVels.at(i) = 127;
						}
						// if we haven't found a dynamic and the ramp vectors are empty
						// it means that we have received some unknown character
						else if (dynamicsRampCounter == 0 && !foundGlissandoLocal) {
							return std::make_pair(3, tokens.at(i).at(index2+1) + (string)": unknown character");
						}
						break;
					}
					index2++;
				}
				j = index2;
			}
			
			// . for dotted note, which is also used for staccato, hence the second test against 45 (hyphen)
			else if (subtokens.back().at(j) == char(46) && subtokens.back().at(j-1) != char(45)) {
				dotIndexes.at(i) = 1;
				dotsCounter.at(i)++;
			}

			else if (subtokens.back().at(j) == char(40)) { // ( for beginning of slur
				slurStarted = true;
				slurBeginningsIndexes.at(i) = i;
				//slurIndexes.at(i).first = i;
				slursCounter++;
			}

			else if (subtokens.back().at(j) == char(41)) { // ) for end of slur
				slurStarted = false;
				slurEndingsIndexes.at(i) = i;
				//slurIndexes.at(i).second = i;
			}

			else if (subtokens.back().at(j) == char(126)) { // ~ for tie
				tieIndexes.at(i) = i;
			}

			// check for _ or ^ and make sure that the previous character is not -
			else if ((int(subtokens.back().at(j)) == 94 || int(subtokens.back().at(j)) == 95) && int(subtokens.back().at(j-1)) != 45) {
				if (textIndexesLocal.at(i).at(0) == 0) {
					return std::make_pair(3, (string)"stray " + subtokens.back().at(j));
				}
				if (subtokens.back().size() < j+3) {
					return std::make_pair(3, "incorrect text notation");
				}
				if (int(subtokens.back().at(j+1)) != 34) {
					return std::make_pair(3, "a text symbol must be followed by quote signs");
				}
				else {
					quotesCounter++;
				}
			}
		}
		// add the extracted octave with \ottava
		for (j = 0; j <= chordNotesIndexes.at(i); j++) {
			octavesForScore.at(i).at(j) -= ottavas.at(i);
		}
		// store durations at the end of each token only if tempDur is greater than 0
		if (tempDur > 0) {
			if (std::find(std::begin(dursArr), std::end(dursArr), tempDur) == std::end(dursArr)) {
				return std::make_pair(3, to_string(tempDur) + " is not a valid duration");
			}
			dursData.at(i) = tempDur;
			durIndexes.at(i) = i;
			numDurs++;
			if (tokenInsideChord) {
				for (unsigned k = beginningOfChordIndex; k < i; k++) {
					dursData.at(k) = tempDur;
				}
			}
			tempDur = 0;
		}
		if (endingOfChords.at(i)) {
			tokenInsideChord = false;
		}
		// once done parsing everything, check if we need to change an accidental by placing the natural sign
		// in case the same note has already been used in this bar with an accidental and now without
		int thisNote = notesForScore.at(i).at(chordNotesIndexes.at(i));
		int thisOctave = octavesForScore.at(i).at(chordNotesIndexes.at(i));
		// do a reverse loop to see if the last same note had its accidental corrected
		bool foundSameNote = false;
		// use ints instead of j (we are inside a loop using i) because j is unsigned
		// and a backwards loop won't work as it will wrap around instead of going below 0
		if (accidentalsForScore.at(i).at(chordNotesIndexes.at(i)) == -1) {
			for (int k = (int)i-1; k >= 0; k--) {
				for (int l = (int)notesForScore.at(k).size()-1; l >= 0; l--) {
					if (notesForScore.at(k).at(l) == thisNote && ((correctOnSameOctaveOnly && octavesForScore.at(k).at(l) == thisOctave) || !correctOnSameOctaveOnly)) {
						if (accidentalsForScore.at(k).at(l) > -1 && accidentalsForScore.at(k).at(l) != 4) {
							accidentalsForScore.at(i).at(chordNotesIndexes.at(i)) = 4;
							foundSameNote = true;
						}
						else {
							accidentalsForScore.at(i).at(chordNotesIndexes.at(i)) = -1;
							foundSameNote = true;
						}
						break;
					}
				}
				if (foundSameNote) break;
			}
			if (!foundSameNote) accidentalsForScore.at(i).at(chordNotesIndexes.at(i)) = -1;
		}
		if (slurStarted) {
			isSlurred.at(i) = true;
		}
	}

	if (dynamicsRampStarted) {
		dynamicsRampIndexes.at(dynamicsRampCounter-1).second = i;
	}

	// check if there are any unmatched quote signs
	if (quotesCounter > 0) {
		return std::make_pair(3, "unmatched quote sign");
	}

	// once all elements have been parsed we can determine whether we're fine to go on
	for (i = 0; i < tokens.size(); i++) {
		if (!foundNotes.at(i) && !foundDynamics.at(i) && !foundAccidentals.at(i) && !foundOctaves.at(i)) {
			return std::make_pair(3, tokens.at(i) + (string)": unknown token");
		}
	}

	if (numDurs == 0) {
		return std::make_pair(3, "no durations added");
	}

	// fill in possible empty slots of durations
	for (i = 0; i < tokens.size(); i++) {
		if ((int)i != durIndexes.at(i)) {
			dursData.at(i) = dursData.at(i-1);
		}
	}

	// fill in possible empty slots of dynamics
	if (numDynamics == 0) {
		for (i = 0; i < tokens.size(); i++) {
			// if this is not the very first bar get the last value of the previous bar
			if (sharedData.loopData.size() > 0) {
				int prevBar = getPrevBarIndex();
				// get the last stored dynamic
				dynamicsData.at(i) = sharedData.instruments.at(lastInstrumentIndex).dynamics.at(prevBar).back();
				// do the same for the MIDI velocities
				midiVels.at(i) = sharedData.instruments.at(lastInstrumentIndex).midiVels.at(prevBar).back();
			}
			else {
				// fill in a default value of halfway from min to max dB
				dynamicsData.at(i) = 100-((100-MINDB)/2);
				midiVels.at(i) = 72; // half way between mp and mf in MIDI velocity
			}
		}
	}
	else {
		for (i = 0; i < tokens.size(); i++) {
			// if a dynamic has not been stored, fill in the last
			// stored dynamic in this empty slot
			if ((int)i != dynamicsIndexes.at(i)) {
				// we don't want to add data for the score, only for the sequencer
				if (i > 0) {
					dynamicsData.at(i) =  dynamicsData.at(i-1);
					midiVels.at(i) = midiVels.at(i-1);
				}
				else {
					// fill in a default value of halfway from min to max dB
					dynamicsData.at(i) = 100-((100-MINDB)/2);
					midiVels.at(i) = 72; // half way between mp and mf in MIDI velocity
				}
			}
		}
	}
	// correct dynamics in case of crescendi or decrescendi
	for (i = 0; i < dynamicsRampCounter; i++) {
		int numSteps = dynamicsRampIndexes.at(i).second - dynamicsRampIndexes.at(i).first;
		int valsDiff = dynamicsData.at(dynamicsRampIndexes.at(i).second) - dynamicsData[dynamicsRampIndexes.at(i).first];
		int midiVelsDiff = midiVels.at(dynamicsRampIndexes.at(i).second) - midiVels[dynamicsRampIndexes.at(i).first];
		int step = valsDiff / max(numSteps, 1);
		int midiVelsStep = midiVelsDiff / max(numSteps, 1);
		index2 = 1;
		for (j = dynamicsRampIndexes.at(i).first+1; j < dynamicsRampIndexes.at(i).second; j++) {
			dynamicsData.at(j) += (step * index2);
			midiVels.at(j) += (midiVelsStep * index2);
			index2++;
		}
	}
	// store the next dynamic in case of the dynamics ramps
	// replace all values within a ramp with the dynamic value of the next index
	for (i = 0; i < dynamicsRampCounter; i++) {
		for (j = dynamicsRampIndexes.at(i).first; j < dynamicsRampIndexes.at(i).second; j++) {
			// if a ramp ends at the end of the bar without setting a new dynamic
			// go back to the pre-last dynamic
			if (j == notesData.size()-1) {
				// if there's only one ramp in the bar store the first dynamic
				if (i == 0) {
					dynamicsRampData.at(j) = dynamicsData.at(0);
				}
				else {
					dynamicsRampData.at(j) = dynamicsData.at(dynamicsRampIndexes.at(i-1).first);
				}
			}
			// otherwise store the next dynamic so that the receiving program
			// ramps from its current dynamic to the next one in the duration
			// of the current note (retrieved by the sharedData.durs vector)
			else {
				dynamicsRampData.at(j) = dynamicsData.at(j+1);
			}
		}
	}
	// get the tempo of the minimum duration
	// barIndex has been defined at the beginning of parseMelodicLine() after done expanding commands
	if (sharedData.numerator.find(barIndex) == sharedData.numerator.end()) {
		sharedData.numerator.at(barIndex) = sharedData.denominator.at(barIndex) = 4;
	}
	int dursAccum = 0;
	int diff = 0; // this is the difference between the expected and the actual sum of tuplets
	int scoreDur; // durations for score should not be affected by tuplet calculations
	int halfDur; // for halving for every dot
	// convert durations to number of beats with respect to minimum duration
	for (i = 0; i < tokens.size(); i++) {
		dursData.at(i) = MINDUR / dursData.at(i);
		halfDur = dursData.at(i) / 2;
		if (dotIndexes.at(i) == 1) {
			for (j = 0; j < dotsCounter.at(i); j++) {
				dursData.at(i) += halfDur;
				halfDur /= 2;
			}
		}
		scoreDur = dursData.at(i);
		for (auto it = tupletStartStop.begin(); it != tupletStartStop.end(); ++it) {
			if (i >= it->second.first && i <= it->second.second) {
				// check if truncated sum is less than the expected sum
				if (i == it->second.first) {
					int tupletDurAccum = 0;
					int expectedSum = 0;
					for (j = it->second.first; j <= it->second.second; j++) {
						tupletDurAccum += (dursData.at(i) * tupletRatios.at(it->first).second / tupletRatios.at(it->first).first);
						expectedSum += dursData.at(i);
					}
					expectedSum = expectedSum * tupletRatios.at(it->first).second / tupletRatios.at(it->first).first;
					diff += expectedSum - tupletDurAccum;
				}
				dursData.at(i) = dursData.at(i) * tupletRatios.at(it->first).second / tupletRatios.at(it->first).first;
				if (diff > 0) {
					dursData.at(i)++;
					diff--;
				}
			}
			//break;
		}
		// make sure there's no difference left between the expected duration and the modified duration
		dursData.at(i) += diff;
		diff = 0;
		dursAccum += dursData.at(i);
		dursForScore.at(i) = scoreDur;
		dursDataWithoutSlurs.at(i) = dursData.at(i);
	}
	// if all we have is "r1", then it's a tacet, otherwise, we need to check the durations sum
	if (!(tokens.size() == 1 && tokens.at(0).compare("r1") == 0)) {
		if (dursAccum < sharedData.numBeats.at(barIndex)) {
			return std::make_pair(3, "durations sum is less than bar duration");
		}
		if (dursAccum > sharedData.numBeats.at(barIndex)) {
			return std::make_pair(3, "durations sum is greater than bar duration");
		}
	}
	// now we can create a vector of pairs with the indexes of the slurs
	int numSlurStarts = 0, numSlurStops = 0;
	for (i = 0; i < numNotesVertical; i++) {
		if (slurBeginningsIndexes.at(i) > -1) numSlurStarts++;
		if (slurEndingsIndexes.at(i) > -1) numSlurStops++;
	}
	vector<std::pair<int, int>> slurIndexes (max(max(numSlurStarts, numSlurStops), 1), std::make_pair(-1, -1));
	int slurNdx = 0;
	for (i = 0; i < numNotesVertical; i++) {
		if (slurBeginningsIndexes.at(i) > -1) slurIndexes.at(slurNdx).first = slurBeginningsIndexes.at(i);
		if (slurEndingsIndexes.at(i) > -1) slurIndexes.at(slurNdx).second = slurEndingsIndexes.at(i);
	}
	// once the melodic line has been parsed, we can insert the new data to the maps of the Instrument object
	map<string, int>::iterator it = sharedData.instrumentIndexes.find(lastInstrument);
	if (it == sharedData.instrumentIndexes.end()) {
		return std::make_pair(3, "instrument does not exist");
	}
	int thisInstIndex = it->second;
	sharedData.instruments.at(thisInstIndex).notes[barIndex] = std::move(notesData);
	sharedData.instruments.at(thisInstIndex).midiNotes[barIndex] = std::move(midiNotesData);
	sharedData.instruments.at(thisInstIndex).durs[barIndex] = std::move(dursData);
	sharedData.instruments.at(thisInstIndex).dursWithoutSlurs[barIndex] = std::move(dursDataWithoutSlurs);
	sharedData.instruments.at(thisInstIndex).midiDursWithoutSlurs[barIndex] = std::move(midiDursDataWithoutSlurs);
	sharedData.instruments.at(thisInstIndex).dynamics[barIndex] = std::move(dynamicsData);
	sharedData.instruments.at(thisInstIndex).midiVels[barIndex] = std::move(midiVels);
	sharedData.instruments.at(thisInstIndex).pitchBendVals[barIndex] = std::move(pitchBendVals);
	sharedData.instruments.at(thisInstIndex).dynamicsRamps[barIndex] = std::move(dynamicsRampData);
	sharedData.instruments.at(thisInstIndex).glissandi[barIndex] = std::move(glissandiIndexes);
	sharedData.instruments.at(thisInstIndex).midiGlissDurs[barIndex] = std::move(midiGlissDurs);
	sharedData.instruments.at(thisInstIndex).midiDynamicsRampDurs[barIndex] = std::move(midiDynamicsRampDurs);
	sharedData.instruments.at(thisInstIndex).articulations[barIndex] = std::move(articulationIndexes);
	sharedData.instruments.at(thisInstIndex).midiArticulationVals[barIndex] = std::move(midiArticulationVals);
	sharedData.instruments.at(thisInstIndex).isSlurred[barIndex] = std::move(isSlurred);
	sharedData.instruments.at(thisInstIndex).text[barIndex] = std::move(texts);
	sharedData.instruments.at(thisInstIndex).textIndexes[barIndex] = std::move(textIndexesLocal);
	sharedData.instruments.at(thisInstIndex).slurIndexes[barIndex] = std::move(slurIndexes);
	sharedData.instruments.at(thisInstIndex).tieIndexes[barIndex] = std::move(tieIndexes);
	sharedData.instruments.at(thisInstIndex).scoreNotes[barIndex] = std::move(notesForScore);
	sharedData.instruments.at(thisInstIndex).scoreDurs[barIndex] = std::move(dursForScore);
	sharedData.instruments.at(thisInstIndex).scoreDotIndexes[barIndex] = std::move(dotIndexes);
	sharedData.instruments.at(thisInstIndex).scoreDotsCounter[barIndex] = std::move(dotsCounter);
	sharedData.instruments.at(thisInstIndex).scoreAccidentals[barIndex] = std::move(accidentalsForScore);
	sharedData.instruments.at(thisInstIndex).scoreNaturalSignsNotWritten[barIndex] = std::move(naturalSignsNotWrittenForScore);
	sharedData.instruments.at(thisInstIndex).scoreOctaves[barIndex] = std::move(octavesForScore);
	sharedData.instruments.at(thisInstIndex).scoreOttavas[barIndex] = std::move(ottavas);
	sharedData.instruments.at(thisInstIndex).scoreGlissandi[barIndex] = std::move(glissandiIndexesForScore);
	//sharedData.instruments.at(thisInstIndex).scoreArticulations[barIndex] = std::move(articulationIndexes);
	sharedData.instruments.at(thisInstIndex).scoreDynamics[barIndex] = std::move(dynamicsForScore);
	sharedData.instruments.at(thisInstIndex).scoreDynamicsIndexes[barIndex] = std::move(dynamicsForScoreIndexes);
	sharedData.instruments.at(thisInstIndex).scoreDynamicsRampStart[barIndex] = std::move(dynamicsRampStartForScore);
	sharedData.instruments.at(thisInstIndex).scoreDynamicsRampEnd[barIndex] = std::move(dynamicsRampEndForScore);
	sharedData.instruments.at(thisInstIndex).scoreDynamicsRampDir[barIndex] = std::move(dynamicsRampDirForScore);
	sharedData.instruments.at(thisInstIndex).scoreTupletRatios[barIndex] = std::move(tupletRatios);
	sharedData.instruments.at(thisInstIndex).scoreTupletStartStop[barIndex] = std::move(tupletStartStop);
	sharedData.instruments.at(thisInstIndex).scoreTexts[barIndex] = std::move(textsForScore);
	sharedData.instruments.at(thisInstIndex).isWholeBarSlurred[barIndex] = false;
	// in case we have no slurs, we must check if there is a slurred that is not closed in the previous bar
	// of if the whole previous bar is slurred
	if (slurIndexes.size() == 1 && slurIndexes.at(0).first == -1 && slurIndexes.at(0).second == -1) {
		int prevBar = getPrevBarIndex();
		if ((sharedData.instruments.at(thisInstIndex).slurIndexes.at(prevBar).back().first > -1 && \
				sharedData.instruments.at(thisInstIndex).slurIndexes.at(prevBar).back().second == -1) ||
				sharedData.instruments.at(thisInstIndex).isWholeBarSlurred.at(prevBar)) {
			sharedData.instruments.at(thisInstIndex).isWholeBarSlurred.at(barIndex) = true;
		}
	}
	sharedData.instruments.at(thisInstIndex).setPassed(true);
	return std::make_pair(0, "");
}

//--------------------------------------------------------------
void ofApp::setPaneCoords()
{
	// this is the minimum height of the traceback space
	// used to calculate the size of the panes
	sharedData.tracebackBase = (float)sharedData.screenHeight - cursorHeight;
	// we need the anchor so we can change the traceback height in case the traceback
	// has more than two lines
	// get the width and height for each pane
	// initializing 0 to avoid warning messages during compilation
	float paneWidth = 0, paneHeight = 0;
	// we compute the maximum number of lines for a pane with the height of the window
	int maxNumLines = sharedData.tracebackBase / cursorHeight;
	// we need an even number of lines so that more than one panes in a column can halve this number
	if (maxNumLines % 2) {
		if (maxNumLines > 1) maxNumLines--;
		else maxNumLines++;
	}
	// depending on the orientation, we must first create either the height or the width of the editors
	if (paneSplitOrientation == 0) {
		paneHeight = ((float)maxNumLines * cursorHeight) / (float)numPanes.size();
	}
	else {
		paneWidth = (float)sharedData.screenWidth / (float)numPanes.size();
	}
	int paneNdx = 0;
	for (map<int, int>::iterator it = numPanes.begin(); it != numPanes.end(); ++it) {
		// each row sets the number of lines for its panes according to the number of rows (it->first) for paneSplitOrientation == 0
		// or number of columns (it->second) for paneSplitOrientation == 1
		int divisor = paneSplitOrientation == 0 ? (int)numPanes.size() : it->second;
		int numLines = maxNumLines / (divisor > 0 ? divisor : 1);
		int additionalLines = 0;
		if (numLines * it->second != maxNumLines) additionalLines = maxNumLines - (numLines * it->second);
		if (paneSplitOrientation == 0) {
			paneWidth = sharedData.screenWidth / it->second;
			for (int col = 0; col < it->second; col++) {
				editors[paneNdx].setFrameWidth(paneWidth); // - (float)(lineWidth / 2));
				editors[paneNdx].setFrameXOffset(paneWidth * (float)col);
				editors[paneNdx].setFrameHeight(paneHeight);
				editors[paneNdx].setFrameYOffset(paneHeight * (float)(it->first));
				//if (!col) editors[paneNdx].setMaxNumLines(numLines+additionalLines);
				//else editors[paneNdx].setMaxNumLines(numLines);
				editors[paneNdx].setMaxNumLines(numLines);
				editors[paneNdx].setMaxCharactersPerString();
				paneNdx++;
			}
		}
		else {
			float paneYOffset = 0;
			float prevHeight = 0;
			for (int col = 0; col < it->second; col++) {
				if (col < additionalLines) paneHeight = (numLines + 1) * cursorHeight;
				else paneHeight = numLines * cursorHeight;
				paneYOffset += prevHeight;
				prevHeight = paneHeight;
				editors[paneNdx].setFrameHeight(paneHeight);
				editors[paneNdx].setFrameYOffset(paneYOffset);
				editors[paneNdx].setFrameWidth(paneWidth);
				editors[paneNdx].setFrameXOffset(paneWidth * (float)it->first);
				if (!col) editors[paneNdx].setMaxNumLines(numLines+additionalLines);
				else editors[paneNdx].setMaxNumLines(numLines);
				editors[paneNdx].setMaxCharactersPerString();
				paneNdx++;
			}
		}
	}
	// once all the editors have their dimensions and coordinates set
	// we correct the traceback position so that there is no gap
	// between the last line of an editor and the traceback rectangle
	//maxNumLines = 0;
	//for (map<int, Editor>::iterator it = editors.begin(); it != editors.end(); ++it) {
	//	maxNumLines = max(maxNumLines, it->second.getMaxNumLines());
	//}
	sharedData.tracebackBase = (float)maxNumLines * cursorHeight;;
	sharedData.tracebackYCoord = sharedData.tracebackBase;
}

//--------------------------------------------------------------
void ofApp::setFontSize()
{
	font.load("DroidSansMono.ttf", fontSize);
	cursorHeight = font.stringHeight("q");
	float heightDiff = font.stringHeight("l") - font.stringHeight("o");
	cursorHeight += heightDiff;
	for (map<int, Editor>::iterator it = editors.begin(); it != editors.end(); ++it) {
		it->second.setFontSize(fontSize, cursorHeight);
	}
	setPaneCoords();
}

//--------------------------------------------------------------
CmdOutput ofApp::scoreCommands(vector<string>& originalCommands, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	// the initial command is \score. followed by a second level command
	// to separate the second level command we create a new vector that will copy the originalCommands vector
	// except from the first item where we trim the \score. part
	vector<string> commands;
	for (unsigned i = 0; i < originalCommands.size(); i++) {
		if (!i) {
			// remove the "\score." part of the command so we isolate the second level command
			string thisCommand = originalCommands[i].substr(originalCommands[i].find(".")+1);
			commands.push_back(thisCommand);
		}
		else {
			commands.push_back(originalCommands[i]);
		}
	}

	if (commands[0].compare("play") == 0) {
		if (commands.size() > 1) {
			return genError("\"play\" subcommand takes no arguments");
		}
		// reset animation in case it is on
		if (sharedData.setAnimation) {
			for (map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				it->second.setAnimation(true);
			}
			sharedData.animate = true;
		}
		startSequencer = true;
	}

	else if (commands[0].compare("stop") == 0) {
		if (commands.size() > 1) {
			return genError("\"stop\" subcommand takes no arguments");
		}
		sequencer.stop();
	}

	else if (commands[0].compare("stopnow") == 0) {
		if (commands.size() > 1) {
			return genError("\"stopnow\" subcommand takes no arguments");
		}
		sequencer.stopNow();
		if (sharedData.animate) {
			for (map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				it->second.setAnimation(false);
			}
			sharedData.animate = false;
			sharedData.setAnimation = true;
		}
		ofxOscMessage m;
		m.setAddress("/play");
		m.addIntArg(0);
		sendToParts(m, true);
		m.clear();
	}

	else if (commands[0].compare("reset") == 0) {
		if (commands.size() > 1) {
			return genError("\"reset\" subcommand takes no arguments");
		}
		for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			it->second.resetBarDataCounter();
		}
		sequencer.setSequencerRunning(false);
	}

	else if (commands[0].compare("finish") == 0) {
		if (commands.size() > 1) {
			return genError("\"finish\" subcommand takes no arguments");
		}
		sequencer.setFinish(true);
	}

	else if (commands[0].compare("beatsin") == 0) {
		if (commands.size() < 2) {
			return genError("\"beatsin\" takes a number for beat countdown");
		}
		else if (commands.size() > 2) {
			return genError("\"beatsin\" takes one argument only");
		}
		if (!isNumber(commands[1])) {
			return genError("\"beatsin\" takes a number as an argument");
		}
		int countdown = stoi(commands[1]);
		if (countdown < 0) {
			return genError("can't set a negative countdown");
		}
		sequencer.setCountdown(countdown);
	}

	else if (commands[0].compare("show") == 0) {
		if (commands.size() > 1) {
			if (commands.size() > 2) {
				return genError("second level command \"show\" takes up to one argument, not more");
			}
			if (commands[1].compare("barcount") == 0) {
				showBarCount = true;
			}
			else if (commands[1].compare("tempo") == 0) {
				showTempo = true;
			}
			else if (commands[1].compare("beat") == 0) {
				if (sequencer.isThreadRunning()) {
					sharedData.beatAnimate = true;
				}
				else {
					sharedData.setBeatAnimation = true;
				}
			}
			else {
				return genError("unkown argument");
			}
		}
		else {
			sharedData.showScore = true;
			editors[whichPane].setMaxCharactersPerString();
		}
	}
	else if (commands[0].compare("hide") == 0) {
		if (commands.size() > 1) {
			if (commands.size() > 2) {
				return genError("second level command \"hide\" takes up to one argument, not more");
			}
			if (commands[1].compare("barcount") == 0) {
				showBarCount = false;
			}
			else if (commands[1].compare("tempo") == 0) {
				showTempo = false;
			}
			else if (commands[1].compare("beat") == 0) {
				sharedData.beatAnimate = false;
				sharedData.setBeatAnimation = false;
			}
			else {
				return genError("unkown argument");
			}
		}
		else {
			sharedData.showScore = false;
			editors[whichPane].setMaxCharactersPerString();
		}
	}
	else if (commands[0].compare("animate") == 0) {
		if (commands.size() > 1) {
			return genError("second level command \"animate\" takes no arguments");
		}
		if (sequencer.isThreadRunning()) {
			for (map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				it->second.setAnimation(true);
			}
			sharedData.animate = true;
		}
		else {
			sharedData.setAnimation = true;
		}
	}
	else if (commands[0].compare("inanimate") == 0) {
		if (commands.size() > 1) {
			return genError("second level command \"inanimate\" takes no arguments");
		}
		for (map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			it->second.setAnimation(false);
		}
		sharedData.animate = false;
		sharedData.setAnimation = false;
	}
	else if (commands[0].compare("beattype") == 0) {
		if (commands.size() < 2) {
			return genError("\"beattype\" takes a 1 or a 2 as an argument");
		}
		if (!isNumber(commands[1])) {
			return genError("\"beattype\" takes a 1 or a 2 as an argument");
		}
		int beattype;
		if (!isNumber(commands[1])) {
			vector<string> v = {commands.begin()+1, commands.end()};
			CmdInput cmdInput = genCmdInput(v);
			cmdOutput = parseCommand(cmdInput, lineNum, numLines);
			if (cmdOutput.errorCode > 0) return cmdOutput;
			if (!isNumber(cmdOutput.outputVec[0])) return genError("\"beattype\" takes a number as an argument");
			beattype = stoi(cmdOutput.outputVec[0]);
		}
		else {
			beattype = stoi(commands[1]);
		}
		if (beattype < 1 || beattype > 2) {
			return genError("unknown beat vizualization type");
		}
		sharedData.beatVizType = beattype;
	}
	else if (commands[0].compare("movex") == 0) {
		if (commands.size() < 2) {
			return genError("\"movex\" takes a number as an argument");
		}
		int numPixels;
		if (!isNumber(commands[1])) {
			vector<string> v = {commands.begin()+1, commands.end()};
			cmdOutput = parseCommand(genCmdInput(v), lineNum, numLines);
			if (cmdOutput.errorCode > 0) return cmdOutput;
			if (!isNumber(cmdOutput.outputVec[0])) return genError("\"movex\" takes a number as an argument");
			numPixels = stoi(cmdOutput.outputVec[0]);
		}
		else {
			numPixels = stoi(commands[1]);
		}
		for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			it->second.moveScoreX(numPixels);
		}
	}
	else if (commands[0].compare("movey") == 0) {
		if (commands.size() < 2) {
			return genError("\"movey\" takes a number as an argument");
		}
		int numPixels;
		if (!isNumber(commands[1])) {
			vector<string> v = {commands.begin()+1, commands.end()};
			cmdOutput = parseCommand(genCmdInput(v), lineNum, numLines);
			if (cmdOutput.errorCode > 0) return cmdOutput;
			if (!isNumber(cmdOutput.outputVec[0])) return genError("\"movey\" takes a number as an argument");
			numPixels = stoi(cmdOutput.outputVec[0]);
		}
		else {
			numPixels = stoi(commands[1]);
		}
		for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			it->second.moveScoreY(numPixels);
		}
	}
	//else if (commands[0].compare("recenter") == 0) {
	//	if (commands.size() > 1) {
	//		return std::make_pair(3, "\"recenter\" takes no arguments");
	//	}
	//	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
	//		it->second.recenterScore();
	//	}
	//	return std::make_pair(0, "");
	//}
	else if (commands[0].compare("numbars") == 0) {
		if (commands.size() < 2) {
			return genError("\"numbars\" takes a number as an argument");
		}
		int numBarsLocal;
		if (!isNumber(commands[1])) {
			vector<string> v = {commands.begin()+1, commands.end()};
			cmdOutput = parseCommand(genCmdInput(v), lineNum, numLines);
			if (cmdOutput.errorCode > 0) return cmdOutput;
			if (!isNumber(cmdOutput.outputVec[0])) return genError("\"numbars\" takes a number as an argument");
			numBarsLocal = stoi(cmdOutput.outputVec[0]);
		}
		else {
			numBarsLocal = stoi(commands[1]);
		}
		if (numBarsLocal < 1) {
			return genError("number of bars to display must be 1 or greater");
		}
		numBarsToDisplay = numBarsLocal;
		for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			it->second.setNumBarsToDisplay(numBarsToDisplay);
		}
	}
	else if (commands[0].compare("update") == 0) {
		if (commands.size() != 2) {
			return genError("\"update\" takes one argument, \"onlast\" or \"immediately\"");
		}
		if (commands[1].compare("onlast") == 0) {
			scoreChangeOnLastBar = true;
		}
		else if (commands[1].compare("immediately") == 0) {
			scoreChangeOnLastBar = false;
		}
		else {
			return genError(commands[1] + (string)"unknown argument to \"update\", must be onlast or immediately");
		}
	}
	else if (commands[0].compare("correct") == 0) {
		if (commands.size() != 2) {
			return genError("\"correct\" takes one argument, \"onoctave\" or \"all\"");
		}
		if (commands[1].compare("onoctave") == 0) {
			correctOnSameOctaveOnly = true;
		}
		else if (commands[1].compare("all") == 0) {
			correctOnSameOctaveOnly = false;
		}
		else {
			return genError( commands[1] + (string)": unknown argument to \"correct\"");
		}
	}
	else {
		return genError("unkown second level command");
	}
	return cmdOutput;
}

//--------------------------------------------------------------
CmdOutput ofApp::maestroCommands(vector<string>& commands, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	for (unsigned i = 1; i < commands.size(); i++) {
		if (commands[i].compare("address") == 0) {
			if (commands.size() < i + 2) {
				return genError("\\maestro address takes one argument, the OSC address to receive sensor data");
			}
			maestroAddress = commands[i+1];
			if (!maestroToggleSet) receivingMaestro = true;
			if (!oscReceiverIsSet) {
				oscReceiver.setup(OSCPORT);
				oscReceiverIsSet = true;
			}
			i++;
		}

		else if (commands[i].compare("toggle") == 0) {
			if (commands.size() < i + 2) {
				return genError("\\maestro toggle takes one argument, the OSC address to receive a 0 or 1");
			}
			maestroToggleAddress = commands[i+1];
			maestroToggleSet = true;
			i++;
		}
		
		else if (commands[i].compare("reset") == 0) {
			if (commands.size() < i + 2) {
				return genError("\\maestro reset takes one argument, the OSC address to receive sensor data");
			}
			maestroResetAddress = commands[i+1];
			i++;
		}

		else if (commands[i].compare("valndx") == 0) {
			if (commands.size() < i + 2) {
				return genError("\\maestro valndx takes one argument, the index of the OSC data packet");
			}
			if (!isNumber(commands[i+1])) {
				return genError("argument to \\maestro valndx must be a number");
			}
			int val = stoi(commands[i+1]);
			if (val < 0) {
				return genError("OSC data packet index must be positive");
			}
			maestroValNdx = val;
			i++;
		}

		else if (commands[i].compare("valthresh") == 0) {
			if (commands.size() < i + 2) {
				return genError("\\maestro valthresh takes one argument, the threshold for triggering beats");
			}
			if (!isFloat(commands[i+1])) {
				return genError("argument to \\maestro valthresh must be a number");
			}
			float val = stof(commands[i+1]);
			if (val < 0) {
				return genError("maestro beat thresh must be positive");
			}
			maestroValThresh = val;
			i++;
		}
	}
	return cmdOutput;
}

//--------------------------------------------------------------
void ofApp::initializeInstrument(string instName)
{
	int maxInstIndex = (int)sharedData.instruments.size();
	sharedData.instrumentIndexes[instName] = maxInstIndex;
	sharedData.instruments[maxInstIndex] = Instrument();
	sharedData.instruments[maxInstIndex].setID(maxInstIndex); // this is for debugging the Notes()
	sharedData.instruments[maxInstIndex].setName(instName.substr(1)); // remove the back slash at the beginning
	sharedData.instruments[maxInstIndex].setMute(false);

	// score part OSC stuff
	sharedData.instruments[maxInstIndex].sendToPart = false;

	int thisInstNameWidth = instFont.stringWidth(instName.substr(1));
	instNameWidths[maxInstIndex] = thisInstNameWidth;
	if (thisInstNameWidth > sharedData.longestInstNameWidth) {
		sharedData.longestInstNameWidth = thisInstNameWidth;
	}
	sharedData.instruments[maxInstIndex].setNotesFontSize(sharedData.scoreFontSize, sharedData.staffLinesDist);
	sharedData.instrumentIndexesOrdered[maxInstIndex] = maxInstIndex;
	sharedData.numInstruments++;
	keywords.push_back(instName);
	setScoreCoords();
}

//--------------------------------------------------------------
void ofApp::setScoreCoords()
{
	scoreXStartPnt = sharedData.longestInstNameWidth + (sharedData.blankSpace * 1.5);
	float staffLength = scoreBackgroundWidth - sharedData.blankSpace - scoreXStartPnt;
	if (scoreOrientation > 0) staffLength /= min(2, numBarsToDisplay);
	sharedData.notesXStartPnt = 0;
	notesLength = staffLength - (sharedData.staffLinesDist * NOTESXOFFSETCOEF);
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		it->second.setStaffCoords(staffLength, sharedData.staffLinesDist);
		it->second.setNoteCoords(notesLength, sharedData.staffLinesDist, sharedData.scoreFontSize);
	}
	// get the note width from the first instrument, as they all have the same size
	if (sharedData.instruments.size() > 0) {
		sharedData.noteWidth = sharedData.instruments[0].getNoteWidth();
		sharedData.noteHeight = sharedData.instruments[0].getNoteHeight();
	}
}

//--------------------------------------------------------------
void ofApp::setScoreNotes(int barIndex)
{
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		it->second.setScoreNotes(barIndex, sharedData.numerator[barIndex],
								 sharedData.denominator[barIndex], sharedData.numBeats[barIndex],
								 sharedData.BPMTempi[barIndex], sharedData.tempoBaseForScore[barIndex],
								 sharedData.BPMDisplayHasDot[barIndex], sharedData.BPMMultiplier[barIndex]);
	}
}

//--------------------------------------------------------------
void ofApp::setNotePositions(int barIndex)
{
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		if (!it->second.getCopied(barIndex)) it->second.setNotePositions(barIndex);
	}
}

//--------------------------------------------------------------
void ofApp::setNotePositions(int barIndex, int numBars)
{
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		if (!it->second.getCopied(barIndex)) it->second.setNotePositions(barIndex, numBars);
	}
}

//--------------------------------------------------------------
void ofApp::calculateStaffPositions(int bar, bool windowChanged)
{
	float maxHeightSum = 0;
	float lastMaxPosition = 0;
	float firstMinPosition = 0;
	int i = 0;
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		float maxPosition = it->second.getMaxYPos(bar);
		float minPosition = it->second.getMinYPos(bar);
		if (i == 0 && it->first == 0) {
			firstMinPosition = minPosition;
		}
		float diff = maxPosition - minPosition;
		if (diff > maxHeightSum) {
			maxHeightSum = diff;
		}
		lastMaxPosition = maxPosition;
		i++;
	}
	// if the current bar is wider than the existing ones, we need to change the current Y coordinates
	if (maxHeightSum >= sharedData.allStaffDiffs || windowChanged) {
		sharedData.allStaffDiffs = min(maxHeightSum, (sharedData.tracebackBase - cursorHeight) / (float)sharedData.instruments.size());
		float totalDiff = lastMaxPosition - firstMinPosition;
		for (int i = 0; i < 2; i++) {
			float heightCompare = sharedData.tracebackBase - cursorHeight;
			if (i == 1) {
				heightCompare = (sharedData.tracebackBase - cursorHeight) / 2;
			}
			if (totalDiff < heightCompare) {
				sharedData.yStartPnts[i] = ((sharedData.tracebackBase - cursorHeight) / (2*(i+1)));
				sharedData.yStartPnts[i] -= (((sharedData.allStaffDiffs + (sharedData.staffLinesDist * 2)) * \
							(sharedData.numInstruments / 2)));
				sharedData.yStartPnts[i] -= (sharedData.staffLinesDist * 2);
				if ((sharedData.instruments.size() % 2) == 0) {
					sharedData.yStartPnts[i] += ((sharedData.allStaffDiffs + (sharedData.staffLinesDist * 2)) / 2); 
				}
				// make sure anything that sticks out of the staff doesn't go below 0 in the Y axis
				if ((sharedData.yStartPnts[i] - firstMinPosition) < 0) {
					sharedData.yStartPnts[i] += firstMinPosition;
				}
			}
			else {
				// if the full score exceeds the screen just place it 10 pixels below the top part of the window
				sharedData.yStartPnts[i] = firstMinPosition + 10;
			}
		}
		sharedData.yStartPnts[2] = sharedData.yStartPnts[0];
	}
}

//--------------------------------------------------------------
void ofApp::swapScorePosition(int orientation)
{
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		it->second.setScoreOrientation(orientation);
	}
	bool setCoords = (scoreOrientation != orientation ? true : false);
	scoreOrientation = orientation;
	switch (orientation) {
		case 0:
			scoreYOffset = 0;
			scoreBackgroundWidth = sharedData.screenWidth / 2;
			scoreBackgroundHeight = sharedData.tracebackBase - cursorHeight;
			if (scoreXOffset == 0) scoreXOffset = sharedData.screenWidth / 2;
			else scoreXOffset = 0;
			break;
		case 1:
			scoreXOffset = 0;
			scoreBackgroundWidth = sharedData.screenWidth;
			scoreBackgroundHeight = (sharedData.tracebackBase / 2) - cursorHeight;
			// in horizontal view of the score, the height of the score background is rounded half the editor lines
			// this is because the number of lines of an editor is always odd, so that the line with the file name
			// and pane index is also included, which makes the total number of lines even
			// when we view the score horizontally, the background should match an exact number of lines
			// and since the number of total lines is odd, we make this background one less
			// e.g. for 37 lines, the score background will be 18 lines, not 19, or 18.5
			// but when viewed on the lower side, it should touch the white line of the editor
			// hence the scoreYOffset is not the same as the scoreBackgroundHeith
			// as is the case with scoreXOffset and scoreBarckgroundWidth, in the case above
			if (scoreYOffset == 0) scoreYOffset = (sharedData.tracebackBase / 2);
			else scoreYOffset = 0;
			break;
		case 2:
			scoreXOffset = 0;
			scoreYOffset = 0;
			scoreBackgroundWidth = sharedData.screenWidth;
			scoreBackgroundHeight = sharedData.tracebackBase - cursorHeight;
		default:
			break;
	}
	if (setCoords) setScoreCoords();
}

//--------------------------------------------------------------
void ofApp::setScoreSizes()
{
	// for a 35 font size the staff lines distance is 10
	sharedData.longestInstNameWidth = 0;
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		it->second.setNotesFontSize(sharedData.scoreFontSize, sharedData.staffLinesDist);
		size_t nameWidth = instFont.stringWidth(it->second.getName());
		instNameWidths[it->first] = nameWidth;
		if ((int)nameWidth > sharedData.longestInstNameWidth) {
			sharedData.longestInstNameWidth = nameWidth;
		}
	}
	scoreXStartPnt = sharedData.longestInstNameWidth + (sharedData.blankSpace * 1.5);
	setScoreCoords();
	if (sharedData.barsIndexes.size() > 0) {
		for (auto it = sharedData.barsIndexes.begin(); it != sharedData.barsIndexes.end(); ++it) {
			for (auto it2 = sharedData.instruments.begin(); it2 != sharedData.instruments.end(); ++it2) {
				it2->second.setMeter(it->second, sharedData.numerator[it->second],
						sharedData.denominator[it->second], sharedData.numBeats[it->second]);
				it2->second.setNotePositions(it->second);
			}
		}
	}
}

//--------------------------------------------------------------
void ofApp::exit()
{
	// should print a warning message first
	sequencer.stop();
	ofxOscMessage m;
	m.setAddress("/exit");
	m.addIntArg(1);
	sendToParts(m, false);
	m.clear();
	for (unsigned i = 0; i < sequencer.midiOuts.size(); i++) {
		sequencer.midiOuts[i].closePort();
	}
	if (serialPortOpen) {
		serial.close();
	}
	ofExit();
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y)
{

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button)
{

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button)
{

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button)
{

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y)
{

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y)
{

}

//--------------------------------------------------------------
void ofApp::resizeWindow()
{
	//sharedData.mutex.lock();
	setPaneCoords();
	if (scoreOrientation == 0) {
		scoreBackgroundWidth = sharedData.screenWidth / 2;
		scoreBackgroundHeight = sharedData.tracebackBase - cursorHeight;
		if (scoreXOffset > 0) scoreXOffset = sharedData.screenWidth / 2;
	}
	else {
		scoreBackgroundWidth = sharedData.screenWidth;
		scoreBackgroundHeight = (sharedData.tracebackBase / 2) - cursorHeight;
		if (scoreYOffset > 0) scoreYOffset = (sharedData.tracebackBase / 2);
	}
	setScoreSizes();
	calculateStaffPositions(getLastBarIndex(), true);
	//sharedData.mutex.unlock();
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h)
{
	// hold the previous middle of screen value so the score is also properly updated
	int prevMiddleOfScreenX = sharedData.middleOfScreenX;
	sharedData.screenWidth = w;
	sharedData.screenHeight = h;
	sharedData.middleOfScreenX = sharedData.screenWidth / 2;
	sharedData.middleOfScreenY = sharedData.screenHeight / 2;
	// once the screen coordinates have been updated, update the score coordinates too
	if (sharedData.staffXOffset == prevMiddleOfScreenX) {
		sharedData.staffXOffset = 0;
		sharedData.staffWidth = sharedData.screenWidth / 2;
	}
	else {
		sharedData.staffXOffset = sharedData.middleOfScreenX;
		sharedData.staffWidth = sharedData.screenWidth;
	}
	windowResizeTimeStamp = ofGetElapsedTimeMillis();
	isWindowResized = true;
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg)
{

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo)
{

}
