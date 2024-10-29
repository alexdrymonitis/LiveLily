#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
	lastInstrumentIndex = 0;
	correctOnSameOctaveOnly = true;
	sharedData.numInstruments = 0;
	sharedData.tempoMs[0] = 500;
	sharedData.BPMMultiplier[0] = 1;
	sharedData.beatAtDifferentThanDivisor[0] = false;
	sharedData.numerator[0] = 4;
	sharedData.denominator[0] = 4;
	sharedData.numBeats[0] = 256;
	
	sharedData.loopIndex = 0;
	sharedData.tempBarLoopIndex = 0;
	sharedData.thisLoopIndex = 0;
	sharedData.beatCounter = 0; // this is controlled by the sequencer
	sharedData.barCounter = 0; // also controlled by the sequencer
	
	lineWidth = 2;
	brightnessCoeff = 0.75;
	// dummy	
	// set the notes chars
	for (int i = 2; i < 9; i++) {
		noteChars[i-2] = char((i%7)+97);
	}
	noteChars[7] = char(114); // "r" for rest
	initializeInstrument("\\inst");
	sharedData.numBeats[-1] = 256;
	cout << "initialized instrument, will parse line\n";
	intStrPair p = parseMelodicLine("<c' ees'>8-.-> <c' ees'>8 <g' a' des''>4 <c' ees'>8-.-> <c' ees'>8-.-> <g' a' des''>4");
	cout << "error code: " << p.first << ", error message: " << p.second << endl;
}

//--------------------------------------------------------------
void ofApp::update(){

}

//--------------------------------------------------------------
void ofApp::draw(){

}

//--------------------------------------------------------------
void ofApp::setScoreCoords()
{

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
	setScoreCoords();
}

//--------------------------------------------------------------
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
intStrPair ofApp::parseMelodicLine(string str)
{
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
	string strToProcess = str;
	vector<string> tokens = tokenizeString(strToProcess, " ");
	// index variables for loop so that they are initialized before vectors with unkown size
	unsigned i = 0, j = 0; //  initialize to avoid compiler warnings
	// then detect any chords, as the size of most vectors equals the number of notes vertically
	// and not single notes within chords
	unsigned numNotesVertical = tokens.size();
	unsigned chordNotesCounter = 0;
	unsigned numErasedOttTokens = 0;
	unsigned chordOpeningNdx;
	unsigned chordClosingNdx;
	// characters that are not permitted inside a chord are
	// open/close parenthesis, hyphen, dot, backslash, carret, underscore, open/close curly brackets
	int forbiddenCharsInChord[9] = {40, 41, 45, 46, 92, 94, 95, 123, 125};
	for (i = 0; i < tokens.size(); i++) {
		if (tokens.at(i).compare("\\ottava") == 0 || tokens.at(i).compare("\\ott") == 0) {
			if (tokens.size() < i+2) {
				intStrPair p;
				p.first = 3;
				p.second = "\\ottava must be followed by (-)1 or (-)2";
				return p;
			}
			if (!isNumber(tokens.at(i+1))) {
				intStrPair p;
				p.first = 3;
				p.second = "\\ottava must be followed by (-)1 or (-)2";
				return p;
			}
			numErasedOttTokens += 2;
			i += 1;
			continue;
		}
		if (tokens.at(i).compare("\\tuplet") == 0 || tokens.at(i).compare("\\tup") == 0) {
			if (tokens.size() < i+3) {
				intStrPair p;
				p.first = 3;
				p.second = "\\tuplet must be followed by ratio and the actual tuplet";
				return p;
			}
			if (tokens.at(i+1).find("/") == string::npos) {
				intStrPair p;
				p.first = 3;
				p.second = "\\tuplet ratio formatted wrong";
				return p;
			}
			size_t divisionNdx = tokens.at(i+1).find("/");
			size_t remainingStr = tokens.at(i+1).size() - divisionNdx - 1;
			if (!isNumber(tokens.at(i+1).substr(0, divisionNdx)) || !isNumber(tokens.at(i+1).substr(divisionNdx+1, remainingStr))) {
				intStrPair p;
				p.first = 3;
				p.second = "numerator or denominator of tuplet ratio is not a number";
				return p;
			}
			if (!startsWith(tokens[i+2], "{")) {
				intStrPair p;
				p.first = 3;
				p.second = "tuplet notes must be placed in curly brackets";
				return p;
			}
		}
		if (chordStarted) {
			bool chordEnded = false;
			for (j = 0; j < 9; j++) {
				if (tokens.at(i).substr(0, tokens.at(i).find(">")).find(char(forbiddenCharsInChord[j])) != string::npos) {
					intStrPair p;
					p.first = 3;
					p.second = char(forbiddenCharsInChord[j]) + (string)" can't be included within a chord, only outside of it";
					return p;
				}
			}
			for (j = 0; j < tokens.at(i).size(); j++) {
				if (tokens.at(i).at(j) == '>') chordEnded = true;
				else if (isdigit(tokens.at(i).at(j)) && !chordEnded) {
					intStrPair p;
					p.first = 3;
					p.second = "durations can't be included within a chord, only outside of it";
					return p;
				}
			}
			chordNotesCounter++;
		}
		// if we find a quote and we're not at the last token
		chordOpeningNdx = tokens.at(i).find("<");
		chordClosingNdx = tokens.at(i).find(">");
		// an opening angle brace is also used for crescendi, but when used to start a chord
		// it must be at the beginning of the token, so we test if its index is 0
		if (chordOpeningNdx != string::npos && chordOpeningNdx == 0 && i < tokens.size()) {
			if (chordStarted) {
				if (chordOpeningNdx > 0) {
					if (tokens.at(i)[chordOpeningNdx-1] == '\\') {
						intStrPair p;
						p.first = 3;
						p.second = "crescendi can't be included within a chord, only outside of it";
						return p;
					}
					else {
						intStrPair p;
						p.first = 3;
						p.second = "chords can't be nested";
						return p;
					}
				}
				else {
					intStrPair p;
					p.first = 3;
					p.second = "chords can't be nested";
					return p;
				}
			}
			if (sharedData.instruments.at(lastInstrumentIndex).isRhythm()) {
				intStrPair p;
				p.first = 3;
				p.second = "chords are not allowed in rhythm staffs";
				return p;
			}
			chordStarted = true;
		}
		else if (chordClosingNdx != string::npos && chordClosingNdx < tokens.at(i).size()) {
			if (chordClosingNdx == 0) {
				intStrPair p;
				p.first = 3;
				p.second = "chord closing character can't be at the beginning of a token";
				return p;
			}
			else if (chordStarted && tokens.at(i)[chordClosingNdx-1] == '\\') {
				intStrPair p;
				p.first = 3;
				p.second = "diminuendi can't be included within a chord, only outside of it";
				return p;
			}
			else if (!chordStarted && tokens.at(i)[chordClosingNdx-1] != '-' && tokens.at(i)[chordClosingNdx-1] != '\\') {
				intStrPair p;
				p.first = 3;
				p.second = "can't close a chord that hasn't been opened";
				return p;
			}
			chordStarted = false;
		}
	}
	if (chordStarted) {
		intStrPair p;
		p.first = 3;
		p.second = "chord left open";
		return p;
	}
	if (tokens.size() == numErasedOttTokens) { // in case we only provide the ottava
		intStrPair p;
		p.first = 0;
		p.second = "";
		return p;
	}
	numNotesVertical -= (chordNotesCounter + numErasedOttTokens);

	// check for tuplets
	int tupMapNdx = 0;
	map<int, intPair> tupRatios;
	for (i = 0; i < tokens.size(); i++) {
		if (tokens.at(i).compare("\\tuplet") == 0 || tokens.at(i).compare("\\tup") == 0) {
			// tests whether the tuplet format is correct have been made above
			// so we can safely query tokens.at(i+1) etc
			size_t divisionNdx = tokens.at(i+1).find("/");
			size_t remainingStr = tokens.at(i+1).size() - divisionNdx - 1;
			intPair p;
			p.first = stoi(tokens.at(i+1).substr(0, divisionNdx));
			p.second = stoi(tokens.at(i+1).substr(divisionNdx+1, remainingStr));
			tupRatios[tupMapNdx++] = p;
		}
	}

	// store the indexes of the tuplets beginnings and endings so we can erase all the unnessacary tokens
	unsigned tupStart, tupStop = 0; // assign 0 to tupStop to stop the compiler from throwing a warning message
	int tupStartHasNoBracket = 0, tupStopHasNoBracket = 0;
	int numErasedTupTokens = 0;
	tupMapNdx = 0;
	map<int, uintPair> tupStartStop;
	for (i = 0; i < tokens.size(); i++) {
		if (tokens.at(i).compare("\\tuplet") == 0 || tokens.at(i).compare("\\tup") == 0) {
			if (tokens[i+2].compare("{") == 0) {
				tupStart = i+3;
				tupStartHasNoBracket = 1;
			}
			else {
				tupStart = i+2;
			}
			for (j = i+2+tupStartHasNoBracket; j < tokens.size(); j++) {
				if (tokens.at(j).find("}") != string::npos) {
					if (tokens.at(j).compare("}") == 0) {
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
			uintPair p;
			p.first = tupStart;
			p.second = tupStop;
			tupStartStop[tupMapNdx++] = p;
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
	vector<int> dynamicsRampStartForScore(numNotesVertical, -1);
	vector<int> dynamicsRampEndForScore(numNotesVertical, -1);
	vector<int> dynamicsRampDirForScore(numNotesVertical, -1);
	vector<bool> isSlurred (numNotesVertical, false);
	vector<int> notesCounter(tokens.size()-numErasedOttTokens, 0);
	//int verticalNotesIndexes[tokens.size()-numErasedOttTokens+1] = {0}; // plus one to easily break out of loops in case of chords
	vector<int> verticalNotesIndexes(tokens.size()-numErasedOttTokens+1, 0);
	//int beginningOfChords[tokens.size()-numErasedOttTokens] = {0};
	vector<int> beginningOfChords(tokens.size()-numErasedOttTokens, 0);
	//int endingOfChords[tokens.size()-numErasedOttTokens] = {0};
	vector<int> endingOfChords(tokens.size()-numErasedOttTokens, 0);
	vector<unsigned> accidentalIndexes(tokens.size()-numErasedOttTokens);
	//unsigned chordNotesIndexes[tokens.size()-numErasedOttTokens] = {0};
	vector<unsigned> chordNotesIndexes(tokens.size()-numErasedOttTokens, 0);
	vector<int> ottavas(numNotesVertical, 0);
	int verticalNoteIndex;
	// create variables for the loop below but outside of it, so they are kept in the stack
	// this way we are sure the push_back() calls won't cause any problems
	bool foundNotes[i];
	//int transposedOctaves[i-numErasedOttTokens] = {0};
	vector<int> transposedOctaves(i-numErasedOttTokens, 0);
	//int transposedAccidentals[i-numErasedOttTokens] = {0};
	vector<int> transposedAccidentals(i-numErasedOttTokens, 0);
	// variable to determine which character we start looking at
	// useful in case we start a bar with a chord
	//unsigned firstChar[tokens.size()-numErasedOttTokens] = {0};
	vector<unsigned> firstChar(tokens.size()-numErasedOttTokens, 0);
	vector<unsigned> firstCharOffset(tokens.size()-numErasedOttTokens, 1); // this is zeroed in case of a rhythm staff with durations only
	//for (i = 0; i < tokens.size()-numErasedOttTokens; i++) firstCharOffset.at(i) = 1;
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
	vector<vector<intPair>> notePairs;
	// then iterate over all the tokens and parse them to store the notes
	for (i = 0; i < tokens.size(); i++) {
		if (erasedTokens > 0) {
			// go back as many indexes as we have erased in case we have an \ottava or \tuplet 
			// the loop returned to its last field that incremented i, so we need to decrement it
			i -= erasedTokens;
			erasedTokens = 0;
		}
		if (tokens.at(i).compare("\\ottava") == 0 || tokens.at(i).compare("\\ott") == 0) {
			// tests whether a digit follows \ottava have been made above
			// so we can now safely query tokens.at(i+1) and extract the digit that follows
			ottava = stoi(tokens.at(i+1));
			if (abs(ottava) > 2) {
				intStrPair p;
				p.first = 3;
				p.second = "up to two octaves up/down can be inserted with \\ottava";
				return p;
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
		if (startsWith(tokens.at(i), "{")) tokens.at(i) = tokens.at(i).substr(1);
		if (endsWith(tokens.at(i), "}")) tokens.at(i) = tokens.at(i).substr(0, tokens.at(i).size()-1);
		verticalNoteIndex = -1;
		for (j = 0; j <= i; j++) {
			// loop till we find a -1, which means that this token is a chord note, but not the first one
			if (verticalNotesIndexes.at(j) < 0) continue;
			verticalNoteIndex++;
		}
		ottavas.at(verticalNoteIndex) = ottava;
		firstChar.at(i) = 0;
		// first check if the token is a comment so we exit the loop
		if (startsWith(tokens.at(i), "%")) continue;
		foundNotes[i] = false;
		transposedOctaves.at(index1) = 0;
		transposedAccidentals.at(index1) = 0;
		// the first element of each token is the note
		// so we first check for it in a separate loop
		// and then we check for the rest of the stuff
		for (j = 0; j < 8; j++) {
			if (tokens.at(i)[firstChar.at(i)] == char(60)) { // <
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
					if (sharedData.instruments.at(lastInstrumentIndex).isRhythm()) {
						midiNote = 59;
						naturalScaleNote = 6;
					}
					else if (sharedData.instruments.at(lastInstrumentIndex).getTransposition() != 0) {
						int transposedMidiNote = midiNotes[j] + sharedData.instruments.at(lastInstrumentIndex).getTransposition();
						// testing against the midiNote it is easier to determine whether we need to add an accidental
						if (transposedMidiNote < 0) {
							transposedMidiNote = 12 + midiNotes[j] + sharedData.instruments.at(lastInstrumentIndex).getTransposition();
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
					intPair p;
					p.first = midiNote;
					p.second = naturalScaleNote;
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
					textIndexesLocal.at(index2) = 0;
					index2++;
					if (firstChordNote) firstChordNote = false;
				}
				else if (chordStarted && !firstChordNote) {
					// if we have a chord, push this note to the current vector
					intPair p;
					p.first = midiNote;
					p.second = naturalScaleNote;
					notePairs.back().push_back(p);
					// a -1 will be filtered out further down in the code
					verticalNotesIndexes.at(index1) = -1;
					// increment the counter of the chord notes
					// so we can set the index for each note in a chord properly
					chordNotesCounter++;
					chordNotesIndexes.at(index1) += chordNotesCounter;
                    notesCounter.at(index1)++;
				}
				else {
					intStrPair p;
					p.first = 3;
					p.second = "something went wrong";
					return p;
				}
				foundNotes[i] = true;
				break;
			}
		}
		if (!foundNotes[i]) {
			if (sharedData.instruments.at(lastInstrumentIndex).isRhythm()) {
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
					else {
						intStrPair p;
						p.first = 3;
						p.second = (string)"first character must be a note or a chord opening symbol (<), not \"" + tokens.at(i).at(0) + (string)"\"";
						return p;
					}
				}
				else if (isdigit(tokens.at(i).at(0))) {
					dur = int(tokens.at(i).at(0));
				}
				else {
					intStrPair p;
					p.first = 3;
					p.second = (string)"first character must be a note or a chord opening symbol (<), not \"" + tokens.at(i).at(0) +(string)"\"";
					return p;
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
				else {
					intStrPair p;
					p.first = 3;
					p.second = std::to_string(dur) + ": wrong duration";
					return p;
				}
			}
			else {
				intStrPair p;
				p.first = 3;
				p.second = (string)"first character must be a note or a chord opening symbol (<), not \"" + tokens.at(i).at(0) + (string)"\"";
				return p;
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
	//auto midiNotesData = std::make_shared<vector<vector<int>>>();
	//for (i = 0; i < notePairs->size(); i++) {
	//	midiNotesData->push_back({notePairs->at(i).at(0).first});
	//	for (j = 1; j < notePairs->at(i).size(); j++) {
	//		midiNotesData->back().push_back(notePairs->at(i).at(j).first);
	//	}
	//}
	//auto notesData = std::make_shared<vector<vector<float>>>();
	vector<vector<float>> notesData;
	//for (i = 0; i < midiNotesData.size(); i++) {
	//	notesData.push_back({(float)midiNotesData.at(i).at(0)});
	//	for (j = 1; j < midiNotesData.at(i).size(); j++) {
	//		notesData.back().push_back((float)midiNotesData.at(i).at(j));
	//	}
	//}
	for (i = 0; i < midiNotesData.size(); i++) {
		float value = midiNotesData.at(i).at(0);
		std::vector<float> aVector(1, value);
		notesData.push_back(std::move(aVector));
		//notesData.emplace_back(std::vector<float>(1, value));
		//notesData.push_back({(float)midiNotesData.at(i).at(0)});
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
		//accidentalsForScore.emplace_back(vector<int>(1, -1));
		//accidentalsForScore.push_back({-1});
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
		//naturalSignsNotWrittenForScore.emplace_back(vector<int>(1, 0));
		//naturalSignsNotWrittenForScore.push_back({0});
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
			//octavesForScore.emplace_back(vector<int>(1, 1));
			//octavesForScore.push_back({1});
		}
		else {
			std::vector<int> aVector(1, transposedOctaves[transposedIndex++]);
			octavesForScore.push_back(std::move(aVector));
			//octavesForScore.emplace_back(vector<int>(1, transposedOctaves[transposedIndex++]));
			//octavesForScore.push_back({transposedOctaves[transposedIndex++]});
		}
		for (j = 1; j < midiNotesData.at(i).size(); j++) {
			octavesForScore.back().push_back(transposedOctaves[transposedIndex++]);
		}
	}

	// before we move on to the rest of the data, we need to store the texts
	// because this is dynamically allocated memory too
	unsigned openQuote, closeQuote;
	vector<string> texts;
	for (i = 0; i < tokens.size(); i++) {
		// again, check if we have a comment, so we exit the loop
		if (startsWith(tokens.at(i), "%")) break;
		verticalNoteIndex = 0;
		for (unsigned k = 0; k <= i; k++) {
			// loop till we find a -1, which means that this token is a chord note, but not the first one
			if (verticalNotesIndexes.at(k) < 0) continue;
			verticalNoteIndex = k;
		}
		for (j = 0; j < tokens.at(i).size(); j++) {
			// ^ or _ for adding text above or below the note
			if ((tokens.at(i).at(j) == char(94)) || (tokens.at(i).at(j) == char(95))) {
				if (j > 0) {
					if (tokens.at(i).at(j-1) == '-') foundArticulation = true;
				}
				if (j >= (tokens.at(i).size()-1) && !foundArticulation) {
					if (tokens.at(i).at(j) == char(94)) {
						intStrPair p;
						p.first = 3;
						p.second = "a carret must be followed by text in quotes";
						return p;
					}
					else {
						intStrPair p;
						p.first = 3;
						p.second = "an undescore must be followed by text in quotes";
						return p;
					}
				}
				if (j < tokens.at(i).size()-1) {
					if (tokens.at(i).at(j+1) != char(34) && !foundArticulation) { // "
						if (tokens.at(i).at(j) == char(94)) {
							intStrPair p;
							p.first = 3;
							p.second = "a carret must be followed by a bouble quote sign";
							return p;
						}
						else {
							intStrPair p;
							p.first = 3;
							p.second = "an underscore must be followed by a bouble quote sign";
							return p;
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
						intStrPair p;
						p.first = 3;
						p.second = "text must be between two double quote signs";
						return p;
					}
					texts.push_back(tokens.at(i).substr(openQuote, closeQuote-openQuote));
					if (tokens.at(i).at(j) == char(94)) {
						textIndexesLocal.at(verticalNoteIndex) = 1;
					}
					else if (tokens.at(i).at(j) == char(95)) {
						textIndexesLocal.at(verticalNoteIndex) = -1;
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
		textsForScore.push_back(texts.at(i));
	}

	// lastly, store articulation symbols, as these have to be allocated dynamically too
	vector<vector<int>> articulationIndexes;
	for (i = 0; i < tokens.size(); i++) {
		unsigned firstArticulIndex = 0;
		verticalNoteIndex = -1;
		for (j = 0; j <= i; j++) {
			// loop till we find a -1, which means that this token is a chord note, but not the first one
			if (verticalNotesIndexes.at(j) < 0) continue;
			verticalNoteIndex++;
		}
		if ((int)articulationIndexes.size() <= verticalNoteIndex) {
			vector<int> aVector(1, 0);
			articulationIndexes.push_back(std::move(aVector));
		}
		for (j = 0; j < tokens.at(i).size(); j++) {
			if (tokens.at(i).at(j) == char(45)) { // - for articulation symbols
				if (j > (tokens.at(i).size()-1)) {
					intStrPair p;
					p.first = 3;
					p.second = "a hyphen must be followed by an articulation symbol";
					return p;
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
					default:
						intStrPair p;
						p.first = 3;
						p.second = "unknown articulation symbol";
						return p;
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
	//int foundAccidentals[tokens.size()] = {0};
	vector<int> foundAccidentals(tokens.size(), 0);
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
			if (verticalNotesIndexes.at(j) < 0) continue;
			verticalNoteIndex++;
		}
		// first check for accidentals, if any
		accidentalIndexes.at(i) = firstChar.at(i) + firstCharOffset.at(i);
		while (accidentalIndexes.at(i) < tokens.at(i).size()) {
			if (tokens.at(i)[accidentalIndexes.at(i)] == char(101)) { // 101 is "e"
				if (accidentalIndexes.at(i) < tokens.at(i).size()-1) {
					// if the character after "e" is "s" or "h" we have an accidental
					if (tokens.at(i)[accidentalIndexes.at(i)+1] == char(115)) { // 115 is "s"
						accidental -= 1.0; // in which case we subtract one semitone
						foundAccidentals.at(i) = 1;
					}
					else if (tokens.at(i)[accidentalIndexes.at(i)+1] == char(104)) { // 104 is "h"
						accidental -= 0.5;
						foundAccidentals.at(i) = 1;
					}
					else {
						intStrPair p;
						p.first = 3;
						p.second = tokens.at(i)[accidentalIndexes.at(i)+1] + (string)": unknown accidental character";
						return p;
					}
				}
				else {
					intStrPair p;
					p.first = 3;
					p.second = "\"e\" must be followed by \"s\" or \"h\"";
					return p;
				}
			}
			else if (tokens.at(i)[accidentalIndexes.at(i)] == char(105)) { // 105 is "i"
				if (accidentalIndexes.at(i) < tokens.at(i).size()-1) {
					if (tokens.at(i)[accidentalIndexes.at(i)+1] == char(115)) { // 115 is "s"
						accidental += 1.0; // in which case we add one semitone
						foundAccidentals.at(i) = 1;
					}
					else if (tokens.at(i)[accidentalIndexes.at(i)+1] == char(104)) { // 104 is "h"
						accidental += 0.5;
						foundAccidentals.at(i) = 1;
					}
					else {
						intStrPair p;
						p.first = 3;
						p.second = tokens.at(i)[accidentalIndexes.at(i)+1] + (string)": unknown accidental character";
						return p;
					}
				}
				else {
					intStrPair p;
					p.first = 3;
					p.second = "\"i\" must be followed by \"s\" or \"h\"";
					return p;
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
			//notesData.at(verticalNoteIndex)[chordNotesIndexes.at(i)] += accidental;
			notesData.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) += accidental;
			//midiNotesData.at(verticalNoteIndex)[chordNotesIndexes.at(i)] += (int)accidental;
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
			//accidentalsForScore.at(verticalNoteIndex)[chordNotesIndexes.at(i)] += transposedAccidentals.at(i);
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
	
	//int foundDynamics[tokens.size()] = {0};
	vector<int> foundDynamics(tokens.size(), 0);
	//int foundOctaves[tokens.size()] = {0};
	vector<int> foundOctaves(tokens.size(), 0);
	bool dynAtFirstNote = false;
	unsigned beginningOfChordIndex = 0;
	bool tokenInsideChord = false;
	int prevScoreDynamic = -1;
	int quotesCounter = 0; // so we can ignore anything that is passed as text
	// now check for the rest of the characters of the token, so start with j = accIndex
	for (i = 0; i < tokens.size(); i++) {
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
		for (j = accidentalIndexes.at(i); j < tokens.at(i).size(); j++) {
			if (int(tokens.at(i).at(j)) == 39) {
				if (!sharedData.instruments.at(lastInstrumentIndex).isRhythm()) {
					//notesData.at(verticalNoteIndex)[chordNotesIndexes.at(i)] += 12.0;
					notesData.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) += 12;
					//midiNotesData.at(verticalNoteIndex)[chordNotesIndexes.at(i)] += 12;
					midiNotesData.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) += 12;
					octavesForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i))++;
				}
				foundOctaves.at(i) = 1;
			}
			else if (int(tokens.at(i).at(j)) == 44) {
				if (!sharedData.instruments.at(lastInstrumentIndex).isRhythm()) {
					//notesData.at(verticalNoteIndex)[chordNotesIndexes.at(i)] -= 12.0;
					notesData.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) -= 12;
					//midiNotesData.at(verticalNoteIndex)[chordNotesIndexes.at(i)] -= 12;
					midiNotesData.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) -= 12;
					octavesForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i))--;
				}
				foundOctaves.at(i) = 1;
			}
		}
		// then check for the rest of the characters
		for (j = accidentalIndexes.at(i); j < tokens.at(i).size(); j++) {
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
				if (verticalNoteIndex > 0 && !tokenInsideChord && !dynAtFirstNote) {
					intStrPair p;
					p.first = 3;
					p.second = "first note doesn't have a duration";
					return p;
				}
			}
			
			else if (tokens.at(i).at(j) == char(92)) { // back slash
				index2 = j;
				int dynamic = 0;
				bool foundDynamicLocal = false;
				bool foundGlissandoLocal = false;
				if (index2 > (tokens.at(i).size()-1)) {
					intStrPair p;
					p.first = 3;
					p.second = "a backslash must be followed by a command";
					return p;
				}
				// loop till you find all dynamics or other commands
				while (index2 < tokens.at(i).size()) {
					if (tokens.at(i)[index2+1] == char(102)) { // f for forte
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
					else if (tokens.at(i)[index2+1] == char(112)) { // p for piano
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
					else if (tokens.at(i)[index2+1] == char(109)) { // m for mezzo
						mezzo = true;
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							dynamicsRampEnd.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampEndForScore.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampIndexes.at(dynamicsRampCounter-1).second = verticalNoteIndex;
						}
					}
					else if (tokens.at(i)[index2+1] == char(60)) { // <
						if (!dynamicsRampStarted) {
							dynamicsRampStarted = true;
							dynamicsRampStart.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampStartForScore.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampDirForScore.at(verticalNoteIndex) = 1;
							dynamicsRampIndexes.at(dynamicsRampCounter).first = verticalNoteIndex;
							dynamicsRampCounter++;
						}
					}
					else if (tokens.at(i)[index2+1] == char(62)) { // >
						if (!dynamicsRampStarted) {
							dynamicsRampStarted = true;
							dynamicsRampStart.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampStartForScore.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampDirForScore.at(verticalNoteIndex) = 0;
							dynamicsRampIndexes.at(dynamicsRampCounter).first = verticalNoteIndex;
							dynamicsRampCounter++;
						}
					}
					else if (tokens.at(i)[index2+1] == char(33)) { // exclamation mark
						if (dynamicsRampStarted) {
							dynamicsRampStarted = false;
							dynamicsRampEnd.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampEndForScore.at(verticalNoteIndex) = verticalNoteIndex;
							dynamicsRampIndexes.at(dynamicsRampCounter-1).second = verticalNoteIndex;
						}
						else {
							intStrPair p;
							p.first = 3;
							p.second = "no crescendo or diminuendo initiated";
							return p;
						}
					}
					else if (tokens.at(i)[index2+1] == char(103)) { // g for gliss
						if ((tokens.at(i).substr(index2+1,5).compare("gliss") == 0) ||
							(tokens.at(i).substr(index2+1,9).compare("glissando") == 0)) {
							glissandiIndexes.at(verticalNoteIndex) = 1;
							foundGlissandoLocal = true;
						}
					}
					else {
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
						// if we haven't found a dynamic and the ramp vectors are empty
						// it means that we have received some unknown character
						else if (dynamicsRampCounter == 0 && !foundGlissandoLocal) {
							intStrPair p;
							p.first = 3;
							p.second = tokens.at(i)[index2+1] + (string)": unknown character";
							return p;
						}
						break;
					}
					index2++;
				}
				j = index2;
			}
			
			// . for dotted note, which is also used for staccato, hence the second test against 45 (hyphen)
			else if (tokens.at(i).at(j) == char(46) && tokens.at(i)[j-1] != char(45)) {
				dotIndexes.at(verticalNoteIndex) = 1;
				dotsCounter++;
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
				// need to implement this
			}

			// check for _ or ^ and make sure that the previous character is not -
			else if ((int(tokens.at(i).at(j)) == 94 || int(tokens.at(i).at(j)) == 95) && int(tokens.at(i)[j-1]) != 45) {
				if (textIndexesLocal.at(verticalNoteIndex) == 0) {
					intStrPair p;
					p.first = 3;
					p.second = (string)"stray " + tokens.at(i).at(j);
					return p;
				}
				if (tokens.at(i).size() < j+3) {
					intStrPair p;
					p.first = 3;
					p.second = "incorrect text notation";
					return p;
				}
				if (int(tokens.at(i).at(j+1)) != 34) {
					intStrPair p;
					p.first = 3;
					p.second = "a text symbol must be followed by quote signs";
					return p;
				}
				else {
					quotesCounter++;
				}
			}
		}
		// add the extracted octave with \ottava
		octavesForScore.at(verticalNoteIndex).at(chordNotesIndexes.at(i)) -= ottavas.at(verticalNoteIndex);
		// store durations at the end of each token only if tempDur is greater than 0
		if (tempDur > 0) {
			if (std::find(std::begin(dursArr), std::end(dursArr), tempDur) == std::end(dursArr)) {
				intStrPair p;
				p.first = 3;
				p.second = std::to_string(tempDur) + " is not a valid duration";
				return p;
			}
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

	// check if there are any unmatched quote signs
	if (quotesCounter > 0) {
		intStrPair p;
		p.first = 3;
		p.second = "unmatched quote sign";
		return p;
	}

	// once all elements have been parsed we can determine whether we're fine to go on
	for (i = 0; i < tokens.size(); i++) {
		//if (tokens.at(i).compare("\\ottava") == 0 || tokens.at(i).compare("\\ott") == 0) {
		//	i += 1;
		//	continue;
		//}
		if (!foundNotes[i] && !foundDynamics.at(i) && !foundAccidentals.at(i) && !foundOctaves.at(i)) {
			intStrPair p;
			p.first = 3;
			p.second = tokens.at(i).at(j) + (string)": unknown character";
			return p;
		}
	}

	if (numDurs == 0) {
		intStrPair p;
		p.first = 3;
		p.second = "no durations added";
		return p;
	}

	// fill in possible empty slots of durations
	for (i = 0; i < numNotesVertical; i++) {
		if ((int)i != durIndexes.at(i)) {
			dursData.at(i) = dursData[i-1];
		}
	}

	// fill in possible empty slots of dynamics
	if (numDynamics == 0) {
		for (i = 0; i < numNotesVertical; i++) {
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
		for (i = 0; i < numNotesVertical; i++) {
			// if a dynamic has not been stored, fill in the last
			// stored dynamic in this empty slot
			if ((int)i != dynamicsIndexes.at(i)) {
				// we don't want to add data for the score, only for the sequencer
				if (i > 0) {
					dynamicsData.at(i) =  dynamicsData[i-1];
					midiVels.at(i) = midiVels[i-1];
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
		int step = valsDiff / numSteps;
		int midiVelsStep = midiVelsDiff / numSteps;
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
	int barIndex = getLastBarIndex();
	if (sharedData.numerator.find(barIndex) == sharedData.numerator.end()) {
		sharedData.numerator.at(barIndex) = sharedData.denominator.at(barIndex) = 4;
	}
	int dursAccum = 0;
	int diff = 0; // this is the difference between the expected and the actual sum of tuplets
	int scoreDur; // durations for score should not be affected by tuplet calculations
	// convert durations to number of beats with respect to minimum duration
	for (i = 0; i < numNotesVertical; i++) {
		dursData.at(i) = MINDUR / dursData.at(i);
		if (dotIndexes.at(i) == 1) {
			dursData.at(i) += (dursData.at(i) / 2);
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
	// if all we have is "r1", then it's a tacet, otherwise, we need to check the durations sum
	if (!(tokens.size() == 1 && tokens.at(0).compare("r1") == 0)) {
		if (dursAccum < sharedData.numBeats.at(barIndex)) {
			intStrPair p;
			p.first = 3;
			p.second = "durations sum is less than bar duration";
			return p;
		}
		if (dursAccum > sharedData.numBeats.at(barIndex)) {
			intStrPair p;
			p.first = 3;
			p.second = "durations sum is greater than bar duration";
			return p;
		}
	}
	// now we can create a vector of pairs with the indexes of the slurs
	int numSlurStarts = 0, numSlurStops = 0;
	for (i = 0; i < numNotesVertical; i++) {
		if (slurBeginningsIndexes.at(i) > -1) numSlurStarts++;
		if (slurEndingsIndexes.at(i) > -1) numSlurStops++;
	}
	intPair p;
	p.first = p.second = -1;
	vector<intPair> slurIndexes (max(max(numSlurStarts, numSlurStops), 1), p);
	int slurNdx = 0;
	for (i = 0; i < numNotesVertical; i++) {
		if (slurBeginningsIndexes.at(i) > -1) slurIndexes[slurNdx].first = slurBeginningsIndexes.at(i);
		if (slurEndingsIndexes.at(i) > -1) slurIndexes[slurNdx].second = slurEndingsIndexes.at(i);
	}
	// once the melodic line has been parsed, we can insert the new data to the maps of the Instrument object
	map<string, int>::iterator it = sharedData.instrumentIndexes.find(lastInstrument);
	if (it == sharedData.instrumentIndexes.end()) {
		intStrPair p;
		p.first = 3;
		p.second = "instrument does not exist";
		return p;
	}
	int thisInstIndex = it->second;
	sharedData.instruments.at(thisInstIndex).notes.at(barIndex) = std::move(notesData);
	sharedData.instruments.at(thisInstIndex).midiNotes.at(barIndex) = std::move(midiNotesData);
	sharedData.instruments.at(thisInstIndex).durs.at(barIndex) = std::move(dursData);
	sharedData.instruments.at(thisInstIndex).dursWithoutSlurs.at(barIndex) = std::move(dursDataWithoutSlurs);
	sharedData.instruments.at(thisInstIndex).midiDursWithoutSlurs.at(barIndex) = std::move(midiDursDataWithoutSlurs);
	sharedData.instruments.at(thisInstIndex).dynamics.at(barIndex) = std::move(dynamicsData);
	sharedData.instruments.at(thisInstIndex).midiVels.at(barIndex) = std::move(midiVels);
	sharedData.instruments.at(thisInstIndex).pitchBendVals.at(barIndex) = std::move(pitchBendVals);
	sharedData.instruments.at(thisInstIndex).dynamicsRamps.at(barIndex) = std::move(dynamicsRampData);
	sharedData.instruments.at(thisInstIndex).glissandi.at(barIndex) = std::move(glissandiIndexes);
	sharedData.instruments.at(thisInstIndex).midiGlissDurs.at(barIndex) = std::move(midiGlissDurs);
	sharedData.instruments.at(thisInstIndex).midiDynamicsRampDurs.at(barIndex) = std::move(midiDynamicsRampDurs);
	sharedData.instruments.at(thisInstIndex).articulations.at(barIndex) = std::move(articulationIndexes);
	sharedData.instruments.at(thisInstIndex).midiArticulationVals.at(barIndex) = std::move(midiArticulationVals);
	sharedData.instruments.at(thisInstIndex).isSlurred.at(barIndex) = std::move(isSlurred);
	sharedData.instruments.at(thisInstIndex).text.at(barIndex) = std::move(texts);
	sharedData.instruments.at(thisInstIndex).textIndexes.at(barIndex) = std::move(textIndexesLocal);
	sharedData.instruments.at(thisInstIndex).slurIndexes.at(barIndex) = std::move(slurIndexes);
	sharedData.instruments.at(thisInstIndex).scoreNotes.at(barIndex) = std::move(notesForScore);
	sharedData.instruments.at(thisInstIndex).scoreDurs.at(barIndex) = std::move(dursForScore);
	sharedData.instruments.at(thisInstIndex).scoreDotIndexes.at(barIndex) = std::move(dotIndexes);
	sharedData.instruments.at(thisInstIndex).scoreAccidentals.at(barIndex) = std::move(accidentalsForScore);
	sharedData.instruments.at(thisInstIndex).scoreNaturalSignsNotWritten.at(barIndex) = std::move(naturalSignsNotWrittenForScore);
	sharedData.instruments.at(thisInstIndex).scoreOctaves.at(barIndex) = std::move(octavesForScore);
	sharedData.instruments.at(thisInstIndex).scoreOttavas.at(barIndex) = std::move(ottavas);
	sharedData.instruments.at(thisInstIndex).scoreGlissandi.at(barIndex) = std::move(glissandiIndexes);
	sharedData.instruments.at(thisInstIndex).scoreArticulations.at(barIndex) = std::move(articulationIndexes);
	sharedData.instruments.at(thisInstIndex).scoreDynamics.at(barIndex) = std::move(dynamicsForScore);
	sharedData.instruments.at(thisInstIndex).scoreDynamicsIndexes.at(barIndex) = std::move(dynamicsForScoreIndexes);
	sharedData.instruments.at(thisInstIndex).scoreDynamicsRampStart.at(barIndex) = std::move(dynamicsRampStartForScore);
	sharedData.instruments.at(thisInstIndex).scoreDynamicsRampEnd.at(barIndex) = std::move(dynamicsRampEndForScore);
	sharedData.instruments.at(thisInstIndex).scoreDynamicsRampDir.at(barIndex) = std::move(dynamicsRampDirForScore);
	sharedData.instruments.at(thisInstIndex).scoreTupRatios.at(barIndex) = std::move(tupRatios);
	sharedData.instruments.at(thisInstIndex).scoreTupStartStop.at(barIndex) = std::move(tupStartStop);
	sharedData.instruments.at(thisInstIndex).scoreTexts.at(barIndex) = std::move(textsForScore);
	sharedData.instruments.at(thisInstIndex).isWholeBarSlurred.at(barIndex) = false;
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
	sharedData.instruments.at(thisInstIndex).passed = true;
	intStrPair p1;
	p1.first = 0;
	p1.second = "";
	return p1;
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

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
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
