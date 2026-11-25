#include "ofApp.h"
#include "system_command.h"
#include "clock.h"
#include "paths.h"
#include <set>
#include "pyo_classes.h" // the Python code of the Pyo synths (pyoClasses), defined in pyo_classes.cpp
#include <string>
#include <time.h> // to give random seed to srand()
#include <ctype.h> // to add isdigit()
#include <algorithm> // for replacing characters in a string
#include <cmath> // to add abs()
#include <utility> // to add pair and make_pair
#include <sstream> // to create a string from vector<string>
#include <iomanip> // std::setprecision

// the includes below are used to execute shell commands and store the output in a string
// sstream is already included above, also needed for shell output storage
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

#ifndef _WIN32
	#include <sys/wait.h>
#endif

/**************************************************************/
/********************* Main OF class **************************/
//--------------------------------------------------------------
void ofApp::setup()
{
	// setup() runs on the main thread, store its id to detect calls from other threads
	workspace.mainThreadId = std::this_thread::get_id();
	// the engine reaches the user interface through this app (see frontend.h), and its audio stream calls this app (see audio.h)
	session.frontend = this;
	session.audioCallback = this;
	// the editors reach the app through EditorHost (see editor_host.h)
	Editor::host = this;
	// relative paths (e.g. of a SoundFont) start from the user's folder of LiveLily, and \help looks in the search paths (see paths.h)
	// in test mode only in the resources of LiveLily, so that files of the user don't change the results of the tests
	std::filesystem::path resourcesPath = ofToDataPath("", true);
	session.dataDirectory = Paths::userFolder().string();
	session.searchPaths = (testFile.empty() ? Paths::searchPaths(resourcesPath) : std::vector<std::filesystem::path>{resourcesPath});
	brightnessCoeff = 0.85;
	backgroundColor = ofColor::black;
	foregroundColor = ofColor::white;
	beatColor = ofColor::paleGreen;
	scoreBackgroundColor = ofColor::white;
	shellStrColor = Colors::white;
	ofBackground(backgroundColor*brightnessCoeff);

	// give a random seed to srand(), and a fixed one in test mode, so that \rand gives the same values every time (see testmode.cpp)
	srand(testFile.empty() ? time(NULL) : 1);

	unsigned int framerate = 60;

	// in test mode, the MIDI ports are made-up ones, so that the dumps don't depend on the machine (see test_midi.cpp)
	// set before the session is set up, as the sequencer takes the MIDI output then
	if (!testFile.empty()) session.midi = makeTestMidiOutput();
	// the defaults of the session and the tables of the names of its languages (see session_setup.cpp)
	session.setup(toColor(foregroundColor));

	notesID = 0;
	fullScreen = false;
	isWindowResized = false;
	windowResizeTimeStamp = 0;

	ofSetFrameRate(framerate);
	lineWidth = 2;
	ofSetLineWidth(lineWidth);

	ofSetWindowTitle("LiveLily");

	view.screenWidth = ofGetWindowWidth();
	view.screenHeight = ofGetWindowHeight();
	view.middleOfScreenX = view.screenWidth / 2;
	view.middleOfScreenY = view.screenHeight / 2;
	view.staffXOffset = view.middleOfScreenX;
	view.staffWidth = view.screenWidth;
	numBarsToDisplay = 2;

	shiftPressed = false;
	ctrlPressed = false;
	altPressed = false;

	numShellLines = 0;
	numShellLsOutputLines = 0;
	executingBind = false;
	editorBindingKey = -1;
	editorBindingArg = -1;
	editorBindingIsDollar = false;
	gPressed = false;
	dPressed = false;

	ofTrueTypeFont::setGlobalDpi(72);
	fontSize = 18;
	workspace.setupFirstPane();
	fontLoaded = false;
	// the function below calls setFontSize() which in turn sets various values
	// including the tracebackBase variable that is used to determine the coordinates and offsets of the panes
	setFontSize(true);
	screenSplitHorizontal = view.screenWidth;

	view.numBars = view.prevNumBars = 1;
	view.thisPosition = view.prevPosition = 0;
	showBarCount = false;
	showTempo = false;

	//sharedData.showNotes = false;
	//showPianoRoll = false;
	//showScope = false;
	showScore = false;
	scoreType = 0;
	view.longestInstNameWidth = 0;
	view.staffLinesDist = 10.0;
	view.scoreFontSize = 35;
	instFontSize = view.scoreFontSize/3.5;
	instFont.load("fonts/LiberationSerif-Regular.ttf", instFontSize);
	view.notesXStartPnt = 0;
	view.blankSpace = 10;
	view.beatAnimate = false;
	view.setBeatAnimation = false;
	view.beatViz = false;
	view.beatTypeCommand = false;
	view.beatVizType = 1;
	view.beatVizDegrade = BEATVIZBRIGHTNESS;
	view.beatVizTimeStamp = 0;
	view.beatVizStepsPerMs = (float)BEATVIZBRIGHTNESS / ((float)session.sharedData.tempoMs[0] / 4.0);
	view.beatVizRampStart = session.sharedData.tempoMs[0] / 4;
	view.beatUpdated = false;
	view.noteWidth = 0;
	view.noteHeight = 0;
	view.allStaffDiffs = 0;
	view.yStartPnts[0] = view.yStartPnts[1] = view.yStartPnts[2] = 0;
	view.maxBarFirstStaffAnchor = 0;

	// various score variables
	scoreOrientation = 0;
	scoreXOffset = view.screenWidth / 2;
	scoreYOffset = 0;
	scoreXStartPnt = 0;
	scoreBackgroundWidth = view.screenWidth/2;
	scoreBackgroundHeight = workspace.belowBase; // - cursorHeight;
	scoreMoveXCommand = false;
	scoreMoveYCommand = false;
	mustUpdateScore = false;
	scoreUpdated = false;
	scoreChangeOnLastBar = false;
	// piano roll variables, with the keys of a piano, from A0 to C8 (see setPianoRollRange())
	view.pianoRollKeysWidth = scoreBackgroundWidth / KEYSWIDTHCOEFF;
	setPianoRollRange(21, 108);

	view.pyStdoutStr = "";
	view.pyStdoutStrNdx = 0;
	view.typePyStrCharByChar = true;
	view.typePyStrLineByLine = false;
	view.typePyStr = false;
	view.whichPyPane = 0;
	view.pyStdoutKeyModNdx = 0;
	view.pyStdoutStrVecNdx = 0;

	exiting = false;

	mouseX = 0;
	mouseY = 0;

	// create the beat rectangle transparency cosine table
	for (int i = 0; i <= BEATVIZBRIGHTNESS; i++) {
		view.beatVizCosTab[i] = (int)(((cos(((float)i/(float)(BEATVIZBRIGHTNESS+1))*M_PI) * -0.5) + 0.5) * 255);
	}

	// commands that the editor needs to check in case of multi-line executing
	// but starting on the first line that is not indented
	noIndentCheck[Session::livelily].push_back("\\bar");
	noIndentCheck[Session::livelily].push_back("\\bars");
	noIndentCheck[Session::livelily].push_back("\\loop");
	noIndentCheck[Session::livelily].push_back("\\function");
	noIndentCheck[Session::livelily].push_back("\\list");
	noIndentCheck[Session::python].push_back("class");
	noIndentCheck[Session::python].push_back("def");
	noIndentCheck[Session::python].push_back("for");
	noIndentCheck[Session::python].push_back("while");
	noIndentCheck[Session::python].push_back("if");

}

//--------------------------------------------------------------
void ofApp::update()
{
	// in test mode, the file is run once setup has finished and a frame has been drawn, and the program quits (see testmode.cpp)
	if (!testFile.empty()) {
		if (!testDone && ofGetFrameNum() > 1) {
			testDone = true;
			testDoneFrame = ofGetFrameNum();
			runTest();
			// the program quits after drawing a few frames, with the screen in the dump (see draw())
		}
		// the keys of the test, one event a frame, as they are typed; the screen is written a few frames after the last one
		else if (testDone && testKeyNdx < testKeys.size()) {
			const KeyEvent& event = testKeys[testKeyNdx++];
			if (event.press) keyPressed(event.key);
			else keyReleased(event.key);
			testDoneFrame = ofGetFrameNum();
		}
		return;
	}
	// apply any tracebacks set from the sequencer thread
	workspace.applyPendingTracebacks();
	// check for OSC messages
	static float prevVal = 0;
	while (session.oscReceiver->hasWaitingMessages()) {
		OscMessage m;
		session.oscReceiver->getNextMessage(m);
		std::string address = m.getAddress();
		if (address == session.maestroAddress) {
			if (!session.sequencer.isThreadRunning() && view.setBeatAnimation) {
				view.beatAnimate = true;
				view.setBeatAnimation = false;
			}
			float val = m.getArgAsFloat(session.maestroValNdx);
			if (session.receivingMaestro) {
				if (val > session.maestroValThresh) {
					if (!view.beatUpdated && Clock::millis() - session.maestroTimeStamp > MAESTROTHRESH) {
						if (session.maestroInitialized) {
							int bar = session.getPlayingBarIndex();
							session.sharedData.beatCounter++;
							if (session.sharedData.beatCounter >= session.sharedData.numerator[bar]) {
								session.sharedData.beatCounter = 0;
								session.sharedData.thisBarIndex++;
								if (session.sharedData.thisBarIndex >= session.sharedData.loopData[session.sharedData.loopIndex].size()) {
									session.sharedData.thisBarIndex = 0;
								}
								bar = session.getPlayingBarIndex();
							}
							session.sharedData.BPMTempi[bar] = session.msToBPM(Clock::millis() - session.maestroTimeStamp);
							sendBeatVizInfo(bar);
						}
						else {
							session.maestroInitialized = true;
						}
						view.beatUpdated = true;
						session.maestroTimeStamp = Clock::millis();
						//cout << sharedData.beatCounter << endl;
						//printVector(maestroVec);
						float accum = 0;
						for (auto it = session.maestroVec.begin(); it != session.maestroVec.end(); ++it) {
							accum += *it;
						}
						accum /= (float)session.maestroVec.size();
						//cout << "accum: " << accum << endl;
						session.maestroVec.clear();
					}
				}
				else {
					view.beatUpdated = false;
					if (prevVal != 0) {
						session.maestroVec.push_back(abs(val - prevVal));
					}
					prevVal = val;
				}
			}
		}
		else if (address == session.maestroToggleAddress) {
			if (m.getArgType(0) == OscArgType::Int32) {
				int val = m.getArgAsInt32(0);
				if (val) session.receivingMaestro = true;
				else session.receivingMaestro = false;
			}
			else if (m.getArgType(0) == OscArgType::Float) {
				int val = (int)m.getArgAsFloat(0);
				if (val) session.receivingMaestro = true;
				else session.receivingMaestro = false;
			}
			else if (m.getArgType(0) == OscArgType::True || m.getArgType(0) == OscArgType::False) {
				session.receivingMaestro = m.getArgAsBool(0);
			}
			if (!session.receivingMaestro) {
				session.maestroInitialized = false;
			}
			else {
				session.parseString("\\score.beatsin " + std::to_string(session.maestroBeatsIn), 1, 1); // dummy 2nd and 3rd args
			}
		}
		else if (address == session.maestroLevareAddress) {
			session.maestroBeatsIn = m.getArgAsInt32(0);
		}
		else {
			for (std::map<int, std::string>::iterator it = fromOscAddr.begin(); it != fromOscAddr.end(); ++it) {
				if (address.substr(0, it->second.size()) == it->second) {
					int whichPaneOsc = it->first;
					std::map<int, Editor>::iterator editIt = editors.find(whichPaneOsc);
					if (editIt != editors.end()) {
						if (m.getArgType(0) == OscArgType::String) {
							std::string oscStr = m.getArgAsString(0);
							for (unsigned i = 0; i < oscStr.size(); i++) {
								if (address.find("/press") != std::string::npos) {
									editIt->second.fromOscPress((int)oscStr.at(i));
								}
								else if (address.find("/release") != std::string::npos) {
									editIt->second.fromOscRelease((int)oscStr.at(i));
								}
							}
						}
						else if (m.getArgType(0) == OscArgType::Int32) {
							size_t numArgs = m.getNumArgs();
							for (size_t j = 0; j < numArgs; j++) {
								int oscVal = (int)m.getArgAsInt32(j);
								if (address.find("/press") != std::string::npos) {
									editIt->second.fromOscPress(oscVal);
								}
								else if (address.find("/release") != std::string::npos) {
									editIt->second.fromOscRelease(oscVal);
								}
							}
						}
						else if (m.getArgType(0) == OscArgType::Int64) {
							size_t numArgs = m.getNumArgs();
							for (size_t j = 0; j < numArgs; j++) {
								int oscVal = (int)m.getArgAsInt64(j);
								if (address.find("/press") != std::string::npos) {
									editIt->second.fromOscPress(oscVal);
								}
								else if (address.find("/release") != std::string::npos) {
									editIt->second.fromOscRelease(oscVal);
								}
							}
						}
						else if (m.getArgType(0) == OscArgType::Float) {
							size_t numArgs = m.getNumArgs();
							for (size_t j = 0; j < numArgs; j++) {
								int oscVal = (int)m.getArgAsFloat(j);
								if (address.find("/press") != std::string::npos) {
									editIt->second.fromOscPress(oscVal);
								}
								else if (address.find("/release") != std::string::npos) {
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

	for (auto it = session.oscServers.begin(); it != session.oscServers.end(); ++it) {
		while (it->second->hasWaitingMessages()) {
			OscMessage m;
			it->second->getNextMessage(m);
			std::string address = m.getAddress();
			if (session.oscServerFuncs.find(it->first) != session.oscServerFuncs.end()) {
				for (auto it2 = session.oscServerFuncs.at(it->first).begin(); it2 != session.oscServerFuncs.at(it->first).end(); ++it2) {
					// the first item of the pair inside the vector is the OSC address
					if (address == it2->first) {
						// the arguments of the message are the arguments of the mapped function ($1, $2, etc.)
						// numbers are converted to text here, as getArgAsString() would print a warning for every number
						std::string args;
						for (size_t argNdx = 0; argNdx < m.getNumArgs(); argNdx++) {
							if (argNdx > 0) args += " ";
							switch (m.getArgType(argNdx)) {
								case OscArgType::Int32:
									args += std::to_string(m.getArgAsInt32(argNdx));
									break;
								case OscArgType::Int64:
									args += std::to_string(m.getArgAsInt64(argNdx));
									break;
								case OscArgType::Float: {
									std::ostringstream stream;
									stream << m.getArgAsFloat(argNdx);
									args += stream.str();
									break;
								}
								case OscArgType::Double: {
									std::ostringstream stream;
									stream << m.getArgAsDouble(argNdx);
									args += stream.str();
									break;
								}
								case OscArgType::True:
									args += "1";
									break;
								case OscArgType::False:
									args += "0";
									break;
								case OscArgType::String:
									args += m.getArgAsString(argNdx);
									break;
								case OscArgType::Symbol:
									args += m.getArgAsSymbol(argNdx);
									break;
								default:
									break;
							}
						}
						// the second item of the pair inside the vector is the mapped function
						// it is called like a bound function, so that its errors go to the line where it is defined
						session.callBoundFunction(it2->second, args);
					}
				}
			}
		}
	}

	// check if any instrument has a delay for receiving OSC messages
	for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
		if (it->second.oscFifo.size() > 0) {
			uint64_t stamp = Clock::millis();
			if (stamp >= it->second.oscFifo.back().second) {
				it->second.scorePartSender->send(it->second.oscFifo.back().first);
				it->second.oscFifo.pop_back();
			}
		}
	}
	// collect the names of the functions to call while holding the lock
	// and call them after releasing it, as calling a function locks it again
	std::vector<std::string> framerateFuncs;
	{
		std::lock_guard<std::mutex> lock(session.sharedData.functionsMutex);
		for (auto func = session.sharedData.functions.begin(); func != session.sharedData.functions.end(); ++func) {
			// binding a function to the number of instruments + 2, binds to the framerate
			if (func->second.getBoundInst() == (int)session.sharedData.instruments.size() + 2) {
				framerateFuncs.push_back(func->second.getName());
			}
		}
	}
	for (const std::string& funcName : framerateFuncs) {
		//parseCommand(genCmdInput(funcName), 1, 1);
		session.callBoundFunction(funcName);
	}

	if (isWindowResized) {
		if ((ofGetElapsedTimeMillis() - windowResizeTimeStamp) > WINDOW_RESIZE_GAP) {
			isWindowResized = false;
			resizeWindow();
		}
	}
	if (session.startSequencer) {
		session.startSequencer = false;
		// in test mode the sequencer plays a dry run instead, at the end of the test (see testmode.cpp)
		if (!session.sequencer.isThreadRunning() && testFile.empty()) {
			session.sequencer.start();
		}
	}

	if (view.typePyStr && ((view.pyStdoutStrNdx == 0 && !ctrlPressed && !shiftPressed) || view.pyStdoutStrNdx > 0)) {
		if (view.typePyStrCharByChar) {
			pyStdoutStrKeyModifier keyMod = checkPyStdoutKeyModifier(view.pyStdoutStrNdx);
			view.pyStdoutStrNdx += keyMod.ndxInc;
			if (keyMod.isModifier) {
				handleKeyModifier(keyMod);
			}
			else {
				int charToType = (int)view.pyStdoutStr[view.pyStdoutStrNdx];
				if (view.pyStdoutStr[view.pyStdoutStrNdx] == '\n') {
					charToType = 13;
				}
				if (view.whichPyPane == session.whichPane) {
					keyPressed(charToType);
				}
				else {
					keyPressedOsc(charToType, view.whichPyPane);
				}
				view.pyStdoutStrNdx++;
			}
			if (view.pyStdoutStrNdx >= view.pyStdoutStr.size()) {
				view.pyStdoutStr.clear();
				view.pyStdoutStrNdx = 0;
				view.typePyStr = false;
			}
		}
		else if (view.typePyStrLineByLine) {
			// check if we have a key modifier at the very beginning
			if (view.pyStdoutStrVecNdx == 0 && view.pyStdoutKeyModNdxs.size() > 0 && view.pyStdoutStrVecNdx == view.pyStdoutKeyModNdxs[view.pyStdoutKeyModNdx]) {
				handleKeyModifier(view.pyStdoutKeyModifiers[view.pyStdoutKeyModNdx]);
				view.pyStdoutKeyModNdx++;
				// if we have a key modifier, we increment the char by char index so that the test in which typing Python strings is included is true
				view.pyStdoutStrNdx++;
			}
			editors[view.whichPyPane].setString(view.pyStdoutStrVec[view.pyStdoutStrVecNdx]);
			view.pyStdoutStrVecNdx++;
			// then check if we have a key modifier after the current string chunk
			if (view.pyStdoutKeyModNdxs.size() > 0 && view.pyStdoutStrVecNdx == view.pyStdoutKeyModNdxs[view.pyStdoutKeyModNdx]) {
				handleKeyModifier(view.pyStdoutKeyModifiers[view.pyStdoutKeyModNdx]);
				view.pyStdoutKeyModNdx++;
				// if we have a key modifier, we increment the char by char index so that the test in which typing Python strings is included is true
				view.pyStdoutStrNdx++;
			}
			if (view.pyStdoutStrVecNdx >= (int)view.pyStdoutStrVec.size()) {
				view.typePyStrLineByLine = false;
				view.pyStdoutStrVec.clear();
				view.pyStdoutKeyModifiers.clear();
				view.pyStdoutKeyModNdxs.clear();
				view.typePyStr = false;
				view.pyStdoutStrVecNdx = 0;
				view.pyStdoutKeyModNdx = 0;
				view.pyStdoutStrNdx = 0;
			}
		}
		else {
			std::vector<int> angleBracketNdxs = session.findIndexesOfCharInStr(view.pyStdoutStr, "<");
			bool hasModifiers = false;
			std::vector<pyStdoutStrKeyModifier> keyModifiers;
			// run through the indexes of the angle brackets and test each one separately for a key modifier
			for (auto it = angleBracketNdxs.begin(); it != angleBracketNdxs.end(); ++it) {
				keyModifiers.push_back(checkPyStdoutKeyModifier(*it));
				if (keyModifiers.back().isModifier) {
					hasModifiers = true;
				}
			}
			if (!hasModifiers) {
				std::vector<std::string> v = session.tokenizeString(view.pyStdoutStr, "\n", true);
				for (std::string s : v) {
					std::vector<std::string> v2 = session.tokenizeString(s, "\b", true);
					view.pyStdoutStrVec.insert(view.pyStdoutStrVec.end(), v2.begin(), v2.end());
				}
				//editors[whichPane].setString(sharedData.pyStdoutStr);
			}
			else {
				size_t lastModifierNdx = 0;
				int prevEnd = 0;
				for (size_t i = 0; i < keyModifiers.size(); i++) {
					if (keyModifiers[i].isModifier) {
						int bracketStart = angleBracketNdxs[i];
						int bracketEnd = bracketStart + keyModifiers[i].ndxInc;
						if (bracketStart > prevEnd) {
							std::vector<std::string> v = session.tokenizeString(view.pyStdoutStr.substr(prevEnd, bracketStart-prevEnd), "\n", true);
							for (std::string s : v) {
								std::vector<std::string> v2 = session.tokenizeString(s, "\b", true);
								view.pyStdoutStrVec.insert(view.pyStdoutStrVec.end(), v2.begin(), v2.end());
							}
							int keyModNdx = 0;
							if (view.pyStdoutKeyModNdxs.size() > 0) keyModNdx = view.pyStdoutKeyModNdxs.back();
							view.pyStdoutKeyModNdxs.push_back((int)v.size() + keyModNdx);
							//editors[sharedData.whichPyPane].setString(sharedData.pyStdoutStr.substr(prevEnd, bracketStart-prevEnd));
						}
						// if we start with a key modifier, the modifiers indexes vector will be empty
						// so we populate it with a 0
						if (view.pyStdoutKeyModNdxs.empty()) view.pyStdoutKeyModNdxs.push_back(0);
						view.pyStdoutKeyModifiers.push_back(keyModifiers[i]);
						//handleKeyModifier(keyModifiers[i]);
						prevEnd = bracketEnd;
						lastModifierNdx = i;
					}
				}
				// check if there is anything remaining in the string after the last modifier key
				if ((int)view.pyStdoutStr.size() > angleBracketNdxs[lastModifierNdx] + keyModifiers[lastModifierNdx].ndxInc + 1) {
					int substrStart = angleBracketNdxs[lastModifierNdx] + keyModifiers[lastModifierNdx].ndxInc;
					std::vector<std::string> v = session.tokenizeString(view.pyStdoutStr.substr(substrStart), "\n", true);
					for (std::string s : v) {
						std::vector<std::string> v2 = session.tokenizeString(s, "\b", true);
						view.pyStdoutStrVec.insert(view.pyStdoutStrVec.end(), v2.begin(), v2.end());
					}
					//editors[sharedData.whichPyPane].setString(sharedData.pyStdoutStr.substr(substrStart));
				}
			}
			view.typePyStrLineByLine = true;
			view.pyStdoutStr.clear();
			view.pyStdoutStrNdx = 0;
			//sharedData.typePyStr = false;
		}
	}

	if (showScope && session.pyoSet) {
		// one waveform for every output channel, sized here as pyo starts in the engine (see Session::initPyo())
		if ((int)scopeWaveforms.size() != session.outChannels) scopeWaveforms.resize(session.outChannels);
		for (int i = 0; i < session.outChannels; i++) {
			scopeWaveforms[i].clear();
		}
		// the last output buffer, copied from the audio thread
		std::vector<float> samples;
		int channels;
		{
			std::lock_guard<std::mutex> lock(scopeMutex);
			samples = scopeSamples;
			channels = scopeChannels;
		}
		size_t numFrames = (channels > 0 ? samples.size() / channels : 0);
		// the root mean square of all the samples, as ofSoundBuffer::getRMSAmplitude() computed it
		double squares = 0;
		for (float sample : samples) squares += (double)sample * sample;
		scopeRms = (samples.empty() ? 0.0f : (float)std::sqrt(squares / samples.size()));
		for(size_t i = 0; i < numFrames; i++) {
			for (int j = 0; j < session.outChannels && j < channels; j++) {
				float sample = samples[i * channels + j];
				float x = ofMap(i, 0, numFrames, scoreXOffset, scoreBackgroundWidth+scoreXOffset);
				float y = ((sample * (scoreBackgroundHeight / 2)) * scopeRms) + (scoreBackgroundHeight / 2);
				if (scoreOrientation == 1) y += scoreYOffset;
				scopeWaveforms[j].addVertex(x, y);
			}
		}
	}
}

/********************* drawing functions ***********************/
//--------------------------------------------------------------
void ofApp::draw()
{
	ofSetLineWidth(lineWidth);
	for (std::map<int, Editor>::iterator it = editors.begin(); it != editors.end(); ++it) {
		it->second.drawText();
		paintGrids(it->second.takePaints(), font, brightnessCoeff);
		// draw the pane separator for panes that do not touch the traceback printing area
		if ((paneSplitOrientation == 0 && numPanes.find(it->second.getPaneRow()+1) != numPanes.end()) || \
				(paneSplitOrientation == 1 && it->second.getPaneCol() < numPanes[it->second.getPaneRow()])) {
			it->second.drawPaneSeparator();
			paintGrids(it->second.takePaints(), font, brightnessCoeff);
		}
	}
	ofSetColor(foregroundColor*brightnessCoeff);
	// draw the lines that separate the different editors
	// initializing to 0 to avoid warning messages during compilation
	float paneWidth = 0, paneHeight = 0;
	// depending on the orientation, we must first create either the height or the width of the editors
	if (paneSplitOrientation == 0) {
		paneHeight = workspace.belowBase / (float)numPanes.size();
	}
	else {
		paneWidth = (float)view.screenWidth / (float)numPanes.size();
	}
	for (std::map<int, int>::iterator it = numPanes.begin(); it != numPanes.end(); ++it) {
		if (paneSplitOrientation == 0) {
			paneWidth = (float)view.screenWidth / (float)it->second;
			// we start the following loop from 1 because if we have one column only
			// we don't need to draw any separating lines
			for (int col = 1; col < it->second; col++) {
				ofDrawLine(paneWidth*(float)col, paneHeight*(float)it->first, paneWidth*(float)col, paneHeight*(float)(it->first+1));
			}
		}
		else {
			// if the orientation is switched, we treat rows as columns and vice versa
			if (it->first > 0) {
				ofDrawLine(paneWidth*(float)it->first, 0, paneWidth*(float)it->first, workspace.belowTop);
			}
			paneHeight = workspace.belowBase / (float)it->second;
		}
	}
	//if (sharedData.showNotes) {
	//	drawScore();
	//}
	//else if (showPianoRoll) {
	//	drawPianoRoll();
	//}
	//else if (showScope) {
	//	drawScope();
	//}
	if (showScore) {
		switch (scoreType) {
			case 0:
				drawScore();
				break;
			case 1:
				drawPianoRoll();
				break;
			case 2:
				drawScope();
				break;
			case 3:
				drawSequencer();
				break;
		}
	}
	// draw the pane separator for panes that do touch the traceback printing area
	for (std::map<int, Editor>::iterator it = editors.begin(); it != editors.end(); ++it) {
		if ((paneSplitOrientation == 0 && numPanes.find(it->second.getPaneRow()+1) == numPanes.end()) || \
				(paneSplitOrientation == 1 && it->second.getPaneCol() >= numPanes[it->second.getPaneRow()])) {
			it->second.drawPaneSeparator();
			paintGrids(it->second.takePaints(), font, brightnessCoeff);
		}
	}
	// draw a horizontal line to separate the panes and score from the traceback area
	ofSetColor(foregroundColor*brightnessCoeff);
	ofDrawLine(0, workspace.belowTop, view.screenWidth, workspace.belowTop);
	// the shell, or the messages of the lines, below the panes (see Workspace::fillBelowArea())
	workspace.fillBelowArea();
	paintBelowGrid();
	// the pop-up of autocompletion is drawn last, so that it is on top of everything
	editors[session.whichPane].drawCompletions();
	paintGrids(editors[session.whichPane].takePaints(), font, brightnessCoeff);
	// in test mode, a few frames after the file has run, the screen as it is drawn is added to the dump, the window is saved
	// if a screenshot was asked for, and the program quits (see testmode.cpp)
	if (!testFile.empty() && testDone && testKeyNdx >= testKeys.size() && !testScreenWritten && ofGetFrameNum() > testDoneFrame + 3) {
		testScreenWritten = true;
		appendTestEnd();
		if (!testScreenshot.empty()) ofSaveScreen(testScreenshot);
		ofExit(0);
	}
}

//--------------------------------------------------------------
void ofApp::paintBelowGrid()
{
	// the text sits at the bottom of every row (see drawLines()), and the cursor of the shell is lower than its row
	GridGeometry geometry;
	geometry.x = halfCharacterWidth;
	geometry.y = workspace.belowTop;
	geometry.cellWidth = (int)oneCharacterWidth;
	geometry.cellHeight = cursorHeight;
	geometry.descent = 0;
	geometry.snapCursor = false;
	geometry.cursorShift = cursorHeight - (cursorHeight * 0.85);
	paintGrid(belowGrid, geometry, font, brightnessCoeff);
}

//--------------------------------------------------------------
void ofApp::drawLines(const std::string& str, float x, float y)
{
	// draws text with many lines, with the line height of the editor, which the areas below the panes are measured in
	// (drawString() would use the line height of the font, which is greater, so the lines would drift down)
	size_t start = 0;
	int lineNdx = 0;
	while (start <= str.size()) {
		size_t end = str.find('\n', start);
		if (end == std::string::npos) end = str.size();
		if (end > start) font.drawString(str.substr(start, end - start), x, y + (lineNdx * cursorHeight));
		lineNdx++;
		start = end + 1;
	}
}

//--------------------------------------------------------------
void ofApp::moveScoreWithTraceback(float prevTracebackYCoord)
{
	// move the score by as much as the traceback has grown or shrunk since its previous size
	// and not by the whole difference from the base of the traceback, which would accumulate every time
	// the number of lines changes, and would eventually push the score out of the window
	// a score shown at the top of the window in horizontal view is not reached by the traceback
	// so it is not moved, as moving it would push its upper part out of the window
	if (scoreOrientation == 1 && scoreYOffset <= 0) return;
	scoreYOffset += (workspace.belowTop - prevTracebackYCoord);
}

//--------------------------------------------------------------
void ofApp::drawScore()
{
	ofSetLineWidth(lineWidth);
	ofSetColor(scoreBackgroundColor * brightnessCoeff);
	ofDrawRectangle(scoreXOffset, scoreYOffset, scoreBackgroundWidth, scoreBackgroundHeight);
	// safety test to avoid crashes when changing the window size before creating any bars
	if (session.sharedData.loopIndex >= (int)session.sharedData.loopData.size()) return;
	//------------- variables for horizontal score view ------------------
	//int numBars = 1;
	bool drawLoopStartEnd = true;
	int bar; //, prevBar = 0;
	int ndx = session.sharedData.thisBarIndex;
	if (scoreOrientation > 0) {
		// numBars is part of the ofApp class and it's used to be compared to the previous number of bars displayed
		// which is necessary to properly position single bars in horizontal view
		view.numBars = std::min(numBarsToDisplay, (int)session.sharedData.loopData[session.sharedData.loopIndex].size());
		drawLoopStartEnd = false;
		// the equation below yields the first bar to display
		// the rest will be calculated by adding the loop variable i
		// e.g. for an 8-bar loop with two bars being displayed
		// the line below yields 0, 2, 4, 6, every other index (it actually ignores the odd indexes)
		ndx = ((session.sharedData.thisBarIndex - (session.sharedData.thisBarIndex % view.numBars)) / view.numBars) * view.numBars;
		// in the if test below we set the number of bars to be displayed to fit the remaining bars
		// e.g. in a six-bar loop, when the first four bars have been played, we need to display two bars only
		// not four bars which is the maximum number of bars to display (these numbers are hypothetical)
		view.numBars = std::min(view.numBars, (int)session.sharedData.loopData[session.sharedData.loopIndex].size() - ndx);
	}
	else {
		view.numBars = 1;
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
	if (session.sharedData.instruments.size() > 0) {
		if (view.beatAnimate) {
			beatPulseMinPosY = view.yStartPnts[scoreOrientation] - view.noteHeight;
			beatPulseMaxPosY = view.yStartPnts[scoreOrientation] + (view.staffLinesDist * 4);
			// one staff for every instrument that is not merged into the staff of another one (see \merge)
			bool firstStaff = true;
			for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
				if (session.mergeHostOf(it->first) != it->first) continue;
				if (!firstStaff) beatPulseMaxPosY += (view.allStaffDiffs + (view.staffLinesDist * 2));
				firstStaff = false;
			}
			beatPulseMaxPosY += view.noteHeight;
			beatPulseSizeY = beatPulseMaxPosY - beatPulseMinPosY;
			if (view.beatViz) {
				switch (view.beatVizType) {
					case 1:
						beatPulseStartX = scoreXStartPnt + scoreXOffset;
						beatPulseEndX = scoreXStartPnt + scoreXOffset + session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getStaffXLength();
						if (scoreOrientation == 1) {
							beatPulseEndX += (session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getStaffXLength() * (view.numBars-1));
							beatPulseEndX -= (session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getClefXOffset() * (view.numBars-1));
							beatPulseEndX -= (session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getMeterXOffset() * (view.numBars-1));
						}
						beatPulsePosX = beatPulseStartX - view.staffLinesDist;
						beatPulseSizeX = beatPulseEndX - beatPulseStartX;
						beatPulseSizeX += view.staffLinesDist;
						beatPulseMinPosY += scoreYOffset;
						ofSetColor(beatColor.r*brightnessCoeff,
								   beatColor.g*brightnessCoeff,
								   beatColor.b*brightnessCoeff,
								   view.beatVizCosTab[view.beatVizDegrade] * BEATVIZCOEFF);
						ofDrawRectangle(beatPulsePosX, beatPulseMinPosY, beatPulseSizeX, beatPulseSizeY);
						break;
					default:
						break;
				}
				// regardless of the beattype, we calculate the alpha value here
				if ((Clock::millis() - view.beatVizTimeStamp) >= view.beatVizRampStart) {
					// calculate how many steps the brightness has to dim, depending on the elapsed time and the steps per millisecond
					int brightnessDegrade = (int)((Clock::millis()-(view.beatVizTimeStamp+view.beatVizRampStart))*view.beatVizStepsPerMs);
					view.beatVizDegrade = BEATVIZBRIGHTNESS - brightnessDegrade;
					if (view.beatVizDegrade < 0) {
						view.beatVizDegrade = 0;
						view.beatViz = false;
						view.beatVizDegrade = BEATVIZBRIGHTNESS;
					}
				}
			}
		}
		// then we display the score
		float staffOffsetX = scoreXStartPnt + scoreXOffset;
		float notesOffsetX = staffOffsetX;
		//notesOffsetX += sharedData.instruments.begin()->second.getClefXOffset();
		//notesOffsetX += sharedData.instruments.begin()->second.getMeterXOffset();
		notesOffsetX += (view.staffLinesDist * NOTESXOFFSETCOEF);
		//float prevNotesOffsetX = notesOffsetX;
		//float notesXCoef = (sharedData.instruments.begin()->second.getStaffXLength() + staffOffsetX - notesOffsetX) / notesLength;
		if (showBarCount && session.sharedData.instruments.size() > 0) {
			ofColor color = toOf(session.sharedData.instruments[0].displayAs<ScoreView>().getStaffColor());
			ofSetColor(color.r*brightnessCoeff, color.g*brightnessCoeff, color.b*brightnessCoeff);
			std::string barCountStr = "(" + std::to_string(session.sharedData.thisBarIndex+1) + "/" + std::to_string(session.sharedData.loopData[session.sharedData.loopIndex].size()) + ") " + std::to_string(session.sharedData.barCounter);
			font.drawString(barCountStr, scoreXOffset+scoreBackgroundWidth-font.stringWidth(barCountStr)-20, scoreYOffset+20);
		}
		std::pair prevMeter = std::make_pair(4, 4);
		std::pair prevTempo = std::make_pair(0, 0);
		std::vector<int> prevClefs (session.sharedData.instruments.size(), 0);
		bool prevDrawClef = false, prevDrawMeter = false;
		for (int i = 0; i < view.numBars; i++) {
			bool drawClef = false, drawMeter = false, animate = false;
			bool drawTempo = false;
			bool showBar = true;
			// safety test to avoid division by zero, in case something went wrong when setting a loop
			if (session.sharedData.loopData[session.sharedData.loopIndex].size() == 0) break;
			int barNdx = (ndx + i) % (int)session.sharedData.loopData[session.sharedData.loopIndex].size();
			bar = session.sharedData.loopData[session.sharedData.loopIndex][barNdx];
			if (i == 0) {
				drawClef = true;
				for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
					prevClefs[it->first] = it->second.getClef(bar);
				}
			}
			else {
				drawClef = false;
				for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
					if (prevClefs[it->first] != it->second.getClef(bar)) {
						drawClef = true;
						break;
					}
				}
				for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
					prevClefs[it->first] = it->second.getClef(bar);
				}
			}
			// get the meter of this bar to determine if it has changed and thus it has be to written
			std::pair<int, int> thisMeter = session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getMeter(bar);
			std::pair<int, int> thisTempo = std::make_pair(session.sharedData.tempoBaseForScore[bar], session.sharedData.BPMTempi[bar]);
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
			if (barNdx == (int)session.sharedData.thisBarIndex) animate = true;
			// the index below is used in insertNaturalSigns() in Notes() in score.cpp to deterimine if we must
			// insert a natural sign at the beginning of a bar in case the same note appears
			// in a previous bar with an accidental (this concerns only the visible bars, e.g. 4 bars in one row)
			// mainly used in LiveLily Part
			int insertNaturalsNdx = session.sharedData.loopIndex;
			// determine which notes all bars but the last must display in case of horizontal view
			// this is so that when the last bar of the displayed line is played, in case there are performers following the score
			// they should be able to look at the left side of the score to see what notes come right after the current playing bar
			// if we are calling a new loop, all bars but the last should display the first bars of the next loop
			// unless we set scoreChangeOnLast to false, where on the last run (e.g. an eight-bar loop will run twice with four bars being display)
			// every bar that finishes playing must display the respective bar of the upcoming loop
			int threshold = (int)session.sharedData.loopData[session.sharedData.loopIndex].size() - view.numBars + i;
			if (scoreChangeOnLastBar) threshold = (int)session.sharedData.loopData[session.sharedData.loopIndex].size() - 2;
			if (mustUpdateScore && (int)session.sharedData.thisBarIndex > threshold && i < view.numBars - 1) {
				if (i >= (int)session.sharedData.loopData[session.sharedData.tempLoopIndex].size()) {
					showBar = false;
				}
				else {
					bar = session.sharedData.loopData[session.sharedData.tempLoopIndex][i];
					insertNaturalsNdx = session.sharedData.tempLoopIndex;
				}
				scoreUpdated = true;
			}
			else if (mustUpdateScore && scoreUpdated && session.sharedData.thisBarIndex == 0) {
				mustUpdateScore = scoreUpdated = false;
			}
			// if we're staying in the same loop, when the currently playing bar is displayed at the right-most staff
			// all the other bars must display the next bars of the loop (if there are any)
			else if ((int)session.sharedData.thisBarIndex - ndx == view.numBars - 1 && i < view.numBars - 1) {
				bar = session.sharedData.loopData[session.sharedData.loopIndex][(barNdx+view.numBars)%(int)session.sharedData.loopData[session.sharedData.loopIndex].size()];
				// the calculations below determine whether the remaining bars are less than numBars - 1
				// (we subtract one because in the last slot, we display the bar of the current loop chunk)
				// in which case, we should leave the remaining slots empty
				// for example, in a 6-bar loop with numBars = 4, when we enter the second chunk of the loop
				// we need to display two bars
				// when at the last bar of the first chunk, we display the 5th and 6th bar on the left side
				// and the 4th bar on the right side, with the 3rd slot being blank
				int loopSize = (int)session.sharedData.loopData[session.sharedData.loopIndex].size();
				int ndxLocal = ndx + view.numBars;
				if (ndxLocal >= loopSize) ndxLocal = 0;
				int numBarsLocal = std::min(std::min(numBarsToDisplay, loopSize), loopSize - ndxLocal);
				if (i >= numBarsLocal) showBar = false;
				else showBar = true;
			}
			// like with the beat visualization, accumulate X offsets for all bars but the first
			if (i > 0) {
				// only the notes need to go back some pixels, in case the clef or meter is not drawn
				// so we use a separate value for the X coordinate
				staffOffsetX += session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getStaffXLength();
				notesOffsetX += session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getStaffXLength();
			}
			if (prevDrawClef != drawClef) {
				if (drawClef) notesOffsetX += session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getClefXOffset();
				else notesOffsetX -= session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getClefXOffset();
			}
			if (prevDrawMeter != drawMeter) {
				if (drawMeter) notesOffsetX += session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getMeterXOffset();
				else notesOffsetX -= session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getMeterXOffset();
			}
			// set position of single bars based on the position of the bars of the previous loop
			if (view.numBars == 1 && scoreOrientation > 0) {
				// initially display the single bar at the second position, and alternate with the third one
				// but only with positions that are inside the score, as there are numBarsToDisplay positions
				// (with two bars, the third position would be outside of the window, and the score would disappear)
				int firstSlot = (numBarsToDisplay > 1 ? 1 : 0);
				int secondSlot = (numBarsToDisplay > 2 ? 2 : 0);
				int slot = firstSlot;
				if ((view.prevNumBars > 1 || (view.prevNumBars == 1 && view.prevPosition == 0)) && session.sequencer.isThreadRunning()) {
					// but the previous loop was a single bar at the first of these positions, display the current single bar at the other one
					slot = secondSlot;
					view.thisPosition = 1;
				}
				else {
					view.thisPosition = 0;
				}
				staffOffsetX += session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getStaffXLength() * (float)slot;
				notesOffsetX += session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getStaffXLength() * (float)slot;
			}
			float notesXCoef = notesLength * session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getXCoef(); // sharedData.instruments.begin()->second.getStaffXLength();
			if (drawClef) notesXCoef -= session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getClefXOffset();
			if (drawMeter) notesXCoef -= session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getMeterXOffset();
			notesXCoef /= notesLength; // (notesLength * sharedData.instruments.begin()->second.getXCoef());
			// if we're displaying the beat pulse on each beat instead of a rectangle that encloses the whole score
			// we need to display it here, in case we display the score horizontally, where we need to give
			// an offset to the pulse so it is displayed on top of the currently playing bar
			if (view.beatAnimate && view.beatViz && view.beatVizType == 2) {
				// draw only for the currenlty playing bar, whichever that is in the horizontal line
				if (barNdx == (int)session.sharedData.thisBarIndex) {
					beatPulseStartX = notesOffsetX + ((notesLength  / session.sharedData.numerator[session.getPlayingBarIndex()]) * session.sharedData.beatCounter) * notesXCoef;
					//beatPulseStartX = ((notesLength * notesXCoef)  / sharedData.numerator[getPlayingBarIndex()]) * sharedData.beatCounter;
					//beatPulseStartX *= notesXCoef;
					//beatPulseStartX += notesOffsetX;
					if (session.sharedData.beatCounter == -1) beatPulseStartX = notesOffsetX - session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getClefXOffset();
					noteSize = session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getNoteWidth();
					beatPulsePosX = beatPulseStartX - noteSize;
					beatPulseSizeX = noteSize * 2;
					beatPulseMinPosY += scoreYOffset;
					ofSetColor(beatColor.r*brightnessCoeff,
							   beatColor.g*brightnessCoeff,
							   beatColor.b*brightnessCoeff,
							   view.beatVizCosTab[view.beatVizDegrade] * BEATVIZCOEFF);
					ofDrawRectangle(beatPulsePosX, beatPulseMinPosY, beatPulseSizeX, beatPulseSizeY);
				}
			}
			// the bar that is played after this one, towards which glissandi, ties, slurs, and crescendi or diminuendi at its end go
			// it is the next bar of the loop this bar is shown from, or the first bar of a loop that has been called, at the end of the loop
			int nextBarOfScore = -1;
			{
				const std::vector<int>& loop = session.sharedData.loopData[insertNaturalsNdx];
				size_t pos = loop.size();
				if (insertNaturalsNdx == session.sharedData.loopIndex && barNdx < (int)loop.size() && loop[barNdx] == bar) pos = barNdx;
				else pos = std::find(loop.begin(), loop.end(), bar) - loop.begin();
				if (pos < loop.size()) {
					if (pos + 1 < loop.size()) nextBarOfScore = loop[pos+1];
					else if (insertNaturalsNdx == session.sharedData.loopIndex && session.sharedData.tempLoopIndex != session.sharedData.loopIndex &&
							session.sequencer.isThreadRunning() && !session.sharedData.loopData[session.sharedData.tempLoopIndex].empty()) {
						nextBarOfScore = session.sharedData.loopData[session.sharedData.tempLoopIndex].front();
					}
					else nextBarOfScore = loop.front();
				}
			}
			float yStartPnt = view.yStartPnts[scoreOrientation];
			// the Y coordinate of every staff, by the instrument it belongs to
			// instruments merged into the staff of another one (see \merge) are drawn on that staff, below
			std::map<int, float> staffYOf;
			{
				float staffY = yStartPnt;
				for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
					if (session.mergeHostOf(it->first) != it->first) continue;
					staffYOf[it->first] = staffY;
					staffY += (view.allStaffDiffs + (view.staffLinesDist * 2));
				}
			}
			unsigned j = 0;
			std::map<int, Instrument>::iterator prevInst;
			for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
				int host = session.mergeHostOf(it->first);
				if (host != it->first) {
					// a voice of the staff of another instrument has no staff, name, or connecting lines of its own
					if (showBar && staffYOf.find(host) != staffYOf.end()) {
						it->second.displayAs<ScoreView>().drawNotes(bar, i, &session.sharedData.loopData[insertNaturalsNdx], notesOffsetX, staffYOf[host], scoreYOffset, animate, notesXCoef, nextBarOfScore);
					}
					continue;
				}
				if (i == 0) {
					// write the names of the instruments without the backslash and get the longest name
					// only at the first iteration of the bars loop
					// a staff with merged instruments (see \merge) has their names one below the other, in the order of their voices
					// centered on the staff, where the name of a single instrument is
					std::vector<int> staffInsts = {it->first};
					for (const auto& group : session.mergeGroups) {
						if (group.size() > 1 && group.front() == it->first) staffInsts = group;
					}
					float lineHeight = instFont.getLineHeight();
					float firstYPos = yStartPnt + (view.staffLinesDist*2.5) + scoreYOffset - (lineHeight * (float)(staffInsts.size() - 1) / 2.0f);
					for (size_t k = 0; k < staffInsts.size(); k++) {
						Instrument& inst = session.sharedData.instruments[staffInsts[k]];
						float xPos = scoreXOffset + (view.blankSpace * 0.75) + (view.longestInstNameWidth - instNameWidths[staffInsts[k]]);
						float yPos = firstYPos + (lineHeight * (float)k);
						ofColor color = toOf(inst.displayAs<ScoreView>().getStaffColor());
						ofSetColor(color.r*brightnessCoeff, color.g*brightnessCoeff, color.b*brightnessCoeff);
						instFont.drawString(inst.getName(), xPos, yPos);
					}
				}
				// then draw one line at each side of the staffs to connect them
				// we draw the first one here, and the rest inside the for loop below
				// the line goes from the top line of this staff to the bottom line of the previous one
				// at the same X and Y coordinates the staves are drawn at:
				// staffOffsetX includes the X offset of all previous bars (and of a single bar in horizontal view)
				// and scoreYOffset is the Y offset of the score (e.g. when the score is at the bottom in horizontal view)
				// the start of the staff can also be moved with \movex, so we include this too
				float connectingLineX = staffOffsetX + it->second.displayAs<ScoreView>().getStaffXStartOffset();
				float connectingLineY1 = yStartPnt + scoreYOffset;
				float connectingLineY2 = connectingLineY1 - view.allStaffDiffs + (view.staffLinesDist * 2);
				// each staff draws its own vertical lines at its sides (from its top to its bottom line) in Staff::drawStaff()
				// here we draw the lines between the staves:
				// on the left side of the score (the first bar), all staves are connected, from the top line of the top staff
				// to the bottom line of the bottom staff, by connecting each staff to the previous one
				// on the right side of every bar (and between bars in horizontal view), only groupped staves are connected
				if (j > 0 && showBar) {
					if (i == 0) {
						ofDrawLine(connectingLineX, connectingLineY1, connectingLineX, connectingLineY2);
					}
					// if this instrument is groupped with the previous one, connect them at the end of the bar
					if (it->second.getGroup() == prevInst->second.getID() && it->second.getGroup() > -1) {
						float connectingLineXEnd = staffOffsetX + session.sharedData.instruments.begin()->second.displayAs<ScoreView>().getStaffXLength();
						ofDrawLine(connectingLineXEnd, connectingLineY1, connectingLineXEnd, connectingLineY2);
					}
				}
				// the tempo belongs to the bar and not to an instrument, so it is drawn once, above the top staff
				// drawn above every other staff, it would fall on the staff above it
				bool drawTempoLocal = (showTempo && drawTempo && j == 0);
				if (showBar) {
					it->second.displayAs<ScoreView>().drawStaff(bar, staffOffsetX, yStartPnt, scoreYOffset, drawClef, drawMeter, drawLoopStartEnd, drawTempoLocal);
					it->second.displayAs<ScoreView>().drawNotes(bar, i, &session.sharedData.loopData[insertNaturalsNdx], notesOffsetX, yStartPnt, scoreYOffset, animate, notesXCoef, nextBarOfScore);
				}
				yStartPnt += (view.allStaffDiffs + (view.staffLinesDist * 2));
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
	// once done drawing the score, draw the line that separates it from the editor
	// this is important in case of changing the color of the score, to clearly separate the two
	if (scoreOrientation < 2) {
		ofColor color;
		color.r = 255 - scoreBackgroundColor.r;
		color.g = 255 - scoreBackgroundColor.g;
		color.b = 255 - scoreBackgroundColor.b;
		// draw this separating line only if its color is different than the background color of the editor
		if (color != backgroundColor) {
			ofSetColor(color * brightnessCoeff);
			if (scoreOrientation == 0) {
				ofDrawLine(view.middleOfScreenX, 0, view.middleOfScreenX, view.screenHeight);
			}
			else {
				ofDrawLine(0, scoreBackgroundHeight, view.screenWidth, scoreBackgroundHeight);
			}
		}
	}
}

//--------------------------------------------------------------
int ofApp::noteNameToMidi(const std::string& name)
{
	static const std::map<char, int> pitchClasses = {{'c', 0}, {'d', 2}, {'e', 4}, {'f', 5}, {'g', 7}, {'a', 9}, {'b', 11}};
	if (name.empty() || pitchClasses.find(std::tolower(name[0])) == pitchClasses.end()) return -1;
	int pitch = pitchClasses.at(std::tolower(name[0]));
	size_t ndx = 1;
	// sharps and flats, as in LiveLily (e.g. fis, bes)
	while (ndx + 1 < name.size() && (name.compare(ndx, 2, "is") == 0 || name.compare(ndx, 2, "es") == 0)) {
		pitch += (name[ndx] == 'i' ? 1 : -1);
		ndx += 2;
	}
	std::string octave = name.substr(ndx);
	if (!session.isNumber(octave)) return -1;
	// the octaves are numbered as in scientific pitch notation, where c4 is the middle C (MIDI 60)
	int midi = (std::stoi(octave) + 1) * 12 + pitch;
	return (midi >= 0 && midi <= 127 ? midi : -1);
}

//--------------------------------------------------------------
void ofApp::setPianoRollRange(int lowestKey, int highestKey)
{
	auto isBlack = [](int note) { int pc = note % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; };
	view.pianoRollLowestKey = std::max(0, lowestKey);
	view.pianoRollHighestKey = std::min(127, highestKey);
	int numWhiteKeys = 0;
	for (int note = view.pianoRollLowestKey; note <= view.pianoRollHighestKey; note++) {
		if (!isBlack(note)) numWhiteKeys++;
	}
	view.pianoRollNumWhiteKeys = std::max(1, numWhiteKeys);
	// the keyboard goes from the start of its lowest key to the end of its highest key
	// a white key takes the whole width of a white key, and a black key the width of its lane (see pianoRollLane())
	// so a keyboard can start or end with a black key (e.g. c1 cis4), without the white key next to it
	view.pianoRollOrigin = 0;
	std::pair<float, float> lowest = pianoRollLane(view.pianoRollLowestKey);
	std::pair<float, float> highest = pianoRollLane(view.pianoRollHighestKey);
	static const int whiteIndex[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
	auto whiteStart = [&](int note) { return (float)((note / 12) * 7 + whiteIndex[note % 12]); };
	float start = (isBlack(view.pianoRollLowestKey) ? lowest.first : whiteStart(view.pianoRollLowestKey));
	float end = (isBlack(view.pianoRollHighestKey) ? highest.first + highest.second : whiteStart(view.pianoRollHighestKey) + 1);
	view.pianoRollOrigin = start;
	view.pianoRollLength = std::max(1.0f, end - start);
	// the keys fill the length of the keyboard, which is the height of the score in vertical view, and its width in horizontal view
	if (scoreOrientation == 0) view.pianoRollKeysHeight = scoreBackgroundHeight / view.pianoRollLength;
	else view.pianoRollKeysWidth = scoreBackgroundWidth / view.pianoRollLength;
}

//--------------------------------------------------------------
std::pair<float, float> ofApp::pianoRollLane(int note)
{
	// the start of every pitch class in an octave and the width of its lane, in widths of white keys
	// C to E are three white keys with five lanes, F to B four white keys with seven lanes
	auto laneInOctave = [](int pc) {
		if (pc <= 4) return std::make_pair(pc * 0.6f, 0.6f);
		return std::make_pair(3.0f + ((pc - 5) * (4.0f / 7.0f)), 4.0f / 7.0f);
	};
	// relative to the start of the keyboard (see setPianoRollRange())
	std::pair<float, float> lane = laneInOctave(note % 12);
	return std::make_pair((float)((note / 12) * 7) + lane.first - view.pianoRollOrigin, lane.second);
}

//--------------------------------------------------------------
void ofApp::drawPianoRoll()
{
	ofSetLineWidth(lineWidth);
	// draw the background of the piano roll
	ofSetColor(backgroundColor * brightnessCoeff);
	ofDrawRectangle(scoreXOffset, scoreYOffset, scoreBackgroundWidth, scoreBackgroundHeight);
	bool vertical = (scoreOrientation == 0);
	// the width of a white key along the keyboard, and the depth of the keys (from the side of the notes to the front of the keys)
	float whiteKeyWidth = (vertical ? view.pianoRollKeysHeight : view.pianoRollKeysWidth);
	float keysDepth = (vertical ? view.pianoRollKeysWidth : view.pianoRollKeysHeight);
	// the keyboard is at the right side of the score in vertical view, with the lowest key at the bottom
	// and at the bottom of the score in horizontal view, with the lowest key on the left
	// the black keys are at the side of the keyboard that faces the notes, two thirds of its depth
	float keyboardStart = (vertical ? scoreXOffset + scoreBackgroundWidth - keysDepth : scoreYOffset + scoreBackgroundHeight - keysDepth);
	const float blackKeysDepth = 2.0f / 3.0f;
	// draws a rectangle on the keyboard, from start to end along it (in widths of white keys)
	// and from depthStart to depthEnd across it (0 at the side of the notes, 1 at the front of the keys)
	auto keyRect = [&](float start, float end, float depthStart, float depthEnd) {
		if (vertical) {
			float y1 = scoreYOffset + scoreBackgroundHeight - (end * whiteKeyWidth);
			ofDrawRectangle(keyboardStart + (depthStart * keysDepth), y1, (depthEnd - depthStart) * keysDepth, (end - start) * whiteKeyWidth);
		}
		else {
			ofDrawRectangle(scoreXOffset + (start * whiteKeyWidth), keyboardStart + (depthStart * keysDepth), (end - start) * whiteKeyWidth, (depthEnd - depthStart) * keysDepth);
		}
	};
	auto isBlack = [](int note) { int pc = note % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; };
	static const int whiteIndex[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
	auto whiteKeyStart = [&](int note) {
		return (float)((note / 12) * 7 + whiteIndex[note % 12]) - view.pianoRollOrigin;
	};
	float keyboardLength = view.pianoRollLength;
	// the notes that play, highlighted on the keyboard with the color of their instrument, as opaque as their velocity
	// by key and by instrument, so that a key that more instruments play is highlighted with their colors mixed (see highlightKey below)
	struct ActiveKey {
		ofColor color;
		int velocity;
	};
	std::map<int, std::map<int, ActiveKey>> activeKeys;
	// the notes of the roll, collected before they are drawn, so that notes of different instruments on the same key at the same time
	// share the lane of the key, side by side, instead of hiding one another (see below)
	struct RollNote {
		int inst;
		int key;
		float pos;
		float length;
		ofColor color;
		int velocity;
	};
	std::vector<RollNote> rollNotes;
	// then draw the notes of all instruments
	if (session.sharedData.instruments.size() > 0 && session.sharedData.loopData.size() > 0) {
		uint64_t timeStamp = Clock::millis();
		// the movement of the notes depends on the tempo of the currently playing bar
		int thisBar = session.sharedData.loopData[session.sharedData.loopIndex][session.sharedData.thisBarIndex];
		// the length of the area of the notes, from the edge of the score to the keyboard
		float rollLength = (vertical ? scoreBackgroundWidth - view.pianoRollKeysWidth : scoreBackgroundHeight - view.pianoRollKeysHeight);
		float rollStart = (vertical ? scoreXOffset : scoreYOffset);
		// with the piano roll we show one bar only
		// but as the notes roll, we start showing the next bar too
		// so we need two bars (or the same bar twice, in case of a single-bar loop)
		// hence the two iteration loop below
		for (int i = 0; i < 2; i++) {
			int bar = session.sharedData.loopData[session.sharedData.loopIndex][session.sharedData.thisBarIndex];
			if (mustUpdateScore && session.sharedData.thisBarIndex == session.sharedData.loopData[session.sharedData.loopIndex].size()-1 && i > 0) {
				scoreUpdated = true;
			}
			else if (mustUpdateScore && session.sequencer.isUpdated() && session.sharedData.thisBarIndex == 0) {
				mustUpdateScore = scoreUpdated = false;
			}
			if (i) {
				if (scoreUpdated) {
					bar = session.sharedData.loopData[session.sharedData.tempLoopIndex][0];
				}
				else {
					if (session.sharedData.thisBarIndex+1 >= session.sharedData.loopData[session.sharedData.loopIndex].size()) {
						bar = session.sharedData.loopData[session.sharedData.loopIndex][0];
					}
					else {
						bar = session.sharedData.loopData[session.sharedData.loopIndex][session.sharedData.thisBarIndex+1];
					}
				}
			}
			for (auto instIt = session.sharedData.instruments.begin(); instIt != session.sharedData.instruments.end(); ++instIt) {
				// a muted instrument is not shown, as it doesn't play
				if (instIt->second.isMuted()) continue;
				ofSetColor(toOf(instIt->second.getColor()) * brightnessCoeff);
				float posAccum = 0;
				// safety test to ensure the bar we're trying to display actually exists
				if (instIt->second.notes.find(bar) == instIt->second.notes.end()) {
					continue;
				}
				// every bar fills the roll, whatever its meter, as the roll moves by its length in the time of a bar
				// (numBeats is the length of a bar in the units of the durations, MINDUR for a whole note)
				float barUnits = (session.sharedData.numBeats[bar] > 0 ? (float)session.sharedData.numBeats[bar] : (float)MINDUR);
				float unitLength = rollLength / barUnits;
				for (size_t j = 0; j < instIt->second.notes.at(bar).size(); j++) {
					float noteLength = (float)instIt->second.durs.at(bar).at(j) * unitLength;
					float durPercentage = instIt->second.durPercentages[instIt->second.articulations[bar][j][0]];
					// the position of the note along the roll, towards the keyboard
					// the ADSR release part is removed from the length of the note, so its start moves by as much
					float pos = rollStart + rollLength - posAccum - noteLength + (noteLength * (1 - durPercentage));
					if (session.sequencer.isThreadRunning()) {
						// advance according to the elapsed time since the start of the bar
						pos += ((rollLength * (timeStamp - view.pianoRollTimeStamp)) / (session.sharedData.tempoMs[thisBar] * session.sharedData.numerator[thisBar]));
					}
					float length = noteLength * durPercentage;
					if (i == 0) {
						// a note of this bar ends at the keyboard
						float diff = length - (rollStart + rollLength - pos);
						if (diff > 0) length -= diff;
						// and is not drawn beyond the edge of the score
						if (pos < rollStart) {
							length -= (rollStart - pos);
							pos = rollStart;
						}
					}
					else {
						// a note of the next bar starts at the edge of the score
						pos -= rollLength;
						length = std::min(length, pos - (rollStart - length));
						pos = std::max(pos, rollStart);
					}
					bool active = session.sequencer.isThreadRunning() && bar == thisBar &&
						((instIt->second.getSeqToggle() && (int)j == instIt->second.getBarDataCounter()) ||
						 (instIt->second.getDurPercentage(bar) == 100 && (int)j == instIt->second.getBarDataCounter()-1));
					const std::vector<int>& velocities = instIt->second.midiVels[bar];
					int velocity = (j < velocities.size() ? velocities[j] : 127);
					for (size_t k = 0; k < instIt->second.notes.at(bar).at(j).size(); k++) {
						int note = (int)instIt->second.notes.at(bar).at(j).at(k);
						if (note < view.pianoRollLowestKey || note > view.pianoRollHighestKey) continue; // -1 is a rest
						if (length <= 0) continue;
						rollNotes.push_back({instIt->first, note, pos, length, toOf(instIt->second.getColor()) * brightnessCoeff, velocity});
						if (active) activeKeys[note][instIt->first] = {toOf(instIt->second.getColor()) * brightnessCoeff, velocity};
					}
					posAccum += noteLength;
				}
			}
		}
	}
	// the notes, each in its lane, a little narrower, so that notes of neighbouring lanes are apart
	// where notes of different instruments are on the same key at the same time, the lane is split between them, for as long as they overlap
	// with the instruments in the order of their indexes, and every note is as opaque as its MIDI velocity (127 is opaque, as in drawSequencer())
	ofEnableAlphaBlending();
	for (const RollNote& note : rollNotes) {
		// the points where the notes of other instruments on the same key start and end during this note
		std::vector<float> points = {note.pos, note.pos + note.length};
		for (const RollNote& other : rollNotes) {
			if (other.key != note.key || other.inst == note.inst) continue;
			if (other.pos >= note.pos + note.length || note.pos >= other.pos + other.length) continue;
			if (other.pos > note.pos) points.push_back(other.pos);
			if (other.pos + other.length < note.pos + note.length) points.push_back(other.pos + other.length);
		}
		std::sort(points.begin(), points.end());
		std::pair<float, float> lane = pianoRollLane(note.key);
		float laneStart = lane.first + (lane.second * 0.1f);
		float laneWidth = lane.second * 0.8f;
		int alpha = std::clamp((int)std::lround((float)note.velocity * 255.0f / 127.0f), 0, 255);
		ofSetColor(note.color.r, note.color.g, note.color.b, alpha);
		for (size_t p = 0; p + 1 < points.size(); p++) {
			float start = points[p], end = points[p+1];
			if (end - start <= 0) continue;
			// the instruments that play this key during this part of the note, and the place of this note among them
			float middle = (start + end) / 2.0f;
			std::set<int> insts;
			for (const RollNote& other : rollNotes) {
				if (other.key == note.key && other.pos <= middle && middle < other.pos + other.length) insts.insert(other.inst);
			}
			insts.insert(note.inst);
			int place = (int)std::distance(insts.begin(), insts.find(note.inst));
			float partWidth = laneWidth / (float)insts.size();
			float partStart = laneStart + (partWidth * (float)place);
			if (vertical) {
				ofDrawRectangle(start, scoreYOffset + scoreBackgroundHeight - ((partStart + partWidth) * whiteKeyWidth), end - start, partWidth * whiteKeyWidth);
			}
			else {
				ofDrawRectangle(scoreXOffset + (partStart * whiteKeyWidth), start, partWidth * whiteKeyWidth, end - start);
			}
		}
	}
	// highlights a part of a key that plays, from start to end along the keyboard and from depthStart to depthEnd across it
	// with the instruments that play it mixed in one color: their colors weighted by how opaque their notes are (by their velocities)
	// and as opaque as the notes would be if they were laid over one another, over the color of the key
	// so one instrument has its own color and the opacity of its note, as the notes of the lanes (see above)
	auto highlightKey = [&](const std::map<int, ActiveKey>& insts, float start, float end, float depthStart, float depthEnd) {
		float r = 0, g = 0, b = 0, weights = 0, transparency = 1;
		for (const auto& inst : insts) {
			float opacity = std::clamp((float)inst.second.velocity / 127.0f, 0.0f, 1.0f);
			r += inst.second.color.r * opacity;
			g += inst.second.color.g * opacity;
			b += inst.second.color.b * opacity;
			weights += opacity;
			transparency *= 1.0f - opacity;
		}
		if (weights <= 0) return;
		ofSetColor(std::lround(r / weights), std::lround(g / weights), std::lround(b / weights), std::lround((1.0f - transparency) * 255.0f));
		keyRect(start, end, depthStart, depthEnd);
	};
	// the white keys, with the notes that play highlighted
	ofSetColor(ofColor::white * brightnessCoeff);
	keyRect(0, keyboardLength, 0, 1);
	for (auto& key : activeKeys) {
		if (isBlack(key.first)) continue;
		float start = whiteKeyStart(key.first);
		// the front of the key is its whole width, and its part between the black keys is from the black key below to the one above
		// within the keyboard, which might start or end with a black key
		highlightKey(key.second, std::max(0.0f, start), std::min(keyboardLength, start + 1), blackKeysDepth, 1);
		float backStart = std::max(0.0f, start), backEnd = std::min(keyboardLength, start + 1);
		if (key.first - 1 >= view.pianoRollLowestKey && isBlack(key.first - 1)) {
			std::pair<float, float> below = pianoRollLane(key.first - 1);
			backStart = below.first + below.second;
		}
		if (key.first + 1 <= view.pianoRollHighestKey && isBlack(key.first + 1)) {
			backEnd = pianoRollLane(key.first + 1).first;
		}
		highlightKey(key.second, backStart, backEnd, 0, blackKeysDepth);
	}
	// the lines between the white keys
	ofSetColor((backgroundColor == ofColor::white ? ofColor(0) : backgroundColor) * brightnessCoeff);
	// at the start of every white key of the range, except at the start of the keyboard
	// and at the end of its last white key, if it ends with a black key (e.g. at the end of c4, when the keyboard ends with cis4)
	for (int note = view.pianoRollLowestKey; note <= view.pianoRollHighestKey + 1; note++) {
		if (isBlack(note)) continue;
		float k = whiteKeyStart(note);
		if (k <= 0.001f || k >= keyboardLength - 0.001f) continue;
		if (vertical) {
			float y = scoreYOffset + scoreBackgroundHeight - (k * whiteKeyWidth);
			ofDrawLine(keyboardStart, y, keyboardStart + keysDepth, y);
		}
		else {
			float x = scoreXOffset + (k * whiteKeyWidth);
			ofDrawLine(x, keyboardStart, x, keyboardStart + keysDepth);
		}
	}
	// the black keys, each over its lane, with the notes that play highlighted
	for (int note = view.pianoRollLowestKey; note <= view.pianoRollHighestKey; note++) {
		if (!isBlack(note)) continue;
		std::pair<float, float> lane = pianoRollLane(note);
		auto activeIt = activeKeys.find(note);
		ofSetColor(0);
		keyRect(lane.first, lane.first + lane.second, 0, blackKeysDepth);
		if (activeIt != activeKeys.end()) {
			highlightKey(activeIt->second, lane.first, lane.first + lane.second, 0, blackKeysDepth);
			// the outline of a highlighted black key, so it is distinguished from the white keys next to it
			ofNoFill();
			ofSetColor(0);
			keyRect(lane.first, lane.first + lane.second, 0, blackKeysDepth);
			ofFill();
		}
	}
	// a line that separates the keyboard from the notes, if the background of the editor is white
	if (backgroundColor == ofColor::white) {
		ofSetColor(0);
		if (vertical) ofDrawLine(keyboardStart, scoreYOffset, keyboardStart, scoreYOffset + scoreBackgroundHeight);
		else ofDrawLine(scoreXOffset, keyboardStart, scoreXOffset + scoreBackgroundWidth, keyboardStart);
	}
	// draw a line that separates the piano roll from the editor
	ofSetColor(foregroundColor * brightnessCoeff);
	if (scoreOrientation == 0 && scoreXOffset > 0) {
		ofDrawLine(scoreXOffset, 0, scoreXOffset, scoreBackgroundHeight);
	}
	else if (scoreYOffset > 0) {
		ofDrawLine(0, scoreYOffset, scoreBackgroundWidth, scoreYOffset);
	}
}

//--------------------------------------------------------------
void ofApp::drawScope()
{
	// draw the background
	ofSetColor(backgroundColor * brightnessCoeff);
	ofDrawRectangle(scoreXOffset, scoreYOffset, scoreBackgroundWidth, scoreBackgroundHeight);
	if (session.pyoSet) {
		// draw the legend
		std::string chanStr = "chan ";
		// get the number of digits of the number of channels
		int numDigits = 0;
		int timesTen = 1;
		while (timesTen <= session.outChannels) {
			timesTen *= 10;
			numDigits++;
		}
		float strWidth = font.stringWidth(chanStr) + (oneCharacterWidth * numDigits);
		float strHeight = font.stringHeight(chanStr);
		for (int i = 0; i < session.outChannels; i++) {
			// use the same colors we use for the piano roll
			ofSetColor(toOf(session.colorNameMap[session.instrumentColors[i%(int)session.instrumentColors.size()]]));
			float x = scoreXOffset+scoreBackgroundWidth-strWidth-10;
			float y = scoreYOffset+(strHeight*(i+1)) + ((strHeight/2)*(i > 0 ? 1 : 0));
			ofDrawLine(x-oneCharacterWidth, y, x-oneCharacterWidth-10, y);
			font.drawString(chanStr + std::to_string(i+1), x, y+(strHeight/2));
		}
		ofSetLineWidth(lineWidth*2);
		// the waveforms, which update() makes, so there might be none yet (e.g. right after \score.show scope)
		for (int i = 0; i < session.outChannels && i < (int)scopeWaveforms.size(); i++) {
			// draw the actual waveforms
			ofSetColor(toOf(session.colorNameMap[session.instrumentColors[i%(int)session.instrumentColors.size()]]));
			scopeWaveforms[i].draw();
		}
		ofSetLineWidth(lineWidth);
	}
	// draw a line that separates the scope from the editor
	ofSetColor(foregroundColor * brightnessCoeff);
	if (scoreOrientation == 0) {
		float x = (scoreXOffset > 0 ? scoreXOffset : scoreBackgroundWidth);
		ofDrawLine(x, 0, x, scoreBackgroundHeight);
	}
	else if (scoreOrientation == 1 && scoreYOffset == 0) {
		ofDrawLine(0, scoreBackgroundHeight, scoreBackgroundWidth, scoreBackgroundHeight);
	}
}

//--------------------------------------------------------------
void ofApp::drawSequencer()
{
	// the step sequencer (\score.show sequencer): a row for every instrument, with a square for every step of \score.seqres in the bar that plays
	// the squares are unfilled, in the foreground color, and every note is a rectangle in the color of its instrument (as in the piano roll)
	// over as many steps as it lasts (a step of the x - notation is one step), and as opaque as its MIDI velocity (127 is opaque)
	ofSetLineWidth(lineWidth);
	ofEnableAlphaBlending();
	ofSetColor(backgroundColor * brightnessCoeff);
	ofDrawRectangle(scoreXOffset, scoreYOffset, scoreBackgroundWidth, scoreBackgroundHeight);
	if (session.sharedData.instruments.empty()) return;
	auto loopIt = session.sharedData.loopData.find(session.sharedData.loopIndex);
	if (loopIt == session.sharedData.loopData.end() || loopIt->second.empty()) return;
	int bar = loopIt->second[std::min((size_t)session.sharedData.thisBarIndex, loopIt->second.size() - 1)];
	// the length of the bar and of a step, in units of the minimum duration, and the steps of the bar (e.g. 12 in 3/4 with 16ths)
	int barLength = (session.sharedData.numBeats.count(bar) ? session.sharedData.numBeats[bar] : MINDUR);
	int stepLength = std::max(1, MINDUR / std::max(1, session.seqResolution));
	int numSteps = std::max(1, (barLength + stepLength - 1) / stepLength);
	// the instruments in the order of the score
	std::vector<int> order;
	for (const auto& ordered : session.sharedData.instrumentIndexesOrdered) {
		if (session.sharedData.instruments.count(ordered.second)) order.push_back(ordered.second);
	}
	if (order.empty()) return;
	// the squares are as large as they can be, after the names of the instruments, and they are centered vertically
	float margin = 10;
	float nameWidth = (float)view.longestInstNameWidth + (margin * 2);
	float square = std::min((scoreBackgroundWidth - nameWidth - margin) / (float)numSteps, (scoreBackgroundHeight - (margin * 2)) / (float)order.size());
	if (square <= 0) return;
	float gridX = scoreXOffset + nameWidth;
	float gridY = scoreYOffset + ((scoreBackgroundHeight - (square * (float)order.size())) / 2.0f);
	ofColor foreground = foregroundColor * brightnessCoeff;
	// a note, from its start to its end in units of the minimum duration, with its velocity
	struct SeqNote {
		int start;
		int end;
		int velocity;
	};
	for (size_t row = 0; row < order.size(); row++) {
		Instrument& inst = session.sharedData.instruments[order[row]];
		float y = gridY + (square * (float)row);
		ofSetColor(foreground);
		instFont.drawString(inst.getName(), scoreXOffset + margin, y + ((square + instFont.stringHeight("A")) / 2.0f));
		// the notes, where a note tied from the note before it is part of that note, and none for a muted instrument, as it doesn't play
		std::vector<SeqNote> seqNotes;
		if (!inst.isMuted() && inst.durs.count(bar)) {
		const std::vector<int>& durs = inst.durs[bar];
		const std::vector<std::vector<float>>& notes = inst.notes[bar];
		const std::vector<int>& ties = inst.tieIndexes[bar];
		const std::vector<int>& velocities = inst.midiVels[bar];
		int onset = 0;
		for (size_t i = 0; i < durs.size(); i++) {
			bool isRest = (i >= notes.size() || notes[i].empty() || notes[i][0] < 0);
			bool tiedFromPrev = (i > 0 && i - 1 < ties.size() && ties[i-1] > -1);
			if (!isRest) {
				if (tiedFromPrev && !seqNotes.empty() && seqNotes.back().end == onset) seqNotes.back().end = onset + durs[i];
				else seqNotes.push_back({onset, onset + durs[i], (i < velocities.size() ? velocities[i] : 127)});
			}
			onset += durs[i];
		}
		}
		// the squares of the steps no note covers, so that a note over many steps is one rectangle, without the lines of the steps in it
		std::vector<bool> covered(numSteps, false);
		for (const SeqNote& note : seqNotes) {
			for (int step = 0; step < numSteps; step++) {
				if (note.start <= step * stepLength && (step + 1) * stepLength <= note.end) covered[step] = true;
			}
		}
		ofNoFill();
		ofSetColor(foreground);
		for (int step = 0; step < numSteps; step++) {
			if (!covered[step]) ofDrawRectangle(gridX + (square * (float)step), y, square, square);
		}
		ofFill();
		ofColor color = toOf(inst.getColor()) * brightnessCoeff;
		for (const SeqNote& note : seqNotes) {
			float x = gridX + (square * (float)note.start / (float)stepLength);
			float width = square * (float)(note.end - note.start) / (float)stepLength;
			// the 256 values of transparency over the 128 values of MIDI velocity, so 127 is opaque
			int alpha = std::clamp((int)std::lround((float)note.velocity * 255.0f / 127.0f), 0, 255);
			ofSetColor(color.r, color.g, color.b, alpha);
			ofDrawRectangle(x, y, width, square);
			// and the outline of the note, in the foreground color, as the squares of the steps
			ofNoFill();
			ofSetColor(foreground);
			ofDrawRectangle(x, y, width, square);
			ofFill();
		}
	}
	// the step that plays, while the sequencer plays, as a red rectangle over the steps of all the rows, with 25% transparency
	if (session.sequencer.isThreadRunning() && session.sharedData.tempoMs.count(bar) && session.sharedData.numerator.count(bar)) {
		float barMs = (float)(session.sharedData.tempoMs[bar] * session.sharedData.numerator[bar]);
		if (barMs > 0) {
			float progress = std::clamp((float)(Clock::millis() - view.pianoRollTimeStamp) / barMs, 0.0f, 1.0f);
			int step = std::min(numSteps - 1, (int)(progress * (float)barLength / (float)stepLength));
			ofColor red = ofColor::red * brightnessCoeff;
			ofSetColor(red.r, red.g, red.b, 191);
			ofDrawRectangle(gridX + (square * (float)step), gridY, square, square * (float)order.size());
		}
	}
}

/******************** key press functions **********************/
//--------------------------------------------------------------
// overloaded keyPressed() function called from OSC
void ofApp::keyPressedOsc(int key, int thisEditor)
{
	// called from an editor when it receives keys from OSC, or for keys typed from Python, in a pane that may be out of focus
	mutex.lock();
	workspace.keyPressedInPane(key, thisEditor);
	mutex.unlock();
}

//--------------------------------------------------------------
// overloaded keyReleased() function called from OSC
void ofApp::keyReleasedOsc(int key, int thisPane)
{
	// the same applies to releasing a key
	mutex.lock();
	workspace.keyReleasedInPane(key, thisPane);
	mutex.unlock();
}

//--------------------------------------------------------------
// OF's default keyPressed() function
void ofApp::keyPressed(int key)
{
	mutex.lock();
	workspace.keyPressed(key);
	mutex.unlock();
}

//--------------------------------------------------------------
// OF's default keyReleased() function
void ofApp::keyReleased(int key)
{
	mutex.lock();
	workspace.keyReleased(key);
	mutex.unlock();
}

//--------------------------------------------------------------
// the following two functions are used to check for key modifiers received from a Python pane
pyStdoutStrKeyModifier ofApp::checkPyStdoutKeyModifier(int ndx)
{
	pyStdoutStrKeyModifier keyMod = {false, 0, 0, 0};
	if (view.pyStdoutStr[ndx] == '<' && ((ndx > 0 && view.pyStdoutStr[ndx-1] != '\\') || ndx == 0)) {
		if (view.pyStdoutStr.substr(ndx).find(">") == std::string::npos) {
			return keyMod;
		}
		std::string s = view.pyStdoutStr.substr(ndx+1, view.pyStdoutStr.substr(ndx+1).find(">"));
		int modifierKey = 0;
		int action = 0;
		if (s.compare("SHIFT_DOWN") == 0) {
			modifierKey = 1;
			action = 1;
		}
		else if (s.compare("SHIFT_UP") == 0) {
			modifierKey = 1;
			action = 2;
		}
		else if (s.compare("CTRL_DOWN") == 0) {
			modifierKey = 2;
			action = 1;
		}
		else if (s.compare("CTRL_UP") == 0) {
			modifierKey = 2;
			action = 2;
		}
		else if (s.compare("ALT_DOWN") == 0) {
			modifierKey = 4;
			action = 1;
		}
		else if (s.compare("ALT_UP") == 0) {
			modifierKey = 4;
			action = 2;
		}
		if (modifierKey > 0) {
			keyMod.isModifier = true;
			keyMod.ndxInc = (int)s.size() + 2;
			keyMod.modifier = modifierKey;
			keyMod.action = action;
			return keyMod;
		}
	}
	return keyMod;
}

//--------------------------------------------------------------
void ofApp::handleKeyModifier(pyStdoutStrKeyModifier modifierStruct)
{
	if (modifierStruct.action == 1) {
		if (view.whichPyPane == session.whichPane) {
			keyPressed(modifierStruct.modifier);
		}
		else {
			keyPressedOsc(modifierStruct.modifier, view.whichPyPane);
		}
	}
	else {
		if (view.whichPyPane == session.whichPane) {
			keyReleased(modifierStruct.modifier);
		}
		else {
			keyReleasedOsc(modifierStruct.modifier, view.whichPyPane);
		}
	}
}

/********************* panes functions ************************/
//--------------------------------------------------------------
// with two columns of panes and the score shown vertically (or two rows and the score shown horizontally)
// the score covers the panes of one column (or row), so when one of them becomes active, the score moves to the other side
void ofApp::revealActivePane()
{
	if (!showScore) return;
	if (scoreOrientation == 0 && paneSplitOrientation == 1 && numPanes.size() == 2) {
		if ((scoreXOffset == 0 && session.whichPane < numPanes[0]) || (scoreXOffset > 0 && session.whichPane >= numPanes[0])) {
			swapScorePosition(0);
		}
	}
	if (scoreOrientation == 1 && paneSplitOrientation == 0 && numPanes.size() == 2) {
		if ((scoreYOffset == 0 && session.whichPane < numPanes[0]) || (scoreYOffset > 0 && session.whichPane >= numPanes[0])) {
			swapScorePosition(1);
		}
	}
}

/*************** arithmetic operations functions **************/

/********************* clearing functions *********************/

/************ command typing/executing functions **************/
/****************** miscellaneous functions ********************/

/****************** OSC to parts functions ********************/

//--------------------------------------------------------------
void ofApp::sendBeatVizInfo(int bar)
{
	// calculate the time stamp regardless of whether we're showing the beat
	// or not in the editor, in case we have connected score parts
	view.beatVizTimeStamp = Clock::millis();
	view.beatVizStepsPerMs = (float)BEATVIZBRIGHTNESS / ((float)session.sharedData.tempoMs[bar] / 4.0);
	view.beatVizRampStart = session.sharedData.tempoMs[bar] / 4;
	if (view.beatAnimate) {
		view.beatViz = true;
	}
	// notify all score parts of the time stamp and the beat visualization counter
	OscMessage m;
	m.setAddress("/beatinfo");
	m.addFloatArg(view.beatVizStepsPerMs);
	m.addIntArg((int)view.beatVizRampStart);
	session.sendToParts(m, false);
	m.clear();
	m.setAddress("/beat");
	m.addIntArg(session.sharedData.beatCounter);
	session.sendToParts(m, false);
	m.clear();
}

/******************** debugging functions *********************/

/************** string/char state query functions *************/

/*************** index/char location functions ***************/

/******************** tokenizing functions *********************/

/*************** token balance ckeck functions ****************/

/******************* list handling functions ******************/

/******************** new bar/loop functions ******************/

/*************** API type generating functions ****************/

/************* bar/loop index querying functions **************/

/***** functions that contain commands/data for the score *****/
//--------------------------------------------------------------
// the methods of \score that concern only what is shown (see FrontendHost::scoreViewCommand()), with their arguments evaluated by the parser
std::string ofApp::scoreViewCommand(const std::vector<std::string>& commands)
{
	if (commands[0].compare("colors") == 0) {
		int rgbVal = 0;
		int itemCounter = 0;
		int colorCounter = 0;
		for (size_t i = 1; i < commands.size(); i++) {
			if (session.isNumber(commands[i])) {
				rgbVal = stoi(commands[i]);
				if (rgbVal < 0 || rgbVal > 255) {
					return "RGB values must be between 0 and 255";
				}
				switch (itemCounter) {
					case 0:
						switch (colorCounter) {
							case 0:
								scoreBackgroundColor.r = rgbVal;
								break;
							case 1:
								scoreBackgroundColor.g = rgbVal;
								break;
							case 2:
								scoreBackgroundColor.b = rgbVal;
								break;
							default:
								break;
						}
						break;
					case 1:
						for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
							it->second.displayAs<ScoreView>().setStaffColor(colorCounter, rgbVal);
							it->second.displayAs<ScoreView>().setNotesColor(colorCounter, rgbVal);
						}
						break;
					case 2:
						for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
							it->second.displayAs<ScoreView>().setActiveNotesColor(colorCounter, rgbVal);
						}
						break;
					case 3:
						switch (colorCounter) {
							case 0:
								beatColor.r = rgbVal;
								break;
							case 1:
								beatColor.g = rgbVal;
								break;
							case 2:
								beatColor.b = rgbVal;
								break;
							default:
								break;
						}
						break;
					default:
						break;
				}
				colorCounter++;
				if (colorCounter >= 3) {
					colorCounter = 0;
					itemCounter++;
				}
			}
			else {
				if (commands[i].compare("invert") == 0) {
					scoreBackgroundColor.r = 255 - scoreBackgroundColor.r;
					scoreBackgroundColor.g = 255 - scoreBackgroundColor.g;
					scoreBackgroundColor.b = 255 - scoreBackgroundColor.b;
					beatColor.r = 255 - beatColor.r;
					beatColor.g = 255 - beatColor.g;
					beatColor.b = 255 - beatColor.b;
					for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
						it->second.displayAs<ScoreView>().invertColor();
					}
				}
				else if (commands[i].compare("reset") == 0){
					scoreBackgroundColor = ofColor::white;
					beatColor = ofColor::paleGreen;
					for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
						it->second.displayAs<ScoreView>().setStaffColor(Colors::black);
						it->second.displayAs<ScoreView>().setNotesColor(Colors::black);
						it->second.displayAs<ScoreView>().setActiveNotesColor(Colors::red);
					}
				}
				else {
					Color color;
					if (session.colorNameMap.find(commands[i]) != session.colorNameMap.end()) {
						color = session.colorNameMap[commands[i]];
						switch (itemCounter) {
							case 0:
								scoreBackgroundColor = toOf(color);
								break;
							case 1:
								for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
									it->second.displayAs<ScoreView>().setStaffColor(color);
									it->second.displayAs<ScoreView>().setNotesColor(color);
								}
								break;
							case 2:
								for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
									it->second.displayAs<ScoreView>().setActiveNotesColor(color);
								}
								break;
							case 3:
								beatColor = toOf(color);
								break;
							default:
								break;
						}
						itemCounter++;
					}
					else {
						return commands[i] + ": unknown color";
					}
				}
			}
		}
	}

	else if (commands[0].compare("show") == 0) {
		if (commands.size() > 1) {
			// \score.show pianoroll takes the lowest and highest keys too (e.g. \score.show pianoroll c1 a5)
			bool pianoRollRange = (commands[1].compare("pianoroll") == 0 && commands.size() == 4);
			if (commands.size() > 2 && !pianoRollRange) {
				if (commands[1].compare("pianoroll") == 0) {
					return "\"pianoroll\" takes no arguments, or the lowest and the highest keys (e.g. c1 a5)";
				}
				return "method \"show\" takes up to one argument, not more";
			}
			if (commands[1].compare("pianoroll") == 0) {
				int lowest = 21, highest = 108;
				if (pianoRollRange) {
					lowest = noteNameToMidi(commands[2]);
					highest = noteNameToMidi(commands[3]);
					if (lowest < 0) return commands[2] + ": not a note name with an octave (e.g. c1 or a5)";
					if (highest < 0) return commands[3] + ": not a note name with an octave (e.g. c1 or a5)";
					if (highest <= lowest) return "the highest key of the piano roll must be higher than its lowest key";
				}
				setPianoRollRange(lowest, highest);
			}
			if (commands[1].compare("barcount") == 0) {
				showBarCount = true;
			}
			else if (commands[1].compare("tempo") == 0) {
				showTempo = true;
			}
			else if (commands[1].compare("beat") == 0) {
				if (session.sequencer.isThreadRunning()) {
					view.beatAnimate = true;
				}
				else {
					view.setBeatAnimation = true;
				}
			}
			else if (commands[1].compare("notes") == 0) {
				setShowScore(0);
			}
			else if (commands[1].compare("pianoroll") == 0) {
				setShowScore(1);
			}
			else if (commands[1].compare("scope") == 0) {
				setShowScore(2);
			}
			else if (commands[1].compare("seq") == 0 || commands[1].compare("sequencer") == 0) {
				setShowScore(3);
			}
			else {
				return "unknown argument to method \"show\"";
			}
		}
		else {
			setShowScore(0);
		}
	}

	else if (commands[0].compare("hide") == 0) {
		if (commands.size() > 1) {
			if (commands.size() > 2) {
				return "method \"hide\" takes up to one argument, not more";
			}
			if (commands[1].compare("barcount") == 0) {
				showBarCount = false;
			}
			else if (commands[1].compare("tempo") == 0) {
				showTempo = false;
			}
			else if (commands[1].compare("beat") == 0) {
				view.beatAnimate = false;
				view.setBeatAnimation = false;
			}
			else {
				return "unknown argument";
			}
		}
		else {
			setShowScore(-1);
		}
	}

	else if (commands[0].compare("beattype") == 0) {
		if (commands.size() < 2) {
			return "\"beattype\" takes a 1 or a 2 as an argument";
		}
		if (!session.isNumber(commands[1])) {
			return "\"beattype\" takes a 1 or a 2 as an argument";
		}
		int beattype = stoi(commands[1]);
		if (beattype < 1 || beattype > 2) {
			return "unknown beat vizualization type";
		}
		view.beatVizType = beattype;
	}

	else if (commands[0].compare("movex") == 0) {
		if (commands.size() < 2) {
			return "\"movex\" takes a number as an argument";
		}
		if (!session.isNumber(commands[1])) return "\"movex\" takes a number as an argument";
		int numPixels = stoi(commands[1]);
		for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
			it->second.displayAs<ScoreView>().moveScoreX(numPixels);
		}
	}

	else if (commands[0].compare("movey") == 0) {
		if (commands.size() < 2) {
			return "\"movey\" takes a number as an argument";
		}
		if (!session.isNumber(commands[1])) return "\"movey\" takes a number as an argument";
		int numPixels = stoi(commands[1]);
		for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
			it->second.displayAs<ScoreView>().moveScoreY(numPixels);
		}
	}

	else if (commands[0].compare("recenter") == 0) {
		if (commands.size() > 1) {
			return "\"recenter\" takes no arguments";
		}
		for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
			it->second.displayAs<ScoreView>().recenterScore();
		}
	}

	else if (commands[0].compare("numbars") == 0) {
		if (commands.size() < 2) {
			return "\"numbars\" takes a number as an argument";
		}
		if (!session.isNumber(commands[1])) return "\"numbars\" takes a number as an argument";
		int numBarsLocal = stoi(commands[1]);
		if (numBarsLocal < 1) {
			return "number of bars to display must be 1 or greater";
		}
		numBarsToDisplay = numBarsLocal;
		for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
			it->second.displayAs<ScoreView>().setNumBarsToDisplay(numBarsToDisplay);
		}
		// the length of the staves depends on the number of bars displayed, so set it and the note positions again
		setScoreSizes();
	}

	else if (commands[0].compare("update") == 0) {
		if (commands.size() != 2) {
			return "\"update\" takes one argument, \"onlast\" or \"immediately\"";
		}
		if (commands[1].compare("onlast") == 0) {
			scoreChangeOnLastBar = true;
		}
		else if (commands[1].compare("immediately") == 0) {
			scoreChangeOnLastBar = false;
		}
		else {
			return commands[1] + ": unknown argument to \"update\", must be onlast or immediately";
		}
	}

	return "";
}

//--------------------------------------------------------------
void ofApp::setShowScore(int ndx)
{
	if (ndx >= 0) {
		showScore = true;
		scoreType = ndx;
		if (ndx == 0) view.showNotes = true;
		else view.showNotes = false;
	}
	else {
		showScore = false;
		view.showNotes = false;
	}
	// the oscilloscope gets the output of the audio thread only while it is shown (see onAudioOut() and update())
	showScope = (ndx == 2);

	//switch (ndx) {
	//	case 0:
	//		sharedData.showNotes = true;
	//		showPianoRoll = false;
	//		showScope = false;
	//		showScore = true;
	//		break;
	//	case 1:
	//		sharedData.showNotes = false;
	//		showPianoRoll = true;
	//		showScope = false;
	//		showScore = true;
	//		break;
	//	case 2:
	//		sharedData.showNotes = false;
	//		showPianoRoll = false;
	//		showScope = true;
	//		showScore = true;
	//		break;
	//	default:
	//		sharedData.showNotes = false;
	//		showPianoRoll = false;
	//		showScope = false;
	//		showScore = false;
	//		break;
	//}
	workspace.layout();
	editors[session.whichPane].setMaxCharactersPerString();
}

//--------------------------------------------------------------
bool ofApp::isScoreVisible()
{
	return showScore;
}

//--------------------------------------------------------------
void ofApp::changeFontSize(int delta)
{
	fontSize += delta;
	if (fontSize <= 0) fontSize = 2;
	fontLoaded = false;
	setFontSize(true); // set font size and calculate pane coordinates
}

//--------------------------------------------------------------
void ofApp::changeScoreSize(int delta)
{
	view.staffLinesDist += delta;
	if (view.staffLinesDist < 4) view.staffLinesDist = 4;
	view.scoreFontSize = (int)(35.0 * view.staffLinesDist / 10.0);
	instFontSize = view.scoreFontSize / 3.5;
	instFont.load("fonts/LiberationSerif-Regular.ttf", instFontSize);
	setScoreSizes();
}

//--------------------------------------------------------------
WorkspaceHost::ScorePlacement ofApp::scorePlacement()
{
	// at the start of the window: on the left when beside the panes (orientation 0), at the top when above or below them (1)
	bool atStart = (scoreOrientation == 0 ? scoreXOffset == 0 : scoreYOffset == 0);
	return {isScoreVisible(), scoreOrientation, atStart};
}

/********************* instrument functions *******************/

/*********** coordinates/positions/sizes functions ************/
//--------------------------------------------------------------
void ofApp::setFontSize(bool calculateCoords)
{
	if (!fontLoaded) {
		font.load("fonts/DroidSansMono.ttf", fontSize);
		fontLoaded = true;
		oneCharacterWidth = font.stringWidth("a");
		halfCharacterWidth = oneCharacterWidth / 2;
		cursorHeight = font.stringHeight("q");
		float heightDiff = font.stringHeight("l") - font.stringHeight("o");
		cursorHeight += heightDiff;
		// from the baseline of the text to the bottom of its row, which centers the letters in the rows of the editor
		characterOffset = font.stringHeight("j") - font.stringHeight("l");
		characterOffset /= 2;
		maxShellChars = (size_t)(view.screenWidth / oneCharacterWidth);
	}
	for (std::map<int, Editor>::iterator it = editors.begin(); it != editors.end(); ++it) {
		it->second.setFontSize(fontSize, oneCharacterWidth, cursorHeight, characterOffset);
	}
	if (calculateCoords) workspace.layout();
}

//--------------------------------------------------------------
void ofApp::setScoreCoords()
{
	scoreXStartPnt = view.longestInstNameWidth + (view.blankSpace * 1.5);
	float staffLength = scoreBackgroundWidth - view.blankSpace - scoreXStartPnt;
	if (scoreOrientation > 0) staffLength /= std::min(2, numBarsToDisplay);
	view.notesXStartPnt = 0;
	notesLength = staffLength - (view.staffLinesDist * NOTESXOFFSETCOEF);
	for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
		it->second.displayAs<ScoreView>().setStaffCoords(staffLength, view.staffLinesDist);
		it->second.displayAs<ScoreView>().setNoteCoords(notesLength, view.staffLinesDist, view.scoreFontSize);
	}
	// get the note width from the first instrument, as they all have the same size
	if (session.sharedData.instruments.size() > 0) {
		view.noteWidth = session.sharedData.instruments[0].displayAs<ScoreView>().getNoteWidth();
		view.noteHeight = session.sharedData.instruments[0].displayAs<ScoreView>().getNoteHeight();
	}
}

void ofApp::resetScoreYOffset()
{
	// reset the Y offset for the score through swapScorePosition() which deals with it
	// set the existing orientation value and reset the Y coord if it's smaller than 0
	// so that they don't change
	if (scoreYOffset < 0) scoreYOffset = 0;
	// call swapScorePosition() twice to reset the score position without actually swapping its position
	// swapScorePosition() also makes the pane on the other side of the score active (e.g. with two panes side by side)
	// which the two calls can leave on the first pane, so the active pane is kept as it was (e.g. when Esc resets the traceback area)
	int activePane = session.whichPane;
	swapScorePosition(scoreOrientation);
	swapScorePosition(scoreOrientation);
	if (session.whichPane != activePane && editors.find(activePane) != editors.end()) workspace.setActivePane(activePane);
}

//--------------------------------------------------------------
void ofApp::setNotePositions(int barIndex)
{
	for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
		if (!it->second.getCopied(barIndex)) it->second.displayAs<ScoreView>().setNotePositions(barIndex);
	}
	applyMergeLayout(barIndex);
}

//--------------------------------------------------------------
void ofApp::setNotePositions(int barIndex, int numBars)
{
	for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
		if (!it->second.getCopied(barIndex)) it->second.displayAs<ScoreView>().setNotePositions(barIndex, numBars);
	}
	for (int i = 0; i < numBars; i++) applyMergeLayout(barIndex + i);
}

//--------------------------------------------------------------
void ofApp::applyMergeLayout(int bar)
{
	// the notes of the voices of a staff are placed by their time, so notes that sound together have the same X coordinate
	// when their note heads would collide, one of them moves to the right, as in LilyPond by default:
	// - a voice with stems up moves when its note heads are a unison or a second away from those of a voice with stems down
	//   except for a unison of the same note head and dots, where the two note heads are drawn as one
	// - a third (and further) voice with the same stem direction as a voice before it moves when they sound together
	//   (voices 3 and 4 by one note head, 5 and 6 by two, etc.), so that their stems and beams don't run into each other
	//   voices with stems up move to the right, and voices with stems down to the left
	const float sameTime = 0.5;
	for (const auto& group : session.mergeGroups) {
		if (group.size() < 2) continue;
		std::vector<std::vector<Notes::NoteColumn>> columns;
		std::vector<int> dirs;
		for (int inst : group) {
			auto it = session.sharedData.instruments.find(inst);
			if (it == session.sharedData.instruments.end()) return;
			columns.push_back(it->second.displayAs<ScoreView>().getNoteColumns(bar));
			dirs.push_back(it->second.displayAs<ScoreView>().getForcedStemDir());
		}
		float noteWidth = session.sharedData.instruments[group.front()].displayAs<ScoreView>().getScoreNoteWidth();
		float halfStaffDist = view.staffLinesDist / 2.0f;
		// the column of a voice that starts at the X coordinate, or -1
		auto columnAt = [&](size_t voice, float x) {
			for (size_t k = 0; k < columns[voice].size(); k++) {
				if (!columns[voice][k].rest && std::abs(columns[voice][k].x - x) < sameTime) return (int)k;
			}
			return -1;
		};
		for (size_t v = 0; v < group.size(); v++) {
			for (size_t i = 0; i < columns[v].size(); i++) {
				const Notes::NoteColumn& column = columns[v][i];
				if (column.rest) continue;
				float shift = 0;
				if (v >= 2) {
					for (size_t u = 0; u < v; u++) {
						if (dirs[u] == dirs[v] && columnAt(u, column.x) >= 0) shift = noteWidth * (float)(v / 2) * (float)dirs[v];
					}
				}
				if (dirs[v] == 1) {
					for (size_t u = 0; u < group.size(); u++) {
						if (dirs[u] != -1) continue;
						int k = columnAt(u, column.x);
						if (k < 0) continue;
						const Notes::NoteColumn& other = columns[u][k];
						float closest = FLT_MAX;
						for (float y1 : column.headsY) {
							for (float y2 : other.headsY) closest = std::min(closest, std::abs(y1 - y2));
						}
						if (closest > halfStaffDist + sameTime) continue;
						bool sharedHead = (column.headsY.size() == 1 && other.headsY.size() == 1 && closest < sameTime &&
								column.headType == other.headType && column.dots == other.dots);
						if (!sharedHead && shift < noteWidth) shift = noteWidth;
					}
				}
				if (shift != 0) session.sharedData.instruments[group[v]].displayAs<ScoreView>().shiftNoteColumn(bar, (int)i, shift);
			}
		}
	}
}

//--------------------------------------------------------------
void ofApp::calculateStaffPositions(int bar, bool windowChanged)
{
	// the staves are drawn one below the other, each one (allStaffDiffs + staffLinesDist * 2) pixels below the previous one
	// (see draw()), so allStaffDiffs determines the gap between them
	// the Y coordinates of the notes of each bar are relative to the top line of its staff
	// with minimum Y coordinates (stems, beams, notes above the staff, etc.) of 0 or less
	// and maximum Y coordinates (notes below the staff, dynamics, etc.) of (staffLinesDist * 4) or more
	// two neighbouring staves don't overlap if the upper one doesn't reach down further than the lower one reaches up
	// so the gap is set by (max Y of the upper staff - min Y of the lower staff), not by the height of each staff
	// all the bars are checked, and not only the given one, as several bars are displayed side by side
	// and the spacing must be enough for all of them, without changing every time a bar is displayed
	// the arguments are kept for compatibility, as the calculation is the same for a new bar or a window change
	(void)bar;
	(void)windowChanged;
	// instruments merged into the staff of another one (see \merge) have no staff of their own
	// so only the staves are counted, and each staff reaches as far as all its voices
	size_t numInsts = 0;
	for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
		if (session.mergeHostOf(it->first) == it->first) numInsts++;
	}
	if (numInsts == 0) return;
	float requiredDiff = 0;
	// how far the first staff reaches up, and how far the last one reaches down, in any bar
	float firstMinPosition = 0;
	float lastMaxPosition = view.staffLinesDist * 4;
	for (auto barIt = session.sharedData.barsIndexes.begin(); barIt != session.sharedData.barsIndexes.end(); ++barIt) {
		int barNdx = barIt->second;
		float prevMaxPosition = 0;
		bool firstInst = true;
		for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
			if (session.mergeHostOf(it->first) != it->first) continue;
			float maxPosition = it->second.displayAs<ScoreView>().getMaxYPos(barNdx);
			float minPosition = it->second.displayAs<ScoreView>().getMinYPos(barNdx);
			for (auto voiceIt = session.sharedData.instruments.begin(); voiceIt != session.sharedData.instruments.end(); ++voiceIt) {
				if (voiceIt->first == it->first || session.mergeHostOf(voiceIt->first) != it->first) continue;
				maxPosition = std::max(maxPosition, voiceIt->second.displayAs<ScoreView>().getMaxYPos(barNdx));
				minPosition = std::min(minPosition, voiceIt->second.displayAs<ScoreView>().getMinYPos(barNdx));
			}
			if (firstInst) {
				firstMinPosition = std::min(firstMinPosition, minPosition);
				// with one staff only, there are no neighbours, so we keep the height of the staff
				if (numInsts == 1) requiredDiff = std::max(requiredDiff, maxPosition - minPosition);
			}
			else {
				requiredDiff = std::max(requiredDiff, prevMaxPosition - minPosition);
			}
			prevMaxPosition = maxPosition;
			firstInst = false;
		}
		lastMaxPosition = std::max(lastMaxPosition, prevMaxPosition);
	}
	// the clefs reach about two staff spaces above the top line and below the bottom line of the staff
	// so the first and last staves include them, and the gap between two staves must be large enough
	// for the clefs of neighbouring staves not to touch, even when the bars are empty
	firstMinPosition = std::min(firstMinPosition, -(view.staffLinesDist * 2));
	lastMaxPosition = std::max(lastMaxPosition, view.staffLinesDist * 6);
	// the staves must not overlap each other, even if the score doesn't fit
	// the gap between two staves is (allStaffDiffs - staffLinesDist * 2), so this leaves four staff spaces between them
	float minDiff = view.staffLinesDist * 6;
	requiredDiff = std::max(requiredDiff, minDiff);
	// the score is displayed in the whole height (vertical view), or in the upper half (horizontal view)
	float fullHeight = workspace.belowBase - cursorHeight;
	// in horizontal view, the height of the score background depends on the pane configuration
	// (see swapScorePosition()), so we use that, if the score is currently displayed horizontally
	float horizontalHeight = fullHeight / 2;
	if (scoreOrientation == 1 && scoreBackgroundHeight > 0) horizontalHeight = std::min(horizontalHeight, scoreBackgroundHeight);
	float availableHeight = (scoreOrientation == 1 ? horizontalHeight : fullHeight);
	// the total height of the score is the distance from the top staff to the bottom staff
	// plus what sticks out above the first and below the last staff
	// so the most the distance between the staves can be for the score to fit is
	float fittingDiff = requiredDiff;
	if (numInsts > 1) {
		fittingDiff = ((availableHeight - (lastMaxPosition - firstMinPosition)) / (float)(numInsts - 1)) - (view.staffLinesDist * 2);
	}
	view.allStaffDiffs = std::max(minDiff, std::min(requiredDiff, fittingDiff));
	float staffDistance = view.allStaffDiffs + (view.staffLinesDist * 2);
	float totalHeight = (staffDistance * (float)(numInsts - 1)) + (lastMaxPosition - firstMinPosition);
	// index 0 is for the vertical view, 1 for the horizontal view (upper half of the window), 2 is the same as 0
	for (int i = 0; i < 2; i++) {
		float height = (i == 0 ? fullHeight : horizontalHeight);
		if (totalHeight < height) {
			// center the score, with whatever sticks out above the first staff included
			view.yStartPnts[i] = ((height - totalHeight) / 2.0) - firstMinPosition;
		}
		else {
			// if the full score exceeds the screen just place it 10 pixels below the top part of the window
			view.yStartPnts[i] = 10 - firstMinPosition;
		}
	}
	view.yStartPnts[2] = view.yStartPnts[0];
}

//--------------------------------------------------------------
void ofApp::swapScorePosition(int orientation)
{
	for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
		it->second.displayAs<ScoreView>().setScoreOrientation(orientation);
	}
	// use a boolean to determine whether before exiting the function we will calculate coords for the score
	bool setCoords = scoreOrientation != orientation ? true : false;
	// update the scoreOrientation variable before anything else because it is used in setPaneCoords()
	// which in turn is called when viewing the score in horizontal view, before calculating the score's size and coords
	scoreOrientation = orientation;
	switch (orientation) {
		case 0:
			scoreYOffset = 0;
			// if we have one pane only or two panes in horizontal split
			scoreBackgroundWidth = view.screenWidth / 2;
			if ((numPanes.size() == 1 && numPanes[0] == 1) || (paneSplitOrientation == 0 && (numPanes.size() == 2 && (numPanes[0] == 1 && numPanes[1] == 1)))) {
				// we set the space between the top of the window the beginning of the traceback as the height of the score
				// in this pane configuration we don't mind which pane index we'll use, so the first is the safest
				// we add one to the number of editor lines to include the pane separating line
				scoreBackgroundHeight = workspace.belowBase;
			}
			else {
				scoreBackgroundHeight = workspace.belowBase - cursorHeight;
			}
			view.pianoRollKeysWidth = scoreBackgroundWidth / KEYSWIDTHCOEFF;
			view.pianoRollKeysHeight = scoreBackgroundHeight / view.pianoRollLength;
			if (scoreXOffset == 0) scoreXOffset = view.screenWidth / 2;
			else scoreXOffset = 0;
			// check if the editor is split in panes and change the active pane accordingly
			if (paneSplitOrientation == 1 && numPanes.size() == 2) {
				if (scoreXOffset == 0 && session.whichPane < numPanes[0]) {
					workspace.setActivePane(numPanes[0]);
				}
				else if (scoreXOffset > 0 && session.whichPane >= numPanes[0]) {
					workspace.setActivePane(0);
				}
			}
			// if we have one pane only or two panes spit horizontally, swap its position with the score
			else if (paneSplitOrientation == 0 && numPanes.size() < 3 &&
					(numPanes.size() == 1 || (numPanes[0] == 1 && numPanes[1] == 1))) {
				if (scoreXOffset == 0) {
					editors[0].setFrameXOffset(view.screenWidth / 2);
					if (numPanes.size() > 1) editors[1].setFrameXOffset(view.screenWidth / 2);
				}
				else {
					editors[0].setFrameXOffset(0);
					if (numPanes.size() > 1) editors[1].setFrameXOffset(0);
				}
			}
			// if we change from horizontal to vertical view, set the coordinates of the panes
			if (setCoords) workspace.layout();
			break;
		case 1:
			scoreXOffset = 0;
			scoreBackgroundWidth = view.screenWidth;
			// if we have one pane only or two panes in vertical split
			if ((numPanes.size() == 1 && numPanes[0] == 1) || (paneSplitOrientation == 1 && (numPanes.size() == 2 && (numPanes[0] == 1 && numPanes[1] == 1)))) {
				// we first calculate the panes coordinates because in case we have one pane only
				// or two panes split vertically, we want to reduce their number of lines
				// so the horizontal score can nicely fit in the window without obscuring the panes
				workspace.layout();
				// then we set the space between the end of the pane(s) and the beginning of the traceback as the height of the score
				// in this pane configuration we don't mind which pane index we'll use, so the first is the safest
				// getMaxNumLines() returns the number of lines minus 1 for the pane separator
				// but in this case we need this additional line for the score height so we add it
				int numLines = editors[0].getMaxNumLines() + 1;
				scoreBackgroundHeight = workspace.belowBase - (numLines * cursorHeight);
				if (scoreYOffset == 0) {
					scoreYOffset = numLines * cursorHeight;
				}
				else {
					scoreYOffset = 0;
					editors[0].setFrameYOffset(scoreBackgroundHeight);
					if (editors.size() > 1) editors[1].setFrameYOffset(scoreBackgroundHeight);
				}
			}
			// with horizontal split and two rows
			else if (paneSplitOrientation == 0 && numPanes.size() == 2) {
				// get the number of lines of the first column in the last row
				int paneNdx = 0;
				// first determine the index of the pane we need to get the correct Y coordinates from
				// with a 0 pane split orientation, we use the last row
				auto lastRowPane = std::prev(numPanes.end());
				// to get the correct pane index though, we need to accumulate all panes up to that point
				for (std::map<int, int>::iterator it = numPanes.begin(); it != lastRowPane; ++it) {
					paneNdx += it->second;
				}
				scoreBackgroundHeight = editors[paneNdx].getMaxNumLines() * cursorHeight;
				if (scoreYOffset == 0) scoreYOffset = editors[paneNdx].getFrameYOffset();
				else scoreYOffset = 0;
				// if we change from horizontal to vertical view, set the coordinates of the panes
				if (setCoords) workspace.layout();
				// the score covers one row of panes, so the active pane is one of the other row
				if (scoreYOffset == 0 && session.whichPane < numPanes[0]) {
					workspace.setActivePane(numPanes[0]);
				}
				else if (scoreYOffset > 0 && session.whichPane >= numPanes[0]) {
					workspace.setActivePane(0);
				}
			}
			else {
				scoreBackgroundHeight = (workspace.belowBase / 2) - cursorHeight;
				// in horizontal view of the score, the height of the score background is rounded half the editor lines
				// this is because the number of lines of an editor is always odd, so that the line with the file name
				// and pane index is also included, which makes the total number of lines even
				// when we view the score horizontally, the background should match an exact number of lines
				// and since the number of total lines is odd, we make this background one less
				// e.g. for 37 lines, the score background will be 18 lines, not 19, or 18.5
				// but when viewed on the lower side, it should touch the white line of the editor
				// hence the scoreYOffset is not the same as the scoreBackgroundHeith
				// as is the case with scoreXOffset and scoreBarckgroundWidth, in the case above
				if (scoreYOffset == 0) scoreYOffset = (workspace.belowBase / 2);
				else scoreYOffset = 0;
			}
			view.pianoRollKeysWidth = scoreBackgroundWidth / view.pianoRollLength;
			view.pianoRollKeysHeight = scoreBackgroundHeight / KEYSWIDTHCOEFF;
			break;
		case 2:
			scoreXOffset = 0;
			scoreYOffset = 0;
			scoreBackgroundWidth = view.screenWidth;
			scoreBackgroundHeight = workspace.belowBase - cursorHeight;
			view.pianoRollKeysWidth = scoreBackgroundWidth / KEYSWIDTHCOEFF;
			view.pianoRollKeysHeight = scoreBackgroundHeight / view.pianoRollLength;
		default:
			break;
	}
	if (setCoords) {
		// the length of the staves changes with the orientation, so the notes must be positioned again
		// and the available height changes too, so the distance between the staves must be calculated again
		setScoreSizes();
		calculateStaffPositions(session.getLastBarIndex(), true);
	}
}

//--------------------------------------------------------------
void ofApp::setScoreSizes()
{
	// for a 35 font size the staff lines distance is 10
	view.longestInstNameWidth = 0;
	for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
		it->second.displayAs<ScoreView>().setNotesFontSize(view.scoreFontSize, view.staffLinesDist);
		size_t nameWidth = instFont.stringWidth(it->second.getName());
		instNameWidths[it->first] = nameWidth;
		if ((int)nameWidth > view.longestInstNameWidth) {
			view.longestInstNameWidth = nameWidth;
		}
	}
	scoreXStartPnt = view.longestInstNameWidth + (view.blankSpace * 1.5);
	setScoreCoords();
	if (session.sharedData.barsIndexes.size() > 0) {
		for (auto it = session.sharedData.barsIndexes.begin(); it != session.sharedData.barsIndexes.end(); ++it) {
			for (auto it2 = session.sharedData.instruments.begin(); it2 != session.sharedData.instruments.end(); ++it2) {
				it2->second.setMeter(it->second, session.sharedData.numerator[it->second],
						session.sharedData.denominator[it->second], session.sharedData.numBeats[it->second]);
				it2->second.displayAs<ScoreView>().setNotePositions(it->second);
			}
			applyMergeLayout(it->second);
		}
	}
}

//--------------------------------------------------------------
void ofApp::setScoreNotes(int barIndex)
{
	for (auto it = session.sharedData.instruments.begin(); it != session.sharedData.instruments.end(); ++it) {
		it->second.displayAs<ScoreView>().setScoreNotes(it->second, barIndex, session.sharedData.numerator[barIndex],
								 session.sharedData.denominator[barIndex], session.sharedData.numBeats[barIndex],
								 session.sharedData.BPMTempi[barIndex], session.sharedData.tempoBaseForScore[barIndex],
								 session.sharedData.BPMDisplayHasDot[barIndex], session.sharedData.BPMMultiplier[barIndex]);
	}
}

//--------------------------------------------------------------
void ofApp::exit()
{
	// should print a warning message first
	session.sequencer.stopNow();
	// the sequencer's thread calls the app (e.g. to highlight the notes that play), and it is held by Session
	// which is destroyed after the members of the app, so the thread ends here
	session.sequencer.waitForThread();
	OscMessage m;
	m.setAddress("/exit");
	m.addIntArg(1);
	session.sendToParts(m, false);
	m.clear();
	session.midi->closeAll();
	if (session.serialPortOpen) {
		session.serial->close();
	}
	exiting = true;
	// the audio stream stops before the app is destroyed, as its callback uses members of the app (e.g. sharedData)
	// and the stream is held by Session, which is destroyed after them
	if (session.soundStreamSet) session.audioStream->close();
	ofExit();
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y)
{
	// store mouse's X and Y coords coontinuously
	mouseX = x;
	mouseY = y;
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
	workspace.layout();
	if (scoreOrientation == 0) {
		scoreBackgroundWidth = view.screenWidth / 2;
		scoreBackgroundHeight = workspace.belowBase - cursorHeight;
		view.pianoRollKeysWidth = scoreBackgroundWidth / KEYSWIDTHCOEFF;
		view.pianoRollKeysHeight = scoreBackgroundHeight / view.pianoRollLength;
		if (scoreXOffset > 0) scoreXOffset = view.screenWidth / 2;
	}
	else {
		scoreBackgroundWidth = view.screenWidth;
		scoreBackgroundHeight = (workspace.belowBase / 2) - cursorHeight;
		view.pianoRollKeysWidth = scoreBackgroundWidth / view.pianoRollLength;
		view.pianoRollKeysHeight = scoreBackgroundHeight / KEYSWIDTHCOEFF;
	}
	resetScoreYOffset();
	setScoreSizes();
	calculateStaffPositions(session.getLastBarIndex(), true);
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h)
{
	// hold the previous middle of screen value so the score is also properly updated
	int prevMiddleOfScreenX = view.middleOfScreenX;
	view.screenWidth = w;
	view.screenHeight = h;
	view.middleOfScreenX = view.screenWidth / 2;
	view.middleOfScreenY = view.screenHeight / 2;
	// once the screen coordinates have been updated, update the score coordinates too
	if (view.staffXOffset == prevMiddleOfScreenX) {
		view.staffXOffset = 0;
		view.staffWidth = view.screenWidth / 2;
	}
	else {
		view.staffXOffset = view.middleOfScreenX;
		view.staffWidth = view.screenWidth;
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

/********************** audio functions ***********************/

//--------------------------------------------------------------
void ofApp::onAudioIn(const float* in, int numFrames, int numChannels)
{
#ifdef USEPYO
	if (session.pyoSet && !exiting) {
		// send audio samples to pyo
		session.sharedData.pyo.fillin(const_cast<float*>(in));
	}
#endif
}

//--------------------------------------------------------------
void ofApp::onAudioOut(float* out, int numFrames, int numChannels, int sampleRate)
{
	bool pyoProcessed = false;
#ifdef USEPYO
	if (session.pyoSet && !exiting) {
		// process and get new audio samples from pyo
		session.sharedData.pyo.process(out);
		if (showScope) {
			std::lock_guard<std::mutex> lock(scopeMutex);
			scopeSamples.assign(out, out + (size_t)numFrames * numChannels);
			scopeChannels = numChannels;
		}
		pyoProcessed = true;
	}
#endif
	// without pyo, the stream plays the click track only
	if (!pyoProcessed) std::fill(out, out + (size_t)numFrames * numChannels, 0.0f);
	// the click track is added to the output (see \clicktrack)
	if (!exiting) session.sharedData.clickTrack.process(out, numFrames, numChannels, sampleRate);
}

/*********** front-end functions moved from the parser (see Session in session.h) ***********/
//--------------------------------------------------------------
void ofApp::instrumentCreated(int instNdx)
{
	Instrument& inst = session.sharedData.instruments[instNdx];
	// what is drawn of the instrument (its staff and its notes)
	inst.attachDisplay(std::make_shared<ScoreView>());
	// the width of its name, for the space of the names left of the score
	int thisInstNameWidth = instFont.stringWidth(inst.getName());
	instNameWidths[instNdx] = thisInstNameWidth;
	if (thisInstNameWidth > view.longestInstNameWidth) {
		view.longestInstNameWidth = thisInstNameWidth;
	}
	inst.displayAs<ScoreView>().setNotesFontSize(view.scoreFontSize, view.staffLinesDist);
	// a new instrument must be displayed like the existing ones, in case the score is not in its default state
	inst.displayAs<ScoreView>().setScoreOrientation(scoreOrientation);
	inst.displayAs<ScoreView>().setNumBarsToDisplay(numBarsToDisplay);
	setScoreCoords();
}

//--------------------------------------------------------------
void ofApp::instrumentRemoved(int instNdx)
{
	// the instruments after the removed one get the indexes before (see Session::removeInstrument())
	auto newIndex = [instNdx](int ndx) { return (ndx > instNdx ? ndx - 1 : ndx); };
	// the widths of the names, for the space of the names left of the score
	instNameWidths.clear();
	view.longestInstNameWidth = 0;
	for (auto& inst : session.sharedData.instruments) {
		int width = instFont.stringWidth(inst.second.getName());
		instNameWidths[inst.first] = width;
		view.longestInstNameWidth = std::max(view.longestInstNameWidth, width);
	}
	// the note highlights and the lines connected to the bars of the instruments
	workspace.instrumentRemoved(instNdx);
	setScoreCoords();
}

//--------------------------------------------------------------
void ofApp::instrumentsCleared()
{
	workspace.instrumentsCleared();
}

//--------------------------------------------------------------
void ofApp::layoutInstrumentBar(int instNdx, int bar)
{
	SharedData& data = session.sharedData;
	Instrument& inst = data.instruments[instNdx];
	inst.displayAs<ScoreView>().setScoreNotes(inst, bar, data.numerator[bar], data.denominator[bar], data.numBeats[bar],
			data.BPMTempi[bar], data.tempoBaseForScore[bar], data.BPMDisplayHasDot[bar], data.BPMMultiplier[bar]);
	inst.displayAs<ScoreView>().setNotePositions(bar);
}

//--------------------------------------------------------------
bool ofApp::scoreShown()
{
	return view.showNotes;
}

//--------------------------------------------------------------
void ofApp::sequencerStarted(float beatStepsPerMs, uint64_t beatRampStart)
{
	// a beat visualization turned on while the sequencer was idle starts with it
	if (view.showNotes && view.setBeatAnimation) {
		view.beatViz = true;
		view.beatAnimate = true;
		view.setBeatAnimation = false;
	}
	view.beatVizStepsPerMs = beatStepsPerMs;
	view.beatVizRampStart = beatRampStart;
}

//--------------------------------------------------------------
void ofApp::beat(uint64_t timeStamp, float stepsPerMs, uint64_t rampStart)
{
	view.beatVizTimeStamp = timeStamp;
	view.beatVizStepsPerMs = stepsPerMs;
	view.beatVizRampStart = rampStart;
	if (view.beatAnimate) view.beatViz = true;
}

//--------------------------------------------------------------
void ofApp::barStarted(uint64_t timeStamp)
{
	view.pianoRollTimeStamp = timeStamp;
}

//--------------------------------------------------------------
void ofApp::loopChanged()
{
	// the previous number of bars displayed and the previous position, to properly display single bars in horizontal view
	view.prevNumBars = view.numBars;
	view.prevPosition = view.thisPosition;
}

//--------------------------------------------------------------
void ofApp::setPythonTyping(bool charByChar)
{
	view.typePyStrCharByChar = charByChar;
}

//--------------------------------------------------------------
int ofApp::scoreBesidePanesWidth()
{
	// with the score shown beside the panes (vertical view), the panes have the half of the window it doesn't take
	if (isScoreVisible() && scoreOrientation == 0) return view.middleOfScreenX;
	return -1;
}

//--------------------------------------------------------------
std::string ofApp::chooseFileToSave(const std::string& defaultName, const std::string& message)
{
	ofFileDialogResult result = ofSystemSaveDialog(defaultName, message);
	return (result.bSuccess ? result.filePath : std::string());
}

//--------------------------------------------------------------
std::string ofApp::chooseFileToOpen(const std::string& message)
{
	ofFileDialogResult result = ofSystemLoadDialog(message);
	return (result.bSuccess ? result.filePath : std::string());
}
