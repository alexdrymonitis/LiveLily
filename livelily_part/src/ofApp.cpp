#include "ofApp.h"
#include <string.h>
#include <sstream>

//--------------------------------------------------------------
void ofApp::setup()
{
	backgroundColor = ofColor::black;
	ofBackground(backgroundColor);
	unsigned int framerate = 60;
	
	brightnessCoeff = 0.85;
	beatBrightnessCoeff = 1.0;
	brightness = (int)(255 * brightnessCoeff);
	notesID = 0;
	fullscreen = false;
	finishState = false;
	changeBeatColorOnUpdate = false;
	
	parsingBar = false;
	parsingBars = false;
	parsingLoop = false;
	barsIterCounter = 0;
	numBarsParsed = 0;
	firstInstForBarsIndex = 0;
	firstInstForBarsSet = false;
	
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
	seqState = false;
	
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
	beatVizStepsPerMs = (float)BEATVIZBRIGHTNESS / (500.0 / 4.0);
	beatVizRampStart = 500 / 4;
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
	scoreOrientation = 1;

	correctOnSameOctaveOnly = true;
	
	showCountdown = false;
	countdown = 0;
	showTempo = true;

	thisBarIndex = 0;
	lastBarIndex = 0;
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
		else if (m.getAddress() == "/line") {
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				linesToParse.push_back(m.getArgAsString(i));
			}
			parseStrings();
		}
		else if (m.getAddress() == "/update") {
			tempBarLoopIndex = m.getArgAsInt32(0);
			mustUpdateScore = true;
			showUpdatePulseColor = true;
		}
		else if (m.getAddress() == "/loopndx") {
			loopIndex = m.getArgAsInt32(0);
		}
		else if (m.getAddress() == "/thisloopndx") {
			thisLoopIndex = m.getArgAsInt32(0);
		}
		else if (m.getAddress() == "/seq") {
			seqState = m.getArgAsBool(0);
		}
		else if (m.getAddress() == "/initinst") {
			int ndx = m.getArgAsInt32(0);
			string name = m.getArgAsString(1);
			initializeInstrument(ndx, "\\" + name);
		}
		else if (m.getAddress() == "/group") {
			for (size_t i = 0; i < m.getNumArgs(); i++) {
				if (i > 0) {
					instruments[instrumentIndexes[m.getArgAsString(i)]].setGroup(instruments[instrumentIndexes[m.getArgAsString(i-1)]].getID());
				}
			}
		}
		else if (m.getAddress() == "/numbars") {
			numBarsToDisplay = m.getArgAsInt32(0);
			cout << "num bars: " << numBarsToDisplay << endl;
			for (auto it = instruments.begin(); it != instruments.end(); ++it) {
				it->second.setNumBarsToDisplay(numBarsToDisplay);
			}
			setScoreCoords();
		}
		else if (m.getAddress() == "/scorechange") {
			scoreChangeOnLastBar = m.getArgAsBool(0);
		}
		else if (m.getAddress() == "/countdown") {
			countdown = m.getArgAsInt32(0);
			if (countdown > 0) showCountdown = true;
			else showCountdown = false;
		}
		else if (m.getAddress() == "/nocountdown") {
			showCountdown = false;
		}
		else if (m.getAddress() == "/fullscreen") {
			bool fullscreenBool = m.getArgAsBool(0);
			if (fullscreenBool != fullscreen) {
				ofToggleFullscreen();
				fullscreen = fullscreenBool;
				screenWidth = ofGetWindowWidth();
				screenHeight = ofGetWindowHeight();
				setScoreSizes();
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
		else if (m.getAddress() == "/beatcolor") {
			changeBeatColorOnUpdate = m.getArgAsBool(0);
		}
		else if (m.getAddress() == "/size") {
			staffLinesDist = m.getArgAsFloat(0);
			scoreFontSize = (int)(35.0 * staffLinesDist / 10.0);
	    	instFontSize = scoreFontSize / 3.5;
	    	instFont.load("Monaco.ttf", instFontSize);
	    	setScoreSizes();
		}
		else if (m.getAddress() == "/accoffset") {
			for (auto it = instruments.begin(); it != instruments.end(); ++it) {
				it->second.setAccidentalsOffsetCoef(m.getArgAsFloat(0));
			}
		}
		else if (m.getAddress() == "/exit") {
			ofShowCursor();
			ofExit();
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
	// ndx takes the index of the first visible bar
	// this is useful in horizontal view, where with a four-bar maximum view and a loop with more than four bars
	// ndx will take 0 first, then, after the first four bars have been played
	// it will take 4
	// in the if test below we set the number of bars to be displayed to fit the remaining bars
	// e.g. in a six-bar loop, when the first four bars have been played, we need to display two bars only
	// not four bars which is the maximum number of bars to display (these numbers are hypothetical)
	if (ndx > 0) {
		numBars -= ((int)loopData[loopIndex].size() - ndx);
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
	if (instruments.size() > 0) {
		if (beatAnimate) {
			beatPulseMinPosY = yStartPnt - noteHeight;
			beatPulseMaxPosY = yStartPnt;
			for (auto it = instruments.begin(); it != instruments.end(); ++it) {
				beatPulseMaxPosY += it->second.getMaxYPos(thisLoopIndex);
				beatPulseMaxPosY += allStaffDiffs;
			}
			beatPulseMaxPosY -= allStaffDiffs;
			beatPulseMaxPosY += noteHeight;
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
		// then we display the score
		float staffOffsetX = scoreXStartPnt + scoreXOffset;
		float notesOffsetX = staffOffsetX;
		//notesOffsetX += instruments.begin()->second.getClefXOffset();
		//notesOffsetX += instruments.begin()->second.getMeterXOffset();
		notesOffsetX += (staffLinesDist * NOTESXOFFSETCOEF);
		//float notesXCoef = (instruments.begin()->second.getStaffXLength() + staffOffsetX - notesOffsetX) / notesLength;
		std::pair prevMeter = std::make_pair(4, 4);
		std::pair prevTempo = std::make_pair(0, 0);
		vector<int> prevClefs (instruments.size(), 0); 
		bool prevDrawClef = false, prevDrawMeter = false;
		float connectingLineX = scoreXStartPnt + scoreXOffset;
		for (int i = 0; i < numBars; i++) {
			bool drawClef = false, drawMeter = false, animate = false;
			bool drawTempo = false;
			bool showBar = true;
			bool iVarAugmented = false;
			int iVarTemp = i;
			if (loopData.find(loopIndex) == loopData.end()) return;
			if (loopData.at(loopIndex).size() == 0) return;
			int barNdx = (ndx + i) % (int)loopData[loopIndex].size();
			if ((int)loopData[loopIndex].size() <= barNdx) return;
			bar = loopData[loopIndex][barNdx];
			if (i == 0) {
				drawClef = true;
				for (auto it = instruments.begin(); it != instruments.end(); ++it) {
					prevClefs[it->first] = it->second.getClef(bar);
				}
			}
			else {
				drawClef = false;
				for (auto it = instruments.begin(); it != instruments.end(); ++it) {
					if (prevClefs[it->first] != it->second.getClef(bar)) {
						drawClef = true;
						break;
					}
				}
				for (auto it = instruments.begin(); it != instruments.end(); ++it) {
					prevClefs[it->first] = it->second.getClef(bar);
				}
			}
			// get the meter of this bar to determine if it has changed and thus it has be to written
			std::pair<int, int> thisMeter = instruments.begin()->second.getMeter(bar);
			std::pair<int, int> thisTempo = std::make_pair(tempoBaseForScore[bar], BPMTempi[bar]);
			if (i > 0 && scoreOrientation == 1) {
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
					if (loopData[tempBarLoopIndex].size() == 1) {
						i = ((prevSingleBarPos + 1) % 2) + 1;
						iVarAugmented = true;
					}
				}
				scoreUpdated = true;
				showUpdatePulseColor = true;
			}
			else if (mustUpdateScore && scoreUpdated && thisLoopIndex == 0) {
				mustUpdateScore = scoreUpdated = showUpdatePulseColor = false;
				prevSingleBarPos++;
				if (prevSingleBarPos > 1) prevSingleBarPos = 0;
			}
			// if we're staying in the same loop, when the currently playing bar is displayed at the right-most staff
			// all the other bars must display the next bars of the loop (if there are any)
			else if (((int)thisLoopIndex % numBars) == numBars - 1 && i < numBars - 1) {
				bar = loopData[loopIndex][(barNdx+numBars)%(int)loopData[loopIndex].size()];
				// the calculations below determine whether the remaining bars are less than numBars - 1
				// (we subtract one because in the last slot, we display the bar of the current loop chunk)
				// in which case, we should leave the remaining slots empty
				// for example, in a 6-bar loop with numBars = 4, when we enter the second chunk of the loop
				// we need to display two bars
				// when at the last bar of the first chunk, we display the 5th and 6th bar on the left side
				// and the 4th bar on the right side, with the 3rd slot being blank
				int loopIndexLocal = thisLoopIndex + 1;
				if (loopIndexLocal >= (int)loopData[loopIndex].size()) loopIndexLocal = 0;
				int ndxLocal = (((loopIndexLocal) - ((loopIndexLocal) % numBars)) / numBars) * numBars;
				int numBarsLocal = (ndxLocal > 0 ? numBars - ((int)loopData[loopIndex].size() - ndxLocal) : numBars);
				if (i >= numBarsLocal) showBar = false;
				else showBar = true;
			}
			if (loopData[loopIndex].size() == 1) {
				i = (prevSingleBarPos % 2) + 1;
				iVarAugmented = true;
			}
			// like with the beat visualization, accumulate X offsets for all bars but the first
			if (i > 0) {
				// only the notes need to go back some pixels, in case the clef or meter is not drawn
				// so we use a separate value for the X coordinate
				staffOffsetX += instruments.begin()->second.getStaffXLength();
				notesOffsetX += instruments.begin()->second.getStaffXLength();
			}
			if (iVarAugmented) {
				i = iVarTemp;
				iVarAugmented = false;
			}
			if (prevDrawClef != drawClef) {
				if (drawClef) notesOffsetX += instruments.begin()->second.getClefXOffset();
				else notesOffsetX -= instruments.begin()->second.getClefXOffset();
			}
			if (prevDrawMeter != drawMeter) {
				if (drawMeter) notesOffsetX += instruments.begin()->second.getMeterXOffset();
				else notesOffsetX -= instruments.begin()->second.getMeterXOffset();
			}
			float notesXCoef = notesLength * instruments.begin()->second.getXCoef(); // instruments.begin()->second.getStaffXLength();
			if (drawClef) notesXCoef -= instruments.begin()->second.getClefXOffset();
			if (drawMeter) notesXCoef -= instruments.begin()->second.getMeterXOffset();
			notesXCoef /= notesLength; // (notesLength * instruments.begin()->second.getXCoef());
			// if we're displaying the beat pulse on each beat instead of a rectangle that encloses the whole score
			// we need to display it here, in case we display the score horizontally, where we need to give
			// an offset to the pulse so it is displayed on top of the currently playing bar
			if (beatAnimate && beatViz && beatVizType == 2) {
				// draw only for the currenlty playing bar, whichever that is in the horizontal line
				if (barNdx == (int)thisLoopIndex) {
					beatPulseStartX = notesOffsetX + ((notesLength / numerator[getPlayingBarIndex()]) * beatCounter) * notesXCoef;
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
								else if (changeBeatColorOnUpdate) color = ofColor::cyan;
								else color = ofColor::paleGreen;
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
							else if (changeBeatColorOnUpdate) color = ofColor::cyan;
							else color = ofColor::paleGreen;
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
			if (showCountdown) {
				float countdownXPos = scoreXStartPnt + scoreXOffset; //notesOffsetX + (((notesLength / numerator[getPlayingBarIndex()]) * beatCounter) * notesXCoef);
				float countdownYPos = instruments.begin()->second.getMinYPos(thisLoopIndex) - noteHeight - countdownFont.stringHeight(to_string(countdown));
				countdownFont.drawString(to_string(countdown), countdownXPos, countdownYPos);
			}
			unsigned instCounter = 0;
			map<int, Instrument>::iterator prevInst;
			float yStartPntLocal = yStartPnt;
			for (auto it = instruments.begin(); it != instruments.end(); ++it) {
				if (i == 0) {
					// write the names of the instruments without the backslash and get the longest name
					// only at the first iteration of the bars loop
					float xPos = scoreXOffset + (blankSpace * 0.75) + (longestInstNameWidth - instNameWidths[it->first]);
					float yPos = yStartPntLocal + (staffLinesDist*2.5) + scoreYOffset;
					instFont.drawString(it->second.getName(), xPos, yPos);
				}
				// then draw one line at each side of the staffs to connect them
				// we draw the first one here, and the rest inside the for loop below
				float connectingLineY2 = yStartPntLocal - allStaffDiffs + (staffLinesDist * 2);
				if (instCounter > 0) {
					if (i == 0) {
						ofDrawLine(connectingLineX, yStartPntLocal, connectingLineX, connectingLineY2);
						if (numBars == 1) {
							if (it->second.getGroup() == prevInst->second.getID() && it->second.getGroup() > -1) {
								int barEndX = connectingLineX + instruments.begin()->second.getStaffXLength();
								ofDrawLine(barEndX, yStartPntLocal, barEndX, connectingLineY2);
							}
						}
					}
					else {
						if (instCounter == 1) connectingLineX += instruments.begin()->second.getStaffXLength();
						// if this instrument is groupped with the previous one, draw a connecting line at the end of the bar too
						if (it->second.getGroup() == prevInst->second.getID() && it->second.getGroup() > -1) {
							ofDrawLine(connectingLineX, yStartPntLocal, connectingLineX, connectingLineY2);
							if (i  == numBars - 1) {
								int barEndX = connectingLineX + instruments.begin()->second.getStaffXLength();
								ofDrawLine(barEndX, yStartPntLocal, barEndX, connectingLineY2);
							}
						}
					}
				}
				bool drawTempoLocal = (showTempo && drawTempo ? true : false);
				if (showBar) {
					it->second.drawStaff(bar, staffOffsetX, yStartPntLocal, scoreYOffset, drawClef, drawMeter, drawLoopStartEnd, drawTempoLocal);
					it->second.drawNotes(bar, i, &loopData[insertNaturalsNdx], notesOffsetX, yStartPntLocal, scoreYOffset, animate, notesXCoef);
				}
				yStartPntLocal += (allStaffDiffs + (staffLinesDist * 2));
				instCounter++;
				prevInst = it;
			}
			prevMeter.first = thisMeter.first;
			prevMeter.second = thisMeter.second;
			prevTempo.first = thisTempo.first;
			prevTempo.second = thisTempo.second;
			prevDrawClef = drawClef;
			prevDrawMeter = drawMeter;
		}
	}
}

//--------------------------------------------------------------
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

//--------------------------------------------------------------
bool ofApp::isFloat(string str)
{
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

void ofApp::parseStrings()
{
	for (unsigned i = 0; i < linesToParse.size(); i++) {
		parseString(linesToParse.at(i));
	}
	if (parsingBar || parsingLoop) {
		// the bar index has already been stored with the \bar command
		int barIndex = getLastLoopIndex();
		if (!parsingLoop) {
			// first add a rest for any instrument that is not included in the bar
			fillInMissingInsts(barIndex);
			// then store the number of beats for this bar
			// if we create a bar, create a loop with this bar only
			loopData[barIndex] = {barIndex};
			// check if no time has been specified and set the default 4/4
			if (numerator.find(barIndex) == numerator.end()) {
				numerator[barIndex] = 4;
				denominator[barIndex] = 4;
			}
			setScoreNotes(barIndex);
			if (!parsingBars) {
				setNotePositions(barIndex);
				calculateStaffPositions(barIndex, false);
			}
			for (auto it = instruments.begin(); it != instruments.end(); ++it) {
				it->second.setPassed(false);
			}
		}
		if (!seqState) {
			loopIndex = tempBarLoopIndex;
		}
		parsingBar = false;
		parsingLoop = false;
	}
	if (parsingBars) {
		// check if all instruments are done parsing all their bars
		bool parsingBarsDone = true;
		for (auto it = instruments.begin(); it != instruments.end(); ++it) {
			if (!it->second.getMultiBarsDone()) {
				parsingBarsDone = false;
				break;
			}
		}
		// call this function recursively, until all instruments are done parsing their bars
		if (!parsingBarsDone) {
			parseStrings();
		}
		else {
			int barIndex = barsIndexes["\\"+multiBarsName+"-1"];
			setNotePositions(barIndex, barsIterCounter);
			for (int i = 0; i < barsIterCounter; i++) {
				int barIndex = barsIndexes["\\"+multiBarsName+"-"+to_string(i+1)];
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
			parseCommand(multiBarsLoop);
			barsIterCounter = 0;
		}
	}
	linesToParse.clear();
}

//--------------------------------------------------------------
void ofApp::parseString(string str)
{
	if (str.length() == 0) return;
	if (startsWith(str, "%")) return;
	// strip white spaces
	while (startsWith(str, " ")) {
		str = str.substr(1);
	}
	// then check again if the string is empty
	if (str.length() == 0) return;
	size_t commentIndex = str.find("%");
	if (commentIndex != string::npos) {
		str = str.substr(0, commentIndex);
	}
	// check if there's a trailing white space
	if (endsWith(str, " ")) {
		str = str.substr(0, str.size()-1);
	}
	// if we're left with a closing bracket only
	if (str.compare("}") == 0) return;
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
	if (str.length() == 0) return;
	// ignore comments
	if (!startsWith(str, "%")) {
		if (parsingLoop) {
			parseBarLoop(str);
		}
		else {
			parseCommand(str);
		}
	}
}

//--------------------------------------------------------------
void ofApp::fillInMissingInsts(int barIndex)
{
	for (auto it = instruments.begin(); it != instruments.end(); ++it) {
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
	if (numerator.find(barIndex) == numerator.end()) {
		numerator[barIndex] = 4;
		denominator[barIndex] = 4;
	}
	barsIndexes[barName] = barIndex;
	loopsIndexes[barName] = barIndex;
	loopsOrdered[barIndex] = barName;
	loopsVariants[barIndex] = 0;
	tempBarLoopIndex = barIndex;
	lastBarIndex = barIndex;
	int prevBarIndex = getPrevBarIndex();
	if (prevBarIndex < 0) {
		numerator[barIndex] = 4;
		denominator[barIndex] = 4;
		numBeats[barIndex] = numerator[barIndex] * (MINDUR / denominator[barIndex]);
		tempoMs[barIndex] = 500;
		BPMTempi[barIndex] = 120;
		BPMMultiplier[barIndex] = 1;
		beatAtDifferentThanDivisor[barIndex] = false;
		beatAtValues[barIndex] = 1;
		tempoBaseForScore[barIndex] = 4;
		BPMDisplayHasDot[barIndex] = false;
		tempo[barIndex] = tempoMs[barIndex] / (MINDUR / denominator[barIndex]);
	}
	else {
		numerator[barIndex] = numerator[prevBarIndex];
		denominator[barIndex] = denominator[prevBarIndex];
		numBeats[barIndex] = numBeats[prevBarIndex];
		tempoMs[barIndex] = tempoMs[prevBarIndex];
		BPMTempi[barIndex] = BPMTempi[prevBarIndex];
		BPMMultiplier[barIndex] = BPMMultiplier[prevBarIndex];
		beatAtDifferentThanDivisor[barIndex] = beatAtDifferentThanDivisor[prevBarIndex];
		beatAtValues[barIndex] = beatAtValues[prevBarIndex];
		tempoBaseForScore[barIndex] = tempoBaseForScore[prevBarIndex];
		BPMDisplayHasDot[barIndex] = BPMDisplayHasDot[prevBarIndex];
		tempo[barIndex] = tempo[prevBarIndex];
	}
	for (auto it = instruments.begin(); it != instruments.end(); ++it) {
		it->second.setMeter(barIndex, numerator[barIndex], denominator[barIndex], numBeats[barIndex]);
	}
	return barIndex;
}

//--------------------------------------------------------------
void ofApp::storeNewLoop(string loopName)
{
	int loopIndex = getLastLoopIndex() + 1;
	loopsIndexes[loopName] = loopIndex;
	loopsOrdered[loopIndex] = loopName;
	loopsVariants[loopIndex] = 0;
	tempBarLoopIndex = loopIndex;
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
void ofApp::parseCommand(string str)
{
	vector<string> commands = tokenizeString(str, " ");
	std::pair<int, string> error = std::make_pair(0, "");

	if (commands[0].compare("\\time") == 0) {
		int divisionSym = commands[1].find("/");
		string numeratorStr = commands[1].substr(0, divisionSym);
		string denominatorStr = commands[1].substr(divisionSym+1);
		int numeratorLocal = 0;
		int denominatorLocal = 0;
		if (isNumber(numeratorStr)) {
			numeratorLocal = stoi(numeratorStr);
		}
		if (isNumber(denominatorStr)) {
			denominatorLocal = stoi(denominatorStr);
		}
		// a \time command must be placed inside a bar definition
		// and the bar index has already been stored, so we can safely query the last bar index
		int bar = getLastBarIndex();
		numerator[bar] = numeratorLocal;
		denominator[bar] = denominatorLocal;
		numBeats[bar] = numerator[bar] * (MINDUR / denominator[bar]);
		if (BPMMultiplier.find(bar) == BPMMultiplier.end()) {
			BPMMultiplier[bar] = 1;
			beatAtValues[bar] = 1;
			beatAtDifferentThanDivisor[bar] = true;
		}
		for (auto it = instruments.begin(); it != instruments.end(); ++it) {
			it->second.setMeter(bar, numerator[bar], denominator[bar], numBeats[bar]);
		}
	}

	else if (commands[0].compare("\\tempo") == 0) {
		// a \time command must be placed inside a bar definition
		// and the bar index has already been stored, so we can safely query the last bar index
		int bar = getLastBarIndex();
		if (commands.size() == 4) {
			BPMMultiplier[bar] = getBaseDurValue(commands[1], denominator[bar]);
			beatAtValues[bar] = BPMMultiplier[bar];
			beatAtDifferentThanDivisor[bar] = true;
			BPMTempi[bar] = stoi(commands[3]);
			// convert BPM to ms
			tempoMs[bar] = 1000.0 / ((float)(BPMTempi[bar]  * BPMMultiplier[bar]) / 60.0);
			// get the tempo of the minimum duration
			tempo[bar] = tempoMs[bar] / (MINDUR / denominator[bar]);
			if (commands[1].find(".") != string::npos) {
				tempoBaseForScore[bar] = stoi(commands[1].substr(0, str.find(".")));
				BPMDisplayHasDot[bar] = true;
			}
			else {
				tempoBaseForScore[bar] = stoi(commands[1]);
				BPMDisplayHasDot[bar] = false;
			}
		}
		else {
			if (isNumber(commands[1])) {
				BPMTempi[bar] = stoi(commands[1]);
				// convert BPM to ms
				tempoMs[bar] = 1000.0 / ((float)BPMTempi[bar] / 60.0);
				// get the tempo of the minimum duration
				tempo[bar] = tempoMs[bar] / (MINDUR / denominator[bar]);
				BPMMultiplier[bar] = beatAtValues[bar] = 1;
				beatAtDifferentThanDivisor[bar] = true;
				tempoBaseForScore[bar] = 4;
				BPMDisplayHasDot[bar] = false;
			}
		}
	}

	else if (commands[0].compare("\\bar") == 0) {
		string barName = "\\" + commands[1];
		parsingBar = true;
		int barIndex = storeNewBar(barName);
		// first set all instruments to not passed and not copied
		for (auto it = instruments.begin(); it != instruments.end(); ++it) {
			it->second.setPassed(false);
			it->second.setCopied(barIndex, false);
			// if this is not the first bar, set a default clef similar to the last one
			if (barIndex > 0) it->second.setClef(barIndex, it->second.getClef(getPrevBarIndex()));
		}
		// since bars include simple melodic lines in their definition
		// we need to know this when we parse these lines
		if (commands.size() > 2) {
			string restOfCommand = "";
			for (unsigned i = 2; i < commands.size()-1; i++) {
				restOfCommand += commands[i];
				restOfCommand += " ";
			}
			restOfCommand += commands[commands.size()-1];
			parseString(restOfCommand);
		}
	}

	else if (commands[0].compare("\\loop") == 0) {
		string loopName = "\\" + commands[1];
		parsingLoop = true;
		storeNewLoop(loopName);
		// loops are most likely defined in one-liners, so we parse the rest of the line here
		if (commands.size() > 2) {
			string restOfCommand = "";
			for (unsigned i = 2; i < commands.size(); i++) {
				restOfCommand += commands[i];
				restOfCommand += " ";
			}
			parseString(restOfCommand);
		}
	}

	else if (commands[0].compare("\\bars") == 0) {
		if (barsIterCounter == 0) {
			for (auto it = instruments.begin(); it != instruments.end(); ++it) {
				it->second.setMultiBarsDone(false);
				it->second.setMultiBarsStrBegin(0);
			}
			parsingBars = true;
		}
		multiBarsName = commands[1];
		barsIterCounter++;
		// store each bar separately with the "-x" suffix, where x is the counter
		// of the iterations, until all instruments have had all their separate bars parsed
		parseCommand("\\bar "+multiBarsName+"-"+to_string(barsIterCounter));
	}

	else if (commands[0].compare("\\rest") == 0) {
		int numFoundInsts = 0;
		if (commands.size() == 2) {
			auto barToCopyIt = barsIndexes.find(commands[1]);
			barToCopy = barToCopyIt->second;
		}
		for (map<int, Instrument>::iterator it = instruments.begin(); it != instruments.end(); ++it) {
			if (!it->second.passed) { // if an instrument hasn't passed
				numFoundInsts++;
				// turn this instrument to passed
				// so we don't find it in the next iteration
				it->second.passed = true;
				break;
			}
		}
		if (numFoundInsts != 0) {
			for (map<int, Instrument>::iterator it = instruments.begin(); it != instruments.end(); ++it) {
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
					parseString(restOfCommand);
				}
			}
		}
	}

	else {
		bool b = isInstrument(commands);
		if (!b) b = isBarLoop(commands);
	}
}

//--------------------------------------------------------------
bool ofApp::isInstrument(vector<string>& commands)
{
	bool instrumentExists = false;
	unsigned commandNdxOffset = 1;
	if (instruments.size() > 0) {
		if (instrumentIndexes.find(commands[0]) != instrumentIndexes.end()) {
			instrumentExists = true;
			lastInstrument = commands[0];
			lastInstrumentIndex = instrumentIndexes[commands[0]];
			if (commands.size() > 1) {
				if (commands[1].compare("clef") == 0) {
					if (commands[2].compare("treble") == 0) {
						instruments[lastInstrumentIndex].setClef(getLastBarIndex(), 0);
					}
					else if (commands[2].compare("bass") == 0) {
						instruments[lastInstrumentIndex].setClef(getLastBarIndex(), 1);
					}
					else if (commands[2].compare("alto") == 0) {
						instruments[lastInstrumentIndex].setClef(getLastBarIndex(), 2);
					}
					else if (commands[2].compare("perc") == 0 || commands[2].compare("percussion") == 0) {
						instruments[lastInstrumentIndex].setClef(getLastBarIndex(), 3);
					}
					if (commands.size() > 3) {
						commandNdxOffset = 3;
						goto parseMelody;
					}
					return instrumentExists;
				}
				else if (commands[1].compare("rhythm") == 0) {
					instruments[lastInstrumentIndex].setRhythm(true);
				}
				else if (commands[1].compare("transpose") == 0) {
					int transposition = stoi(commands[2]);
					instruments[lastInstrumentIndex].setTransposition(transposition);
				}
				else if (commands[1].compare("sendmidi") == 0) {
					instruments[lastInstrumentIndex].setSendMIDI(true);
				}
				else {
				parseMelody:
					string restOfCommand = "";
					for (unsigned i = commandNdxOffset; i < commands.size(); i++) {
						// add a white space only after the first token has been added
						if (i > commandNdxOffset) restOfCommand += " ";
						restOfCommand += commands[i];
					}
					if (parsingBars && barsIterCounter == 1 && !firstInstForBarsSet) {
						firstInstForBarsIndex = lastInstrumentIndex;
						firstInstForBarsSet = true;
					}
					parseMelodicLine(restOfCommand);
					return instrumentExists;
				}
			}
		}
	}
	return instrumentExists;
}

//--------------------------------------------------------------
bool ofApp::isBarLoop(vector<string>& commands)
{
	bool barLoopExists = false;
	if (loopsIndexes.size() > 0) {
		if (loopsIndexes.find(commands[0]) != loopsIndexes.end()) {
			barLoopExists = true;
			int indexLocal = loopsIndexes[commands[0]];
			// if all is OK, or if we insist on calling this loop, it goes through
			if (commands.size() > 1) {
				if (commands.size() == 3) {
					auto it = find(loopData[indexLocal].begin(), loopData[indexLocal].end(), loopsIndexes[commands[2]]);
					int gotoIndexOrBarName = 0;
					if (it == loopData[indexLocal].end()) {
						if (isNumber(commands[2])) {
							int indexToGoto = stoi(commands[2]);
							if (indexToGoto <= (int)loopData[indexLocal].size()) {
								thisLoopIndex = indexToGoto - 1; // argument is 1-based
								gotoIndexOrBarName = 1;
							}
						}
					}
					if (commands[1].compare("goto") == 0) {
						if (gotoIndexOrBarName == 0) thisLoopIndex = it - loopData[indexLocal].begin();
					}
					else {
						return barLoopExists;
					}
				}
			}
		}
	}
	return barLoopExists;
}

//--------------------------------------------------------------
int ofApp::getLastLoopIndex()
{
	int loopIndex = -1;
	// since every bar is copied to the loop maps, but loops are not copied to the bar map
	// we query the loopsIndexes map instead of the barIndexes map
	// so that the keys between bars and loops are the same
	if (loopsOrdered.size() > 0) {
		map<int, string>::reverse_iterator it = loopsOrdered.rbegin();
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
	if (lastLoopIndex > -1) {
		while (barsIndexes.find(loopsOrdered[lastLoopIndex]) == barsIndexes.end()) {
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
		while (barsIndexes.find(loopsOrdered[prevBarIndex]) == barsIndexes.end()) {
			prevBarIndex--;
		}
	}
	return prevBarIndex;
}

//--------------------------------------------------------------
int ofApp::getPlayingBarIndex()
{
	if (loopData.find(loopIndex) == loopData.end()) {
		return -1;
	}
	if (thisLoopIndex >= loopData[loopIndex].size()) {
		return -1;
	}
	return loopData[loopIndex][thisLoopIndex];
}

//--------------------------------------------------------------
void ofApp::stripLineFromBar(string str)
{
	auto barToCopyIt = loopsIndexes.find(str);
	int barIndex = getLastLoopIndex();
	barToCopy = barToCopyIt->second;
	instruments[lastInstrumentIndex].setPassed(true);
	instruments[lastInstrumentIndex].setCopied(barIndex, true);
	instruments[lastInstrumentIndex].setCopyNdx(barIndex, barToCopy);
	instruments[lastInstrumentIndex].copyMelodicLine(barIndex);
}

//--------------------------------------------------------------
void ofApp::parseBarLoop(string str)
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
			for (auto it = loopsOrdered.begin(); it != loopsOrdered.end(); ++it) {
				if (barsIndexes.find(it->second) == barsIndexes.end()) {
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
			howManyTimes = stoi(name.substr(multIndex+1));
		}
		// first check for barLoops because when we define a bar with data
		// and not with combinations of other bars, we create a barLoop with the same name
		thisBarLoopIndexIt = loopsIndexes.find(name.substr(0, nameLength));
		if (thisBarLoopIndexIt == loopsIndexes.end()) {
			// check if the substring ends with a closing curly bracket, in which case we must remove it
			if (endsWith(name.substr(0, nameLength), "}")) {
				thisBarLoopIndexIt = loopsIndexes.find(name.substr(0, nameLength-1));
			}
		}
		// find the map of the loop with the vector that contains indexes of bars
		thisBarLoopDataIt = loopData.find(thisBarLoopIndexIt->second);
		// then iterate over the number of repetitions of this loop
		for (i = 0; i < howManyTimes; i++) {
			// and push back every item in the vector that contains the indexes of bars
			for (barIndex = thisBarLoopDataIt->second.begin(); barIndex != thisBarLoopDataIt->second.end(); ++barIndex) {
				thisLoopIndexes.push_back(*barIndex);
			}
		}
	}
	// find the last index of the stored loops and store the vector we just created to the value of loopData
	int loopNdx = getLastLoopIndex();
	loopData[loopNdx] = thisLoopIndexes;
	if (restOfCommand.size() > 0) {
		// falsify this so that the name of the loop is treated properly in parseString()
		// otherwise, parseString() will again call this function
		parsingLoop = false;
		parseString(restOfCommand);
	}
	if (!seqState) {
		loopIndex = tempBarLoopIndex;
	}
}

//--------------------------------------------------------------
void ofApp::parseMelodicLine(string str)
{
	bool parsingBarsFromLoop = false;
	int ndx;
	string parsingBarsFromLoopName;
	if (parsingBars && startsWith(str, "\\") && \
			(str.substr(0, str.find(" ")).compare("\\ottava") != 0 && str.substr(0, str.find(" ")).compare("\\ott") != 0) && \
			(str.substr(0, str.find(" ")).compare("\\tuplet") != 0 && str.substr(0, str.find(" ")).compare("\\tup") != 0) && \
			str.find("|") == string::npos) {
		if (barsIndexes.find(str) == barsIndexes.end()) {
			if (loopsIndexes.find(str) != loopsIndexes.end()) {
				parsingBarsFromLoop = true;
				parsingBarsFromLoopName = str;
			}
		}
	}
	size_t strBegin, strLen;
	string strToProcess = detectRepetitions(str);
	if (parsingBars) {
		if (parsingBarsFromLoop) {
			if (lastInstrumentIndex == firstInstForBarsIndex) {
				numBarsParsed = (int)loopData[loopsIndexes[parsingBarsFromLoopName]].size();
			}
			else {
				// left operand of if below was barsIterCounter
				int size = (int)loopData[loopsIndexes[parsingBarsFromLoopName]].size();
			}
			if (barsIterCounter-1 >= (int)loopData[loopsIndexes[parsingBarsFromLoopName]].size()-1) {
				instruments.at(lastInstrumentIndex).setMultiBarsDone(true);
			}
			ndx = loopData[loopsIndexes[parsingBarsFromLoopName]][barsIterCounter-1];
			strToProcess = loopsOrdered[ndx];
		}
		else {
			strBegin = instruments.at(lastInstrumentIndex).getMultiBarsStrBegin();
			strLen = strToProcess.substr(strBegin).find("|");
			vector<string> bars = tokenizeString(strToProcess, "|"); // to get the number of bars
			if (lastInstrumentIndex == firstInstForBarsIndex) {
				numBarsParsed = (int)bars.size();
			}
			if (strLen != string::npos) {
				strToProcess = strToProcess.substr(strBegin, strLen-1);
				instruments.at(lastInstrumentIndex).setMultiBarsStrBegin(strBegin+strLen+2);
			}
			else {
				strToProcess = strToProcess.substr(strBegin);
				instruments.at(lastInstrumentIndex).setMultiBarsDone(true);
			}
		}
	}
	if (startsWith(strToProcess, "\\") && \
			(strToProcess.substr(0, strToProcess.find(" ")).compare("\\ottava") != 0 && strToProcess.substr(0, strToProcess.find(" ")).compare("\\ott") != 0) && \
			(strToProcess.substr(0, strToProcess.find(" ")).compare("\\tuplet") != 0 && strToProcess.substr(0, strToProcess.find(" ")).compare("\\tup") != 0)) {
		if (barsIndexes.find(strToProcess) != barsIndexes.end()) {
			stripLineFromBar(strToProcess);
			return;
		}
	}
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
	bool chordStarted = false;
	bool firstChordNote = false;
	
	bool foundArticulation = false;
	// boolean for dynamics, whether it's a mezzo forte or mezzo piano
	bool mezzo = false;
	vector<string> tokens = tokenizeString(strToProcess, " ");
	// index variables for loop so that they are initialized before vectors with unkown size
	int i = 0, j = 0; //  initialize to avoid compiler warnings
	// first detect if there are any quotes, which might include one or more white spaces
	bool closedQuote = true;
	for (i = 0; i < (int)tokens.size(); i++) {
		// first determine if we have both quotes in the same token
		vector<int> quoteNdx = findIndexesOfCharInStr(tokens.at(i), "\"");
		if (quoteNdx.size() > 1) {
			continue;
		}
		// if we find a quote and we're not at the last token
		if (tokens.at(i).find("\"") != string::npos && i < (tokens.size()-1)) {
			unsigned ndxLocal = i+1;
			closedQuote = false;
			while (ndxLocal < tokens.size()) {
				// if we find the pairing quote, concatenate the strings into one token
				if (tokens.at(ndxLocal).find("\"") != string::npos) {
					tokens.at(i) += (" " + tokens.at(ndxLocal));
					tokens.erase(tokens.begin()+ndxLocal);
					closedQuote = true;
					break;
				}
				ndxLocal++;
			}
		}
	}
	// then detect any chords, as the size of most vectors equals the number of notes vertically
	// and not single notes within chords
	unsigned numNotesVertical = tokens.size();
	unsigned chordNotesCounter = 0;
	unsigned numErasedOttTokens = 0;
	unsigned chordOpeningNdx;
	unsigned chordClosingNdx;
	// characters that are not permitted inside a chord are
	// open/close parenthesis, hyphen, dot, backslash, carret, underscore, open/close curly brackets
	vector<int> forbiddenCharsInChord{40, 41, 45, 46, 92, 94, 95, 123, 125};
	for (i = 0; i < (int)tokens.size(); i++) {
		if (tokens.at(i).compare("\\ottava") == 0 || tokens.at(i).compare("\\ott") == 0) {
			numErasedOttTokens += 2;
			i += 1;
			continue;
		}
		if (chordStarted) {
			bool chordEnded = false;
			chordNotesCounter++;
		}
		// if we find a quote and we're not at the last token
		chordOpeningNdx = tokens.at(i).find("<");
		chordClosingNdx = tokens.at(i).find(">");
		// an opening angle brace is also used for crescendi, but when used to start a chord
		// it must be at the beginning of the token, so we test if its index is 0
		if (chordOpeningNdx != string::npos && chordOpeningNdx == 0 && i < tokens.size()) {
			chordStarted = true;
		}
		else if (chordClosingNdx != string::npos && chordClosingNdx < tokens.at(i).size()) {
			chordStarted = false;
		}
	}
	if (tokens.size() == numErasedOttTokens) return; // in case we only provide the ottava
	numNotesVertical -= (chordNotesCounter + numErasedOttTokens);

	// check for tuplets
	int tupMapNdx = 0;
	map<int, std::pair<int, int>> tupRatios;
	for (i = 0; i < (int)tokens.size(); i++) {
		if (tokens.at(i).compare("\\tuplet") == 0 || tokens.at(i).compare("\\tup") == 0) {
			// tests whether the tuplet format is correct have been made above
			// so we can safely query tokens.at(i+1) etc
			size_t divisionNdx = tokens.at(i+1).find("/");
			size_t remainingStr = tokens.at(i+1).size() - divisionNdx - 1;
			tupRatios[tupMapNdx++] = std::make_pair(stoi(tokens.at(i+1).substr(0, divisionNdx)), stoi(tokens.at(i+1).substr(divisionNdx+1, remainingStr)));
		}
	}

	// store the indexes of the tuplets beginnings and endings so we can erase all the unnessacary tokens
	unsigned tupStart, tupStop = 0; // assign 0 to tupStop to stop the compiler from throwing a warning message
	unsigned tupStartNoChords, tupStopNoChords = 0; // indexes for erasing items from tokens vector
	int tupStartHasNoBracket = 0, tupStopHasNoBracket = 0;
	int numErasedTupTokens = 0;
	unsigned tupStartNdx = 0, tupStopNdx = 0;
	bool tupStartInsideChord = false, tupStopInsideChord = false;
	tupMapNdx = 0;
	map<int, std::pair<unsigned, unsigned>> tupStartStop;
	for (i = 0; i < (int)tokens.size(); i++) {
		if (startsWith(tokens.at(i), "<")) {
			tupStartInsideChord = true;
		}
		else if (startsWith(tokens.at(i), "{") && tokens.at(i).size() > 1) {
			if (tokens.at(i).at(1) == char(60)) { // <
				tupStartInsideChord = true;
			}
		}
		if (tokens.at(i).compare("\\tuplet") == 0 || tokens.at(i).compare("\\tup") == 0) {
			if (tokens.at(i+2).compare("{") == 0) {
				tupStart = tupStartNdx+3;
				tupStartNoChords = i+3;
				tupStartHasNoBracket = 1;
			}
			else {
				tupStart = tupStartNdx+2;
				tupStartNoChords = (unsigned)i+2;
			}
			tupStopInsideChord = false;
			tupStopNdx = tupStartNdx + 2 + tupStartHasNoBracket;
			for (j = i+2+tupStartHasNoBracket; j < tokens.size(); j++) {
				if (startsWith(tokens.at(j), "<")) {
					tupStopInsideChord = true;
				}
				else if (startsWith(tokens.at(j), "{") && tokens.at(j).size() > 1) {
					if (tokens.at(j).at(1) == char(60)) { // <
						tupStopInsideChord = true;
					}
				}
				if (tokens.at(j).find("}") != string::npos) {
					if (tokens.at(j).compare("}") == 0) {
						tupStop = tupStopNdx-1;
						tupStopNoChords = (unsigned)j-1;
						tupStopHasNoBracket = 1;
					}
					else {
						tupStop = tupStopNdx;
						tupStopNoChords = (unsigned)j;
					}
					break;
				}
				if (tupStopInsideChord && tokens.at(j).find(">") != string::npos) {
					tupStopInsideChord = false;
				}
				if (!tupStopInsideChord) tupStopNdx++;
			}
			// offset the start and stop of the tuplet
			tupStart -= (2 + tupStartHasNoBracket);
			tupStartNoChords -= (2 + tupStartHasNoBracket);
			tupStop -= (2 + tupStartHasNoBracket);
			tupStopNoChords -= (2 + tupStartHasNoBracket);
			// now that we have temporarily stored the start and end of this tuplet
			// we can erase it from the tokens vector and correct the stored indexes
			// before we store them to the vector of pairs
			// erase \tuplet and ratio
			tokens.erase(tokens.begin() + i);
			tokens.erase(tokens.begin() + i);
			// check if the opening and closing curly bracket is isolated from the tuplet body
			if (tupStartHasNoBracket) tokens.erase(tokens.begin() + tupStartNoChords);
			if (tupStopHasNoBracket) tokens.erase(tokens.begin() + tupStopNoChords + 1);
			tupStartStop[tupMapNdx++] = std::make_pair(tupStart, tupStop);
			numErasedTupTokens += (2 + tupStartHasNoBracket + tupStopHasNoBracket);
			i = (int)tupStopNoChords;
			tupStartNdx = tupStopNdx - 2 - tupStartHasNoBracket;
			tupStartHasNoBracket = tupStopHasNoBracket = 0;
		}
		else {
			if (tupStartInsideChord && tokens.at(i).find(">") != string::npos) {
				tupStartInsideChord = false;
			}
			if (!tupStartInsideChord) tupStartNdx++;
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
	vector<unsigned> dotsCounter(numNotesVertical, 0);
	vector<int> dynamicsData(numNotesVertical, 0);
	vector<int> dynamicsRampData(numNotesVertical, 0);
	vector<int> slurBeginningsIndexes(numNotesVertical, -1);
	vector<int> slurEndingsIndexes(numNotesVertical, -1);
	vector<int> tieIndexes(numNotesVertical, -1);
	//vector<int> textIndexesLocal(numNotesVertical, 0);
	vector<int> dursForScore(numNotesVertical, 0);
	vector<int> dynamicsForScore(numNotesVertical, -1);
	vector<int> dynamicsForScoreIndexes(numNotesVertical, -1);
	vector<int> dynamicsRampStart(numNotesVertical, 0);
	vector<int> dynamicsRampEnd(numNotesVertical, 0);
	vector<pair<unsigned, unsigned>> dynamicsRampIndexes(numNotesVertical);
	vector<int> midiDynamicsRampDurs(numNotesVertical, 0);
	vector<int> glissandiIndexes(numNotesVertical, 0);
	vector<int> midiGlissDurs(numNotesVertical, 0);
	vector<int> dynamicsRampStartForScore(numNotesVertical, -1);
	vector<int> dynamicsRampEndForScore(numNotesVertical, -1);
	vector<int> dynamicsRampDirForScore(numNotesVertical, -1);
	vector<bool> isSlurred (numNotesVertical, false);
	vector<int> notesCounter(tokens.size()-numErasedOttTokens, 0);
	vector<int> verticalNotesIndexes(tokens.size()-numErasedOttTokens+1, 0); // plus one to easily break out of loops in case of chords
	vector<int> beginningOfChords(tokens.size()-numErasedOttTokens, 0);
	vector<int> endingOfChords(tokens.size()-numErasedOttTokens, 0);
	vector<unsigned> accidentalIndexes(tokens.size()-numErasedOttTokens);
	vector<unsigned> chordNotesIndexes(tokens.size()-numErasedOttTokens, 0);
	vector<int> ottavas(numNotesVertical, 0);
	int verticalNoteIndex;
	vector<bool> foundNotes(i-numErasedOttTokens);
	vector<int> transposedOctaves(i-numErasedOttTokens, 0);
	vector<int> transposedAccidentals(i-numErasedOttTokens, 0);
	// variable to determine which character we start looking at
	// useful in case we start a bar with a chord
	vector<unsigned> firstChar(tokens.size()-numErasedOttTokens, 0);
	vector<unsigned> firstCharOffset(tokens.size()-numErasedOttTokens, 1); // this is zeroed in case of a rhythm staff with durations only
	int midiNote, naturalScaleNote; // the second is for the score
	int ottava = 0;
	// boolean used to set the values to isSlurred vector
	bool slurStarted = false;
	// a counter for the number of notes in each chord
	chordNotesCounter = 0;
	unsigned index1 = 0, index2 = 0; // variables to index various data
	// create an unpopullated vector of vector of pairs of the notes as MIDI and natural scale
	// after all vectors with known size and all single variables have been created
	//vector<vector<std::pair<int, int>>> notePairs;
	vector<vector<intPair>> notePairs;
	// then iterate over all the tokens and parse them to store the notes
	for (i = 0; i < (int)tokens.size(); i++) {
		if (tokens.at(i).compare("\\ottava") == 0 || tokens.at(i).compare("\\ott") == 0) {
			// tests whether a digit follows \ottava have been made above
			// so we can now safely query tokens.at(i+1) and extract the digit that follows
			ottava = stoi(tokens.at(i+1));
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
			i--;
			continue;
		}
		// ignore curly brackets that belong to \tuplets
		if (startsWith(tokens.at(i), "{")) tokens.at(i) = tokens.at(i).substr(1);
		if (endsWith(tokens.at(i), "}")) tokens.at(i) = tokens.at(i).substr(0, tokens.at(i).size()-1);
		firstChar.at(i) = 0;
		// first check if the token is a comment so we exit the loop
		if (startsWith(tokens.at(i), "%")) continue;
		foundNotes.at(i) = false;
		transposedOctaves.at(index1) = 0;
		transposedAccidentals.at(index1) = 0;
		// the first element of each token is the note
		// so we first check for it in a separate loop
		// and then we check for the rest of the stuff
		for (j = 0; j < 8; j++) {
			if (tokens.at(i).at(firstChar.at(i)) == char(60)) { // <
				chordStarted = true;
				beginningOfChords.at(i) = 1;
				firstChordNote = true;
				firstChar.at(i) = 1;
			}
			if (tokens.at(i).at(firstChar.at(i)) == noteChars[j]) {
				midiNote = -1;
				if (j < 7) {
					midiNote = midiNotes[j];
					// we start one octave below the middle C
					midiNote += 48;
					naturalScaleNote = j;
					if (instruments.at(lastInstrumentIndex).isRhythm()) {
						midiNote = 59;
						naturalScaleNote = 6;
					}
					else if (instruments.at(lastInstrumentIndex).getTransposition() != 0) {
						int transposedMidiNote = midiNotes[j] + instruments.at(lastInstrumentIndex).getTransposition();
						// testing against the midiNote it is easier to determine whether we need to add an accidental
						if (transposedMidiNote < 0) {
							transposedMidiNote = 12 + midiNotes[j] + instruments.at(lastInstrumentIndex).getTransposition();
						}
						else if (transposedMidiNote > 11) {
							transposedMidiNote %= 12;
						}
						if (midiNotes[j] < 4) {
							transposedOctaves.at(index1) -= 1;
						}
						if (transposedMidiNote > 4 && (transposedMidiNote % 2) == 0) {
							transposedAccidentals.at(index1) = 2;
						}
						else if (transposedMidiNote <= 4 && (transposedMidiNote % 2) == 1) {
							transposedAccidentals.at(index1) = 2;
						}
						naturalScaleNote = distance(midiNotes, find(begin(midiNotes), end(midiNotes), (transposedMidiNote-(transposedAccidentals.at(index1)/2))));
					}
				}
				else {
					// the last element of the noteChars array is the rest
					midiNote = naturalScaleNote = -1;
				}
			storeNote:
				if (!chordStarted || (chordStarted && firstChordNote)) {
					// create a new vector for each single note or a group of notes of a chord
					//std::pair p = std::make_pair(midiNote, naturalScaleNote);
					intPair p;
					p.first = midiNote;
					p.second = naturalScaleNote;
					//std::vector<std::pair<int, int>> aVector(1, p);
					std::vector<intPair> aVector(1, p);
					notePairs.push_back(std::move(aVector));
					verticalNotesIndexes.at(i) = i;
					chordNotesIndexes.at(i) = 0;
					chordNotesCounter = 0;
                    notesCounter.at(i)++;
					dotIndexes.at(index2) = 0;
					glissandiIndexes.at(index2) = 0;
					midiGlissDurs.at(index2) = 0;
					midiDynamicsRampDurs.at(index2) = 0;
					pitchBendVals.at(index2) = 0;
					//textIndexesLocal.at(index2) = 0;
					ottavas.at(index2) = ottava;
					index2++;
					if (firstChordNote) firstChordNote = false;
				}
				else if (chordStarted && !firstChordNote) {
					// if we have a chord, push this note to the current vector
					intPair p;
					p.first = midiNote;
					p.second = naturalScaleNote;
					//notePairs.back().push_back(std::make_pair(midiNote, naturalScaleNote));
					notePairs.back().push_back(p);
					// a -1 will be filtered out further down in the code
					verticalNotesIndexes.at(index1) = -1;
					// increment the counter of the chord notes
					// so we can set the index for each note in a chord properly
					chordNotesCounter++;
					chordNotesIndexes.at(index1) += chordNotesCounter;
                    notesCounter.at(index1)++;
				}
				foundNotes.at(i) = true;
				break;
			}
		}
		if (!foundNotes.at(i)) {
			if (instruments.at(lastInstrumentIndex).isRhythm()) {
				// to be able to write a duration without a note, for rhythm staffs
				// we need to check if the first character is a number
				int dur = 0;
				if (tokens.at(i).size() > 1) {
					if (isdigit(tokens.at(i).at(0)) && isdigit(tokens.at(i).at(1))) {
						dur = int(tokens.at(i).at(0)+tokens.at(i).at(1));
					}
					else if (isdigit(tokens.at(i).at(0))) {
						dur = int(tokens.at(i).at(0));
					}
				}
				else if (isdigit(tokens.at(i).at(0))) {
					dur = int(tokens.at(i).at(0));
				}
				if (std::find(std::begin(dursArr), std::end(dursArr), dur) == std::end(dursArr)) {
					midiNote = 59;
					naturalScaleNote = 6;
					// we need to assign 0 to firstCharOffset which defaults to 1
					// because further down it is added to firstChar
					// but, in this case, we want to check the token from its beginning, index 0
					firstCharOffset.at(i) = 0;
					goto storeNote;
				}
			}
		}
		else {
			if (chordStarted && tokens.at(i).find(">") != string::npos) {
				chordStarted = false;
				firstChordNote = false;
				endingOfChords.at(i) = 1;
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
		if (instruments.at(lastInstrumentIndex).isRhythm()) {
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
	for (i = 0; i < (int)tokens.size(); i++) {
		// check if we have a comment, so we exit the loop
		if (startsWith(tokens.at(i), "%")) break;
		verticalNoteIndex = 0;
		for (int k = 0; k <= i; k++) {
			// loop till we find a -1, which means that this token is a chord note, but not the first one
			if (verticalNotesIndexes.at(k) < 0) continue;
			verticalNoteIndex = k;
		}
		// create an empty entry for every note/chord
		if (textIndexesLocal.size() <= verticalNoteIndex) {
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
	for (i = 0; i < (int)tokens.size(); i++) {
		// again, check if we have a comment, so we exit the loop
		if (startsWith(tokens.at(i), "%")) break;
		verticalNoteIndex = 0;
		for (int k = 0; k <= i; k++) {
			// loop till we find a -1, which means that this token is a chord note, but not the first one
			if (verticalNotesIndexes.at(k) < 0) continue;
			verticalNoteIndex = k;
		}
	}
	for (i = 0; i < (int)tokens.size(); i++) {
		// again, check if we have a comment, so we exit the loop
		if (startsWith(tokens.at(i), "%")) break;
		verticalNoteIndex = 0;
		for (int k = 0; k <= i; k++) {
			// loop till we find a -1, which means that this token is a chord note, but not the first one
			if (verticalNotesIndexes.at(k) < 0) continue;
			verticalNoteIndex = k;
		}
		for (j = 0; j < (int)tokens.at(i).size(); j++) {
			// ^ or _ for adding text above or below the note
			if ((tokens.at(i).at(j) == char(94)) || (tokens.at(i).at(j) == char(95))) {
				if (j > 0) {
					if (tokens.at(i).at(j-1) == '-') foundArticulation = true;
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
					texts.push_back(tokens.at(i).substr(openQuote, closeQuote-openQuote));
					//if (tokens.at(i).at(j) == char(94)) {
					//	textIndexesLocal.at(verticalNoteIndex) = 1;
					//}
					//else if (tokens.at(i).at(j) == char(95)) {
					//	textIndexesLocal.at(verticalNoteIndex) = -1;
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
	// lastly, store articulation symbols, as these have to be allocated dynamically too
	vector<vector<int>> articulationIndexes;
	for (i = 0; i < (int)tokens.size(); i++) {
		foundArticulation = false;
		unsigned firstArticulIndex = 0;
		verticalNoteIndex = -1;
		for (j = 0; j <= i; j++) {
			// loop till we find a -1, which means that this token is a chord note, but not the first one
			if (verticalNotesIndexes.at(j) < 0) continue;
			verticalNoteIndex++;
		}
		if ((int)articulationIndexes.size() <= verticalNoteIndex) {
			articulationIndexes.push_back({0});
		}
		for (j = 0; j < (int)tokens.at(i).size(); j++) {
			if (tokens.at(i).at(j) == char(45)) { // - for articulation symbols
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
				int articulChar = char(tokens.at(i).at(j+1));
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
				}
				if (articulationIndexes.at(verticalNoteIndex).size() == 1 && articulationIndexes.at(verticalNoteIndex).at(0) == 0) {
					articulationIndexes.at(verticalNoteIndex).at(0) = articulIndex;
				}
				else {
					articulationIndexes.at(verticalNoteIndex).push_back(articulIndex);
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

	// we are now done with dynamically allocating memory, so we can move on to the rest of the data
	// we can now create more variables without worrying about memory
	vector<int> foundAccidentals(tokens.size(), 0);
	float accidental = 0.0;
	// various counters
	unsigned numDurs = 0;
	unsigned numDynamics = 0;
	unsigned dynamicsRampCounter = 0;
	unsigned slursCounter = 0;
	for (i = 0; i < (int)tokens.size(); i++) {
		verticalNoteIndex = -1;
		for (j = 0; j <= i; j++) {
			// loop till we find a -1, which means that this token is a chord note, but not the first one
			if (verticalNotesIndexes.at(j) < 0) continue;
			verticalNoteIndex++;
		}
		// first check for accidentals, if any
		accidentalIndexes.at(i) = firstChar.at(i) + firstCharOffset.at(i);
		while (accidentalIndexes.at(i) < tokens.at(i).size()) {
			if (tokens.at(i).at(accidentalIndexes.at(i)) == char(101)) { // 101 is "e"
				if (accidentalIndexes.at(i) < tokens.at(i).size()-1) {
					// if the character after "e" is "s" or "h" we have an accidental
					if (tokens.at(i).at(accidentalIndexes.at(i)+1) == char(115)) { // 115 is "s"
						accidental -= 1.0; // in which case we subtract one semitone
						foundAccidentals.at(i) = 1;
					}
					else if (tokens.at(i).at(accidentalIndexes.at(i)+1) == char(104)) { // 104 is "h"
						accidental -= 0.5;
						foundAccidentals.at(i) = 1;
					}
				}
			}
			else if (tokens.at(i).at(accidentalIndexes.at(i)) == char(105)) { // 105 is "i"
				if (accidentalIndexes.at(i) < tokens.at(i).size()-1) {
					if (tokens.at(i).at(accidentalIndexes.at(i)+1) == char(115)) { // 115 is "s"
						accidental += 1.0; // in which case we add one semitone
						foundAccidentals.at(i) = 1;
					}
					else if (tokens.at(i).at(accidentalIndexes.at(i)+1) == char(104)) { // 104 is "h"
						accidental += 0.5;
						foundAccidentals.at(i) = 1;
					}
				}
			}
			// we ignore "s" and "h" as we have checked them above
			else if (tokens.at(i).at(accidentalIndexes.at(i)) == char(115) || tokens.at(i).at(accidentalIndexes.at(i)) == char(104)) {
				accidentalIndexes.at(i)++;
				continue;
			}
			// when the accidentals characters are over we move on
			else {
				break;
			}
			accidentalIndexes.at(i)++;
		}
		if (foundAccidentals.at(i)) {
			notesData.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) += accidental;
			midiNotesData.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) += (int)accidental;
			// accidentals can go up to a whole tone, which is accidental=2.0
			// but in case it's one and a half tone, one tone is already added above
			// so we need the half tone only, which we get with accidental-(int)accidental
			pitchBendVals.at(verticalNoteIndex) = (abs(accidental)-abs((int)accidental));
			if (accidental < 0.0) pitchBendVals.at(verticalNoteIndex) *= -1.0;
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
			accidentalsForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) = mapped;
			//accidentalsForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) += transposedAccidentals.at(i);
			// go back one character because of the last accidentalIndexes.at(i)++ above this if chunk
			accidentalIndexes.at(i)--;
		}
		else {
			accidentalsForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) = -1;
		}
		// add the accidentals based on transposition (if set)
		if (accidentalsForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) == -1) {
			// if we have no accidental, add the transposition to the natural sign indexes with 4
			accidentalsForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) = 4 + transposedAccidentals.at(i);
		}
		else {
			accidentalsForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) += transposedAccidentals.at(i);
		}
		// if the transposed accidental results in natural, assign -1 instead of 4 so as to not display the natural sign
		// if the natural sign is needed to be displayed, this will be taken care of at the end of the last loop
		// that iterates through the tokens of the string we parse in this function
		if (accidentalsForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) == 4) {
			accidentalsForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) = -1;
		}
		accidental = 0;
	}
	
	vector<int> foundDynamics(tokens.size(), 0);
	vector<int> foundOctaves(tokens.size(), 0);
	bool dynAtFirstNote = false;
	unsigned beginningOfChordIndex = 0;
	bool tokenInsideChord = false;
	int prevScoreDynamic = -1;
	int quotesCounter = 0; // so we can ignore anything that is passed as text
	// now check for the rest of the characters of the token, so start with j = accIndex
	for (i = 0; i < (int)tokens.size(); i++) {
		foundArticulation = false;
		verticalNoteIndex = -1;
		for (j = 0; j <= i; j++) {
			// loop till we find a -1, which means that this token is a chord note, but not the first one
			if (verticalNotesIndexes.at(j) < 0) continue;
			verticalNoteIndex++;
		}
		if (beginningOfChords.at(i)) {
			beginningOfChordIndex = i;
			tokenInsideChord = true;
		}
		// first check for octaves
		for (j = (int)accidentalIndexes.at(i); j < (int)tokens.at(i).size(); j++) {
			if (int(tokens.at(i).at(j)) == 39) {
				if (!instruments.at(lastInstrumentIndex).isRhythm()) {
					notesData.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) += 12;
					midiNotesData.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) += 12;
					octavesForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i))++;
				}
				foundOctaves.at(i) = 1;
			}
			else if (int(tokens.at(i).at(j)) == 44) {
				if (!instruments.at(lastInstrumentIndex).isRhythm()) {
					notesData.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) -= 12;
					midiNotesData.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) -= 12;
					octavesForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i))--;
				}
				foundOctaves.at(i) = 1;
			}
		}
		// then check for the rest of the characters
		for (j = (int)accidentalIndexes.at(i); j < (int)tokens.at(i).size(); j++) {
			// we don't check inside chords, as the characters we look for in this loop
			// are not permitted inside chords, so we check only at the last chord note
			// verticalNotesIndexes is tokens.size() + 1 so it's safe to test against i + 1 here
			if (verticalNotesIndexes.at(i+1) == -1) break;
			if (int(tokens.at(i).at(j)) == 34 && quotesCounter >= 1) {
				quotesCounter++;
				if (quotesCounter > 2) quotesCounter = 0;
			}
			if (quotesCounter > 0) continue; // if we're inside text, ignore
			if (isdigit(tokens.at(i).at(j))) {
				// assemble the value from its ASCII characters
				tempDur = tempDur * 10 + int(tokens.at(i).at(j)) - 48;
				if (verticalNoteIndex == 0) {
					dynAtFirstNote = true;
				}
			}
			
			else if (tokens.at(i).at(j) == char(92)) { // back slash
				index2 = j;
				int dynamic = 0;
				bool foundDynamicLocal = false;
				bool foundGlissandoLocal = false;
				// loop till you find all dynamics or other commands
				while (index2 < tokens.at(i).size()) {
					if (index2+1 == tokens.at(i).size()) {
						goto foundDynamicTest;
					}
					if (tokens.at(i).at(index2+1) == char(102)) { // f for forte
						if (mezzo) {
							dynamic++;
							mezzo = false;
						}
						else {
							dynamic += 3;
						}
						dynamicsIndexes.at(verticalNoteIndex) = verticalNoteIndex;
						dynamicsForScoreIndexes.at(verticalNoteIndex) = verticalNoteIndex;
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							dynamicsRampEnd.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampEndForScore.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampIndexes.at(dynamicsRampCounter-1).second = verticalNoteIndex;
						}
						foundDynamics.at(i) = 1;
						foundDynamicLocal = true;
					}
					else if (tokens.at(i).at(index2+1) == char(112)) { // p for piano
						if (mezzo) {
							dynamic--;
							mezzo = false;
						}
						else {
							dynamic -= 3;
						}
						dynamicsIndexes.at(verticalNoteIndex) = verticalNoteIndex;
						dynamicsForScoreIndexes.at(verticalNoteIndex) = verticalNoteIndex;
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							dynamicsRampEnd.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampEndForScore.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampIndexes.at(dynamicsRampCounter-1).second = verticalNoteIndex;
						}
						foundDynamics.at(i) = 1;
						foundDynamicLocal = true;
					}
					else if (tokens.at(i).at(index2+1) == char(109)) { // m for mezzo
						mezzo = true;
						//if (dynamicsRampStarted) {
						//	dynamicsRampStarted = false;
						//	dynamicsRampEnd.at(verticalNoteIndex) = verticalNoteIndex;
						//	dynamicsRampEndForScore.at(verticalNoteIndex) = verticalNoteIndex;
						//	dynamicsRampIndexes.at(dynamicsRampCounter-1).second = verticalNoteIndex;
						//	if (dynamicsRampStart.at(verticalNoteIndex) == dynamicsRampEnd.at(verticalNoteIndex)) {
						//		std::pair<int, string> p = std::make_pair(3, "can't start and end a crescendo/diminuendo on the same note");
						//		return p;
						//	}
						//}
					}
					else if (tokens.at(i).at(index2+1) == char(60)) { // <
						if (!dynamicsRampStarted) {
							dynamicsRampStarted = true;
							dynamicsRampStart.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampStartForScore.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampDirForScore.at(verticalNoteIndex) = 1;
							dynamicsRampIndexes.at(dynamicsRampCounter).first = verticalNoteIndex;
							dynamicsRampCounter++;
						}
					}
					else if (tokens.at(i).at(index2+1) == char(62)) { // >
						if (!dynamicsRampStarted) {
							dynamicsRampStarted = true;
							dynamicsRampStart.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampStartForScore.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampDirForScore.at(verticalNoteIndex) = 0;
							dynamicsRampIndexes.at(dynamicsRampCounter).first = verticalNoteIndex;
							dynamicsRampCounter++;
						}
					}
					else if (tokens.at(i).at(index2+1) == char(33)) { // exclamation mark
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							dynamicsRampEnd.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampEndForScore.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampIndexes.at(dynamicsRampCounter-1).second = verticalNoteIndex;
						}
					}
					else if (tokens.at(i).at(index2+1) == char(103)) { // g for gliss
						if ((tokens.at(i).substr(index2+1,5).compare("gliss") == 0) ||
							(tokens.at(i).substr(index2+1,9).compare("glissando") == 0)) {
							glissandiIndexes.at(verticalNoteIndex) = 1;
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
							dynamicsData.at(verticalNoteIndex) = scaledDynamic;
							// convert the range MINDB to 100 to indexes from 0 to 7
							// subtract the minimum dB value from the dynamic, multipled by (7/dB-range)
							// add 0.5 and get the int to round the float properly
							int dynamicForScore = (int)((((float)dynamicsData.at(verticalNoteIndex)-(float)MINDB)*(7.0/(100.0-(float)MINDB)))+0.5);
							if (dynamicForScore != prevScoreDynamic) {
								dynamicsForScore.at(verticalNoteIndex) = dynamicForScore;
								prevScoreDynamic = dynamicForScore;
							}
							else {
								// if it's the same dynamic, we must invalidate this index
								dynamicsForScoreIndexes.at(verticalNoteIndex) = -1;
							}
							numDynamics++;
							// to get the dynamics in MIDI velocity values, we map ppp to 16, and fff to 127
							// based on this website https://www.hedsound.com/p/midi-velocity-db-dynamics-db-and.html
							// first find the index of the dynamic value in the array {-9, -6, -3, -1, 1, 3, 6, 9}
							int midiVelIndex = std::distance(dynsArr, std::find(dynsArr, dynsArr+8, dynamic));
							midiVels.at(verticalNoteIndex) = (midiVelIndex+1)*16;
							if (midiVels.at(verticalNoteIndex) > 127) midiVels.at(verticalNoteIndex) = 127;
						}
						break;
					}
					index2++;
				}
				j = index2;
			}
			
			// . for dotted note, which is also used for staccato, hence the second test against 45 (hyphen)
			else if (tokens.at(i).at(j) == char(46) && tokens.at(i).at(j-1) != char(45)) {
				dotIndexes.at(verticalNoteIndex) = 1;
				dotsCounter.at(verticalNoteIndex)++;
			}

			else if (tokens.at(i).at(j) == char(40)) { // ( for beginning of slur
				slurStarted = true;
				slurBeginningsIndexes.at(verticalNoteIndex) = verticalNoteIndex;
				//slurIndexes.at(verticalNoteIndex).first = verticalNoteIndex;
				slursCounter++;
			}

			else if (tokens.at(i).at(j) == char(41)) { // ) for end of slur
				slurStarted = false;
				slurEndingsIndexes.at(verticalNoteIndex) = verticalNoteIndex;
				//slurIndexes.at(verticalNoteIndex).second = verticalNoteIndex;
			}

			else if (tokens.at(i).at(j) == char(126)) { // ~ for tie
				tieIndexes.at(verticalNoteIndex) = verticalNoteIndex;
			}

			// check for _ or ^ and make sure that the previous character is not -
			else if ((int(tokens.at(i).at(j)) == 94 || int(tokens.at(i).at(j)) == 95) && int(tokens.at(i).at(j-1)) != 45) {
				quotesCounter++;
			}
		}
		// add the extracted octave with \ottava
		octavesForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) -= ottavas.at(verticalNoteIndex);
		// store durations at the end of each token only if tempDur is greater than 0
		if (tempDur > 0) {
			dursData.at(verticalNoteIndex) = tempDur;
			durIndexes.at(verticalNoteIndex) = verticalNoteIndex;
			numDurs++;
			if (tokenInsideChord) {
				for (int k = (int)beginningOfChordIndex; k < verticalNoteIndex; k++) {
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
		int thisNote = notesForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i));
		int thisOctave = octavesForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i));
		// do a reverse loop to see if the last same note had its accidental corrected
		bool foundSameNote = false;
		// use ints instead of j (we are inside a loop using i) because j is unsigned
		// and a backwards loop won't work as it will wrap around instead of going below 0
		if (accidentalsForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) == -1) {
			for (int k = (int)verticalNoteIndex-1; k >= 0; k--) {
				for (int l = (int)notesForScore.at(k).size()-1; l >= 0; l--) {
					if (notesForScore.at(k).at(l) == thisNote && ((correctOnSameOctaveOnly && octavesForScore.at(k).at(l) == thisOctave) || !correctOnSameOctaveOnly)) {
						if (accidentalsForScore.at(k).at(l) > -1 && accidentalsForScore.at(k).at(l) != 4) {
							accidentalsForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) = 4;
							foundSameNote = true;
						}
						else {
							accidentalsForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) = -1;
							foundSameNote = true;
						}
						break;
					}
				}
				if (foundSameNote) break;
			}
			if (!foundSameNote) accidentalsForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) = -1;
		}
		if (slurStarted) {
			isSlurred.at(verticalNoteIndex) = true;
		}
	}

	if (dynamicsRampStarted) {
		dynamicsRampIndexes.at(dynamicsRampCounter-1).second = verticalNoteIndex;
	}

	// fill in possible empty slots of durations
	for (i = 0; i < numNotesVertical; i++) {
		if (i != durIndexes.at(i)) {
			dursData.at(i) = dursData.at(i-1);
		}
	}

	// fill in possible empty slots of dynamics
	//if (numDynamics == 0) {
	//	for (i = 0; i < numNotesVertical; i++) {
	//		// if this is not the very first bar get the last value of the previous bar
	//		if (loopData.size() > 0) {
	//			int prevBar = getPrevBarIndex();
	//			// get the last stored dynamic
	//			cout << "trying to access " << i << " while dyns data and midiVels have " << numNotesVertical << " elements\n";
	//			dynamicsData.at(i) = instruments.at(lastInstrumentIndex).dynamics.at(prevBar).back();
	//			// do the same for the MIDI velocities
	//			midiVels.at(i) = instruments.at(lastInstrumentIndex).midiVels.at(prevBar).back();
	//		}
	//		else {
	//			// fill in a default value of halfway from min to max dB
	//			dynamicsData.at(i) = 100-((100-MINDB)/2);
	//			midiVels.at(i) = 72; // half way between mp and mf in MIDI velocity
	//		}
	//	}
	//	cout << "if in dyns done\n";
	//}
	//else {
	//	for (i = 0; i < numNotesVertical; i++) {
	//		// if a dynamic has not been stored, fill in the last
	//		// stored dynamic in this empty slot
	//		if (i != dynamicsIndexes.at(i)) {
	//			// we don't want to add data for the score, only for the sequencer
	//			if (i > 0) {
	//				dynamicsData.at(i) =  dynamicsData.at(i-1);
	//				midiVels.at(i) = midiVels.at(i-1);
	//			}
	//			else {
	//				// fill in a default value of halfway from min to max dB
	//				dynamicsData.at(i) = 100-((100-MINDB)/2);
	//				midiVels.at(i) = 72; // half way between mp and mf in MIDI velocity
	//			}
	//		}
	//	}
	//	cout << "else in dyns done\n";
	//}
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
			// of the current note (retrieved by the durs vector)
			else {
				dynamicsRampData.at(j) = dynamicsData.at(j+1);
			}
		}
	}
	// get the tempo of the minimum duration
	int barIndex = getLastBarIndex();
	if (numerator.find(barIndex) == numerator.end()) {
		numerator.at(barIndex) = denominator.at(barIndex) = 4;
	}
	int dursAccum = 0;
	int diff = 0; // this is the difference between the expected and the actual sum of tuplets
	int scoreDur; // durations for score should not be affected by tuplet calculations
	int halfDur; // for halving for every dot
	// convert durations to number of beats with respect to minimum duration
	for (i = 0; i < numNotesVertical; i++) {
		dursData.at(i) = MINDUR / dursData.at(i);
		halfDur = dursData.at(i) / 2;
		if (dotIndexes.at(i) == 1) {
			for (j = 0; j < dotsCounter.at(i); j++) {
				dursData.at(i) += halfDur;
				halfDur /= 2;
			}
		}
		scoreDur = dursData.at(i);
		for (auto it = tupStartStop.begin(); it != tupStartStop.end(); ++it) {
			if (i >= it->second.first && i <= it->second.second) {
				// check if truncated sum is less than the expected sum
				if (i == it->second.first) {
					int tupDurAccum = 0;
					int expectedSum = 0;
					for (j = it->second.first; j <= it->second.second; j++) {
						tupDurAccum += (dursData.at(i) * tupRatios.at(it->first).second / tupRatios.at(it->first).first);
						expectedSum += dursData.at(i);
					}
					expectedSum = expectedSum * tupRatios.at(it->first).second / tupRatios.at(it->first).first;
					diff = expectedSum - tupDurAccum;
				}
				dursData.at(i) = dursData.at(i) * tupRatios.at(it->first).second / tupRatios.at(it->first).first;
				if (diff > 0) {
					dursData.at(i)++;
					diff--;
				}
			}
			//break;
		}
		dursAccum += dursData.at(i);
		dursForScore.at(i) = scoreDur;
		dursDataWithoutSlurs.at(i) = dursData.at(i);
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
	map<string, int>::iterator it = instrumentIndexes.find(lastInstrument);
	int thisInstIndex = it->second;
	instruments.at(thisInstIndex).notes[barIndex] = std::move(notesData);
	instruments.at(thisInstIndex).midiNotes[barIndex] = std::move(midiNotesData);
	instruments.at(thisInstIndex).durs[barIndex] = std::move(dursData);
	instruments.at(thisInstIndex).dursWithoutSlurs[barIndex] = std::move(dursDataWithoutSlurs);
	instruments.at(thisInstIndex).midiDursWithoutSlurs[barIndex] = std::move(midiDursDataWithoutSlurs);
	instruments.at(thisInstIndex).dynamics[barIndex] = std::move(dynamicsData);
	instruments.at(thisInstIndex).midiVels[barIndex] = std::move(midiVels);
	instruments.at(thisInstIndex).pitchBendVals[barIndex] = std::move(pitchBendVals);
	instruments.at(thisInstIndex).dynamicsRamps[barIndex] = std::move(dynamicsRampData);
	instruments.at(thisInstIndex).glissandi[barIndex] = std::move(glissandiIndexes);
	instruments.at(thisInstIndex).midiGlissDurs[barIndex] = std::move(midiGlissDurs);
	instruments.at(thisInstIndex).midiDynamicsRampDurs[barIndex] = std::move(midiDynamicsRampDurs);
	instruments.at(thisInstIndex).articulations[barIndex] = std::move(articulationIndexes);
	instruments.at(thisInstIndex).midiArticulationVals[barIndex] = std::move(midiArticulationVals);
	instruments.at(thisInstIndex).isSlurred[barIndex] = std::move(isSlurred);
	instruments.at(thisInstIndex).text[barIndex] = std::move(texts);
	instruments.at(thisInstIndex).textIndexes[barIndex] = std::move(textIndexesLocal);
	instruments.at(thisInstIndex).slurIndexes[barIndex] = std::move(slurIndexes);
	instruments.at(thisInstIndex).tieIndexes[barIndex] = std::move(tieIndexes);
	instruments.at(thisInstIndex).scoreNotes[barIndex] = std::move(notesForScore);
	instruments.at(thisInstIndex).scoreDurs[barIndex] = std::move(dursForScore);
	instruments.at(thisInstIndex).scoreDotIndexes[barIndex] = std::move(dotIndexes);
	instruments.at(thisInstIndex).scoreDotsCounter[barIndex] = std::move(dotsCounter);
	instruments.at(thisInstIndex).scoreAccidentals[barIndex] = std::move(accidentalsForScore);
	instruments.at(thisInstIndex).scoreNaturalSignsNotWritten[barIndex] = std::move(naturalSignsNotWrittenForScore);
	instruments.at(thisInstIndex).scoreOctaves[barIndex] = std::move(octavesForScore);
	instruments.at(thisInstIndex).scoreOttavas[barIndex] = std::move(ottavas);
	instruments.at(thisInstIndex).scoreGlissandi[barIndex] = std::move(glissandiIndexes);
	//instruments.at(thisInstIndex).scoreArticulations[barIndex] = std::move(articulationIndexes);
	instruments.at(thisInstIndex).scoreDynamics[barIndex] = std::move(dynamicsForScore);
	instruments.at(thisInstIndex).scoreDynamicsIndexes[barIndex] = std::move(dynamicsForScoreIndexes);
	instruments.at(thisInstIndex).scoreDynamicsRampStart[barIndex] = std::move(dynamicsRampStartForScore);
	instruments.at(thisInstIndex).scoreDynamicsRampEnd[barIndex] = std::move(dynamicsRampEndForScore);
	instruments.at(thisInstIndex).scoreDynamicsRampDir[barIndex] = std::move(dynamicsRampDirForScore);
	instruments.at(thisInstIndex).scoreTupRatios[barIndex] = std::move(tupRatios);
	instruments.at(thisInstIndex).scoreTupStartStop[barIndex] = std::move(tupStartStop);
	instruments.at(thisInstIndex).scoreTexts[barIndex] = std::move(textsForScore);
	instruments.at(thisInstIndex).isWholeBarSlurred[barIndex] = false;
	// in case we have no slurs, we must check if there is a slurred that is not closed in the previous bar
	// of if the whole previous bar is slurred
	if (slurIndexes.size() == 1 && slurIndexes.at(0).first == -1 && slurIndexes.at(0).second == -1) {
		int prevBar = getPrevBarIndex();
		if ((instruments.at(thisInstIndex).slurIndexes.at(prevBar).back().first > -1 && \
				instruments.at(thisInstIndex).slurIndexes.at(prevBar).back().second == -1) ||
				instruments.at(thisInstIndex).isWholeBarSlurred.at(prevBar)) {
			instruments.at(thisInstIndex).isWholeBarSlurred.at(barIndex) = true;
		}
	}
	instruments.at(thisInstIndex).passed = true;
}

//--------------------------------------------------------------
void ofApp::initializeInstrument(int index, string instName)
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
	instruments[maxIndex].setNotesFontSize(scoreFontSize, staffLinesDist);
	instruments[maxIndex].setNumBarsToDisplay(numBarsToDisplay);
	instruments[maxIndex].setScoreOrientation(1);
	numInstruments++;
	setScoreCoords();
	createFirstBar(maxIndex);
}

//--------------------------------------------------------------
void ofApp::createFirstBar(int instNdx)
{
	int barIndex = 0;
	if (instruments.size() == 1) {
		numerator[0] = denominator[0] = numBeats[0] = 4;
		BPMTempi[0] = 120;
		tempoBaseForScore[0] = 1;
		BPMDisplayHasDot[0] = false;
		BPMMultiplier[0] = 1;
		tempoMs[0] = 500;
		barIndex = storeNewBar("-1");
	}
	instruments[instNdx].createEmptyMelody(barIndex);
	instruments[instNdx].setScoreNotes(barIndex, denominator[0], numerator[0], numBeats[0],
			BPMTempi[0], tempoBaseForScore[0], BPMDisplayHasDot[0], BPMMultiplier[0]);
	instruments[instNdx].setNotePositions(0);
	loopData[0] = {0};
	calculateStaffPositions(0, false);
}

//--------------------------------------------------------------
void ofApp::updateTempoBeatsInsts(int barIndex)
{
	tempo[barIndex] = newTempo;
	numBeats[barIndex] = tempNumBeats;
}

//--------------------------------------------------------------
void ofApp::setScoreCoords()
{
	scoreXStartPnt = longestInstNameWidth + (blankSpace * 1.5);
	float staffLength = screenWidth - blankSpace - scoreXStartPnt;
	staffLength /= min(2, numBarsToDisplay);
	notesXStartPnt = 0;
	notesLength = staffLength - (staffLinesDist * NOTESXOFFSETCOEF);
	for (auto it = instruments.begin(); it != instruments.end(); ++it) {
		it->second.setStaffCoords(staffLength, staffLinesDist);
		it->second.setNoteCoords(notesLength, staffLinesDist, scoreFontSize);
	}
	// get the note width from the first instrument, as they all have the same size
	noteWidth = instruments[0].getNoteWidth();
	noteHeight = instruments[0].getNoteHeight();
}

//--------------------------------------------------------------
void ofApp::setScoreNotes(int barIndex)
{
	for (auto it = instruments.begin(); it != instruments.end(); ++it) {
		it->second.setScoreNotes(barIndex, numerator[barIndex],
				denominator[barIndex], numBeats[barIndex],
				BPMTempi[barIndex], tempoBaseForScore[barIndex],
				BPMDisplayHasDot[barIndex], BPMMultiplier[barIndex]);
	}
}

//--------------------------------------------------------------
void ofApp::setNotePositions(int barIndex)
{
	for (auto it = instruments.begin(); it != instruments.end(); ++it) {
		if (!it->second.getCopied(barIndex)) it->second.setNotePositions(barIndex);
	}
}

//--------------------------------------------------------------
void ofApp::setNotePositions(int barIndex, int numBars)
{
	for (auto it = instruments.begin(); it != instruments.end(); ++it) {
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
	for (auto it = instruments.begin(); it != instruments.end(); ++it) {
		if ((int)instruments.size() > numInstruments && it == instruments.begin()) continue;
		float maxPosition = it->second.getMaxYPos(bar);
		float minPosition = it->second.getMinYPos(bar);
		if (i == 0) { // used to be  && it->first == 0
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
	if (maxHeightSum >= allStaffDiffs || windowChanged) {
		allStaffDiffs = min(maxHeightSum, screenHeight / (float)instruments.size());
		float totalDiff = lastMaxPosition - firstMinPosition;
		if (totalDiff < screenHeight) {
			yStartPnt = (screenHeight / 2);
			yStartPnt -= (((allStaffDiffs + (staffLinesDist * 2)) * (numInstruments / 2)));
			yStartPnt -= (staffLinesDist * 2);
			if ((instruments.size() % 2) == 0) {
				yStartPnt += ((allStaffDiffs + (staffLinesDist * 2)) / 2); 
			}
			// make sure anything that sticks out of the staff doesn't go below 0 in the Y axis
			if ((yStartPnt - firstMinPosition) < 0) {
				yStartPnt += firstMinPosition;
			}
		}
		else {
			// if the full score exceeds the screen just place it 10 pixels below the top part of the window
			yStartPnt = firstMinPosition + 10;
		}
	}
}

//--------------------------------------------------------------
void ofApp::setScoreSizes()
{
	longestInstNameWidth = 0;
	for (auto it = instruments.begin(); it != instruments.end(); ++it) {
		it->second.setNotesFontSize(scoreFontSize, staffLinesDist);
		size_t nameWidth = instFont.stringWidth(it->second.getName());
		instNameWidths[it->first] = nameWidth;
		if ((int)nameWidth > longestInstNameWidth) {
			longestInstNameWidth = nameWidth;
		}
	}
	scoreXStartPnt = longestInstNameWidth + (blankSpace * 1.5);
	setScoreCoords();
	if (barsIndexes.size() > 0) {
		for (auto it = barsIndexes.begin(); it != barsIndexes.end(); ++it) {
			//setScoreCoords();
			for (auto it2 = instruments.begin(); it2 != instruments.end(); ++it2) {
				it2->second.setMeter(it->second, numerator[it->second], denominator[it->second], numBeats[it->second]);
				it2->second.setNotePositions(it->second);
			}
		}
	}
	//else {
	//	setScoreCoords();
	//}
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
void ofApp::windowResized(int w, int h)
{
	// hold the previous middle of screen value so the score is also properly updated
	int prevMiddleOfScreenX = middleOfScreenX;
	screenWidth = w;
	screenHeight = h;
	middleOfScreenX = screenWidth / 2;
	middleOfScreenY = screenHeight / 2;
	if (staffXOffset == prevMiddleOfScreenX) {
		staffXOffset = 0;
		staffWidth = screenWidth / 2;
	}
	else {
		staffXOffset = middleOfScreenX;
		staffWidth = screenWidth;
	}
	if (scoreYOffset > 0) scoreYOffset = (screenHeight / 2);
	setScoreSizes();
	calculateStaffPositions(lastBarIndex, true);
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg)
{

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo)
{

}
