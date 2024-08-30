#include "ofApp.h"
#include <string.h>
#include <sstream>

//--------------------------------------------------------------
void ofApp::setup()
{
	backgroundColor = ofColor::black;
	ofBackground(backgroundColor);
	errorCounter = 0;
	unsigned int framerate = 60;
	
	brightnessCoeff = 0.85;
	beatBrightnessCoeff = 1.0;
	brightness = (int)(255 * brightnessCoeff);
	notesID = 0;
	fullscreen = false;
	finishState = false;
	isWindowResized = false;
	windowResizeTimeStamp = 0;
	
	ofSetFrameRate(framerate);
	lineWidth = 2;
	ofSetLineWidth(lineWidth);
	
	ofSetWindowTitle("LiveLily Part");
	
	oscReceiver.setup(OSCPORT);
	
	screenWidth = ofGetWindowWidth();
	screenHeight = ofGetWindowHeight();
	middleOfScreenX = screenWidth / 2;
	middleOfScreenY = screenHeight / 2;

	numInstruments = 0;
	numBarsToDisplay = 4;
	
	lineWidth = 2;
	
	ofTrueTypeFont::setGlobalDpi(72);
	
	numerator[0] = 4;
	denominator[0] = 4;
	numBeats[0] = 4;
	tempNumBeats = 4;
	
	loopIndex = 0;
	tempBarLoopIndex = 0;
	thisLoopIndex = 0;
	beatCounter = 0; // this is controlled by the sequencer
	
	longestInstNameWidth = 0;
	staffLinesDist = 10.0;
	scoreFontSize = 35;
	instFontSize = scoreFontSize/3.5;
	instFont.load("Monaco.ttf", instFontSize);
	countdownFont.load("Monaco.ttf", 50);
	notesXStartPnt = 0;
	blankSpace = 10;
	animate = false;
	setAnimation = false;
	beatAnimate = true;
	setBeatAnimation = false;
	beatViz = false;
	beatTypeCommand = false;
	beatVizCounter = 0;
	beatVizType = 2; // the score part has the 2nd beat visualization type as default
	beatVizDegrade = BEATVIZBRIGHTNESS;
	beatVizTimeStamp = 0;
	beatVizStepsPerMs = (float)BEATVIZBRIGHTNESS / ((float)tempoMs / 4.0);
	beatVizRampStart = tempoMs / 4;
	noteWidth = 0;
	noteHeight = 0;
	allStaffDiffs = 0;
	maxBarFirstStaffAnchor = 0;

	scoreXOffset = 0;
	scoreYOffset = 0;
	mustUpdateScore = false;
	scoreUpdated = false;
	showUpdatePulseColor = false;
	scoreChangeOnLastBar = false;

	showCountdown = false;
	countdown = 0;

	oscSenderSetup = false;

	thisBarIndex = 0;
	barDataError = false;
	//instNdx = 0;
	
	// set the notes chars
	for (int i = 2; i < 9; i++) {
		noteChars[i-2] = char((i%7)+97);
	}
	noteChars[7] = char(114); // "r" for rest
	
	// store the articulation names strings
	string articulSymsLocal[8] = {"none", "marcato", "trill", "tenuto", \
								  "staccatissimo", "accent", "staccato", "portando"};
	for (int i = 0; i < 8; i++) {
		articulSyms[i] = articulSymsLocal[i];
	}
}

//--------------------------------------------------------------
void ofApp::update()
{
    // check for OSC messages
	while (oscReceiver.hasWaitingMessages()) {
		ofxOscMessage m;
		oscReceiver.getNextMessage(m);
		if (m.getAddress() == "/beatinfo") {
			beatVizStepsPerMs = m.getArgAsFloat(0);
			beatVizRampStart = m.getArgAsInt32(1);
			beatVizTimeStamp = ofGetElapsedTimeMillis();
			beatViz = true;
		}
		else if (m.getAddress() == "/beat") {
			beatCounter = m.getArgAsInt32(0);
		}
		else if (m.getAddress() == "/newbar") {
			barName = m.getArgAsString(0);
			storeNewBar(m.getArgAsInt32(1));
		}
		else if (m.getAddress() == "/loopndx") {
			loopIndex = m.getArgAsInt32(0);
		}
		else if (m.getAddress() == "/thisloopndx") {
			thisLoopIndex = m.getArgAsInt32(0);
		}
		else if (m.getAddress() == "/update") {
			tempBarLoopIndex = m.getArgAsInt32(0);
			mustUpdateScore = true;
			showUpdatePulseColor = true;
		}
		else if (m.getAddress() == "/copy") {
			// here we check for the number of data to make sure we received it correctly
			// in case of an error, we can't report back to the main program because we
			// might be missing the bar index, so we just don't report success
			// this way the main program will resend this data
			if (m.getNumArgs() == 3) {
				if (m.getArgType(0) == OFXOSC_TYPE_INT32 && m.getArgType(1) == OFXOSC_TYPE_INT32 && m.getArgType(2) == OFXOSC_TYPE_INT32) {
					int inst = getMappedIndex(m.getArgAsInt32(0));
					int barIndex = m.getArgAsInt32(1);
					int barToCopy = m.getArgAsInt32(2);
					instruments[inst].setCopied(barIndex, true);
					instruments[inst].setCopyNdx(barIndex, barToCopy);
					if (instrumentCounterPerBar.find(barIndex) == instrumentCounterPerBar.end()) {
						instrumentCounterPerBar[barIndex] = 1;
					}
					else {
						instrumentCounterPerBar[barIndex]++;
					}
					ofxOscMessage m;
					m.setAddress("/barok");
					// add instrument index and bar index to the message
					m.addIntArg(instrumentIndexMap[inst]);
					m.addIntArg(barIndex);
					oscSender.sendMessage(m, false);
					m.clear();
					if (instrumentCounterPerBar[barIndex] == numInstruments) {
						setScoreNotes(barIndex);
					}
				}
			}
		}
		else if (m.getAddress() == "/bardata") {
			if (m.getArgType(0) != OFXOSC_TYPE_INT32 || m.getArgType(1) != OFXOSC_TYPE_INT32) {
				barDataError = true;
			}
			else {
				thisBarIndex = m.getArgAsInt32(0);
				numBeats[thisBarIndex] = m.getArgAsInt32(1);
				if (instrumentCounterPerBar.find(thisBarIndex) == instrumentCounterPerBar.end()) {
					instrumentCounterPerBar[thisBarIndex] = 0;
				}
			}
		}
		else if (m.getAddress() == "/meter") {
			if (m.getArgType(0) != OFXOSC_TYPE_INT32 || m.getArgType(1) != OFXOSC_TYPE_INT32) {
				barDataError = true;
			}
			else {
				if (numerator.find(thisBarIndex) == numerator.end()) {
					numerator[thisBarIndex] = m.getArgAsInt32(0);
					denominator[thisBarIndex] = m.getArgAsInt32(1);
				}
			}
		}
		else if (m.getAddress() == "/instndx") {
			if (m.getArgType(0) != OFXOSC_TYPE_INT32) {
				barDataError = true;
			}
			else {
				instNdx = getMappedIndex(m.getArgAsInt32(0));
				instruments[instNdx].setCopied(thisBarIndex, false);
			}
		}
		else if (m.getAddress() == "/notes") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) instruments[instNdx].setNotes(thisBarIndex, v);
		}
		else if (m.getAddress() == "/acc") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) instruments[instNdx].setAccidentals(thisBarIndex, v);
		}
		else if (m.getAddress() == "/natur") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) instruments[instNdx].setNaturalSignsNotWritten(thisBarIndex, v);
		}
		else if (m.getAddress() == "/oct") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) instruments[instNdx].setOctaves(thisBarIndex, v);
		}
		else if (m.getAddress() == "/ott") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) instruments[instNdx].setOttavas(thisBarIndex, v);
		}
		else if (m.getAddress() == "/durs") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) instruments[instNdx].setDurs(thisBarIndex, v);
		}
		else if (m.getAddress() == "/dots") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) instruments[instNdx].setDotIndexes(thisBarIndex, v);
		}
		else if (m.getAddress() == "/gliss") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) instruments[instNdx].setGlissandi(thisBarIndex, v);
		}
		else if (m.getAddress() == "/artic") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) instruments[instNdx].setArticulations(thisBarIndex, v);
		}
		else if (m.getAddress() == "/dyn") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) instruments[instNdx].setDynamics(thisBarIndex, v);
		}
		else if (m.getAddress() == "/dynidx") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) instruments[instNdx].setDynamicsIndexes(thisBarIndex, v);
		}
		else if (m.getAddress() == "/dynramp_start") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) instruments[instNdx].setDynamicsRampStart(thisBarIndex, v);
		}
		else if (m.getAddress() == "/dynramp_end") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) instruments[instNdx].setDynamicsRampEnd(thisBarIndex, v);
		}
		else if (m.getAddress() == "/dynramp_dir") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) instruments[instNdx].setDynamicsRampDir(thisBarIndex, v);
		}
		else if (m.getAddress() == "/slurbegin") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) instruments[instNdx].setSlurBeginnings(thisBarIndex, v);
		}
		else if (m.getAddress() == "/slurend") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) instruments[instNdx].setSlurEndings(thisBarIndex, v);
		}
		else if (m.getAddress() == "/texts") {
			vector<string> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_STRING && i > 0) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsString(i));
				else if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) instruments[instNdx].setTexts(thisBarIndex, v);
		}
		else if (m.getAddress() == "/textidx") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (m.getArgType(i) != OFXOSC_TYPE_INT32) {
					barDataError = true;
					break;
				}
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else numBarData = m.getArgAsInt32(i);
			}
			if ((int)v.size() != numBarData) {
				barDataError = true;
			}
			if (!barDataError) {
				// first check if all vectors have the same size
				bool allSizesAreEqual = instruments[instNdx].checkVecSizesForEquality(thisBarIndex);
				if (!allSizesAreEqual) {
					// add this bar index to the vector of this instrument only if it doesn't already exist
					if (find(barDataErrorsMap[instNdx].begin(), barDataErrorsMap[instNdx].end(), thisBarIndex) == barDataErrorsMap[instNdx].end()) {
						barDataErrorsMap[instNdx].push_back(thisBarIndex);
					}
				}
				else {
					instruments[instNdx].setTextIndexes(thisBarIndex, v);
					instrumentCounterPerBar[thisBarIndex]++;
					ofxOscMessage m;
					m.setAddress("/barok");
					// add instrument index and bar index to the message
					m.addIntArg(instrumentIndexMap[instNdx]);
					m.addIntArg(thisBarIndex);
					oscSender.sendMessage(m, false);
					m.clear();
					if (instrumentCounterPerBar[thisBarIndex] == numInstruments) {
						setScoreNotes(thisBarIndex);
					}
				}
			}
			else {
				// add this bar index to the vector of this instrument only if it doesn't already exist
				if (find(barDataErrorsMap[instNdx].begin(), barDataErrorsMap[instNdx].end(), thisBarIndex) == barDataErrorsMap[instNdx].end()) {
					barDataErrorsMap[instNdx].push_back(thisBarIndex);
				}
				barDataError = false;
			}
		}
		else if (m.getAddress() == "/loop") {
			vector<int> v;
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (i > 0) v.push_back(m.getArgAsInt32(i));
				else oscLoopIndex = m.getArgAsInt32(i);
			}
			loopData[oscLoopIndex] = v;
		}
		else if (m.getAddress() == "/initinst") {
			int ndx = m.getArgAsInt32(0);
			string name = m.getArgAsString(1);
			string hostIP = m.getArgAsString(2);
			int hostPort = m.getArgAsInt32(3);
			initializeInstrument(ndx, "\\" + name, hostIP, hostPort);
		}
		else if (m.getAddress() == "/clef") {
			instruments[getMappedIndex(m.getArgAsInt32(0))].setClef(m.getArgAsInt32(1));
		}
		else if (m.getAddress() == "/rhythm") {
			instruments[getMappedIndex(m.getArgAsInt32(0))].setRhythm(m.getArgAsBool(1));
		}
		else if (m.getAddress() == "/numbars") {
			numBarsToDisplay = m.getArgAsInt32(0);
		}
		else if (m.getAddress() == "/scorechange") {
			scoreChangeOnLastBar = m.getArgAsBool(0);
		}
		else if (m.getAddress() == "/finish") {
			finishState = m.getArgAsBool(0);
		}
		else if (m.getAddress() == "/countdown") {
			showCountdown = true;
			countdown = m.getArgAsInt32(0);
		}
		else if (m.getAddress() == "/nocountdown") {
			showCountdown = false;
		}
		else if (m.getAddress() == "/fullscreen") {
			bool fullscreenBool = m.getArgAsBool(0);
			if (fullscreenBool != fullscreen) {
				ofToggleFullscreen();
				fullscreen = fullscreenBool;
			}
		}
		else if (m.getAddress() == "/beatviztype") {
			beatVizType = m.getArgAsInt32(0);
		}
		else if (m.getAddress() == "/beatbright") {
			beatBrightnessCoeff = m.getArgAsFloat(0);
		}
		else if (m.getAddress() == "/cursor") {
			if (m.getArgAsBool(0)) {
				ofShowCursor();
			}
			else {
				ofHideCursor();
			}
		}
		else if (m.getAddress() == "/exit") {
			ofShowCursor();
			ofExit();
		}
	}

	int counter = 0;
	ofxOscMessage m;
	m.setAddress("/error");
	for (auto it = barDataErrorsMap.begin(); it != barDataErrorsMap.end(); ++it) {
		if (it->second.size() > 0) {
			// add instrument index and bar index to the message
			for (unsigned i = 0; i < it->second.size(); i++) {
				// send the index of the main program, not of this score part
				m.addIntArg(instrumentIndexMap[it->first]);
				m.addIntArg(it->second[i]);
				counter++;
			}
			it->second.clear();
		}
	}
	if (counter > 0) oscSender.sendMessage(m, false);
	m.clear();

	if (isWindowResized) {
		if ((ofGetElapsedTimeMillis() - windowResizeTimeStamp) > WINDOW_RESIZE_GAP) {
			isWindowResized = false;
			resizeWindow();
		}
	}
}

//--------------------------------------------------------------
void ofApp::draw()
{
	ofSetColor(brightness);
	ofDrawRectangle(scoreXOffset, scoreYOffset, screenWidth, screenHeight);
	if (instruments.size() == 0) return;
	//------------- variables for horizontal score view ------------------
	int numBars = min(numBarsToDisplay, (int)loopData[loopIndex].size());
	numBars = max(numBars, 1);
	bool drawLoopStartEnd = false;
	int bar;
	int ndx = ((thisLoopIndex - (thisLoopIndex % numBars)) / numBars) * numBars;
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
	if (instruments.size() > 0) {
		if (beatAnimate) {
			beatPulseMinPosY = instruments.begin()->second.getMinYPos(thisLoopIndex) - noteHeight;
			beatPulseMaxPosY = instruments.rbegin()->second.getMaxYPos(thisLoopIndex) + noteHeight;
			beatPulseSizeY = beatPulseMaxPosY - beatPulseMinPosY;
			if (beatViz) {
				switch (beatVizType) {
					case 1:
						beatPulseStartX = scoreXStartPnt + scoreXOffset;
						beatPulseEndX = scoreXStartPnt + scoreXOffset + instruments.begin()->second.getStaffXLength();
						beatPulseEndX += (instruments.begin()->second.getStaffXLength() * (numBars-1));
						beatPulseEndX -= (instruments.begin()->second.getClefXOffset() * (numBars-1));
						beatPulseEndX -= (instruments.begin()->second.getMeterXOffset() * (numBars-1));
						beatPulsePosX = beatPulseStartX - staffLinesDist;
						beatPulseSizeX = beatPulseEndX + staffLinesDist - beatPulsePosX;
						beatPulseMinPosY += scoreYOffset;
						ofSetColor(ofColor::paleGreen.r*beatBrightnessCoeff,
								   ofColor::paleGreen.g*beatBrightnessCoeff,
								   ofColor::paleGreen.b*beatBrightnessCoeff,
								   beatVizDegrade);
						ofDrawRectangle(beatPulsePosX, beatPulseMinPosY, beatPulseSizeX, beatPulseSizeY);
						ofSetColor(0);
						break;
					default:
						break;
				}
				// regardless of the beattype, we calculate the alpha value here
				if ((ofGetElapsedTimeMillis() - beatVizTimeStamp) >= beatVizRampStart) {
					// calculate how many steps the brightness has to dim, depending on the elapsed time and the steps per millisecond
					int brightnessDegrade = (int)((ofGetElapsedTimeMillis()-(beatVizTimeStamp+beatVizRampStart))*beatVizStepsPerMs);
					beatVizDegrade = BEATVIZBRIGHTNESS - brightnessDegrade;
					if (beatVizDegrade < 0) {
						beatVizDegrade = 0;
						beatViz = false;
						beatVizDegrade = BEATVIZBRIGHTNESS;
					}
				}
			}
		}
		// write the names of the instruments without the backslash and get the longest name
		for (map<string, int>::iterator it = instrumentIndexes.begin(); it != instrumentIndexes.end(); ++it) {
			float xPos = scoreXStartPnt + scoreXOffset - instNameWidths[it->second] - (blankSpace/2);
			float yPos = instruments[it->second].getStaffYAnchor()+(staffLinesDist*2.5) + scoreYOffset;
			instFont.drawString(it->first.substr(1), xPos, yPos);
		}
		// then draw one line at each side of the staffs to connect them
		// we draw the first one here, and the rest inside the for loop below
		float connectingLineX = scoreXStartPnt + scoreXOffset;
		float connectingLineY1 = instruments.begin()->second.getStaffYAnchor() + scoreYOffset;
		float connectingLineY2 = instruments.rbegin()->second.getStaffYAnchor() + scoreYOffset;
		if (numInstruments > 1) {
			ofDrawLine(connectingLineX, connectingLineY1, connectingLineX, connectingLineY2);
		}
		// then we display the score
		float staffOffsetX = scoreXStartPnt + scoreXOffset;
		float notesOffsetX = staffOffsetX;
		notesOffsetX += instruments.begin()->second.getClefXOffset();
		notesOffsetX += instruments.begin()->second.getMeterXOffset();
		notesOffsetX += (staffLinesDist * 2);
		float notesXCoef = (instruments.begin()->second.getStaffXLength() + staffOffsetX - notesOffsetX) / notesXLength;
		for (int i = 0; i < numBars; i++) {
			bool drawClef = false, drawMeter = false, animate = false;
			bool showBar = true;
			if (i == 0) drawClef = drawMeter = true;
			int barNdx = (ndx + i) % (int)loopData[loopIndex].size();
			if (loopData.find(loopIndex) == loopData.end()) return;
			if ((int)loopData[loopIndex].size() <= barNdx) return;
			bar = loopData[loopIndex][barNdx];
			if (barNdx == (int)thisLoopIndex) animate = true;
			// the index below is used in insertNaturalSigns() in Notes() to deterimine if we must
			// insert a natural sign at the beginning of a bar in case the same note appears
			// in a previous bar with an accidental (this concerns only the visible bars, e.g. 4 bars in one row)
			// mainly used in LiveLily Part
			int insertNaturalsNdx = loopIndex;
			// determine which notes all bars but the last must display in case of horizontal view
			// this is so that when the last bar of the displayed line is played, in case there are performers following the score
			// they should be able to look at the right side of the score to see what notes come right after the current playing bar
			// if we are calling a new loop, all bars but the last should display the first bars of the next loop
			int threshold = (int)loopData[loopIndex].size() - numBars + i;
			if (scoreChangeOnLastBar) threshold = (int)loopData[loopIndex].size() - 2;
			if (mustUpdateScore && (int)thisLoopIndex > threshold && i < numBars - 1) {
				if (i >= (int)loopData[tempBarLoopIndex].size()) {
					showBar = false;
				}
				else {
					bar = loopData[tempBarLoopIndex][i];
					insertNaturalsNdx = tempBarLoopIndex;
				}
				scoreUpdated = true;
				showUpdatePulseColor = true;
			}
			else if (mustUpdateScore && scoreUpdated && thisLoopIndex == 0) {
				mustUpdateScore = scoreUpdated = showUpdatePulseColor = false;
			}
			// if we're staying in the same loop, when the currently playing bar is displayed at the right-most staff
			// all the other bars must display the next bars of the loop (if there are any)
			else if (((int)thisLoopIndex % numBars) == numBars - 1 && i < numBars - 1) {
				bar = loopData[loopIndex][(barNdx+numBars)%(int)loopData[loopIndex].size()];
			}
			// draw the rest of the lines that connect the various staffs together horizontally
			if (numInstruments > 1) {
				connectingLineX += instruments.begin()->second.getStaffXLength();
				if (i > 0) {
					if (!drawClef) connectingLineX -= instruments.begin()->second.getClefXOffset();
					if (!drawMeter) connectingLineX -= instruments.begin()->second.getMeterXOffset();
				}
				ofDrawLine(connectingLineX, connectingLineY1, connectingLineX, connectingLineY2);
			}
			// like with the beat visualization, accumulate X offsets for all bars but the first
			if (i > 0) {
				// only the notes need to go back some pixels, in case the clef or meter is not drawn
				// so we use a separate value for the X coordinate
				staffOffsetX += instruments.begin()->second.getStaffXLength();
				notesOffsetX += instruments.begin()->second.getStaffXLength();
				if (i > 1) {
					if (!drawClef) staffOffsetX -= instruments.begin()->second.getClefXOffset();
					if (!drawMeter) staffOffsetX -= instruments.begin()->second.getMeterXOffset();
				}
				if (!drawClef) notesOffsetX -= instruments.begin()->second.getClefXOffset();
				if (!drawMeter) notesOffsetX -= instruments.begin()->second.getMeterXOffset();
			}
			// if we're displaying the beat pulse on each beat instead of a rectangle that encloses the whole score
			// we need to display it here, in case we display the score horizontally, where we need to give
			// an offset to the pulse so it is displayed on top of the currently playing bar
			if (beatAnimate && beatViz && beatVizType == 2) {
				// draw only for the currenlty playing bar, whichever that is in the horizontal line
				if (barNdx == (int)thisLoopIndex) {
					beatPulseStartX = notesOffsetX + (((notesXLength / numerator[getPlayingBarIndex()]) * beatCounter) * notesXCoef);
					if (beatCounter == -1) beatPulseStartX = notesOffsetX - instruments.begin()->second.getClefXOffset();
					noteSize = instruments.begin()->second.getNoteWidth();
					beatPulsePosX =  beatPulseStartX - noteSize;
					beatPulseSizeX = noteSize * 2;
					beatPulseMinPosY += scoreYOffset;
					ofColor color;
					if (finishState || showUpdatePulseColor) {
						if ((int)loopData[loopIndex].size() > numBars) {
							if (barNdx > numBars) {
								if (finishState) color = ofColor::red;
								else color = ofColor::cyan;
								for (auto it = instruments.begin(); it != instruments.end(); ++it) {
									it->second.setScoreEnd(true);
								}
							}
							else {
								color = ofColor::paleGreen;
								for (auto it = instruments.begin(); it != instruments.end(); ++it) {
									it->second.setScoreEnd(false);
								}
							}
						}
						else {
							if (finishState) color = ofColor::red;
							else color = ofColor::cyan;
							for (auto it = instruments.begin(); it != instruments.end(); ++it) {
								it->second.setScoreEnd(true);
							}
						}
					}
					else {
						color = ofColor::paleGreen;
						for (auto it = instruments.begin(); it != instruments.end(); ++it) {
							it->second.setScoreEnd(false);
						}
					}
					ofSetColor(color.r*beatBrightnessCoeff, color.g*beatBrightnessCoeff, color.b*beatBrightnessCoeff, beatVizDegrade);
					ofDrawRectangle(beatPulsePosX, beatPulseMinPosY, beatPulseSizeX, beatPulseSizeY);
					ofSetColor(0);
				}
			}
			//if (showCountdown) {
			//	float countdownXPos = notesOffsetX + (((notesXLength / numerator[getPlayingBarIndex()]) * beatCounter) * notesXCoef);
			//	float countdownYPos = instruments.begin()->second.getMinYPos(thisLoopIndex) - noteHeight - countdownFont.stringHeight(to_string(countdown));
			//	countdownFont.drawString(to_string(countdown), countdownXPos, countdownYPos);
			//}
			if (showBar) {
				for (auto it = instruments.begin(); it != instruments.end(); ++it) {
					it->second.drawStaff(bar, staffOffsetX, scoreYOffset, drawClef, drawMeter, drawLoopStartEnd);
					it->second.drawNotes(bar, i, &loopData[insertNaturalsNdx], notesOffsetX, scoreYOffset, animate, notesXCoef);
				}
			}
		}
	}
}

int ofApp::getMappedIndex(int index)
{
	int foundIndex = 0;
	for (auto it = instrumentIndexMap.begin(); it != instrumentIndexMap.end(); ++it) {
		if (it->second == index) {
			foundIndex = it->first;
			break;
		}
	}
	return foundIndex;
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key)
{
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key)
{
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
void ofApp::storeNewBar(int barIndex)
{
	if (numerator.find(barIndex) == numerator.end()) {
		numerator[barIndex] = 4;
		denominator[barIndex] = 4;
	}
	barsIndexes[barName] = barIndex;
	tempBarLoopIndex = barIndex;
}

//--------------------------------------------------------------
int ofApp::getPlayingBarIndex()
{
	return loopData[loopIndex][thisLoopIndex];
}

//--------------------------------------------------------------
void ofApp::initializeInstrument(int index, string instName, string hostIP, int hostPort)
{
	int maxIndex = (int)instruments.size();
	instrumentIndexMap[maxIndex] = index;
	instrumentIndexes[instName] = maxIndex;
	instruments[maxIndex] = Instrument();
	instruments[maxIndex].setID(maxIndex); // this is for debugging the Notes()
	instruments[maxIndex].setName(instName.substr(1)); // remove the back slash at the beginning

	int thisInstNameWidth = instFont.stringWidth(instName.substr(1));
	instNameWidths[maxIndex] = thisInstNameWidth;
	if (thisInstNameWidth > longestInstNameWidth) {
		longestInstNameWidth = thisInstNameWidth;
	}
	instruments[maxIndex].setStaffSize(scoreFontSize);
	instruments[maxIndex].setNotesFontSize(scoreFontSize);
	instruments[maxIndex].setNumBarsToDisplay(numBarsToDisplay);
	barDataErrorsMap[maxIndex] = vector<int>();
	sendBarDataOKMap[maxIndex] = std::make_pair(false, 0);
	numInstruments++;
	if (!oscSenderSetup) {
		oscSender.setup(hostIP, hostPort);
		oscSenderSetup = true;
	}
	setScoreCoords();
	createFirstBar(maxIndex);
}

//--------------------------------------------------------------
void ofApp::createFirstBar(int instNdx)
{
	numerator[0] = denominator[0] = numBeats[0] = 4;
	instruments[instNdx].createEmptyMelody(0);
	instruments[instNdx].setScoreNotes(0, denominator[0], numerator[0], numBeats[0]);
	loopData[0] = {0};
	calculateStaffPositions(0);
}

//--------------------------------------------------------------
void ofApp::updateTempoBeatsInsts(int barIndex)
{
	tempo = newTempo;
	numBeats[barIndex] = tempNumBeats;
}

//--------------------------------------------------------------
void ofApp::setScoreCoords()
{
	scoreXStartPnt = longestInstNameWidth + (blankSpace * 1.5);
	float staffXLength = (screenWidth / 2) - blankSpace - staffLinesDist - scoreXStartPnt;
	// place yPos at the center of the screen in the Y axis
	float yPos1 = (screenHeight / 2) - (staffLinesDist * 2);
	// and give an offset according to the number of staffs
	// so they are all centered
	if (instruments.size() > 1) {
		// we distance each staff so that another staff could fit in between them
		// hence staffLinesDist * 10, which yields ten times the distance between staff lines
		yPos1 -= ((instruments.size() / 2) * (staffLinesDist * 10));
		// if the number of staffs is even, we must subtract half the distance
		// between each staff
		if ((instruments.size() % 2) == 0) {
			yPos1 += (staffLinesDist * 5);
		}
	}
	notesXStartPnt = 0;
	int i = 0;
	for (auto it = instruments.begin(); it != instruments.end(); ++it) {
		float yAnchor1 = yPos1 + (i * (staffLinesDist * 10));
		// to make maintainance easier, we use the same score.h and score.cpp files for both
		// the main program and the accompaning one, so we use to identical Y anchors here
		// since in the accompaning program, the score orientation is always horizontal
		float yAnchor2 = yAnchor1;
		it->second.setStaffCoords(staffXLength, yAnchor1, yAnchor2, staffLinesDist);
		notesXLength = staffXLength - it->second.getClefXOffset() - it->second.getMeterXOffset() - (staffLinesDist * 2);
		it->second.setNoteCoords(notesXLength, yAnchor1, yAnchor2, staffLinesDist, scoreFontSize);
		i++;
	}
	// get the note width from the first instrument, as they all have the same size
	noteWidth = instruments[0].getNoteWidth();
	noteHeight = instruments[0].getNoteHeight();
}

//--------------------------------------------------------------
void ofApp::setScoreNotes(int barIndex)
{
	for (auto it = instruments.begin(); it != instruments.end(); ++it) {
		if (it->second.getCopied(barIndex)) {
			// copy the numerator and demoninator of the meter as this is used for drawing the pulsating rectangle
			numerator[barIndex] = numerator[it->second.getCopyNdx(barIndex)];
			denominator[barIndex] = denominator[it->second.getCopyNdx(barIndex)];
			it->second.copyMelodicLine(barIndex);
		}
		else {
			it->second.setScoreNotes(barIndex, denominator[barIndex], numerator[barIndex], numBeats[barIndex]);
		}
	}
	calculateStaffPositions(barIndex);
}

//--------------------------------------------------------------
void ofApp::calculateStaffPositions(int bar)
{
	// no need to calculate any coordinates for one instrument only
	if (numInstruments == 1) return;
	float maxHeightSum = 0;
	float minVsAnchor = 0;
	float lastMaxPosition = 0;
	float firstMinPosition = 0;
	int i = 0;
	for (auto it = instruments.begin(); it != instruments.end(); ++it) {
		if ((int)instruments.size() > numInstruments && it == instruments.begin()) continue;
		float maxPosition = it->second.getMaxYPos(bar);
		float minPosition = it->second.getMinYPos(bar);
		float minVsAnchorLocal = it->second.getMinVsAnchor(bar);
		if (i == 0) { // used to be  && it->first == 0
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
	if (maxHeightSum >= allStaffDiffs) {
		allStaffDiffs = maxHeightSum;
		float yAnchor1, yAnchor2;
		float totalDiff = lastMaxPosition - firstMinPosition;
		// if all staffs fit in the screen
		if (totalDiff < screenHeight) {
			yAnchor1 = (screenHeight / 2);
			yAnchor1 -= (((allStaffDiffs + (staffLinesDist * 2)) * \
						(numInstruments / 2)));
			yAnchor1 -= (staffLinesDist * 2);
			if ((instruments.size() % 2) == 0) {
				yAnchor1 += ((allStaffDiffs + (staffLinesDist * 2)) / 2); 
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
		// to make maintainance easier, we use the same score.h and score.cpp files for both
		// the main program and the accompaning one, so we use to identical Y anchors here
		// since in the accompaning program, the score orientation is always horizontal
		yAnchor2 = yAnchor1;
		// use a counter instead of the Instrument() map key, as in the case of the part
		// only one part can have key 0 (this is for the comparison below: if (counter > 0)
		// which used to be: if (it->first > 0), as it is in the main program)
		int counter = 0;
		for (auto it = instruments.begin(); it != instruments.end(); ++it) {
			if ((int)instruments.size() > numInstruments && it == instruments.begin()) continue;
			if (counter > 0) {
				yAnchor1 += (allStaffDiffs + (staffLinesDist * 2));
				yAnchor2 += (allStaffDiffs + (staffLinesDist * 2));
			}
			counter++;
			it->second.correctScoreYAnchor(yAnchor1, yAnchor2);
		}
	}
}

//--------------------------------------------------------------
void ofApp::setScoreSizes()
{
	// for a 35 font size the staff lines distance is 10
	staffLinesDist = 10 * ((float)scoreFontSize / 35.0);
	longestInstNameWidth = 0;
	for (auto it = instruments.begin(); it != instruments.end(); ++it) {
		it->second.setStaffSize(scoreFontSize);
		it->second.setNotesFontSize(scoreFontSize);
		if (instFont.stringWidth(it->second.getName()) > longestInstNameWidth) {
			longestInstNameWidth = instFont.stringWidth(it->second.getName());
		}
	}
	scoreXStartPnt = longestInstNameWidth + (blankSpace * 1.5);
	if (barsIndexes.size() > 0) {
		for (map<string, int>::iterator it = barsIndexes.begin(); it != barsIndexes.end(); ++it) {
			setScoreCoords();
			for (auto it2 = instruments.begin(); it2 != instruments.end(); ++it2) {
				it2->second.setNotePositions(it->second);
			}
		}
	}
	else {
		setScoreCoords();
	}
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
	setScoreSizes();
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h)
{
	screenWidth = w;
	screenHeight = h;
	middleOfScreenX = screenWidth / 2;
	middleOfScreenY = screenHeight / 2;
	windowResizeTimeStamp = ofGetElapsedTimeMillis();
	isWindowResized = true;
	setScoreCoords();
	for (auto it = barsIndexes.begin(); it != barsIndexes.end(); ++it) {
		for (auto it2 = instruments.begin(); it2 != instruments.end(); ++it2) {
			// first we need to set the meter, because that's where the distance between beats is set
			// for the Notes() object
			it2->second.setMeter(it->second, numerator[it->second],
								 denominator[it->second], numBeats[it->second]);
			it2->second.setNotePositions(it->second);
		}
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
