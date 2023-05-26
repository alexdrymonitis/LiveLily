#include "scores.h"
#include "ofApp.h"
#include "limits.h"


Staff::Staff(float x, float y, float z, float dist, int numer, int denom) {
	numLines = 5;
  lineWidth = ((ofApp*)ofGetAppPtr())->lineWidth;
  // G, F, C clefs
  string clefSymsLocal[3] = {"&", "?", "B"};
  string meterSymsLocal[2] = {"c", "C"};
  for (int i = 0; i < 3; i++) {
    clefSyms[i] = clefSymsLocal[i];
  }
  for (int i = 0; i < 2; i++) {
    meterSyms[i] = meterSymsLocal[i];
  }
  clefIndex = 0;
	numerator = 4;
	denominator = 4;
  isLoopStart = false;
  isLoopEnd = false;
  isScoreEnd = false;
  meterIndex = 0;
	//setCoords(loopIndex, x, y, z, dist, fontSize, numer, denom);
}

void Staff::setClef(int clefIdx){
	clefIndex = clefIdx;
}

void Staff::setSize(int fontSize){
	Staff::fontSize = fontSize;
  notationFont.load("sonata.ttf", fontSize);
	for (int i = 0; i < 64; i++) {
		meterSymsWidths[i] = notationFont.stringWidth(ofToString(i));
	}
	meterSymsWidths[64] = notationFont.stringWidth(meterSyms[0]);
	meterSymsWidths[65] = notationFont.stringWidth(meterSyms[1]);
}

void Staff::setCoords(int loopIndex, float x, float y, float z,
											float dist, int numer, int denom) {
  xStartPnt = x;
  if ((int)yPnt.size() <= loopIndex) {
    yPnt.resize(loopIndex+1);
  }
  yPnt[loopIndex] = y;
  xEndPnt = z;
  yDist = dist;
  numerator = numer;
  denominator = denom;
  if ((numerator == 4) && (denominator == 4)) meterIndex = 0;
  else if ((numerator == 2) && (denominator == 2)) meterIndex = 1;
  else meterIndex = 2;
}

float Staff::getXPos() {
  float longestNum = 0;
  if ((numerator == 4) && (denominator == 4)) {
    if (longestNum < meterSymsWidths[64]) {
      longestNum = meterSymsWidths[64];
    }
  }
  else if ((numerator == 2) && (denominator == 2)) {
    if (longestNum < meterSymsWidths[65]) {
      longestNum = meterSymsWidths[65];
    }
  }
  else {
    if (meterSymsWidths[numerator] > longestNum) {
      longestNum = meterSymsWidths[numerator];
    }
    if (meterSymsWidths[denominator] > longestNum) {
      longestNum = meterSymsWidths[denominator];
    }
  }
  // below should be (yDist*3.5) like it's in meter.drawString()
  // in drawStaff() below, but we need a little bit more space
  // so we multiply by 6
  return xStartPnt + (yDist*6) + longestNum;
}

void Staff::drawStaff(int loopIndex) {
  ofSetColor(0);
  ofDrawLine(xStartPnt, yPnt[loopIndex]-(lineWidth/2), xStartPnt, (yPnt[loopIndex]+(4*yDist))+(lineWidth/2));
  ofDrawLine(xEndPnt, yPnt[loopIndex]-(lineWidth/2), xEndPnt, (yPnt[loopIndex]+(4*yDist))+(lineWidth/2));
  for (int i = 0; i < numLines; i++) {
    float yPntLocal = yPnt[loopIndex] + (i*yDist);
    ofDrawLine(xStartPnt, yPntLocal, xEndPnt, yPntLocal);
  }
  if (clefIndex < 3) {
    notationFont.drawString(clefSyms[clefIndex], xStartPnt+(yDist/2), yPnt[loopIndex]+(yDist*4)-(yDist*0.2));
  }
  else {
    float xPos = xStartPnt+(yDist*1.5);
    float yPos = yPnt[loopIndex] + yDist;
    for (int i = 0; i < 2; i++) {
      drawThickLine(xPos+(i*yDist), yPos, xPos+(i*yDist), yPos+(yDist*2), yDist*0.25);
    }
  }
  float numeratorY = yPnt[loopIndex]+yDist;
  float denominatorY = numeratorY + (yDist*2);
  if (meterIndex < 2) {
    notationFont.drawString(meterSyms[meterIndex], xStartPnt+(yDist*3.5), numeratorY+yDist);
  }
  else {
    notationFont.drawString(ofToString(numerator), xStartPnt+(yDist*3.5), numeratorY);
    notationFont.drawString(ofToString(denominator), xStartPnt+(yDist*3.5), denominatorY);
  }

  if (isLoopStart) drawLoopStart(loopIndex);
  if (isLoopEnd) drawLoopEnd(loopIndex);
  if (isScoreEnd) drawScoreEnd(loopIndex);

  denominatorY += (yPnt[loopIndex]+(yDist*2)+(yDist/4));
}

void Staff::drawLoopStart(int loopIndex){
  // the " + (yDist*3.5)" is taken from drawStaff() where the meter is drawn
  float xPos = xStartPnt + (yDist*3.5);
  if (meterIndex < 2) {
    xPos += notationFont.stringWidth(meterSyms[meterIndex]);
  }
  else {
    float maxWidth = max(notationFont.stringWidth(ofToString(numerator)), notationFont.stringWidth(ofToString(denominator)));
    xPos += maxWidth;
  }
  // a little bit of offset so that symbols are not clamped together
  xPos += (yDist * 0.5);
  ofDrawLine(xPos, yPnt[loopIndex],
             xPos, yPnt[loopIndex]+(yDist*4));
  ofDrawLine(xPos+(yDist*0.5), yPnt[loopIndex],
             xPos+(yDist*0.5), yPnt[loopIndex]+(yDist*4));
  ofDrawCircle(xPos+yDist, yPnt[loopIndex]+(yDist*1.5), yDist*0.2);
  ofDrawCircle(xPos+yDist, yPnt[loopIndex]+(yDist*2.5), yDist*0.2);
}

void Staff::drawLoopEnd(int loopIndex){
  ofDrawLine(xEndPnt-(yDist*0.5), yPnt[loopIndex],
             xEndPnt-(yDist*0.5), yPnt[loopIndex]+(yDist*4));
  ofDrawCircle(xEndPnt-yDist, yPnt[loopIndex]+(yDist*1.5), yDist*0.2);
  ofDrawCircle(xEndPnt-yDist, yPnt[loopIndex]+(yDist*2.5), yDist*0.2);
}

void Staff::drawScoreEnd(int loopIndex) {
  ofDrawLine(xEndPnt-(yDist*0.75), yPnt[loopIndex],
             xEndPnt-(yDist*0.75), yPnt[loopIndex]+(yDist*4));
  float x1 = xEndPnt-(yDist*0.125); // multiply by half of the value in the m.addVertex lines
  float y1 = yPnt[loopIndex]-(((ofApp*)ofGetAppPtr())->lineWidth/2);
  float x2 = xEndPnt-(yDist*0.125);
  float y2 = yPnt[loopIndex]+(yDist*4)+(((ofApp*)ofGetAppPtr())->lineWidth/2);
  drawThickLine(x1, y1, x2, y2, yDist*0.25);
}

void Staff::drawThickLine(float x1, float y1, float x2, float y2, float width) {
  // draw a thick line without affecting the thickness
  // of all the other lines
  // taken from
  // https://forum.openframeworks.cc/t/how-do-i-draw-lines-of-a-
  // reasonable-thickness-on-a-very-large-canvas-given-opengl-
  // constraints/30815/7
  ofPoint a;
  ofPoint b;
  a.x = x1;
  a.y = y1;
  b.x = x2;
  b.y = y2;
  ofPoint diff = (a - b).getNormalized();
  diff.rotate(90, ofPoint(0,0,1));
  ofMesh m;
  m.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);
  m.addVertex(a + diff * width);
  m.addVertex(a - diff * width);
  m.addVertex(b + diff * width);
  m.addVertex(b - diff * width);
  m.draw();
}

Staff::~Staff(){}

//--------------------------------------------------------------------------
Notes::Notes(int denom, int fontSize, int id){
  // whole and half (first symbol), and the rest of rests
  string restsSymsLocal[6] = {"î", "Î", "ä", "Å", "¨", "ô"};
  // ppp, pp, p, mp, mf, f, ff, fff
  string dynSymsLocal[8] = {"¸", "¹", "p", "P", "F", "f", "Ä", "ì"};
  // double flat, dummy 1.5 flat, flat, dummy half flat, natural,
  // dummy half sharp, sharp, dummy 1.5 sharp, double sharp
  string accSymsLocal[9] = {"º", "", "b", "", "n", "", "#", "", "Ü"};
  // whole note, half note, filled note, tail for upward stem, tail for downward stem
  string notesSymsLocal[5] = {"w", "ú", "ö", "j", "J"};
  // marcato, trill, tenuto, staccatissimo, accent, staccato
  string articulSymsLocal[6] = {"^", "Ù", "_", "à", ">", "."};
  for (int i = 0; i < 6; i++) {
    restsSyms[i] = restsSymsLocal[i];
  }
  for (int i = 0; i < 8; i++) {
    dynSyms[i] = dynSymsLocal[i];
  }
  for (int i = 0; i < 9; i++) {
    accSyms[i] = accSymsLocal[i];
  }
  for (int i = 0; i < 5; i++) {
    notesSyms[i] = notesSymsLocal[i];
  }
  for (int i = 0; i < 6; i++) {
    articulSyms[i] = articulSymsLocal[i];
  }
  octaveSym = "Ã";
  objID = id;
	beamsLineWidth = 2;
	denominator = denom;
	//beats = numBeats;
  animate = false;
  mute = false;
  whichNote = -1;
  lineWidth = ((ofApp*)ofGetAppPtr())->lineWidth;
  quarterSharp.load("quarter_sharp.png");
	setFontSize(fontSize);
}

void Notes::setFontSize(int fontSize){
	Notes::fontSize = fontSize;
  notationFont.load("sonata.ttf", fontSize);
	textFont.load("times-new-roman.ttf", fontSize/2);
  noteWidth = notationFont.stringWidth(notesSyms[2]);
  noteHeight = notationFont.stringHeight(notesSyms[2]);
	// store the widths and heights of dynamic and rest symbols
	// since these are called from a threaded class and calling stringWidth won't work
	for (int i = 0; i < 8; i++) {
		dynSymsWidths[i] = notationFont.stringWidth(dynSyms[i]);
		dynSymsHeights[i] = notationFont.stringHeight(dynSyms[i]);
	}
	for (int i = 0; i < 6; i++) {
		restsSymsWidths[i] = notationFont.stringWidth(restsSyms[i]);
		restsSymsHeights[i] = notationFont.stringHeight(restsSyms[i]);
	}
}

void Notes::setCoords(int bar, int loopIndex, float xStart, float yStart,
                      float xEnd, float staffLinesDist, int fontSize, int numBeats){
  if (bar > (int)xStartPnt.size()) {
    xStartPnt.resize(bar);
    yStartPnt.resize(bar);
    xEndPnt.resize(bar);
    xLength.resize(bar);
    maxYCoord.resize(bar);
    minYCoord.resize(bar);
    distBetweenBeats.resize(bar);
  }
  if ((int)xStartPnt.size() == bar) {
    xStartPnt.push_back(xStart);
    yStartPnt.resize(yStartPnt.size()+1);
    if ((int)yStartPnt[bar].size() <= loopIndex) {
      yStartPnt[bar].resize(loopIndex+1);
    }
  	yStartPnt[bar][loopIndex] = yStart;
  	xEndPnt.push_back(xEnd);
  	xLength.push_back(xEndPnt.back() - xStartPnt.back());
    maxYCoord.push_back(FLT_MIN);
    minYCoord.push_back(FLT_MAX);
    distBetweenBeats.push_back(0);
  }
  else {
    xStartPnt[bar] = xStart;
    if ((int)yStartPnt[bar].size() <= loopIndex) {
      yStartPnt[bar].resize(loopIndex+1);
    }
  	yStartPnt[bar][loopIndex] = yStart;
  	xEndPnt[bar] = xEnd;
  	xLength[bar] = xEndPnt[bar] - xStartPnt[bar];
    maxYCoord[bar] = FLT_MIN;
    minYCoord[bar] = FLT_MAX;
    distBetweenBeats[bar] = 0;
  }
  staffDist = staffLinesDist;
  halfStaffDist = staffDist / 2;
	stemHeight = (int)((float)staffDist * 3.5);
  middleOfStaff = yStartPnt[bar][loopIndex] + (halfStaffDist * 4);
  // minimum offset must be 3
  restYOffset = 3;
  int i = 35; // 10 is the minimum staff distance, so 35 the minimum font size
  while (i <= fontSize) {
    i += 5; // every five pixels we increment restYOffset by one
    restYOffset++;
  }
  // then check if the actual font size is closer to one step below
  // of what i has incremented to (e.g. i is 40 but fontSize is 37)
  int diff = i - fontSize;
  if (diff > 2) restYOffset--;
  beats = numBeats;
  setLength(bar, numBeats);
}

void Notes::setLength(int bar, int numBeats){
  distBetweenBeats[bar] = xLength[bar] / beats;
}

float Notes::getMaxYPos(int bar){
  return maxYCoord[bar];
}

float Notes::getMinYPos(int bar){
  return minYCoord[bar];
}

float Notes::getMinVsAnchor(int loopIndex, int bar){
  return minYCoord[bar] - yStartPnt[bar][loopIndex];
}

float Notes::getNoteWidth(){
  return notationFont.stringWidth(notesSyms[0]);
}

float Notes::getNoteHeight(){
  return notationFont.stringHeight(notesSyms[0]);
}

void Notes::setClef(int clefIdx){
	clefIndex = clefIdx;
}

void Notes::setAnimation(bool anim) {
  animate = anim;
}

void Notes::setActiveNote(int note) {
  whichNote = note;
}

void Notes::setNotes(int bar, int loopIndex, vector<int> durs, int numBeats) {
  for (unsigned i = 0; i < durs.size(); i++) {
    durations[bar][i] = durs[i];
  }
  beats = numBeats;
  setLength(bar, numBeats);
  setNotePositions(bar, loopIndex);
}

void Notes::setNotes(int bar, int loopIndex,
                     vector<vector<int>> notes,
                     vector<vector<int>> accidentals,
                     vector<vector<int>> octaves,
                     vector<int> durs, vector<int> dots,
                     vector<int>gliss, vector<int> articul,
                     vector<int> dyns, vector<int> dynIdx,
                     vector<int> dynRampStart, vector<int> dynRampEnd,
                     vector<int> dynRampDir,
                     vector<int> slurBegin,
                     vector<int> slurEnd,
										 vector<string> texts, vector<int> textIndexes,
										 int numBeats){
  // an instrument might not be initialized from the beginning so the bar
  // of its first notes might not be 0, and if we go back to a pattern with
  // a non-existing bar, the program will crash, so we fill in possible empty slots
  if (bar > (int)allNotes.size()) {
    allNotes.resize(bar);
    allAccidentals.resize(bar);
    allOctaves.resize(bar);
    durations.resize(bar);
		dotIndexes.resize(bar);
    allGlissandi.resize(bar);
    dynamics.resize(bar);
    dynamicsIndexes.resize(bar);
    dynamicsRampStart.resize(bar);
    dynamicsRampEnd.resize(bar);
    dynamicsRampDir.resize(bar);
    slurBeginnings.resize(bar);
    slurEndings.resize(bar);
		allTexts.resize(bar);
		allTextsIndexes.resize(bar);
    // vectors populated in setNotePositions()
    stemDirections.resize(bar);
    grouppedStemDirs.resize(bar);
    articulYPos.resize(bar);
    extraLinesDir.resize(bar);
    numExtraLines.resize(bar);
    extraLinesYPos.resize(bar);
    hasExtraLines.resize(bar);
    actualDurs.resize(bar);
    hasStem.resize(bar);
    stemsX.resize(bar);
    stemsY1.resize(bar);
    stemsY2.resize(bar);
    slurStartX.resize(bar);
    slurStartY.resize(bar);
    slurStopX.resize(bar);
    slurStopY.resize(bar);
    slurMiddleX1.resize(bar);
    slurMiddleY1.resize(bar);
    slurMiddleX2.resize(bar);
    slurMiddleY2.resize(bar);
    dynsXCoords.resize(bar);
    dynsYCoords.resize(bar);
    dynsRampStartXCoords.resize(bar);
    dynsRampEndXCoords.resize(bar);
    dynsRampStartYCoords.resize(bar);
    dynsRampEndYCoords.resize(bar);
    hasRests.resize(bar);
    maxNumLines.resize(bar);
    startPnts.resize(bar);
    grouppedStemDirsCoeff.resize(bar);
    beamsX.resize(bar);
    beamsIndexes.resize(bar);
    numNotesWithBeams.resize(bar);
    allBeamYCoords.resize(bar);
    numBeams.resize(bar);
		numTails.resize(bar);
    beamXCoords1.resize(bar);
    beamXCoords2.resize(bar);
    beamYCoords1.resize(bar);
    beamYCoords2.resize(bar);
    allNotesXCoords.resize(bar);
    allNotesYCoords1.resize(bar);
    allNotesYCoords2.resize(bar);
    allNotesMaxYPos.resize(bar);
    allNotesMinYPos.resize(bar);
    whichIdxBase.resize(bar);
    whichIdxEdge.resize(bar);
		allTextsXCoords.resize(bar);
		allTextsYCoords.resize(bar);
  }
  // every time we type a new melodic line the durations are checked and probably
  // changed to match the current shortest duration so setNotes() is called for
  // all Notes objects
  // in this case we must make sure we don't resize the vectors for concecutive calls
  // with the same bar
  if ((int)allNotes.size() == bar) {
    allNotes.push_back({{}});
    allAccidentals.push_back({{}});
    allOctaves.push_back({{}});
    allNotes.back().resize(notes.size());
    allAccidentals.back().resize(notes.size());
    allOctaves.back().resize(notes.size());
    durations.push_back({});
		dotIndexes.push_back({});
    allGlissandi.push_back({});
    allArticulations.push_back({});
    dynamics.push_back({});
    dynamicsIndexes.push_back({});
    dynamicsRampStart.push_back({});
    dynamicsRampEnd.push_back({});
    dynamicsRampDir.push_back({});
    slurBeginnings.push_back({});
    slurEndings.push_back({});
		allTexts.push_back({});
		allTextsIndexes.push_back({});
    for (unsigned i = 0; i < notes.size(); i++) {
      for (unsigned j = 0; j < notes[i].size(); j++) {
        allNotes[bar][i].push_back(notes[i][j]);
      }
      for (unsigned j = 0; j < accidentals[i].size(); j++) {
        allAccidentals[bar][i].push_back(accidentals[i][j]);
      }
      for (unsigned j = 0; j < octaves[i].size(); j++) {
        allOctaves[bar][i].push_back(octaves[i][j]);
      }
			allTextsIndexes[bar].push_back(textIndexes[i]);
      durations[bar].push_back(durs[i]);
			dotIndexes[bar].push_back(dots[i]);
      allGlissandi[bar].push_back(gliss[i]);
      allArticulations[bar].push_back(articul[i]);
    }
		// if we have a different clef we must offset the notes
		if (clefIndex != 0) {
			if (clefIndex == 1) {
				bool tacetLocal = false;
				for (unsigned i = 0; i < allNotes[bar].size(); i++) {
					if (!((allNotes[bar].size() == 1) && (allNotes[bar][i][0] == -1))) {
						for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
							allNotes[bar][i][j] -= 2;
							if (allNotes[bar][i][j] < 0) {
								allNotes[bar][i][j] = 7 + allNotes[bar][i][j];
							}
						}
					}
					else {
						tacetLocal = true;
					}
				}
				if (!tacetLocal) {
					for (unsigned i = 0; i < allOctaves[bar].size(); i++) {
						for (unsigned j = 0; j < allOctaves[bar][i].size(); j++) {
							allOctaves[bar][i][j] += 2;
						}
					}
				}
			}
		}
    // the rest of the vectors are filled with a default value even if there are none written
    // so it's easier to test the first value here and only if it's not -1 then write them
    if (dyns[0] != -1) {
      for (unsigned i = 0; i < dyns.size(); i++) {
        dynamics[bar].push_back(dyns[i]);
      }
    }
    if (dynIdx[0] != -1) {
      for (unsigned i = 0; i < dynIdx.size(); i++) {
        dynamicsIndexes[bar].push_back(dynIdx[i]);
      }
    }
    if (dynRampStart[0] != -1) {
      for (unsigned i = 0; i < dynRampStart.size(); i++) {
        dynamicsRampStart[bar].push_back(dynRampStart[i]);
      }
    }
    if (dynRampEnd[0] != -1) {
      for (unsigned i = 0; i < dynRampEnd.size(); i++) {
        dynamicsRampEnd[bar].push_back(dynRampEnd[i]);
      }
    }
    if (dynRampDir[0] != -1) {
      for (unsigned i = 0; i < dynRampDir.size(); i++) {
        dynamicsRampDir[bar].push_back(dynRampDir[i]);
      }
    }
    if (slurBegin[0] != -1) {
      for (unsigned i = 0; i < slurBegin.size(); i++) {
        slurBeginnings[bar].push_back(slurBegin[i]);
      }
    }
    if (slurEnd[0] != -1){
      for (unsigned i = 0; i < slurEnd.size(); i++) {
        slurEndings[bar].push_back(slurEnd[i]);
      }
    }
		if (texts.size() > 0) {
			for (unsigned i = 0; i < texts.size(); i++) {
				allTexts[bar].push_back(texts[i]);
			}
		}
  }
  // otherwise just write to this bar
  else {
    allNotes[bar].resize(notes.size());
    allAccidentals[bar].resize(accidentals.size());
    allOctaves[bar].resize(octaves.size());
    durations[bar].resize(durs.size());
		dotIndexes[bar].resize(dots.size());
    allGlissandi[bar].resize(gliss.size());
    allArticulations[bar].resize(articul.size());
    dynamics[bar].resize(dyns.size());
    dynamicsIndexes[bar].resize(dynIdx.size());
    dynamicsRampStart[bar].resize(dynRampStart.size());
    dynamicsRampEnd[bar].resize(dynRampEnd.size());
    dynamicsRampDir[bar].resize(dynRampDir.size());
    slurBeginnings[bar].resize(slurBegin.size());
    slurEndings[bar].resize(slurEnd.size());
		allTexts.resize(texts.size());
		allTextsIndexes.resize(textIndexes.size());
    for (unsigned i = 0; i < notes.size(); i++) {
      allNotes[bar][i].resize(notes[i].size());
      for (unsigned j = 0; j < notes[i].size(); j++) {
        allNotes[bar][i][j] = notes[i][j];
      }
    }
    for (unsigned i = 0; i < accidentals.size(); i++) {
      allAccidentals[bar][i].resize(accidentals[i].size());
      for (unsigned j = 0; j < accidentals[i].size(); j++) {
        allAccidentals[bar][i][j] = accidentals[i][j];
      }
    }
    for (unsigned i = 0; i < octaves.size(); i++) {
      allOctaves[bar][i].resize(octaves[i].size());
      for (unsigned j = 0; j < octaves[i].size(); j++) {
        allOctaves[bar][i][j] = octaves[i][j];
      }
    }
    for (unsigned i = 0; i < durs.size(); i++) {
      durations[bar][i] = durs[i];
    }
    for (unsigned i = 0; i < dots.size(); i++) {
      dotIndexes[bar][i] = dots[i];
    }
    for (unsigned i = 0; i < gliss.size(); i++) {
      allGlissandi[bar][i] = gliss[i];
    }
    for (unsigned i = 0; i < articul.size(); i++) {
      allArticulations[bar][i] = articul[i];
    }
    for (unsigned i = 0; i < texts.size(); i++) {
      allTexts[bar][i] = texts[i];
    }
    for (unsigned i = 0; i < textIndexes.size(); i++) {
      allTextsIndexes[bar][i] = textIndexes[i];
    }
    if (dyns[0] != -1) {
      for (unsigned i = 0; i < dyns.size(); i++) {
        dynamics[bar][i] = dyns[i];
      }
    }
    else {
      dynamics[bar].clear();
    }
    if (dynIdx[0] != -1) {
      for (unsigned i = 0; i < dynIdx.size(); i++) {
        dynamicsIndexes[bar][i] = dynIdx[i];
      }
    }
    else {
      dynamicsIndexes[bar].clear();
    }
    if (dynRampStart[0] != -1) {
      for (unsigned i = 0; i < dynRampStart.size(); i++) {
        dynamicsRampStart[bar][i] = dynRampStart[i];
      }
    }
    else {
      dynamicsRampStart[bar].clear();
    }
    if (dynRampEnd[0] != -1) {
      for (unsigned i = 0; i < dynRampEnd.size(); i++) {
        dynamicsRampEnd[bar][i] = dynRampEnd[i];
      }
    }
    else {
      dynamicsRampEnd[bar].clear();
    }
    if (dynRampDir[0] != -1) {
      for (unsigned i = 0; i < dynRampDir.size(); i++) {
        dynamicsRampDir[bar][i] = dynRampDir[i];
      }
    }
    else {
      dynamicsRampDir[bar].clear();
    }
    if (slurBegin[0] != -1) {
      for (unsigned i = 0; i < slurBegin.size(); i++) {
        slurBeginnings[bar][i] = slurBegin[i];
      }
    }
    else {
      slurBeginnings[bar].clear();
    }
    if (slurEnd[0] != -1){
      for (unsigned i = 0; i < slurEnd.size(); i++) {
        slurEndings[bar][i] = slurEnd[i];
      }
    }
    else {
      slurEndings[bar].clear();
    }
  }
  beats = numBeats;
  setLength(bar, numBeats);
  setNotePositions(bar, loopIndex);
}

void Notes::correctDynamics(int bar, vector<int> dyns){
	if (dynamics[bar].size() != dyns.size()) {
		cout << "vectors don't have the same size\n";
		return;
	}
	for (unsigned i = 0; i < dynamics[bar].size(); i++) {
		dynamics[bar][i] = dyns[i];
	}
}

void Notes::resizeLoopIndexBasedVectors(int bar, int loopIndex){
  if ((int)allNotesYCoords1[bar].size() <= loopIndex) {
    allNotesYCoords1[bar].resize(loopIndex+1);
    allNotesYCoords2[bar].resize(loopIndex+1);
    stemsY1[bar].resize(loopIndex+1);
    stemsY2[bar].resize(loopIndex+1);
    beamYCoords1[bar].resize(loopIndex+1);
    beamYCoords2[bar].resize(loopIndex+1);
    articulYPos[bar].resize(loopIndex+1);
    slurStartY[bar].resize(loopIndex+1);
    slurMiddleY1[bar].resize(loopIndex+1);
    slurMiddleY2[bar].resize(loopIndex+1);
    slurStopY[bar].resize(loopIndex+1);
    dynsYCoords[bar].resize(loopIndex+1);
    dynsRampStartYCoords[bar].resize(loopIndex+1);
    dynsRampEndYCoords[bar].resize(loopIndex+1);
		allTextsYCoords[bar].resize(loopIndex+1);
  }
}

void Notes::setNotePositions(int bar, int loopIndex) {
  // create an empty slot in the vectors only for a new bar
  if ((int)stemDirections.size() == bar) {
    stemDirections.resize(stemDirections.size()+1);
    grouppedStemDirs.resize(grouppedStemDirs.size()+1);
    articulXPos.resize(articulXPos.size()+1);
    articulYPos.resize(articulYPos.size()+1);
    extraLinesDir.resize(extraLinesDir.size()+1);
    extraLinesYPos.resize(extraLinesYPos.size()+1);
    numExtraLines.resize(numExtraLines.size()+1);
    hasExtraLines.resize(hasExtraLines.size()+1);
    extraLinesDir.back().push_back({{}});
    extraLinesYPos.back().push_back({{}});
    numExtraLines.back().push_back({{}});
    hasExtraLines.back().push_back({{}});
    actualDurs.resize(actualDurs.size()+1);
    hasStem.resize(hasStem.size()+1);
    stemsX.resize(stemsX.size()+1);
    stemsY1.resize(stemsY1.size()+1);
    stemsY2.resize(stemsY2.size()+1);
    slurStartX.resize(slurStartX.size()+1);
    slurStartY.resize(slurStartY.size()+1);
    slurStopX.resize(slurStopX.size()+1);
    slurStopY.resize(slurStopY.size()+1);
    slurMiddleX1.resize(slurMiddleX1.size()+1);
    slurMiddleY1.resize(slurMiddleY1.size()+1);
    slurMiddleX2.resize(slurMiddleX2.size()+1);
    slurMiddleY2.resize(slurMiddleY2.size()+1);
    dynsXCoords.resize(dynsXCoords.size()+1);
    dynsYCoords.resize(dynsYCoords.size()+1);
    dynsRampStartXCoords.resize(dynsRampStartXCoords.size()+1);
    dynsRampEndXCoords.resize(dynsRampEndXCoords.size()+1);
    dynsRampStartYCoords.resize(dynsRampStartXCoords.size()+1);
    dynsRampEndYCoords.resize(dynsRampEndYCoords.size()+1);
    beamsX.resize(beamsX.size()+1);
    beamsIndexes.resize(beamsIndexes.size()+1);
    numNotesWithBeams.resize(numNotesWithBeams.size()+1);
    allBeamYCoords.resize(allBeamYCoords.size()+1);
    hasRests.resize(hasRests.size()+1);
    maxNumLines.resize(maxNumLines.size()+1);
    startPnts.resize(startPnts.size()+1);
    grouppedStemDirsCoeff.resize(grouppedStemDirsCoeff.size()+1);
    numBeams.resize(numBeams.size()+1);
		numTails.resize(numTails.size()+1);
    beamXCoords1.resize(beamXCoords1.size()+1);
    beamXCoords2.resize(beamXCoords2.size()+1);
    beamYCoords1.resize(beamYCoords1.size()+1);
    beamYCoords2.resize(beamYCoords2.size()+1);
    allNotesXCoords.resize(allNotesXCoords.size()+1);
    allNotesYCoords1.resize(allNotesYCoords1.size()+1);
    allNotesYCoords2.resize(allNotesYCoords2.size()+1);
    allNotesMaxYPos.resize(allNotesMaxYPos.size()+1);
    allNotesMinYPos.resize(allNotesMinYPos.size()+1);
    numOctavesOffset.resize(numOctavesOffset.size()+1);
    whichIdxBase.resize(whichIdxBase.size()+1);
    whichIdxEdge.resize(whichIdxEdge.size()+1);
		allTextsXCoords.resize(allTextsXCoords.size()+1);
		allTextsYCoords.resize(allTextsYCoords.size()+1);
		tacet.resize(tacet.size()+1);
    resizeLoopIndexBasedVectors(bar, loopIndex);
  }
  else {
    resizeLoopIndexBasedVectors(bar, loopIndex);
    stemDirections[bar].clear();
    grouppedStemDirs[bar].clear();
    articulXPos[bar].clear();
    articulYPos[bar][loopIndex].clear();
    extraLinesDir[bar].clear();
    extraLinesYPos[bar].clear();
    numExtraLines[bar].clear();
    hasExtraLines[bar].clear();
    actualDurs[bar].clear();
    hasStem[bar].clear();
    stemsX[bar].clear();
    stemsY1[bar][loopIndex].clear();
    stemsY2[bar][loopIndex].clear();
    slurStartX[bar].clear();
    slurStartY[bar][loopIndex].clear();
    slurStopX[bar].clear();
  	slurStopY[bar][loopIndex].clear();
    slurMiddleX1[bar].clear();
    slurMiddleY1[bar][loopIndex].clear();
    slurMiddleX2[bar].clear();
    slurMiddleY2[bar][loopIndex].clear();
    dynsXCoords[bar].clear();
    dynsYCoords[bar][loopIndex].clear();
    dynsRampStartXCoords[bar].clear();
    dynsRampEndXCoords[bar].clear();
    dynsRampStartYCoords[bar][loopIndex].clear();
    dynsRampEndYCoords[bar][loopIndex].clear();
    beamsX[bar].clear();
    beamsIndexes[bar].clear();
    numNotesWithBeams[bar].clear();
    allBeamYCoords[bar].clear();
    hasRests[bar].clear();
    maxNumLines[bar].clear();
    startPnts[bar].clear();
    grouppedStemDirsCoeff[bar].clear();
    numBeams[bar].clear();
		numTails[bar].clear();
    beamXCoords1[bar].clear();
    beamXCoords2[bar].clear();
    beamYCoords1[bar][loopIndex].clear();
    beamYCoords2[bar][loopIndex].clear();
    allNotesXCoords[bar].clear();
    allNotesYCoords1[bar][loopIndex].clear();
  	allNotesYCoords2[bar][loopIndex].clear();
    allNotesMaxYPos[bar].clear();
    allNotesMinYPos[bar].clear();
    numOctavesOffset[bar].clear();
    whichIdxBase[bar].clear();
    whichIdxEdge[bar].clear();
		allTextsXCoords[bar].clear();
		allTextsYCoords[bar][loopIndex].clear();
  }
  // populate the vectors
  for (unsigned i = 0; i < allNotes[bar].size(); i++) {
    stemDirections[bar].push_back(1);
    articulXPos[bar].push_back(FLT_MIN);
    articulYPos[bar][loopIndex].push_back(FLT_MIN);
    extraLinesDir[bar].push_back({});
    extraLinesYPos[bar].push_back({});
    numExtraLines[bar].push_back({});
    hasExtraLines[bar].push_back({});
    actualDurs[bar].push_back(0);
    hasStem[bar].push_back(false);
    stemsX[bar].push_back(0);
    stemsY1[bar][loopIndex].push_back(0);
    stemsY2[bar][loopIndex].push_back(0);
		numTails[bar].push_back(0);
    allNotesXCoords[bar].push_back({});
    allNotesYCoords1[bar][loopIndex].push_back({});
    allNotesMaxYPos[bar].push_back(FLT_MIN);
    allNotesMinYPos[bar].push_back(FLT_MAX);
    numOctavesOffset[bar].push_back(0);
    whichIdxBase[bar].push_back(0);
    whichIdxEdge[bar].push_back(0);
  }
  float x = xStartPnt[bar];
  vector<float> yVec;
  int prevBeamIndex = 0;
  int beamsCounter = 0;
  float prevX = x;
  float durAccum = 0.0;
  vector<float> yLocal;
  int hasRestsLocal = 0;
  int numLinesLocal = 0;
	tacet[bar] = false;
  beamIndexCounter = 0;
  beamIndexCounter2 = 0;
  // cast to int to avoid cast further down in if tests
  for (int i = 0; i < (int)allNotes[bar].size(); i++) {
    yVec.clear();
		if ((allNotes[bar].size() == 1) && (allNotes[bar][i][0] == -1)) {
			tacet[bar] = true;
			minYCoord[bar] = yStartPnt[bar][loopIndex];
			maxYCoord[bar] = minYCoord[bar] + (staffDist * 4);
		}
    bool fullBeat = false;
    int stemDir = 1;
    // get the duration in musical terms, 1 for whole note
    // 2 for half note, 4 for quarter, etc.
    actualDurs[bar][i] = (int)(((float)beats / (float)durations[bar][i]) + 0.5);
    // detect the beats based on the denominator
    // so eights and shorter notes are groupped properly
    durAccum += ((float)denominator/((float)beats/(float)denominator)) * \
								 ((float)durations[bar][i]/(float)denominator);
    // if we get a whole beat or more
    if (durAccum >= 1.0) {
      fullBeat = true;
			durAccum -= (int)durAccum;
    }
    if (actualDurs[bar][i] >= 2) {
      // checking the first note, even in case of a chord, is enough
      // to determine whether we have a rest or not
      if (allNotes[bar][i][0] > -1) {
        hasStem[bar][i] = true;
      }
    }
    if (i > 0) {
      x += (durations[bar][i-1] * distBetweenBeats[bar]);
    }
		else if (tacet[bar]) {
			x = xStartPnt[bar] + ((xEndPnt[bar] - xStartPnt[bar]) / 2);
		}
    // accumulate Ys to determine stem direction
    int stemDirAccum = 0;
    allNotesXCoords[bar][i].resize(allNotes[bar][i].size());
    allNotesYCoords1[bar][loopIndex][i].resize(allNotes[bar][i].size());
    int outOfBounds = 0;
    for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
      extraLinesDir[bar][i].push_back(0);
      extraLinesYPos[bar][i].push_back(0);
      numExtraLines[bar][i].push_back(0);
      hasExtraLines[bar][i].push_back(false);
      // set y as the C one octave below middle C
      yVec.push_back(yStartPnt[bar][loopIndex] + (halfStaffDist * 17));
      float noteYOffset = (allNotes[bar][i][j] * halfStaffDist) + \
                          (allOctaves[bar][i][j] * 7 * halfStaffDist);
      yVec.back() -= noteYOffset;
      // if we have a chord and outOfBounds is already set, don't bother for the rest of the chord notes
      if (outOfBounds == 0) {
        // if the note is supposed to be drawn on the fourth extra line below the staff or lower
        if (yVec.back() >= (yStartPnt[bar][loopIndex] + (staffDist*8))) outOfBounds = 1;
        // else, if it is supposed to be drawn on the fourth line above the staff or higher
        else if (yVec.back() <= (yStartPnt[bar][loopIndex] - (staffDist*4))) outOfBounds = -1;
      }
    }
    if (outOfBounds != 0) {
      float extremeY;
      if (outOfBounds > 0) {
        extremeY = FLT_MIN;
        for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
          if (yVec[j] > extremeY) extremeY = yVec[j];
        }
        while (extremeY > (yStartPnt[bar][loopIndex]+(staffDist*8))) {
          extremeY -= (halfStaffDist*7);
          numOctavesOffset[bar][i]--;
        }
      }
      else {
        extremeY = FLT_MAX;
        for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
          if (yVec[j] < extremeY) extremeY = yVec[j];
        }
        while (extremeY < (yStartPnt[bar][loopIndex]-(staffDist*4))) {
          extremeY += (halfStaffDist*7);
          numOctavesOffset[bar][i]++;
        }
      }
      // once we determine how many octaves a group of notes must shift
      for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
        yVec[j] += (halfStaffDist*(numOctavesOffset[bar][i]*7));
        allOctaves[bar][i][j] -= numOctavesOffset[bar][i];
      }
    }
    // run the loop a second time, since the octave offsets have now been updated
    for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
			allNotesYCoords1[bar][loopIndex][i][j] = yVec[j];
      if (yVec[j] > allNotesMaxYPos[bar][i]) allNotesMaxYPos[bar][i] = yVec[j];
      if (yVec[j] < allNotesMinYPos[bar][i]) allNotesMinYPos[bar][i] = yVec[j];
      // if we're at B in he middle of the staff or above
      if (yVec[j] <= middleOfStaff) {
        stemDirAccum -= 1;
      }
      else {
        stemDirAccum += 1;
      }
      // yStartPnt is at F on the top line of the staff
      // so we need to subtract half the distance of the staff lines
      // times 9 (from top F to middle C)
      if (yVec[j] > (yStartPnt[bar][loopIndex] + (halfStaffDist * 9))) {
        if (allNotes[bar][i][j] > -1) {
          extraLinesDir[bar][i][j] = 1;
        }
      }
      // or add this once, to see if we're at A above the staff or higher
      else if (yVec[j] < (yStartPnt[bar][loopIndex] + halfStaffDist)) {
        if (allNotes[bar][i][j] > -1) {
          extraLinesDir[bar][i][j] = -1;
        }
      }
      if (extraLinesDir[bar][i][j] != 0) {
        float yPos;
        if (extraLinesDir[bar][i][j] > 0) {
          numExtraLines[bar][i][j] = (int)((yVec[j] - (yStartPnt[bar][loopIndex] + (staffDist * 4))) / staffDist);
          yPos = yStartPnt[bar][loopIndex] + (staffDist * 4);
        }
        else {
          numExtraLines[bar][i][j] = (int)((yStartPnt[bar][loopIndex] - yVec[j]) / staffDist);
          yPos = yStartPnt[bar][loopIndex];
        }
        extraLinesYPos[bar][i][j] = yPos;
        hasExtraLines[bar][i][j] = true;
      }
    }
    // once we've stored the Y coords of the note heads
    // we run a loop to see if there are any note heads close together
    // which means we'll have to change the X coord of one of them
    // e.g. in an <e f g> sequence, only f will shift on the X axis
    // initialize all X coords to x
    vector<float> xLocal (allNotesYCoords1[bar][loopIndex][i].size(), x);
    for (unsigned j = 1; j < allNotesYCoords1[bar][loopIndex][i].size(); j++) {
      for (unsigned k = 0; k < allNotesYCoords1[bar][loopIndex][i].size(); k++) {
        if (j == k) continue; // don't bother with the same bar
        // if two notes are close together on the Y axis
        if (abs(allNotesYCoords1[bar][loopIndex][i][j] - allNotesYCoords1[bar][loopIndex][i][k]) < staffDist) {
          // if k is on top (smaller Y coord)
          if (allNotesYCoords1[bar][loopIndex][i][j] > allNotesYCoords1[bar][loopIndex][i][k]) {
            // if both j and k haven't moved yet
            if ((xLocal[j] == x) && (xLocal[k] == x)) {
              // move the top note
              xLocal[k] += noteWidth;
            }
          }
          else {
            if ((xLocal[k] == x) && (xLocal[j] == x)) {
              xLocal[j] += noteWidth;
            }
          }
        }
      }
    }
    allNotesXCoords[bar][i] = xLocal;
    // get the Y coords of the extreme notes of a chord
    float maxVal = 0;
    float minVal = FLT_MAX;
    if (stemDirAccum >= 0) {
      stemDir = 1; // local stem dir
      // update stemDirections which is used in drawAccidentals()
      stemDirections[bar][i] = 1;
      for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
        if (allNotesYCoords1[bar][loopIndex][i][j] > maxVal) {
          maxVal = allNotesYCoords1[bar][loopIndex][i][j];
          whichIdxBase[bar][i] = j;
        }
        if (allNotesYCoords1[bar][loopIndex][i][j] < minVal) {
          minVal = allNotesYCoords1[bar][loopIndex][i][j];
          whichIdxEdge[bar][i] = j;
        }
      }
    }
    else {
      stemDir = -1;
      stemDirections[bar][i] = -1;
      for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
        if (allNotesYCoords1[bar][loopIndex][i][j] < minVal) {
          minVal = allNotesYCoords1[bar][loopIndex][i][j];
          whichIdxBase[bar][i] = j;
        }
        if (allNotesYCoords1[bar][loopIndex][i][j] > maxVal) {
          maxVal = allNotesYCoords1[bar][loopIndex][i][j];
          whichIdxEdge[bar][i] = j;
        }
      }
    }
    // set the Y coord to be the lowest note (highest Y value)
    // in case of stem up, otherwise the other way round
    float y = yStartPnt[bar][loopIndex] + (halfStaffDist * 17);
    float yEdge = y;
    float noteYOffsetBase = (allNotes[bar][i][whichIdxBase[bar][i]] * halfStaffDist) + \
                            (allOctaves[bar][i][whichIdxBase[bar][i]] * 7 * halfStaffDist);
    float noteYOffsetEdge = (allNotes[bar][i][whichIdxEdge[bar][i]] * halfStaffDist) + \
                            (allOctaves[bar][i][whichIdxEdge[bar][i]] * 7 * halfStaffDist);
    y -= noteYOffsetBase;
    yEdge -= noteYOffsetEdge;
    // caclulate the coords of the stems for half and quarter notes only
    // the stems for eights and shorter will be drawn
    // before their connecting lines are drawn
    if (hasStem[bar][i] && actualDurs[bar][i] < 8) {
      float xPos = x + (((noteWidth/2)-(noteWidth/10)) * stemDir);
      // stems up need a little less offset than stems down
      if (stemDir == 1) {
        xPos -= (noteWidth/10);
      }
			float stemY = y;
			if (stemDir < 0) stemY -= (allNotesYCoords1[bar][loopIndex][i][whichIdxBase[bar][i]] -\
																 allNotesYCoords1[bar][loopIndex][i][whichIdxEdge[bar][i]]);
      float yPos = yEdge - (stemHeight*stemDir);
      stemsX[bar][i] = xPos;
      stemsY1[bar][loopIndex][i] = stemY;
      stemsY2[bar][loopIndex][i] = yPos;
			float yPosLocal = yPos; // copy the value to a new variable to avoid double free error
			int index = i;
      storeAllNotesSecondYCoords(bar, loopIndex, index, yPosLocal);
    }
    else if (actualDurs[bar][i] == 1) {
			float yPosLocal = y;
			int index = i;
      storeAllNotesSecondYCoords(bar, loopIndex, index, yPosLocal);
    }
    // the code below checks to group connected notes together
    // so that a beam is drawn straight and not broken
    // in case the connected notes don't rise or fall linearly
    if (actualDurs[bar][i] > 4) {
      beamsCounter++;
      if (allNotes[bar][i][0] < 0) {
        // we need to know if a group of connected notes has rests
        // so we can make sure the line that connects them is not
        // drawn on top of the rest
        if (actualDurs[bar][i] > hasRestsLocal) hasRestsLocal = actualDurs[bar][i];
        // in case of a rest, store an extreme value that we'll filter
        // in findExtremes(), based on the grouppedStemDirsCoeff of the stems
        yLocal.push_back(INT_MAX);
      }
      else {
        // note the max number of beams as the variable above
        // won't be very useful if we calculate for one beam only
        // and end up with more, in which case we're likely to draw on top
        // of the rest anyway
        int numLines = (log(actualDurs[bar][i]) / log(2)) - 2;
        if (numLines > numLinesLocal) numLinesLocal = numLines;
        yLocal.push_back(y);
      }
      if (fullBeat) {
        saveBeamsIndexes(bar, i, x, yLocal, hasRestsLocal, numLinesLocal);
        yLocal.clear();
        hasRestsLocal = 0;
        numLinesLocal = 0;
        beamsCounter = 0;
      }
      else if (beamsCounter == 1) {
        saveBeamsIndexes(bar, i, x);
      }
      prevBeamIndex = i;
    }
    else {
      // if we read a note longer than 8th and there
      // has already been at least one 8th note or shorter
      if (beamsCounter > 1) {
        saveBeamsIndexes(bar, prevBeamIndex, prevX, yLocal, hasRestsLocal, numLinesLocal);
        yLocal.clear();
        hasRestsLocal = 0;
        numLinesLocal = 0;
        beamsCounter = 0;
      }
    }
    prevX = x;
  }
  // check if the last note needs to be connected
  if (beamsCounter > 1) {
    // if the duration is the same with the last 8th or shorter note
    if (durations[bar][(int)allNotes[bar].size()-1] <= durations[bar][prevBeamIndex]) {
      saveBeamsIndexes(bar, prevBeamIndex, prevX, yLocal, hasRestsLocal, numLinesLocal);
      yLocal.clear();
      hasRestsLocal = 0;
      numLinesLocal = 0;
      beamsCounter = 0;
    }
  }
  // the code below calculates the coordinates of the beams
  if (beamsX[bar].size() > 0) {
    vector<vector<int>> notesIndexes;
    // local variables to separate the two lowest Y coordinates for each connected group
    for (unsigned i = 0; i < allBeamYCoords[bar].size(); i++) {
      float averageHeight = 0;
      int stemDir;
      // in case we have a INT_MAX we want to normalize with
      // allBeamYCoords[bar][i].size() -1; or howeven many rests we have
      int offset = 0;
      // we also store the number of notes locally so we can isolate single notes
      // that are not connected but are still groupped with rests
      numNotesWithBeams[bar].push_back(0);
      // and their indexes
      vector<int> notesIndexesLocal;
      for (unsigned j = 0; j < allBeamYCoords[bar][i].size(); j++) {
        // ignore the INT_MAX value as it is there to actually ignore
        // the Y coordinates of rests
        if (allBeamYCoords[bar][i][j] == INT_MAX) {
          offset++;
          continue;
        }
        else {
          averageHeight += allBeamYCoords[bar][i][j];
          numNotesWithBeams[bar][i]++;
          notesIndexesLocal.push_back(j);
        }
      }
      notesIndexes.push_back(notesIndexesLocal);
      averageHeight /= (float)(allBeamYCoords[bar][i].size() - offset);
      if (averageHeight < middleOfStaff) {
        stemDir = -1;
      }
      else {
        stemDir = 1;
      }
      // groupped stem directions for groupped notes
      grouppedStemDirs[bar].push_back(stemDir);
      // update stemDirections which are used in drawAccidentals()
      for (unsigned j = 0; j < allBeamYCoords[bar][i].size(); j++) {
        if (stemDirections[bar][(beamsIndexes[bar][i*2]+j)] != stemDir) {
          stemDirections[bar][(beamsIndexes[bar][i*2]+j)] = stemDir;
          // in case we have a chord within a group of notes with beams
          // the stem direction might change, so we must update the extreme notes indexes here
          int tempIdx = whichIdxBase[bar][(beamsIndexes[bar][i*2]+j)];
          whichIdxBase[bar][(beamsIndexes[bar][i*2]+j)] = whichIdxEdge[bar][(beamsIndexes[bar][i*2]+j)];
          whichIdxEdge[bar][(beamsIndexes[bar][i*2]+j)] = tempIdx;
        }
      }
    }
    // find the highest or lowest note, depending on the stem grouppedStemDirsCoeff
    findExtremes(bar, loopIndex);
    // store coords for the stems of the connected notes
    for (unsigned i = 0; i < allBeamYCoords[bar].size(); i++) {
      // don't store coords for stems for single notes that are not groupped
      if (numNotesWithBeams[bar][i] > 1) {
        // check if the groupped notes should be alligned
        // which is true when the extreme notes of a group have the same Y coordinate
        // in this case stems must have the same height and beams must be horizontal
        bool commonY = false;
        unsigned firstNdx = beamsIndexes[bar][i*2];
        unsigned lastNdx = beamsIndexes[bar][i*2]+(allBeamYCoords[bar][i].size()-1);
        if (allNotesYCoords1[bar][loopIndex][firstNdx][whichIdxBase[bar][firstNdx]] == \
            allNotesYCoords1[bar][loopIndex][lastNdx][whichIdxBase[bar][lastNdx]]) {
          commonY = true;
        }
        for (unsigned j = 0; j < allBeamYCoords[bar][i].size(); j++) {
					int indexLocal = beamsIndexes[bar][i*2]+j;
          float stemX = xStartPnt[bar];
					float stemY1 = allBeamYCoords[bar][i][j];
					float stemY2 = startPnts[bar][i];
          if ((beamsIndexes[bar][i*2]+j) > 0) {
            int loopIter = beamsIndexes[bar][i*2]+j;
            for (int k = 0; k < loopIter; k++) {
              stemX += (durations[bar][k] * distBetweenBeats[bar]);
            }
          }
          stemX += (((noteWidth/2)-(noteWidth/10)) * grouppedStemDirs[bar][i]);
          // stems up need a little less offset than stems down
          if (grouppedStemDirs[bar][i] == 1) {
            stemX -= (noteWidth/10);
          }
          // segment the line between the connected notes
          int stemYOffset = (halfStaffDist / ((int)allBeamYCoords[bar][i].size()-1)) * j;
          // if beams must be horizontal, then the stems must all have the same height
          if (commonY) stemYOffset = 0;
          // don't draw stems for rests
          if (allNotes[bar][indexLocal][0] > -1) {
            stemY2 += ((stemYOffset * grouppedStemDirsCoeff[bar][i]) - \
                     	 (stemHeight * grouppedStemDirs[bar][i]));
            stemY2 -= ((beamsLineWidth*2) * grouppedStemDirs[bar][i]);
						if (stemDirections[bar][indexLocal] < 0) {
							stemY1 += (allNotesYCoords1[bar][loopIndex][indexLocal][whichIdxBase[bar][indexLocal]] -\
												 allNotesYCoords1[bar][loopIndex][indexLocal][whichIdxEdge[bar][indexLocal]]);
						}
            stemsX[bar][indexLocal] = stemX;
            stemsY1[bar][loopIndex][indexLocal] = stemY1;
            stemsY2[bar][loopIndex][indexLocal] = stemY2;
						int yCoordsIndex = (int)indexLocal;
            storeAllNotesSecondYCoords(bar, loopIndex, yCoordsIndex, stemY2);
          }
        }
      }
      // store coords for the stems and tails of single notes separately
      else {
        float xLocal = xStartPnt[bar];
        int loopIter = beamsIndexes[bar][i*2]+notesIndexes[i][0];
				int indexLocal = beamsIndexes[bar][i*2]+notesIndexes[i][0];
        for (int k = 0; k < loopIter; k++) {
          xLocal += (durations[bar][k] * distBetweenBeats[bar]);
        }
        float stemX = xLocal + (((noteWidth/2)-(noteWidth/10)) * grouppedStemDirs[bar][i]);
        // stems up need a little less offset than stems down
        if (grouppedStemDirs[bar][i] == 1) {
          stemX -= (noteWidth/10);
        }
        stemsX[bar][indexLocal] = stemX;
        stemsY1[bar][loopIndex][indexLocal] = allBeamYCoords[bar][i][notesIndexes[i][0]];
        stemsY2[bar][loopIndex][indexLocal] = allBeamYCoords[bar][i][notesIndexes[i][0]]-\
                                              (stemHeight*grouppedStemDirs[bar][i]);
				int noteDur = beats / durations[bar][indexLocal];
				numTails[bar][indexLocal] = log(noteDur) / log(2) - 2;
				if (stemDirections[bar][indexLocal] < 0) {
					numTails[bar][indexLocal] *= -1;
				}
				int yCoordsIndex = (int)indexLocal;
        storeAllNotesSecondYCoords(bar, loopIndex, yCoordsIndex,
                                   allBeamYCoords[bar][i][notesIndexes[i][0]]-\
                                   (stemHeight*grouppedStemDirs[bar][i]));
      }
    }
    // we have stored pairs of connected notes (the two extremes in a group)
    // so we need to run the loop as many times as half the number of elements
    for (int i = 0; i < (int)beamsX[bar].size()/2; i++) {
      beamXCoords1[bar].push_back({});
      beamXCoords2[bar].push_back({});
      beamYCoords1[bar][loopIndex].push_back({});
      beamYCoords2[bar][loopIndex].push_back({});
      // don't draw beams for single notes that are not groupped
      if (numNotesWithBeams[bar][i] > 1) {
        vector<int> numBeamsLocal;
        for (unsigned j = 0; j < allBeamYCoords[bar][i].size(); j++) {
          int noteDur = beats / durations[bar][beamsIndexes[bar][i*2]+j];
          numBeamsLocal.push_back((log(noteDur) / log(2)) - 2);
        }
        // store the number of beams per note
        numBeams[bar].push_back(numBeamsLocal);
        // determine whether the beams should be completely horizontal
        // in case first and last notes of the group have the same Y coordinate
        bool commonY = false;
        unsigned firstNdx = beamsIndexes[bar][i*2];
        unsigned lastNdx = beamsIndexes[bar][i*2]+(allBeamYCoords[bar][i].size()-1);
        if (allNotesYCoords1[bar][loopIndex][firstNdx][whichIdxBase[bar][firstNdx]] == \
            allNotesYCoords1[bar][loopIndex][lastNdx][whichIdxBase[bar][lastNdx]]) {
          commonY = true;
        }
        // then get the X and Y coordinates of the connecting lines
        // one pair or coordinates per note
        // we get these coordinates similarly to the stem ones
        for (unsigned j = 0; j < allBeamYCoords[bar][i].size()-1; j++) {
          float xCoord1 = xStartPnt[bar];
          float xCoord2 = xStartPnt[bar];
          if ((beamsIndexes[bar][i*2]+j) > 0) {
            int loopIter = beamsIndexes[bar][i*2]+j;
            for (int k = 0; k < loopIter; k++) {
              xCoord1 += (durations[bar][k] * distBetweenBeats[bar]);
            }
          }
          // the second X coord is definitely not the first bar
          // so we add the next duration anyway
          // no need for the if test above
          int loopIter = beamsIndexes[bar][i*2]+j+1;
          for (int k = 0; k < loopIter; k++) {
            xCoord2 += (durations[bar][k] * distBetweenBeats[bar]);
          }
          xCoord1 += (((noteWidth/2)-(noteWidth/10)) * grouppedStemDirs[bar][i]);
          xCoord2 += (((noteWidth/2)-(noteWidth/10)) * grouppedStemDirs[bar][i]);
          // stems up need a little less offset than stems down
          if (grouppedStemDirs[bar][i] == 1) {
            xCoord1 -= (noteWidth/10);
            xCoord2 -= (noteWidth/10);
          }
          beamXCoords1[bar][i].push_back(xCoord1);
          beamXCoords2[bar][i].push_back(xCoord2);
          float stemYOffset1 = (halfStaffDist / ((float)allBeamYCoords[bar][i].size()-1)) * j;
          float stemYOffset2 = (halfStaffDist / ((float)allBeamYCoords[bar][i].size()-1)) * (j+1);
          // if beams must be horizontal
          if (commonY) stemYOffset1 = stemYOffset2 = 0;
          float yCoord1 = (startPnts[bar][i] + (stemYOffset1 * grouppedStemDirsCoeff[bar][i])) \
                          - ((stemHeight + beamsLineWidth) * grouppedStemDirs[bar][i]);
          float yCoord2 = (startPnts[bar][i] + (stemYOffset2 * grouppedStemDirsCoeff[bar][i])) \
                          - ((stemHeight + beamsLineWidth) * grouppedStemDirs[bar][i]);
          beamYCoords1[bar][loopIndex][i].push_back(yCoord1);
          beamYCoords2[bar][loopIndex][i].push_back(yCoord2);
        }
      }
    }
  }
  sortAllNotesCoords(bar, loopIndex);
  storeArticulationsCoords(bar, loopIndex);
  storeSlurCoords(bar, loopIndex);
	storeTextCoords(bar, loopIndex);
  storeDynamicsCoords(bar, loopIndex);
  storeMinMaxY(bar, loopIndex);
  // cout << "max y for notes " << objID << endl;
  // printVector(allNotesMaxYPos[bar]);
  // cout << "min y for notes " << objID << endl;
  // printVector(allNotesMinYPos[bar]);
  // cout << endl;
}

void Notes::saveBeamsIndexes(int bar, int beamIndex, float x, vector<float> y,
                             int rests, int numLines) {
  beamsX[bar].push_back(x+(noteWidth/2)+(lineWidth/2));
  beamsIndexes[bar].push_back(beamIndex);
  allBeamYCoords[bar].push_back(y);
  hasRests[bar].push_back(rests);
  maxNumLines[bar].push_back(numLines);
}

void Notes::saveBeamsIndexes(int bar, int beamIndex, float x){
  beamsX[bar].push_back(x+(noteWidth/2)+(lineWidth/2));
  beamsIndexes[bar].push_back(beamIndex);
}

void Notes::storeAllNotesSecondYCoords(int bar, int loopIndex, int noteIndex, float y){
	allNotesYCoords2[bar][loopIndex].push_back({(float)noteIndex, y});
  if (y > allNotesMaxYPos[bar][noteIndex]) allNotesMaxYPos[bar][noteIndex] = y;
  if (y < allNotesMinYPos[bar][noteIndex]) allNotesMinYPos[bar][noteIndex] = y;
}

void Notes::sortAllNotesCoords(int bar, int loopIndex){
	sort(allNotesYCoords2[bar][loopIndex].begin(), allNotesYCoords2[bar][loopIndex].end(),
       [](const vector<float>& a, const vector<float>& b) {return a[0] < b[0];});
  // sort(allNotesMaxYPos[bar].begin(), allNotesMaxYPos[bar].end()); //,
  // //     [](const vector<float>& a, const vector<float>& b) {return a[0] < b[0];});
  // sort(allNotesMinYPos[bar].begin(), allNotesMinYPos[bar].end()); //,
  // //     [](const vector<float>& a, const vector<float>& b) {return a[0] < b[0];});
}

void Notes::storeArticulationsCoords(int bar, int loopIndex){
  for (unsigned i = 0; i < allArticulations[bar].size(); i++) {
    if (allArticulations[bar][i] > 0) {
      float xPos = allNotesXCoords[bar][i][whichIdxBase[bar][i]];
      if (stemDirections[bar][i] > 0) xPos -= (noteWidth/4);
      else xPos -= (noteWidth/2);
      float yPos = allNotesYCoords1[bar][loopIndex][i][whichIdxBase[bar][i]];
      if (stemDirections[bar][i] > 0) {
        yPos += staffDist;
        if (yPos > allNotesMaxYPos[bar][i]) allNotesMaxYPos[bar][i] = yPos;
      }
      else {
        yPos -= staffDist;
        if (yPos < allNotesMinYPos[bar][i]) allNotesMinYPos[bar][i] = yPos;
      }
      // check if the note head is on a line, but only inside the stave
      if ((allNotesYCoords1[bar][loopIndex][i][whichIdxBase[bar][i]] > yStartPnt[bar][loopIndex]) && \
          ((allNotesYCoords1[bar][loopIndex][i][whichIdxBase[bar][i]]-yStartPnt[bar][loopIndex]) < \
           (yStartPnt[bar][loopIndex]+(staffDist*5)))) {
        if (((int)(abs(allNotesYCoords1[bar][loopIndex][i][whichIdxBase[bar][i]] - \
             yStartPnt[bar][loopIndex]) / halfStaffDist) % 2) == 0) {
          if (stemDirections[bar][i] > 0) yPos += halfStaffDist;
          else  yPos -= halfStaffDist;
          if (yPos + (notationFont.stringHeight(articulSyms[allArticulations[bar][i]-1])/2) > allNotesMaxYPos[bar][i]) {
            // articulation symbols start with index 1, because 0 is reserved for no articulation
            // so we subtract 1 from the index
            allNotesMaxYPos[bar][i] = yPos + (notationFont.stringHeight(articulSyms[allArticulations[bar][i]-1])/2);
          }
          if (yPos - (notationFont.stringHeight(articulSyms[allArticulations[bar][i]-1])/2) < allNotesMinYPos[bar][i]) {
            allNotesMinYPos[bar][i] = yPos - (notationFont.stringHeight(articulSyms[allArticulations[bar][i]-1])/2);
          }
        }
      }
      articulXPos[bar][i] = xPos;
      articulYPos[bar][loopIndex][i] = yPos;
      if (allArticulations[bar][i] == 7) {
        float yPosPortando;
        if (stemDirections[bar][i] > 0) {
          // check if the note head is inside the stave
          if ((allNotesYCoords1[bar][loopIndex][i][whichIdxBase[bar][i]] - \
               yStartPnt[bar][loopIndex]) < (staffDist*5)) {
            yPosPortando = yStartPnt[bar][loopIndex] + (staffDist*5);
          }
          else {
            yPosPortando = allNotesYCoords1[bar][loopIndex][i][whichIdxBase[bar][i]] + noteHeight;
          }
        }
        else {
          // check if the note head is inside the stave
          if (allNotesYCoords1[bar][loopIndex][i][whichIdxBase[bar][i]] > yStartPnt[bar][loopIndex]) {
            yPosPortando = yStartPnt[bar][loopIndex] - staffDist;
          }
          else {
            yPosPortando = allNotesYCoords1[bar][loopIndex][i][whichIdxBase[bar][i]] - noteHeight;
          }
        }
        // if case the articulation is a portando, we must get the height as in the line below
        float halfArticulHeight = (notationFont.stringHeight(".") + notationFont.stringHeight("_")) / 2;
        if (yPosPortando + halfArticulHeight > allNotesMaxYPos[bar][i]) {
          allNotesMaxYPos[bar][i] = yPosPortando + halfArticulHeight;
        }
        if (yPosPortando - halfArticulHeight < allNotesMinYPos[bar][i]) {
          allNotesMinYPos[bar][i] = yPosPortando - halfArticulHeight;
        }
        articulYPos[bar][loopIndex][i] = yPosPortando;
      }
    }
  }
}

void Notes::storeSlurCoords(int bar, int loopIndex){
  // the vector below is used to calculate the orientation of the middle points of the curve
  vector<int> slurDir;
  for (unsigned i = 0; i < slurBeginnings[bar].size(); i++) {
    for (int j = 0; j < (int)allNotes[bar].size(); j++) {
      if (slurBeginnings[bar][i] == j) {
        int k;
        int dirAccum = 0;
        // the direction of the slur will depend on the majority of stem directions
        for (k = slurBeginnings[bar][i]; k <= slurEndings[bar][i]; k++) {
          dirAccum += stemDirections[bar][k];
        }
        // if the accumulation is 0 it means we're even so we chose to go below the staff
        if (dirAccum == 0) {
          slurDir.push_back(1);
        }
        else {
          //float average = (float)dirAccum / (float)k;
          slurDir.push_back(dirAccum > 0 ? 1 : 0);
        }
        slurStartX[bar].push_back(allNotesXCoords[bar][j][whichIdxBase[bar][j]]);
        if (slurDir.back() > 0) {
          if (stemDirections[bar][j] > 0) {
            // if slur and stem directions are the same, we store the Y coord of the note head
            slurStartY[bar][loopIndex].push_back(allNotesYCoords1[bar][loopIndex][j][whichIdxBase[bar][j]]);
            slurStartY[bar][loopIndex].back() += noteHeight;
            if (slurStartY[bar][loopIndex].back() < allNotesMaxYPos[bar][j]) {
              slurStartY[bar][loopIndex].back() = allNotesMaxYPos[bar][j] + noteHeight;
              allNotesMaxYPos[bar][j] = slurStartY[bar][loopIndex].back(); // + (noteHeight/2);
            }
          }
          else {
            // otherwise we store the Y coord of the note stem (allNotesYCoords2)
            slurStartY[bar][loopIndex].push_back(allNotesYCoords2[bar][loopIndex][j][1]);
            slurStartY[bar][loopIndex].back() += halfStaffDist;
            if (slurStartY[bar][loopIndex].back() < allNotesMaxYPos[bar][j]) {
              slurStartY[bar][loopIndex].back() = allNotesMaxYPos[bar][j] + halfStaffDist;
              allNotesMaxYPos[bar][j] = slurStartY[bar][loopIndex].back(); // + (noteHeight/2);
            }
          }
        }
        else {
          if (stemDirections[bar][j] < 0) {
            slurStartY[bar][loopIndex].push_back(allNotesYCoords1[bar][loopIndex][j][whichIdxBase[bar][j]]);
            slurStartY[bar][loopIndex].back() -= noteHeight;
            if (slurStartY[bar][loopIndex].back() > allNotesMinYPos[bar][j]) {
              slurStartY[bar][loopIndex].back() = allNotesMinYPos[bar][j] - noteHeight;
              allNotesMinYPos[bar][j] = slurStartY[bar][loopIndex].back(); // - (noteHeight/2);
            }
          }
          else {
            slurStartY[bar][loopIndex].push_back(allNotesYCoords2[bar][loopIndex][j][1]);
            slurStartY[bar][loopIndex].back() -= halfStaffDist;
            if (slurStartY[bar][loopIndex].back() > allNotesMinYPos[bar][j]) {
              slurStartY[bar][loopIndex].back() = allNotesMinYPos[bar][j] - halfStaffDist;
              allNotesMinYPos[bar][j] = slurStartY[bar][loopIndex].back(); // - (noteHeight/2);
            }
          }
        }
      }
    }
  }
  for (unsigned i = 0; i < slurEndings[bar].size(); i++) {
    for (int j = 0; j < (int)allNotes[bar].size(); j++) {
      if (slurEndings[bar][i] == j) {
        slurStopX[bar].push_back(allNotesXCoords[bar][j][whichIdxBase[bar][j]]);
        if (slurDir[i] > 0) {
          if (stemDirections[bar][j] > 0) {
            slurStopY[bar][loopIndex].push_back(allNotesYCoords1[bar][loopIndex][j][whichIdxBase[bar][j]]);
            slurStopY[bar][loopIndex].back() += noteHeight;
            if (slurStopY[bar][loopIndex].back() < allNotesMaxYPos[bar][j]) {
              slurStopY[bar][loopIndex].back() = allNotesMaxYPos[bar][j] + noteHeight;
              allNotesMaxYPos[bar][j] = slurStopY[bar][loopIndex].back(); // + (noteHeight/2);
            }
          }
          else {
            slurStopY[bar][loopIndex].push_back(allNotesYCoords2[bar][loopIndex][j][1]);
            slurStopY[bar][loopIndex].back() += halfStaffDist;
            if (slurStopY[bar][loopIndex].back() < allNotesMaxYPos[bar][j]) {
              slurStopY[bar][loopIndex].back() = allNotesMaxYPos[bar][j] + halfStaffDist;
              allNotesMaxYPos[bar][j] = slurStopY[bar][loopIndex].back(); // + (noteHeight/2);
            }
          }
        }
        else {
          if (stemDirections[bar][j] < 0) {
            slurStopY[bar][loopIndex].push_back(allNotesYCoords1[bar][loopIndex][j][whichIdxBase[bar][j]]);
            slurStopY[bar][loopIndex].back() -= noteHeight;
            if (slurStopY[bar][loopIndex].back() > allNotesMinYPos[bar][j]) {
              slurStopY[bar][loopIndex].back() = allNotesMinYPos[bar][j] - noteHeight;
              allNotesMinYPos[bar][j] = slurStopY[bar][loopIndex].back(); // - (noteHeight/2);
            }
          }
          else {
            slurStopY[bar][loopIndex].push_back(allNotesYCoords2[bar][loopIndex][j][1]);
            slurStopY[bar][loopIndex].back() -= halfStaffDist;
            if (slurStopY[bar][loopIndex].back() > allNotesMinYPos[bar][j]) {
              slurStopY[bar][loopIndex].back() = allNotesMinYPos[bar][j] - halfStaffDist;
              allNotesMinYPos[bar][j] = slurStopY[bar][loopIndex].back(); // - (noteHeight/2);
            }
          }
        }
      }
    }
  }
  for (unsigned i = 0; i < slurBeginnings[bar].size(); i++) {
    float curveSegment = (slurStopX[bar][i] - slurStartX[bar][i]) / 3.0;
    float middleX = slurStartX[bar][i] + curveSegment;
    slurMiddleX1[bar].push_back(middleX);
    middleX += curveSegment;
    slurMiddleX2[bar].push_back(middleX);
    float middleY1;
    if (slurDir[i] > 0) {
      middleY1 = slurStartY[bar][loopIndex][i] > slurStopY[bar][loopIndex][i] ? \
                 slurStartY[bar][loopIndex][i] : slurStopY[bar][loopIndex][i];
    }
    else {
      middleY1 = slurStartY[bar][loopIndex][i] < slurStopY[bar][loopIndex][i] ? \
                 slurStartY[bar][loopIndex][i] : slurStopY[bar][loopIndex][i];
    }
    float middleY2 = middleY1;
    if (slurDir[i] > 0) {
      middleY1 += staffDist;
      middleY2 += staffDist;
    }
    else {
      middleY1 -= staffDist;
      middleY2 -= staffDist;
    }
    for (int j = slurBeginnings[bar][i]; j < slurEndings[bar][i]; j++) {
      if (slurDir[i] > 0) {
        if (middleY1 < allNotesMaxYPos[bar][j]) {
          middleY1 = allNotesMaxYPos[bar][j] + halfStaffDist;
        }
        if (middleY2 < allNotesMaxYPos[bar][j]) {
          middleY2 = allNotesMaxYPos[bar][j] + halfStaffDist;
        }
        // separate test for allNotesMaxYPos, because if it is included in the
        // if tests above, only one such y position inside the slur will be updated
        // and it is possible that many notes are included in the slur
        // the actual beginning and ending of a slur though, should not be included
        // as the y position is already determined there
        if (j > 0 && j < slurEndings[bar][i]-1) {
          if (allNotesMaxYPos[bar][j] < middleY1 || allNotesMaxYPos[bar][j] < middleY2) {
            allNotesMaxYPos[bar][j] = max(middleY1, middleY2); // + (noteHeight/2);
          }
        }
      }
      else {
        if (middleY1 > allNotesMinYPos[bar][j]) {
          middleY1 = allNotesMinYPos[bar][j] + halfStaffDist;
        }
        if (middleY2 > allNotesMinYPos[bar][j]) {
          middleY2 = allNotesMinYPos[bar][j] + halfStaffDist;
        }
        if (j > 0 && j < slurEndings[bar][i]-1) {
          if (allNotesMinYPos[bar][j] > middleY1 || allNotesMinYPos[bar][j] < middleY2) {
            allNotesMinYPos[bar][j] = min(middleY1, middleY2); // - (noteHeight/2);
          }
        }
      }
    }
    slurMiddleY1[bar][loopIndex].push_back(middleY1);
    slurMiddleY2[bar][loopIndex].push_back(middleY2);
  }
}

void Notes::storeTextCoords(int bar, int loopIndex){
	for (unsigned i = 0; i < allTextsIndexes[bar].size(); i++) {
		if (allTextsIndexes[bar][i] != 0) {
			allTextsXCoords[bar].push_back(allNotesXCoords[bar][i][whichIdxBase[bar][i]]-(noteWidth/2.0));
			float yPos;
      float textHalfHeight = textFont.stringHeight(allTexts[bar][i]) / 2;
			if (allTextsIndexes[bar][i] > 0) {
				if (stemDirections[bar][i] > 0) {
          yPos = min(allNotesMinYPos[bar][i], min(allNotesYCoords2[bar][loopIndex][i][1],
																			            yStartPnt[bar][loopIndex]));
				}
        
				else {
          yPos = min(allNotesMinYPos[bar][i], min(allNotesYCoords1[bar][loopIndex][i][0],
																			            yStartPnt[bar][loopIndex]));
				}
				yPos -= (noteHeight + halfStaffDist);
        if (yPos - textHalfHeight < allNotesMinYPos[bar][i]) {
          allNotesMinYPos[bar][i] = yPos; // - textHalfHeight;
        }
			}
			else {
				if (stemDirections[bar][i] > 0) {
          yPos =  max(allNotesMaxYPos[bar][i], max(allNotesYCoords1[bar][loopIndex][i][0],
													                         yStartPnt[bar][loopIndex]+(halfStaffDist*9)));
				}
				else {
          yPos =  max(allNotesMaxYPos[bar][i], max(allNotesYCoords2[bar][loopIndex][i][1],
												                           yStartPnt[bar][loopIndex]+(halfStaffDist*9)));
				}
				yPos += (noteHeight + halfStaffDist);
        if (yPos + textHalfHeight > allNotesMaxYPos[bar][i]) {
          allNotesMaxYPos[bar][i] = yPos; // + textHalfHeight;
        }
			}
			allTextsYCoords[bar][loopIndex].push_back(yPos);
		}
	}
}

void Notes::storeDynamicsCoords(int bar, int loopIndex){
  // find the X Y coordinate for dynamics symbols
  for (unsigned i = 0; i < dynamicsIndexes[bar].size(); i++) {
    // cast to int to avoid cast in if test below
    for (int j = 0; j < (int)allNotes[bar].size(); j++) {
      if (dynamicsIndexes[bar][i] == j) {
        float y = max(yStartPnt[bar][loopIndex]+(staffDist*4), allNotesMaxYPos[bar][j]);
        y += (notationFont.stringHeight(dynSyms[dynamics[bar][i]]) * 1.5); // give a bit of room with 1.2
        if (y > allNotesMaxYPos[bar][j]) allNotesMaxYPos[bar][j] = y;
        dynsXCoords[bar].push_back(allNotesXCoords[bar][j][whichIdxBase[bar][j]]-\
                                   dynSymsWidths[dynamics[bar][i]]/2);
        dynsYCoords[bar][loopIndex].push_back(y);
      }
    }
  }
  // then find the X Y coordinates for crescendi and decrescendi
  for (unsigned i = 0; i < dynamicsRampStart[bar].size(); i++) {
    // cast to int to avoid cast in if test below
    for (int j = 0; j < (int)allNotes[bar].size(); j++) {
      if (dynamicsRampStart[bar][i] == j) {
        float y = max(yStartPnt[bar][loopIndex]+(staffDist*4), allNotesMaxYPos[bar][j]);
        if (y > allNotesMaxYPos[bar][j]) allNotesMaxYPos[bar][j] = y;
        dynsRampStartXCoords[bar].push_back(allNotesXCoords[bar][j][whichIdxBase[bar][j]]);
        dynsRampStartYCoords[bar][loopIndex].push_back(y);
      }
    }
  }
  for (unsigned i = 0; i < dynamicsRampEnd[bar].size(); i++) {
    // cast to int to avoid cast in if test below
    for (int j = 0; j < (int)allNotes[bar].size(); j++) {
      if (dynamicsRampEnd[bar][i] == j) {
        float y = max(yStartPnt[bar][loopIndex]+(staffDist*4), allNotesMaxYPos[bar][j]);
        if (y > allNotesMaxYPos[bar][j]) allNotesMaxYPos[bar][j] = y;
        dynsRampEndXCoords[bar].push_back(allNotesXCoords[bar][j][whichIdxBase[bar][j]]);
        dynsRampEndYCoords[bar][loopIndex].push_back(y);
      }
    }
  }
  // check if a crescendo or decrescendo starts or ends at a dynamic
  for (unsigned i = 0; i < dynamicsRampStart[bar].size(); i++) {
    for (unsigned j = 0; j < dynamicsIndexes[bar].size(); j++) {
      if (dynamicsRampStart[bar][i] == dynamicsIndexes[bar][j]) {
        // if we start at a dynamic symbol add half the width of that symbol to the (de)crescendo start
        dynsRampStartXCoords[bar][i] += dynSymsWidths[dynamics[bar][j]]/2;
        // and allign ramp start with the dynamic symbol on the Y axis
        dynsRampStartYCoords[bar][loopIndex][i] = dynsYCoords[bar][loopIndex][j];
        if (j < dynamicsIndexes[bar].size()-1) {
          // run through all the notes in between to make sure the ramp won't fall
          // on one of these notes or an articulation sign
          for (int k = dynamicsIndexes[bar][j]; k < dynamicsIndexes[bar][j+1]; k++) {
            dynsRampStartYCoords[bar][loopIndex][i] = max(dynsRampStartYCoords[bar][loopIndex][i], allNotesMaxYPos[bar][k]);
            if (dynsRampStartYCoords[bar][loopIndex][i] > allNotesMaxYPos[bar][k]) {
              allNotesMaxYPos[bar][k] = dynsRampStartYCoords[bar][loopIndex][i];
            }
          }
          // check if the currect ramp ends at the next dynamic
          if (dynamicsRampEnd[bar][i] == dynamicsIndexes[bar][j+1]) {
            dynsRampEndXCoords[bar][i] -= dynSymsWidths[dynamics[bar][j+1]]/2.0;
            dynsRampStartYCoords[bar][loopIndex][i] = max(dynsRampStartYCoords[bar][loopIndex][i], dynsYCoords[bar][loopIndex][j+1]);
            dynsRampEndYCoords[bar][loopIndex][i] = max(dynsRampStartYCoords[bar][loopIndex][i], dynsRampEndYCoords[bar][loopIndex][i]);
            for (int k = dynamicsIndexes[bar][j]; k < dynamicsIndexes[bar][j+1]; k++) {
              if (dynsRampEndYCoords[bar][loopIndex][i] > allNotesMaxYPos[bar][k]) {
                allNotesMaxYPos[bar][k] = dynsRampEndYCoords[bar][loopIndex][i];
              }
            }
            // allign the two dynamics as well
            dynsYCoords[bar][loopIndex][j] = dynsYCoords[bar][loopIndex][j+1] = max(dynsYCoords[bar][loopIndex][j],
                                                                                    dynsYCoords[bar][loopIndex][j+1]);
            for (int k = dynamicsIndexes[bar][j]; k < dynamicsIndexes[bar][j+1]; k++) {
              if (dynsYCoords[bar][loopIndex][i] > allNotesMaxYPos[bar][k]) {
                allNotesMaxYPos[bar][k] = dynsYCoords[bar][loopIndex][i];
              }
            }
          }
          // otherwise set the start and end of the dynamic to the same Y coord
          else {
            dynsRampStartYCoords[bar][loopIndex][i] = dynsRampEndYCoords[bar][loopIndex][i] = max(dynsRampStartYCoords[bar][loopIndex][i],
                                                                                                  dynsRampEndYCoords[bar][loopIndex][i]);
            for (int k = dynamicsIndexes[bar][j]; k < dynamicsIndexes[bar][j+1]; k++) {
              if (dynsRampStartYCoords[bar][loopIndex][i] > allNotesMaxYPos[bar][k]) {
                allNotesMaxYPos[bar][k] = dynsRampStartYCoords[bar][loopIndex][i];
              }
            }
          }
        }
        // if a ramp ends at the end of the bar and there are no more dynamics
        else {
          dynsRampStartYCoords[bar][loopIndex][i] = dynsRampEndYCoords[bar][loopIndex][i] = max(dynsRampStartYCoords[bar][loopIndex][i],
                                                                                                dynsRampEndYCoords[bar][loopIndex][i]);
          if (j < dynamicsIndexes[bar].size()-1) {
            for (int k = dynamicsIndexes[bar][j]; k < dynamicsIndexes[bar][j+1]; k++) {
              if (dynsRampStartYCoords[bar][loopIndex][i] > allNotesMaxYPos[bar][k]) {
                 allNotesMaxYPos[bar][k] = dynsRampStartYCoords[bar][loopIndex][i];
              }
            }
          }
        }
      }
    }
  }
  // check if a ramp starts without a dynamic sign, but ends at a dynamic sign
  for (unsigned i = 0; i < dynamicsRampEnd[bar].size(); i++) {
    for (unsigned j = 0; j < dynamicsIndexes[bar].size(); j++) {
      if (dynamicsRampEnd[bar][i] == dynamicsIndexes[bar][j]) {
        dynsRampEndXCoords[bar][i] -= dynSymsWidths[dynamics[bar][j]]/2.0;
        // check if the beginning of the ramp doesn't start from a dynamic sign
        if (j > 0) {
          if (dynamicsRampStart[bar][i] != dynamicsIndexes[bar][j-1]) {
            dynsRampEndYCoords[bar][loopIndex][i] = max(dynsRampEndYCoords[bar][loopIndex][i], dynsYCoords[bar][loopIndex][j]);
            dynsRampStartYCoords[bar][loopIndex][i] = dynsRampEndYCoords[bar][loopIndex][i];
            if (j < dynamicsIndexes[bar].size()-1) {
              for (int k = dynamicsIndexes[bar][j]; k < dynamicsIndexes[bar][j+1]; k++) {
                if (dynsRampStartYCoords[bar][loopIndex][i] > allNotesMaxYPos[bar][k]) {
                   allNotesMaxYPos[bar][k] = dynsRampStartYCoords[bar][loopIndex][i];
                }
              }
            }         
          }
        }
        // if j is 0 it means that we end the ramp at the first dynamic sign
        // so the ramp definitely doesn't start from a dynamic sign
        else {
          dynsRampEndYCoords[bar][loopIndex][i] = max(dynsRampEndYCoords[bar][loopIndex][i], dynsYCoords[bar][loopIndex][j]);
          dynsRampStartYCoords[bar][loopIndex][i] = dynsRampEndYCoords[bar][loopIndex][i];
          if (j < dynamicsIndexes[bar].size()-1) {
            for (int k = dynamicsIndexes[bar][j]; k < dynamicsIndexes[bar][j+1]; k++) {
              if (dynsRampStartYCoords[bar][loopIndex][i] > allNotesMaxYPos[bar][k]) {
                 allNotesMaxYPos[bar][k] = dynsRampStartYCoords[bar][loopIndex][i];
              }
            }
          }
        }
      }
    }
  }
}

void Notes::storeMinMaxY(int bar, int loopIndex) {
  for (unsigned i = 0; i < allNotes[bar].size(); i++) {
    if (allNotesMaxYPos[bar][i] > maxYCoord[bar] && !tacet[bar]) maxYCoord[bar] = allNotesMaxYPos[bar][i];
    if (allNotesMinYPos[bar][i] < minYCoord[bar] && !tacet[bar]) minYCoord[bar] = allNotesMinYPos[bar][i];
  }
  if (maxYCoord[bar] < yStartPnt[bar][loopIndex] + (staffDist*4) && !tacet[bar]) {
    maxYCoord[bar] = yStartPnt[bar][loopIndex] + (staffDist*4);
  }
  if (minYCoord[bar] > yStartPnt[bar][loopIndex] && !tacet[bar]) minYCoord[bar] = yStartPnt[bar][loopIndex];
}

void Notes::findExtremes(int bar, int loopIndex) {
  for (unsigned i = 0; i < allBeamYCoords[bar].size(); i++) {
    float extreme;
    if (grouppedStemDirs[bar][i] > 0) {
      extreme = *min_element(allBeamYCoords[bar][i].begin(), allBeamYCoords[bar][i].end());
    }
    else {
      replace(allBeamYCoords[bar][i].begin(), allBeamYCoords[bar][i].end(), INT_MAX, -INT_MAX);
      extreme = *max_element(allBeamYCoords[bar][i].begin(), allBeamYCoords[bar][i].end());
    }
    auto it = find(allBeamYCoords[bar][i].begin(), allBeamYCoords[bar][i].end(), extreme);
    int indexLocal = it - allBeamYCoords[bar][i].begin();
    // if the note with the extreme stem is the first
    // we draw a line starting from there
    if (indexLocal == 0) {
      if (grouppedStemDirs[bar][i] > 0) grouppedStemDirsCoeff[bar].push_back(1);
      else grouppedStemDirsCoeff[bar].push_back(-1);
    }
    // otherwise if the extreme stem is at the end, we draw a line ending there
    else if (indexLocal == ((int)allBeamYCoords[bar][i].size() - 1)) {
      extreme += halfStaffDist;
      if (grouppedStemDirs[bar][i] > 0) grouppedStemDirsCoeff[bar].push_back(-1);
      else grouppedStemDirsCoeff[bar].push_back(1);
    }
    else {
      int directionSign = -1;
      if (allBeamYCoords[bar][i][0] < allBeamYCoords[bar][i][(int)allBeamYCoords[bar][i].size()-1]) {
        directionSign = 1;
      }
      grouppedStemDirsCoeff[bar].push_back(directionSign);
    }
    if (hasRests[bar][i]) {
      extreme += offsetToExtreme(loopIndex, bar, i, extreme);
    }
    startPnts[bar].push_back(extreme);
  }
}

float Notes::offsetToExtreme(int loopIndex, int index1, int index2, float extreme){
  float diff = 0;
  float extremeLocal = extreme - (stemHeight * grouppedStemDirs[index1][index2]) + \
                       (((maxNumLines[index1][index2]-1)*halfStaffDist) * grouppedStemDirs[index1][index2]);
  if (grouppedStemDirs[index1][index2] > 0) {
    // if the lowest line falls on top of the rest (or lower)
    if (extremeLocal > (yStartPnt[index1][loopIndex]+halfStaffDist)) {
      diff = extremeLocal - (yStartPnt[index1][loopIndex]+halfStaffDist) + halfStaffDist;
      diff *= -1;
    }
  }
  else {
    int indexLocal = (log(hasRests[index1][index2]) / log(2)) - 1;
    float restLowestPnt = notationFont.stringHeight(restsSyms[indexLocal]);
    if (extremeLocal < (yStartPnt[index1][loopIndex]+halfStaffDist+restLowestPnt)) {
      diff = (yStartPnt[index1][loopIndex]+halfStaffDist+restLowestPnt) - extremeLocal + halfStaffDist;
    }
  }
  return diff;
}

void Notes::drawNotes(int bar, int loopIndex){
  if (!mute) {
    for (unsigned i = 0; i < allNotes[bar].size(); i++) {
      int restsColor = 0;
      for (unsigned j = 0; j < allNotes[bar][i].size(); j++) {
        if (hasExtraLines[bar][i][j]) {
          float xStart = allNotesXCoords[bar][i][j] - (noteWidth*EXTRALINECOEFF);
          float yPos = extraLinesYPos[bar][i][j];
          // draw the extra lines first so the animation is drawn on top
          for (int k = 0; k < numExtraLines[bar][i][j]; k++) {
            yPos += (staffDist * extraLinesDir[bar][i][j]);
            ofDrawLine(xStart, yPos, allNotesXCoords[bar][i][j]+(noteWidth*EXTRALINECOEFF), yPos);
          }
        }
        if (j == 0) {
          // then set the animation color
          if (animate && whichNote == (int)i) {
            ofSetColor(255, 0, 0);
            restsColor = 255;
          }
        }
        // then draw all the notes heads and rests
        if (allNotes[bar][i][j] > -1) {
          int noteIdx;
          switch (actualDurs[bar][i]) {
            case 1:
              noteIdx = 0;
              break;
            case 2:
              noteIdx = 1;
              break;
            default:
              noteIdx = 2;
              break;
          }
				  float x = allNotesXCoords[bar][i][j];
				  float y = allNotesYCoords1[bar][loopIndex][i][j]+(noteHeight/8.0)-(staffDist/10.0);
          notationFont.drawString(notesSyms[noteIdx], x-(noteWidth/2.0), y);
				  if (dotIndexes[bar][i] == 1) {
            // if the note is on a line
            if (((int)((y - yStartPnt[bar][loopIndex]) / halfStaffDist) % 2) == 0) {
              // add a small offset to the dot
              y -= halfStaffDist;
            }
					  ofDrawCircle(x+(noteWidth/1.5), y, staffDist*0.2);
				  }
        }
      }
      if (allNotes[bar][i][0] == -1) {
			  float x = allNotesXCoords[bar][i][whichIdxBase[bar][i]];
        int restIndex = drawRest(bar, loopIndex, actualDurs[bar][i], x, restsColor);
			  if (dotIndexes[bar][i] == 1) {
				  ofDrawCircle(x+(notationFont.stringWidth(restsSyms[restIndex])*1.2), middleOfStaff, staffDist*0.2);
			  }
      }
      // then draw the stems
      if (hasStem[bar][i]) {
        ofDrawLine(stemsX[bar][i], stemsY1[bar][loopIndex][i], stemsX[bar][i], stemsY2[bar][loopIndex][i]);
      }
		  // then draw the individual tails
		  if (numTails[bar][i] != 0) {
			  int symIdx = 3;
			  int tailDir = 1;
			  float xPos = stemsX[bar][i];
			  float yPos = stemsY2[bar][loopIndex][i];
        // minor corrections on the tails coordinates need to be made here
        // because if done in setNotePositions(), then the stem won't be drawn correctly
			  if (numTails[bar][i] < 0) {
				  symIdx = 4;
				  tailDir = -1;
          yPos -= (staffDist*3);
			  }
        else {
          xPos -= (noteWidth - lineWidth);
          yPos += (staffDist*3);
        }
			  int loopIter = abs(numTails[bar][i]);
			  for (int j = 0; j < loopIter; j++) {
				  notationFont.drawString(notesSyms[symIdx], xPos, yPos+((j*halfStaffDist)*tailDir));
			  }
		  }
      // reset the animation color
      if (animate && whichNote == (int)i) {
        ofSetColor(0);
      }
    }
    // then draw the beams
    if (beamsX[bar].size() > 0) {
      // we have stored pairs of connected notes (the two extremes in a group)
      // so we need to run the loop as many times as half the number of elements
      for (int i = 0; i < (int)beamsX[bar].size()/2; i++) {
        // don't draw beams for single notes that are not groupped
        if (numNotesWithBeams[bar][i] > 1) {
          for (unsigned j = 0; j < allBeamYCoords[bar][i].size()-1; j++) {
            int indexLocal;
            // the note with the fewer lines will set the
            // number of iterations in the loop below
            if (numBeams[bar][i][j] <= numBeams[bar][i][j+1]) {
              indexLocal = j;
            }
            else {
              indexLocal = j + 1;
            }
            float yCoord1 = beamYCoords1[bar][loopIndex][i][j];
            float yCoord2 = beamYCoords2[bar][loopIndex][i][j];
            // separate variable for correct offsetting in the loop below
            float yCoordOffset1;
            float yCoordOffset2;
            for (int k = 0; k < numBeams[bar][i][indexLocal]; k++) {
              yCoordOffset1 = yCoord1 + ((k*halfStaffDist) * grouppedStemDirs[bar][i]);
              yCoordOffset2 = yCoord2 + ((k*halfStaffDist) * grouppedStemDirs[bar][i]);
              drawBeams(beamXCoords1[bar][i][j], yCoordOffset1, beamXCoords2[bar][i][j], yCoordOffset2);
            }
          }
        }
      }
      }
    drawAccidentals(bar, loopIndex);
    drawGlissandi(bar, loopIndex);
    drawArticulations(bar, loopIndex);
	  drawText(bar, loopIndex);
    drawSlurs(bar, loopIndex);
    drawDynamics(bar, loopIndex);
  }
  else {
    int restColor = 0;
    float xPos = xStartPnt[bar] + ((xEndPnt[bar]-xStartPnt[bar])/2);
    if (animate) restColor = 255;
    drawRest(bar, loopIndex, 1, xPos, restColor);
  }
}

int Notes::drawRest(int bar, int loopIndex, int restDur, float x, int color){
  ofSetColor(color, 0, 0);
	int indexLocal;
  if (restDur < 4) {
		indexLocal = 0;
    notationFont.drawString(restsSyms[indexLocal], x-(noteHeight/2),
                            yStartPnt[bar][loopIndex]+staffDist+\
														halfStaffDist+((staffDist/2.0)*(restDur-1)));
  }
  else {
    indexLocal = (log(restDur) / log(2)) - 1;
    // for an offset of 3 for the eigth rest, every other rest must go
    // 12 pixels lower, and for 4 pxs this offset is 13, so every
    // time it's plus 9
    float extraOffset;
    if (indexLocal == 1) {
      extraOffset = 16;
      if (restYOffset > 3) {
        // add 2 for every 1 increment of the restYOffset
        extraOffset += ((restYOffset-3) * 2);
      }
    }
    else {
      extraOffset = restYOffset + 9;
      extraOffset *= (indexLocal-2);
      extraOffset += restYOffset;
      extraOffset += halfStaffDist;
      extraOffset -= ((indexLocal-2)*(restYOffset-2)*3);
    }
    notationFont.drawString(restsSyms[indexLocal], x-((noteHeight*1.5)/2.0),
                            yStartPnt[bar][loopIndex]+staffDist+extraOffset);
  }
  ofSetColor(0);
	return indexLocal;
}

void Notes::drawBeams(float x1, float y1, float x2, float y2) {
  // draw a thick line without affecting the thickness
  // of all the other lines
  // taken from
  // https://forum.openframeworks.cc/t/how-do-i-draw-lines-of-a-
  // reasonable-thickness-on-a-very-large-canvas-given-opengl-
  // constraints/30815/7
  ofPoint a;
  ofPoint b;
  a.x = x1;
  a.y = y1;
  b.x = x2;
  b.y = y2;
  ofPoint diff = (a - b).getNormalized();
  diff.rotate(90, ofPoint(0,0,1));
  ofMesh m;
  m.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);
  m.addVertex(a + diff * beamsLineWidth);
  m.addVertex(a - diff * beamsLineWidth);
  m.addVertex(b + diff * beamsLineWidth);
  m.addVertex(b - diff * beamsLineWidth);
  m.draw();
}

void Notes::drawAccidentals(int bar, int loopIndex){
  for (unsigned i = 0; i < allAccidentals[bar].size(); i++) {
    for (unsigned j = 0; j < allAccidentals[bar][i].size(); j++) {
      // escape dummy accentals
      if ((allAccidentals[bar][i][j]%2) == 0) {
        // don't draw the natural
        if (allAccidentals[bar][i][j] != 4) {
          notationFont.drawString(accSyms[allAccidentals[bar][i][j]],
                                  allNotesXCoords[bar][i][j]-(noteWidth*1.2),
                                  allNotesYCoords1[bar][loopIndex][i][j]);
        }
      }
    }
  }
}

void Notes::drawGlissandi(int bar, int loopIndex){
  float xStart, xEnd;
  float yStart, yEnd;
  for (unsigned i = 0; i < allGlissandi[bar].size(); i++) {
    if (allGlissandi[bar][i] > 0) {
      xStart = allNotesXCoords[bar][i][whichIdxBase[bar][i]];
      xEnd = allNotesXCoords[bar][i+1][whichIdxBase[bar][i]] - noteWidth;
      yStart = allNotesYCoords1[bar][loopIndex][i][whichIdxBase[bar][i]];
      yEnd = allNotesYCoords1[bar][loopIndex][i+1][whichIdxBase[bar][i]];
      // if we have an accidental at the end of the gliss
      if ((allAccidentals[bar][i+1][whichIdxBase[bar][i]]%2) == 0) {
        if (allAccidentals[bar][i+1][whichIdxBase[bar][i]] != 4) {
          // this is taken from the drawAccidentals() function
          xEnd = allNotesXCoords[bar][i+1][whichIdxBase[bar][i]]-(noteWidth*1.2);
          if (stemDirections[bar][i+1] < 0) {
            xEnd += staffDist;
          }
          xEnd -= (notationFont.stringWidth(accSyms[allAccidentals[bar][i+1][whichIdxBase[bar][i]]])/2.0);
        }
      }
      if (stemDirections[bar][i] > 0) {
        xStart += (noteWidth/2);
      }
      else {
        xStart += noteWidth;
      }
      // check if the start and end notes are on a line
      if (((int)(abs(yStart - yStartPnt[bar][loopIndex]) / halfStaffDist) % 2) == 0) {
        float glissLineOffset = staffDist / 4.0;
        if (yStart > yEnd) {
          yStart -= glissLineOffset;
        }
        else {
          yStart += glissLineOffset;
        }
      }
      if (((int)(abs(yEnd - yStartPnt[bar][loopIndex]) / halfStaffDist) % 2) == 0) {
        float glissLineOffset = staffDist / 4.0;
        if (yEnd > yStart) {
          yEnd -= glissLineOffset;
        }
        else {
          yEnd += glissLineOffset;
        }
      }
      ofDrawLine(xStart, yStart, xEnd, yEnd);
    }
  }
}

void Notes::drawArticulations(int bar, int loopIndex){
  for (unsigned i = 0; i < allArticulations[bar].size(); i++) {
    if (allArticulations[bar][i] > 0) {
      if (allArticulations[bar][i] < 7) {
        // we don't have a symbol for no articulation so bar with allArticulations[i]-1
        string articulStr = articulSyms[allArticulations[bar][i]-1];
        if ((allArticulations[bar][i] == 1) && (stemDirections[bar][i] > 0)) {
          // the marcato sign needs to be inverted in case of being projected under the note
          ofPushMatrix();
          ofTranslate(articulXPos[bar][i], articulYPos[bar][loopIndex][i]);
          ofRotateXDeg(180);
          notationFont.drawString(articulStr, 0, 0);
          ofPopMatrix();
        }
        else {
          notationFont.drawString(articulStr, articulXPos[bar][i], articulYPos[bar][loopIndex][i]);
        }
      }
      else {
        float offset;
        if (stemDirections[bar][i] > 0) {
          offset = halfStaffDist;
        }
        else {
          offset = -halfStaffDist;
        }
        notationFont.drawString(".", articulXPos[bar][i]+(noteWidth/2), articulYPos[bar][loopIndex][i]);
        notationFont.drawString("_", articulXPos[bar][i], articulYPos[bar][loopIndex][i]+offset);
      }
    }
  }
}

void Notes::drawText(int bar, int loopIndex){
	for (unsigned i = 0; i < allTextsXCoords[bar].size(); i++) {
		textFont.drawString(allTexts[bar][i], allTextsXCoords[bar][i], allTextsYCoords[bar][loopIndex][i]);
	}
}

void Notes::drawSlurs(int bar, int loopIndex) {
  ofNoFill();
  for (unsigned i = 0; i < slurStartX[bar].size(); i++) {
    float x0 = slurStartX[bar][i];
    float y0 = slurStartY[bar][loopIndex][i];
    float x1 = slurMiddleX1[bar][i];
    float y1 = slurMiddleY1[bar][loopIndex][i];
    float x2 = slurMiddleX2[bar][i];
    float y2 = slurMiddleY2[bar][loopIndex][i];
    float x3 = slurStopX[bar][i];
    float y3 = slurStopY[bar][loopIndex][i];
    ofBeginShape();
    ofVertex(x0,y0);
  	ofBezierVertex(x1,y1,x2,y2,x3,y3);
    ofEndShape();
  }
  ofFill();
}

void Notes::drawDynamics(int bar, int loopIndex){
  for (unsigned i = 0; i < dynamics[bar].size(); i++) {
    notationFont.drawString(dynSyms[dynamics[bar][i]], dynsXCoords[bar][i], dynsYCoords[bar][loopIndex][i]);
  }
  for (unsigned i = 0; i < dynamicsRampStart[bar].size(); i++) {
    for (int j = 0; j < 2; j++) {
      ofDrawLine(dynsRampStartXCoords[bar][i],
                 dynsRampStartYCoords[bar][loopIndex][i]+(staffDist*(((dynamicsRampDir[bar][i]*(-1))+1)*((j*2)-1))),
                 dynsRampEndXCoords[bar][i],
                 dynsRampEndYCoords[bar][loopIndex][i]+(staffDist*(dynamicsRampDir[bar][i]*((j*2)-1))));
    }
  }
}

void Notes::drawTails(float xPos, float yPos, int numTails, int stemDir){
  int idx = 3;
  if (stemDir < 0) idx = 4;
  for (int i = 0; i < numTails; i++) {
    notationFont.drawString(notesSyms[idx], xPos, yPos+((i*halfStaffDist)*stemDir));
  }
}

void Notes::printVector(vector<int> v){
	for (unsigned i = 0; i < v.size(); i++) {
		cout << v[i] << " ";
	}
	cout << endl;
}

void Notes::printVector(vector<string> v){
	for (unsigned i = 0; i < v.size(); i++) {
		cout << v[i] << " ";
	}
	cout << endl;
}

void Notes::printVector(vector<float> v){
	for (unsigned i = 0; i < v.size(); i++) {
		cout << v[i] << " ";
	}
	cout << endl;
}

Notes::~Notes(){}
