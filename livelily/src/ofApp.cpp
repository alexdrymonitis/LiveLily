#include "ofApp.h"
#include <string.h>
#include <time.h> // to give random seed to srand()
#include <sstream>
#include <ctype.h> // to add isdigit()
#include <algorithm> // for replacing characters in a string
#include <cmath> // to add abs()
#include <utility> // to add pair and make_pair

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
void Sequencer::tempoUpdate()
{
	updateTempo = true;
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
void Sequencer::sendBeatVizInfo()
{
	// calculate the time stamp regardless of whether we're showing the beat
	// or not in the editor, in case we have connected score parts
	sharedData->beatVizTimeStamp = ofGetElapsedTimeMillis();
	sharedData->beatVizStepsPerMs = (float)BEATVIZBRIGHTNESS / ((float)sharedData->tempoMs / 4.0);
	sharedData->beatVizRampStart = sharedData->tempoMs / 4;
	if (sharedData->beatAnimate) {
		sharedData->beatViz = true;
	}
	// notify all score parts of the time stamp and the beat visualization counter
	ofxOscMessage m;
	m.setAddress("/beatinfo");
	m.addFloatArg(sharedData->beatVizStepsPerMs);
	m.addIntArg((int)sharedData->beatVizRampStart);
	((ofApp*)ofGetAppPtr())->sendToParts(m);
	m.clear();
	m.setAddress("/beat");
	m.addIntArg(sharedData->beatCounter);
	((ofApp*)ofGetAppPtr())->sendToParts(m);
	m.clear();
}

//--------------------------------------------------------------
float Sequencer::midiToFreq(int midiNote)
{
	return pow(2.0, ((midiNote - 69.0) / 12.0)) * midiTuneVal;
}

//--------------------------------------------------------------
void Sequencer::threadedFunction()
{
	while (isThreadRunning()) {
		timer.waitNext();
		if (runSequencer && !sequencerRunning) {
			sharedData->mutex.lock();
			int bar = sharedData->loopData[sharedData->loopIndex][sharedData->thisLoopIndex];
			sharedData->tempo = sharedData->newTempo;
			// in here we calculate the number of beats on a microsecond basis
			numBeats = sharedData->numBeats[bar] * sharedData->tempo;
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
			sharedData->beatVizStepsPerMs = (float)BEATVIZBRIGHTNESS / ((float)sharedData->tempoMs / 4.0);
			sharedData->beatVizRampStart = sharedData->tempoMs / 4;
			((ofApp*)ofGetAppPtr())->sendFinishToParts(false);
			ofxOscMessage m;
			m.setAddress("/beatinfo");
			m.addFloatArg(sharedData->beatVizStepsPerMs);
			m.addIntArg(sharedData->beatVizRampStart);
			((ofApp*)ofGetAppPtr())->sendToParts(m);
			m.clear();
			sequencerRunning = true;
			runSequencer = false;
			if (sendMidiClock) midiOut.sendMidiByte(onNextStartMidiByte);
			firstIter = true;
			endOfBar = false;
			for (auto instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
				instMapIt->second.initSeqToggle();
				instMapIt->second.setFirstIter(true);
			}
			sharedData->mutex.unlock();
		}
	
		if (sequencerRunning) {
			sharedData->mutex.lock();
			uint64_t timeStamp = ofGetElapsedTimeMicros();
			int bar = sharedData->loopData[sharedData->loopIndex][sharedData->thisLoopIndex];
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
			int compareVal = sharedData->tempoMs * 1000; // in microseconds
			if (diff >= compareVal || firstIter) {
				if (finished) {
					for (auto func = sharedData->functions.begin(); func != sharedData->functions.end(); ++func) {
						// binding a function to the number of instruments + 2, binds to the end
						if (func->second.getBoundInst() == (int)sharedData->instruments.size() + 2 && sharedData->beatCounter == 0) {
							((ofApp*)ofGetAppPtr())->parseCommand(func->second.getName(), 1, 1);
						}
					}
					finished = false;
					stopNow();
					sharedData->mutex.unlock();
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
					((ofApp*)ofGetAppPtr())->sendToParts(m);
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
					}
					// after checking if we must update the indexes, increment them
					if (!countdown) {
						thisLoopIndex++;
						if (thisLoopIndex >= sharedData->loopData[sharedData->loopIndex].size()) {
							thisLoopIndex = 0;
							// when we start a loop from its beginning, we must check if an instrument needs to be muted
							muteChecked = false;
						}
						// increment the bar counter which is used for display on the score only
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
							((ofApp*)ofGetAppPtr())->parseCommand(func->second.getName(), 1, 1);
						}
					}
					if (!(sharedData->beatCounter == 0 && mustStop)) sendBeatVizInfo();
					endOfBar = true;
					if (countdownCounter == 0) {
						countdown = false;
						sharedData->beatCounter = 0;
					}
				}
				else {
					sharedData->beatCounter = beatCounter;
					beatCounter++;
					if (beatCounter >= sharedData->numerator[bar]) beatCounter = 0;
					sendBeatVizInfo();
					for (auto func = sharedData->functions.begin(); func != sharedData->functions.end(); ++func) {
						// binding a function to the number of instruments + 1, binds to the start of each bar
						if (func->second.getBoundInst() == (int)sharedData->instruments.size() + 1 && sharedData->beatCounter == 0) {
							((ofApp*)ofGetAppPtr())->parseCommand(func->second.getName(), 1, 1);
						}
						// binding to the number of instruments, binds a function to the pulse of the beat
						else if (func->second.getBoundInst() == (int)sharedData->instruments.size()) { 
							((ofApp*)ofGetAppPtr())->parseCommand(func->second.getName(), 1, 1);
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
					}
				}
			}
			if (!countdown) {
				// send MIDI clock, if set
				// need to calculate the number of steps
				uint64_t midiClockTimeStamp = ofGetElapsedTimeMicros();
				if (sendMidiClock && \
						(midiClockTimeStamp - sharedData->PPQNTimeStamp) >= sharedData->PPQNPerUs && \
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
					if (instMapIt->second.hasNotesInBar(bar)) {
						if (instMapIt->second.mustFireStep(timeStamp, bar, sharedData->tempo)) {
							// if we have a note, not a rest, and the current instrument is not muted
							if (instMapIt->second.hasNotesInStep(bar) && !instMapIt->second.isMuted()) {
								if (!instMapIt->second.isMidi()) { // send OSC message
									// if the toggle is on and the note is not slurred, send a note off
									if (instMapIt->second.getSeqToggle() && !instMapIt->second.isNoteSlurred(bar, instMapIt->second.getBarDataCounter())) {
										ofxOscMessage m;
										m.setAddress("/" + instMapIt->second.getName() + "/dynamics");
										m.addIntArg(0);
										oscSender.sendMessage(m, false);
										m.clear();
									}
									else {
										ofxOscMessage m;
										m.setAddress("/" + instMapIt->second.getName() + "/articulation");
										m.addStringArg(sharedData->articulSyms[instMapIt->second.getArticulationIndex(bar)]);
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
											m.addFloatArg(midiToFreq(*it));
										}
										oscSender.sendMessage(m, false);
										m.clear();
										
										m.setAddress("/" + instMapIt->second.getName() + "/dynamics");
										m.addIntArg(instMapIt->second.getDynamic(bar));
										oscSender.sendMessage(m, false);
										m.clear();
									}
								}
								// sending MIDI
								else {
									if (instMapIt->second.getSeqToggle() && !instMapIt->second.isNoteSlurred(bar, instMapIt->second.getBarDataCounter())) {
										for (auto it = instMapIt->second.midiNotes[bar][instMapIt->second.getBarDataCounter()].begin(); it != instMapIt->second.midiNotes[bar][instMapIt->second.getBarDataCounter()].end(); ++it) {
											midiOut.sendNoteOff(instMapIt->second.getMidiChan(), *it, 0);
										}
									}
									else {
										midiOut.sendPitchBend(instMapIt->second.getMidiChan(), instMapIt->second.getPitchBendVal(bar));
										midiOut.sendProgramChange(instMapIt->second.getMidiChan(), instMapIt->second.getProgramChangeVal(bar));
										for (auto it = instMapIt->second.midiNotes[bar][instMapIt->second.barDataCounter].begin(); it != instMapIt->second.midiNotes[bar][instMapIt->second.barDataCounter].end(); ++it) {
											midiOut.sendNoteOn(instMapIt->second.getMidiChan(), *it, instMapIt->second.getMidiVel(bar));
										}
										// if the previous note was slurred, send the note off message after the note on of the new note
										if (instMapIt->second.isNoteSlurred(bar, instMapIt->second.getBarDataCounter()-1)) {
											for (auto it = instMapIt->second.midiNotes[bar][instMapIt->second.getBarDataCounter()-1].begin(); it != instMapIt->second.midiNotes[bar][instMapIt->second.getBarDataCounter()-1].end(); ++it) {
												midiOut.sendNoteOff(instMapIt->second.getMidiChan(), *it, 0);
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
											((ofApp*)ofGetAppPtr())->parseCommand(func->second.getName(), 1, 1); // dummy 2nd and 3rd arguments
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
				else {
					//for (auto instMapIt = sharedData->instruments.begin(); instMapIt != sharedData->instruments.end(); ++instMapIt) {
					//	instMapIt->second.resetBarDataCounter();
					//}
					if (updateTempo) {
						sharedData->tempo = sharedData->newTempo;
						numBeats = sharedData->numBeats[bar] * sharedData->tempo;
						for (auto it = sharedData->instruments.begin(); it != sharedData->instruments.end(); ++it) {
							// we set it to true here so they are updated at the first beat of the next bar
							it->second.setUpdateTempo(true);
						}
						updateTempo = false;
					}
				}
			}
			sharedData->mutex.unlock();
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
	numBarsParsed = 0;
	firstInstForBarsIndex = 0;
	firstInstForBarsSet = false;
	waitToSendBarDataToParts = false;
	sendBarDataToPartsBool = false;
	sendBarDataToPartsCounter = 0;
	barDataOKFromPartsCounter = 0;
	checkIfAllPartsReceivedBarData = false;
	lastFunctionIndex = 0;
	fullScreen = false;
	isWindowResized = false;
	windowResizeTimeStamp = 0;
	
	ofSetFrameRate(framerate);
	lineWidth = 2;
	ofSetLineWidth(lineWidth);
	
	ofSetWindowTitle("LiveLily");
	
	oscReceiver.setup(OSCPORT);
	// waiting time to receive a response from a score part server is one frame at 60 FPS
	scorePartResponseTime = (uint64_t)((1000.0 / 60.0) + 0.5);
	
	midiPortOpen = false;
	
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
	changeFontSizeForAllPanes();
	screenSplitHorizontal = sharedData.screenWidth;
	
	sharedData.tempoMs = 500;
	sharedData.numerator[0] = 4;
	sharedData.denominator[0] = 4;
	sharedData.numBeats[0] = 4;
	sharedData.tempNumBeats = 4;
	
	barError = false;
	inserting = false;
	
	sharedData.loopIndex = 0;
	sharedData.tempBarLoopIndex = 0;
	sharedData.thisLoopIndex = 0;
	sharedData.beatCounter = 0; // this is controlled by the sequencer
	sharedData.barCounter = 0; // also controlled by the sequencer
	showBarCount = false;
	
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
	sharedData.beatVizStepsPerMs = (float)BEATVIZBRIGHTNESS / ((float)sharedData.tempoMs / 4.0);
	sharedData.beatVizRampStart = sharedData.tempoMs / 4;
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

	correctOnSameOctaveOnly = true;
	
	sharedData.PPQN = 24;
	sharedData.PPQNCounter = 0;
	// get the ms duration of the PPQM
	sharedData.PPQNPerUs = (uint64_t)(sharedData.tempoMs / (float)sharedData.PPQN) * 1000;
	
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

	/* Colors used for syntax highlighting
	   basic commands - fuchsia
	   secondary commands - pink 
	   instruments - greenYellow
	   bars - orchid
	   loops - lightSeaGreen
	   functions - turqoise
	   command execution - cyan
	   errors - red
	   warnings - orange
	   brackets pairing - magenta
	   comments - gray
	   digits - gold
	   text highlight box - goldenRod
	   -- paleGreen is used for the pulse of the beat
	*/
	commands_map[livelily]["\\bar"] = ofColor::fuchsia;
	commands_map[livelily]["\\loop"] = ofColor::fuchsia;
	commands_map[livelily]["\\bars"] = ofColor::fuchsia;
	commands_map[livelily]["\\function"] = ofColor::fuchsia;
	commands_map[livelily]["\\play"] = ofColor::fuchsia;
	commands_map[livelily]["\\stop"] =  ofColor::fuchsia;
	commands_map[livelily]["\\tempo"] = ofColor::fuchsia;
	commands_map[livelily]["\\solo"] = ofColor::fuchsia;
	commands_map[livelily]["\\solonow"] = ofColor::fuchsia;
	commands_map[livelily]["\\mute"] =  ofColor::fuchsia;
	commands_map[livelily]["\\unmute"] =  ofColor::fuchsia;
	commands_map[livelily]["\\mutenow"] =  ofColor::fuchsia;
	commands_map[livelily]["\\unmutenow"] = ofColor::fuchsia;
	commands_map[livelily]["\\change"] = ofColor::fuchsia;
	commands_map[livelily]["\\rest"] =  ofColor::fuchsia;
	commands_map[livelily]["\\cursor"] =  ofColor::fuchsia;
	commands_map[livelily]["\\score"] =  ofColor::fuchsia;
	commands_map[livelily]["\\insts"] =  ofColor::fuchsia;
	commands_map[livelily]["\\finish"] =  ofColor::fuchsia;
	commands_map[livelily]["\\reset"] =  ofColor::fuchsia;
	commands_map[livelily]["\\fromosc"] =  ofColor::fuchsia;
	commands_map[livelily]["\\listmidiports"] = ofColor::fuchsia;
	commands_map[livelily]["\\openmidiport"] = ofColor::fuchsia;
	commands_map[livelily]["\\mididur"] = ofColor::fuchsia;
	commands_map[livelily]["\\midistaccato"] = ofColor::fuchsia;
	commands_map[livelily]["\\midistaccatissimo"] = ofColor::fuchsia;
	commands_map[livelily]["\\miditenuto"] = ofColor::fuchsia;
	commands_map[livelily]["\\midiclock"] = ofColor::fuchsia;
	commands_map[livelily]["\\dur"] = ofColor::fuchsia;
	commands_map[livelily]["\\staccato"] = ofColor::fuchsia;
	commands_map[livelily]["\\staccatissimo"] = ofColor::fuchsia;
	commands_map[livelily]["\\tenuto"] = ofColor::fuchsia;
	commands_map[livelily]["\\listserialports"] = ofColor::fuchsia;
	commands_map[livelily]["\\openserialport"] = ofColor::fuchsia;
	commands_map[livelily]["\\time"] = ofColor::fuchsia;
	commands_map[livelily]["\\print"] = ofColor::fuchsia;
	commands_map[livelily]["\\osc"] = ofColor::fuchsia;
	commands_map[livelily]["\\oscsend"] = ofColor::fuchsia;
	commands_map[livelily]["\\beatcount"] = ofColor::fuchsia;
	commands_map[livelily]["\\rand"] = ofColor::fuchsia;
	commands_map[livelily]["\\barnum"] = ofColor::fuchsia;
	commands_map[livelily]["\\list"] = ofColor::fuchsia;
	commands_map[livelily]["\\ottava"] = ofColor::fuchsia;
	commands_map[livelily]["\\ott"] = ofColor::fuchsia;
	commands_map[livelily]["\\barcount"] = ofColor::fuchsia;
	commands_map[livelily]["\\beatsin"] = ofColor::fuchsia;
	commands_map[livelily]["\\instorder"] = ofColor::fuchsia;
	commands_map[livelily]["\\miditune"] = ofColor::fuchsia;
	commands_map[livelily]["\\tuplet"] = ofColor::fuchsia;
	commands_map[livelily]["\\tup"] = ofColor::fuchsia;
	
	commands_map_second[livelily]["clef"] = ofColor::pink;
	commands_map_second[livelily]["rhythm"] = ofColor::pink;
	commands_map_second[livelily]["bind"] = ofColor::pink;
	commands_map_second[livelily]["unbind"] = ofColor::pink;
	commands_map_second[livelily]["onrelease"] = ofColor::pink;
	commands_map_second[livelily]["release"] = ofColor::pink;
	commands_map_second[livelily]["beat"] = ofColor::pink;
	commands_map_second[livelily]["barstart"] = ofColor::pink;
	commands_map_second[livelily]["sendto"] = ofColor::pink;
	commands_map_second[livelily]["midichan"] = ofColor::pink;
	commands_map_second[livelily]["show"] = ofColor::pink;
	commands_map_second[livelily]["hide"] = ofColor::pink;
	commands_map_second[livelily]["animate"] = ofColor::pink;
	commands_map_second[livelily]["inanimate"] = ofColor::pink;
	commands_map_second[livelily]["showbeat"] = ofColor::pink;
	commands_map_second[livelily]["hidebeat"] = ofColor::pink;
	commands_map_second[livelily]["beattype"] = ofColor::pink;
	commands_map_second[livelily]["recenter"] =  ofColor::pink;
	commands_map_second[livelily]["numbars"] =  ofColor::pink;
	commands_map_second[livelily]["all"] =  ofColor::pink;
	commands_map_second[livelily]["framerate"] =  ofColor::pink;
	commands_map_second[livelily]["fr"] =  ofColor::pink;
	commands_map_second[livelily]["traverse"] =  ofColor::pink;
	commands_map_second[livelily]["fullscreen"] =  ofColor::pink;
	commands_map_second[livelily]["cursor"] =  ofColor::pink;
	commands_map_second[livelily]["transpose"] =  ofColor::pink;
	commands_map_second[livelily]["update"] =  ofColor::pink;
	commands_map_second[livelily]["onlast"] =  ofColor::pink;
	commands_map_second[livelily]["immediately"] =  ofColor::pink;
	commands_map_second[livelily]["correct"] =  ofColor::pink;
	commands_map_second[livelily]["onoctave"] =  ofColor::pink;
	commands_map_second[livelily]["correct"] =  ofColor::pink;
}

//--------------------------------------------------------------
void ofApp::update()
{
	sharedData.mutex.lock();
	bool mutexLocked = true;
	// if there are any error messages already received by a server
	// resend the bar data
	if (scorePartErrors.size() > 0) {
		for (unsigned i = 0; i < scorePartErrors.size(); i++) {
			cout << "resending bar " << scorePartErrors[i].second << " to " << sharedData.instruments[scorePartErrors[i].first].getName() << endl;
			// scorePartErrors is a vector of pairs, which consists of the instrument index and the bar index
			if (sharedData.instruments[scorePartErrors[i].first].getCopied(scorePartErrors[i].second)) {
				sendCopyToPart(scorePartErrors[i].first, scorePartErrors[i].second, sharedData.instruments[scorePartErrors[i].first].getCopyNdx(scorePartErrors[i].second));
			}
			else {
				sendNewBarToPart(scorePartErrors[i].first, sharedData.loopsUnordered[scorePartErrors[i].second], scorePartErrors[i].second);
				sendBarDataToPart(scorePartErrors[i].first, scorePartErrors[i].second);
			}
			sendLoopDataToPart(scorePartErrors[i].first, scorePartErrors[i].second, sharedData.loopData[scorePartErrors[i].second]);
		}
		scorePartErrors.clear();
	}
	// check if enought time has passed since we sent all bar data to parts
	// and if it has, check whether all parts have reported that they have received the data without errors
	if (checkIfAllPartsReceivedBarData && ofGetElapsedTimeMillis() - sendBarDataToPartsTimeStamp > SENDBARDATA_WAITDUR) {
		cout << "failure time has passed, checking which insts failed to receive data\n";
		// resend data to parts that haven't reported success through the /barok OSC address
		for (unsigned i = 0; i < sharedData.loopData[sharedData.tempBarLoopIndex].size(); i++) {
			for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				if (it->second.sendToPart) {
					if (it->second.barDataOKFromParts.find(sharedData.loopData[sharedData.tempBarLoopIndex][i]) == it->second.barDataOKFromParts.end()) {
						cout << it->second.getName() << " didn't report success, resending data for bar " << sharedData.loopData[sharedData.tempBarLoopIndex][i] << endl;
						if (it->second.getCopied(sharedData.loopData[sharedData.tempBarLoopIndex][i])) {
							sendCopyToPart(it->first, sharedData.loopData[sharedData.tempBarLoopIndex][i], it->second.getCopyNdx(sharedData.loopData[sharedData.tempBarLoopIndex][i]));
						}
						else {
							sendNewBarToPart(it->first, sharedData.loopsUnordered[sharedData.tempBarLoopIndex], sharedData.tempBarLoopIndex);
							sendBarDataToPart(it->first, sharedData.loopData[sharedData.tempBarLoopIndex][i]);
						}
						sendLoopDataToPart(it->first, sharedData.loopData[sharedData.tempBarLoopIndex][i], sharedData.loopData[sharedData.tempBarLoopIndex]);
					}
					else if (it->second.barDataOKFromParts[sharedData.loopData[sharedData.tempBarLoopIndex][i]] == false) {
						cout << it->second.getName() << " didn't report success, resending data for bar " << sharedData.loopData[sharedData.tempBarLoopIndex][i] << endl;
						if (it->second.getCopied(sharedData.loopData[sharedData.tempBarLoopIndex][i])) {
							sendCopyToPart(it->first, sharedData.loopData[sharedData.tempBarLoopIndex][i], it->second.getCopyNdx(sharedData.loopData[sharedData.tempBarLoopIndex][i]));
						}
						else {
							sendNewBarToPart(it->first, sharedData.loopsUnordered[sharedData.tempBarLoopIndex], sharedData.tempBarLoopIndex);
							sendBarDataToPart(it->first, sharedData.loopData[sharedData.tempBarLoopIndex][i]);
						}
						sendLoopDataToPart(it->first, sharedData.loopData[sharedData.tempBarLoopIndex][i], sharedData.loopData[sharedData.tempBarLoopIndex]);
					}
				}
			}
		}
		// take a new time stamp
		// if the parts that haven't received bar data properly do receive it now
		// the checkIfAllPartsReceivedBarData boolean will be set to false and this
		// if test will not run again for this bar(s)
		sendBarDataToPartsTimeStamp = ofGetElapsedTimeMillis();
	}
	// check if there are new bar data to be sent to parts
	if (sendBarDataToPartsBool && !waitToSendBarDataToParts) {
		if (sendBarDataToPartsCounter < (int)sharedData.loopData[sharedData.tempBarLoopIndex].size()) {
			int barIndex = sharedData.loopData[sharedData.tempBarLoopIndex][sendBarDataToPartsCounter++];
			sendBarDataToParts(barIndex);
		}
		else {
			sendBarDataToPartsBool = false;
			// once all bars have been sent, take a time stamp so we can check if all parts
			// have received bar data properly
			// a counter that increments when we receive an OK at the /barok OSC address
			// will set the boolean below to false once it reaches the number of bars that are supposed to be sent
			// if not, then the chunk of code above this nested if will be executed
			sendBarDataToPartsTimeStamp = ofGetElapsedTimeMillis();
			checkIfAllPartsReceivedBarData = true;
			cout << "all bars sent, setting time stamp\n";
		}
	}
    // check for OSC messages
	while (oscReceiver.hasWaitingMessages()) {
		ofxOscMessage m;
		oscReceiver.getNextMessage(m);
		string address = m.getAddress();
		// check the error message received by a server
		if (address == "/error") {
			std::pair<int, int> p;
			for (size_t i = 0; i < m.getNumArgs()/2; i++) {
				cout << sharedData.instruments[m.getArgAsInt32(i*2)].getName() << " got error for bar " << m.getArgAsInt32((i*2)+1) << endl;
				p = std::make_pair(m.getArgAsInt32(i*2), m.getArgAsInt32((i*2)+1));
			}
			scorePartErrors.push_back(p);
		}
		else if (address == "/barok") {
			cout << "got bar OK from " << sharedData.instruments[m.getArgAsInt32(0)].getName() << endl;
			// store the instruments that have received bar data without errors
			sharedData.instruments[m.getArgAsInt32(0)].barDataOKFromParts[m.getArgAsInt32(1)] = true;
			barDataOKFromPartsCounter++;
			if (barDataOKFromPartsCounter / numScorePartSenders == sendBarDataToPartsCounter) {
				checkIfAllPartsReceivedBarData = false;
				cout << "all parts received bar data with part counter " << barDataOKFromPartsCounter << " bar counter " << sendBarDataToPartsCounter << " and num senders " << numScorePartSenders << endl;;
				if (!sequencer.isThreadRunning()) {
					sendLoopIndexToParts();
				}
			}
		}
		else {
			for (map<int, string>::iterator it = fromOscAddr.begin(); it != fromOscAddr.end(); ++it) {
				if (address.substr(0, it->second.size()) == it->second) {
					int whichPaneOsc = fromOsc[it->first] - 1;
					// unlock the mutex here because the function that handles data received from OSC locks it
					sharedData.mutex.unlock();
					mutexLocked = false;
					map<int, Editor>::iterator editIt = editors.find(whichPaneOsc);
					if (editIt != editors.end()) {
						if (m.getArgType(0) == OFXOSC_TYPE_STRING) {
							string oscStr = m.getArgAsString(0);
							for (unsigned j = 0; j < oscStr.size(); j++) {
								if (address.find("/press") != string::npos) {
									editIt->second.fromOscPress((int)oscStr.at(j));
								}
								else if (address.find("/release") != string::npos) {
									editIt->second.fromOscRelease((int)oscStr.at(j));
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

	if (!mutexLocked) sharedData.mutex.lock();
	for (auto func = sharedData.functions.begin(); func != sharedData.functions.end(); ++func) {
		// binding a function to the number of instruments + 2, binds to the framerate
		if (func->second.getBoundInst() == (int)sharedData.instruments.size() + 2) {
			parseCommand(func->second.getName(), 1, 1);
		}
	}
	sharedData.mutex.unlock();

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
	sharedData.mutex.lock();
	for (map<int, Editor>::iterator it = editors.begin(); it != editors.end(); ++it) {
		it->second.drawText();
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
	drawTraceback();
	sharedData.mutex.unlock();
}

//--------------------------------------------------------------
void ofApp::drawTraceback()
{
	static int tracebackNumLinesStatic = MINTRACEBACKLINES;
	int tracebackCounter = 0;
	uint64_t stamps[editors[whichPane].tracebackTimeStamps.size()];
	int ndxs[editors[whichPane].tracebackTimeStamps.size()];
	int i = 0;
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
void ofApp::drawScore()
{
	ofSetColor(brightness);
	ofDrawRectangle(scoreXOffset, scoreYOffset, scoreBackgroundWidth, scoreBackgroundHeight);
	// safety test to avoid crashes when changing the window size before creating any bars
	if (sharedData.loopIndex >= (int)sharedData.loopData.size()) return;
	//------------- variables for horizontal score view ------------------
	int numBars = 1;
	bool drawLoopStartEnd = true;
	int bar;
	int ndx = sharedData.thisLoopIndex;
	if (scoreOrientation == 1) {
		numBars = min(numBarsToDisplay, (int)sharedData.loopData[sharedData.loopIndex].size());
		drawLoopStartEnd = false;
		// the equation below yields the first bar to display
		// the rest will be calculated by adding the loop variable i
		// e.g. for an 8-bar loop with two bars being displayed
		// the line below yields 0, 2, 4, 6, every other index (it actually ignores the odd indexes)
		ndx = ((sharedData.thisLoopIndex - (sharedData.thisLoopIndex % numBars)) / numBars) * numBars;
	}
	//--------------- end of horizontal score view variables --------------
	//------------------ variables for the beat visualization -------------
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
			beatPulseMinPosY = sharedData.instruments.begin()->second.getMinYPos(sharedData.thisLoopIndex) - sharedData.noteHeight;
			beatPulseMaxPosY = sharedData.instruments.rbegin()->second.getMaxYPos(sharedData.thisLoopIndex) + sharedData.noteHeight;
			beatPulseSizeY = beatPulseMaxPosY - beatPulseMinPosY;
			if (sharedData.beatViz) {
				switch (sharedData.beatVizType) {
					case 1:
						beatPulseStartX = scoreXStartPnt + scoreXOffset;
						beatPulseEndX = scoreXStartPnt + scoreXOffset + sharedData.instruments.begin()->second.getStaffXLength();
						if (scoreOrientation == 1) {
							beatPulseEndX += (sharedData.instruments.begin()->second.getStaffXLength() * (numBars-1));
							beatPulseEndX -= (sharedData.instruments.begin()->second.getClefXOffset() * (numBars-1));
							beatPulseEndX -= (sharedData.instruments.begin()->second.getMeterXOffset() * (numBars-1));
						}
						beatPulsePosX = beatPulseStartX - sharedData.staffLinesDist;
						beatPulseSizeX = beatPulseEndX + sharedData.staffLinesDist - beatPulsePosX;
						beatPulseMinPosY += scoreYOffset;
						ofSetColor(ofColor::paleGreen.r*brightnessCoeff,
								   ofColor::paleGreen.g*brightnessCoeff,
								   ofColor::paleGreen.b*brightnessCoeff,
								   sharedData.beatVizDegrade);
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
		// write the names of the instruments without the backslash and get the longest name
		for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
			float xPos = scoreXStartPnt + scoreXOffset - instNameWidths[it->second] - (sharedData.blankSpace/2);
			float yPos = sharedData.instruments[it->second].getStaffYAnchor()+(sharedData.staffLinesDist*2.5) + scoreYOffset;
			instFont.drawString(it->first.substr(1), xPos, yPos);
		}
		// then draw one line at each side of the staffs to connect them
		// we draw the first one here, and the rest inside the for loop below
		float connectingLineX = scoreXStartPnt + scoreXOffset;
		float connectingLineY1 = sharedData.instruments.begin()->second.getStaffYAnchor() + scoreYOffset;
		float connectingLineY2 = sharedData.instruments.rbegin()->second.getStaffYAnchor() + scoreYOffset;
		if (sharedData.instruments.size() > 1) {
			ofDrawLine(connectingLineX, connectingLineY1, connectingLineX, connectingLineY2);
		}
		// then we display the score
		float staffOffsetX = scoreXStartPnt + scoreXOffset;
		float notesOffsetX = staffOffsetX;
		notesOffsetX += sharedData.instruments.begin()->second.getClefXOffset();
		notesOffsetX += sharedData.instruments.begin()->second.getMeterXOffset();
		notesOffsetX += (sharedData.staffLinesDist * 2);
		float notesXCoef = (sharedData.instruments.begin()->second.getStaffXLength() + staffOffsetX - notesOffsetX) / notesXLength;
		if (showBarCount) {
			string barCountStr = "(" + to_string(sharedData.thisLoopIndex+1) + "/" + to_string(sharedData.loopData[sharedData.loopIndex].size()) + ") " + to_string(sharedData.barCounter);
			font.drawString(barCountStr, scoreXOffset+scoreBackgroundWidth-font.stringWidth(barCountStr)-20, scoreYOffset+20);
		}
		for (int i = 0; i < numBars; i++) {
			bool drawClef = false, drawMeter = false, animate = false;
			bool showBar = true;
			if (i == 0) drawClef = drawMeter = true;
			int barNdx = (ndx + i) % (int)sharedData.loopData[sharedData.loopIndex].size();
			bar = sharedData.loopData[sharedData.loopIndex][barNdx];
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
			int threshold = (int)sharedData.loopData[sharedData.loopIndex].size() - numBars + i;
			if (scoreChangeOnLastBar) threshold = (int)sharedData.loopData[sharedData.loopIndex].size() - 2;
			if (mustUpdateScore && (int)sharedData.thisLoopIndex > threshold && i < numBars - 1) {
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
			else if (((int)sharedData.thisLoopIndex % numBars) == numBars - 1 && i < numBars - 1) {
				bar = sharedData.loopData[sharedData.loopIndex][(barNdx+numBars)%(int)sharedData.loopData[sharedData.loopIndex].size()];
			}
			// draw the rest of the lines that connect the various staffs together horizontally
			if (sharedData.instruments.size() > 1) {
				connectingLineX += sharedData.instruments.begin()->second.getStaffXLength();
				if (i > 0) {
					if (!drawClef) connectingLineX -= sharedData.instruments.begin()->second.getClefXOffset();
					if (!drawMeter) connectingLineX -= sharedData.instruments.begin()->second.getMeterXOffset();
				}
				ofDrawLine(connectingLineX, connectingLineY1, connectingLineX, connectingLineY2);
			}
			// like with the beat visualization, accumulate X offsets for all bars but the first
			if (i > 0) {
				// only the notes need to go back some pixels, in case the clef or meter is not drawn
				// so we use a separate value for the X coordinate
				staffOffsetX += sharedData.instruments.begin()->second.getStaffXLength();
				notesOffsetX += sharedData.instruments.begin()->second.getStaffXLength();
				if (i > 1) {
					if (!drawClef) staffOffsetX -= sharedData.instruments.begin()->second.getClefXOffset();
					if (!drawMeter) staffOffsetX -= sharedData.instruments.begin()->second.getMeterXOffset();
				}
				if (!drawClef) notesOffsetX -= sharedData.instruments.begin()->second.getClefXOffset();
				if (!drawMeter) notesOffsetX -= sharedData.instruments.begin()->second.getMeterXOffset();
			}
			// if we're displaying the beat pulse on each beat instead of a rectangle that encloses the whole score
			// we need to display it here, in case we display the score horizontally, where we need to give
			// an offset to the pulse so it is displayed on top of the currently playing bar
			if (sharedData.beatAnimate && sharedData.beatViz && sharedData.beatVizType == 2) {
				// draw only for the currenlty playing bar, whichever that is in the horizontal line
				if (barNdx == (int)sharedData.thisLoopIndex) {
					beatPulseStartX = notesOffsetX + (((notesXLength / sharedData.numerator[getPlayingBarIndex()]) * sharedData.beatCounter) * notesXCoef);
					if (sharedData.beatCounter == -1) beatPulseStartX = notesOffsetX - sharedData.instruments.begin()->second.getClefXOffset();
					noteSize = sharedData.instruments.begin()->second.getNoteWidth();
					beatPulsePosX = beatPulseStartX - noteSize;
					beatPulseSizeX = noteSize * 2;
					beatPulseMinPosY += scoreYOffset;
					ofSetColor(ofColor::paleGreen.r*brightnessCoeff,
							   ofColor::paleGreen.g*brightnessCoeff,
							   ofColor::paleGreen.b*brightnessCoeff,
							   sharedData.beatVizDegrade);
					ofDrawRectangle(beatPulsePosX, beatPulseMinPosY, beatPulseSizeX, beatPulseSizeY);
					ofSetColor(0);
				}
			}
			if (showBar) {
				for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
					it->second.drawStaff(bar, staffOffsetX, scoreYOffset, drawClef, drawMeter, drawLoopStartEnd);
					it->second.drawNotes(bar, i, &sharedData.loopData[insertNaturalsNdx], notesOffsetX, scoreYOffset, animate, notesXCoef);
				}
			}
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
	    default:
	    	if (key == 61 && ctrlPressed && !altPressed) { // +
	    		fontSize += 2;
				setPaneCoords();
	    	}
	    	else if (key == 45 && ctrlPressed && !altPressed) { // -
	    		fontSize -= 2;
	    		if (fontSize == 0) fontSize = 2;
				setPaneCoords();
	    	}
	    	else if (key == 43 && ctrlPressed) { // + with shift pressed
	    		sharedData.scoreFontSize += 5;
	    		instFontSize = sharedData.scoreFontSize / 3.5;
	    		instFont.load("Monaco.ttf", instFontSize);
	    		setScoreSizes();
	    	}
	    	else if (key == 95 && ctrlPressed) { // - with shift pressed
	    		sharedData.scoreFontSize -= 5;
	    		if (sharedData.scoreFontSize < 5) sharedData.scoreFontSize = 5;
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
					}
				}
				editors[whichPane+1] = editor;
				changeFontSizeForAllPanes();
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
	    	// remove panes. Ctrl+w, where if only one pane, we quit 
	    	else if (key == 119 && ctrlPressed) {
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
	    	// swap score position with Ctl+P for horizontal
	    	// and Ctl+Shift+P for vertical
	    	else if ((key == 112) && ctrlPressed) {
	    		swapScorePosition(0);
	    	}
	    	else if ((key == 80) && ctrlPressed) {
	    		swapScorePosition(1);
	    	}
	    	else {
				editors[whichPane].allOtherKeys(key);
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
void ofApp::keyPressed(int key, int thisEditor)
{
	sharedData.mutex.lock();
	int whichPaneBackup = whichPane;
	// change the currently active editor temporarily
	// because this overloaded function is called from an editor
	// when it receives input from OSC
	// this way, even if an editor is out of focus
	// we're still able to type in it properly
	whichPane = thisEditor;
	executeKeyPressed(key);
	whichPane = whichPaneBackup;
	sharedData.mutex.unlock();
}

//--------------------------------------------------------------
// overloaded keyReleased() function called from OSC
void ofApp::keyReleased(int key, int thisEditor)
{
	// the same applies to releasing a key
	sharedData.mutex.lock();
	int whichPaneBackup = whichPane;
	whichPane = thisEditor;
	executeKeyReleased(key);
	whichPane = whichPaneBackup;
	sharedData.mutex.unlock();
}

//--------------------------------------------------------------
// OF's default keyPressed() function
void ofApp::keyPressed(int key)
{
	sharedData.mutex.lock();
	executeKeyPressed(key);
	sharedData.mutex.unlock();
}

//--------------------------------------------------------------
// OF's default keyReleased() function
void ofApp::keyReleased(int key)
{
	sharedData.mutex.lock();
	executeKeyReleased(key);
	sharedData.mutex.unlock();
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
void ofApp::sendToParts(ofxOscMessage m)
{
	for (auto it = grouppedOSCClients.begin(); it != grouppedOSCClients.end(); ++it) {
		sharedData.instruments[*it].scorePartSender.sendMessage(m, false);
	}
}

//--------------------------------------------------------------
void ofApp::sendCountdownToParts(int countdown)
{
	ofxOscMessage m;
	m.setAddress("/countdown");
	m.addIntArg(countdown);
	sendToParts(m);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendStopCountdownToParts()
{
	ofxOscMessage m;
	m.setAddress("/nocountdown");
	m.addIntArg(1); // dummy arg
	sendToParts(m);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendLoopDataToParts(int barIndex, vector<int> ndxs)
{
	// send the bar indexes of the loop to any connected instrument
	ofxOscMessage m;
	m.setAddress("/loop");
	m.addIntArg(barIndex);
	for (int ndx : ndxs) m.addIntArg(ndx);
	sendToParts(m);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendLoopDataToPart(int instNdx, int barIndex, vector<int> ndxs)
{
	// send the bar indexes of the loop to any connected instrument
	ofxOscMessage m;
	m.setAddress("/loop");
	m.addIntArg(barIndex);
	for (int ndx : ndxs) m.addIntArg(ndx);
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendBarIndexBeatMeterToPart(int instNdx, int barIndex)
{
	ofxOscMessage m;
	m.setAddress("/bardata");
	m.addIntArg(barIndex);
	m.addIntArg(sharedData.numBeats[barIndex]);
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	m.setAddress("/meter");
	m.addIntArg(sharedData.numerator[barIndex]);
	m.addIntArg(sharedData.denominator[barIndex]);
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendInstNdxToPart(int instNdx)
{
	ofxOscMessage m;
	m.setAddress("/instndx");
	m.addIntArg(instNdx);
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendBarDataToPart(int instNdx, int barIndex)
{
	sendBarIndexBeatMeterToPart(instNdx, barIndex);
	sendInstNdxToPart(instNdx);
	int size = 0;
	ofxOscMessage m;
	m.setAddress("/notes");
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreNotes[barIndex].size(); i++) {
		size += (int)sharedData.instruments[instNdx].scoreNotes[barIndex][i].size();
		size++; // we add a -2 to separate the vectors within a vector
	}
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreNotes[barIndex].size(); i++) {
		for (unsigned j = 0; j < sharedData.instruments[instNdx].scoreNotes[barIndex][i].size(); j++) {
			m.addIntArg(sharedData.instruments[instNdx].scoreNotes[barIndex][i][j]);
		}
		m.addIntArg(-2);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	size = 0;
	m.setAddress("/acc");
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreAccidentals[barIndex].size(); i++) {
		size += (int)sharedData.instruments[instNdx].scoreAccidentals[barIndex][i].size();
		size++; // we add a -2 to separate the vectors within a vector
	}
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreAccidentals[barIndex].size(); i++) {
		for (unsigned j = 0; j < sharedData.instruments[instNdx].scoreAccidentals[barIndex][i].size(); j++) {
			m.addIntArg(sharedData.instruments[instNdx].scoreAccidentals[barIndex][i][j]);
		}
		m.addIntArg(-2);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	size = 0;
	m.setAddress("/natur");
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreNaturalSignsNotWritten[barIndex].size(); i++) {
		size += (int)sharedData.instruments[instNdx].scoreNaturalSignsNotWritten[barIndex][i].size();
		size++; // we add a -2 to separate the vectors within a vector
	}
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreNaturalSignsNotWritten[barIndex].size(); i++) {
		for (unsigned j = 0; j < sharedData.instruments[instNdx].scoreNaturalSignsNotWritten[barIndex][i].size(); j++) {
			m.addIntArg(sharedData.instruments[instNdx].scoreNaturalSignsNotWritten[barIndex][i][j]);
		}
		m.addIntArg(-2);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	size = 0;
	m.setAddress("/oct");
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreOctaves[barIndex].size(); i++) {
		size += (int)sharedData.instruments[instNdx].scoreOctaves[barIndex][i].size();
		size++; // we add a -2 to separate the vectors within a vector
	}
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreOctaves[barIndex].size(); i++) {
		for (unsigned j = 0; j < sharedData.instruments[instNdx].scoreOctaves[barIndex][i].size(); j++) {
			m.addIntArg(sharedData.instruments[instNdx].scoreOctaves[barIndex][i][j]);
		}
		m.addIntArg(-2);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	m.setAddress("/ott");
	size = (int)sharedData.instruments[instNdx].scoreOttavas[barIndex].size();
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreOttavas[barIndex].size(); i++) {
		m.addIntArg(sharedData.instruments[instNdx].scoreOttavas[barIndex][i]);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	m.setAddress("/durs");
	size = (int)sharedData.instruments[instNdx].scoreDurs[barIndex].size();
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreDurs[barIndex].size(); i++) {
		m.addIntArg(sharedData.instruments[instNdx].scoreDurs[barIndex][i]);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	m.setAddress("/dots");
	size = (int)sharedData.instruments[instNdx].scoreDotIndexes[barIndex].size();
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreDotIndexes[barIndex].size(); i++) {
		m.addIntArg(sharedData.instruments[instNdx].scoreDotIndexes[barIndex][i]);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	m.setAddress("/gliss");
	size = (int)sharedData.instruments[instNdx].scoreGlissandi[barIndex].size();
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreGlissandi[barIndex].size(); i++) {
		m.addIntArg(sharedData.instruments[instNdx].scoreGlissandi[barIndex][i]);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	m.setAddress("/artic");
	size = (int)sharedData.instruments[instNdx].scoreArticulations[barIndex].size();
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreArticulations[barIndex].size(); i++) {
		m.addIntArg(sharedData.instruments[instNdx].scoreArticulations[barIndex][i]);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	m.setAddress("/dyn");
	size = (int)sharedData.instruments[instNdx].scoreDynamics[barIndex].size();
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreDynamics[barIndex].size(); i++) {
		m.addIntArg(sharedData.instruments[instNdx].scoreDynamics[barIndex][i]);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	m.setAddress("/dynidx");
	size = (int)sharedData.instruments[instNdx].scoreDynamicsIndexes[barIndex].size();
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreDynamicsIndexes[barIndex].size(); i++) {
		m.addIntArg(sharedData.instruments[instNdx].scoreDynamicsIndexes[barIndex][i]);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	m.setAddress("/dynramp_start");
	size = (int)sharedData.instruments[instNdx].scoreDynamicsRampStart[barIndex].size();
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreDynamicsRampStart[barIndex].size(); i++) {
		m.addIntArg(sharedData.instruments[instNdx].scoreDynamicsRampStart[barIndex][i]);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	m.setAddress("/dynramp_end");
	size = (int)sharedData.instruments[instNdx].scoreDynamicsRampEnd[barIndex].size();
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreDynamicsRampEnd[barIndex].size(); i++) {
		m.addIntArg(sharedData.instruments[instNdx].scoreDynamicsRampEnd[barIndex][i]);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	m.setAddress("/dynramp_dir");
	size = (int)sharedData.instruments[instNdx].scoreDynamicsRampDir[barIndex].size();
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreDynamicsRampDir[barIndex].size(); i++) {
		m.addIntArg(sharedData.instruments[instNdx].scoreDynamicsRampDir[barIndex][i]);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	m.setAddress("/slurbegin");
	size = (int)sharedData.instruments[instNdx].slurIndexes[barIndex].size();
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].slurIndexes[barIndex].size(); i++) {
		m.addIntArg(sharedData.instruments[instNdx].slurIndexes[barIndex][i].first);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	m.setAddress("/slurend");
	size = (int)sharedData.instruments[instNdx].slurIndexes[barIndex].size();
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].slurIndexes[barIndex].size(); i++) {
		m.addIntArg(sharedData.instruments[instNdx].slurIndexes[barIndex][i].second);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	m.setAddress("/texts");
	size = (int)sharedData.instruments[instNdx].scoreTexts[barIndex].size();
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].scoreTexts[barIndex].size(); i++) {
		m.addStringArg(sharedData.instruments[instNdx].scoreTexts[barIndex][i]);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
	m.setAddress("/textidx");
	size = (int)sharedData.instruments[instNdx].textIndexes[barIndex].size();
	m.addIntArg(size);
	for (unsigned i = 0; i < sharedData.instruments[instNdx].textIndexes[barIndex].size(); i++) {
		m.addIntArg(sharedData.instruments[instNdx].textIndexes[barIndex][i]);
	}
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendCopyToPart(int instNdx, int barIndex, int barToCopy)
{
	ofxOscMessage m;
	m.setAddress("/copy");
	m.addIntArg(instNdx);
	m.addIntArg(barIndex);
	m.addIntArg(barToCopy);
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendBarDataToParts(int barIndex)
{
	for (auto it = sharedData.instrumentIndexesOrdered.begin(); it != sharedData.instrumentIndexesOrdered.end(); ++it) {
		if (sharedData.instruments[it->second].sendToPart) {
			cout << "sending to " << sharedData.instruments[it->second].getName() << endl;
			if (sharedData.instruments[it->second].getCopied(barIndex)) {
				sendCopyToPart(it->second, barIndex, sharedData.instruments[it->second].getCopyNdx(barIndex));
			}
			else {
				sendNewBarToPart(it->second, sharedData.loopsUnordered[barIndex], barIndex);
				sendBarDataToPart(it->second, barIndex);
			}
			sendLoopDataToPart(it->second, barIndex, sharedData.loopData[barIndex]);
		}
	}
}

//--------------------------------------------------------------
void ofApp::sendLoopIndexToParts()
{
	ofxOscMessage m;
	m.setAddress("/loopndx");
	m.addIntArg(sharedData.loopIndex);
	sendToParts(m);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendNewBarToParts(string barName, int barIndex)
{
	ofxOscMessage m;
	m.setAddress("/newbar");
	m.addStringArg(barName);
	m.addIntArg(barIndex);
	sendToParts(m);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendNewBarToPart(int instNdx, string barName, int barIndex)
{
	ofxOscMessage m;
	m.setAddress("/newbar");
	m.addStringArg(barName);
	m.addIntArg(barIndex);
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendClefToPart(int instNdx, int clef)
{
	ofxOscMessage m;
	m.setAddress("/clef");
	m.addIntArg(instNdx);
	m.addIntArg(clef);
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
	m.clear();
}

//--------------------------------------------------------------
void ofApp::sendRhythmToPart(int instNdx)
{
	ofxOscMessage m;
	m.setAddress("/rhythm");
	m.addIntArg(instNdx);
	m.addBoolArg(true);
	sharedData.instruments[instNdx].scorePartSender.sendMessage(m, false);
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
	sendToParts(m);
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
	string aLocal(1, a[0]);
	if (aLocal.compare(b) == 0) return true;
	return false;
}

//--------------------------------------------------------------
bool ofApp::endsWith(string a, string b)
{
	string aLocal(1, a[a.size()-1]);
	if (aLocal.compare(b) == 0) return true;
	return false;
}

//--------------------------------------------------------------
bool ofApp::isNumber(string str)
{
	// first check if there is a hyphen in the beginning
	int loopStart = 0;
	if (str[0] == '-') loopStart = 1;
	for (int i = loopStart; i < (int)str.length(); i++) {
		if (!isdigit(str[i])) {
			return false;
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
// the following function is inspired by
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
	int offset = 1;	
	string firstPart = "";
	if (whiteSpaceBefore != string::npos) {
		firstPart = str.substr(0, whiteSpaceBefore) + " ";
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
string ofApp::detectRepetitions(string str)
{
	size_t bracketNdx = str.find("[");
	size_t multIndex;
	size_t chordIndex;
	size_t firstPartSize = 0;
	std::pair<int, string> p (0, str);
	while (bracketNdx != string::npos) {
		p = expandStringBasedOnBrackets(p.second);
		if (p.first) break; // if we get a 1 as a return value, it's an error
		bracketNdx = p.second.find("[");
	}
	chordIndex = p.second.find("<");
	while (chordIndex != std::string::npos) {
		p = expandChordsInString(p.second.substr(0, firstPartSize), p.second.substr(firstPartSize));
		chordIndex = p.second.substr(p.first).find("<");
		firstPartSize = p.first;
		if (chordIndex != std::string::npos) firstPartSize += chordIndex;
	}
	multIndex = p.second.find("*");
	while (multIndex != string::npos) {
		p = expandSingleChars(p.second);
		if (p.first) break;
		multIndex = p.second.find("*");
	}
	return p.second;
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

//--------------------------------------------------------------
void ofApp::parseStrings(int index, int numLines)
{
	bool noErrors = true;
	barError = false;
	vector<std::pair<int, string>> errors;
	for (int i = index; i < (index+numLines); i++) {
		// parseString() returns a pair of int and string, where the int determines the error type
		// 0 for nothing, 1 for note, 2 for warning, and 3 for error
		errors.push_back(parseString(editors[whichPane].allStrings[i], i, numLines));
	}
	for (std::pair<int, string> error : errors) {
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
					parsingBars = false;
					firstInstForBarsSet = false;
				}
				else {
					deleteLastBar();
				}
				parsingBar = false;
				checkIfAllPartsReceivedBarData = false;
				sendBarDataToPartsBool = false;
			}
			if (parsingLoop) {
				// must clear loops
				deleteLastLoop();
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
		// first add a rest for any instrument that is not included in the bar
		if (!parsingLoop) {
			fillInMissingInsts(barIndex);
			// then store the number of beats for this bar
			sharedData.numBeats[barIndex] = sharedData.tempNumBeats;
			// if we create a bar, create a loop with this bar only
			sharedData.loopData[barIndex] = {barIndex};
			// check if no time has been specified and set the default 4/4
			if (sharedData.numerator.find(barIndex) == sharedData.numerator.end()) {
				sharedData.numerator[barIndex] = 4;
				sharedData.denominator[barIndex] = 4;
			}
			setScoreNotes(barIndex);
			barError = false;
			if (numScorePartSenders > 0) {
				sendBarDataToPartsBool = true;
				sendBarDataToPartsCounter = 0;
				barDataOKFromPartsCounter = 0;
			}
			for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				it->second.setPassed(false);
				if (it->second.barDataOKFromParts.find(barIndex) == it->second.barDataOKFromParts.end()) {
					it->second.barDataOKFromParts[barIndex] = false;
				}
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
			parseCommand(multiBarsLoop, index, 1);
			waitToSendBarDataToParts = false;
			sendBarDataToPartsCounter = 0;
			barDataOKFromPartsCounter = 0;
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
				editors[whichPane].setTraceback(error, lineNum);
				popNewPattern();
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
			std::pair<int, string> error = traverseList(str, lineNum, numLines); 
			if (error.first > 0) {
				editors[whichPane].setTraceback(error, lineNum);
			}
			else {
				editors[whichPane].releaseTraceback(lineNum);
			}
			return error;
		}
		else {
			parsingCommand = true;
			std::pair<int, string> error = parseCommand(str, lineNum, numLines);
			if (error.first > 0) {
				editors[whichPane].setTraceback(error, lineNum);
			}
			else {
				editors[whichPane].releaseTraceback(lineNum);
			}
			return error;
		}
	}
	return std::make_pair(0, "");
}

//--------------------------------------------------------------
std::pair<int, string> ofApp::traverseList(string str, int lineNum, int numLines)
{
	unsigned i;
	string lineWithArgs = str;
	if (lineWithArgs.compare("{") == 0) return std::make_pair(0, "");
	if (lineWithArgs.compare("}") == 0) return std::make_pair(0, "");
	if (startsWith(lineWithArgs, "{")) lineWithArgs = lineWithArgs.substr(1);
	else if (endsWith(lineWithArgs, "}")) lineWithArgs = lineWithArgs.substr(0, lineWithArgs.size()-1);
	vector<int> argNdxs = findIndexesOfCharInStr(lineWithArgs, "$");
	for (auto it = listMap[lastListIndex].begin(); it != listMap[lastListIndex].end(); ++it) {
		for (i = 0; i < argNdxs.size(); i++) {
			string strToExecute = replaceCharInStr(lineWithArgs, lineWithArgs.substr(argNdxs[i],1), *it);
			std::pair<int, string> error = parseCommand(strToExecute, lineNum, numLines);
			if (error.first == 3) return error;
		}
	}
	return std::make_pair(0, "");
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
	sharedData.loopsUnordered[barIndex] = barName;
	sharedData.loopsVariants[barIndex] = 0;
	sharedData.tempBarLoopIndex = barIndex;
	return barIndex;
}

//--------------------------------------------------------------
void ofApp::storeNewLoop(string loopName)
{
	int loopIndex = getLastLoopIndex() + 1;
	sharedData.loopsIndexes[loopName] = loopIndex;
	sharedData.loopsUnordered[loopIndex] = loopName;
	sharedData.loopsVariants[loopIndex] = 0;
	sharedData.tempBarLoopIndex = loopIndex;
	partsReceivedOKCounters[loopIndex] = 0;
}

//--------------------------------------------------------------
void ofApp::popNewPattern()
{
	map<string, int>::reverse_iterator it = sharedData.barsIndexes.rbegin();
	sharedData.barsIndexes.erase(it->first);
	it = sharedData.loopsIndexes.rbegin();
	sharedData.loopsIndexes.erase(it->first);
	//sendPushPopPattern();
}

//--------------------------------------------------------------
std::pair<int, string> ofApp::parseCommand(string str, int lineNum, int numLines)
{
	vector<string> commands = tokenizeString(str, " ");
	int BPMTempo = 0;
	std::pair<int, string> error = std::make_pair(0, "");

	if (commands[0].compare("\\time") == 0) {
		if (commands.size() > 2) {
			return std::make_pair(3, "\\time command takes one argument only");
		}
		else if (commands.size() < 2) {
			return std::make_pair(3, "\\time command needs one arguments");
		}
		int divisionSym = commands[1].find("/");
		string numeratorStr = commands[1].substr(0, divisionSym);
		string denominatorStr = commands[1].substr(divisionSym+1);
		int numeratorLocal = 0;
		int denominatorLocal = 0;
		if (isNumber(numeratorStr)) {
			numeratorLocal = stoi(numeratorStr);
		}
		else {
			return std::make_pair(3, numeratorStr + " is not an int");
		}
		if (isNumber(denominatorStr)) {
			denominatorLocal = stoi(denominatorStr);
		}
		else {
			return std::make_pair(3, denominatorStr + " is not an int");
		}
		// a \time command must be placed inside a bar definition
		// and the bar index has already been stored, so we can safely query the last bar index
		int bar = sharedData.barsIndexes.rbegin()->second;
		sharedData.numerator[bar] = numeratorLocal;
		sharedData.denominator[bar] = denominatorLocal;
	}

	else if (commands[0].compare("\\tempo") == 0) {
		if (commands.size() > 2) {
			return std::make_pair(3, "\\tempo command takes one argument only");
		}
		if (commands.size() < 2) {
			return std::make_pair(3, "\\tempo command needs one argument");
		}
		if (isNumber(commands[1])) {
			BPMTempo = stoi(commands[1]);
			// convert BPM to ms
			sharedData.tempoMs = 1000.0 / ((float)BPMTempo / 60.0);
			// get the tempo of the minimum duration
			sharedData.newTempo = sharedData.tempoMs / (MINDUR / sharedData.denominator[getPlayingBarIndex()]);
			// get the usec duration of the PPQM
			sharedData.PPQNPerUs = (uint64_t)(sharedData.tempoMs / (float)sharedData.PPQN) * 1000;
		}
		else {
			return std::make_pair(3, "" + commands[1] + " is not an int");
		}
		if (sequencer.isThreadRunning()) {
			sequencer.tempoUpdate();
		}
	}

	else if (commands[0].compare("\\ppqn") == 0) {
		if (commands.size() < 2) {
			return std::make_pair(3, "\\ppqn command needs the PPQN number as an argument");
		}
		if (commands.size() > 2) {
			return std::make_pair(3, "\\ppqn command takes one argument only");
		}
		if (!isNumber(commands[1])) {
			return std::make_pair(3, "the argument to \\ppqn must be a number");
		}
		int PPQNLocal = stoi(commands[1]);
		if (PPQNLocal != 24 && PPQNLocal != 48 && PPQNLocal != 96) {
			return std::make_pair(3, "PPQN value can be 24, 48, or 96");
		}
		sharedData.PPQN = PPQNLocal;
		sharedData.PPQNPerUs = (uint64_t)(sharedData.tempoMs / (float)sharedData.PPQN) * 1000;
	}

	else if (commands[0].compare("\\midiclock") == 0) {
		if (commands.size() < 2) {
			return std::make_pair(3, "\\midiclock takes one argument");
		}
		if (commands[1].compare("on") == 0) {
			sequencer.setSendMidiClock(true);
			sharedData.PPQNTimeStamp = ofGetElapsedTimeMicros();
		}
		else if (commands[1].compare("off") == 0) {
			sequencer.setSendMidiClock(false);
		}
		else {
			return std::make_pair(3, commands[1] + ": unknown argument, \\midiclock takes on or off");
		}
	}

	else if (commands[0].compare("\\play") == 0) {
		if (commands.size() > 1) {
			return std::make_pair(3, "\\play command takes no arguments");
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

	else if (commands[0].compare("\\stop") == 0) {
		if (commands.size() > 1) {
			return std::make_pair(3, "\\stop command takes no arguments");
		}
		sequencer.stop();
	}

	else if (commands[0].compare("\\stopnow") == 0) {
		if (commands.size() > 1) {
			return std::make_pair(3, "\\stopnow command takes no arguments");
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
		sendToParts(m);
		m.clear();
	}

	else if (commands[0].compare("\\reset") == 0) {
		if (commands.size() > 1) {
			return std::make_pair(3, "\\reset command takes no arguments");
		}
		for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			it->second.resetBarDataCounter();
		}
		sequencer.setSequencerRunning(false);
	}

	else if (commands[0].compare("\\finish") == 0) {
		if (commands.size() > 1) {
			return std::make_pair(3, "\\finish command takes no arguments");
		}
		sequencer.setFinish(true);
	}

	else if (commands[0].compare("\\beatsin") == 0) {
		if (commands.size() < 2) {
			return std::make_pair(3, "\\beatsin takes a number for beat countdown");
		}
		else if (commands.size() > 2) {
			return std::make_pair(3, "\\beatsin takes one argument only");
		}
		if (!isNumber(commands[1])) {
			return std::make_pair(3, "\\beatsin takes a number as an argument");
		}
		int countdown = stoi(commands[1]);
		if (countdown < 0) {
			return std::make_pair(3, "can't set a negative countdown");
		}
		sequencer.setCountdown(countdown);
	}

	else if (commands[0].compare("\\instorder") == 0) {
		if (commands.size() != sharedData.instruments.size()+1) {
			return std::make_pair(3, "instrument list must be equal to number of instruments");
		}
		for (unsigned i = 1; i < commands.size(); i++) {
			sharedData.instrumentIndexesOrdered[i-1] = sharedData.instrumentIndexes[commands[i]];
		}
	}

	else if (commands[0].compare("\\print") == 0) {
		string restOfCommand = "";
		for (unsigned i = 1; i < commands.size(); i++) {
			restOfCommand += " ";
			restOfCommand += commands[i];
		}
		std::pair<int, string> error = parseString(restOfCommand, lineNum, numLines);
		if (error.first == 0) return std::make_pair(1, error.second);
		else return error;
	}

	else if (commands[0].compare("\\osc") == 0) {
		if (commands.size() < 2) {
			return std::make_pair(3, "\\osc command takes one argument, the name of the OSC client");
		}
		if (oscClients.find(commands[1]) != oscClients.end()) {
			return std::make_pair(3, "OSC client already exists");
		}
		string oscClientName = "\\" + commands[1];
		oscClients[oscClientName] = ofxOscSender();
		commands_map[livelily][oscClientName] = ofColor::lightSeaGreen;
	}
	
	else if (commands[0].compare("\\barnum") == 0) {
		if (commands.size() > 1) {
			return std::make_pair(3, "\\barnum takes no arguments");
		}
		return std::make_pair(0, to_string(getPlayingBarIndex()));
	}

	else if (commands[0].compare("\\beatcount") == 0) {
		if (commands.size() > 1) {
			return std::make_pair(3, "\\beatcount takes no arguments");
		}
		// livelily is 1-based, but C++ is 0-based, so we add one to the beat counter
		// this is not needed with the \barnum command, because livelily creates the first bar
		// with index 0 upon creating the instruments, so the first bar created by the user
		// will have index 1
		return std::make_pair(0, to_string(sharedData.beatCounter + 1));
	}

	else if (commands[0].compare("\\insts") == 0) {
		if (commands.size() < 2) {
			return std::make_pair(3, "insts command takes at least one argument");
		}
		for (unsigned i = 1; i < commands.size(); i++) {
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
				commands_map[livelily]["\\" + commands[i]] = ofColor::greenYellow;
			}
			lastInstrument = commands[i];
		}
		// create a default empty bar with a -1 bar number
		// by recursively calling parseCommand() like below
		parseCommand("\\bar -1", 0, 1); // dummy line number and number of lines arguments
	}

	else if (commands[0].compare("\\score") == 0) {
		if (commands.size() < 2) {
			return std::make_pair(3, "\\score command takes at least one argument");
		}
		if (commands[1].compare("show") == 0) {
			sharedData.showScore = true;
			editors[whichPane].setMaxCharactersPerString();
		}
		else if (commands[1].compare("hide") == 0) {
			sharedData.showScore = false;
			editors[whichPane].setMaxCharactersPerString();
		}
		else {
			return scoreCommands(commands, lineNum, numLines);
		}
	}

	else if (commands[0].compare("\\cursor") == 0) {
		if ((commands.size() == 1) || (commands.size() > 2)) {
			return std::make_pair(3, "\\cursor takes one argument");
		}
		// for some reason the OF functions are inverted
		if (commands[1].compare("hide")) {
			ofShowCursor();
		}
		else if (commands[1].compare("show")) {
			ofHideCursor();
		}
		else {
			return std::make_pair(3, "unknown argument for \\cursor");
		}
	}

	else if (commands[0].compare("\\bar") == 0) {
		if (commands.size() < 2) {
			barError = true;
			return std::make_pair(3, "\\bar command takes a name as an argument");
		}
		if (commands.size() >= 2 && commands[1].compare("{") == 0) {
			barError = true;
			return std::make_pair(3, "\\bar command takes a name as an argument");
		}
		string barName = "\\" + commands[1];
		auto barExists = sharedData.barsIndexes.find(barName);
		if (barExists != sharedData.barsIndexes.end()) {
			barError = true;
			return std::make_pair(3, (string)"bar " + barName.substr(1) + (string)" already exists");
		}
		parsingBar = true;
		waitToSendBarDataToParts = false;
		int barIndex = storeNewBar(barName);
		// first set all instruments to not passed and not copied
		for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			it->second.setPassed(false);
			it->second.setCopied(barIndex, false);
		}
		commands_map[livelily][barName] = ofColor::orchid;
		// since bars include simple melodic lines in their definition
		// we need to know this when we parse these lines
		if (commands.size() > 2) {
			string restOfCommand = "";
			for (unsigned i = 2; i < commands.size()-1; i++) {
				restOfCommand += commands[i];
				restOfCommand += " ";
			}
			restOfCommand += commands[commands.size()-1];
			return parseString(restOfCommand, lineNum, numLines);
		}
	}

	else if (commands[0].compare("\\loop") == 0) {
		if (commands.size() < 2) {
			return std::make_pair(3, "\\loop command takes a name as an argument");
		}
		if (commands.size() > 1 && startsWith(commands[1], "{")) {
			return std::make_pair(3, "\\loop command must take a name as an argument");
		}
		string loopName = "\\" + commands[1];
		auto loopExists = sharedData.loopsIndexes.find(loopName);
		if (loopExists != sharedData.loopsIndexes.end()) {
			return std::make_pair(3, (string)"loop " + loopName.substr(1) + (string)" already exists");
		}
		parsingLoop = true;
		storeNewLoop(loopName);
		commands_map[livelily][loopName] = ofColor::lightSeaGreen;
		// loops are most likely defined in one-liners, so we parse the rest of the line here
		if (commands.size() > 2) {
			string restOfCommand = "";
			for (unsigned i = 2; i < commands.size(); i++) {
				restOfCommand += commands[i];
				restOfCommand += " ";
			}
			std::pair<int, string> p = parseString(restOfCommand, lineNum, numLines);
			if (p.first > 0) {
				parsingLoop = false;
				return p;
			}
		}
	}

	else if (commands[0].compare("\\bars") == 0) {
		if (commands.size() < 2) {
			barError = true;
			return std::make_pair(3, "\\bars command takes a name for a set of bars as an argument");
		}
		if (barsIterCounter == 0) {
			for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				it->second.setMultiBarsDone(false);
				it->second.setMultiBarsStrBegin(0);
			}
			parsingBars = true;
			waitToSendBarDataToParts = true;
		}
		multiBarsName = commands[1];
		auto barsExist = sharedData.loopsIndexes.find("\\" + multiBarsName);
		if (barsExist != sharedData.loopsIndexes.end()) {
			return std::make_pair(3, (string)"bars " + multiBarsName + (string)" already exist");
		}
		barsIterCounter++;
		// store each bar separately with the "-x" suffix, where x is the counter
		// of the iterations, until all instruments have had all their separate bars parsed
		parseCommand("\\bar "+multiBarsName+"-"+to_string(barsIterCounter), lineNum, numLines);
	}

	else if (commands[0].compare("\\barcount") == 0) {
		if (commands.size() < 2) {
			return std::make_pair(3, "\\barcount command takes one argument, show or hide");
		}
		if (commands.size() > 2) {
			return std::make_pair(3, "\\barcount command takes one argument only, show or hide");
		}
		if (commands[1].compare("show") == 0) {
			showBarCount = true;
		}
		else if (commands[1].compare("hide") == 0) {
			showBarCount = false;
		}
		else {
			return std::make_pair(3, commands[1] + (string)"unknown argument to \\barcount");
		}
	}

	else if (commands[0].compare("\\list") == 0) {
		if (commands.size() < 2) {
			return std::make_pair(3, "\\list takes a name as an argument");
		}
		if (commands.size() < 3) {
			return std::make_pair(3, "\\list must be assigned at least one item, inside curly brackets");
		}
		string listName = "\\" + commands[1];
		bool firstList = true;
		if (listIndexes.size() > 0) {
			firstList = false;
			if (listIndexes.find(listName) != listIndexes.end()) {
				return std::make_pair(3, "list already exists");
			}
		}
		storingList = true;
		if (firstList) lastListIndex = 0;
		else lastListIndex = listIndexes.rbegin()->second + 1;
		listIndexes[listName] = lastListIndex;
		commands_map[livelily][listName] = ofColor::lightSeaGreen;
		string restOfCommand = "";
		for (unsigned i = 2; i < commands.size(); i++) {
			if (i > 2) restOfCommand += " ";
			restOfCommand += commands[i];
		}
		std::pair<int, string> error = parseString(restOfCommand, lineNum, numLines);
		if (error.first == 3) {
			storingList = false;
			return error;
		}
	}

	else if (commands[0].compare("\\fromosc") == 0) {
		if (commands.size() > 2) {
			return std::make_pair(3, "\\fromosc commands takes maximum one argument");
		}
		if (commands.size() > 1) {
			if (!startsWith(commands[1], "/")) {
				return std::make_pair(3, "OSC address must start with /");
			}
			fromOscAddr[whichPane] = commands[1];
		}
		else {
			fromOscAddr[whichPane] = "/livelily" + to_string(whichPane+1);
		}
		fromOsc[whichPane] = true;
	}

	else if (commands[0].compare("\\rest") == 0) {
		int numFoundInsts = 0;
		if (commands.size() == 2) {
			auto barToCopyIt = sharedData.barsIndexes.find(commands[1]);
			if (barToCopyIt == sharedData.barsIndexes.end()) {
				return std::make_pair(3, "bar " + commands[1] + " doesn't exist");
			}
			barToCopy = barToCopyIt->second;
		}
		else {
			return std::make_pair(3, "\\rest takes one argument");
		}
		for (map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			if (!it->second.passed) { // if an instrument hasn't passed
				numFoundInsts++;
				// turn this instrument to passed
				// so we don't find it in the next iteration
				it->second.passed = true;
				break;
			}
		}
		if (numFoundInsts == 0) {
			return std::make_pair(2, "all instruments have been defined, nothing left for \\rest");
		}
		for (map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			if (!it->second.passed) {
				// store the bar passed to the "\rest" command
				// for undefined instruments in the current bar
				// we need to define lastInstrumentIndex because it is used
				// in copyMelodicLine() which is called by strinLineFromPattern()
				lastInstrumentIndex = it->first;
				// once we store the current instrument, we parse the string
				// with the rest of the command, which is the bar to copy from
				// so the melodic line can be copied with stringLineFromPattern()
				string restOfCommand = commands[1];
				std::pair<int, string> p = parseString(restOfCommand, lineNum, numLines);
				if (p.first == 3) return p;
			}
		}
	}

	else if (commands[0].compare("\\rand") == 0) {
		if (commands.size() < 2) {
			return std::make_pair(3, "\\rand command takes at least one argument");
		}
		if (commands.size() > 3) {
			return std::make_pair(3, "\\rand command takes up to two arguments");
		}
		int start, end;
		if (commands.size() == 2) {
			if (!isNumber(commands[1])) {
				return std::make_pair(3, "\\rand takes only numbers as arguments");
			}
			end = stoi(commands[1]);
			start = 0;
		}
		else {
			if (!isNumber(commands[1]) || !isNumber(commands[2])) {
				return std::make_pair(3, "\\rand takes only numbers as arguments");
			}
			start = stoi(commands[1]);
			end = stoi(commands[2]);
		}
		int diff = end - start;
		int randVal = rand() % diff + start;
		return std::make_pair(0, to_string(randVal));
	}

	else if (commands[0].compare("\\solo") == 0) {
		if (commands.size() < 2) {
			return std::make_pair(3, "\\solo commands takes at least one argument");
		}
		// below we test if the number of tokens is greater than the number of instruments
		// and not greater than or equal to, because the first token in the \solo command
		if (commands.size() > sharedData.instrumentIndexes.size()) {
			return std::make_pair(3, "too many arguments for \\solo command");
		}
		vector<string> soloInsts;
		for (unsigned i = 1; i < commands.size(); i++) {
			if (sharedData.instrumentIndexes.find(commands[i]) == sharedData.instrumentIndexes.end()) {
				return std::make_pair(3, commands[i] + ": unknown instrument");
			}
			else {
				soloInsts.push_back(commands[i]);
			}
		}
		if (soloInsts.size() == sharedData.instrumentIndexes.size()) {
			return std::make_pair(2, "no point in calling \\solo for all instruments");
		}
		// mute all instruments but the solo ones
		for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
			if (find(soloInsts.begin(), soloInsts.end(), it->first) >= soloInsts.end()) {
				sharedData.instruments[it->second].setToMute(true);
			}
		}
	}

	else if (commands[0].compare("\\solonow") == 0) {
		if (commands.size() < 2) {
			return std::make_pair(3, "\\solo commands takes at least one argument");
		}
		// below we test if the number of tokens is greater than the number of instruments
		// and not greater than or equal to, because the first token in the \solo command
		if (commands.size() > sharedData.instrumentIndexes.size()) {
			return std::make_pair(3, "too many arguments for \\solonow command");
		}
		vector<string> soloInsts;
		for (unsigned i = 1; i < commands.size(); i++) {
			if (sharedData.instrumentIndexes.find(commands[i]) == sharedData.instrumentIndexes.end()) {
				return std::make_pair(3, commands[i] + ": unknown instrument");
			}
			else {
				soloInsts.push_back(commands[i]);
			}
		}
		if (soloInsts.size() == sharedData.instrumentIndexes.size()) {
			return std::make_pair(2, "no point in calling \\solonow for all instruments");
		}
		// mute all instruments but the solo ones
		for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
			if (find(soloInsts.begin(), soloInsts.end(), it->first) >= soloInsts.end()) {
				sharedData.instruments[it->second].setMute(true);
			}
		}
	}

	else if (commands[0].compare("\\mute") == 0) {
		if (commands.size() < 2) {
			return std::make_pair(3, "\\mute command takes at least one argument");
		}
		// again test if greater than and not if greater than or equal to
		if (commands.size() > sharedData.instruments.size()) {
			return std::make_pair(3, "too many arguments for \\mute command");
		}
		if (commands[1].compare("\\all") == 0) {
			for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				sharedData.instruments[it->second].setToMute(true);
			}
		}
		else {
			for (unsigned i = 1; i < commands.size(); i++) {
				map<string, int>::iterator it = sharedData.instrumentIndexes.find(commands[i]);
				if (it == sharedData.instrumentIndexes.end()) {
					return std::make_pair(3, commands[i] + ": unknown instrument");
				}
				sharedData.instruments[it->second].setToMute(true);
			}
		}
	}

	else if (commands[0].compare("\\unmute") == 0) {
		if (commands.size() < 2) {
			return std::make_pair(3, "\\unmute command takes at least one argument");
		}
		// again test if greater than and not if greater than or equal to
		if (commands.size() > sharedData.instruments.size()) {
			return std::make_pair(3, "too many arguments for \\unmute command");
		}
		if (commands[1].compare("\\all") == 0) {
			for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				sharedData.instruments[it->second].setToUnmute(true);
			}
		}
		else {
			for (unsigned i = 1; i < commands.size(); i++) {
				map<string, int>::iterator it = sharedData.instrumentIndexes.find(commands[i]);
				if (it == sharedData.instrumentIndexes.end()) {
					return std::make_pair(3, commands[i] + ": unknown instrument");
				}
				sharedData.instruments[it->second].setToUnmute(true);
			}
		}
	}

	else if (commands[0].compare("\\mutenow") == 0) {
		if (commands.size() < 2) {
			return std::make_pair(3, "\\mutenow command takes at least one argument");
		}
		// again test if greater than and not if greater than or equal to
		if (commands.size() > sharedData.instruments.size()) {
			return std::make_pair(3, "too many arguments for \\mutenow command");
		}
		if (commands[1].compare("\\all") == 0) {
			for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				sharedData.instruments[it->second].setMute(true);
			}
		}
		else {
			for (unsigned i = 1; i < commands.size(); i++) {
				map<string, int>::iterator it = sharedData.instrumentIndexes.find(commands[i]);
				if (it == sharedData.instrumentIndexes.end()) {
					return std::make_pair(3, commands[i] + ": unknown instrument");
				}
				sharedData.instruments[it->second].setMute(true);
			}
		}
	}

	else if (commands[0].compare("\\unmutenow") == 0) {
		if (commands.size() < 2) {
			return std::make_pair(3, "\\unmutenow command takes at least one argument");
		}
		// again test if greater than and not if greater than or equal to
		if (commands.size() > sharedData.instruments.size()) {
			return std::make_pair(3, "too many arguments for \\unmutenow command");
		}
		if (commands[1].compare("\\all") == 0) {
			for (map<string, int>::iterator it = sharedData.instrumentIndexes.begin(); it != sharedData.instrumentIndexes.end(); ++it) {
				sharedData.instruments[it->second].setMute(false);
			}
		}
		else {
			for (unsigned i = 1; i < commands.size(); i++) {
				map<string, int>::iterator it = sharedData.instrumentIndexes.find(commands[i]);
				if (it == sharedData.instrumentIndexes.end()) {
					return std::make_pair(3, commands[i] + ": unknown instrument");
				}
				sharedData.instruments[it->second].setMute(false);
			}
		}
	}

	else if (commands[0].compare("\\function") == 0) {
		if (commands.size() < 2) {
			return std::make_pair(3, "\\function needs to be given a name");
		}
		if (startsWith(commands[1], "{")) {
			return std::make_pair(3, "\\function needs to be given a name");
		}
		if (commands.size() > 2) {
			if (!startsWith(commands[2], "{")) {
				return std::make_pair(3, "after the function name, the function body must be enclosed in curly brackets");
			}
		}
		if (functionIndexes.find("\\"+commands[1]) != functionIndexes.end()) {
			lastFunctionIndex = functionIndexes["\\"+commands[1]];
			sharedData.functions[lastFunctionIndex].clear();
			storingFunction = true;
		}
		else {
			int functionNdx = 0;
			if (functionIndexes.size() > 0) {
				map<string, int>::reverse_iterator it = functionIndexes.rbegin();
				functionNdx = it->second + 1;
			}
			functionIndexes["\\"+commands[1]] = functionNdx;
			Function function;
			sharedData.functions[functionNdx] = function;
			sharedData.functions[functionNdx].setName("\\"+commands[1]);
			if (sharedData.functions[functionNdx].getNameError()) {
				return std::make_pair(3, "function name too long");
			}
			commands_map[livelily]["\\"+commands[1]] = ofColor::turquoise;
			storingFunction = true;
			lastFunctionIndex = functionNdx;
		}
		// check if there are more characters in this line
		// and then get all the strings in all executing lines
		// to allocate memory for the char array of functions
		size_t functionBodySize = 0;
		if (commands.size() > 2) {
			// if the line of the function definition includes more than the opening bracket
			if (commands[2].compare("{") != 0) {
				functionBodySize += commands[2].substr(1).size();
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
		string restOfCommand = "";
		for (unsigned i = 2; i < commands.size()-1; i++) {
			restOfCommand += commands[i];
			restOfCommand += " ";
		}
		restOfCommand += commands[commands.size()-1];
		std::pair<int, string> p = parseString(restOfCommand, lineNum, numLines);
		if (p.first > 0) {
			storingFunction = false;
		}
	}

	else if (commands[0].compare("\\miditune") == 0) {
		if (commands.size() != 2) {
			return std::make_pair(3, "\\miditune takes one argument, the MIDI tuning frequency");
		}
		if (!isNumber(commands[1])) {
			return std::make_pair(3, "the argument to \\miditune must be a number");
		}
		int midiTune = stoi(commands[1]);
		if (midiTune < 0) {
			return std::make_pair(3, "MIDI tuning frequency must be positive");
		}
		sequencer.setMidiTune(midiTune);
	}

	else if (commands[0].compare("\\listmidiports") == 0) {
		if (commands.size() > 1) {
			return std::make_pair(3, "\\listmidiports command takes no arguments");
		}
		vector<string> outPorts = sequencer.midiOut.getOutPortList();
		string outPortsOneStr = "";
		for (unsigned i = 0; i < outPorts.size()-1; i++) {
			outPortsOneStr += "MIDI out port " + to_string(i) + ": " + outPorts[i] += "\n";
		}
		// append the last port separately to not include the newline
		outPortsOneStr += "MIDI out port " + to_string(outPorts.size()-1) + ": " + outPorts.back();
		return std::make_pair(1, outPortsOneStr);
	}
	
	else if (commands[0].compare("\\openmidiport") == 0) {
		if (commands.size() > 2) {
			return std::make_pair(3, "\\openmidiport takes one argument only");
		}
		else if (commands.size() == 1) {
			return std::make_pair(3, "no midi port number provided");
		}
		if (!isNumber(commands[1])) {
			return std::make_pair(3, "midi port argument must be a number");
		}
		sequencer.midiOut.openPort(stoi(commands[1]));
		midiPortOpen = true;
		return std::make_pair(1, "opened MIDI port " + commands[1]);
	}

	else if (commands[0].compare("\\dur") == 0 || commands[0].compare("\\mididur") == 0) {
		if (commands.size() == 1) {
			return std::make_pair(3, "no duration percentage set");
		}
		if (commands.size() > 2) {
			return std::make_pair(3, "\\dur command takes one argument only");
		}
		if (!isNumber(commands[1])) {
			return std::make_pair(3, "duration percentage must be a number");
		}
		int durPercent = stoi(commands[1]);
		if (durPercent < 0) {
			return std::make_pair(3, "duration percentage can't be below 0");
		}
		if (durPercent == 0) {
			return std::make_pair(3, "duration percentage can't be 0");
		}
		if (durPercent > 100) {
			return std::make_pair(3, "duration percentage can't be over 100%");
		}
		sharedData.instruments[lastInstrumentIndex].setDefaultDur((float)durPercent/10.0);
		if (durPercent == 100) {
			return std::make_pair(2, "duration set to 100\% of Note On duration");
		}
	}

	else if (commands[0].compare("\\staccato") == 0 || commands[0].compare("\\midistaccato") == 0) {
		if (commands.size() == 1) {
			return std::make_pair(3, "no percentage value provided");
		}
		if (commands.size() > 2) {
			return std::make_pair(3, "\\staccato command takes one argument only");
		}
		if (!isNumber(commands[1])) {
			return std::make_pair(3, "staccato percentage must be a number");
		}
		int stacPercent = stoi(commands[1]);
		if (stacPercent < 0) {
			return std::make_pair(3, "staccato percentage can't be below 0");
		}
		if (stacPercent == 0) {
			return std::make_pair(3, "staccato percentage can't be 0");
		}
		if (stacPercent > 100) {
			return std::make_pair(3, "staccato percentage can't be over 100%");
		}
		sharedData.instruments[lastInstrumentIndex].setStaccatoDur((float)stacPercent/10.0);
		if (stacPercent == 100) {
			return std::make_pair(2, "staccato set to 100\% of Note On duration");
		}
	}

	else if (commands[0].compare("\\staccatissimo") == 0 || commands[0].compare("\\midistaccatissimo") == 0) {
		if (commands.size() == 1) {
			return std::make_pair(3, "no percentage value provided");
		}
		if (commands.size() > 2) {
			return std::make_pair(3, "\\staccatissimo command takes one argument only");
		}
		if (!isNumber(commands[1])) {
			return std::make_pair(3, "staccatissimo percentage must be a number");
		}
		int stacPercent = stoi(commands[1]);
		if (stacPercent < 0) {
			return std::make_pair(3, "staccatissimo percentage can't be below 0");
		}
		if (stacPercent == 0) {
			return std::make_pair(3, "staccatissimo percentage can't be 0");
		}
		if (stacPercent > 100) {
			return std::make_pair(3, "staccatissimo percentage can't be over 100%");
		}
		sharedData.instruments[lastInstrumentIndex].setStaccatissimoDur(stacPercent);
		if (stacPercent == 100) {
			return std::make_pair(2, "staccatissimo set to 100% of Note On duration");
		}
	}

	else if (commands[0].compare("\\tenuto") == 0 || commands[0].compare("\\miditenuto") == 0) {
		if (commands.size() == 1) {
			return std::make_pair(3, "no percentage value provided");
		}
		if (commands.size() > 2) {
			return std::make_pair(3, "\\tenuto command takes one argument only");
		}
		if (!isNumber(commands[1])) {
			return std::make_pair(3, "tenuto percentage must be a number");
		}
		int tenutoPercent = stoi(commands[1]);
		if (tenutoPercent < 0) {
			return std::make_pair(3, "tenuto percentage can't be below 0");
		}
		if (tenutoPercent == 0) {
			return std::make_pair(3, "tenuto percentage can't be 0");
		}
		if (tenutoPercent > 100) {
			return std::make_pair(3, "tenuto percentage can't be over 100%");
		}
		sharedData.instruments[lastInstrumentIndex].setTenutoDur(tenutoPercent);
		if (tenutoPercent == 100) {
			return std::make_pair(2, "tenuto set to 100% of Note On duration");
		}
	}

	else if (commands[0].compare("\\listserialports") == 0) {
		if (commands.size() > 1) {
			return std::make_pair(3, "\\listserialports command takes no arguments");
		}
		serial.listDevices();
		vector<ofSerialDeviceInfo> serialDeviceList = serial.getDeviceList();
		string outPortsOneStr = "";
		for (unsigned i = 0; i < serialDeviceList.size()-1; i++) {
			outPortsOneStr += "Serial port " + ofToString(serialDeviceList[i].getDeviceID()) + \
							  ": " + serialDeviceList[i].getDevicePath() + "\n";
		}
		// append last port separately to not include the newline
		outPortsOneStr += "Serial port " + ofToString(serialDeviceList.back().getDeviceID()) + \
						  ": " + serialDeviceList.back().getDevicePath();
		return std::make_pair(1, outPortsOneStr);
	}

	else if (commands[0].compare("\\openserialport") == 0) {
		if (commands.size() < 3 || commands.size() > 3) {
			return std::make_pair(3, "\\openserialport takes two arguments, port path and baud rate");
		}
		if (isNumber(commands[1])) {
			return std::make_pair(3, "first argument must be port path");
		}
		if (!isNumber(commands[2])) {
			return std::make_pair(3, "second argument must be baud rate");
		}
		serial.setup(commands[1], stoi(commands[2]));
		return std::make_pair(1, "opened " + commands[1] + " at baud " + commands[2]);
	}

	else if (commands[0].compare("\\livelily") == 0) {
		if (commands.size() > 1) {
			return std::make_pair(3, "language command takes no arguments");
		}
		editors[whichPane].setLanguage(livelily);
	}
	
	else if (commands[0].compare("\\python") == 0) {
		if (commands.size() > 1) {
			return std::make_pair(3, "language command takes no arguments");
		}
		editors[whichPane].setLanguage(python);
	}
	
	else if (commands[0].compare("\\lua") == 0) {
		if (commands.size() > 1) {
			return std::make_pair(3, "language command takes no arguments");
		}
		editors[whichPane].setLanguage(lua);
	}
	
	else {
		// the functions return a pair with a boolean stating if an instrument, loop, function, or list is found
		// and an error string
		// if the boolean is true, we just return the error string, otherwise we go on to the next function
		std::pair<bool, std::pair<int, string>> p;
		p = isInstrument(commands, lineNum, numLines);
		if (p.first) return p.second;
		p = isBarLoop(commands, lineNum, numLines);
		if (p.first) return p.second;
		p = isFunction(commands, lineNum, numLines);
		if (p.first) return p.second;
		p = isList(commands, lineNum, numLines);
		if (p.first) return p.second;
		p = isOscClient(commands, lineNum, numLines);
		if (p.first) return p.second;
		return std::make_pair(3, commands[0] + ": unknown command");
	}
	return error;
}

//--------------------------------------------------------------
std::pair<bool, std::pair<int, string>> ofApp::isInstrument(vector<string>& commands, int lineNum, int numLines)
{
	bool instrumentExists = false;
	std::pair<int, string> error = std::make_pair(0, "");
	if (sharedData.instruments.size() > 0) {
		if (sharedData.instrumentIndexes.find(commands[0]) != sharedData.instrumentIndexes.end()) {
			instrumentExists = true;
			lastInstrument = commands[0];
			lastInstrumentIndex = sharedData.instrumentIndexes[commands[0]];
			if (commands.size() > 1) {
				if (commands[1].compare("clef") == 0) {
					int clef = 0;
					if (commands.size() < 3) {
						std::pair<int, string> p = std::make_pair(3, "clef command takes a clef name as an argument");
						return std::make_pair(instrumentExists, p);
					}
					else if (commands.size() > 3) {
						std::pair<int, string> p = std::make_pair(3, "clef command takes one argument only");
						return std::make_pair(instrumentExists, p);
					}
					if (commands[2].compare("treble") == 0) {
						sharedData.instruments[lastInstrumentIndex].setClef(0);
						clef = 0;
					}
					else if (commands[2].compare("bass") == 0) {
						sharedData.instruments[lastInstrumentIndex].setClef(1);
						clef = 1;
					}
					else if (commands[2].compare("alto") == 0) {
						sharedData.instruments[lastInstrumentIndex].setClef(2);
						clef = 2;
					}
					else if (commands[2].compare("perc") == 0 || commands[2].compare("percussion") == 0) {
						sharedData.instruments[lastInstrumentIndex].setClef(3);
						clef = 3;
					}
					else {
						std::pair<int, string> p = std::make_pair(3, "unknown clef");
						return std::make_pair(instrumentExists, p);
					}
					if (sharedData.instruments[lastInstrumentIndex].sendToPart) {
						sendClefToPart(lastInstrumentIndex, clef);
					}
					return std::make_pair(instrumentExists, error);
				}
				else if (commands[1].compare("rhythm") == 0) {
					if (commands.size() > 2) {
						std::pair<int, string> p = std::make_pair(3, "\"rhythm\" command takes no arguments");
						return std::make_pair(instrumentExists, p);
					}
					sharedData.instruments[lastInstrumentIndex].setRhythm(true);
					if (sharedData.instruments[lastInstrumentIndex].sendToPart) {
						sendRhythmToPart(lastInstrumentIndex);
					}
				}
				else if (commands[1].compare("transpose") == 0) {
					if (commands.size() != 3) {
						std::pair<int, string> p = std::make_pair(3, "\"transpose\" command takes one argument");
						return std::make_pair(instrumentExists, p);
					}
					if (!isNumber(commands[2])) {
						std::pair<int, string> p = std::make_pair(3, "argument to \"transpose\" command must be a number");
						return std::make_pair(instrumentExists, p);
					}
					int transposition = stoi(commands[2]);
					if (transposition < -11 || transposition > 11) {
						std::pair<int, string> p = std::make_pair(3, "argument to \"transpose\" command must be between -11 and 11");
						return std::make_pair(instrumentExists, p);
					}
					sharedData.instruments[lastInstrumentIndex].setTransposition(transposition);
					if (transposition == 0) {
						std::pair<int, string> p = std::make_pair(2, "0 \"transpose\" has no effect");
						return std::make_pair(instrumentExists, p);
					}
				}
				else if (commands[1].compare("sendto") == 0) {
					bool sendNotice = false;
					if (commands.size() < 3) {
						sendNotice = true;
					}
					else if (commands.size() > 5) {
						std::pair<int, string> p = std::make_pair(3, "\"sendto\" takes zero or three arguments, host IP, port, and this computer's IP");
						return std::make_pair(instrumentExists, p);
					}
					string host;
					int port;
					if (commands.size() < 3) {
						host = "localhost\t127.0.0.1";
						port = SCOREPARTPORT;
					}
					else if (commands.size() > 3) {
						if (commands.size() == 5) {
							if (isNumber(commands[2]) || !isNumber(commands[3]) || isNumber(commands[4])) {
								std::pair<int, string> p = std::make_pair(3, "if two arguments are provided, first argument must be host IP, second must be port number, and third this computer's IP");
								return std::make_pair(instrumentExists, p);
							}
							host = commands[2] + "\t" + commands[4];
							port = stoi(commands[3]);
						}
						else {
							if (isNumber(commands[2]) || isNumber(commands[3])) {
								std::pair<int, string> p = std::make_pair(3, "if two arguments are provided, first argument must be host IP, and second this computer's IP");
								return std::make_pair(instrumentExists, p);
							}
							host = commands[2] + "\t" + commands[3];
							port = SCOREPARTPORT;
						}
					}
					else {
						if (isNumber(commands[2])) {
							host = "localhost\t127.0.0.1";
							port = stoi(commands[2]);
						}
						else {
							std::pair<int, string> p = std::make_pair(3, "if one argument is provided, it must be the port number");
							return std::make_pair(instrumentExists, p);
						}
					}
					size_t tabPos = host.find("\t");
					string hostIP = host.substr(0, tabPos);
					std::pair<string, int> hostPort = std::make_pair(hostIP, port);
					bool clientExists = false;
					for (auto it = instrumentOSCHostPorts.begin(); it != instrumentOSCHostPorts.end(); ++it) {
						if (hostPort == it->second) {
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
					instrumentOSCHostPorts[lastInstrumentIndex] = hostPort;
					sharedData.instruments[lastInstrumentIndex].sendToPart = true;
					sharedData.instruments[lastInstrumentIndex].scorePartSender.setup(hostIP, port);
					numScorePartSenders++;
					string thisIP = host.substr(tabPos+1);
					ofxOscMessage m;
					m.setAddress("/initinst");
					m.addIntArg(lastInstrumentIndex);
					m.addStringArg(sharedData.instruments[lastInstrumentIndex].getName());
					m.addStringArg(thisIP);
					m.addIntArg(OSCPORT);
					sharedData.instruments[lastInstrumentIndex].scorePartSender.sendMessage(m, false);
					m.clear();
					if (sendNotice) {
						std::pair<int, string> p = std::make_pair(1, "setting localhost and port 9000");
						return std::make_pair(instrumentExists, p);
					}
				}
				else if (commands[1].compare("fullscreen") == 0) {
					if (!sharedData.instruments[lastInstrumentIndex].sendToPart) {
						std::pair<int, string> p = std::make_pair(3, "instrument doesn't have OSC set");
						return std::make_pair(instrumentExists, p);
					}
					if (commands.size() < 3) {
						std::pair<int, string> p = std::make_pair(3, "\"fullscreen\" takes one argument, on or off");
						return std::make_pair(instrumentExists, p);
					}
					if (commands.size() > 3) {
						std::pair<int, string> p = std::make_pair(3, "\"fullscreen\" takes one argument only");
						return std::make_pair(instrumentExists, p);
					}
					if (commands[2].compare("on") == 0) {
						sendFullscreenToPart(lastInstrumentIndex, true);
					}
					else if (commands[2].compare("off") == 0) {
						sendFullscreenToPart(lastInstrumentIndex, false);
					}
					else {
						std::pair<int, string> p = std::make_pair(3, "unknown argument to \"fullscreen\"");
						return std::make_pair(instrumentExists, p);
					}
				}
				else if (commands[1].compare("cursor") == 0) {
					if (!sharedData.instruments[lastInstrumentIndex].sendToPart) {
						std::pair<int, string> p = std::make_pair(3, "instrument doesn't have OSC set");
						return std::make_pair(instrumentExists, p);
					}
					if (commands.size() < 3) {
						std::pair<int, string> p = std::make_pair(3, "\"cursor\" takes one argument, show or hide ");
						return std::make_pair(instrumentExists, p);
					}
					if (commands.size() > 3) {
						std::pair<int, string> p = std::make_pair(3, "\"cursor\" takes one argument only");
						return std::make_pair(instrumentExists, p);
					}
					if (commands[2].compare("show") == 0) {
						sendCursorToPart(lastInstrumentIndex, true);
					}
					else if (commands[2].compare("hide") == 0) {
						sendCursorToPart(lastInstrumentIndex, false);
					}
					else {
						std::pair<int, string> p = std::make_pair(3, "unknown argument to \"cursor\"");
						return std::make_pair(instrumentExists, p);
					}
				}
				else if (commands[1].compare("update") == 0) {
					if (commands.size() < 3) {
						std::pair<int, string> p = std::make_pair(3, "\"update\" takes one argument, onlast or immediately");
						return std::make_pair(instrumentExists, p);
					}
					if (commands[2].compare("onlast") == 0) {
						sendScoreChangeToPart(lastInstrumentIndex, true);
					}
					else if (commands[2].compare("immediately") == 0) {
						sendScoreChangeToPart(lastInstrumentIndex, false);
					}
					else {
						std::pair<int, string> p = std::make_pair(3, commands[2] + (string)"unknown argument to \"update\", must be onlast or immediately");
						return std::make_pair(instrumentExists, p );
					}
				}
				else if (commands[1].compare("midichan")== 0) {
					if (commands.size() == 2) {
						std::pair<int, string> p = std::make_pair(3, "no MIDI channel set");
						return std::make_pair(instrumentExists, p);
					}
					if (commands.size() > 3) {
						std::pair<int, string> p = std::make_pair(3, "\"midichan\"  takes one argument only");
						return std::make_pair(instrumentExists, p);
					}
					if (!isNumber(commands[2])) {
						std::pair<int, string> p = std::make_pair(3, "MIDI channel must be a number");
						return std::make_pair(instrumentExists, p);
					}
					int midiChan = stoi(commands[2]);
					if (midiChan < 1 || midiChan > 16) {
						std::pair<int, string> p = std::make_pair(3, "MIDI must be between 1 and 16");
						return std::make_pair(instrumentExists, p);
					}
					sharedData.instruments[lastInstrumentIndex].setMidiChan(midiChan);
					sharedData.instruments[lastInstrumentIndex].setMidi(true);
				}
				else {
					string restOfCommand = "";
					for (unsigned i = 1; i < commands.size(); i++) {
						// add a white space only after the first token has been added
						if (i > 1) restOfCommand += " ";
						restOfCommand += commands[i];
					}
					if (parsingBars && barsIterCounter == 1 && !firstInstForBarsSet) {
						firstInstForBarsIndex = lastInstrumentIndex;
						firstInstForBarsSet = true;
					}
					std::pair<int, string> p = parseMelodicLine(restOfCommand);
					return std::make_pair(instrumentExists, p);
				}
			}
		}
	}
	return std::make_pair(instrumentExists, error);
}

//--------------------------------------------------------------
std::pair<bool, std::pair<int, string>> ofApp::isBarLoop(vector<string>& commands, int lineNum, int numLines)
{
	bool barLoopExists = false;
	std::pair p = std::make_pair(0, "");
	if (sharedData.loopsIndexes.size() > 0) {
		if (sharedData.loopsIndexes.find(commands[0]) != sharedData.loopsIndexes.end()) {
			barLoopExists = true;
			bool partsReceivedOK = true;
			int indexLocal = sharedData.loopsIndexes[commands[0]];
			// if this is the first time we call this bar/loop
			if (numScorePartSenders > 0 && partsReceivedOKCounters[indexLocal] == 0) {
				// make sure all score parts have received the data for this bar without errors
				for (unsigned i = 0; i < sharedData.loopData[indexLocal].size(); i++) {
					for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
						// if a bar from the loop we're calling hasn't been received correctly by all parts
						if (it->second.sendToPart && !it->second.barDataOKFromParts[sharedData.loopData[indexLocal][i]]) {
							partsReceivedOK = false;
							break;
						}
					}
					if (!partsReceivedOK) break;
				}
			}
			// the first time we'll call this loop, if a part hasn't received bar data correctly
			// we notify with a warning
			if (!partsReceivedOK && partsReceivedOKCounters[indexLocal] == 0) {
				string errorStr = "not all parts have received data for ";
				if (sharedData.loopData[indexLocal].size() > 1) errorStr += "this loop";
				else errorStr += "this bar";
				std::pair<int, string> p = std::make_pair(2, errorStr);
				// increment the counter so if we call this loop again it means that we want it to go through
				partsReceivedOKCounters[indexLocal]++; 
				return std::make_pair(barLoopExists, p);
			}
			// if all is OK, or if we insist on calling this loop, it goes through
			else {
				partsReceivedOKCounters[indexLocal] = 1;
				sharedData.tempBarLoopIndex = sharedData.loopsIndexes[commands[0]];
				if (sequencer.isThreadRunning()) {
					sequencer.update();
					mustUpdateScore = true;
					ofxOscMessage m;
					m.setAddress("/update");
					m.addIntArg(sharedData.tempBarLoopIndex);
					sendToParts(m);
					m.clear();
				}
				else {
					sharedData.loopIndex = sharedData.tempBarLoopIndex;
					sendLoopIndexToParts();
				}
			}
		}
	}
	return std::make_pair(barLoopExists, p);
}

//--------------------------------------------------------------
std::pair<bool, std::pair<int, string>> ofApp::isFunction(vector<string>& commands, int lineNum, int numLines)
{
	bool functionExists = false;
	bool executeFunction = false;
	std::pair<int, string> error = std::make_pair(0, "");
	if (functionIndexes.size() > 0) {
		if (functionIndexes.find(commands[0]) != functionIndexes.end()) {
			functionExists = true;
			executeFunction = true;
			lastFunctionIndex = functionIndexes[commands[0]];
			std::pair<int, string> p = functionFuncs(commands);
			if (p.first > 0) return std::make_pair(functionExists, p);
			if (p.second.compare("noexec") == 0) executeFunction = false;
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
										lineWithArgs.substr(argNdxs[i],
										nextWhiteSpaceNdx),
										sharedData.functions[lastFunctionIndex].getArgument(dollarVal-1));
					}
					else {
						lineWithArgs = replaceCharInStr(lineWithArgs, lineWithArgs.substr(argNdxs[i],
														nextWhiteSpaceNdx), "0");
					}
				}
			}
			std::pair<int, string> p = parseString(lineWithArgs, lineNum, numLines);
			if (p.first == 3) { // on error, return
				return std::make_pair(functionExists, p);
			}
		}
		int onUnbindFuncNdx = sharedData.functions[lastFunctionIndex].incrementCallCounter();
		if (onUnbindFuncNdx > -1) {
			std::pair<int, string> p = parseCommand(sharedData.functions[onUnbindFuncNdx].getName(), 1, 1);
			return std::make_pair(functionExists, p);
		}
	}
	return std::make_pair(functionExists, error);
}

//--------------------------------------------------------------
std::pair<bool, std::pair<int, string>> ofApp::isList(vector<string>& commands, int lineNum, int numLines)
{
	bool listExists = false;
	std::pair<int, string> error = std::make_pair(0, "");
	if (listIndexes.size() > 0) {
		if (listIndexes.find(commands[0]) != listIndexes.end()) {
			listExists = true;
			lastListIndex = listIndexes[commands[0]];
			if (commands.size() < 2) return std::make_pair(listExists, error);
			if (commands[1].compare("traverse") == 0) {
				traversingList = true;
				listIndexCounter = 0;
				string restOfCommand = "";
				for (unsigned i = 2; i < commands.size(); i++) {
					if (i > 2) restOfCommand += " ";
					restOfCommand += commands[i];
				}
				std::pair<int, string> p = traverseList(restOfCommand, lineNum, numLines);
				if (p.first == 3) {
					traversingList = false;
					return std::make_pair(listExists, p);
				}
			}
		}
	}
	return std::make_pair(listExists, error);
}

//--------------------------------------------------------------
std::pair<bool, std::pair<int, string>> ofApp::isOscClient(vector<string>& commands, int lineNum, int numLines)
{
	bool oscClientExists = false;
	std::pair<int, string> error = std::make_pair(0, "");
	if (oscClients.size() > 0) {
		if (oscClients.find(commands[0]) != oscClients.end()) {
			oscClientExists = true;
			if (commands.size() < 2) {
				error.first = 2;
				error.second = "no arguments provided";
				return std::make_pair(oscClientExists, error);
			}
			for (unsigned i = 1; i < commands.size(); i++) {
				if (commands[i].compare("setup") == 0) {
					if (i > 1) {
						error.first = 3;
						error.second = "\"setup\" can't be combined with other commands";
						return std::make_pair(oscClientExists, error);
					}
					if (commands.size() > i+3) {
						error.first = 3;
						error.second = "\"setup\" takes two arguments maximum, host IP and port";
						return std::make_pair(oscClientExists, error);
					}
					string host;
					int port;
					if (commands.size() > i+2) {
						if (isNumber(commands[i+1]) || !isNumber(commands[i+2])) {
							error.first = 3;
							error.second = "when two arguments are provided to \"setup\", the first must be the IP address";
							return std::make_pair(oscClientExists, error);
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
							error.first = 3;
							error.second = "if one argument is provided to \"setup\", this must be the port number";
							return std::make_pair(oscClientExists, error);
						}
					}
					oscClients[commands[0]].setup(host, port);
					return std::make_pair(oscClientExists, error);
				}
				else if (commands[i].compare("send") == 0) {
					if (commands.size() < i+1) {
						error.first = 2;
						error.second = "no data sepcified, not sending message";
						return std::make_pair(oscClientExists, error);
					}
					if (startsWith(commands[i+1], "/")) {
						if (commands.size() < i+2) {
							error.first = 2;
							error.second = "no data sepcified, not sending message";
							return std::make_pair(oscClientExists, error);
						}
						ofxOscMessage m;
						m.setAddress(commands[i+1]);
						bool allNumbers = true;
						for (unsigned j = i+2; j < commands.size(); j++) {
							if (!isNumber(commands[j])) allNumbers = false;
						}
						if (allNumbers) {
							for (unsigned j = i+2; j < commands.size(); j++) {
								m.addIntArg(stoi(commands[j]));
							}
							oscClients[commands[0]].sendMessage(m, false);
							return std::make_pair(oscClientExists, error);
						}
						string restOfCommand = "";
						for (unsigned j = i+2; j < commands.size(); j++) {
							if (j > i+2) restOfCommand += " ";
							restOfCommand += commands[j];
						}
						std::pair<int, string> errorNew = parseCommand(restOfCommand, lineNum, numLines);
						if (errorNew.first < 2) {
							vector<string> tokens = tokenizeString(errorNew.second, " ");
							for (string token : tokens) {
								if (isNumber(token)) m.addIntArg(stoi(token));
								else m.addStringArg(token);
							}
							oscClients[commands[0]].sendMessage(m, false);
							m.clear();
							return std::make_pair(oscClientExists, errorNew);
						}
						else {
							return std::make_pair(oscClientExists, errorNew);
						}
					}
					else {
						error.first = 3;
						error.second = "no OSC address specified, must begin with \"/\"";
						return std::make_pair(oscClientExists, error);
					}
				}
			}
		}
	}
	return std::make_pair(oscClientExists, error);
}

//--------------------------------------------------------------
std::pair<int, string> ofApp::functionFuncs(vector<string>& commands)
{
	for (unsigned i = 1; i < commands.size(); i++) {
		if (commands[i].compare("setargs") == 0) {
			for (unsigned j = i+1; j < commands.size(); j++) {
				sharedData.functions[lastFunctionIndex].setArgument(commands[j]);
				if (sharedData.functions[lastFunctionIndex].getArgError() > 0) {
					switch (sharedData.functions[lastFunctionIndex].getArgError()) {
						case 1:
							return std::make_pair(3, "too many arguments");
						case 2:
							return std::make_pair(3, "argument size too big");
						default:
							return std::make_pair(3, "something went wrong");
					}
				}
			}
			return std::make_pair(0, "noexec");
		}
		else if (commands[i].compare("bind") == 0) {
			if (commands.size() < i+2) {
				return std::make_pair(3, "\\bind command takes at least one argument");
			}
			if (sharedData.instrumentIndexes.find(commands[i+1]) == sharedData.instrumentIndexes.end() && \
				!(commands[i+1].compare("beat") == 0 || commands[i+1].compare("barstart") == 0 || \
				  commands[i+1].compare("framerate") == 0 || commands[i+1].compare("fr") == 0)) {
				return std::make_pair(3, commands[i+1] + (string)" is not a defined instrument");
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
						if (val < 0) return std::make_pair(3, "value can't be negative");
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
							return std::make_pair(3, "argument \"every\" to \"bind\" takes a number after it");
						}
						if (!isNumber(commands[j+1])) {
							return std::make_pair(3, "argument \"every\" to \"bind\" takes a number after it");
						}
						if (!callingStepSet) callingStep = 1;
						settingEvery = true;
					}
					else if (commands[j].compare("times") == 0) {
						if (commands.size() < j + 2) {
							return std::make_pair(3, "argument \"times\" to \"bind\" takes a number after it");
						}
						if (!isNumber(commands[j+1])) {
							return std::make_pair(3, "argument \"times\" to \"bind\" takes a number after it");
						}
						if (!callingStepSet) callingStep = 1;
						settingTimes = true;
					}
					else {
						return std::make_pair(3, commands[j] + ": unkonw argument");
					}
				}
			}
			sharedData.functions[lastFunctionIndex].setBind(instNdx, callingStep, everyOtherStep, numTimes);
			if (sendValWarning) {
				return std::make_pair(2, "starting step must be 1 or greater, setting to 1");
			}
			else return std::make_pair(0, "noexec");
		}
		else if (commands[i].compare("unbind") == 0) {
			if (commands.size() > i+1) {
				return std::make_pair(3, "\"unbind\" command takes no arguments");
			}
			sharedData.functions[lastFunctionIndex].releaseBind();
			return std::make_pair(0, "");
		}
		else if (commands[i].compare("onrelease") == 0) {
			if (commands.size() < i+2) {
				return std::make_pair(3, "\"onrelease\" takes at least one argument");
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
			return std::make_pair(0, "noexec");
		}
	}
	return std::make_pair(0, "");
}

//--------------------------------------------------------------
int ofApp::getLastLoopIndex()
{
	int barIndex = -1;
	// since every bar is copied to the loop maps, but loops are not copied to the bar map
	// we query the loopsIndexes map instead of the barIndexes map
	// so that the keys between bars and loops are the same
	if (sharedData.loopsUnordered.size() > 0) {
		map<int, string>::reverse_iterator it = sharedData.loopsUnordered.rbegin();
		barIndex = it->first;
	}
	return barIndex;
}

//--------------------------------------------------------------
int ofApp::getLastBarIndex()
{
	int barIndex = -1;
	int lastLoopIndex = getLastLoopIndex();
	// since loop indexes are not stored as bar indexes, once we get the last loop index
	// we query whether this is also a bar index, and if not, we subtract one until we find a bar index
	if (lastLoopIndex > 0) {
		while (sharedData.barsIndexes.find(sharedData.loopsUnordered[lastLoopIndex]) == sharedData.barsIndexes.end()) {
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
		while (sharedData.barsIndexes.find(sharedData.loopsUnordered[prevBarIndex]) == sharedData.barsIndexes.end()) {
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
	sharedData.tempNumBeats = sharedData.numBeats[barToCopy];
	sharedData.instruments[lastInstrumentIndex].setPassed(true);
	sharedData.instruments[lastInstrumentIndex].setCopied(barIndex, true);
	sharedData.instruments[lastInstrumentIndex].setCopyNdx(barIndex, barToCopy);
	sharedData.instruments[lastInstrumentIndex].copyMelodicLine(barIndex);
	return std::make_pair(0, "");
}

//--------------------------------------------------------------
void ofApp::deleteLastBar()
{
	if (barError) return; // if a bar error has been raised, nothing has been stored
	int bar = getLastBarIndex();
	for (map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		if (it->second.hasPassed() && it->second.notes.find(bar) != it->second.notes.end()) {
			it->second.notes.erase(bar);
			it->second.midiNotes.erase(bar);
			it->second.durs.erase(bar);
			it->second.dursWithoutSlurs.erase(bar);
			it->second.midiDursWithoutSlurs.erase(bar);
			it->second.dynamics.erase(bar);
			it->second.midiVels.erase(bar);
			it->second.pitchBendVals.erase(bar);
			it->second.dynamicsRamps.erase(bar);
			it->second.glissandi.erase(bar);
			it->second.midiGlissDurs.erase(bar);
			it->second.midiDynamicsRampDurs.erase(bar);
			it->second.articulations.erase(bar);
			it->second.midiArticulationVals.erase(bar);
			it->second.text.erase(bar);
			it->second.textIndexes.erase(bar);
			//it->second.slurBeginnings.erase(bar);
			//it->second.slurEndings.erase(bar);
			it->second.slurIndexes.erase(bar);
			it->second.isWholeBarSlurred.erase(bar);
			it->second.scoreNotes.erase(bar);
			it->second.scoreDurs.erase(bar);
			it->second.scoreDotIndexes.erase(bar);
			it->second.scoreAccidentals.erase(bar);
			it->second.scoreOctaves.erase(bar);
			it->second.scoreGlissandi.erase(bar);
			it->second.scoreArticulations.erase(bar);
			it->second.scoreDynamics.erase(bar);
			it->second.scoreDynamicsIndexes.erase(bar);
			it->second.scoreDynamicsRampStart.erase(bar);
			it->second.scoreDynamicsRampEnd.erase(bar);
			it->second.scoreDynamicsRampDir.erase(bar);
			//it->second.scoreSlurBeginnings.erase(bar);
			//it->second.scoreSlurEndings.erase(bar);
			it->second.scoreTexts.erase(bar);
			it->second.passed = false;
		}
	}
	if (sharedData.barsIndexes.find(sharedData.loopsUnordered[bar]) != sharedData.barsIndexes.end()) {
		sharedData.barsIndexes.erase(sharedData.loopsUnordered[bar]);
	}
	if (sharedData.loopsIndexes.find(sharedData.loopsUnordered[bar]) != sharedData.loopsIndexes.end()) {
		sharedData.loopsIndexes.erase(sharedData.loopsUnordered[bar]);
	}
	if (sharedData.numerator.find(bar) != sharedData.numerator.end()) {
		sharedData.numerator.erase(bar);
		sharedData.denominator.erase(bar);
	}
	sharedData.loopsUnordered.erase(bar);
	sharedData.loopsVariants.erase(bar);
}

//--------------------------------------------------------------
void ofApp::deleteLastLoop()
{
	int bar = getLastLoopIndex();
	if (sharedData.loopsIndexes.find(sharedData.loopsUnordered[bar]) != sharedData.loopsIndexes.end()) {
		sharedData.loopsIndexes.erase(sharedData.loopsUnordered[bar]);
	}
	if (partsReceivedOKCounters.find(bar) != partsReceivedOKCounters.end()) {
		partsReceivedOKCounters.erase(bar);
	}
}

//--------------------------------------------------------------
std::pair<int, string> ofApp::parseBarLoop(string str, int lineNum, int numLines)
{
	// first check if there is anything after the closing bracket
	size_t closingBracketIndex = str.find("}");
	size_t strLength = str.size();
	string restOfCommand = "";
	// storing anything after a closing bracket enables us
	// to call a loop in the same line we define it, like this
	// \loop name {\bar1 \bar2} \name
	if (closingBracketIndex != string::npos) {
		restOfCommand = str.substr(closingBracketIndex);
		strLength = closingBracketIndex;
	}
	vector<string> names = tokenizeString(str.substr(0, strLength), " ");
	// initialize all variables used in the loop below, so we can initialize the growing vector
	// after them, to ensure it is at the top of the stack
	size_t multIndex;
	size_t nameLength;
	int howManyTimes;
	map<string, int>::iterator thisBarLoopIndexIt;
	map<int, vector<int>>::iterator thisBarLoopDataIt;
	vector<int>::iterator barIndex;
	int j;
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
			howManyTimes = stoi(name.substr(multIndex+1));
		}
		else if (multIndex == 0) {
			// the multiplication symbol cannot be the first symbol in the token
			return std::make_pair(3, "the multiplication character must be concatenated to the bar/loop name");
		}
		else if (multIndex == name.size()-1) {
			// the multiplication symbol must be followed by a number
			return std::make_pair(3, "the multiplication symbol must be concatenated to a number");
		}
		// first check for barLoops because when we define a bar  with data
		// and not with combinations of other bars, we create a barLoop with the same name
		thisBarLoopIndexIt = sharedData.loopsIndexes.find(name.substr(0, nameLength));
		if (thisBarLoopIndexIt == sharedData.loopsIndexes.end()) {
			return std::make_pair(3, thisBarLoopIndexIt->first + (string)" doesn't exist");
		}
		// find the map of the loop with the vector that contains indexes of bars
		thisBarLoopDataIt = sharedData.loopData.find(thisBarLoopIndexIt->second);
		// then iterate over the number of repetitions of this loop
		for (j = 0; j < howManyTimes; j++) {
			// and push back every item in the vector that contains the indexes of bars
			for (barIndex = thisBarLoopDataIt->second.begin(); barIndex != thisBarLoopDataIt->second.end(); ++barIndex) {
				thisLoopIndexes.push_back(*barIndex);
			}
		}
	}
	// find the last index of the stored loops and store the vector we just created to the value of loopData
	int loopNdx = getLastLoopIndex();
	sharedData.loopData[loopNdx] = thisLoopIndexes;
	sendLoopDataToParts(loopNdx, thisLoopIndexes);
	if (restOfCommand.size() > 0) {
		// falsify this so that the name of the loop is treated properly in parseString()
		// otherwise, parseString() will again call this function
		parsingLoop = false;
		parseString(restOfCommand, lineNum, numLines);
	}
	if (!sequencer.isThreadRunning()) {
		sharedData.loopIndex = sharedData.tempBarLoopIndex;
		sendLoopIndexToParts();
	}
	return std::make_pair(0, "");
}

//--------------------------------------------------------------
std::pair<int, string> ofApp::parseMelodicLine(string str)
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
	if (!areBracketsBalanced(str)) {
		return std::make_pair(3, "brackets are not balanced");
	}
	bool parsingBarsFromLoop = false;
	int ndx;
	string parsingBarsFromLoopName;
	if (parsingBars && startsWith(str, "\\") && \
			(str.substr(0, str.find(" ")).compare("\\ottava") != 0 && str.substr(0, str.find(" ")).compare("\\ott") != 0) && \
			(str.substr(0, str.find(" ")).compare("\\tuplet") != 0 && str.substr(0, str.find(" ")).compare("\\tup") != 0) && \
			str.find("|") == string::npos) {
		if (sharedData.barsIndexes.find(str) == sharedData.barsIndexes.end()) {
			if (sharedData.loopsIndexes.find(str) != sharedData.loopsIndexes.end()) {
				parsingBarsFromLoop = true;
				parsingBarsFromLoopName = str;
			}
			else {
				return std::make_pair(3, (string)"bar/loop " + str + (string)" doesn't exist");
			}
		}
	}
	size_t strBegin, strLen;
	string strToProcess = detectRepetitions(str);
	if (parsingBars) {
		if (parsingBarsFromLoop) {
			if (lastInstrumentIndex == firstInstForBarsIndex) {
				numBarsParsed = (int)sharedData.loopData[sharedData.loopsIndexes[parsingBarsFromLoopName]].size(); //barsIterCounter;
			}
			else {
				// left operand of if below was barsIterCounter
				int size = (int)sharedData.loopData[sharedData.loopsIndexes[parsingBarsFromLoopName]].size();
				if (size != numBarsParsed) {
					return std::make_pair(3, (string)"parsed " + to_string(size) + (string)" bars, while first instrument parsed " + to_string(numBarsParsed));
				}
			}
			if (barsIterCounter-1 >= (int)sharedData.loopData[sharedData.loopsIndexes[parsingBarsFromLoopName]].size()-1) {
				sharedData.instruments[lastInstrumentIndex].setMultiBarsDone(true);
			}
			ndx = sharedData.loopData[sharedData.loopsIndexes[parsingBarsFromLoopName]][barsIterCounter-1];
			strToProcess = sharedData.loopsUnordered[ndx];
		}
		else {
			strBegin = sharedData.instruments[lastInstrumentIndex].getMultiBarsStrBegin();
			strLen = strToProcess.substr(strBegin).find("|");
			vector<string> bars = tokenizeString(strToProcess, "|"); // to get the number of bars
			if (lastInstrumentIndex == firstInstForBarsIndex) {
				numBarsParsed = (int)bars.size();
			}
			else {
				// left operand of if below was barsIterCounter
				int size = (int)bars.size();
				if (size != numBarsParsed) {
					return std::make_pair(3, (string)"parsed " + to_string(size) + (string)" bars, while first instrument parsed " + to_string(numBarsParsed));
				}
			}
			if (strLen != string::npos) {
				strToProcess = strToProcess.substr(strBegin, strLen-1);
				sharedData.instruments[lastInstrumentIndex].setMultiBarsStrBegin(strBegin+strLen+2);
			}
			else {
				strToProcess = strToProcess.substr(strBegin);
				sharedData.instruments[lastInstrumentIndex].setMultiBarsDone(true);
			}
		}
	}
	if (startsWith(strToProcess, "\\") && \
			(strToProcess.substr(0, strToProcess.find(" ")).compare("\\ottava") != 0 && strToProcess.substr(0, strToProcess.find(" ")).compare("\\ott") != 0) && \
			(strToProcess.substr(0, strToProcess.find(" ")).compare("\\tuplet") != 0 && strToProcess.substr(0, strToProcess.find(" ")).compare("\\tup") != 0)) {
		if (sharedData.barsIndexes.find(strToProcess) != sharedData.barsIndexes.end()) {
			std::pair<int, string> p = stripLineFromBar(strToProcess);
			return p;
		}
		else {
			return std::make_pair(3, strToProcess + ": bar doesn't exist");
		}
	}
	int tempDur = 0;
	// the following array is used to map the dynamic values
	// to the corresponding MIDI velocity values
	int dynsArr[8] = {-9, -6, -3, -1, 1, 3, 6, 9};
	// an array to test against durations
	int dursArr[7] = {1, 2, 4, 8, 16, 32, 64};
	// the notes of the natural scale in MIDI values
	int midiNotes[7] = {0, 2, 4, 5, 7, 9, 11};
	bool dynamicsRampStarted = false;
	// booleans to determine whether we're writing a chord or not
	bool chordStarted = false;
	bool firstChordNote = false;
	
	bool foundArticulation = false;
	// boolean for dynamics, whether it's a mezzo forte or mezzo piano
	bool mezzo = false;
	vector<string> tokens = tokenizeString(strToProcess, " ");
	// index variables for loop so that they are initialized before vectors with unkown size
	unsigned i = 0, j = 0; //  initialize to avoid compiler warnings
	// first detect if there are any quotes, which might include one or more white spaces
	for (i = 0; i < tokens.size(); i++) {
		// first determine if we have both quotes in the same token
		vector<int> quoteNdx = findIndexesOfCharInStr(tokens[i], "\"");
		if (quoteNdx.size() > 1) {
			continue;
		}
		// if we find a quote and we're not at the last token
		if (tokens[i].find("\"") != string::npos && i < (tokens.size()-1)) {
			unsigned ndx = i+1;
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
	// then detect any chords, as the size of most vectors equals the number of notes vertically
	// and not single notes within chords
	unsigned numNotesVertical = tokens.size();
	unsigned chordNotesCounter = 0;
	unsigned numErasedOttTokens = 0;
	// characters that are not permitted inside a chord are
	// open/close parenthesis, hyphen, dot, backslash, carret, underscore, open/close curly brackets
	int forbiddenCharsInChord[9] = {40, 41, 45, 46, 92, 94, 95, 123, 125};
	for (i = 0; i < tokens.size(); i++) {
		if (tokens[i].compare("\\ottava") == 0 || tokens[i].compare("\\ott") == 0) {
			if (tokens.size() < i+2) {
				return std::make_pair(3, "\\ottava must be followed by (-)1 or (-)2");
			}
			if (!isNumber(tokens[i+1])) {
				return std::make_pair(3, "\\ottava must be followed by (-)1 or (-)2");
			}
			numErasedOttTokens += 2;
			i += 1;
			continue;
		}
		if (tokens[i].compare("\\tuplet") == 0 || tokens[i].compare("\\tup") == 0) {
			if (tokens.size() < i+3) {
				return std::make_pair(3, "\\tuplet must be followed by ration and the actual tuplet");
			}
			if (tokens[i+1].find("/") == string::npos) {
				return std::make_pair(3, "\\tuplet ratio formatted wrong");
			}
			size_t divisionNdx = tokens[i+1].find("/");
			size_t remainingStr = tokens[i+1].size() - divisionNdx - 1;
			if (!isNumber(tokens[i+1].substr(0, divisionNdx)) || !isNumber(tokens[i+1].substr(divisionNdx+1, remainingStr))) {
				return std::make_pair(3, "numerator or denominator of tuplet ratio is not a number");
			}
			if (!startsWith(tokens[i+2], "{")) {
				return std::make_pair(3, "tuplet notes must be placed in curly brackets");
			}
		}
		if (chordStarted) {
			bool chordEnded = false;
			for (j = 0; j < 9; j++) {
				if (tokens[i].find(char(forbiddenCharsInChord[j])) != string::npos) {
					return std::make_pair(3, char(forbiddenCharsInChord[j]) + (string)" can't be included within a chord, only outside of it");
				}
			}
			for (j = 0; j < tokens[i].size(); j++) {
				if (tokens[i][j] == '>') chordEnded = true;
				else if (isdigit(tokens[i][j]) && !chordEnded) {
					return std::make_pair(3, "durations can't be included within a chord, only outside of it");
				}
			}
			chordNotesCounter++;
		}
		// if we find a quote and we're not at the last token
		unsigned chordOpeningNdx = tokens[i].find("<");
		unsigned chordClosingNdx = tokens[i].find(">");
		// an opening angle brace is also used for crescendi, but when used to start a chord
		// it must be at the beginning of the token, so we test if its index is 0
		if (chordOpeningNdx != string::npos && chordOpeningNdx == 0 && i < tokens.size()) {
			if (chordStarted) {
				if (chordOpeningNdx > 0) {
					if (tokens[i][chordOpeningNdx-1] == '\\') {
						return std::make_pair(3, "crescendi can't be included within a chord, only outside of it");
					}
					else {
						return std::make_pair(3, "chords can't be nested");
					}
				}
				else return std::make_pair(3, "chords can't be nested");
			}
			if (sharedData.instruments[lastInstrumentIndex].isRhythm()) {
				return std::make_pair(3, "chords are not allowed in rhythm staffs");
			}
			chordStarted = true;
		}
		else if (chordClosingNdx != string::npos && chordClosingNdx < tokens[i].size()) {
			if (chordClosingNdx == 0) {
				return std::make_pair(3, "chord closing character can't be at the beginning of a token");
			}
			else if (chordStarted && tokens[i][chordClosingNdx-1] == '\\') {
				return std::make_pair(3, "diminuendi can't be included within a chord, only outside of it");
			}
			else if (!chordStarted && tokens[i][chordClosingNdx-1] != '-' && tokens[i][chordClosingNdx-1] != '\\') {
				return std::make_pair(3, "can't close a chord that hasn't been opened");
			}
			chordStarted = false;
		}
	}
	if (chordStarted) {
		return std::make_pair(3, "chord left open");
	}
	if (tokens.size() == numErasedOttTokens) return std::make_pair(1, ""); // in case we only provide the ottava
	numNotesVertical -= (chordNotesCounter + numErasedOttTokens);

	// check for tuplets
	int tupMapNdx = 0;
	map<int, std::pair<int, int>> tupRatios;
	for (i = 0; i < tokens.size(); i++) {
		if (tokens[i].compare("\\tuplet") == 0 || tokens[i].compare("\\tup") == 0) {
			// tests whether the tuplet format is correct have been made above
			// so we can safely query tokens[i+1] etc
			size_t divisionNdx = tokens[i+1].find("/");
			size_t remainingStr = tokens[i+1].size() - divisionNdx - 1;
			tupRatios[tupMapNdx++] = std::make_pair(stoi(tokens[i+1].substr(0, divisionNdx)), stoi(tokens[i+1].substr(divisionNdx+1, remainingStr)));
		}
	}

	// store the indexes of the tuplets beginnings and endings so we can erase all the unnessacary tokens
	unsigned tupStart, tupStop = 0; // assign 0 to tupStop to stop the compiler from throwing a warning message
	int tupStartHasNoBracket = 0, tupStopHasNoBracket = 0;
	int numErasedTupTokens = 0;
	tupMapNdx = 0;
	map<int, std::pair<unsigned, unsigned>> tupStartStop;
	for (i = 0; i < tokens.size(); i++) {
		if (tokens[i].compare("\\tuplet") == 0 || tokens[i].compare("\\tup") == 0) {
			if (tokens[i+2].compare("{") == 0) {
				tupStart = i+3;
				tupStartHasNoBracket = 1;
			}
			else {
				tupStart = i+2;
			}
			for (j = i+2+tupStartHasNoBracket; j < tokens.size(); j++) {
				if (tokens[j].find("}") != string::npos) {
					if (tokens[j].compare("}") == 0) {
						tupStop = j-1;
						tupStopHasNoBracket = 1;
					}
					else {
						tupStop = j;
					}
					break;
				}
			}
			// offset the start and stop of the tuplet
			tupStart -= (2 + tupStartHasNoBracket);
			tupStop -= (2 + tupStartHasNoBracket);
			// now that we have temporarily stored the start and end of this tuplet
			// we can erase it from the tokens vector and correct the stored indexes
			// before we store them to the vector of pairs
			// erase \tuplet and ratio
			tokens.erase(tokens.begin() + i);
			tokens.erase(tokens.begin() + i);
			// check if the opening and closing curly bracket is isolated from the tuplet body
			if (tupStartHasNoBracket) tokens.erase(tokens.begin() + tupStart);
			if (tupStopHasNoBracket) tokens.erase(tokens.begin() + tupStop + 1);
			tupStartStop[tupMapNdx++] = std::make_pair(tupStart, tupStop);
			numErasedTupTokens += (2 + tupStartHasNoBracket + tupStopHasNoBracket);
			tupStartHasNoBracket = tupStopHasNoBracket = 0;
			i = tupStop;
		}
	}
	numNotesVertical -= numErasedTupTokens;

	// once we define the number of tokens and vertical notes, we can create the vectors with known size
	vector<int> durIndexes(numNotesVertical, -1);
	vector<int> dynamicsIndexes(numNotesVertical, -1);
	vector<int> midiVels(numNotesVertical, 0);
	vector<int> midiPitchBendVals(numNotesVertical, 0);
	vector<int> dursData(numNotesVertical, 0);
	vector<int> dursDataWithoutSlurs(numNotesVertical, 0);
	vector<int> midiDursDataWithoutSlurs(numNotesVertical, 0);
	// pitch bend values are calculated by the sequencer
	// so here we store the microtones only
	vector<int> pitchBendVals(numNotesVertical, 0);
	vector<int> dotIndexes(numNotesVertical, 0);
	vector<int> dynamicsData(numNotesVertical, 0);
	vector<int> dynamicsRampData(numNotesVertical, 0);
	vector<int> slurBeginningsIndexes(numNotesVertical, -1);
	vector<int> slurEndingsIndexes(numNotesVertical, -1);
	vector<int> textIndexesLocal(numNotesVertical, 0);
	vector<int> dursForScore(numNotesVertical, 0);
	vector<int> dynamicsForScore(numNotesVertical, -1);
	vector<int> dynamicsForScoreIndexes(numNotesVertical, -1);
	vector<int> dynamicsRampStart(numNotesVertical, 0);
	vector<int> dynamicsRampEnd(numNotesVertical, 0);
	vector<pair<unsigned, unsigned>> dynamicsRampIndexes(numNotesVertical);
	vector<int> midiDynamicsRampDurs(numNotesVertical, 0);
	vector<int> glissandiIndexes(numNotesVertical, 0);
	vector<int> midiGlissDurs(numNotesVertical, 0);
	vector<int> articulationIndexes(numNotesVertical, 0);
	vector<int> midiArticulationVals(numNotesVertical, 0);
	vector<int> dynamicsRampStartForScore(numNotesVertical, -1);
	vector<int> dynamicsRampEndForScore(numNotesVertical, -1);
	vector<int> dynamicsRampDirForScore(numNotesVertical, -1);
	vector<bool> isSlurred (numNotesVertical, false);
	vector<int> notesCounter(tokens.size()-numErasedOttTokens, 0);
	int verticalNotesIndexes[tokens.size()-numErasedOttTokens+1] = {0}; // plus one to easily break out of loops in case of chords
	int beginningOfChords[tokens.size()-numErasedOttTokens] = {0};
	int endingOfChords[tokens.size()-numErasedOttTokens] = {0};
	unsigned accidentalIndexes[tokens.size()-numErasedOttTokens];
	unsigned chordNotesIndexes[tokens.size()-numErasedOttTokens] = {0};
	vector<int> ottavas(numNotesVertical, 0);
	int verticalNoteIndex;
	// create variables for the loop below but outside of it, so they are kept in the stack
	// this way we are sure the push_back() calls won't cause any problems
	bool foundNotes[i];
	int transposedOctaves[i-numErasedOttTokens] = {0};
	int transposedAccidentals[i-numErasedOttTokens] = {0};
	// variable to determine which character we start looking at
	// useful in case we start a bar with a chord
	unsigned firstChar = 0; // initialize to 0 to avoid compiler warning
	unsigned firstCharOffset = 1; // this is zeroed in case of a rhythm staff with durations only
	int midiNote, naturalScaleNote; // the second is for the score
	int ottava = 0;
	// boolean used to set the values to isSlurred vector
	bool slurStarted = false;
	// a counter for the number of notes in each chord
	chordNotesCounter = 0;
	unsigned index1 = 0, index2 = 0; // variables to index various data
	int erasedTokens = 0;
	
	// create an unpopullated vector of vector of pairs of the notes as MIDI and natural scale
	// after all vectors with known size and all single variables have been created
	vector<vector<std::pair<int, int>>> notePairs;
	// then iterate over all the tokens and parse them to store the notes
	for (i = 0; i < tokens.size(); i++) {
		if (erasedTokens > 0) {
			// go back as many indexes as we have erased in case we have an \ottava or \tuplet 
			// the loop returned to its last field that incremented i, so we need to decrement it
			i -= erasedTokens;
			erasedTokens = 0;
		}
		if (tokens[i].compare("\\ottava") == 0 || tokens[i].compare("\\ott") == 0) {
			// tests whether a digit follows \ottava have been made above
			// so we can now safely query tokens[i+1] and extract the digit that follows
			ottava = stoi(tokens[i+1]);
			if (abs(ottava) > 2) {
				return std::make_pair(3, "up to two octaves up/down can be inserted with \\ottava");
			}
			// erase the \ottava val tokens from the tokens vector
			tokens.erase(tokens.begin() + i);
			tokens.erase(tokens.begin() + i);
			// check if we need to change any tuplet indexes
			for (auto it = tupStartStop.begin(); it != tupStartStop.end(); ++it) {
				if (i >= it->second.first && i <= it->second.second) {
					it->second.first -= 2;
					it->second.second -= 2;
					// store the current position of the iterator
					auto it2 = it;
					// then increment and check for the rest of the tuplets
					++it2;
					// check for any possible remaining tuplets after this one
					while (it2 != tupStartStop.end()) {
						it2->second.first -= 2;
						it2->second.second -= 2;
						++it2;
					}
				}
			}
			erasedTokens = 1;
			continue;
		}
		// ignore curly brackets that belong to \tuplets
		if (startsWith(tokens[i], "{")) tokens[i] = tokens[i].substr(1);
		if (endsWith(tokens[i], "}")) tokens[i] = tokens[i].substr(0, tokens[i].size()-1);
		verticalNoteIndex = -1;
		for (j = 0; j <= i; j++) {
			// loop till we find a -1, which means that this token is a chord note, but not the first one
			if (verticalNotesIndexes[j] < 0) continue;
			verticalNoteIndex++;
		}
		ottavas[verticalNoteIndex] = ottava;
		firstChar = 0;
		// first check if the token is a comment so we exit the loop
		if (startsWith(tokens[i], "%")) continue;
		foundNotes[i] = false;
		transposedOctaves[index1] = 0;
		transposedAccidentals[index1] = 0;
		// the first element of each token is the note
		// so we first check for it in a separate loop
		// and then we check for the rest of the stuff
	parseStart:
		for (j = 0; j < 8; j++) {
			if (tokens[i][firstChar] == noteChars[j]) {
				midiNote = -1;
				if (j < 7) {
					midiNote = midiNotes[j];
					// we start one octave below the middle C
					midiNote += 48;
					naturalScaleNote = j;
					if (sharedData.instruments[lastInstrumentIndex].isRhythm()) {
						midiNote = 59;
						naturalScaleNote = 6;
					}
					else if (sharedData.instruments[lastInstrumentIndex].getTransposition() != 0) {
						int transposedMidiNote = midiNotes[j] + sharedData.instruments[lastInstrumentIndex].getTransposition();
						// testing against the midiNote it is easier to determine whether we need to add an accidental
						if (transposedMidiNote < 0) {
							transposedMidiNote = 12 + midiNotes[j] + sharedData.instruments[lastInstrumentIndex].getTransposition();
						}
						else if (transposedMidiNote > 11) {
							transposedMidiNote %= 12;
						}
						if (midiNotes[j] < 4) {
							transposedOctaves[index1] -= 1;
						}
						if (transposedMidiNote > 4 && (transposedMidiNote % 2) == 0) {
							transposedAccidentals[index1] = 2;
						}
						else if (transposedMidiNote <= 4 && (transposedMidiNote % 2) == 1) {
							transposedAccidentals[index1] = 2;
						}
						naturalScaleNote = distance(midiNotes, find(begin(midiNotes), end(midiNotes), (transposedMidiNote-(transposedAccidentals[index1]/2))));
					}
				}
				else {
					// the last element of the noteChars array is the rest
					midiNote = naturalScaleNote = -1;
				}
			storeNote:
				if (!chordStarted || (chordStarted && firstChordNote)) {
					// create a new vector for each single note or a group of notes of a chord
					notePairs.push_back({std::make_pair(midiNote, naturalScaleNote)});
					verticalNotesIndexes[i] = i;
					chordNotesIndexes[i] = 0;
					chordNotesCounter = 0;
                    notesCounter[i]++;
					dotIndexes[index2] = 0;
					glissandiIndexes[index2] = 0;
					midiGlissDurs[index2] = 0;
					midiDynamicsRampDurs[index2] = 0;
					articulationIndexes[index2] = 0;
					midiArticulationVals[index2] = 1;
					pitchBendVals[index2] = 0;
					textIndexesLocal[index2] = 0;
					index2++;
					if (firstChordNote) firstChordNote = false;
				}
				else if (chordStarted && !firstChordNote) {
					// if we have a chord, push this note to the current vector
					notePairs.back().push_back(std::make_pair(midiNote, naturalScaleNote));
					// a -1 will be filtered out further down in the code
					verticalNotesIndexes[index1] = -1;
					// increment the counter of the chord notes
					// so we can set the index for each note in a chord properly
					chordNotesCounter++;
					chordNotesIndexes[index1] += chordNotesCounter;
                    notesCounter[index1]++;
				}
				else {
					return std::make_pair(3, "something went wrong");
				}
				foundNotes[i] = true;
				break;
			}
		}
		if (!foundNotes[i]) {
			if (sharedData.instruments[lastInstrumentIndex].isRhythm()) {
				// to be able to write a duration without a note, for rhythm staffs
				// we need to check if the first character is a number
				int dur = 0;
				if (tokens[i].size() > 1) {
					if (isdigit(tokens[i][0]) && isdigit(tokens[i][1])) {
						dur = int(tokens[i][0]+tokens[i][1]);
					}
					else if (isdigit(tokens[i][0])) {
						dur = int(tokens[i][0]);
					}
					else {
						return std::make_pair(3, (string)"first character must be a note or a chord opening symbol (<), not \"" + tokens[i][0] + (string)"\"");
					}
				}
				else if (isdigit(tokens[i][0])) {
					dur = int(tokens[i][0]);
				}
				else {
					return std::make_pair(3, (string)"first character must be a note or a chord opening symbol (<), not \"" + tokens[i][0] +(string)"\"");
				}
				if (std::find(std::begin(dursArr), std::end(dursArr), dur) == std::end(dursArr)) {
					midiNote = 59;
					naturalScaleNote = 6;
					// we need to assign 0 to firstCharOffset which defaults to 1
					// because further down it is added to firstChar
					// but, in this case, we want to check the token from its beginning, index 0
					firstCharOffset = 0;
					goto storeNote;
				}
				else {
					return std::make_pair(3, to_string(dur) + ": wrong duration");
				}
			}
			else if (tokens[i][firstChar] == char(60)) { // <
				chordStarted = true;
				beginningOfChords[i] = 1;
				firstChordNote = true;
				firstChar = 1;
				goto parseStart; // reiterate if starting with a chord symbol
			}
			else {
				return std::make_pair(3, (string)"first character must be a note or a chord opening symbol (<), not \"" + tokens[i][0] + (string)"\"");
			}
		}
		else {
			if (chordStarted && tokens[i].find(">") != string::npos) {
				chordStarted = false;
				firstChordNote = false;
				endingOfChords[i] = 1;
			}
		}
		index1++;
	}
	
	// reset the indexes
	index1 = index2 = 0;

	// now we have enough information to allocate memory for the rest of the vectors
	// but we do it one by one, to allocate the required memory only
	// as each note might be a chord or not
	vector<vector<int>> midiNotesData;
	for (i = 0; i < notePairs.size(); i++) {
		midiNotesData.push_back({notePairs[i][0].first});
		for (j = 1; j < notePairs[i].size(); j++) {
			midiNotesData.back().push_back(notePairs[i][j].first);
		}
	}
	vector<vector<float>> notesData;
	for (i = 0; i < midiNotesData.size(); i++) {
		notesData.push_back({(float)midiNotesData[i][0]});
		for (j = 1; j < midiNotesData[i].size(); j++) {
			notesData.back().push_back((float)midiNotesData[i][j]);
		}
	}
	vector<vector<int>> notesForScore;
	for (i = 0; i < notePairs.size(); i++) {
		notesForScore.push_back({notePairs[i][0].second});
		for (j = 1; j < notePairs[i].size(); j++) {
			notesForScore.back().push_back(notePairs[i][j].second);
		}
	}
	vector<vector<int>> accidentalsForScore;
	for (i = 0; i < midiNotesData.size(); i++) {
		accidentalsForScore.push_back({-1});
		for (j = 1; j < midiNotesData[i].size(); j++) {
			accidentalsForScore.back().push_back(-1);
		}
	}
	// the 2D vector below is used here only for memory allocation
	// its contents are updated in drawNotes() of the Notes() class
	vector<vector<int>> naturalSignsNotWrittenForScore;
	for (i = 0; i < midiNotesData.size(); i++) {
		naturalSignsNotWrittenForScore.push_back({0});
		for (j = 1; j < midiNotesData[i].size(); j++) {
			naturalSignsNotWrittenForScore.back().push_back(0);
		}
	}
	int transposedIndex = 0;
	vector<vector<int>> octavesForScore;
	for (i = 0; i < midiNotesData.size(); i++) {
		if (sharedData.instruments[lastInstrumentIndex].isRhythm()) {
			octavesForScore.push_back({1});
		}
		else {
			octavesForScore.push_back({transposedOctaves[transposedIndex++]});
		}
		for (j = 1; j < midiNotesData[i].size(); j++) {
			octavesForScore.back().push_back(transposedOctaves[transposedIndex++]);
		}
	}

	// before we move on to the rest of the data, we need to store the texts
	// because this is the last bit of dynamically allocated memory
	unsigned openQuote, closeQuote;
	vector<string> texts;
	for (i = 0; i < tokens.size(); i++) {
		// again, check if we have a comment, so we exit the loop
		if (startsWith(tokens[i], "%")) break;
		verticalNoteIndex = 0;
		for (unsigned k = 0; k <= i; k++) {
			// loop till we find a -1, which means that this token is a chord note, but not the first one
			if (verticalNotesIndexes[k] < 0) continue;
			verticalNoteIndex = k;
		}
		for (j = 0; j < tokens[i].size(); j++) {
			// ^ or _ for adding text above or below the note
			if ((tokens[i][j] == char(94)) || (tokens[i][j] == char(95))) {
				if (j > 0) {
					if (tokens[i][j-1] == '-') foundArticulation = true;
				}
				if (j >= (tokens[i].size()-1) && !foundArticulation) {
					if (tokens[i][j] == char(94)) {
						return std::make_pair(3, "a carret must be followed by text in quotes");
					}
					else {
						return std::make_pair(3, "an undescore must be followed by text in quotes");
					}
				}
				if (j < tokens[i].size()-1) {
					if (tokens[i][j+1] != char(34) && !foundArticulation) { // "
						if (tokens[i][j] == char(94)) {
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
					while (index2 < tokens[i].size()) {
						if (tokens[i][index2] == char(34)) {
							closeQuote = index2;
							break;
						}
						index2++;
					}
					if (closeQuote <= openQuote) {
						return std::make_pair(3, "text must be between two double quote signs");
					}
					texts.push_back(tokens[i].substr(openQuote, closeQuote-openQuote));
					if (tokens[i][j] == char(94)) {
						textIndexesLocal[verticalNoteIndex] = 1;
					}
					else if (tokens[i][j] == char(95)) {
						textIndexesLocal[verticalNoteIndex] = -1;
					}
					j = closeQuote;
				}
			}
		}
		foundArticulation = false;
	}
	// then copy the texts to textsForScore
	vector<string> textsForScore;
	for (i = 0; i < texts.size(); i++) {
		textsForScore.push_back(texts[i]);
	}

	// we are now done with dynamically allocating memory, so we can move on to the rest of the data
	// we can now create more variables without worrying about memory
	int foundAccidentals[tokens.size()] = {0};
	float accidental = 0.0;
	// various counters
	unsigned numDurs = 0;
	unsigned numDynamics = 0;
	unsigned dynamicsRampCounter = 0;
	unsigned slursCounter = 0;
	unsigned dotsCounter = 0;
	for (i = 0; i < tokens.size(); i++) {
		verticalNoteIndex = -1;
		for (j = 0; j <= i; j++) {
			// loop till we find a -1, which means that this token is a chord note, but not the first one
			if (verticalNotesIndexes[j] < 0) continue;
			verticalNoteIndex++;
		}
		// first check for accidentals, if any
		accidentalIndexes[i] = firstChar + firstCharOffset;
		while (accidentalIndexes[i] < tokens[i].size()) {
			if (tokens[i][accidentalIndexes[i]] == char(101)) { // 101 is "e"
				if (accidentalIndexes[i] < tokens[i].size()-1) {
					// if the character after "e" is "s" or "h" we have an accidental
					if (tokens[i][accidentalIndexes[i]+1] == char(115)) { // 115 is "s"
						accidental -= 1.0; // in which case we subtract one semitone
						foundAccidentals[i] = 1;
					}
					else if (tokens[i][accidentalIndexes[i]+1] == char(104)) { // 104 is "h"
						accidental -= 0.5;
						foundAccidentals[i] = 1;
					}
					else {
						return std::make_pair(3, tokens[i][accidentalIndexes[i]+1] + (string)": unknown accidental character");
					}
				}
				else {
					return std::make_pair(3, "\"e\" must be followed by \"s\" or \"h\"");
				}
			}
			else if (tokens[i][accidentalIndexes[i]] == char(105)) { // 105 is "i"
				if (accidentalIndexes[i] < tokens[i].size()-1) {
					if (tokens[i][accidentalIndexes[i]+1] == char(115)) { // 115 is "s"
						accidental += 1.0; // in which case we add one semitone
						foundAccidentals[i] = 1;
					}
					else if (tokens[i][accidentalIndexes[i]+1] == char(104)) { // 104 is "h"
						accidental += 0.5;
						foundAccidentals[i] = 1;
					}
					else {
						return std::make_pair(3, tokens[i][accidentalIndexes[i]+1] + (string)": unknown accidental character");
					}
				}
				else {
					return std::make_pair(3, "\"i\" must be followed by \"s\" or \"h\"");
				}
			}
			// we ignore "s" and "h" as we have checked them above
			else if (tokens[i][accidentalIndexes[i]] == char(115) || tokens[i][accidentalIndexes[i]] == char(104)) {
				accidentalIndexes[i]++;
				continue;
			}
			// when the accidentals characters are over we move on
			else {
				break;
			}
			accidentalIndexes[i]++;
		}
		if (foundAccidentals[i]) {
			notesData[verticalNoteIndex][chordNotesIndexes[i]] += accidental;
			midiNotesData[verticalNoteIndex][chordNotesIndexes[i]] += (int)accidental;
			// accidentals can go up to a whole tone, which is accidental=2.0
			// but in case it's one and a half tone, one tone is already added above
			// so we need the half tone only, which we get with accidental-(int)accidental
			pitchBendVals[verticalNoteIndex] = (abs(accidental)-abs((int)accidental));
			if (accidental < 0.0) pitchBendVals[verticalNoteIndex] *= -1.0;
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
			accidentalsForScore[verticalNoteIndex][chordNotesIndexes[i]] = mapped;
			//accidentalsForScore[verticalNoteIndex][chordNotesIndexes[i]] += transposedAccidentals[i];
			// go back one character because of the last accidentalIndexes[i]++ above this if chunk
			accidentalIndexes[i]--;
		}
		else {
			accidentalsForScore[verticalNoteIndex][chordNotesIndexes[i]] = -1;
		}
		// add the accidentals based on transposition (if set)
		if (accidentalsForScore[verticalNoteIndex][chordNotesIndexes[i]] == -1) {
			// if we have no accidental, add the transposition to the natural sign indexes with 4
			accidentalsForScore[verticalNoteIndex][chordNotesIndexes[i]] = 4 + transposedAccidentals[i];
		}
		else {
			accidentalsForScore[verticalNoteIndex][chordNotesIndexes[i]] += transposedAccidentals[i];
		}
		// if the transposed accidental results in natural, assign -1 instead of 4 so as to not display the natural sign
		// if the natural sign is needed to be displayed, this will be taken care of at the end of the last loop
		// that iterates through the tokens of the string we parse in this function
		if (accidentalsForScore[verticalNoteIndex][chordNotesIndexes[i]] == 4) {
			accidentalsForScore[verticalNoteIndex][chordNotesIndexes[i]] = -1;
		}
		accidental = 0;
	}
	
	int foundDynamics[tokens.size()] = {0};
	int foundOctaves[tokens.size()] = {0};
	bool dynAtFirstNote = false;
	unsigned beginningOfChordIndex = 0;
	bool tokenInsideChord = false;
	int prevScoreDynamic = -1;
	int quotesCounter = 0; // so we can ignore anything that is passed as text
	// now check for the rest of the characters of the token, so start with j = accIndex
	for (i = 0; i < tokens.size(); i++) {
		foundArticulation = false;
		bool foundStaccato = false;
		unsigned firstArticulIndex = 0;
		verticalNoteIndex = -1;
		for (j = 0; j <= i; j++) {
			// loop till we find a -1, which means that this token is a chord note, but not the first one
			if (verticalNotesIndexes[j] < 0) continue;
			verticalNoteIndex++;
		}
		if (beginningOfChords[i]) {
			beginningOfChordIndex = i;
			tokenInsideChord = true;
		}
		// first check for octaves
		for (j = accidentalIndexes[i]; j < tokens[i].size(); j++) {
			if (int(tokens[i][j]) == 39) {
				if (!sharedData.instruments[lastInstrumentIndex].isRhythm()) {
					notesData[verticalNoteIndex][chordNotesIndexes[i]] += 12.0;
					midiNotesData[verticalNoteIndex][chordNotesIndexes[i]] += 12;
					octavesForScore[verticalNoteIndex][chordNotesIndexes[i]]++;
				}
				foundOctaves[i] = 1;
			}
			else if (int(tokens[i][j]) == 44) {
				if (!sharedData.instruments[lastInstrumentIndex].isRhythm()) {
					notesData[verticalNoteIndex][chordNotesIndexes[i]] -= 12.0;
					midiNotesData[verticalNoteIndex][chordNotesIndexes[i]] -= 12;
					octavesForScore[verticalNoteIndex][chordNotesIndexes[i]]--;
				}
				foundOctaves[i] = 1;
			}
		}
		// then check for the rest of the characters
		for (j = accidentalIndexes[i]; j < tokens[i].size(); j++) {
			// we don't check inside chords, as the characters we look for in this loop
			// are not permitted inside chords, so we check only at the last chord note
			// verticalNotesIndexes is tokens.size() + 1 so it's safe to test against i + 1 here
			if (verticalNotesIndexes[i+1] == -1) break;
			if (int(tokens[i][j]) == 34 && quotesCounter >= 1) {
				quotesCounter++;
				if (quotesCounter > 2) quotesCounter = 0;
			}
			if (quotesCounter > 0) continue; // if we're inside text, ignore
			if (isdigit(tokens[i][j])) {
				// assemble the value from its ASCII characters
				tempDur = tempDur * 10 + int(tokens[i][j]) - 48;
				if (verticalNoteIndex == 0) {
					dynAtFirstNote = true;
				}
				if (verticalNoteIndex > 0 && !tokenInsideChord && !dynAtFirstNote) {
					return std::make_pair(3, "first note doesn't have a duration");
				}
			}
			
			else if (tokens[i][j] == char(92)) { // back slash
				index2 = j;
				int dynamic = 0;
				bool foundDynamicLocal = false;
				bool foundGlissandoLocal = false;
				if (index2 > (tokens[i].size()-1)) {
					return std::make_pair(3, "a backslash must be followed by a command");
				}
				// loop till you find all dynamics or other commands
				while (index2 < tokens[i].size()) {
					if (tokens[i][index2+1] == char(102)) { // f for forte
						if (mezzo) {
							dynamic++;
							mezzo = false;
						}
						else {
							dynamic += 3;
						}
						dynamicsIndexes[verticalNoteIndex] = verticalNoteIndex;
						dynamicsForScoreIndexes[verticalNoteIndex] = verticalNoteIndex;
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							dynamicsRampEnd[verticalNoteIndex] = verticalNoteIndex;
							dynamicsRampEndForScore[verticalNoteIndex] = verticalNoteIndex;
							dynamicsRampIndexes[dynamicsRampCounter-1].second = verticalNoteIndex;
						}
						foundDynamics[i] = 1;
						foundDynamicLocal = true;
					}
					else if (tokens[i][index2+1] == char(112)) { // p for piano
						if (mezzo) {
							dynamic--;
							mezzo = false;
						}
						else {
							dynamic -= 3;
						}
						dynamicsIndexes[verticalNoteIndex] = verticalNoteIndex;
						dynamicsForScoreIndexes[verticalNoteIndex] = verticalNoteIndex;
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							dynamicsRampEnd[verticalNoteIndex] = verticalNoteIndex;
							dynamicsRampEndForScore[verticalNoteIndex] = verticalNoteIndex;
							dynamicsRampIndexes[dynamicsRampCounter-1].second = verticalNoteIndex;
						}
						foundDynamics[i] = 1;
						foundDynamicLocal = true;
					}
					else if (tokens[i][index2+1] == char(109)) { // m for mezzo
						mezzo = true;
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							dynamicsRampEnd[verticalNoteIndex] = verticalNoteIndex;
							dynamicsRampEndForScore[verticalNoteIndex] = verticalNoteIndex;
							dynamicsRampIndexes[dynamicsRampCounter-1].second = verticalNoteIndex;
						}
					}
					else if (tokens[i][index2+1] == char(60)) { // <
						if (!dynamicsRampStarted) {
							dynamicsRampStarted = true;
							dynamicsRampStart[verticalNoteIndex] = verticalNoteIndex;
							dynamicsRampStartForScore[verticalNoteIndex] = verticalNoteIndex;
							dynamicsRampDirForScore[verticalNoteIndex] = 1;
							dynamicsRampIndexes[dynamicsRampCounter].first = verticalNoteIndex;
							dynamicsRampCounter++;
						}
					}
					else if (tokens[i][index2+1] == char(62)) { // >
						if (!dynamicsRampStarted) {
							dynamicsRampStarted = true;
							dynamicsRampStart[verticalNoteIndex] = verticalNoteIndex;
							dynamicsRampStartForScore[verticalNoteIndex] = verticalNoteIndex;
							dynamicsRampDirForScore[verticalNoteIndex] = 0;
							dynamicsRampIndexes[dynamicsRampCounter].first = verticalNoteIndex;
							dynamicsRampCounter++;
						}
					}
					else if (tokens[i][index2+1] == char(33)) { // exclamation mark
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							dynamicsRampEnd[verticalNoteIndex] = verticalNoteIndex;
							dynamicsRampEndForScore[verticalNoteIndex] = verticalNoteIndex;
							dynamicsRampIndexes[dynamicsRampCounter-1].second = verticalNoteIndex;
						}
						else {
							return std::make_pair(3, "no crescendo or diminuendo initiated");
						}
					}
					else if (tokens[i][index2+1] == char(103)) { // g for gliss
						if ((tokens[i].substr(index2+1,5).compare("gliss") == 0) ||
							(tokens[i].substr(index2+1,9).compare("glissando") == 0)) {
							glissandiIndexes[verticalNoteIndex] = 1;
							foundGlissandoLocal = true;
						}
					}
					else {
						if (foundDynamicLocal) {
							// sharedData.dynamics are stored like this:
							// ppp = -9, pp = -6, p = -3, mp = -1, mf = 1, f = 3, ff = 6, fff = 9
							// so to map this range to 60-100 (dB) we must first add 9 (subtract -9)
							// multiply by 40 (100 - 60) / (9 - (-9)) = 40 / 18
							// and add 60 (the lowest value)
							if (dynamic < -9) dynamic = -9;
							else if (dynamic > 9) dynamic = 9;
							int scaledDynamic = (int)((float)(dynamic + 9) * ((100.0-(float)MINDB) / 18.0)) + MINDB;
							dynamicsData[verticalNoteIndex] = scaledDynamic;
							// convert the range MINDB to 100 to indexes from 0 to 7
							// subtract the minimum dB value from the dynamic, multipled by (7/dB-range)
							// add 0.5 and get the int to round the float properly
							int dynamicForScore = (int)((((float)dynamicsData[i]-(float)MINDB)*(7.0/(100.0-(float)MINDB)))+0.5);
							if (dynamicForScore != prevScoreDynamic) {
								dynamicsForScore[verticalNoteIndex] = dynamicForScore;
								prevScoreDynamic = dynamicForScore;
							}
							else {
								// if it's the same dynamic, we must invalidate this index
								dynamicsForScoreIndexes[verticalNoteIndex] = -1;
							}
							numDynamics++;
							// to get the dynamics in MIDI velocity values, we map ppp to 16, and fff to 127
							// based on this website https://www.hedsound.com/p/midi-velocity-db-dynamics-db-and.html
							// first find the index of the dynamic value in the array {-9, -6, -3, -1, 1, 3, 6, 9}
							int midiVelIndex = std::distance(dynsArr, std::find(dynsArr, dynsArr+8, dynamic));
							midiVels[verticalNoteIndex] = (midiVelIndex+1)*16;
							if (midiVels[verticalNoteIndex] > 127) midiVels[verticalNoteIndex] = 127;
						}
						// if we haven't found a dynamic and the ramp vectors are empty
						// it means that we have received some unknown character
						else if (dynamicsRampCounter == 0 && !foundGlissandoLocal) {
							return std::make_pair(3, tokens[i][index2+1] + (string)": unknown character");
						}
						break;
					}
					index2++;
				}
				j = index2;
			}

			else if (tokens[i][j] == char(45)) { // - for articulation symbols
				if (j > (tokens[i].size()-1)) {
					return std::make_pair(3, "a hyphen must be followed by an articulation symbol");
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
						articulationIndexes[verticalNoteIndex] = 1;
						midiArticulationVals[verticalNoteIndex] = 10;
						break;
					case 43: // + for trill
						articulationIndexes[verticalNoteIndex] = 2;
						midiArticulationVals[verticalNoteIndex] = 11;
						break;
					case 45: // - for tenuto
						articulationIndexes[verticalNoteIndex] = 3;
						midiArticulationVals[verticalNoteIndex] = 12;
						break;
					case 33: // ! for staccatissimo
						articulationIndexes[verticalNoteIndex] = 4;
						midiArticulationVals[verticalNoteIndex] = 13;
						break;
					case 62: // > for accent
						articulationIndexes[verticalNoteIndex] = 5;
						midiArticulationVals[verticalNoteIndex] = 14;
						break;
					case 46: // . for staccato
						articulationIndexes[verticalNoteIndex] = 6;
						midiArticulationVals[verticalNoteIndex] = 15;
						foundStaccato = true; // to avoid interpreting the dot as part of the duration
						break;
					case 95: // _ for portando
						articulationIndexes[verticalNoteIndex] = 7;
						midiArticulationVals[verticalNoteIndex] = 16;
						break;
					default:
						return std::make_pair(3, "unknown articulation symbol");
				}
				j++;
				continue;
			}
			
			// . for dotted note, which is also used for staccato, so here we need a boolean as well
			// this is why we test after we have checked for articulation symbols
			else if (tokens[i][j] == char(46) && !foundStaccato) {
				dotIndexes[verticalNoteIndex] = 1;
				dotsCounter++;
			}

			else if (tokens[i][j] == char(40)) { // ( for beginning of slur
				slurStarted = true;
				slurBeginningsIndexes[verticalNoteIndex] = verticalNoteIndex;
				//slurIndexes[verticalNoteIndex].first = verticalNoteIndex;
				slursCounter++;
			}

			else if (tokens[i][j] == char(41)) { // ) for end of slur
				slurStarted = false;
				slurEndingsIndexes[verticalNoteIndex] = verticalNoteIndex;
				//slurIndexes[verticalNoteIndex].second = verticalNoteIndex;
			}

			// check for _ or ^ and make sure that the previous character is not -
			else if ((int(tokens[i][j]) == 94 || int(tokens[i][j]) == 95) && int(tokens[i][j-1]) != 45) {
				if (textIndexesLocal[verticalNoteIndex] == 0) {
					return std::make_pair(3, (string)"stray " + tokens[i][j]);
				}
				if (tokens[i].size() < j+3) {
					return std::make_pair(3, "incorrect text notation");
				}
				if (int(tokens[i][j+1]) != 34) {
					return std::make_pair(3, "a text symbol must be followed by quote signs");
				}
				else {
					quotesCounter++;
				}
			}
		}
		// add the extracted octave with \ottava
		octavesForScore[verticalNoteIndex][chordNotesIndexes[i]] -= ottavas[verticalNoteIndex];
		// store durations at the end of each token only if tempDur is greater than 0
		if (tempDur > 0) {
			if (std::find(std::begin(dursArr), std::end(dursArr), tempDur) == std::end(dursArr)) {
				return std::make_pair(3, to_string(tempDur) + " is not a valid duration");
			}
			dursData[verticalNoteIndex] = tempDur;
			durIndexes[verticalNoteIndex] = verticalNoteIndex;
			numDurs++;
			if (tokenInsideChord) {
				for (int k = (int)beginningOfChordIndex; k < verticalNoteIndex; k++) {
					dursData[k] = tempDur;
				}
			}
			tempDur = 0;
		}
		if (endingOfChords[i]) {
			tokenInsideChord = false;
		}
		// once done parsing everything, check if we need to change an accidental by placing the natural sign
		// in case the same note has already been used in this bar with an accidental and now without
		int thisNote = notesForScore[verticalNoteIndex][chordNotesIndexes[i]];
		int thisOctave = octavesForScore[verticalNoteIndex][chordNotesIndexes[i]];
		// do a reverse loop to see if the last same note had its accidental corrected
		bool foundSameNote = false;
		// use ints instead of j (we are inside a loop using i) because j is unsigned
		// and a backwards loop won't work as it will wrap around instead of going below 0
		if (accidentalsForScore[verticalNoteIndex][chordNotesIndexes[i]] == -1) {
			for (int k = (int)verticalNoteIndex-1; k >= 0; k--) {
				for (int l = (int)notesForScore[k].size()-1; l >= 0; l--) {
					if (notesForScore[k][l] == thisNote && ((correctOnSameOctaveOnly && octavesForScore[k][l] == thisOctave) || !correctOnSameOctaveOnly)) {
						if (accidentalsForScore[k][l] > -1 && accidentalsForScore[k][l] != 4) {
							accidentalsForScore[verticalNoteIndex][chordNotesIndexes[i]] = 4;
							foundSameNote = true;
						}
						else {
							accidentalsForScore[verticalNoteIndex][chordNotesIndexes[i]] = -1;
							foundSameNote = true;
						}
						break;
					}
				}
				if (foundSameNote) break;
			}
			if (!foundSameNote) accidentalsForScore[verticalNoteIndex][chordNotesIndexes[i]] = -1;
		}
		if (slurStarted) {
			isSlurred[verticalNoteIndex] = true;
		}
	}

	// check if there are any unmatched quote signs
	if (quotesCounter > 0) {
		return std::make_pair(3, "unmatched quote sign");
	}

	// once all elements have been parsed we can determine whether we're fine to go on
	for (i = 0; i < tokens.size(); i++) {
		if (tokens[i].compare("\\ottava") == 0 || tokens[i].compare("\\ott") == 0) {
			i += 1;
			continue;
		}
		if (!foundNotes[i] && !foundDynamics[i] && !foundAccidentals[i] && !foundOctaves[i]) {
			return std::make_pair(3, tokens[i][j] + (string)": unknown character");
		}
	}

	if (numDurs == 0) {
		return std::make_pair(3, "no durations added");
	}

	// fill in possible empty slots of durations
	for (i = 0; i < numNotesVertical; i++) {
		if ((int)i != durIndexes[i]) {
			dursData[i] = dursData[i-1];
		}
	}

	// fill in possible empty slots of dynamics
	if (numDynamics == 0) {
		for (i = 0; i < numNotesVertical; i++) {
			// if this is not the very first bar get the last value of the previous bar
			if (sharedData.loopData.size() > 0) {
				int prevBar = getPrevBarIndex();
				// get the last stored dynamic
				dynamicsData[i] = sharedData.instruments[lastInstrumentIndex].dynamics[prevBar].back();
				// do the same for the MIDI velocities
				midiVels[i] = sharedData.instruments[lastInstrumentIndex].midiVels[prevBar].back();
			}
			else {
				// fill in a default value of halfway from min to max dB
				dynamicsData[i] = 100-((100-MINDB)/2);
				midiVels[i] = 72; // half way between mp and mf in MIDI velocity
			}
		}
	}
	else {
		for (i = 0; i < numNotesVertical; i++) {
			// if a dynamic has not been stored, fill in the last
			// stored dynamic in this empty slot
			if ((int)i != dynamicsIndexes[i]) {
				// we don't want to add data for the score, only for the sequencer
				if (i > 0) {
					dynamicsData[i] =  dynamicsData[i-1];
					midiVels[i] = midiVels[i-1];
				}
				else {
					// fill in a default value of halfway from min to max dB
					dynamicsData[i] = 100-((100-MINDB)/2);
					midiVels[i] = 72; // half way between mp and mf in MIDI velocity
				}
			}
		}
	}
	// correct dynamics in case of crescendi or decrescendi
	for (i = 0; i < dynamicsRampCounter; i++) {
		int numSteps = dynamicsRampIndexes[i].second - dynamicsRampIndexes[i].first;
		int valsDiff = dynamicsData[dynamicsRampIndexes[i].second] - dynamicsData[dynamicsRampIndexes[i].first];
		int midiVelsDiff = midiVels[dynamicsRampIndexes[i].second] - midiVels[dynamicsRampIndexes[i].first];
		int step = valsDiff / numSteps;
		int midiVelsStep = midiVelsDiff / numSteps;
		index2 = 1;
		for (j = dynamicsRampIndexes[i].first+1; j < dynamicsRampIndexes[i].second; j++) {
			dynamicsData[j] += (step * index2);
			midiVels[j] += (midiVelsStep * index2);
			index2++;
		}
	}
	// store the next dynamic in case of the dynamics ramps
	// replace all values within a ramp with the dynamic value of the next index
	for (i = 0; i < dynamicsRampCounter; i++) {
		for (j = dynamicsRampIndexes[i].first; j < dynamicsRampIndexes[i].second; j++) {
			// if a ramp ends at the end of the bar without setting a new dynamic
			// go back to the pre-last dynamic
			if (j == notesData.size()-1) {
				// if there's only one ramp in the bar store the first dynamic
				if (i == 0) {
					dynamicsRampData[j] = dynamicsData[0];
				}
				else {
					dynamicsRampData[j] = dynamicsData[dynamicsRampIndexes[i-1].first];
				}
			}
			// otherwise store the next dynamic so that the receiving program
			// ramps from its current dynamic to the next one in the duration
			// of the current note (retrieved by the sharedData.durs vector)
			else {
				dynamicsRampData[j] = dynamicsData[j+1];
			}
		}
	}
	// get the tempo of the minimum duration
	int barIndex = getLastLoopIndex();
	if (sharedData.numerator.find(barIndex) == sharedData.numerator.end()) {
		sharedData.numerator[barIndex] = sharedData.denominator[barIndex] = 4;
	}
	sharedData.newTempo = (float)sharedData.tempoMs / ((float)MINDUR / (float)sharedData.denominator[barIndex]);
	sharedData.tempNumBeats = sharedData.numerator[barIndex] * (MINDUR / sharedData.denominator[barIndex]);
	int dursAccum = 0;
	int diff = 0; // this is the difference between the expected and the actual sum of tuplets
	int scoreDur; // durations for score should not be affected by tuplet calculations
	// convert durations to number of beats with respect to minimum duration
	for (i = 0; i < numNotesVertical; i++) {
		dursData[i] = MINDUR / dursData[i];
		if (dotIndexes[i] == 1) {
			dursData[i] += (dursData[i] / 2);
		}
		scoreDur = dursData[i];
		for (auto it = tupStartStop.begin(); it != tupStartStop.end(); ++it) {
			if (i >= it->second.first && i <= it->second.second) {
				// check if truncated sum is less than the expected sum
				if (i == it->second.first) {
					int tupDurAccum = 0;
					int expectedSum = 0;
					for (j = it->second.first; j <= it->second.second; j++) {
						tupDurAccum += (dursData[i] * tupRatios[it->first].second / tupRatios[it->first].first);
						expectedSum += dursData[i];
					}
					expectedSum = expectedSum * tupRatios[it->first].second / tupRatios[it->first].first;
					diff = expectedSum - tupDurAccum;
				}
				dursData[i] = dursData[i] * tupRatios[it->first].second / tupRatios[it->first].first;
				if (diff > 0) {
					dursData[i]++;
					diff--;
				}
			}
			break;
		}
		dursAccum += dursData[i];
		dursForScore[i] = scoreDur;
		dursDataWithoutSlurs[i] = dursData[i];
	}
	if (dursAccum < sharedData.tempNumBeats) {
		return std::make_pair(3, "durations sum is less than bar duration");
	}
	if (dursAccum > sharedData.tempNumBeats) {
		return std::make_pair(3, "durations sum is greater than bar duration");
	}
	// now we can create a vector of pairs with the indexes of the slurs
	int numSlurStarts = 0, numSlurStops = 0;
	for (i = 0; i < numNotesVertical; i++) {
		if (slurBeginningsIndexes[i] > -1) numSlurStarts++;
		if (slurEndingsIndexes[i] > -1) numSlurStops++;
	}
	vector<std::pair<int, int>> slurIndexes (max(max(numSlurStarts, numSlurStops), 1), std::make_pair(-1, -1));
	int slurNdx = 0;
	for (i = 0; i < numNotesVertical; i++) {
		if (slurBeginningsIndexes[i] > -1) slurIndexes[slurNdx].first = slurBeginningsIndexes[i];
		if (slurEndingsIndexes[i] > -1) slurIndexes[slurNdx].second = slurEndingsIndexes[i];
	}
	// once the melodic line has been parsed, we can insert the new data to the maps
	map<string, int>::iterator it = sharedData.instrumentIndexes.find(lastInstrument);
	if (it == sharedData.instrumentIndexes.end()) {
		return std::make_pair(3, "instrument does not exist");
	}
	int thisInstIndex = it->second;
	sharedData.instruments[thisInstIndex].notes[barIndex] = notesData;
	sharedData.instruments[thisInstIndex].midiNotes[barIndex] = midiNotesData;
	sharedData.instruments[thisInstIndex].durs[barIndex] = dursData;
	sharedData.instruments[thisInstIndex].dursWithoutSlurs[barIndex] = dursDataWithoutSlurs;
	sharedData.instruments[thisInstIndex].midiDursWithoutSlurs[barIndex] = midiDursDataWithoutSlurs;
	sharedData.instruments[thisInstIndex].dynamics[barIndex] = dynamicsData;
	sharedData.instruments[thisInstIndex].midiVels[barIndex] = midiVels;
	sharedData.instruments[thisInstIndex].pitchBendVals[barIndex] = pitchBendVals;
	sharedData.instruments[thisInstIndex].dynamicsRamps[barIndex] = dynamicsRampData;
	sharedData.instruments[thisInstIndex].glissandi[barIndex] = glissandiIndexes;
	sharedData.instruments[thisInstIndex].midiGlissDurs[barIndex] = midiGlissDurs;
	sharedData.instruments[thisInstIndex].midiDynamicsRampDurs[barIndex] = midiDynamicsRampDurs;
	sharedData.instruments[thisInstIndex].articulations[barIndex] = articulationIndexes;
	sharedData.instruments[thisInstIndex].midiArticulationVals[barIndex] = midiArticulationVals;
	sharedData.instruments[thisInstIndex].isSlurred[barIndex] = isSlurred;
	sharedData.instruments[thisInstIndex].text[barIndex] = texts;
	sharedData.instruments[thisInstIndex].textIndexes[barIndex] = textIndexesLocal;
	//sharedData.instruments[thisInstIndex].slurBeginnings[barIndex] = slurBeginningsIndexes;
	//sharedData.instruments[thisInstIndex].slurEndings[barIndex] = slurEndingsIndexes;
	sharedData.instruments[thisInstIndex].slurIndexes[barIndex] = slurIndexes;
	sharedData.instruments[thisInstIndex].scoreNotes[barIndex] = notesForScore;
	sharedData.instruments[thisInstIndex].scoreDurs[barIndex] = dursForScore;
	sharedData.instruments[thisInstIndex].scoreDotIndexes[barIndex] = dotIndexes;
	sharedData.instruments[thisInstIndex].scoreAccidentals[barIndex] = accidentalsForScore;
	sharedData.instruments[thisInstIndex].scoreNaturalSignsNotWritten[barIndex] = naturalSignsNotWrittenForScore;
	sharedData.instruments[thisInstIndex].scoreOctaves[barIndex] = octavesForScore;
	sharedData.instruments[thisInstIndex].scoreOttavas[barIndex] = ottavas;
	sharedData.instruments[thisInstIndex].scoreGlissandi[barIndex] = glissandiIndexes;
	sharedData.instruments[thisInstIndex].scoreArticulations[barIndex] = articulationIndexes;
	sharedData.instruments[thisInstIndex].scoreDynamics[barIndex] = dynamicsForScore;
	sharedData.instruments[thisInstIndex].scoreDynamicsIndexes[barIndex] = dynamicsForScoreIndexes;
	sharedData.instruments[thisInstIndex].scoreDynamicsRampStart[barIndex] = dynamicsRampStartForScore;
	sharedData.instruments[thisInstIndex].scoreDynamicsRampEnd[barIndex] = dynamicsRampEndForScore;
	sharedData.instruments[thisInstIndex].scoreDynamicsRampDir[barIndex] = dynamicsRampDirForScore;
	//sharedData.instruments[thisInstIndex].scoreSlurBeginnings[barIndex] = slurBeginningsIndexes;
	//sharedData.instruments[thisInstIndex].scoreSlurEndings[barIndex] = slurEndingsIndexes;
	sharedData.instruments[thisInstIndex].scoreTupRatios[barIndex] = tupRatios;
	sharedData.instruments[thisInstIndex].scoreTupStartStop[barIndex] = tupStartStop;
	sharedData.instruments[thisInstIndex].scoreTexts[barIndex] = textsForScore;
	sharedData.instruments[thisInstIndex].isWholeBarSlurred[barIndex] = false;
	// in case we have no slurs, we must check if there is a slurred that is not closed in the previous bar
	// of if the whole previous bar is slurred
	if (slurIndexes.size() == 1 && slurIndexes[0].first == -1 && slurIndexes[0].second == -1) {
		int prevBar = getPrevBarIndex();
		if (sharedData.instruments[thisInstIndex].slurIndexes[prevBar].back().second == -1 ||
				sharedData.instruments[thisInstIndex].isWholeBarSlurred[prevBar]) {
			sharedData.instruments[thisInstIndex].isWholeBarSlurred[barIndex] = true;
		}
	}
	sharedData.instruments[thisInstIndex].passed = true;
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
	if (maxNumLines % 2) maxNumLines--;
	// depending on the orientation, we must first create either the height or the width of the editors
	if (paneSplitOrientation == 0) {
		paneHeight = ((float)maxNumLines * cursorHeight) / (float)numPanes.size();
	}
	else {
		paneWidth = (float)sharedData.screenWidth / (float)numPanes.size();
	}
	int paneNdx = 0;
	for (map<int, int>::iterator it = numPanes.begin(); it != numPanes.end(); ++it) {
		// each row sets the number of lines for its panes according to the number of columns (it->second)
		int numLines = maxNumLines / it->second;
		int additionalLines = 0;
		if (numLines * it->second != maxNumLines) additionalLines = maxNumLines - (numLines * it->second);
		if (paneSplitOrientation == 0) {
			paneWidth = sharedData.screenWidth / it->second;
			for (int col = 0; col < it->second; col++) {
				editors[paneNdx].setFrameWidth(paneWidth); // - (float)(lineWidth / 2));
				editors[paneNdx].setFrameXOffset(paneWidth * (float)col);
				editors[paneNdx].setFrameHeight(paneHeight);
				editors[paneNdx].setFrameYOffset(paneHeight * (float)(it->first));
				if (!col) editors[paneNdx].setMaxNumLines(numLines+additionalLines);
				else editors[paneNdx].setMaxNumLines(numLines);
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
void ofApp::changeFontSizeForAllPanes()
{
	//for (unsigned i = 0; i < editors.size(); i++) {
	//	editors[i].setFontSize(fontSize);
	//}
	// setFontSize has to be called after the font size has been set for all editors
	// because the function below calls setPaneCoords which in turn calls setMaxNunLines
	// for each editor and it editors[i].setFontSize() hasn't been called
	// a division by zero will occur
	setFontSize();
}

//--------------------------------------------------------------
std::pair<int, string> ofApp::scoreCommands(vector<string>& commands, int lineNum, int numLines)
{
	for (unsigned i = 1; i < commands.size(); i++) {
		if (commands[i].compare("animate") == 0) {
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
		else if (commands[i].compare("inanimate") == 0) {
			for (map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				it->second.setAnimation(false);
			}
			sharedData.animate = false;
			sharedData.setAnimation = false;
		}
		else if (commands[i].compare("showbeat") == 0) {
			if (sequencer.isThreadRunning()) {
				sharedData.beatAnimate = true;
			}
			else {
				sharedData.setBeatAnimation = true;
			}
		}
		else if (commands[i].compare("hidebeat") == 0) {
			sharedData.beatAnimate = false;
			sharedData.setBeatAnimation = false;
		}
		else if (commands[i].compare("beattype") == 0) {
			if (commands.size() < i + 2) {
				return std::make_pair(3, "beattype takes a 1 or a 2 as an argument");
			}
			if (!isNumber(commands[i+1])) {
				return std::make_pair(3, "beattype takes a 1 or a 2 as an argument");
			}
			int beattype;
			unsigned iIncrement = 1;
			if (!isNumber(commands[i+1])) {
				string restOfCommands = "";
				for (unsigned j = i+1; j < commands.size(); j++) {
					if (j > i+1) restOfCommands += " ";
					restOfCommands += commands[j];
				}
				std::pair<int, string> p = parseCommand(restOfCommands, lineNum, numLines);
				if (p.first > 0) return p;
				if (!isNumber(p.second)) return std::make_pair(3, "beattype takes a number as an argument");
				beattype = stoi(p.second);
				iIncrement = commands.size();
			}
			else {
				beattype = stoi(commands[i+1]);
			}
			if (beattype < 1 || beattype > 2) {
				return std::make_pair(3, "unknown beat vizualization type");
			}
			sharedData.beatVizType = beattype;
			i += iIncrement; // move i so we check the command after the argument
		}
		else if (commands[i].compare("movex") == 0) {
			if (commands.size() < i + 2) {
				return std::make_pair(3, "movex takes a number as an argument");
			}
			int numPixels;
			unsigned iIncrement = 1;
			if (!isNumber(commands[i+1])) {
				string restOfCommands = "";
				for (unsigned j = i+1; j < commands.size(); j++) {
					if (j > i+1) restOfCommands += " ";
					restOfCommands += commands[j];
				}
				std::pair<int, string> p = parseCommand(restOfCommands, lineNum, numLines);
				if (p.first > 0) return p;
				if (!isNumber(p.second)) return std::make_pair(3, "movex takes a number as an argument");
				numPixels = stoi(p.second);
				iIncrement = commands.size();
			}
			else {
				numPixels = stoi(commands[i+1]);
			}
			for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				it->second.moveScoreX(numPixels);
			}
			i += iIncrement; // move i so we check the command after the argument
		}
		else if (commands[i].compare("movey") == 0) {
			if (commands.size() < i + 2) {
				return std::make_pair(3, "movey takes a number as an argument");
			}
			int numPixels;
			unsigned iIncrement = 1;
			if (!isNumber(commands[i+1])) {
				string restOfCommands = "";
				for (unsigned j = i+1; j < commands.size(); j++) {
					if (j > i+1) restOfCommands += " ";
					restOfCommands += commands[j];
				}
				std::pair<int, string> p = parseCommand(restOfCommands, lineNum, numLines);
				if (p.first > 0) return p;
				if (!isNumber(p.second)) return std::make_pair(3, "movey takes a number as an argument");
				numPixels = stoi(p.second);
				iIncrement = commands.size();
			}
			else {
				numPixels = stoi(commands[i+1]);
			}
			for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				it->second.moveScoreY(numPixels);
			}
			i += iIncrement; // move i so we check the command after the argument
		}
		else if (commands[i].compare("recenter") == 0) {
			for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				it->second.recenterScore();
			}
			return std::make_pair(0, "");
		}
		else if (commands[i].compare("numbars") == 0) {
			if (commands.size() < i+2) {
				return std::make_pair(3, "\"numbars\" takes a number as an argument");
			}
			if (!isNumber(commands[i+1])) {
				return std::make_pair(3, "\"numbars\" takes a number as an argument");
			}
			int numBarsLocal = stoi(commands[i+1]);
			if (numBarsLocal < 1) {
				return std::make_pair(3, "number of bars to display must be 1 or greater");
			}
			numBarsToDisplay = numBarsLocal;
			for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				it->second.setNumBarsToDisplay(numBarsToDisplay);
			}
			i += 2;
		}
		else if (commands[i].compare("update") == 0) {
			if (commands.size() < i+2) {
				return std::make_pair(3, "\"update\" takes one argument, onlast or immediately");
			}
			if (commands[i+1].compare("onlast") == 0) {
				scoreChangeOnLastBar = true;
			}
			else if (commands[i+1].compare("immediately") == 0) {
				scoreChangeOnLastBar = false;
			}
			else {
				return std::make_pair(3, commands[i+1] + (string)"unknown argument to \"update\", must be onlast or immediately");
			}
			i += 2;
		}
		else if (commands[i].compare("correct") == 0) {
			if (commands.size() < i+2) {
				return std::make_pair(3, "\"correct\" takes one argument, onoctave or all");
			}
			if (commands[i+1].compare("onoctave") == 0) {
				correctOnSameOctaveOnly = true;
			}
			else if (commands[i+1].compare("all") == 0) {
				correctOnSameOctaveOnly = false;
			}
			else {
				return std::make_pair(3, commands[i+1] + (string)": unknown argument to \"correct\"");
			}
			i += 2;
		}
		else {
			string restOfCommands = "";
			for (unsigned j = i; j < commands.size(); j++) {
				if (j > i) restOfCommands += " ";
				restOfCommands += commands[j];
			}
			return parseCommand(restOfCommands, lineNum, numLines);
		}
	}
	return std::make_pair(0, "");
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
	sharedData.instruments[maxInstIndex].setStaffSize(sharedData.scoreFontSize);
	sharedData.instruments[maxInstIndex].setNotesFontSize(sharedData.scoreFontSize);
	sharedData.numInstruments++;
	setScoreCoords();
}

//--------------------------------------------------------------
void ofApp::updateTempoBeatsInsts(int barIndex)
{
	sharedData.tempo = sharedData.newTempo;
	sharedData.numBeats[barIndex] = sharedData.tempNumBeats;
}

//--------------------------------------------------------------
void ofApp::setScoreCoords()
{
	scoreXStartPnt = sharedData.longestInstNameWidth + (sharedData.blankSpace * 1.5);
	float staffXLength = (sharedData.screenWidth / 2) - sharedData.blankSpace - sharedData.staffLinesDist - scoreXStartPnt;
	// place yPos at the center of the screen in the Y axis
	float yPos1 = ((sharedData.tracebackBase - cursorHeight) / 2) - (sharedData.staffLinesDist * 2);
	float yPos2 = ((sharedData.tracebackBase - cursorHeight) / 4) - (sharedData.staffLinesDist * 2);
	// and give an offset according to the number of sharedData.staffs
	// so they are all centered
	if (sharedData.instruments.size() > 1) {
		// we distance each staff so that another staff could fit in between them
		// hence staffLinesDist * 10, which yields ten times the distance between staff lines
		yPos1 -= ((sharedData.instruments.size() / 2) * (sharedData.staffLinesDist * 10));
		yPos2 -= ((sharedData.instruments.size() / 2) * (sharedData.staffLinesDist * 10));
		// if the number of sharedData.staffs is even, we must subtract half the distance
		// between each staff
		if ((sharedData.instruments.size() % 2) == 0) {
			yPos1 += (sharedData.staffLinesDist * 5);
			yPos2 += (sharedData.staffLinesDist * 5);
		}
	}
	sharedData.notesXStartPnt = 0;
	int i = 0;
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		float yAnchor1 = yPos1 + (i * (sharedData.staffLinesDist * 10));
		float yAnchor2 = yPos2 + (i * (sharedData.staffLinesDist * 10));
		it->second.setStaffCoords(staffXLength, yAnchor1, yAnchor2, sharedData.staffLinesDist);
		//float xStartPnt = it->second.getStaffXPos(); // this yields the X coord after the meter symbols
		// get the maximum start point so all scores are alligned on the X axis
		//if (xStartPnt > sharedData.notesXStartPnt) sharedData.notesXStartPnt = xStartPnt;
		// give the appropriate offset so that notes are not too close to the loop symbols
		//float xNotesStart = sharedData.notesXStartPnt + sharedData.staffLinesDist;
		//float notesXLength = (sharedData.screenWidth / 2) - sharedData.blankSpace - (sharedData.staffLinesDist * 2);
		notesXLength = staffXLength - it->second.getClefXOffset() - it->second.getMeterXOffset() - (sharedData.staffLinesDist * 2);
		it->second.setNoteCoords(notesXLength, yAnchor1, yAnchor2, sharedData.staffLinesDist, sharedData.scoreFontSize);
		i++;
	}
	// get the note width from the first instrument, as they all have the same size
	sharedData.noteWidth = sharedData.instruments[0].getNoteWidth();
	sharedData.noteHeight = sharedData.instruments[0].getNoteHeight();
}

//--------------------------------------------------------------
void ofApp::setScoreNotes(int barIndex)
{
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		it->second.setScoreNotes(barIndex, sharedData.numerator[barIndex],
								 sharedData.denominator[barIndex], sharedData.numBeats[barIndex]);
	}
	calculateStaffPositions(barIndex);
}

//--------------------------------------------------------------
void ofApp::calculateStaffPositions(int bar)
{
	// no need to calculate any coordinates for one instrument only
	if (sharedData.instruments.size() == 1) return;
	float maxHeightSum = 0;
	float minVsAnchor = 0;
	float lastMaxPosition = 0;
	float firstMinPosition = 0;
	int i = 0;
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		float maxPosition = it->second.getMaxYPos(bar);
		float minPosition = it->second.getMinYPos(bar);
		float minVsAnchorLocal = it->second.getMinVsAnchor(bar);
		if (i == 0 && it->first == 0) {
			firstMinPosition = minPosition;
		}
		float diff = maxPosition - minPosition;
		if (diff > maxHeightSum) {
			maxHeightSum = diff;
			minVsAnchor = minVsAnchorLocal;
		}
		lastMaxPosition = maxPosition;
		i++;
	}
	// if the current bar is wider than the existing ones, we need to change the current Y coordinates
	if (maxHeightSum >= sharedData.allStaffDiffs) {
		sharedData.allStaffDiffs = maxHeightSum;
		float yAnchor1, yAnchor2;
		float totalDiff = lastMaxPosition - firstMinPosition;
		// if all staffs fit in the screen
		if (totalDiff < (sharedData.tracebackBase - cursorHeight)) {
			yAnchor1 = ((sharedData.tracebackBase - cursorHeight) / 2);
			yAnchor1 -= (((sharedData.allStaffDiffs + (sharedData.staffLinesDist * 2)) * \
						(sharedData.numInstruments / 2)));
			yAnchor1 -= (sharedData.staffLinesDist * 2);
			if ((sharedData.instruments.size() % 2) == 0) {
				yAnchor1 +=((sharedData.allStaffDiffs + (sharedData.staffLinesDist * 2)) / 2); 
			}
			// make sure anything that sticks out of the staff doesn't go below 0 in the Y axis
			if ((yAnchor1 - minVsAnchor) < 0) {
				yAnchor1 += minVsAnchor;
			}
		}
		else {
			// if the full score exceeds the screen just place it 10 pixels below the top part of the window
			yAnchor1 = minVsAnchor + 10;
		}
		// do the same for the horizontal orientation
		if (totalDiff < ((sharedData.tracebackBase - cursorHeight) / 2)) {
			yAnchor2 = ((sharedData.tracebackBase - cursorHeight) / 4);
			yAnchor2 -= (((sharedData.allStaffDiffs + (sharedData.staffLinesDist * 2)) * \
						(sharedData.numInstruments / 2)));
			yAnchor2 -= (sharedData.staffLinesDist * 2);
			if ((sharedData.instruments.size() % 2) == 0) {
				yAnchor2 +=((sharedData.allStaffDiffs + (sharedData.staffLinesDist * 2)) / 2); 
			}
			// make sure anything that sticks out of the staff doesn't go below 0 in the Y axis
			if ((yAnchor2 - minVsAnchor) < 0) {
				yAnchor2 += minVsAnchor;
			}
		}
		else {
			// if the full score exceeds the screen just place it 10 pixels below the top part of the window
			yAnchor2 = minVsAnchor + 10;
		}
		for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			if (it->first > 0) {
				yAnchor1 += (sharedData.allStaffDiffs + (sharedData.staffLinesDist * 2));
				yAnchor2 += (sharedData.allStaffDiffs + (sharedData.staffLinesDist * 2));
			}
			it->second.correctScoreYAnchor(yAnchor1, yAnchor2);
		}
	}
}

//--------------------------------------------------------------
void ofApp::swapScorePosition(int orientation)
{
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		it->second.setScoreOrientation(orientation);
	}
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
			// and since the number of total lines is oddeven, we make this background one less
			// e.g. for 37 lines, the score background will be 18 lines, not 19, or 18.5
			// but when viewed on the lower side, it should touch the white line of the editor
			// hence the scoreYOffset is not the same as the scoreBackgroundHeith
			// as is the case with scoreXOffset and scoreBarckgroundWidth, in the case above
			if (scoreYOffset == 0) scoreYOffset = (sharedData.tracebackBase / 2);
			else scoreYOffset = 0;
			break;
		default:
			break;
	}
}

//--------------------------------------------------------------
void ofApp::setScoreSizes()
{
	// for a 35 font size the staff lines distance is 10
	sharedData.staffLinesDist = 10 * ((float)sharedData.scoreFontSize / 35.0);
	sharedData.longestInstNameWidth = 0;
	for (map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		it->second.setStaffSize(sharedData.scoreFontSize);
		it->second.setNotesFontSize(sharedData.scoreFontSize);
		if (instFont.stringWidth(it->second.getName()) > sharedData.longestInstNameWidth) {
			sharedData.longestInstNameWidth = instFont.stringWidth(it->second.getName());
		}
	}
	scoreXStartPnt = sharedData.longestInstNameWidth + (sharedData.blankSpace * 1.5);
	if (sharedData.barsIndexes.size() > 0) {
		for (map<string, int>::iterator it = sharedData.barsIndexes.begin(); it != sharedData.barsIndexes.end(); ++it) {
			setScoreCoords();
			for (map<int, Instrument>::iterator it2 = sharedData.instruments.begin(); it2 != sharedData.instruments.end(); ++it2) {
				it2->second.setNotePositions(it->second);
			}
		}
	}
	else {
		setScoreCoords();
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
	sendToParts(m);
	m.clear();
	if (midiPortOpen) sequencer.midiOut.closePort();
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
	sharedData.mutex.lock();
	setPaneCoords();
	setScoreSizes();
	sharedData.mutex.unlock();
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
	sharedData.tracebackBase = sharedData.screenHeight - cursorHeight;
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
	setPaneCoords();
	setScoreCoords();
	for (auto it = sharedData.barsIndexes.begin(); it != sharedData.barsIndexes.end(); ++it) {
		for (auto it2 = sharedData.instruments.begin(); it2 != sharedData.instruments.end(); ++it2) {
			// first we need to set the meter, because that's where the distance between beats is set
			// for the Notes() object
			it2->second.setMeter(it->second, sharedData.numerator[it->second],
								 sharedData.denominator[it->second], sharedData.numBeats[it->second]);
			it2->second.setNotePositions(it->second);
		}
	}
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
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg)
{

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo)
{

}
