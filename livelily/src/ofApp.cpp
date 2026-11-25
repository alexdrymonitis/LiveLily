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
	// the views draw with the canvas of the app (see canvas.h), which is set before any instrument is created
	Canvas::current = &canvas;
	// relative paths (e.g. of a SoundFont) start from the user's folder of LiveLily, and \help looks in the search paths (see paths.h)
	// in test mode only in the resources of LiveLily, so that files of the user don't change the results of the tests
	std::filesystem::path resourcesPath = ofToDataPath("", true);
	session.dataDirectory = Paths::userFolder().string();
	session.searchPaths = (testFile.empty() ? Paths::searchPaths(resourcesPath) : std::vector<std::filesystem::path>{resourcesPath});
	brightnessCoeff = 0.85;
	shellStrColor = Colors::white;
	ofBackground(toOf(atBrightness(backgroundColor, brightnessCoeff)));

	// give a random seed to srand(), and a fixed one in test mode, so that \rand gives the same values every time (see testmode.cpp)
	srand(testFile.empty() ? time(NULL) : 1);

	unsigned int framerate = 60;

	// in test mode, the MIDI ports are made-up ones, so that the dumps don't depend on the machine (see test_midi.cpp)
	// set before the session is set up, as the sequencer takes the MIDI output then
	if (!testFile.empty()) session.midi = makeTestMidiOutput();
	// the defaults of the session and the tables of the names of its languages (see session_setup.cpp)
	session.setup(foregroundColor);

	notesID = 0;
	fullScreen = false;
	isWindowResized = false;
	windowResizeTimeStamp = 0;

	ofSetFrameRate(framerate);
	lineWidth = 2;
	ofSetLineWidth(lineWidth);
	canvas.lineWidth = lineWidth;

	ofSetWindowTitle("LiveLily");

	views.setWindow(ofGetWindowWidth(), ofGetWindowHeight());

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

	// the score, the beat, and the piano roll, once the panes are laid out (see Views::setup())
	views.setup();

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
							views.sendBeatVizInfo(bar);
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
				scopeWaveforms[j].push_back({x, y});
			}
		}
	}
}

/********************* drawing functions ***********************/
//--------------------------------------------------------------
void ofApp::draw()
{
	// the brightness the views draw at, which the keys change (see WorkspaceHost::changeBrightness())
	canvas.brightness = brightnessCoeff;
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
	ofSetColor(toOf(atBrightness(foregroundColor, brightnessCoeff)));
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
	//	views.drawScore();
	//}
	//else if (showPianoRoll) {
	//	views.drawPianoRoll();
	//}
	//else if (showScope) {
	//	views.drawScope();
	//}
	if (showScore) {
		switch (scoreType) {
			case 0:
				views.drawScore();
				break;
			case 1:
				views.drawPianoRoll();
				break;
			case 2:
				views.drawScope();
				break;
			case 3:
				views.drawSequencer();
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
	ofSetColor(toOf(atBrightness(foregroundColor, brightnessCoeff)));
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
/*************** arithmetic operations functions **************/

/********************* clearing functions *********************/

/************ command typing/executing functions **************/
/****************** miscellaneous functions ********************/

/****************** OSC to parts functions ********************/

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
void ofApp::changeFontSize(int delta)
{
	fontSize += delta;
	if (fontSize <= 0) fontSize = 2;
	fontLoaded = false;
	setFontSize(true); // set font size and calculate pane coordinates
}

/********************* instrument functions *******************/

/*********** coordinates/positions/sizes functions ************/
//--------------------------------------------------------------
void ofApp::setFontSize(bool calculateCoords)
{
	if (!fontLoaded) {
		font.load("fonts/DroidSansMono.ttf", fontSize);
		// the same font for the labels of the views (see views.h)
		views.labelFont = canvas.loadFont("fonts/DroidSansMono.ttf", fontSize);
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
	views.resetScoreYOffset();
	views.setScoreSizes();
	views.calculateStaffPositions(session.getLastBarIndex(), true);
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
void ofApp::instrumentsCleared()
{
	workspace.instrumentsCleared();
}

//--------------------------------------------------------------
void ofApp::setPythonTyping(bool charByChar)
{
	view.typePyStrCharByChar = charByChar;
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
