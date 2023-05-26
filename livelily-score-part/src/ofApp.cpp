#include "ofApp.h"
#include <string.h>
#include <sstream>
#include <ctype.h> // to add isdigit()
#include <algorithm> // for replacing characters in a string

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
		if (totalDiff < seqVec->screenHeight) {
			//firstStaff = seqVec->middleOfScreenY - (totalDiff / 2.0);
			int numInsts = seqVec->patternData[seqVec->patternLoopIndexes[loopIndex][0]].size() - 2;
			firstStaff = (seqVec->screenHeight/2) - (((seqVec->allStaffDiffs * numInsts) + 5) / 2.0);
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
		seqVec->patternFirstStaffAnchor[correctAllStaffLoopIndex] = (seqVec->screenHeight/2) - (seqVec->staffLinesDist * 2);
	}
	float xPos = seqVec->staffXOffset+(seqVec->blankSpace*1.5);
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

	posCalculator.setup(&seqVec);

	brightness = 220;
	backGround = BACKGROUND;
	notesID = 0;
	barIndexGlobal = 0;
	fullScreen = false;
	isWindowResized = false;
	windowResizeTimeStamp = 0;

	ofSetFrameRate(framerate);
	ofSetLineWidth(lineWidth);

	ofSetWindowTitle("LiveLily Score Part");

	oscReceiver.setup(OFRECVPORT);

	seqVec.screenWidth = ofGetWindowWidth();
	seqVec.screenHeight = ofGetWindowHeight();
	seqVec.middleOfScreenX = seqVec.screenWidth / 2;
	seqVec.middleOfScreenY = seqVec.screenHeight / 2;
	seqVec.staffWidth = seqVec.screenWidth;

	lineWidth = 2;

	patternIndex = 0;

	seqVec.patternLoopIndex = 0;
	seqVec.tempPatternLoopIndex = 0;
	seqVec.thisPatternLoopIndex = 0;
	seqVec.updatePatternLoop = false;
	seqVec.currentScoreIndex = 0;
	seqVec.nextScoreIndex = 0;
	seqVec.receivedNextScoreIndex = false;

	seqVec.numerator = 4;
	seqVec.denominator = 4;

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
	seqVec.beatVizType = 2;
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
		// first place all data coming in from the sequencer
		// so the if / else if tests are done faster
		if (m.getAddress() == "/beat") {
			seqVec.beatVizTimeStamp = m.getArgAsInt64(0);
			seqVec.beatVizCounter = m.getArgAsInt(1);
			seqVec.beatViz = true;
			cout << "stamp: " << seqVec.beatVizTimeStamp << endl;
		}
		else if (m.getAddress() == "/loopndx") {
			seqVec.patternLoopIndex = m.getArgAsInt(0);
			seqVec.currentScoreIndex = m.getArgAsInt(1);
		}
		else if (m.getAddress() == "/scorendx") {
			seqVec.currentScoreIndex = m.getArgAsInt(0);
			seqVec.receivedNextScoreIndex = false;
		}
		else if (m.getAddress() == "/beatinfo") {
			seqVec.beatVizStepsPerMs = m.getArgAsFloat(0);
			seqVec.beatVizRampStart = m.getArgAsInt64(1);
			seqVec.beatViz = true;
			seqVec.beatAnimate = true;
			cout << "ramp start: " << seqVec.beatVizRampStart << endl;
		}
		else if (m.getAddress() == "/nextndx") {
			seqVec.nextScoreIndex = m.getArgAsInt(0);
			seqVec.receivedNextScoreIndex = true;
		}
		else if (m.getAddress() == "/mute") {
			
		}
		// the all the other data
		else if (m.getAddress() == "/initinsts") {
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				initializeInstrument(m.getArgAsString(i));
				createStaff();
				createNotes();
				// as with the main LiveLily program, when we create a Staff object
				// we send a dummy patternIndex argument, and later on, when we'll set
				// the coordinates, this argument will be meaningful
				initStaffCoords(patternIndex);
			}
		}
		else if (m.getAddress() == "/push_pop_pat") {
			seqVec.patternData.resize(m.getArgAsInt(0)+1);
			seqVec.patternLoopIndexes.resize(m.getArgAsInt(0)+1);
			seqVec.patternFirstStaffAnchor.resize(m.getArgAsInt(0)+1);
			seqVec.maxStaffScorePos.resize(m.getArgAsInt(0)+1);
			seqVec.minStaffScorePos.resize(m.getArgAsInt(0)+1);
			seqVec.distBetweenBeats.resize(patternIndex+1);
			patternIndex = m.getArgAsInt(0);
			seqVec.tempPatternLoopIndex = patternIndex;
		}
		else if (m.getAddress() == "/patidx") {
			seqVec.patternLoopIndexes[patternIndex].push_back(m.getArgAsInt(0));
		}
		else if (m.getAddress() == "/initcoords") {
			initNotesCoords(m.getArgAsInt(0));
		}
		else if (m.getAddress() == "/setnotes") {
			setScoreNotes(m.getArgAsInt(0));
		}
		else if (m.getAddress() == "/patdata") {
			// store the bar number and the number of beats for this pattern
			for (size_t i = 0; i < 2; i++) {
				seqVec.patternData[patternIndex].push_back(m.getArgAsInt(i));
			}
			// then push back the instruments of this part
			for (int i = 0; i < (int)seqVec.instruments.size(); i++) {
				seqVec.patternData[patternIndex].push_back(i);
			}
		}
		else if (m.getAddress() == "/notes") {
			int bar = seqVec.patternData[patternIndex][0];
			int numBeats = seqVec.patternData[patternIndex][1];
			int inst = m.getArgAsInt(0);
			if (scoreNotes[inst].size() <= bar) {
				scoreNotes[inst].resize(bar+1);
			}
			// we push back a vector so we can store chords properly
			scoreNotes[inst][bar].push_back({});
			for (size_t i = 1; i < m.getNumArgs(); i++) {
				// then we push back as many notes as we have in each position in this vector
				scoreNotes[inst][bar].back().push_back(m.getArgAsInt(i));
			}
		}
		else if (m.getAddress() == "/acc") {
			int bar = seqVec.patternData[patternIndex][0];
			int numBeats = seqVec.patternData[patternIndex][1];
			int inst = m.getArgAsInt(0);
			if (scoreAccidentals[inst].size() <= bar) {
				scoreAccidentals[inst].resize(bar+1);
			}
			// we push back a vector so we can store chords properly
			scoreAccidentals[inst][bar].push_back({});
			for (size_t i = 1; i < m.getNumArgs(); i++) {
				// then we push back as many notes as we have in each position in this vector
				scoreAccidentals[inst][bar].back().push_back(m.getArgAsInt(i));
			}
		}
		else if (m.getAddress() == "/oct") {
			int bar = seqVec.patternData[patternIndex][0];
			int numBeats = seqVec.patternData[patternIndex][1];
			int inst = m.getArgAsInt(0);
			if (scoreOctaves[inst].size() <= bar) {
				scoreOctaves[inst].resize(bar+1);
			}
			// we push back a vector so we can store chords properly
			scoreOctaves[inst][bar].push_back({});
			for (size_t i = 1; i < m.getNumArgs(); i++) {
				// then we push back as many notes as we have in each position in this vector
				scoreOctaves[inst][bar].back().push_back(m.getArgAsInt(i));
			}
		}
		else if (m.getAddress() == "/durs") {
			int bar = seqVec.patternData[patternIndex][0];
			int numBeats = seqVec.patternData[patternIndex][1];
			int inst = m.getArgAsInt(0);
			if (scoreDurs[inst].size() <= bar) {
				scoreDurs[inst].resize(bar+1);
			}
			for (size_t i = 1; i < m.getNumArgs(); i++) {
				scoreDurs[inst][bar].push_back(m.getArgAsInt(i));
			}
		}
		else if (m.getAddress() == "/dots") {
			int bar = seqVec.patternData[patternIndex][0];
			int numBeats = seqVec.patternData[patternIndex][1];
			int inst = m.getArgAsInt(0);
			if (scoreDotIndexes[inst].size() <= bar) {
				scoreDotIndexes[inst].resize(bar+1);
			}
			for (size_t i = 1; i < m.getNumArgs(); i++) {
				scoreDotIndexes[inst][bar].push_back(m.getArgAsInt(i));
			}
		}
		else if (m.getAddress() == "/gliss") {
			int bar = seqVec.patternData[patternIndex][0];
			int numBeats = seqVec.patternData[patternIndex][1];
			int inst = m.getArgAsInt(0);
			if (scoreGlissandi[inst].size() <= bar) {
				scoreGlissandi[inst].resize(bar+1);
			}
			for (size_t i = 1; i < m.getNumArgs(); i++) {
				scoreGlissandi[inst][bar].push_back(m.getArgAsInt(i));
			}
		}
		else if (m.getAddress() == "/artic") {
			int bar = seqVec.patternData[patternIndex][0];
			int numBeats = seqVec.patternData[patternIndex][1];
			int inst = m.getArgAsInt(0);
			if (scoreArticulations[inst].size() <= bar) {
				scoreArticulations[inst].resize(bar+1);
			}
			for (size_t i = 1; i < m.getNumArgs(); i++) {
				scoreArticulations[inst][bar].push_back(m.getArgAsInt(i));
			}
		}
		else if (m.getAddress() == "/dyn") {
			int bar = seqVec.patternData[patternIndex][0];
			int numBeats = seqVec.patternData[patternIndex][1];
			int inst = m.getArgAsInt(0);
			if (scoreDynamics[inst].size() <= bar) {
				scoreDynamics[inst].resize(bar+1);
			}
			for (size_t i = 1; i < m.getNumArgs(); i++) {
				scoreDynamics[inst][bar].push_back(m.getArgAsInt(i));
			}
		}
		else if (m.getAddress() == "/dynIdx") {
			int bar = seqVec.patternData[patternIndex][0];
			int numBeats = seqVec.patternData[patternIndex][1];
			int inst = m.getArgAsInt(0);
			if (scoreDynamicsIndexes[inst].size() <= bar) {
				scoreDynamicsIndexes[inst].resize(bar+1);
			}
			for (size_t i = 1; i < m.getNumArgs(); i++) {
				scoreDynamicsIndexes[inst][bar].push_back(m.getArgAsInt(i));
			}
		}
		else if (m.getAddress() == "/dynramp_start") {
			int bar = seqVec.patternData[patternIndex][0];
			int numBeats = seqVec.patternData[patternIndex][1];
			int inst = m.getArgAsInt(0);
			if (scoreDynamicsRampStart[inst].size() <= bar) {
				scoreDynamicsRampStart[inst].resize(bar+1);
			}
			for (size_t i = 1; i < m.getNumArgs(); i++) {
				scoreDynamicsRampStart[inst][bar].push_back(m.getArgAsInt(i));
			}
		}
		else if (m.getAddress() == "/dynramp_end") {
			int bar = seqVec.patternData[patternIndex][0];
			int numBeats = seqVec.patternData[patternIndex][1];
			int inst = m.getArgAsInt(0);
			if (scoreDynamicsRampEnd[inst].size() <= bar) {
				scoreDynamicsRampEnd[inst].resize(bar+1);
			}
			for (size_t i = 1; i < m.getNumArgs(); i++) {
				scoreDynamicsRampEnd[inst][bar].push_back(m.getArgAsInt(i));
			}
		}
		else if (m.getAddress() == "/dynramp_dir") {
			int bar = seqVec.patternData[patternIndex][0];
			int numBeats = seqVec.patternData[patternIndex][1];
			int inst = m.getArgAsInt(0);
			if (scoreDynamicsRampDir[inst].size() <= bar) {
				scoreDynamicsRampDir[inst].resize(bar+1);
			}
			for (size_t i = 1; i < m.getNumArgs(); i++) {
				scoreDynamicsRampDir[inst][bar].push_back(m.getArgAsInt(i));
			}
		}
		else if (m.getAddress() == "/slurbegin") {
			int bar = seqVec.patternData[patternIndex][0];
			int numBeats = seqVec.patternData[patternIndex][1];
			int inst = m.getArgAsInt(0);
			if (scoreSlurBeginnings[inst].size() <= bar) {
				scoreSlurBeginnings[inst].resize(bar+1);
			}
			for (size_t i = 1; i < m.getNumArgs(); i++) {
				scoreSlurBeginnings[inst][bar].push_back(m.getArgAsInt(i));
			}
		}
		else if (m.getAddress() == "/slurend") {
			int bar = seqVec.patternData[patternIndex][0];
			int numBeats = seqVec.patternData[patternIndex][1];
			int inst = m.getArgAsInt(0);
			if (scoreSlurEndings[inst].size() <= bar) {
				scoreSlurEndings[inst].resize(bar+1);
			}
			for (size_t i = 1; i < m.getNumArgs(); i++) {
				scoreSlurEndings[inst][bar].push_back(m.getArgAsInt(i));
			}
		}
		else if (m.getAddress() == "/texts") {
			int bar = seqVec.patternData[patternIndex][0];
			int numBeats = seqVec.patternData[patternIndex][1];
			int inst = m.getArgAsInt(0);
			if (scoreTexts[inst].size() <= bar) {
				scoreTexts[inst].resize(bar+1);
			}
			for (size_t i = 1; i < m.getNumArgs(); i++) {
				scoreTexts[inst][bar].push_back(m.getArgAsString(i));
			}
		}
		else if (m.getAddress() == "/textIdx") {
			int bar = seqVec.patternData[patternIndex][0];
			int numBeats = seqVec.patternData[patternIndex][1];
			int inst = m.getArgAsInt(0);
			if (seqVec.textIndexes[inst].size() <= bar) {
				seqVec.textIndexes[inst].resize(bar+1);
			}
			for (size_t i = 1; i < m.getNumArgs(); i++) {
				seqVec.textIndexes[inst][bar].push_back(m.getArgAsInt(i));
			}
		}
		else if (m.getAddress() == "/poscalc") {
			posCalculator.setLoopIndex(m.getArgAsInt(0));
			posCalculator.startThread();
		}
		else if (m.getAddress() == "/exit") {
			releaseMem();
			ofExit();
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
	ofBackground(brightness);
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
					xAnimationPos = seqVec.blankSpace*1.5;
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
				cout << "degrade: " << brightnessDegrade << endl;
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
	int xPos = seqVec.blankSpace*1.5;
	// draw one line at each side of the seqVec.staffs to connect them
	if (seqVec.instruments.size() > 1) {
		float y1 = seqVec.scoreYAnchors[0][seqVec.currentScoreIndex];
		float y2 = seqVec.scoreYAnchors.back()[seqVec.currentScoreIndex];
		ofDrawLine(xPos, y1, xPos, y2);
		ofDrawLine(seqVec.staffWidth-seqVec.blankSpace-seqVec.staffLinesDist, y1,
				   seqVec.staffWidth-seqVec.blankSpace-seqVec.staffLinesDist, y2);
	}
	// write the names of the instruments
	// for (unsigned i = 0; i < seqVec.instruments.size(); i++) {
	// 	int xPos = seqVec.blankSpace+seqVec.longestInstNameWidth-instNameWidths[i];
	// 	instFont.drawString(seqVec.instruments[i], xPos, seqVec.scoreYAnchors[i][seqVec.currentScoreIndex]+(seqVec.staffLinesDist*2.5));
	// }
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
void ofApp::keyPressed(int key){
	
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){
	
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
void ofApp::initializeInstrument(string instName){
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
	instNameWidths.resize(instNameWidths.size()+1);
	seqVec.scoreYAnchors.resize(seqVec.scoreYAnchors.size()+1);
	seqVec.textIndexes.resize(seqVec.textIndexes.size()+1);

	seqVec.instruments.back() = instName;
	tempNumInstruments = (int)seqVec.instruments.size();
	int thisInstNameWidth = instFont.stringWidth(instName);
	instNameWidths.back() = thisInstNameWidth;
	if (thisInstNameWidth > seqVec.longestInstNameWidth) {
		seqVec.longestInstNameWidth = thisInstNameWidth;
	}
	seqVec.scoreYAnchors.back().push_back({0});
}

//--------------------------------------------------------------
void ofApp::updateTempoBeatsInsts(int ptrnIdx){
	numInstruments = seqVec.activeInstruments.size();
	seqVec.tempo = seqVec.newTempo;
	seqVec.numBeats = seqVec.tempNumBeats;
}

//--------------------------------------------------------------
void ofApp::createStaff(){
	float xPos = seqVec.blankSpace*1.5;
	float xEdge = seqVec.screenWidth-seqVec.blankSpace;
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
	float xPos = seqVec.staffXOffset+(seqVec.blankSpace*1.5);
	float xEdge = seqVec.staffWidth-seqVec.blankSpace-seqVec.staffLinesDist;
	// place yPos at the center of the screen in the Y axis
	float yPos = (seqVec.screenHeight/2) - (seqVec.staffLinesDist * 2);
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
	// give the appropriate offset so that notes are not too close to the loop symbols
	float xStart = seqVec.notesXStartPnt + seqVec.staffLinesDist;
	float xEdge = seqVec.screenWidth - seqVec.blankSpace - (seqVec.staffLinesDist*2);
	// place yPos at the center of the screen in the Y axis
	float yPos = (seqVec.screenHeight/2) - (seqVec.staffLinesDist * 2);
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
void ofApp::releaseMem(){
	posCalculator.stopThread();
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
	windowResizeTimeStamp = ofGetElapsedTimeMillis();
	isWindowResized = true;
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){

}
