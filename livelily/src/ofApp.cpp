#include "ofApp.h"
#include "system_command.h"
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
	mainThreadId = std::this_thread::get_id();
	// the engine reaches the user interface through this app (see frontend.h), and its audio stream calls this app (see audio.h)
	session.frontend = this;
	session.audioCallback = this;
	session.dataDirectory = ofToDataPath("", true);
	brightnessCoeff = 0.85;
	backgroundColor = ofColor::black;
	foregroundColor = ofColor::white;
	beatColor = ofColor::paleGreen;
	scoreBackgroundColor = ofColor::white;
	shellStrColor = ofColor::white;
	ofBackground(backgroundColor*brightnessCoeff);

	// give a random seed to srand(), and a fixed one in test mode, so that \rand gives the same values every time (see testmode.cpp)
	srand(testFile.empty() ? time(NULL) : 1);

	unsigned int framerate = 60;

	session.sequencer.setup(&session.sharedData);
	session.sequencer.setMidiOutput(session.midi.get());
	session.sequencer.setFrontend(this);

	// define audio properties
	session.sampleRate = 48000;
	session.bufferSize = 256;
	session.inChannels = 2;
	session.outChannels = 2;
	//audioDevID = 1;
	session.inSoundDeviceSet = false;
	session.outSoundDeviceSet = false;
	session.pyoSet = false;

	session.startSequencer = false;

	notesID = 0;
	session.parsingBar = false;
	session.parsingBars = false;
	session.parsingLoop = false;
	session.storingFunction = false;
	session.storingList = false;
	session.barsIterCounter = 0;
	session.numBarsToParse = 0;
	session.numBarsToParseSet = false;
	session.barsCmdNumBars = 0;
	session.firstInstForBarsIndex = 0;
	session.firstInstForBarsSet = false;
	session.checkIfAllPartsReceivedBarData = false;
	session.lastFunctionIndex = 0;
	session.storingFunctionIndex = 0;
	session.functionBodyDepth = 0;
	fullScreen = false;
	isWindowResized = false;
	windowResizeTimeStamp = 0;

	ofSetFrameRate(framerate);
	lineWidth = 2;
	ofSetLineWidth(lineWidth);

	ofSetWindowTitle("LiveLily");

	//oscReceiver->setup(OSCPORT);
	session.oscReceiverIsSet = false;
	// waiting time to receive a response from a score part server is one frame at 60 FPS
	session.scorePartResponseTime = (uint64_t)((1000.0 / 60.0) + 0.5);

	view.screenWidth = ofGetWindowWidth();
	view.screenHeight = ofGetWindowHeight();
	view.middleOfScreenX = view.screenWidth / 2;
	view.middleOfScreenY = view.screenHeight / 2;
	view.staffXOffset = view.middleOfScreenX;
	view.staffWidth = view.screenWidth;
	numBarsToDisplay = 2;

	session.sharedData.numInstruments = 0;

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
	session.whichPane = 0;
	Editor editor;
	editors[0] = editor;
	editors[0].setActivity(true);
	editors[0].setID(0);
	editors[0].setPaneRow(0);
	editors[0].setPaneCol(0);
	numPanes[0] = 1;
	paneSplitOrientation = 0;
	fontLoaded = false;
	// the function below calls setFontSize() which in turn sets various values
	// including the tracebackBase variable that is used to determine the coordinates and offsets of the panes
	setFontSize(true);
	screenSplitHorizontal = view.screenWidth;

	session.sharedData.tempoMs[0] = 500;
	session.sharedData.BPMMultiplier[0] = 1;
	session.sharedData.beatAtDifferentThanDivisor[0] = false;
	session.sharedData.beatAtValCounter = 0;
	session.sharedData.numerator[0] = 4;
	session.sharedData.denominator[0] = 4;
	session.sharedData.numBeats[0] = 4;

	session.barError = false;

	session.sharedData.loopIndex = 0;
	session.sharedData.tempLoopIndex = 0;
	session.sharedData.thisBarIndex = 0;
	session.sharedData.beatCounter = 0; // this is controlled by the sequencer
	session.sharedData.barCounter = 0; // also controlled by the sequencer
	view.numBars = view.prevNumBars = 1;
	view.thisPosition = view.prevPosition = 0;
	showBarCount = false;
	showTempo = false;

	//sharedData.showNotes = false;
	//showPianoRoll = false;
	//showScope = false;
	showScore = false;
	scoreType = 0;
	session.seqResolution = 16;
	view.longestInstNameWidth = 0;
	view.staffLinesDist = 10.0;
	view.scoreFontSize = 35;
	instFontSize = view.scoreFontSize/3.5;
	instFont.load("Monaco.ttf", instFontSize);
	view.notesXStartPnt = 0;
	view.blankSpace = 10;
	session.sharedData.animate = false;
	session.sharedData.setAnimation = false;
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
	scoreBackgroundHeight = view.tracebackBase; // - cursorHeight;
	scoreMoveXCommand = false;
	scoreMoveYCommand = false;
	mustUpdateScore = false;
	scoreUpdated = false;
	scoreChangeOnLastBar = false;
	// piano roll variables, with the keys of a piano, from A0 to C8 (see setPianoRollRange())
	view.pianoRollKeysWidth = scoreBackgroundWidth / KEYSWIDTHCOEFF;
	setPianoRollRange(21, 108);
	view.pianoRollMinDur = (scoreBackgroundWidth - view.pianoRollKeysWidth) / (float)MINDUR;

	session.maestroToggleSet = false;
	session.receivingMaestro = false;
	session.maestroValNdx = 0;
	session.maestroInitialized = false;
	session.maestroBeatsIn = 1;
	session.maestroValThresh = 7.5;
	session.maestroTimeStamp = ofGetElapsedTimeMillis();
	session.maestroAddress = "/maestro";
	session.maestroToggleAddress = "/toggle";
	session.maestroLevareAddress = "/levare";

	view.pyStdoutStr = "";
	view.pyStdoutStrNdx = 0;
	view.typePyStrCharByChar = true;
	view.typePyStrLineByLine = false;
	view.typePyStr = false;
	view.whichPyPane = 0;
	view.pyStdoutKeyModNdx = 0;
	view.pyStdoutStrVecNdx = 0;

	session.correctOnSameOctaveOnly = true;

	session.serialPortOpen = false;

	session.sharedData.PPQN = 24;
	session.sharedData.PPQNCounter = 0;
	// get the ms duration of the PPQM
	session.sharedData.PPQNPerUs[0] = (uint64_t)(session.sharedData.tempoMs[0] / (float)session.sharedData.PPQN) * 1000;

	exiting = false;

	mouseX = 0;
	mouseY = 0;

	// set the notes chars
	for (int i = 2; i < 9; i++) {
		session.noteChars[i-2] = char((i%7)+97);
	}
	session.noteChars[7] = char(114); // "r" for rest

	// store the articulation names std::strings
	std::string articulSymsLocal[8] = {"none", "marcato", "trill", "tenuto", \
								  "staccatissimo", "accent", "staccato", "portando"};
	for (int i = 0; i < 8; i++) {
		session.sharedData.articulSyms[i] = articulSymsLocal[i];
	}

	// create the beat rectangle transparency cosine table
	for (int i = 0; i <= BEATVIZBRIGHTNESS; i++) {
		view.beatVizCosTab[i] = (int)(((cos(((float)i/(float)(BEATVIZBRIGHTNESS+1))*M_PI) * -0.5) + 0.5) * 255);
	}

	/* Colors used for syntax highlighting for LiveLily
	   first level commands -- fuchsia
	   methods --------------- violet
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
	   variables ------------- darkTurquoise
	   python decorators ----- coral
	   -- paleGreen is used for the pulse of the beat
	*/
	// first level commands
	// the commands of LiveLily, from the table of commands (see commandTable() in parser.cpp)
	// so that a command added there is colored, autocompleted, and reserved as a name (see reservedNames below)
	for (const auto& command : session.commandTable()) session.commandsMap[Session::livelily][command.first[1]][command.first] = Colors::fuchsia;
	session.commandsMap[Session::livelily]['j'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['q'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['x'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['z'][""] = toColor(foregroundColor);

	// methods
	session.commandsMap[Session::livelily]['a']["accoffset"] = Colors::violet;
	session.commandsMap[Session::livelily]['a']["add"] = Colors::violet;
	session.commandsMap[Session::livelily]['a']["alignment"] = Colors::violet;
	session.commandsMap[Session::livelily]['a']["autobrackets"] = Colors::violet;
	session.commandsMap[Session::livelily]['a']["autocomplete"] = Colors::violet;
	session.commandsMap[Session::livelily]['a']["animation"] = Colors::violet;
	session.commandsMap[Session::livelily]['b']["beatsin"] = Colors::violet;
	session.commandsMap[Session::livelily]['b']["beattype"] = Colors::violet;
	session.commandsMap[Session::livelily]['b']["bind"] = Colors::violet;
	session.commandsMap[Session::livelily]['c']["check"] = Colors::violet;
	session.commandsMap[Session::livelily]['c']["clear"] = Colors::violet;
	session.commandsMap[Session::livelily]['c']["colors"] = Colors::violet;
	session.commandsMap[Session::livelily]['c']["correct"] = Colors::violet;
	session.commandsMap[Session::livelily]['c']["cursor"] = Colors::violet;
	session.commandsMap[Session::livelily]['d']["delay"] = Colors::violet;
	session.commandsMap[Session::livelily]['f']["finish"] = Colors::violet;
	session.commandsMap[Session::livelily]['f']["fullscreen"] = Colors::violet;
	session.commandsMap[Session::livelily]['g']["goto"] = Colors::violet;
	session.commandsMap[Session::livelily]['h']["hide"] = Colors::violet;
	session.commandsMap[Session::livelily]['i']["init"] = Colors::violet;
	session.commandsMap[Session::livelily]['l']["levareaddr"] = Colors::violet;
	session.commandsMap[Session::livelily]['l']["locate"] = Colors::violet;
	session.commandsMap[Session::livelily]['m']["mainaddr"] = Colors::violet;
	session.commandsMap[Session::livelily]['m']["map"] = Colors::violet;
	session.commandsMap[Session::livelily]['m']["midichan"] = Colors::violet;
	session.commandsMap[Session::livelily]['m']["midiport"] = Colors::violet;
	session.commandsMap[Session::livelily]['m']["movex"] = Colors::violet;
	session.commandsMap[Session::livelily]['m']["movey"] = Colors::violet;
	session.commandsMap[Session::livelily]['n']["normal"] = Colors::violet;
	session.commandsMap[Session::livelily]['n']["numbars"] = Colors::violet;
	session.commandsMap[Session::livelily]['o']["onrelease"] = Colors::violet;
	session.commandsMap[Session::livelily]['p']["play"] = Colors::violet;
	session.commandsMap[Session::livelily]['r']["recenter"] = Colors::violet;
	session.commandsMap[Session::livelily]['r']["release"] = Colors::violet;
	session.commandsMap[Session::livelily]['r']["remove"] = Colors::violet;
	session.commandsMap[Session::livelily]['s']["send"] = Colors::violet;
	session.commandsMap[Session::livelily]['s']["sendmidi"] = Colors::violet;
	session.commandsMap[Session::livelily]['s']["setargs"] = Colors::violet;
	session.commandsMap[Session::livelily]['s']["setup"] = Colors::violet;
	session.commandsMap[Session::livelily]['s']["sendto"] = Colors::violet;
	session.commandsMap[Session::livelily]['s']["seqres"] = Colors::violet;
	session.commandsMap[Session::livelily]['s']["show"] = Colors::violet;
	session.commandsMap[Session::livelily]['s']["showbeat"] = Colors::violet;
	session.commandsMap[Session::livelily]['s']["sndfont"] = Colors::violet;
	session.commandsMap[Session::livelily]['s']["stop"] = Colors::violet;
	session.commandsMap[Session::livelily]['s']["stopnow"] = Colors::violet;
	session.commandsMap[Session::livelily]['t']["toggleaddr"] = Colors::violet;
	session.commandsMap[Session::livelily]['t']["transpose"] = Colors::violet;
	session.commandsMap[Session::livelily]['t']["traverse"] = Colors::violet;
	session.commandsMap[Session::livelily]['t']["tune"] = Colors::violet;
	session.commandsMap[Session::livelily]['u']["unbind"] = Colors::violet;
	session.commandsMap[Session::livelily]['u']["update"] = Colors::violet;
	session.commandsMap[Session::livelily]['v']["val"] = Colors::violet;
	session.commandsMap[Session::livelily]['v']["valndx"] = Colors::violet;
	session.commandsMap[Session::livelily]['v']["valthresh"] = Colors::violet;
	session.commandsMap[Session::livelily]['v']["volume"] = Colors::violet;

	// arguments
	session.commandsMap[Session::livelily]['a']["all"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['a']["alto"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['b']["barcount"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['b']["barstart"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['b']["bass"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['b']["beat"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['c']["char"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['f']["fr"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['f']["framerate"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['h']["hor"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['i']["immediately"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['i']["invert"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["line"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["localhost"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["loopstart"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['n']["notes"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['o']["off"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['o']["on"] = Colors::skyBlue;
	// the widths of the units of \editor.alignment
	session.commandsMap[Session::livelily]['s']["short"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["long"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['o']["onlast"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['o']["onoctave"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['p']["perc"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['p']["percussion"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['p']["pianoroll"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['s']["sequencer"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['s']["seq"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['p']["python"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['r']["reset"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['r']["rhythm"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['s']["scope"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['t']["tempo"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['t']["treble"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['v']["vert"] = Colors::skyBlue;

	// color names, same color as arguments above
	session.commandsMap[Session::livelily]['a']["aliceBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['a']["antiqueWhite"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['a']["aqua"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['a']["aquamarine"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['a']["azure"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['b']["black"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['b']["blue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['b']["beige"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['b']["bisque"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['b']["blanchedAlmond"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['b']["blueViolet"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['b']["brown"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['b']["burlyWood"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['c']["cyan"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['c']["cadetBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['c']["chartreuse"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['c']["chocolate"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['c']["coral"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['c']["cornflowerBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['c']["cornsilk"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['c']["crimson"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkCyan"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkGoldenRod"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkGray"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkGrey"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkGreen"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkKhaki"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkMagenta"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkOliveGreen"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkOrange"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkOrchid"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkRed"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkSalmon"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkSeaGreen"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkSlateBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkSlateGray"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkSlateGrey"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkTurquoise"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["darkViolet"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["deepPink"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["deepSkyBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["dimGray"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["dimGrey"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['d']["dodgerBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['f']["fireBrick"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['f']["floralWhite"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['f']["forestGreen"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['f']["fuchsia"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['g']["green"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['g']["gray"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['g']["gainsboro"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['g']["ghostWhite"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['g']["gold"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['g']["goldenRod"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['g']["grey"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['g']["greenYellow"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['h']["honeyDew"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['h']["hotPink"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['i']["indianRed "] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['i']["indigo "] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['i']["ivory"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['k']["khaki"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lavender"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lavenderBlush"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lawnGreen"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lemonChiffon"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lightBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lightCoral"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lightCyan"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lightGoldenRodYellow"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lightGray"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lightGrey"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lightGreen"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lightPink"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lightSalmon"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lightSeaGreen"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lightSkyBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lightSlateGray"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lightSlateGrey"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lightSteelBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lightYellow"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["lime"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["limeGreen"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['l']["linen"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['m']["magenta"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['m']["maroon"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['m']["mediumAquaMarine"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['m']["mediumBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['m']["mediumOrchid"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['m']["mediumPurple"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['m']["mediumSeaGreen"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['m']["mediumSlateBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['m']["mediumSpringGreen"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['m']["mediumTurquoise"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['m']["mediumVioletRed"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['m']["midnightBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['m']["mintCream"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['m']["mistyRose"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['m']["moccasin"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['n']["navajoWhite"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['n']["navy"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['o']["oldLace"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['o']["olive"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['o']["oliveDrab"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['o']["orange"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['o']["orangeRed"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['o']["orchid"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['p']["paleGoldenRod"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['p']["paleGreen"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['p']["paleTurquoise"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['p']["paleVioletRed"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['p']["papayaWhip"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['p']["peachPuff"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['p']["peru"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['p']["pink"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['p']["plum"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['p']["powderBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['p']["purple"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['r']["red"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['r']["rosyBrown"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['r']["royalBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['s']["saddleBrown"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['s']["salmon"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['s']["sandyBrown"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['s']["seaGreen"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['s']["seaShell"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['s']["sienna"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['s']["silver"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['s']["skyBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['s']["slateBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['s']["slateGray"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['s']["slateGrey"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['s']["snow"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['s']["springGreen"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['s']["steelBlue"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['t']["tan"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['t']["teal"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['t']["thistle"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['t']["tomato"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['t']["turquoise"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['v']["violet"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['w']["white"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['w']["wheat"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['w']["whiteSmoke"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['y']["yellow"] = Colors::skyBlue;
	session.commandsMap[Session::livelily]['y']["yellowGreen"] = Colors::skyBlue;

	// dummy upper case letters (the LiveLily language doesn't have keywords with upper case letters)
	session.commandsMap[Session::livelily]['A'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['B'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['C'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['D'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['E'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['F'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['G'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['H'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['I'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['J'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['K'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['L'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['M'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['N'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['O'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['P'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['Q'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['R'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['S'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['T'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['U'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['V'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['W'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['X'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['Y'][""] = toColor(foregroundColor);
	session.commandsMap[Session::livelily]['Z'][""] = toColor(foregroundColor);
	// the commands of LiveLily are reserved names (see isReservedName()), which are stored before any name is defined
	for (const auto& byChar : session.commandsMap[Session::livelily]) {
		for (const auto& cmd : byChar.second) {
			if (cmd.first.size() > 1 && cmd.first[0] == '\\') session.reservedNames.insert(cmd.first);
		}
	}

	/* Colors used for syntax highlighting for Python
	   commands -------------- fuchsia
	   keywords -------------- violet
	   Pyo classes ----------- deepSkyBlue
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
	// keywords
	session.commandsMap[Session::python]['a']["as"] = Colors::violet;
	session.commandsMap[Session::python]['a']["assert"] = Colors::violet;
	session.commandsMap[Session::python]['a']["and"] = Colors::violet;
	session.commandsMap[Session::python]['b']["break"] = Colors::violet;
	session.commandsMap[Session::python]['c']["class"] = Colors::violet;
	session.commandsMap[Session::python]['c']["continue"] = Colors::violet;
	session.commandsMap[Session::python]['d']["def"] = Colors::violet;
	session.commandsMap[Session::python]['d']["del"] = Colors::violet;
	session.commandsMap[Session::python]['e']["elif"] = Colors::violet;
	session.commandsMap[Session::python]['e']["else"] = Colors::violet;
	session.commandsMap[Session::python]['e']["except"] = Colors::violet;
	session.commandsMap[Session::python]['f']["for"] = Colors::violet;
	session.commandsMap[Session::python]['f']["from"] = Colors::violet;
	session.commandsMap[Session::python]['f']["finally"] = Colors::violet;
	session.commandsMap[Session::python]['g']["global"] = Colors::violet;
	session.commandsMap[Session::python]['h'][""] = toColor(foregroundColor);
	session.commandsMap[Session::python]['i']["if"] = Colors::violet;
	session.commandsMap[Session::python]['i']["is"] = Colors::violet;
	session.commandsMap[Session::python]['i']["import"] = Colors::violet;
	session.commandsMap[Session::python]['i']["in"] = Colors::violet;
	session.commandsMap[Session::python]['j'][""] = toColor(foregroundColor);
	session.commandsMap[Session::python]['k'][""] = toColor(foregroundColor);
	session.commandsMap[Session::python]['l']["lambda"] = Colors::violet;
	session.commandsMap[Session::python]['m'][""] = toColor(foregroundColor);
	session.commandsMap[Session::python]['n']["not"] = Colors::violet;
	session.commandsMap[Session::python]['n']["nonlocal"] = Colors::violet;
	session.commandsMap[Session::python]['n']["not"] = Colors::violet;
	session.commandsMap[Session::python]['o']["or"] = Colors::violet;
	session.commandsMap[Session::python]['p']["pass"] = Colors::violet;
	session.commandsMap[Session::python]['q'][""] = toColor(foregroundColor);
	session.commandsMap[Session::python]['r']["raise"] = Colors::violet;
	session.commandsMap[Session::python]['r']["return"] = Colors::violet;
	session.commandsMap[Session::python]['s'][""] = toColor(foregroundColor);
	session.commandsMap[Session::python]['t']["try"] = Colors::violet;
	session.commandsMap[Session::python]['u'][""] = toColor(foregroundColor);
	session.commandsMap[Session::python]['v'][""] = toColor(foregroundColor);
	session.commandsMap[Session::python]['w']["while"] = Colors::violet;
	session.commandsMap[Session::python]['w']["with"] = Colors::violet;
	session.commandsMap[Session::python]['x'][""] = toColor(foregroundColor);
	session.commandsMap[Session::python]['y']["yield"] = Colors::violet;
	session.commandsMap[Session::python]['z'][""] = toColor(foregroundColor);
	session.commandsMap[Session::python]['F']["False"] = Colors::violet;
	session.commandsMap[Session::python]['J'][""] = toColor(foregroundColor);
	session.commandsMap[Session::python]['K'][""] = toColor(foregroundColor);
	session.commandsMap[Session::python]['N']["None"] = Colors::violet;
	session.commandsMap[Session::python]['Q'][""] = toColor(foregroundColor);
	session.commandsMap[Session::python]['T']["True"] = Colors::violet;

	// Pyo classes
	session.commandsMap[Session::python]['A']["Abs"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['A']["Atan2"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['A']["Adsr"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['A']["AllpassWV"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['A']["Allpass"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['A']["Allpass2"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['A']["Atone"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['A']["AToDB"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['A']["AtanTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['A']["AttackDetector"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['A']["Average"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['B']["Balance"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['B']["Biquad"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['B']["Biquadx"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['B']["Biquada"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['B']["BandSplit"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['B']["ButLP"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['B']["ButHP"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['B']["ButBP"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['B']["ButBR"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['B']["Blit"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['B']["BrownNoise"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['B']["Between"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['B']["Beat"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['B']["Bendin"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['B']["Binaural"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["Centroid"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["Cos"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["Ceil"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["Clip"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["Convolve"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["Chorus"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["ComplexRes"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["CarToPol"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["CvlVerb"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["ChenLee"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["CrossFM"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["CallAfter"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["CtlScan"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["CtlScan2"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["Change"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["Cloud"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["Count"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["Counter"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["CentsToTranspo"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["Clean_objects"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["Compare"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["ConotrolRead"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["ConotrolRec"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["ChebyTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["CosLogTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["CosTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["CurvyTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['C']["Compress"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['D']["Div"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['D']["Degrade"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['D']["Disto"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['D']["Delay"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['D']["Delay1"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['D']["DCBlock"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['D']["DBToA"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['D']["Denorm"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['D']["DataTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['D']["Dummy"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["Expseg"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["Exp"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["EventScale"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["EventGenerator"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["EventDummy"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["EventFilter"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["EventKey"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["EventSeq"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["EventSlide"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["EventIndex"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["EventMarkov"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["EventChoice"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["EventDrunk"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["EventNoise"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["EventCall"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["EventConditional"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["EventInstrument"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["Events"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["EQ"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["Euclide"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["ExpTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['E']["Expand"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['F']["Fader"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['F']["Follower"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['F']["Follower2"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['F']["Floor"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['F']["FreqShift"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['F']["FFT"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['F']["FrameAccum"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['F']["FrameDelta"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['F']["FastSine"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['F']["FM"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['F']["FToM"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['F']["FourBand"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['F']["Freeverb"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['G']["Gate"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['G']["Granulator"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['G']["Granule"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['H']["Hilbert"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['H']["HannTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['H']["HarmTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['H']["HRTF"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['H']["Harmonizer"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['I']["IRWinSync"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['I']["IRAverage"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['I']["IRPulse"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['I']["IRFM"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['I']["IFFTMatrix"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['I']["Input"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['I']["InputFader"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['I']["Iter"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['I']["IFFT"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['I']["Interp"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['L']["Log"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['L']["Log2"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['L']["Log10"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['L']["Linseg"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['L']["LFO"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['L']["Lorenz"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['L']["LogiMap"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['L']["Lookup"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['L']["Looper"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['L']["LinTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['L']["LogTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["MidiListener"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["MidiDispatcher"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["Mirror"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["Min"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["Max"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["MultiBand"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["Mix"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["MatrixMorph"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["MatrixPointer"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["MatrixRec"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["MatrixRecLoop"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["MidiAdsr"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["MidiDelAdsr"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["MidiCtl"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["Metro"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["MToF"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["MToT"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["Map"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["MoogLP"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["Mixer"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["MidiLinseg"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['M']["MML"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['N']["Noise"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['N']["Notein"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['N']["NextTrig"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['N']["NoteinRead"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['N']["NoteinRec"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['N']["NewTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['N']["NewMatrix"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['O']["OscListener"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['O']["OscDataReceive"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['O']["OscDataSend"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['O']["OscListReceive"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['O']["OscReceive"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['O']["OscSend"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['O']["Osc"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['O']["OscBank"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['O']["OscLoop"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['O']["OscTrig"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PyoBaseObject"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PyoObject"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PyoTableObject"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PyoMatrixObject"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PyoPVObject"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PeakAmp"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PolToCar"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVAnal"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVSynth"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVAddSynth"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVTranspose"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVVerb"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVGate"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVCross"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVMult"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVMorph"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVFilter"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVDelay"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVBuffer"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVShift"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVAmpMod"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVFreqMod"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVBufLoops"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVBufTabLoops"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PVMix"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["Phasor"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PinkNoise"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["Particle"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["Particle2"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["Pointer"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["Pointer2"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["Pulsar"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["Percent"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PyGuiControlSlider"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PyGuiVuMeter"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PyGuiGrapher"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PyGuiMultiSlider"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PyGuiSpectrum"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PyGuiScope"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PyGuiSndView"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PyGuiKeyboard"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["ParaTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PartialTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["Port"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["Phaser"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["PadSynthTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["Print"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["Pan"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["Pow"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["Programin"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['P']["Pattern"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['R']["RawMidi"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['R']["RandDur"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['R']["RandInt"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['R']["Randh"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['R']["Randi"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['R']["Record"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['R']["Resample"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['R']["RMS"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['R']["Round"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['R']["Reson"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['R']["Resonx"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['R']["RCOsc"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['R']["Rossler"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["Server"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["Spectrum"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["Scope"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["Sin"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["Sqrt"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["Sub"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["Sig"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SigTo"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SDelay"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SVF"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SVF2"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["STRev"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SmoothDelay"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["Sine"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SineLoop"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SumOsc"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SuperSaw"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["Stream"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SPan"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["Selector"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["Switch"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["Score"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SfPlayer"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SfMarkerLooper"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SfMarkerShuffler"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SampHold"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["Scale"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["Snap"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SawTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SyncTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SndTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SquareTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SharedTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SLMap"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SLMapFreq"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SLMapMul"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SLMapPhase"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SLMapQ"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SLMapDur"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["SLMapPan"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["Select"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['S']["Seq"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["Tan"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["Tanh"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["Tone"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["Touchin"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TableIndex"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TableMorph"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TablePut"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TableRead"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TableRec"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TableWrite"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TableScale"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TableFill"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TableScan"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["Thresh"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["Timer"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["Trig"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TrigBurst"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TrigChoice"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TrigEnv"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TrigExpseg"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TrigFunc"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TrigLinseg"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TrigRand"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TrigRandInt"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TrigTableRec"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TrigVal"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TrigXnoise"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TrigXnoiseMidi"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TranspoToCents"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TrackHold"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['T']["TriangleTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['U']["Urn"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['V']["Vocoder"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['V']["Vectral"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['V']["VarPort"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['V']["VoiceManager"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['W']["Wrap"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['W']["Waveguide"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['W']["WGVerb"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['W']["WinTable"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['X']["XNoise"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['X']["XNoiseDur"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['X']["XNoiseMidi"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['Y']["Yin"] = Colors::deepSkyBlue;
	session.commandsMap[Session::python]['Z']["ZCross"] = Colors::deepSkyBlue;

	// push commands that won't let the editor tokenize based on digits
	session.commandsToNotTokenize.push_back("\\insts");
	session.commandsToNotTokenize.push_back("\\bar");
	session.commandsToNotTokenize.push_back("\\bars");
	session.commandsToNotTokenize.push_back("\\loop");
	session.commandsToNotTokenize.push_back("\\function");
	session.commandsToNotTokenize.push_back("\\std::list");
	session.commandsToNotTokenize.push_back("\\group");

	// commands that the editor needs to check in case of multi-line executing
	// but starting on the first line that is not indented
	noIndentCheck[Session::livelily].push_back("\\bar");
	noIndentCheck[Session::livelily].push_back("\\bars");
	noIndentCheck[Session::livelily].push_back("\\loop");
	noIndentCheck[Session::livelily].push_back("\\function");
	noIndentCheck[Session::livelily].push_back("\\std::list");
	noIndentCheck[Session::python].push_back("class");
	noIndentCheck[Session::python].push_back("def");
	noIndentCheck[Session::python].push_back("for");
	noIndentCheck[Session::python].push_back("while");
	noIndentCheck[Session::python].push_back("if");

	// a map with the color names, copied from the colorsExtendedExample
	session.colorNameMap["white"] = Colors::white;
	session.colorNameMap["gray"] = Colors::gray;
	session.colorNameMap["black"] = Colors::black;
	session.colorNameMap["red"] = Colors::red;
	session.colorNameMap["green"] = Colors::green;
	session.colorNameMap["blue"] = Colors::blue;
	session.colorNameMap["cyan"] = Colors::cyan;
	session.colorNameMap["magenta"] = Colors::magenta;
	session.colorNameMap["yellow"] = Colors::yellow;
	session.colorNameMap["aliceBlue"] = Colors::aliceBlue;
	session.colorNameMap["antiqueWhite"] = Colors::antiqueWhite;
	session.colorNameMap["aqua"] = Colors::aqua;
	session.colorNameMap["aquamarine"] = Colors::aquamarine;
	session.colorNameMap["azure"] = Colors::azure;
	session.colorNameMap["beige"] = Colors::beige;
	session.colorNameMap["bisque"] = Colors::bisque;
	session.colorNameMap["blanchedAlmond"] = Colors::blanchedAlmond;
	session.colorNameMap["blueViolet"] = Colors::blueViolet;
	session.colorNameMap["brown"] = Colors::brown;
	session.colorNameMap["burlyWood"] = Colors::burlyWood;
	session.colorNameMap["cadetBlue"] = Colors::cadetBlue;
	session.colorNameMap["chartreuse"] = Colors::chartreuse;
	session.colorNameMap["chocolate"] = Colors::chocolate;
	session.colorNameMap["coral"] = Colors::coral;
	session.colorNameMap["cornflowerBlue"] = Colors::cornflowerBlue;
	session.colorNameMap["cornsilk"] = Colors::cornsilk;
	session.colorNameMap["crimson"] = Colors::crimson;
	session.colorNameMap["darkBlue"] = Colors::darkBlue;
	session.colorNameMap["darkCyan"] = Colors::darkCyan;
	session.colorNameMap["darkGoldenRod"] = Colors::darkGoldenRod;
	session.colorNameMap["darkGray"] = Colors::darkGray;
	session.colorNameMap["darkGrey"] = Colors::darkGrey;
	session.colorNameMap["darkGreen"] = Colors::darkGreen;
	session.colorNameMap["darkKhaki"] = Colors::darkKhaki;
	session.colorNameMap["darkMagenta"] = Colors::darkMagenta;
	session.colorNameMap["darkOliveGreen"] = Colors::darkOliveGreen;
	session.colorNameMap["darkOrange"] = Colors::darkOrange;
	session.colorNameMap["darkOrchid"] = Colors::darkOrchid;
	session.colorNameMap["darkRed"] = Colors::darkRed;
	session.colorNameMap["darkSalmon"] = Colors::darkSalmon;
	session.colorNameMap["darkSeaGreen"] = Colors::darkSeaGreen;
	session.colorNameMap["darkSlateBlue"] = Colors::darkSlateBlue;
	session.colorNameMap["darkSlateGray"] = Colors::darkSlateGray;
	session.colorNameMap["darkSlateGrey"] = Colors::darkSlateGrey;
	session.colorNameMap["darkTurquoise"] = Colors::darkTurquoise;
	session.colorNameMap["darkViolet"] = Colors::darkViolet;
	session.colorNameMap["deepPink"] = Colors::deepPink;
	session.colorNameMap["deepSkyBlue"] = Colors::deepSkyBlue;
	session.colorNameMap["dimGray"] = Colors::dimGray;
	session.colorNameMap["dimGrey"] = Colors::dimGrey;
	session.colorNameMap["dodgerBlue"] = Colors::dodgerBlue;
	session.colorNameMap["fireBrick"] = Colors::fireBrick;
	session.colorNameMap["floralWhite"] = Colors::floralWhite;
	session.colorNameMap["forestGreen"] = Colors::forestGreen;
	session.colorNameMap["fuchsia"] = Colors::fuchsia;
	session.colorNameMap["gainsboro"] = Colors::gainsboro;
	session.colorNameMap["ghostWhite"] = Colors::ghostWhite;
	session.colorNameMap["gold"] = Colors::gold;
	session.colorNameMap["goldenRod"] = Colors::goldenRod;
	session.colorNameMap["grey"] = Colors::grey;
	session.colorNameMap["greenYellow"] = Colors::greenYellow;
	session.colorNameMap["honeyDew"] = Colors::honeyDew;
	session.colorNameMap["hotPink"] = Colors::hotPink;
	session.colorNameMap["indianRed "] = Colors::indianRed;
	session.colorNameMap["indigo "] = Colors::indigo;
	session.colorNameMap["ivory"] = Colors::ivory;
	session.colorNameMap["khaki"] = Colors::khaki;
	session.colorNameMap["lavender"] = Colors::lavender;
	session.colorNameMap["lavenderBlush"] = Colors::lavenderBlush;
	session.colorNameMap["lawnGreen"] = Colors::lawnGreen;
	session.colorNameMap["lemonChiffon"] = Colors::lemonChiffon;
	session.colorNameMap["lightBlue"] = Colors::lightBlue;
	session.colorNameMap["lightCoral"] = Colors::lightCoral;
	session.colorNameMap["lightCyan"] = Colors::lightCyan;
	session.colorNameMap["lightGoldenRodYellow"] = Colors::lightGoldenRodYellow;
	session.colorNameMap["lightGray"] = Colors::lightGray;
	session.colorNameMap["lightGrey"] = Colors::lightGrey;
	session.colorNameMap["lightGreen"] = Colors::lightGreen;
	session.colorNameMap["lightPink"] = Colors::lightPink;
	session.colorNameMap["lightSalmon"] = Colors::lightSalmon;
	session.colorNameMap["lightSeaGreen"] = Colors::lightSeaGreen;
	session.colorNameMap["lightSkyBlue"] = Colors::lightSkyBlue;
	session.colorNameMap["lightSlateGray"] = Colors::lightSlateGray;
	session.colorNameMap["lightSlateGrey"] = Colors::lightSlateGrey;
	session.colorNameMap["lightSteelBlue"] = Colors::lightSteelBlue;
	session.colorNameMap["lightYellow"] = Colors::lightYellow;
	session.colorNameMap["lime"] = Colors::lime;
	session.colorNameMap["limeGreen"] = Colors::limeGreen;
	session.colorNameMap["linen"] = Colors::linen;
	session.colorNameMap["maroon"] = Colors::maroon;
	session.colorNameMap["mediumAquaMarine"] = Colors::mediumAquaMarine;
	session.colorNameMap["mediumBlue"] = Colors::mediumBlue;
	session.colorNameMap["mediumOrchid"] = Colors::mediumOrchid;
	session.colorNameMap["mediumPurple"] = Colors::mediumPurple;
	session.colorNameMap["mediumSeaGreen"] = Colors::mediumSeaGreen;
	session.colorNameMap["mediumSlateBlue"] = Colors::mediumSlateBlue;
	session.colorNameMap["mediumSpringGreen"] = Colors::mediumSpringGreen;
	session.colorNameMap["mediumTurquoise"] = Colors::mediumTurquoise;
	session.colorNameMap["mediumVioletRed"] = Colors::mediumVioletRed;
	session.colorNameMap["midnightBlue"] = Colors::midnightBlue;
	session.colorNameMap["mintCream"] = Colors::mintCream;
	session.colorNameMap["mistyRose"] = Colors::mistyRose;
	session.colorNameMap["moccasin"] = Colors::moccasin;
	session.colorNameMap["navajoWhite"] = Colors::navajoWhite;
	session.colorNameMap["navy"] = Colors::navy;
	session.colorNameMap["oldLace"] = Colors::oldLace;
	session.colorNameMap["olive"] = Colors::olive;
	session.colorNameMap["oliveDrab"] = Colors::oliveDrab;
	session.colorNameMap["orange"] = Colors::orange;
	session.colorNameMap["orangeRed"] = Colors::orangeRed;
	session.colorNameMap["orchid"] = Colors::orchid;
	session.colorNameMap["paleGoldenRod"] = Colors::paleGoldenRod;
	session.colorNameMap["paleGreen"] = Colors::paleGreen;
	session.colorNameMap["paleTurquoise"] = Colors::paleTurquoise;
	session.colorNameMap["paleVioletRed"] = Colors::paleVioletRed;
	session.colorNameMap["papayaWhip"] = Colors::papayaWhip;
	session.colorNameMap["peachPuff"] = Colors::peachPuff;
	session.colorNameMap["peru"] = Colors::peru;
	session.colorNameMap["pink"] = Colors::pink;
	session.colorNameMap["plum"] = Colors::plum;
	session.colorNameMap["powderBlue"] = Colors::powderBlue;
	session.colorNameMap["purple"] = Colors::purple;
	session.colorNameMap["rosyBrown"] = Colors::rosyBrown;
	session.colorNameMap["royalBlue"] = Colors::royalBlue;
	session.colorNameMap["saddleBrown"] = Colors::saddleBrown;
	session.colorNameMap["salmon"] = Colors::salmon;
	session.colorNameMap["sandyBrown"] = Colors::sandyBrown;
	session.colorNameMap["seaGreen"] = Colors::seaGreen;
	session.colorNameMap["seaShell"] = Colors::seaShell;
	session.colorNameMap["sienna"] = Colors::sienna;
	session.colorNameMap["silver"] = Colors::silver;
	session.colorNameMap["skyBlue"] = Colors::skyBlue;
	session.colorNameMap["slateBlue"] = Colors::slateBlue;
	session.colorNameMap["slateGray"] = Colors::slateGray;
	session.colorNameMap["slateGrey"] = Colors::slateGrey;
	session.colorNameMap["snow"] = Colors::snow;
	session.colorNameMap["springGreen"] = Colors::springGreen;
	session.colorNameMap["steelBlue"] = Colors::steelBlue;
	session.colorNameMap["tan"] = Colors::tan;
	session.colorNameMap["teal"] = Colors::teal;
	session.colorNameMap["thistle"] = Colors::thistle;
	session.colorNameMap["tomato"] = Colors::tomato;
	session.colorNameMap["turquoise"] = Colors::turquoise;
	session.colorNameMap["violet"] = Colors::violet;
	session.colorNameMap["wheat"] = Colors::wheat;
	session.colorNameMap["whiteSmoke"] = Colors::whiteSmoke;
	session.colorNameMap["yellowGreen"] = Colors::yellowGreen;
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
			// with a screenshot, the program quits after drawing (see draw())
			if (testScreenshot.empty()) ofExit(0);
		}
		return;
	}
	// apply any tracebacks set from the sequencer thread
	applyPendingTracebacks();
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
					if (!view.beatUpdated && ofGetElapsedTimeMillis() - session.maestroTimeStamp > MAESTROTHRESH) {
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
							session.sharedData.BPMTempi[bar] = session.msToBPM(ofGetElapsedTimeMillis() - session.maestroTimeStamp);
							sendBeatVizInfo(bar);
						}
						else {
							session.maestroInitialized = true;
						}
						view.beatUpdated = true;
						session.maestroTimeStamp = ofGetElapsedTimeMillis();
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
			uint64_t stamp = ofGetElapsedTimeMillis();
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
		// draw the pane separator for panes that do not touch the traceback printing area
		if ((paneSplitOrientation == 0 && numPanes.find(it->second.getPaneRow()+1) != numPanes.end()) || \
				(paneSplitOrientation == 1 && it->second.getPaneCol() < numPanes[it->second.getPaneRow()])) {
			it->second.drawPaneSeparator();
		}
	}
	ofSetColor(foregroundColor*brightnessCoeff);
	// draw the lines that separate the different editors
	// initializing to 0 to avoid warning messages during compilation
	float paneWidth = 0, paneHeight = 0;
	// depending on the orientation, we must first create either the height or the width of the editors
	if (paneSplitOrientation == 0) {
		paneHeight = view.tracebackBase / (float)numPanes.size();
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
				ofDrawLine(paneWidth*(float)it->first, 0, paneWidth*(float)it->first, view.tracebackYCoord);
			}
			paneHeight = view.tracebackBase / (float)it->second;
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
		}
	}
	// draw a horizontal line to separate the panes and score from the traceback area
	ofSetColor(foregroundColor*brightnessCoeff);
	ofDrawLine(0, view.tracebackYCoord, view.screenWidth, view.tracebackYCoord);
	// when the shell stops being shown (e.g. a new error message replaces the output of a command)
	// the area below the panes goes back to its size for error messages, as it does when pressing Esc
	bool shellShown = editors[session.whichPane].showingShell();
	if (shellWasShown && !shellShown) {
		shellStr = "";
		numShellLines = numShellLsOutputLines = 0;
		shellScroll = 0;
		view.tracebackYCoord = view.tracebackBase;
		restorePaneLayout();
		editors[session.whichPane].resetMaxNumLines();
		// the sizes of the areas below the panes are calculated again in drawTraceback() and drawShell()
		tracebackNumLinesShown = shellNumLinesShown = -1;
	}
	shellWasShown = shellShown;
	if (shellShown) {
		drawShell();
	}
	else{
		drawTraceback();
	}
	// the pop-up of autocompletion is drawn last, so that it is on top of everything
	editors[session.whichPane].drawCompletions();
	// in test mode with a screenshot, the window is saved a few frames after the file has run, and the program quits (see testmode.cpp)
	if (!testScreenshot.empty() && testDone && ofGetFrameNum() > testDoneFrame + 3) {
		ofSaveScreen(testScreenshot);
		ofExit(0);
	}
}

//--------------------------------------------------------------
void ofApp::drawTraceback()
{
	int tracebackCounter = 0;
	uint64_t stamps[editors[session.whichPane].allLines.size()];
	int ndxs[editors[session.whichPane].allLines.size()];
	int i = 0;
	int paneNdx = 0;
	//for (auto it = numPanes.begin(); it != numPanes.end(); ++it) {
	//	for (i = 0; i < it->second; i++) {
	//		editors[paneNdx++].resetMaxNumLines();
	//	}
	//}
	// we must first determine which traceback string was initiated first, so we can print this on top
	for (auto it = editors[session.whichPane].getAllLinesBegin(); it != editors[session.whichPane].getAllLinesEnd(); ++it) {
		if ((ofGetElapsedTimeMillis() - editors[session.whichPane].getTracebackTimeStamp(it->first)) < editors[session.whichPane].getTracebackDur() && editors[session.whichPane].getTracebackStr(it->first).size() > 0) {
			// exclude the line of the cursor as this is check below
			if (it->first != editors[session.whichPane].getCursorLineIndex()) {
				ndxs[i] = it->first;
				stamps[i] = editors[session.whichPane].getTracebackTimeStamp(it->first);
				i++;
			}
		}
		else {
			editors[session.whichPane].setTracebackTimeStamp(it->first, 0);
		}
	}
	// sort indexes based on time stamps (the smaller the time stamp, the higher it should be in the traceback)
	quickSort(stamps, ndxs, 0, i-1);
	int numTracebackLines = 0;
	bool numTracebackLinesChanged = false;
	for (int j = 0; j < i; j++) {
		numTracebackLines += editors[session.whichPane].getTracebackNumLines(ndxs[j]);
	}
	if (numTracebackLines > 0) {
		numTracebackLinesChanged = true;
	}
	// once we check the traceback messages of lines other than that where the cursor is
	// we check the line of the cursor too
	if (editors[session.whichPane].getTracebackStr(editors[session.whichPane].getCursorLineIndex()).size() > 0) {
		// if there was something in the traceback, accumulate
		if (numTracebackLinesChanged) {
			numTracebackLines += editors[session.whichPane].getTracebackNumLines(editors[session.whichPane].getCursorLineIndex());
		}
		// otherwise just set this number of lines
		else {
			numTracebackLines = editors[session.whichPane].getTracebackNumLines(editors[session.whichPane].getCursorLineIndex());
		}
		// add the line index of the cursor to ndxs
		ndxs[i] = editors[session.whichPane].getCursorLineIndex();
		// increment i so the loop below includes the line of the cursor as well
		i++;
	}
	// minimum number of lines is 2
	// so the line that separates the panes from the traceback can fit one line of text
	numTracebackLines = std::max(numTracebackLines, MINTRACEBACKLINES);
	// if the number of lines in the traceback has changed, change the maximum number of lines in the editor
	if (numTracebackLines != tracebackNumLinesShown) {
		tracebackNumLinesShown = numTracebackLines;
		// if the number of lines of the traceback changes, reset the maximum number of lines of the editor
		// need to check which editors go till the bottom of the window
		paneNdx = 0;
		for (auto it = numPanes.begin(); it != numPanes.end(); ++it) {
			for (int i = 0; i < it->second; i++) {
				// with split orientation set to 0, we need to offset the number of lines
				// for the editors on the last row, so the last key of the numPanes std::map
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
		if (numTracebackLines > MINTRACEBACKLINES) {
			float prevTracebackYCoord = view.tracebackYCoord;
			view.tracebackYCoord = view.tracebackBase - (cursorHeight * (numTracebackLines - MINTRACEBACKLINES));
			view.tracebackYCoord = std::min(view.tracebackYCoord, view.tracebackBase);
			moveScoreWithTraceback(prevTracebackYCoord);
		}
		else {
			view.tracebackYCoord = view.tracebackBase;
			resetScoreYOffset();
		}
	}
	for (int j = 0; j < i; j++) {
		switch (editors[session.whichPane].getTracebackColor(ndxs[j])) {
			case 0:
				ofSetColor(ofColor::white*brightnessCoeff);
				break;
			case 1:
				ofSetColor(ofColor::orange*brightnessCoeff);
				break;
			case 2:
				ofSetColor(ofColor::red*brightnessCoeff);
				break;
			default:
				ofSetColor(ofColor::white*brightnessCoeff);
				break;
		}
		std::string tracebackStr = "";
		// if the stderr message is a warning or error in LiveLily (not Python), prepend the line number
		if (editors[session.whichPane].getTracebackColor(ndxs[j]) > 0 && editors[session.whichPane].getLanguage() != 1) {
			tracebackStr += "line " + ofToString(ndxs[j]+1) + ": ";
		}
		tracebackStr += editors[session.whichPane].getTracebackStr(ndxs[j]);
		drawLines(tracebackStr, halfCharacterWidth, view.tracebackYCoord+(cursorHeight*(tracebackCounter+1)));
		tracebackCounter += editors[session.whichPane].getTracebackNumLines(ndxs[j]);
	}
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
void ofApp::drawShell()
{
	// the strings of the shell are broken into lines to fit the width of the window as it is now, so that they fit it also after it is resized
	// a command that is typed has no newlines, so its cursor is on the row of its position, and it can be on a row of its own
	// right after the last character of a row
	maxShellChars = std::max((size_t)1, (size_t)(view.screenWidth / oneCharacterWidth));
	std::string shellLines, shellLsOutputLines;
	if (!shellStr.empty()) {
		shellLines = wrapShellLines(shellStr, numShellLines);
		if (editors[session.whichPane].isTypingShell()) numShellLines = std::max(numShellLines, (int)(shellStrCursorPos / maxShellChars) + 1);
	}
	if (!shellLsOutputStr.empty()) shellLsOutputLines = wrapShellLines(shellLsOutputStr, numShellLsOutputLines);
	// at least two lines, so that the line that separates the panes from the area below them has one line of text under it
	int numShellLinesWanted = std::max(numShellLines + numShellLsOutputLines, MINTRACEBACKLINES);
	// the panes are made shorter when the number of lines changes, or when another pane becomes active, which then keeps its height the longest
	if (numShellLinesWanted != shellNumLinesShown || session.whichPane != shellLayoutPane) {
		shellNumLinesShown = numShellLinesWanted;
		shellLayoutPane = session.whichPane;
		shellVisibleLines = layoutPanesForShell(numShellLinesWanted);
		if (shellVisibleLines > MINTRACEBACKLINES) {
			float prevTracebackYCoord = view.tracebackYCoord;
			view.tracebackYCoord = view.tracebackBase - (cursorHeight * (shellVisibleLines - MINTRACEBACKLINES));
			view.tracebackYCoord = std::min(view.tracebackYCoord, view.tracebackBase);
			moveScoreWithTraceback(prevTracebackYCoord);
		}
		else {
			view.tracebackYCoord = view.tracebackBase;
			resetScoreYOffset();
		}
	}
	// the lines of the command and of its output, of which the lines from shellScroll on are shown, as many as fit
	// (a command that is typed is short, so it is always shown from its start)
	shellTotalLines = numShellLines + numShellLsOutputLines;
	if (editors[session.whichPane].isTypingShell()) shellScroll = 0;
	shellScroll = std::max(0, std::min(shellScroll, shellTotalLines - shellVisibleLines));
	std::vector<std::string> rows;
	auto addRows = [&rows](const std::string& lines, int numLines) {
		size_t first = rows.size();
		size_t start = 0;
		while (start <= lines.size() && !lines.empty()) {
			size_t end = lines.find('\n', start);
			if (end == std::string::npos) end = lines.size();
			rows.push_back(lines.substr(start, end - start));
			start = end + 1;
		}
		// the row of the cursor after the last character of a full row has no characters
		while (rows.size() < first + (size_t)numLines) rows.push_back("");
	};
	addRows(shellLines, numShellLines);
	addRows(shellLsOutputLines, numShellLsOutputLines);
	std::string shownRows;
	for (int i = shellScroll; i < std::min((int)rows.size(), shellScroll + shellVisibleLines); i++) {
		if (i > shellScroll) shownRows += "\n";
		shownRows += rows[i];
	}
	// the area below the panes has already moved up to fit all the lines, so the first line is right below its top
	float y = view.tracebackYCoord + cursorHeight;
	ofSetColor(shellStrColor*brightnessCoeff);
	drawLines(shownRows, halfCharacterWidth, y);
	if (editors[session.whichPane].isTypingShell()) {
		float cursorX = halfCharacterWidth + ((shellStrCursorPos % maxShellChars) * oneCharacterWidth);
		float cursorY = y + ((shellStrCursorPos / maxShellChars) * cursorHeight);
		ofDrawRectangle(cursorX, cursorY - (cursorHeight * 0.85), oneCharacterWidth, cursorHeight);
		if (shellStrCursorPos < (int)shellStr.size()) {
			ofSetColor(backgroundColor*brightnessCoeff);
			font.drawString(shellStr.substr(shellStrCursorPos, 1), cursorX, cursorY);
		}
	}
}

//--------------------------------------------------------------
int ofApp::layoutPanesForShell(int wanted)
{
	int extra = wanted - MINTRACEBACKLINES;
	if (extra <= 0) {
		restorePaneLayout();
		return MINTRACEBACKLINES;
	}
	// the layout of the panes before they are made shorter, which is stored once, so that it can be given back (see restorePaneLayout())
	if (paneBase.empty()) {
		for (auto& editor : editors) {
			paneBase[editor.first] = {editor.second.getMaxNumLinesReset(), editor.second.getFrameYOffset(), editor.second.getFrameHeight()};
		}
	}
	// the panes are in columns of rows, which all reach the area below the panes
	// with split orientation 0 there is one column, with the rows of numPanes, and the panes of each row side by side
	// with split orientation 1, every item of numPanes is a column, with one pane in each of its rows
	// (the pane indexes follow the same order as in setPaneCoords())
	std::vector<std::vector<std::vector<int>>> columns;
	int paneNdx = 0;
	if (paneSplitOrientation == 0) {
		columns.resize(1);
		for (const auto& row : numPanes) {
			std::vector<int> panes;
			for (int i = 0; i < row.second; i++) panes.push_back(paneNdx++);
			columns[0].push_back(panes);
		}
	}
	else {
		for (const auto& column : numPanes) {
			std::vector<std::vector<int>> rows;
			for (int i = 0; i < column.second; i++) rows.push_back({paneNdx++});
			columns.push_back(rows);
		}
	}
	const int minRowLines = 2; // one line of text and the line with the file name
	auto rowLines = [this](const std::vector<int>& row) {
		auto it = paneBase.find(row.front());
		return (it != paneBase.end() ? it->second.lines : minRowLines);
	};
	// the most lines the shell can get is what the column that can give the fewest lines can give
	for (const auto& column : columns) {
		int canGive = 0;
		for (const auto& row : column) canGive += std::max(0, rowLines(row) - minRowLines);
		extra = std::min(extra, canGive);
	}
	for (const auto& column : columns) {
		int numRows = (int)column.size();
		// the row of the active pane, if it is in this column
		int activeRow = -1;
		for (int r = 0; r < numRows; r++) {
			if (std::find(column[r].begin(), column[r].end(), session.whichPane) != column[r].end()) activeRow = r;
		}
		// the rows give lines from the bottom up, and the row of the active pane last
		std::vector<int> order;
		for (int r = numRows - 1; r >= 0; r--) {
			if (r != activeRow) order.push_back(r);
		}
		if (activeRow >= 0) order.push_back(activeRow);
		std::vector<int> taken(numRows, 0);
		int left = extra;
		for (int r : order) {
			taken[r] = std::min(left, std::max(0, rowLines(column[r]) - minRowLines));
			left -= taken[r];
		}
		// every row moves up by the lines the rows above it gave
		int above = 0;
		for (int r = 0; r < numRows; r++) {
			for (int pane : column[r]) {
				auto baseIt = paneBase.find(pane);
				if (baseIt == paneBase.end()) continue;
				if (taken[r] > 0) editors[pane].offsetMaxNumLines(-taken[r]);
				else editors[pane].resetMaxNumLines();
				editors[pane].setFrameYOffset(baseIt->second.y - (above * cursorHeight));
				editors[pane].setFrameHeight(baseIt->second.height - (taken[r] * cursorHeight));
			}
			above += taken[r];
		}
	}
	return MINTRACEBACKLINES + extra;
}

//--------------------------------------------------------------
void ofApp::restorePaneLayout()
{
	for (const auto& base : paneBase) {
		auto it = editors.find(base.first);
		if (it == editors.end()) continue;
		it->second.resetMaxNumLines();
		it->second.setFrameYOffset(base.second.y);
		it->second.setFrameHeight(base.second.height);
	}
	paneBase.clear();
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
	scoreYOffset += (view.tracebackYCoord - prevTracebackYCoord);
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
				if ((ofGetElapsedTimeMillis() - view.beatVizTimeStamp) >= view.beatVizRampStart) {
					// calculate how many steps the brightness has to dim, depending on the elapsed time and the steps per millisecond
					int brightnessDegrade = (int)((ofGetElapsedTimeMillis()-(view.beatVizTimeStamp+view.beatVizRampStart))*view.beatVizStepsPerMs);
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
		uint64_t timeStamp = ofGetElapsedTimeMillis();
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
				for (size_t j = 0; j < instIt->second.notes.at(bar).size(); j++) {
					float noteLength = (float)instIt->second.durs.at(bar).at(j) * view.pianoRollMinDur;
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
			float progress = std::clamp((float)(ofGetElapsedTimeMillis() - view.pianoRollTimeStamp) / barMs, 0.0f, 1.0f);
			int step = std::min(numSteps - 1, (int)(progress * (float)barLength / (float)stepLength));
			ofColor red = ofColor::red * brightnessCoeff;
			ofSetColor(red.r, red.g, red.b, 191);
			ofDrawRectangle(gridX + (square * (float)step), gridY, square, square * (float)order.size());
		}
	}
}

/******************** key press functions **********************/
//--------------------------------------------------------------
// Shift selects text when moving the cursor only in insert mode
// in normal mode the selection extends only in visual mode, whether Shift is pressed or not
// (keys like G, J, K, H, and L are typed with Shift, and they should only move the cursor)
// after moving the cursor, the Shift state of the editor is restored with setShiftPressed(shiftPressed)
void ofApp::setNormalModeShift()
{
	if (!editors[session.whichPane].getInserting()) editors[session.whichPane].setShiftPressed(editors[session.whichPane].getVisualMode());
}


//--------------------------------------------------------------
// all LiveLily commands and the names defined in the session (instruments, bars, functions, etc.), with their backslash
// these are the names of commandsMap that start with a backslash, which is also used for the colors of the editor
std::vector<std::string> ofApp::getCommandCompletions()
{
	std::set<std::string> names;
	for (const auto& byChar : session.commandsMap[Session::livelily]) {
		for (const auto& cmd : byChar.second) {
			if (cmd.first.size() > 1 && cmd.first[0] == '\\') names.insert(cmd.first);
		}
	}
	return std::vector<std::string>(names.begin(), names.end());
}

//--------------------------------------------------------------
std::vector<std::string> ofApp::getArgumentCompletions()
{
	std::set<std::string> names;
	for (const auto& byChar : session.commandsMap[Session::livelily]) {
		for (const auto& cmd : byChar.second) {
			if (!cmd.first.empty() && cmd.first[0] != '\\') names.insert(cmd.first);
		}
	}
	return std::vector<std::string>(names.begin(), names.end());
}

//--------------------------------------------------------------
bool ofApp::startsMelodicLine(const std::string& word)
{
	return session.sharedData.instrumentIndexes.find(word) != session.sharedData.instrumentIndexes.end() || session.instGroups.find(word) != session.instGroups.end();
}

//--------------------------------------------------------------
// the strings of the interpreter have one name per line (see Pyo::names() and Pyo::signature())
static std::vector<std::string> pythonLines(const std::string& str)
{
	std::vector<std::string> lines;
	size_t start = 0;
	while (start < str.size()) {
		size_t end = str.find('\n', start);
		if (end == std::string::npos) end = str.size();
		if (end > start) lines.push_back(str.substr(start, end - start));
		start = end + 1;
	}
	return lines;
}

bool ofApp::refreshPythonTable()
{
#ifdef USEPYO
	if (!session.pyoSet || !session.sharedData.pyo.isSetup()) return false;
	unsigned executions = session.sharedData.pyo.getExecutions();
	if (!pythonTable.made || pythonTable.executions != executions) {
		std::vector<std::string> names = pythonLines(session.sharedData.pyo.globalNames());
		pythonTable.names = std::set<std::string>(names.begin(), names.end());
		pythonTable.attributes.clear();
		pythonTable.signatures.clear();
		pythonTable.executions = executions;
		pythonTable.made = true;
	}
	return true;
#else
	return false;
#endif
}

std::vector<std::string> ofApp::pythonNames(const std::string& dotted)
{
#ifdef USEPYO
	if (!refreshPythonTable()) return {};
	if (dotted.empty()) return std::vector<std::string>(pythonTable.names.begin(), pythonTable.names.end());
	// a name that is not in the interpreter has no attributes to ask for
	if (pythonTable.names.find(dotted.substr(0, dotted.find('.'))) == pythonTable.names.end()) return {};
	auto it = pythonTable.attributes.find(dotted);
	if (it != pythonTable.attributes.end()) return it->second;
	std::vector<std::string> attributes = pythonLines(session.sharedData.pyo.names(dotted));
	pythonTable.attributes[dotted] = attributes;
	return attributes;
#else
	(void)dotted;
	return {};
#endif
}

std::vector<std::string> ofApp::pythonSignature(const std::string& dotted)
{
#ifdef USEPYO
	if (!refreshPythonTable() || dotted.empty()) return {};
	if (pythonTable.names.find(dotted.substr(0, dotted.find('.'))) == pythonTable.names.end()) return {};
	auto it = pythonTable.signatures.find(dotted);
	if (it != pythonTable.signatures.end()) return it->second;
	std::vector<std::string> params = pythonLines(session.sharedData.pyo.signature(dotted));
	pythonTable.signatures[dotted] = params;
	return params;
#else
	(void)dotted;
	return {};
#endif
}

//--------------------------------------------------------------
// the methods of a command or of an object that is defined in the session, by its name with a backslash (e.g. \score or \violin)
std::vector<std::string> ofApp::getMethodCompletions(const std::string& name)
{
	static const std::map<std::string, std::vector<std::string>> commandMethods = {
		{"\\score", {"animation", "beatsin", "beattype", "colors", "correct", "finish", "hide", "movex", "movey",
					 "numbars", "play", "recenter", "reset", "seqres", "show", "stop", "stopnow", "update"}},
		{"\\editor", {"alignment", "animation", "autobrackets", "autocomplete"}},
		{"\\insts", {"add", "clear", "init", "remove"}},
		{"\\python", {"send"}},
		{"\\clicktrack", {"tune", "volume"}},
		{"\\maestro", {"levareaddr", "mainaddr", "toggleaddr", "valndx", "valthresh"}}
	};
	// groups of instruments take the methods of instruments
	static const std::vector<std::string> instrumentMethods = {"accoffset", "beatcolor", "cursor", "delay", "fullscreen", "midichan",
		"midiport", "numbars", "rhythm", "sendmidi", "sendto", "size", "sndfont", "transpose", "update"};
	static const std::vector<std::string> functionMethods = {"bind", "onrelease", "setargs", "unbind"};
	static const std::vector<std::string> listMethods = {"traverse"};
	static const std::vector<std::string> barLoopMethods = {"check", "goto", "locate"};
	static const std::vector<std::string> oscClientMethods = {"send", "setup"};
	static const std::vector<std::string> oscServerMethods = {"map", "setup"};
	static const std::vector<std::string> variableMethods = {"val"};
	auto it = commandMethods.find(name);
	if (it != commandMethods.end()) return it->second;
	// some of the maps below store names with their backslash and some without it
	std::string plainName = (startsWith(name, "\\") ? name.substr(1) : name);
	auto has = [&](const auto& m) { return m.find(name) != m.end() || m.find(plainName) != m.end(); };
	if (has(session.sharedData.instrumentIndexes) || has(session.instGroups)) return instrumentMethods;
	if (has(session.functionIndexes)) return functionMethods;
	if (has(session.listIndexes)) return listMethods;
	if (has(session.sharedData.barsIndexes) || has(session.sharedData.loopsIndexes)) return barLoopMethods;
	if (has(session.oscClients)) return oscClientMethods;
	if (has(session.oscServers)) return oscServerMethods;
	if (has(session.variables)) return variableMethods;
	return {};
}

//--------------------------------------------------------------
// the function that is called when a key is pressed
// or when a key press is simulated from OSC
void ofApp::executeKeyPressed(int key)
{
	// while asking to save before quitting, the keys answer the question (see quitPromptKey())
	// except the modifier keys, and the arrow keys while typing a path
	bool modifierKey = (key == 0 || key == 1 || key == 2 || key == 4 || (key >= 3680 && key <= 3685));
	bool arrowKey = (key >= 57356 && key <= 57361);
	if (quitPromptState > 0 && !modifierKey && !(arrowKey && quitPromptState == 2)) {
		quitPromptKey(key);
		return;
	}
	// the output of a shell command that doesn't fit the area below the panes scrolls with the up and down arrows
	// and with Page Up and Page Down by the lines that are shown (see drawShell()), until Esc clears it
	if (editors[session.whichPane].showingShell() && !editors[session.whichPane].isTypingShell() && shellTotalLines > shellVisibleLines &&
			(key == 57357 || key == 57359 || key == 57360 || key == 57361)) {
		if (key == 57357) shellScroll--;
		else if (key == 57359) shellScroll++;
		else if (key == 57360) shellScroll -= shellVisibleLines;
		else shellScroll += shellVisibleLines;
		shellScroll = std::max(0, std::min(shellScroll, shellTotalLines - shellVisibleLines));
		return;
	}
	switch (key) {
		case 0: // caps lock
			break;
		case 1:
			editors[session.whichPane].setShiftPressed(true);
			shiftPressed = true;
			break;
		case 2:
			editors[session.whichPane].setCtrlPressed(true);
			ctrlPressed = true;
			break;
		case 4:
			editors[session.whichPane].setAltPressed(true);
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
			if (!editors[session.whichPane].isTypingShell()) {
				setNormalModeShift();
				editors[session.whichPane].upArrow(-1); // -1 for single increment
				editors[session.whichPane].setShiftPressed(shiftPressed);
			}
			break;
		case 57359:
			if (!editors[session.whichPane].isTypingShell()) {
				setNormalModeShift();
				editors[session.whichPane].downArrow(-1); // -1 for single decrement
				editors[session.whichPane].setShiftPressed(shiftPressed);
			}
			break;
		case 57358:
			if (!editors[session.whichPane].isTypingShell()) {
				setNormalModeShift();
				editors[session.whichPane].rightArrow();
				editors[session.whichPane].setShiftPressed(shiftPressed);
			}
			else {
				shellStrCursorPos++;
				if (shellStrCursorPos > (int)shellStr.size()) shellStrCursorPos = (int)shellStr.size();
			}
			break;
		case 57356:
			if (!editors[session.whichPane].isTypingShell()) {
				setNormalModeShift();
				editors[session.whichPane].leftArrow();
				editors[session.whichPane].setShiftPressed(shiftPressed);
			}
			else {
				shellStrCursorPos--;
				if (shellStrCursorPos < 0) shellStrCursorPos = 0;
			}
			break;
		case 57360:
			if (!editors[session.whichPane].isTypingShell()) {
				setNormalModeShift();
				editors[session.whichPane].pageUp();
				editors[session.whichPane].setShiftPressed(shiftPressed);
			}
			break;
		case 57361:
			if (!editors[session.whichPane].isTypingShell()) {
				setNormalModeShift();
				editors[session.whichPane].pageDown();
				editors[session.whichPane].setShiftPressed(shiftPressed);
			}
			break;
		case 27: // esc
			editors[session.whichPane].setInserting(false);
			editors[session.whichPane].setTypingShell(false);
			editors[session.whichPane].setShowingShell(false);
			// the output of a command is cleared, and the panes get back the lines they gave to it (see layoutPanesForShell())
			shellStr = "";
			shellLsOutputStr = "";
			shellScroll = 0;
			restorePaneLayout();
			editors[session.whichPane].resetMaxNumLines();
			if (editors[session.whichPane].getVisualMode()) editors[session.whichPane].setVisualMode(false);
			if (editors[session.whichPane].highlightManyChars) editors[session.whichPane].highlightManyChars = false;
			// if we have typed commands and their output is greater than two lines
			// then sharedData.tracebackYCoord will have changed so we need to reset it
			view.tracebackYCoord = view.tracebackBase;
			// the same applies to the number of lines for commands and their output
			numShellLines = numShellLsOutputLines = 0;
			// and the sizes of the areas are calculated again the next time they are shown
			shellNumLinesShown = tracebackNumLinesShown = -1;
			//resetScoreYOffset();
		default:
		{
			// keys that wait for a second key (g and the first key of a two-key binding) wait for the very next key only
			// so their state is reset here, and what it was is kept for the checks in normal mode below
			// (a two-key binding is also cancelled when its first key is released, see executeKeyReleased())
			bool gWasPressed = gPressed;
			bool dWasPressed = dPressed;
			dPressed = false;
			// after Ctrl+w in normal mode: h, j, k, and l move to the pane on the left, below, above, and on the right
			// q and c close the pane, and any other key (e.g. Esc) cancels Ctrl+w
			bool ctrlWWasPressed = ctrlWPressed;
			ctrlWPressed = false;
			if (ctrlWWasPressed && !editors[session.whichPane].getInserting() && !editors[session.whichPane].isTypingShell()) {
				switch (key) {
					case 'h': moveToPane(0); break;
					case 'j': moveToPane(1); break;
					case 'k': moveToPane(2); break;
					case 'l': moveToPane(3); break;
					case 'q':
					case 'c':
						if (editors[session.whichPane].isHelpFileOpen()) editors[session.whichPane].closeHelpFile();
						else removePane();
						break;
					default:
						break;
				}
				return;
			}
			editors[session.whichPane].setDPressed(false);
			int pendingBindingKey = editorBindingKey;
			int pendingBindingArg = editorBindingArg;
			bool pendingBindingIsDollar = editorBindingIsDollar;
			gPressed = false;
			editorBindingArg = editorBindingKey = -1;
			editorBindingIsDollar = false;
			if (key == 61 && editors[session.whichPane].isCtrlPressed() && !editors[session.whichPane].isAltPressed()) { // +
				fontSize += 2;
				fontLoaded = false;
				setFontSize(true); // set font size and calculate pane coordinates
			}
			else if (key == 45 && editors[session.whichPane].isCtrlPressed() && !editors[session.whichPane].isAltPressed()) { // -
				fontSize -= 2;
				if (fontSize == 0) fontSize = 2;
				fontLoaded = false;
				setFontSize(true);
			}
			else if (key == 43 && editors[session.whichPane].isCtrlPressed()) { // + with shift pressed
				view.staffLinesDist += 2;
				view.scoreFontSize = (int)(35.0 * view.staffLinesDist / 10.0);
				instFontSize = view.scoreFontSize / 3.5;
				instFont.load("Monaco.ttf", instFontSize);
				setScoreSizes();
			}
			else if (key == 95 && editors[session.whichPane].isCtrlPressed()) { // - with shift pressed
				view.staffLinesDist -= 2;
				if (view.staffLinesDist < 4) view.staffLinesDist = 4;
				view.scoreFontSize = (int)(35.0 * view.staffLinesDist / 10.0);
				instFontSize = view.scoreFontSize / 3.5;
				instFont.load("Monaco.ttf", instFontSize);
				setScoreSizes();
			}
			else if (key == 61 && editors[session.whichPane].isCtrlPressed() && editors[session.whichPane].isAltPressed()) { // + (actually =)
				brightnessCoeff += 0.01;
				if (brightnessCoeff > 1) brightnessCoeff = 1;
			}
			else if (key == 45 && editors[session.whichPane].isCtrlPressed() && editors[session.whichPane].isAltPressed()) { // -
				brightnessCoeff -= 0.01;
				if (brightnessCoeff < 0) brightnessCoeff = 0;
			}
			else if (key == 102 && editors[session.whichPane].isCtrlPressed()) { // f
				if (editors[session.whichPane].getInserting()) {
					ofToggleFullscreen();
					fullScreen = !fullScreen;
				}
				else {
					setNormalModeShift();
					editors[session.whichPane].pageDown();
					editors[session.whichPane].setShiftPressed(shiftPressed);
				}
			}
			else if (char(key) == 'q' && editors[session.whichPane].isCtrlPressed()) {
				requestQuit();
			}
			else if (char(key) == 'n' && editors[session.whichPane].isCtrlPressed()) {
				// remove all panes but one
				for (size_t i = 0; i < editors.size()-1; i++) {
					removePane();
				}
				{
					// the sequencer might be playing, so the instruments are not cleared while it goes through them
					std::lock_guard<std::recursive_mutex> instrumentsLock(session.sharedData.instrumentsMutex);
					session.clearBars();
					session.clearLoops();
					session.clearLists();
					session.clearFunctions();
					session.clearInstruments();
				}
				// hide score
				setShowScore(-1);
				editors[session.whichPane].clearText();
				editors[session.whichPane].createNewLine("", 1);
				// a new session starts a new history
				editors[session.whichPane].clearUndoHistory();
			}
			// add panes. V for vertical, H for horizontal (with shift pressed)
			else if (((key == 86) || (key == 72)) && editors[session.whichPane].isCtrlPressed()) {
				addPane(key);
			}
			// remove panes. Ctrl+w, where if only one pane, we quit
			// in normal mode Ctrl+w waits for a key to move to another pane, or to close this one, as in Vim (see above)
			else if (key == 119 && editors[session.whichPane].isCtrlPressed() && !editors[session.whichPane].getInserting() && !editors[session.whichPane].isTypingShell()) {
				ctrlWPressed = true;
			}
			else if (key == 119 && editors[session.whichPane].isCtrlPressed()) {
				if (editors[session.whichPane].isHelpFileOpen()) {
					editors[session.whichPane].closeHelpFile();
				}
				else {
					removePane();
				}
			}
			// set which editor is active with keys 0-9 and alt pressed
			else if ((key >= 49) && (key <= 57) && altPressed) {
				if ((key - 49) < (int)editors.size()) {
					setActivePane(key - 49);
					revealActivePane();
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
			else if (key == 105 && !editors[session.whichPane].getInserting() && !editors[session.whichPane].isTypingShell()) { // i
				editors[session.whichPane].setInserting(true);
				shellStr = "";
				editors[session.whichPane].resetMaxNumLines();
			}
			else if (!editors[session.whichPane].getInserting() && ((key == 58 && !editors[session.whichPane].getVisualMode()) || editors[session.whichPane].isTypingShell())) { // :
				typeShellCommand(key);
			}
			else if (!editors[session.whichPane].getInserting() && !editors[session.whichPane].isTypingShell()) {
				// bindings are not checked for Ctrl+key and Enter
				bool bindingKey = !editors[session.whichPane].isCtrlPressed() && key != 13;
				// if we are holding a key bound with a second key and we now press the second key
				// this is checked first, so that the second key can be a reserved key, without the function it has in normal mode
				if (bindingKey && pendingBindingKey >= 0 && (key == pendingBindingArg || (pendingBindingIsDollar && key >= 48 && key <= 57))) {
					if (pendingBindingIsDollar) {
						shellStr = editors[session.whichPane].bindings[pendingBindingKey].actionVec[0] + " " + std::to_string(key - 48);
					}
					else {
						shellStr = session.genStrFromVec(editors[session.whichPane].bindings[pendingBindingKey].actionVec);
					}
					executingBind = true;
					shellTabCounter = 0;
					shellStrCursorPos = (int)shellStr.size();
					editors[session.whichPane].setTypingShell(true);
					editors[session.whichPane].setShowingShell(true);
				}
				// enable executing chunks of code in normal mode too
				else if ((editors[session.whichPane].isCtrlPressed() || editors[session.whichPane].isShiftPressed()) && key == 13) {
					editors[session.whichPane].allOtherKeys(key);
				}
				// Ctrl+r redoes and u undoes in normal mode, as in Vim (in insert mode these are Ctrl+y and Ctrl+z, see Editor::allOtherKeys())
				else if (key == 114 && editors[session.whichPane].isCtrlPressed()) { // r
					editors[session.whichPane].redo();
				}
				else if (key == 117 && !editors[session.whichPane].isCtrlPressed()) { // u
					editors[session.whichPane].undo();
				}
				else if ((key == 106 || key == 74)) { // j  or J
					setNormalModeShift();
					editors[session.whichPane].downArrow(-1); // -1 for single increment
					editors[session.whichPane].setShiftPressed(shiftPressed);
				}
				else if ((key == 107 || key == 75)) { // k or K
					setNormalModeShift();
					editors[session.whichPane].upArrow(-1); // -1 for single decrement
					editors[session.whichPane].setShiftPressed(shiftPressed);
				}
				else if ((key == 104 || key == 72)) { // h or H
					setNormalModeShift();
					editors[session.whichPane].leftArrow();
					editors[session.whichPane].setShiftPressed(shiftPressed);
				}
				else if ((key == 108 || key == 76)) { // l or L
					setNormalModeShift();
					editors[session.whichPane].rightArrow();
					editors[session.whichPane].setShiftPressed(shiftPressed);
				}
				else if ((key == 98 && editors[session.whichPane].isCtrlPressed())) { // b
					setNormalModeShift();
					editors[session.whichPane].pageUp();
					editors[session.whichPane].setShiftPressed(shiftPressed);
				}
				else if (key == 121) { // y
					editors[session.whichPane].copyString(1); // 1 is index for yanking a string
				}
				else if (key == 112) { // p
					editors[session.whichPane].pasteString(1); //  1 is index for pasting a yanked string
				}
				else if (key == 118 && editors[session.whichPane].isCtrlPressed()) {
					// Ctrl+v selects a block of columns, as in Vim (in insert mode Ctrl+v pastes, see Editor::allOtherKeys())
					editors[session.whichPane].startVisualSelection(2);
				}
				else if (key == 118) {
					// letter v
					editors[session.whichPane].startVisualSelection(0); // immitating Vim's Visual mode
				}
				else if (key == 86 && !editors[session.whichPane].isCtrlPressed()) {
					// V selects whole lines, as in Vim (Ctrl+V adds a pane, see above)
					editors[session.whichPane].startVisualSelection(1);
				}
				else if (key == 68 && !editors[session.whichPane].isCtrlPressed()) { // D
					// in visual mode D deletes whole lines, otherwise it deletes from the cursor to the end of the line, as in Vim
					// in visual mode D deletes the lines of the selection, as in Vim (see Editor::deleteVisualLines())
					if (editors[session.whichPane].getVisualMode()) {
						editors[session.whichPane].deleteVisualLines();
					}
					else {
						editors[session.whichPane].deleteToLineEnd();
					}
				}
				else if (key == 73 && !editors[session.whichPane].isCtrlPressed()) { // I
					// after Ctrl+v, I inserts the same text in all lines of the block, otherwise it inserts at the start of the line
					if (editors[session.whichPane].getVisualMode() && editors[session.whichPane].getSelectionMode() == 2) {
						editors[session.whichPane].startBlockInsert();
					}
					else {
						editors[session.whichPane].insertAtLineStart();
					}
				}
				else if (key == 65) {
					// capital A when in normal mode sets pane to insert mode and moves cursor to end of line
					editors[session.whichPane].setCursorPos(editors[session.whichPane].maxCursorPos()+1);
					editors[session.whichPane].setInserting(true);
				}
				else if (key == 100) { // d
					if (editors[session.whichPane].highlightManyChars) {
						// the selection is yanked without clearing it (false), so that deleteString() deletes all of it
						// and not only the character under the cursor (deleteString() clears it afterwards)
						editors[session.whichPane].copyString(1, false); // 1 is index for yanking a string
						editors[session.whichPane].deleteString();
						if (editors[session.whichPane].getVisualMode()) editors[session.whichPane].setVisualMode(false);
					}
					// right after v, before the cursor moves, the selection is the character under the cursor, as in Vim
					else if (editors[session.whichPane].getVisualMode()) {
						editors[session.whichPane].copyString(1); // 1 is index for yanking a string
						editors[session.whichPane].deleteString();
						editors[session.whichPane].setVisualMode(false);
					}
					// without a selection, dd deletes the line, as in Vim
					else if (dWasPressed) {
						editors[session.whichPane].deleteCurrentLine();
						if (editors[session.whichPane].getVisualMode()) editors[session.whichPane].setVisualMode(false);
						editors[session.whichPane].setDPressed(false);
					}
					else {
						dPressed = true;
						editors[session.whichPane].setDPressed(true);
					}
				}
				else if (key == 120 && !editors[session.whichPane].isCtrlPressed()) { // x
					// x deletes the character under the cursor, as in Vim
					editors[session.whichPane].copyString(1); // 1 is index for yanking a string
					editors[session.whichPane].deleteString();
				}
				else if (key == 48) { // 0
					editors[session.whichPane].setCursorPos(0);
				}
				else if (key == 36) { // dollar sign
					editors[session.whichPane].setCursorPos(editors[session.whichPane].maxCursorPos());
				}
				else if (key == 103) { // g, twice moves the cursor to the first line
					if (gWasPressed) {
						setNormalModeShift();
						editors[session.whichPane].upArrow(0);
						editors[session.whichPane].setShiftPressed(shiftPressed);
					}
					else {
						gPressed = true;
					}
				}
				else if (key == 71) { // G moves the cursor to the last line
					// G is typed with Shift, which makes the editor select text when moving the cursor
					// so Shift is considered pressed only in visual mode, where the selection extends
					setNormalModeShift();
					editors[session.whichPane].downArrow(editors[session.whichPane].lineCount-1);
					editors[session.whichPane].setShiftPressed(shiftPressed);
				}
				// bound keys (the keys of normal mode above are reserved and can't be bound)
				else if (bindingKey && editors[session.whichPane].bindings.find(key) != editors[session.whichPane].bindings.end()) {
					// if the binding is two keys, we execute on the press of the second
					if (editors[session.whichPane].bindings[key].bindingVec[0].size() > 0) {
						editorBindingIsDollar = editors[session.whichPane].bindings[key].bindingVec[0].compare("$") == 0;
						editorBindingKey = key;
						editorBindingArg = static_cast<int>(editors[session.whichPane].bindings[key].bindingVec[0][0]);
					}
					// otherwise we execute on the press of the first
					else {
						shellStr = session.genStrFromVec(editors[session.whichPane].bindings[key].actionVec);
						executingBind = true;
						shellTabCounter = 0;
						shellStrCursorPos = (int)shellStr.size();
						editors[session.whichPane].setTypingShell(true);
						editors[session.whichPane].setShowingShell(true);
					}
				}
			}
			else if (editors[session.whichPane].getInserting()) {
				editors[session.whichPane].allOtherKeys(key);
			}
			break;
		}
	}
}

//--------------------------------------------------------------
// the function that is called when a key is released
// or when a key release is simulated from OSC
void ofApp::executeKeyReleased(int key)
{
	switch (key) {
		case 1:
			editors[session.whichPane].setShiftPressed(false);
			shiftPressed = false;
			break;
		case 2:
			editors[session.whichPane].setCtrlPressed(false);
			ctrlPressed = false;
			break;
		case 4:
			editors[session.whichPane].setAltPressed(false);
			altPressed = false;
			break;
		default:
			// a two-key binding works only while its first key is held, so releasing the first key cancels it
			if (key == editorBindingKey) {
				editorBindingArg = editorBindingKey = -1;
				editorBindingIsDollar = false;
			}
			break;
	}
}

//--------------------------------------------------------------
// overloaded keyPressed() function called from OSC
void ofApp::keyPressedOsc(int key, int thisEditor)
{
	mutex.lock();
	int whichPaneBackup = session.whichPane;
	// change the currently active editor temporarily
	// because this overloaded function is called from an editor
	// when it receives input from OSC
	// this way, even if an editor is out of focus
	// we're still able to type in it properly
	session.whichPane = thisEditor;
	executeKeyPressed(key);
	session.whichPane = whichPaneBackup;
	mutex.unlock();
}

//--------------------------------------------------------------
// overloaded keyReleased() function called from OSC
void ofApp::keyReleasedOsc(int key, int thisPane)
{
	// the same applies to releasing a key
	mutex.lock();
	int whichPaneBackup = session.whichPane;
	session.whichPane = thisPane;
	executeKeyReleased(key);
	session.whichPane = whichPaneBackup;
	mutex.unlock();
}

//--------------------------------------------------------------
// OF's default keyPressed() function
void ofApp::keyPressed(int key)
{
	mutex.lock();
	if (editors[session.whichPane].getSendKeys()) {
		OscMessage m;
		m.setAddress("/livelily"+std::to_string(editors[session.whichPane].getSendKeysPaneNdx())+"/press");
		m.addIntArg(key);
		editors[session.whichPane].oscKeys->send(m);
	}
	executeKeyPressed(key);
	mutex.unlock();
}

//--------------------------------------------------------------
// OF's default keyReleased() function
void ofApp::keyReleased(int key)
{
	mutex.lock();
	if (editors[session.whichPane].getSendKeys()) {
		OscMessage m;
		m.setAddress("/livelily"+std::to_string(editors[session.whichPane].getSendKeysPaneNdx())+"/release");
		m.addIntArg(key);
		editors[session.whichPane].oscKeys->send(m);
	}
	executeKeyReleased(key);
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
// sets the row and column of every pane from the layout
// panes are laid out by the order of their keys in editors, row by row (or column by column with paneSplitOrientation 1)
// with numPanes holding how many panes every row has (see setPaneCoords())
void ofApp::setPanesRowsAndCols()
{
	int key = 0;
	for (const auto& row : numPanes) {
		for (int col = 0; col < row.second; col++) {
			auto editIt = editors.find(key++);
			if (editIt == editors.end()) continue;
			editIt->second.setPaneRow(row.first);
			editIt->second.setPaneCol(col);
		}
	}
}

//--------------------------------------------------------------
void ofApp::moveToPane(int direction)
{
	Editor& current = editors[session.whichPane];
	float left = current.getFrameXOffset();
	float right = left + current.getFrameWidth();
	float top = current.getFrameYOffset();
	float bottom = top + current.getFrameHeight();
	// the position of the cursor, so that of more panes in a direction, the one next to the cursor is chosen, as in Vim
	float cursorX = current.getCursorScreenX();
	float cursorY = current.getCursorScreenY();
	const float tolerance = 2;
	int best = -1;
	float bestDistance = FLT_MAX;
	bool bestByCursor = false;
	for (auto& editor : editors) {
		if (editor.first == session.whichPane) continue;
		Editor& other = editor.second;
		float oLeft = other.getFrameXOffset();
		float oRight = oLeft + other.getFrameWidth();
		float oTop = other.getFrameYOffset();
		float oBottom = oTop + other.getFrameHeight();
		// the pane must be on that side of the active one, and overlap with it on the other axis
		float distance;
		bool overlaps, byCursor;
		switch (direction) {
			case 0:
				if (oRight > left + tolerance) continue;
				distance = left - oRight;
				overlaps = (oTop < bottom && oBottom > top);
				byCursor = (cursorY >= oTop && cursorY < oBottom);
				break;
			case 1:
				if (oTop < bottom - tolerance) continue;
				distance = oTop - bottom;
				overlaps = (oLeft < right && oRight > left);
				byCursor = (cursorX >= oLeft && cursorX < oRight);
				break;
			case 2:
				if (oBottom > top + tolerance) continue;
				distance = top - oBottom;
				overlaps = (oLeft < right && oRight > left);
				byCursor = (cursorX >= oLeft && cursorX < oRight);
				break;
			default:
				if (oLeft < right - tolerance) continue;
				distance = oLeft - right;
				overlaps = (oTop < bottom && oBottom > top);
				byCursor = (cursorY >= oTop && cursorY < oBottom);
				break;
		}
		if (!overlaps) continue;
		// the closest pane, and of panes equally close, the one by the cursor
		if (distance < bestDistance - tolerance || (std::abs(distance - bestDistance) <= tolerance && byCursor && !bestByCursor)) {
			best = editor.first;
			bestDistance = distance;
			bestByCursor = byCursor;
		}
	}
	if (best >= 0) {
		setActivePane(best);
		revealActivePane();
	}
}

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

//--------------------------------------------------------------
// adds a pane next to the active one, with Ctrl+Shift+V (key 86) side by side, and with Ctrl+Shift+H (key 72) one above the other, as in Vim
// the first split sets whether rows of panes are stacked (paneSplitOrientation 0, after Ctrl+Shift+H) or columns are put side by side (1)
// a split in the other direction adds a new row (or column) after the one of the active pane
void ofApp::addPane(int key)
{
	bool firstAddition = (editors.size() == 1);
	if (firstAddition) {
		if (key == 86) paneSplitOrientation = 1;
		else paneSplitOrientation = 0;
	}
	int paneRowWithCursor = editors[session.whichPane].getPaneRow();
	bool sameRow = ((paneSplitOrientation == 0 && key == 86) || (paneSplitOrientation == 1 && key == 72));
	// the key of the new pane, right after the active pane in its row, or after the last pane of its row, at the start of a new row
	int newKey;
	if (sameRow) {
		newKey = session.whichPane + 1;
		numPanes[paneRowWithCursor]++;
	}
	else {
		newKey = 0;
		std::map<int, int> rows;
		int rowNdx = 0;
		for (const auto& row : numPanes) {
			rows[rowNdx++] = row.second;
			if (row.first <= paneRowWithCursor) newKey += row.second;
			if (row.first == paneRowWithCursor) rows[rowNdx++] = 1;
		}
		numPanes = rows;
	}
	// the panes from the key of the new one on move one key up, starting from the last one
	for (int k = editors.rbegin()->first; k >= newKey; k--) {
		auto nodeHolder = editors.extract(k);
		if (nodeHolder.empty()) continue;
		nodeHolder.key() = k + 1;
		editors.insert(std::move(nodeHolder));
		editors[k+1].setID(k+1);
	}
	editors[newKey] = Editor();
	editors[newKey].setID(newKey);
	setFontSize(false); // set the size for the font of the new editor but don't calculate the coordinates yet
	setPanesRowsAndCols();
	editors[session.whichPane].setActivity(false);
	// release the Ctrl key from the previous pane before moving to the new one
	editors[session.whichPane].setCtrlPressed(false);
	// for safety, set also the shift and alt keys to released
	editors[session.whichPane].setShiftPressed(false);
	editors[session.whichPane].setAltPressed(false);
	session.whichPane = newKey;
	editors[session.whichPane].setActivity(true);
	// if we add a second pane with the score visible
	// if the panes are split vertically and the score is also vertical (opposite values)
	// or both are horizontal
	// we need to swap the position of the score so the new pane becomes visible
	if (firstAddition && (showScore && paneSplitOrientation != scoreOrientation)) {
		// swap the score position without changing its orientation
		swapScorePosition(scoreOrientation);
		// with two panes split horizontally and the score shown horizontally
		// setPaneCoords() is called from swapScorePosition()
		// so we test for vertical spit to call it explicitly
		if (paneSplitOrientation == 1) {
			setPaneCoords();
		}
	}
	else {
		setPaneCoords();
	}
}

//--------------------------------------------------------------
// closes the active pane, or quits if it is the only one
// panes are laid out by the order of their keys in editors, row by row (or column by column with paneSplitOrientation 1)
// with numPanes holding how many panes every row has (see setPaneCoords()), so after removing a pane the keys of the panes after it
// go down by one, a row that is left empty is removed, and the row and column of every pane are set again from this layout
void ofApp::removePane()
{
	if (editors.size() <= 1) {
		requestQuit();
		return;
	}
	int paneRowWithCursor = editors[session.whichPane].getPaneRow();
	int lastKey = editors.rbegin()->first;
	bool isLastPane = (lastKey == session.whichPane);
	editors.erase(session.whichPane);
	// the panes after the removed one move one key down
	for (int key = session.whichPane + 1; key <= lastKey; key++) {
		auto nodeHolder = editors.extract(key);
		if (nodeHolder.empty()) continue;
		nodeHolder.key() = key - 1;
		editors.insert(std::move(nodeHolder));
		editors[key-1].setID(key-1);
	}
	// the row of the removed pane has one pane less, and if it is left empty it is removed, and the rows after it move one row up
	numPanes[paneRowWithCursor]--;
	if (numPanes[paneRowWithCursor] <= 0) {
		std::map<int, int> rows;
		int rowNdx = 0;
		for (const auto& row : numPanes) {
			if (row.second > 0) rows[rowNdx++] = row.second;
		}
		numPanes = rows;
	}
	setPanesRowsAndCols();
	// if we deleted the last pane we must decrement the pane index
	// otherwise it stays as it is, and the next pane becomes active
	if (isLastPane) session.whichPane--;
	if (session.whichPane < 0) session.whichPane = 0;
	if (session.whichPane > editors.rbegin()->first) session.whichPane = editors.rbegin()->first;
	editors[session.whichPane].setActivity(true);
	// if we're left with one pane only, reset paneSplitOrientation
	// (with more panes in one row or column the orientation stays, as it sets whether they are side by side or one above the other)
	if (editors.size() == 1) {
		paneSplitOrientation = 0;
		if ((scoreOrientation == 0 && scoreXOffset == 0) || (scoreOrientation == 1 && scoreYOffset == 0)) {
			swapScorePosition(scoreOrientation);
		}
	}
	fontLoaded = false;
	setFontSize(true);
}

//--------------------------------------------------------------
void ofApp::storeActiveEditorElement(int instNdx, int barNdx, int dataCounter, bool state)
{
	// this function is called by the sequencer thread when a note of an instrument starts (state is true) or stops
	// the editors draw the tokens of the notes that play (see getActiveEditorNoteSpans()), on the main thread
	std::lock_guard<std::mutex> lock(activeEditorNotesMutex);
	if (state) {
		activeEditorNotes[instNdx] = std::make_pair(barNdx, dataCounter);
		activeEditorRests.erase(instNdx);
	}
	else {
		auto it = activeEditorNotes.find(instNdx);
		if (it != activeEditorNotes.end() && it->second == std::make_pair(barNdx, dataCounter)) {
			activeEditorNotes.erase(it);
			activeEditorRests.erase(instNdx);
		}
	}
}

//--------------------------------------------------------------
// a rest stays highlighted for its whole duration, as it has no release, until the next note or rest of the instrument replaces it
void ofApp::storeActiveEditorRest(int instNdx, int barNdx, int dataCounter)
{
	std::lock_guard<std::mutex> lock(activeEditorNotesMutex);
	activeEditorNotes[instNdx] = std::make_pair(barNdx, dataCounter);
	activeEditorRests.insert(instNdx);
}


//--------------------------------------------------------------
void ofApp::clearActiveEditorElement(int instNdx)
{
	std::lock_guard<std::mutex> lock(activeEditorNotesMutex);
	activeEditorNotes.erase(instNdx);
	activeEditorRests.erase(instNdx);
}

//--------------------------------------------------------------
std::vector<EditorNoteSpan> ofApp::getActiveEditorNoteSpans(int paneNdx)
{
	// the tokens in a pane of the notes that play, for highlighting them
	std::vector<EditorNoteSpan> spans;
	if (!session.sequencer.isThreadRunning()) {
		std::lock_guard<std::mutex> lock(activeEditorNotesMutex);
		activeEditorNotes.clear();
		return spans;
	}
	std::map<int, std::pair<int, int>> active;
	std::set<int> rests;
	{
		std::lock_guard<std::mutex> lock(activeEditorNotesMutex);
		active = activeEditorNotes;
		rests = activeEditorRests;
	}
	// a rest is a token that starts with r (e.g. r4 or r8.), as no note name starts with r, or a - of the x - notation of rhythm staffs
	auto isRestToken = [](const std::string& text) { return !text.empty() && (text[0] == 'r' || text == "-"); };
	for (auto& note : active) {
		auto it = editorNoteSpans.find(std::make_pair(note.first, note.second.first));
		if (it == editorNoteSpans.end() || note.second.second < 0 || note.second.second >= (int)it->second.size()) continue;
		const std::vector<EditorNoteSpan>& noteSpans = it->second[note.second.second];
		// a rest is highlighted only if it is written as a rest, in the line or in a copied bar, also inside \transpose
		// (the tokens of \transpose and of the copied bar are highlighted together with it)
		// and not if a command makes it (e.g. \euclide, whose token is the whole command)
		if (rests.count(note.first)) {
			bool hasRestToken = false;
			bool written = true;
			for (const EditorNoteSpan& span : noteSpans) {
				if (isRestToken(span.text)) {
					hasRestToken = true;
				}
				else if (span.text != "\\transpose") {
					// a copied bar, e.g. \1 or \1.\inst
					std::string barName = span.text.substr(0, span.text.find('.'));
					if (session.sharedData.barsIndexes.find(barName) == session.sharedData.barsIndexes.end()) written = false;
				}
			}
			if (!written || !hasRestToken) continue;
			// only the rest is highlighted, in the bar where it is written, and not the token of the bar that copies it (e.g. \1)
			for (const EditorNoteSpan& span : noteSpans) {
				if (span.pane == paneNdx && isRestToken(span.text)) spans.push_back(span);
			}
			continue;
		}
		for (const EditorNoteSpan& span : noteSpans) {
			if (span.pane == paneNdx) spans.push_back(span);
		}
	}
	return spans;
}

/*************** arithmetic operations functions **************/



/********************* clearing functions *********************/





/************ command typing/executing functions **************/
//--------------------------------------------------------------
// completes the word that is typed last in the shell with the Tab key, as in a terminal
// the first word is a command, an editor command (see shellCommands in ofApp.h) or a program in one of the directories of PATH
// the other words are paths, absolute, starting with ~ (the home directory), or relative to the working directory
// with one match the word is completed (with a white space after it, unless it is a directory, which gets a slash)
// with more matches the first Tab completes the part they all start with, and the second one lists them
void ofApp::completeShellCommand()
{
	namespace fs = std::filesystem;
	shellLsOutputStr.clear();
	numShellLsOutputLines = 0;
	// any newlines are removed first (long commands are broken into lines only when they are drawn, see drawShell())
	std::string line = shellStr;
	line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
	if (line.empty() || line[0] != ':') return;
	size_t wordStart = line.find_last_of(' ');
	bool completingCommand = (wordStart == std::string::npos);
	wordStart = (completingCommand ? 1 : wordStart + 1);
	std::string word = line.substr(wordStart);
	// the part of the word that is completed, and the matches, which replace it
	std::string prefix;
	std::vector<std::string> matches;
	std::vector<bool> matchIsDir;
	std::error_code ec;
	// a command that is a path (e.g. :~/scripts/run.sh or :./run.sh) is completed as a path, as in a terminal
	if (completingCommand && (word.find('/') != std::string::npos || startsWith(word, "~") || startsWith(word, "."))) {
		completingCommand = false;
	}
	if (completingCommand) {
		prefix = word;
		std::set<std::string> names;
		for (const std::string& cmd : shellCommands) {
			if (cmd.size() > 1 && startsWith(cmd.substr(1), prefix)) names.insert(cmd.substr(1));
		}
		const char* pathEnv = getenv("PATH");
		std::vector<std::string> pathDirs = session.tokenizeString(pathEnv ? std::string(pathEnv) : std::string(), ":");
		for (const std::string& dir : pathDirs) {
			if (dir.empty()) continue;
			for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
				std::string name = it->path().filename().string();
				if (!startsWith(name, prefix)) continue;
				fs::file_status status = it->status(ec);
				if (ec || fs::is_directory(status)) continue;
				// only programs, that are executable
				if ((status.permissions() & (fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec)) == fs::perms::none) continue;
				names.insert(name);
			}
			ec.clear();
		}
		// the directories and the programs of the working directory, so that a path can be typed without ./ (e.g. :Documents/run.sh)
		std::set<std::string> dirs;
		for (fs::directory_iterator it(fs::current_path(ec), fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
			std::string name = it->path().filename().string();
			if (prefix.empty() || !startsWith(name, prefix) || (name[0] == '.' && !startsWith(prefix, "."))) continue;
			fs::file_status status = it->status(ec);
			if (ec) {
				ec.clear();
				continue;
			}
			if (fs::is_directory(status)) dirs.insert(name);
			else if ((status.permissions() & (fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec)) != fs::perms::none) names.insert(name);
		}
		ec.clear();
		for (const std::string& name : names) {
			if (dirs.find(name) != dirs.end()) continue;
			matches.push_back(name);
			matchIsDir.push_back(false);
		}
		for (const std::string& dir : dirs) {
			matches.push_back(dir);
			matchIsDir.push_back(true);
		}
	}
	else {
		// the directory of the path is the part up to its last slash, and the rest is the start of a name in it
		size_t slash = word.find_last_of('/');
		std::string dirPart = (slash == std::string::npos ? "" : word.substr(0, slash + 1));
		prefix = (slash == std::string::npos ? word : word.substr(slash + 1));
		std::string dir = dirPart;
		if (startsWith(dir, "~")) {
			const char* home = getenv("HOME");
			dir = (home ? std::string(home) : std::string()) + dir.substr(1);
		}
		else if (!startsWith(dir, "/")) {
			dir = fs::current_path(ec).string() + "/" + dir;
		}
		// only ~ without a slash completes to the home directory
		if (word == "~") {
			prefix = "";
			dirPart = "~";
			matches.push_back("/");
			matchIsDir.push_back(true);
		}
		else {
			std::vector<std::pair<std::string, bool>> entries;
			// :cd takes a directory, so only directories are completed
			bool onlyDirs = startsWith(line, ":cd ");
			for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
				std::string name = it->path().filename().string();
				// hidden files are listed only when their dot is typed
				if (!startsWith(name, prefix) || (name[0] == '.' && !startsWith(prefix, "."))) continue;
				bool isDir = it->is_directory(ec);
				if (onlyDirs && !isDir) continue;
				entries.push_back(std::make_pair(name, isDir));
			}
			std::sort(entries.begin(), entries.end());
			for (const auto& entry : entries) {
				matches.push_back(entry.first);
				matchIsDir.push_back(entry.second);
			}
		}
		wordStart += dirPart.size();
	}
	if (matches.empty()) return;
	std::string completion;
	if (matches.size() == 1) {
		completion = matches[0] + (matchIsDir[0] ? (endsWith(matches[0], "/") ? "" : "/") : " ");
	}
	else {
		if (shellTabCounter > 0) {
			// the second Tab lists the matches, with a slash after directories
			std::vector<std::string> listed;
			for (size_t i = 0; i < matches.size(); i++) listed.push_back(matches[i] + (matchIsDir[i] ? "/" : ""));
			formatShellLsOutput(listed);
			shellTabCounter++;
			return;
		}
		// the first Tab completes the part all matches start with
		completion = matches[0];
		for (size_t i = 1; i < matches.size(); i++) {
			size_t j = 0;
			while (j < completion.size() && j < matches[i].size() && completion[j] == matches[i][j]) j++;
			completion = completion.substr(0, j);
		}
	}
	shellStr = line.substr(0, wordStart) + completion;
	shellStrCursorPos = (int)shellStr.size();
	shellTabCounter++;
}

//--------------------------------------------------------------
void ofApp::typeShellCommand(int key)
{
	if (key == 13) {
		executeShellCommand();
		editors[session.whichPane].setTypingShell(false);
		shellTabCounter = 0;
		// a command without output (e.g. :w or :set wrap) leaves nothing to show
		// so the area below the panes shows the error messages again, which it doesn't while it shows the shell
		if (shellStr.empty() && numShellLsOutputLines == 0) {
			editors[session.whichPane].setShowingShell(false);
		}
	}
	else if (key == 8) {
		if (shellStrCursorPos > 0) {
			shellStr.erase(shellStrCursorPos-1, 1);
		}
		shellTabCounter = 0;
		shellStrCursorPos--;
		if (shellStrCursorPos < 0) {
			shellStrCursorPos = 0;
		}
		if (shellStr.empty()) {
			editors[session.whichPane].setTypingShell(false);
			editors[session.whichPane].setShowingShell(false);
		}
	}
	else if (key == 127) {
		if (shellStrCursorPos < (int)shellStr.size()) {
			shellStr.erase(shellStrCursorPos, 1);
		}
		if (shellStr.empty()) {
			editors[session.whichPane].setTypingShell(false);
			editors[session.whichPane].setShowingShell(false);
		}
	}
	else if (key == 105 && shellStr.size() == 0) {
		editors[session.whichPane].setTypingShell(false);
		editors[session.whichPane].setShowingShell(false);
		shellTabCounter = 0;
		editors[session.whichPane].resetMaxNumLines();
	}
	else if (key == 9) {
		completeShellCommand();
	}
	else {
		if (key == 58) {
			shellStr = "";
			shellLsOutputStr = "";
			shellStrCursorPos = 0;
			numShellLines = 1;
			shellStrColor = ofColor::white;
			numShellLsOutputLines = 0;
			editors[session.whichPane].setTypingShell(true);
			editors[session.whichPane].setShowingShell(true);
		}
		if (shellStrCursorPos < (int)shellStr.size()) shellStr.insert(shellStr.begin()+shellStrCursorPos, char(key));
		else shellStr += char(key);
		shellTabCounter = 0;
		shellStrCursorPos++;
	}
}

//--------------------------------------------------------------
std::string ofApp::wrapShellLines(const std::string& str, int& numLines)
{
	std::string wrapped;
	numLines = 0;
	size_t start = 0;
	while (start <= str.size()) {
		size_t end = str.find('\n', start);
		if (end == std::string::npos) end = str.size();
		// every line of the string, broken into rows of maxShellChars characters, with an empty line as one row
		size_t lineStart = start;
		do {
			size_t rowLength = std::min(maxShellChars, end - lineStart);
			if (numLines > 0) wrapped += '\n';
			wrapped += str.substr(lineStart, rowLength);
			numLines++;
			lineStart += rowLength;
		} while (lineStart < end);
		start = end + 1;
	}
	return wrapped;
}

//--------------------------------------------------------------
int ofApp::run_command(const std::string& cmd, std::string& output) {
	// Runs a shell command, stores combined STDOUT+STDERR in `output`.
	// Returns the command's exit code, or -1 if it could not be run.
	namespace fs = std::filesystem;

	std::random_device rd;
	std::stringstream name;
	name << "cmdout_" << std::hex << rd() << rd();
	fs::path tmp = fs::temp_directory_path() / name.str();

	std::string full = "(" + cmd + ") > \"" + tmp.string() + "\" 2>&1";
#ifdef _WIN32
	full = "\"" + full + "\"";   // cmd.exe /c quoting quirk
#endif

	int status = std::system(full.c_str());

	int code;
#ifdef _WIN32
	code = status;
#else
	code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif

	std::ifstream file(tmp, std::ios::binary);
	std::stringstream buffer;
	buffer << file.rdbuf();
	output = buffer.str();

	file.close();
	std::error_code ec;
	fs::remove(tmp, ec);         // ignore cleanup errors
	return code;
}

//--------------------------------------------------------------
void ofApp::executeShellCommand()
{
	if (executingBind) {
		std::vector<std::string> v = session.tokenizeString(shellStr, " ");
		shellStr.clear();
		executingBind = false;
		session.parseCommand(CmdInput(v, std::vector<std::string>{}, false, true), 1, 1); // dummy second and third arguments
		return;
	}
	shellStrColor = ofColor::white;
	// first extract the argument and expand it according to the symbols it uses
	// like the tilde character or a dot
	size_t whiteSpace = shellStr.find(" ");
	std::string arg;
	if (whiteSpace != std::string::npos || whiteSpace < shellStr.size()-1) {
		std::string shellStrLocal = shellStr.substr(whiteSpace+1);
		// remove newline characters from command
		shellStrLocal.erase(std::remove(shellStrLocal.begin(), shellStrLocal.end(), '\n'), shellStrLocal.end());
		// and the white spaces around the argument, as Tab completion adds a white space after a completed file name
		shellStrLocal.erase(0, shellStrLocal.find_first_not_of(" \t"));
		if (shellStrLocal.find_last_not_of(" \t") != std::string::npos) shellStrLocal.erase(shellStrLocal.find_last_not_of(" \t") + 1);
		else shellStrLocal.clear();
		if (startsWith(shellStrLocal, "~")) {
			arg = getenv("HOME") + shellStrLocal.substr(1);
		}
		else if (startsWith(shellStrLocal, "/")) {
			arg = shellStrLocal;
		}
		else {
			if (startsWith(shellStr, ":bind") || startsWith(shellStr, ":unbind")) {
				arg = shellStrLocal;
			}
			else {
				std::string pwdStr = systemCommandOutput("pwd");
				// there's a newline character at the end of systemCommandOutput("pwd"), so we remove it below
				arg = pwdStr.substr(0, pwdStr.size()-1) + "/";
				if (startsWith(shellStrLocal, "./")) {
					arg += shellStrLocal.substr(2);
				}
				else {
					arg += shellStrLocal;
				}
			}
		}
		// paths like ../bin/data/file.lyv are tidied to /full/path/bin/data/file.lyv, which is also shown as the path of a loaded file
		if (!arg.empty() && startsWith(arg, "/") && !(startsWith(shellStr, ":bind") || startsWith(shellStr, ":unbind"))) {
			std::string normalized = std::filesystem::path(arg).lexically_normal().string();
			// a directory keeps no trailing slash, except the root directory
			if (normalized.size() > 1 && endsWith(normalized, "/")) normalized.pop_back();
			arg = normalized;
		}
	}
	// then check which command we're calling
	if (shellStr == ":save" || startsWith(shellStr, ":save ")) {
		if (shellStr.compare(":save") == 0) {
			shellStr = "save command called without an argument";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		if (!startsWith(shellStr, ":save ")) {
			shellStr = shellStr + ": not an editor command";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		if (arg.empty()) {
			shellStr = "save command called without an argument";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		editors[session.whichPane].saveFile(arg);
		shellStr.clear();
		numShellLines = 0;
	}
	else if (shellStr.compare(":w") == 0) {
		editors[session.whichPane].saveExistingFile();
		shellStr.clear();
		numShellLines = 0;
	}
	// :wq saves the file of the active pane and closes the pane, or quits if it is the only one, as :q does
	// a file that has no name yet is saved to the path typed after it (e.g. :wq ~/music/piece.lyv)
	// :x does the same, as in Vim, but writes the file only if it has changes that are not saved
	else if (shellStr == ":wq" || startsWith(shellStr, ":wq ") || shellStr == ":x" || startsWith(shellStr, ":x ")) {
		bool onlyIfEdited = startsWith(shellStr, ":x");
		Editor& editor = editors[session.whichPane];
		if (editor.isHelpFileOpen()) {
			// a help file is not saved, it is closed
			editor.closeHelpFile();
			shellStr.clear();
			numShellLines = 0;
			return;
		}
		bool hasPath = (whiteSpace != std::string::npos && shellStr.find_first_not_of(" \t\n", whiteSpace) != std::string::npos && !arg.empty());
		if (onlyIfEdited && !hasPath && !editor.isFileEdited()) {
			// nothing to save
		}
		else if (hasPath) editor.saveFile(arg);
		else if (editor.fileLoaded) editor.saveExistingFile();
		else {
			shellStr = "no file name, type the path after " + std::string(onlyIfEdited ? ":x" : ":wq");
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		// the pane is closed only if its file has been saved
		if (editor.isFileEdited()) {
			shellStr = "could not save " + editor.getFileName();
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		shellStr.clear();
		numShellLines = 0;
		editors[session.whichPane].setTypingShell(false);
		editors[session.whichPane].setShowingShell(false);
		removePane();
	}
	// :wa saves the files of all panes that have changes that are not saved, and :wqa saves them and quits
	// files without a name can't be saved this way, so they are listed, and :wqa asks for their paths (see requestQuit())
	// :xa is the same as :wqa, as in Vim
	else if (shellStr.compare(":wa") == 0 || shellStr.compare(":wqa") == 0 || shellStr.compare(":xa") == 0) {
		bool quitting = (shellStr.compare(":wqa") == 0 || shellStr.compare(":xa") == 0);
		std::vector<std::string> notSaved;
		for (auto& editor : editors) {
			if (!editor.second.isFileEdited() || editor.second.isHelpFileOpen()) continue;
			if (editor.second.fileLoaded) editor.second.saveExistingFile();
			if (editor.second.isFileEdited()) notSaved.push_back(editor.second.getFileName() + " (pane " + std::to_string(editor.first + 1) + ")");
		}
		shellStr.clear();
		numShellLines = 0;
		if (quitting) {
			// the files that are left unsaved are asked for, with their paths if they have no name
			requestQuit();
			return;
		}
		if (!notSaved.empty()) {
			std::string list;
			for (size_t i = 0; i < notSaved.size(); i++) list += (i > 0 ? ", " : "") + notSaved[i];
			shellStr = "not saved: " + list + ", save them with :save";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			}
	}

	else if (shellStr == ":load" || startsWith(shellStr, ":load ")) {
		if (shellStr.compare(":load") == 0) {
			shellStr = "load command called without an argument";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		if (!startsWith(shellStr, ":load ")) {
			shellStr = shellStr + ": not an editor command";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		if (arg.empty()) {
			shellStr = "load command called without an argument";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		editors[session.whichPane].loadFile(arg);
		shellStr.clear();
		numShellLines = 0;
	}
	else if (startsWith(shellStr, ":bindings")) {
		if (shellStr.size() > 9) {
			shellStr = shellStr + ": not an editor command";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		if (!arg.empty()) {
			shellStr = "bindings command takes no arguments";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		shellStr.clear();
		int counter = 0;
		for (auto it = editors[session.whichPane].bindings.begin(); it != editors[session.whichPane].bindings.end(); ++it) {
			if (counter > 0) shellStr += "\n";
			counter++;
			shellStr += {static_cast<char>(it->first)};
			if (!it->second.bindingVec.empty()) shellStr += session.genStrFromVec(it->second.bindingVec);
			shellStr += " -> ";
			shellStr += session.genStrFromVec(it->second.actionVec);
		}
		numShellLines = editors[session.whichPane].bindings.size();
	}
	else if (shellStr == ":bind" || startsWith(shellStr, ":bind ")) {
		if (whiteSpace == std::string::npos && shellStr.size() > 5) {
			shellStr = shellStr + ": not an editor command";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		if (arg.empty()) {
			shellStr = "bind command called without an argument";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		std::vector<std::string> v = session.tokenizeString(arg, " ");
		if (std::find(v.begin(), v.end(), "->") == v.end()) {
			shellStr = "wrong formatting for bind";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		bool foundArrow = false;
		std::vector<std::string> v1;
		std::vector<std::string> v2;
		for (std::string s : v) {
			if (s.compare("->") == 0) {
				if (v1.empty()) {
					shellStr = "wrong formatting for bind";
					shellStrColor = ofColor::red;
					numShellLines = 1;
					return;
				}
				foundArrow = true;
			}
			else {
				if (!foundArrow) {
					v1.push_back(s);
				}
				else {
					v2.push_back(s);
				}
			}
		}
		if (v1[0].size() > 1) {
			shellStr = "binding key can be one character only";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		int key = static_cast<int>(v1[0][0]);
		// the keys of normal mode are reserved and can't be bound
		static const std::map<int, std::string> normalModeKeys = {
			{':', "types commands"}, {'i', "inserts text"},
			{'h', "moves the cursor left"}, {'H', "moves the cursor left"},
			{'j', "moves the cursor down"}, {'J', "moves the cursor down"},
			{'k', "moves the cursor up"}, {'K', "moves the cursor up"},
			{'l', "moves the cursor right"}, {'L', "moves the cursor right"},
			{'u', "undoes"}, {'y', "yanks"}, {'p', "pastes"}, {'v', "starts visual mode"},
			{'A', "inserts at the end of the line"}, {'d', "deletes"},
			{'0', "moves the cursor to the start of the line"}, {'$', "moves the cursor to the end of the line"},
			{'D', "deletes to the end of the line"}, {'V', "selects whole lines"}, {'I', "inserts at the start of the line"},
			{'x', "deletes the character under the cursor"},
			{'g', "moves the cursor to the first line when pressed twice"}, {'G', "moves the cursor to the last line"}
		};
		auto normalIt = normalModeKeys.find(key);
		if (normalIt != normalModeKeys.end()) {
			shellStr = v1[0] + " is reserved, it " + normalIt->second + " in normal mode";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		if (editors[session.whichPane].bindings.find(key) != editors[session.whichPane].bindings.end()) {
			shellStr = v1[0] + " is already bound";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		editors[session.whichPane].bindings[key] = {v1.size() > 1 ? std::vector<std::string>(v1.begin()+1, v1.end()) : std::vector<std::string>{""}, std::move(v2), false};
		shellStr.clear();
		shellStrColor = ofColor::white;
		return;
	}
	else if (shellStr == ":unbind" || startsWith(shellStr, ":unbind ")) {
		if (whiteSpace == std::string::npos && shellStr.size() > 7) {
			shellStr = shellStr + ": not an editor command";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		if (arg.empty()) {
			shellStr = "unbind command called without an argument";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		std::vector<std::string> v = session.tokenizeString(arg, " ");
		int key = static_cast<int>(v[0][0]);
		auto it = editors[session.whichPane].bindings.find(key);
		if (it == editors[session.whichPane].bindings.end()) {
			shellStr = v[0] + " is not bound";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		editors[session.whichPane].bindings.erase(it);
		shellStr.clear();
		shellStrColor = ofColor::white;
		return;
	}
	else if (shellStr == ":ls" || startsWith(shellStr, ":ls ")) {
		if (whiteSpace == std::string::npos && shellStr.size() > 3) {
			shellStr = shellStr + ": not an editor command";
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		// with options (e.g. :ls -a), the arguments are passed to ls as they are typed
		std::string rawArgs = (whiteSpace == std::string::npos ? std::string() : shellStr.substr(whiteSpace+1));
		rawArgs.erase(std::remove(rawArgs.begin(), rawArgs.end(), '\n'), rawArgs.end());
		std::string lsOutput = systemCommandOutput("ls -p " + (startsWith(rawArgs, "-") ? rawArgs : arg));
		shellStr.clear();
		numShellLines = 0;
		formatShellLsOutput(session.tokenizeString(lsOutput, "\n"));
	}
	else if (shellStr.compare(":pwd") == 0) {
		shellStr = systemCommandOutput("pwd");
		numShellLines = 1;
	}
	// :cd changes the working directory of LiveLily, as in Vim, which :pwd, :ls, :load, :save, other shell commands, and Tab completion use
	// :cd without an argument goes to the home directory, and :cd - to the previous working directory
	else if (shellStr == ":cd" || startsWith(shellStr, ":cd ")) {
		namespace fs = std::filesystem;
		std::string rawArg = (whiteSpace == std::string::npos ? std::string() : shellStr.substr(whiteSpace+1));
		rawArg.erase(std::remove(rawArg.begin(), rawArg.end(), '\n'), rawArg.end());
		rawArg.erase(0, rawArg.find_first_not_of(" \t"));
		if (rawArg.find_last_not_of(" \t") != std::string::npos) rawArg.erase(rawArg.find_last_not_of(" \t") + 1);
		std::error_code ec;
		std::string target;
		if (rawArg.empty()) {
			const char* home = getenv("HOME");
			target = (home ? std::string(home) : std::string());
		}
		else if (rawArg == "-") {
			if (prevWorkingDir.empty()) {
				shellStr = "no previous directory";
				shellStrColor = ofColor::red;
				numShellLines = 1;
				return;
			}
			target = prevWorkingDir;
		}
		else {
			// the argument with ~ expanded, or relative to the working directory (see above)
			target = arg;
			target.erase(target.find_last_not_of(" \t") + 1);
		}
		std::string currentDir = fs::current_path(ec).string();
		if (target.empty() || !fs::is_directory(target, ec)) {
			shellStr = (rawArg.empty() ? std::string("no home directory") : rawArg + ": no such directory");
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		fs::current_path(target, ec);
		if (ec) {
			shellStr = rawArg + ": " + ec.message();
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		prevWorkingDir = currentDir;
		shellStr.clear();
		numShellLines = 0;
	}
	else if (shellStr.compare(":q") == 0) {
		if (editors[session.whichPane].isHelpFileOpen()) {
			editors[session.whichPane].closeHelpFile();
			// the command leaves nothing to show, so the area below the panes shows the error messages again (see typeShellCommand())
			shellStr.clear();
			numShellLines = 0;
		}
		else {
			removePane();
		}
	}
	// :vs (or :vsplit) adds a pane side by side with the active one, and :sp (or :split) one below it, as in Vim
	// with a file as an argument, the file is opened in the new pane
	else if (shellStr == ":vs" || shellStr == ":vsplit" || shellStr == ":sp" || shellStr == ":split" ||
			startsWith(shellStr, ":vs ") || startsWith(shellStr, ":vsplit ") || startsWith(shellStr, ":sp ") || startsWith(shellStr, ":split ")) {
		bool vertical = startsWith(shellStr, ":vs");
		bool hasFile = (whiteSpace != std::string::npos && !arg.empty() && shellStr.find_first_not_of(" \t\n", whiteSpace) != std::string::npos);
		// the shell of this pane closes, as the new pane becomes the active one
		shellStr.clear();
		numShellLines = 0;
		editors[session.whichPane].setTypingShell(false);
		editors[session.whichPane].setShowingShell(false);
		addPane(vertical ? 86 : 72);
		// the new pane starts in normal mode, as a pane does in Vim
		editors[session.whichPane].setInserting(false);
		if (hasFile) editors[session.whichPane].loadFile(arg);
	}
	else if (shellStr.compare(":qa") == 0) {
		requestQuit();
	}
	// :q! and :qa! quit without asking to save the files that have changes that are not saved, as in Vim
	// :q! closes the pane when there are more panes (or the help file, if one is open), as :q does
	else if (shellStr.compare(":q!") == 0) {
		if (editors[session.whichPane].isHelpFileOpen()) {
			editors[session.whichPane].closeHelpFile();
			// the command leaves nothing to show, so the area below the panes shows the error messages again (see typeShellCommand())
			shellStr.clear();
			numShellLines = 0;
		}
		// the same test removePane() uses to quit instead of closing a pane
		else if (editors.size() <= 1) {
			exit();
		}
		else {
			removePane();
		}
	}
	else if (shellStr.compare(":qa!") == 0) {
		exit();
	}
	// editor options, as in Vim, for the active pane
	// :set wrap wraps long lines around the edge of the pane, and :set wrap! (or :set nowrap) scrolls them, which is the default
	else if (startsWith(shellStr, ":set ") || shellStr.compare(":set") == 0) {
		std::string option = (shellStr.size() > 5 ? shellStr.substr(5) : "");
		option.erase(0, option.find_first_not_of(" \t\n"));
		if (option.find_last_not_of(" \t\n") != std::string::npos) option.erase(option.find_last_not_of(" \t\n") + 1);
		if (option == "wrap") {
			editors[session.whichPane].setWrapLines(true);
		}
		else if (option == "wrap!" || option == "nowrap") {
			editors[session.whichPane].setWrapLines(false);
		}
		else {
			shellStr = (option.empty() ? std::string(":set needs an option") : option + ": unknown option");
			shellStrColor = ofColor::red;
			numShellLines = 1;
			return;
		}
		shellStr.clear();
		numShellLines = 0;
	}
	else {
		// any other command is a shell command, typed after the colon (e.g. :ls -l or :git status)
		// its output and its error messages are shown below the panes
		std::string cmd = shellStr.substr(1);
		cmd.erase(std::remove(cmd.begin(), cmd.end(), '\n'), cmd.end());
		std::string output;
		int err = run_command(cmd, output);
		if (err && output.empty()) output = cmd + ": failed with exit code " + std::to_string(err);
		setShellOutput(output, err != 0);
	}
}

//--------------------------------------------------------------
// shows the output of a shell command below the panes, with its lines broken to fit the width of the window
// the panes are made shorter for long output, and what doesn't fit scrolls with the arrow keys (see drawShell())
void ofApp::setShellOutput(const std::string& output, bool error)
{
	shellStrColor = (error ? ofColor::red : ofColor::white);
	std::vector<std::string> lines;
	std::string line;
	for (char c : output) {
		if (c == '\n') {
			lines.push_back(line);
			line.clear();
		}
		else if (c == '\t') {
			// tabs go to the next multiple of eight characters, as in a terminal
			line += std::string(8 - (line.size() % 8), ' ');
		}
		else if (c != '\r') {
			line += c;
		}
	}
	if (!line.empty()) lines.push_back(line);
	// break long lines to fit the width of the window
	std::vector<std::string> rows;
	size_t width = std::max((size_t)1, maxShellChars);
	for (const std::string& l : lines) {
		if (l.empty()) rows.push_back("");
		for (size_t i = 0; i < l.size(); i += width) rows.push_back(l.substr(i, width));
	}
	// a command without output (e.g. :touch file) leaves nothing to show, so the shell is hidden (see typeShellCommand())
	if (rows.empty()) {
		shellStr.clear();
		numShellLines = 0;
		return;
	}
	// all of the output is kept, and what doesn't fit the area below the panes scrolls with the arrow keys (see drawShell())
	shellScroll = 0;
	shellStr.clear();
	for (size_t i = 0; i < rows.size(); i++) {
		if (i > 0) shellStr += "\n";
		shellStr += rows[i];
	}
	numShellLines = (int)rows.size();
}

//--------------------------------------------------------------
void ofApp::formatShellLsOutput(std::vector<std::string> lsOutputTokens)
{
	if (lsOutputTokens.size() == 0) return;
	// first find the longest string
	size_t maxStrLength = 1;
	for (const std::string& s : lsOutputTokens) {
		if (s.size() > maxStrLength) maxStrLength = s.size();
	}
	// then determine how many columns fit in the width of the window, with two white spaces between them
	// the last column doesn't need the two white spaces, so they are added to the width of the window
	int numColumns = std::max(1, (int)((maxShellChars + 2) / (maxStrLength + 2)));
	// the strings are listed in columns, from top to bottom and then from left to right, as ls does in a terminal
	int numTokens = (int)lsOutputTokens.size();
	int numRows = (numTokens + numColumns - 1) / numColumns;
	// with the number of rows set, fewer columns might be enough (e.g. 5 strings in 4 columns need 2 rows, which need 3 columns only)
	numColumns = (numTokens + numRows - 1) / numRows;
	shellLsOutputStr.clear();
	shellScroll = 0;
	for (int row = 0; row < numRows; row++) {
		if (row > 0) shellLsOutputStr += "\n";
		for (int col = 0; col < numColumns; col++) {
			int ndx = (col * numRows) + row;
			if (ndx >= numTokens) break;
			const std::string& s = lsOutputTokens[ndx];
			shellLsOutputStr += s;
			// pad all but the last string of the row, to the start of the next column
			if (col < numColumns - 1 && ((col + 1) * numRows) + row < numTokens) {
				shellLsOutputStr += std::string((maxStrLength + 2) - s.size(), ' ');
			}
		}
	}
	numShellLsOutputLines = numRows;
}

/****************** miscellaneous functions ********************/

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

//--------------------------------------------------------------
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


/****************** OSC to parts functions ********************/

//--------------------------------------------------------------
void ofApp::sendBeatVizInfo(int bar)
{
	// calculate the time stamp regardless of whether we're showing the beat
	// or not in the editor, in case we have connected score parts
	view.beatVizTimeStamp = ofGetElapsedTimeMillis();
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
	setPaneCoords();
	editors[session.whichPane].setMaxCharactersPerString();
}


//--------------------------------------------------------------
bool ofApp::isScoreVisible()
{
	return showScore;
}

/********************* instrument functions *******************/




/*********** coordinates/positions/sizes functions ************/
//--------------------------------------------------------------
void ofApp::setFontSize(bool calculateCoords)
{
	if (!fontLoaded) {
		font.load("DroidSansMono.ttf", fontSize);
		fontLoaded = true;
		oneCharacterWidth = font.stringWidth("a");
		halfCharacterWidth = oneCharacterWidth / 2;
		cursorHeight = font.stringHeight("q");
		float heightDiff = font.stringHeight("l") - font.stringHeight("o");
		cursorHeight += heightDiff;
		maxShellChars = (size_t)(view.screenWidth / oneCharacterWidth);
	}
	for (std::map<int, Editor>::iterator it = editors.begin(); it != editors.end(); ++it) {
		it->second.setFontSize(fontSize, oneCharacterWidth, cursorHeight);
	}
	if (calculateCoords) setPaneCoords();
}

//--------------------------------------------------------------
void ofApp::setPaneCoords()
{
	// this is the minimum height of the traceback space, used to calculate the size of the panes
	// MINTRACEBACKLINES lines of the current font, with their baselines one and two lines below the panes (see drawTraceback() and drawShell())
	// and the part of the last line below its baseline, as low as the cursor reaches (0.15 of a line, see drawShell())
	// the panes get a whole number of lines in the rest of the window, so the area below them is at least this high
	view.tracebackBase = (float)view.screenHeight - (cursorHeight * ((float)MINTRACEBACKLINES + 0.15f));
	// we need the anchor so we can change the traceback height in case the traceback
	// has more than two lines
	// get the width and height for each pane
	// initializing 0 to avoid warning messages during compilation
	float paneWidth = 0, paneHeight = 0;
	// we compute the maximum number of lines for a pane with the height of the window
	int maxNumLines = view.tracebackBase / cursorHeight;
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
		paneWidth = (float)view.screenWidth / (float)numPanes.size();
	}
	int paneNdx = 0;
	for (std::map<int, int>::iterator it = numPanes.begin(); it != numPanes.end(); ++it) {
		// each row sets the number of lines for its panes according to the number of rows (it->first) for paneSplitOrientation == 0
		// or number of columns (it->second) for paneSplitOrientation == 1
		bool calcAdditionalLines = true;
		int numLinesDivisor = 1;
		int widthDivisor = 1;
		// if we are showing the score and we have only one pane or two panes
		if (isScoreVisible() && ((numPanes.size() == 1 && numPanes[0] == 1) || (numPanes.size() == 2 && (numPanes[0] == 1 && numPanes[1] == 1)))) {
			if (numPanes.size() == 1) {
				if (scoreOrientation == 0) {
					widthDivisor = 2;
				}
				else {
					numLinesDivisor = 2;
				}
			}
			else {
				if (paneSplitOrientation == 1) {
					if (scoreOrientation == 1) {
						numLinesDivisor = 2;
						widthDivisor = it->second;
					}
					else {
						numLinesDivisor = it->second;
						widthDivisor = 2;
					}
				}
				else {
					if (scoreOrientation == 0) {
						numLinesDivisor = widthDivisor = 2;
					}
					else {
						numLinesDivisor = 2;
						widthDivisor = it->second;
					}
				}
			}
			calcAdditionalLines = false;
		}
		else {
			numLinesDivisor = paneSplitOrientation == 0 ? (int)numPanes.size() : it->second;
			widthDivisor = it->second;
		}
		int numLines = maxNumLines / (numLinesDivisor > 0 ? numLinesDivisor : 1);
		int additionalLines = 0;
		int numLinesMultiplier = paneSplitOrientation == 0 ? (int)numPanes.size() : it->second;
		if (calcAdditionalLines && numLines * numLinesMultiplier != maxNumLines) {
			additionalLines = maxNumLines - (numLines * numLinesMultiplier);
		}
		if (paneSplitOrientation == 0) {
			paneWidth = view.screenWidth / widthDivisor;
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
	//for (std::map<int, Editor>::iterator it = editors.begin(); it != editors.end(); ++it) {
	//	maxNumLines = max(maxNumLines, it->second.getMaxNumLines());
	//}
	view.tracebackBase = (float)maxNumLines * cursorHeight;
	view.tracebackYCoord = view.tracebackBase;
	// the panes have a new layout, which the shell makes shorter again if it needs to (see layoutPanesForShell())
	paneBase.clear();
	// the area below the panes is reset, so its size is calculated again in drawTraceback() and drawShell()
	// for the lines that are shown in it, which are broken again to fit the width of the window (e.g. after it is resized)
	tracebackNumLinesShown = shellNumLinesShown = -1;
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
	if (session.whichPane != activePane && editors.find(activePane) != editors.end()) setActivePane(activePane);
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
	float fullHeight = view.tracebackBase - cursorHeight;
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
				scoreBackgroundHeight = view.tracebackBase;
			}
			else {
				scoreBackgroundHeight = view.tracebackBase - cursorHeight;
			}
			view.pianoRollKeysWidth = scoreBackgroundWidth / KEYSWIDTHCOEFF;
			view.pianoRollKeysHeight = scoreBackgroundHeight / view.pianoRollLength;
			view.pianoRollMinDur = (scoreBackgroundWidth - view.pianoRollKeysWidth) / (float)MINDUR;
			if (scoreXOffset == 0) scoreXOffset = view.screenWidth / 2;
			else scoreXOffset = 0;
			// check if the editor is split in panes and change the active pane accordingly
			if (paneSplitOrientation == 1 && numPanes.size() == 2) {
				if (scoreXOffset == 0 && session.whichPane < numPanes[0]) {
					setActivePane(numPanes[0]);
				}
				else if (scoreXOffset > 0 && session.whichPane >= numPanes[0]) {
					setActivePane(0);
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
			if (setCoords) setPaneCoords();
			break;
		case 1:
			scoreXOffset = 0;
			scoreBackgroundWidth = view.screenWidth;
			// if we have one pane only or two panes in vertical split
			if ((numPanes.size() == 1 && numPanes[0] == 1) || (paneSplitOrientation == 1 && (numPanes.size() == 2 && (numPanes[0] == 1 && numPanes[1] == 1)))) {
				// we first calculate the panes coordinates because in case we have one pane only
				// or two panes split vertically, we want to reduce their number of lines
				// so the horizontal score can nicely fit in the window without obscuring the panes
				setPaneCoords();
				// then we set the space between the end of the pane(s) and the beginning of the traceback as the height of the score
				// in this pane configuration we don't mind which pane index we'll use, so the first is the safest
				// getMaxNumLines() returns the number of lines minus 1 for the pane separator
				// but in this case we need this additional line for the score height so we add it
				int numLines = editors[0].getMaxNumLines() + 1;
				scoreBackgroundHeight = view.tracebackBase - (numLines * cursorHeight);
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
				if (setCoords) setPaneCoords();
			}
			else {
				scoreBackgroundHeight = (view.tracebackBase / 2) - cursorHeight;
				// in horizontal view of the score, the height of the score background is rounded half the editor lines
				// this is because the number of lines of an editor is always odd, so that the line with the file name
				// and pane index is also included, which makes the total number of lines even
				// when we view the score horizontally, the background should match an exact number of lines
				// and since the number of total lines is odd, we make this background one less
				// e.g. for 37 lines, the score background will be 18 lines, not 19, or 18.5
				// but when viewed on the lower side, it should touch the white line of the editor
				// hence the scoreYOffset is not the same as the scoreBackgroundHeith
				// as is the case with scoreXOffset and scoreBarckgroundWidth, in the case above
				if (scoreYOffset == 0) scoreYOffset = (view.tracebackBase / 2);
				else scoreYOffset = 0;
				// check if the editor is split in panes and change the active pane accordingly
				if (paneSplitOrientation == 0 && numPanes.size() == 2) {
					if (scoreYOffset == 0 && session.whichPane < numPanes[0]) {
						setActivePane(numPanes[0]);
					}
					else if (scoreYOffset > 0 && session.whichPane >= numPanes[0]) {
						setActivePane(0);
					}
				}
			}
			view.pianoRollKeysWidth = scoreBackgroundWidth / view.pianoRollLength;
			view.pianoRollKeysHeight = scoreBackgroundHeight / KEYSWIDTHCOEFF;
			view.pianoRollMinDur = (scoreBackgroundHeight - view.pianoRollKeysHeight) / (float)MINDUR;
			break;
		case 2:
			scoreXOffset = 0;
			scoreYOffset = 0;
			scoreBackgroundWidth = view.screenWidth;
			scoreBackgroundHeight = view.tracebackBase - cursorHeight;
			view.pianoRollKeysWidth = scoreBackgroundWidth / KEYSWIDTHCOEFF;
			view.pianoRollKeysHeight = scoreBackgroundHeight / view.pianoRollLength;
			view.pianoRollMinDur = (scoreBackgroundWidth - view.pianoRollKeysWidth) / (float)MINDUR;
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
void ofApp::setActivePane(int activePane)
{
	editors[session.whichPane].setActivity(false);
	// set all hot keys to false
	editors[session.whichPane].setShiftPressed(false);
	editors[session.whichPane].setCtrlPressed(false);
	editors[session.whichPane].setAltPressed(false);
	if (editors[session.whichPane].highlightManyChars) editors[session.whichPane].highlightManyChars = false;
	session.whichPane = activePane;
	editors[session.whichPane].setActivity(true);
}

//--------------------------------------------------------------
void ofApp::requestQuit()
{
	quitPromptPanes.clear();
	for (auto& editor : editors) {
		// a help file is not saved, it replaces the text of its pane only while it is open
		if (editor.second.isFileEdited() && !editor.second.isHelpFileOpen()) quitPromptPanes.push_back(editor.first);
	}
	quitPromptNext();
}

//--------------------------------------------------------------
void ofApp::quitPromptNext()
{
	if (quitPromptPanes.empty()) {
		quitPromptState = 0;
		exit();
		return;
	}
	int pane = quitPromptPanes.front();
	// quitting can be called while typing in the shell of another pane (with :qa), which stops here
	for (auto& editor : editors) {
		editor.second.setTypingShell(false);
		editor.second.setShowingShell(false);
	}
	// the pane that is asked for becomes the active one, so it can be seen which file it is
	if (pane != session.whichPane && editors.find(pane) != editors.end()) setActivePane(pane);
	quitPromptState = 1;
	shellStr = "save " + editors[session.whichPane].getFileName() + "? y/n";
	shellStrColor = ofColor::yellow;
	shellStrCursorPos = (int)shellStr.size();
	shellLsOutputStr.clear();
	numShellLsOutputLines = 0;
	numShellLines = 1;
	editors[session.whichPane].setTypingShell(false);
	editors[session.whichPane].setShowingShell(true);
}

//--------------------------------------------------------------
void ofApp::quitPromptKey(int key)
{
	// Esc cancels quitting
	if (key == 27) {
		quitPromptState = 0;
		quitPromptPanes.clear();
		shellStr.clear();
		shellLsOutputStr.clear();
		numShellLines = numShellLsOutputLines = 0;
		editors[session.whichPane].setTypingShell(false);
		editors[session.whichPane].setShowingShell(false);
		return;
	}
	if (quitPromptState == 1) {
		if (key == 'n' || key == 'N') {
			quitPromptPanes.erase(quitPromptPanes.begin());
			quitPromptNext();
		}
		else if (key == 'y' || key == 'Y') {
			// a file that has a path is saved to it, otherwise (or if it can't be saved there) the path is asked for
			if (editors[session.whichPane].fileLoaded) editors[session.whichPane].saveExistingFile();
			if (!editors[session.whichPane].isFileEdited()) {
				quitPromptPanes.erase(quitPromptPanes.begin());
				quitPromptNext();
				return;
			}
			// the path is typed after :save, as when saving with :save, so Tab completion works (see completeShellCommand())
			quitPromptState = 2;
			shellStr = ":save ";
			shellStrColor = ofColor::white;
			shellStrCursorPos = (int)shellStr.size();
			shellTabCounter = 0;
			numShellLines = 1;
			editors[session.whichPane].setTypingShell(true);
			editors[session.whichPane].setShowingShell(true);
		}
		return;
	}
	// typing the path
	if (key == 13) {
		std::string path = shellStr.substr(std::min(shellStr.size(), (size_t)6));
		path.erase(std::remove(path.begin(), path.end(), '\n'), path.end());
		if (path.find_first_not_of(" \t") == std::string::npos) return;
		// :save expands ~ and relative paths, and saves the file
		executeShellCommand();
		if (!editors[session.whichPane].isFileEdited()) {
			quitPromptPanes.erase(quitPromptPanes.begin());
			quitPromptNext();
		}
		else {
			// the file couldn't be saved there (e.g. a directory that doesn't exist), so the path is asked for again
			shellStr = ":save ";
			shellStrColor = ofColor::white;
			shellStrCursorPos = (int)shellStr.size();
			shellLsOutputStr = "could not save " + editors[session.whichPane].getFileName() + " there, type another path, or Esc to cancel quitting";
			numShellLsOutputLines = 1;
			numShellLines = 1;
			editors[session.whichPane].setTypingShell(true);
			editors[session.whichPane].setShowingShell(true);
		}
		return;
	}
	// the :save at the start of the line can't be deleted, and : would start a new command
	if ((key == 8 && shellStrCursorPos <= 6) || (key == 127 && shellStrCursorPos < 6) || key == ':') return;
	typeShellCommand(key);
	// the text typed so far keeps the cursor after :save
	if (shellStrCursorPos < 6) shellStrCursorPos = 6;
}

//--------------------------------------------------------------
void ofApp::exit()
{
	// should print a warning message first
	session.sequencer.stopNow();
	// the sequencer's thread calls the app (e.g. to highlight the notes that play), and it is held by Session
	// which is destroyed after the members of the app, so the thread ends here
	session.sequencer.waitForThread(false);
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
	setPaneCoords();
	if (scoreOrientation == 0) {
		scoreBackgroundWidth = view.screenWidth / 2;
		scoreBackgroundHeight = view.tracebackBase - cursorHeight;
		view.pianoRollKeysWidth = scoreBackgroundWidth / KEYSWIDTHCOEFF;
		view.pianoRollKeysHeight = scoreBackgroundHeight / view.pianoRollLength;
		view.pianoRollMinDur = (scoreBackgroundWidth - view.pianoRollKeysWidth) / (float)MINDUR;
		if (scoreXOffset > 0) scoreXOffset = view.screenWidth / 2;
	}
	else {
		scoreBackgroundWidth = view.screenWidth;
		scoreBackgroundHeight = (view.tracebackBase / 2) - cursorHeight;
		view.pianoRollKeysWidth = scoreBackgroundWidth / view.pianoRollLength;
		view.pianoRollKeysHeight = scoreBackgroundHeight / KEYSWIDTHCOEFF;
		view.pianoRollMinDur = (scoreBackgroundHeight - view.pianoRollKeysHeight) / (float)MINDUR;
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
void ofApp::parseStrings(int index, int numLines)
{
	session.barError = false;
	// an unmatched curly bracket (e.g. a bar definition that is not finished) is an error
	// and in this case none of the lines is parsed
	std::string bracketError;
	int unmatchedBracketLine = editors[session.whichPane].findUnmatchedBracket(index, numLines, bracketError);
	if (unmatchedBracketLine > -1) {
		editors[session.whichPane].setTraceback(3, bracketError, unmatchedBracketLine);
		return;
	}
	// collect the lines to parse from the editor
	std::vector<std::pair<int, std::string>> lines;
	for (int i = index; i < (index+numLines); i++) {
		if (editors[session.whichPane].getSessionActivity()) {
			lines.push_back(std::make_pair(i, editors[session.whichPane].allLines[i].str));
			if (editors[session.whichPane].getSendLines()) {
				OscMessage m;
				m.setAddress("/livelily"+std::to_string(editors[session.whichPane].getSendLinesPaneNdx())+"/press");
				m.addStringArg(editors[session.whichPane].allLines[i].str);
				editors[session.whichPane].oscKeys->send(m);
			}
		}
		else {
			if (editors[session.whichPane].allLines[i].str == "\\active") {
				lines.push_back(std::make_pair(i, editors[session.whichPane].allLines[i].str));
			}
		}
	}
	session.parseLines(lines, numLines, false);
}

//--------------------------------------------------------------
// the editors are drawn on the main thread, so a message from another thread (e.g. a function called by the sequencer)
// or from a function the sequencer called, which goes to the line of its definition, is queued, and shown in update()
void ofApp::setLineMessage(int pane, int line, int errorCode, const std::string& text, const std::string& boundFunction)
{
	if (std::this_thread::get_id() == mainThreadId && boundFunction.empty()) {
		editors[pane].setTraceback(errorCode, text, line);
		return;
	}
	std::lock_guard<std::mutex> lock(pendingTracebacksMutex);
	pendingTracebacks.push_back({pane, line, false, errorCode, text, boundFunction});
}

//--------------------------------------------------------------
void ofApp::clearLineMessage(int pane, int line, const std::string& boundFunction)
{
	if (std::this_thread::get_id() == mainThreadId && boundFunction.empty()) {
		editors[pane].releaseTraceback(line);
		return;
	}
	std::lock_guard<std::mutex> lock(pendingTracebacksMutex);
	pendingTracebacks.push_back({pane, line, true, 0, "", boundFunction});
}

//--------------------------------------------------------------
bool ofApp::lineDefinesFunction(const std::string& str, const std::string& funcName)
{
	// a definition line is: optional white space, "\function", white space, the function name (without the backslash)
	// followed by white space, an opening curly bracket, a comment, or the end of the line
	const std::string cmd = "\\function";
	size_t pos = str.find_first_not_of(" \t");
	if (pos == std::string::npos || str.compare(pos, cmd.size(), cmd) != 0) return false;
	pos += cmd.size();
	size_t nameStart = str.find_first_not_of(" \t", pos);
	// there must be white space between \function and the name
	if (nameStart == std::string::npos || nameStart == pos) return false;
	size_t nameEnd = str.find_first_of(" \t{%", nameStart);
	std::string name = str.substr(nameStart, nameEnd == std::string::npos ? std::string::npos : nameEnd - nameStart);
	return "\\" + name == funcName;
}

//--------------------------------------------------------------
std::pair<int, int> ofApp::findFunctionDefinitionLine(const std::string& funcName)
{
	int pane, defLine;
	{
		std::lock_guard<std::mutex> lock(session.sharedData.functionsMutex);
		auto funcIndexIt = session.functionIndexes.find(funcName);
		if (funcIndexIt == session.functionIndexes.end()) return std::make_pair(-1, -1);
		auto funcIt = session.sharedData.functions.find(funcIndexIt->second);
		if (funcIt == session.sharedData.functions.end()) return std::make_pair(-1, -1);
		pane = funcIt->second.getDefinitionPane();
		defLine = funcIt->second.getDefinitionLine();
	}
	auto editorIt = editors.find(pane);
	if (editorIt == editors.end()) return std::make_pair(-1, -1);
	std::map<int, line>& lines = editorIt->second.allLines;
	// first check the stored line, which is the common case and avoids searching the pane on every call
	auto lineIt = lines.find(defLine);
	if (lineIt != lines.end() && lineDefinesFunction(lineIt->second.str, funcName)) {
		return std::make_pair(pane, defLine);
	}
	// the definition has moved (e.g. lines were inserted or deleted above it), or its line was changed or deleted
	// so search the pane for it, and if it is defined more than once, pick the definition closest to the stored line
	int foundLine = -1;
	int minDistance = std::numeric_limits<int>::max();
	for (auto it = lines.begin(); it != lines.end(); ++it) {
		if (lineDefinesFunction(it->second.str, funcName)) {
			int distance = std::abs(it->first - defLine);
			if (distance < minDistance) {
				minDistance = distance;
				foundLine = it->first;
			}
		}
	}
	// the definition is no longer in the pane
	if (foundLine < 0) return std::make_pair(-1, -1);
	// store the new line, so the next lookup finds it without searching
	{
		std::lock_guard<std::mutex> lock(session.sharedData.functionsMutex);
		auto funcIndexIt = session.functionIndexes.find(funcName);
		if (funcIndexIt != session.functionIndexes.end()) {
			auto funcIt = session.sharedData.functions.find(funcIndexIt->second);
			// only if the function hasn't been redefined elsewhere in the meantime
			if (funcIt != session.sharedData.functions.end() && funcIt->second.getDefinitionPane() == pane && funcIt->second.getDefinitionLine() == defLine) {
				funcIt->second.setDefinitionLocation(pane, foundLine);
			}
		}
	}
	return std::make_pair(pane, foundLine);
}

//--------------------------------------------------------------
void ofApp::applyPendingTracebacks()
{
	// take the queued tracebacks while holding the lock, and apply them after releasing it
	std::vector<PendingTraceback> tracebacks;
	{
		std::lock_guard<std::mutex> lock(pendingTracebacksMutex);
		if (pendingTracebacks.empty()) return;
		tracebacks.swap(pendingTracebacks);
	}
	for (const PendingTraceback& tb : tracebacks) {
		int pane = tb.pane;
		int lineNum = tb.lineNum;
		// tracebacks of bound functions go to the line where the function is defined
		// if that line still defines it, otherwise to the dummy line they were set with
		if (!tb.boundFunction.empty()) {
			std::pair<int, int> definition = findFunctionDefinitionLine(tb.boundFunction);
			if (definition.first > -1) {
				pane = definition.first;
				lineNum = definition.second;
			}
		}
		// the pane might have been closed and the line deleted since the traceback was queued
		auto editorIt = editors.find(pane);
		if (editorIt == editors.end()) continue;
		// setTraceback() uses operator[] which would create the line if it doesn't exist
		if (editorIt->second.allLines.find(lineNum) == editorIt->second.allLines.end()) continue;
		if (tb.release) editorIt->second.releaseTraceback(lineNum);
		else editorIt->second.setTraceback(tb.errorCode, tb.errorStr, lineNum);
	}
}

//--------------------------------------------------------------
void ofApp::setEditorOption(int pane, const std::string& option, const std::string& value)
{
	Editor& editor = editors[pane];
	if (option == "alignment") {
		// the width of a unit of the grid applies at once if alignment is on, and the next time it is turned on otherwise
		if (value == "short" || value == "long") {
			editor.alignShort = (value == "short");
			if (editor.alignment) alignBars(pane);
		}
		else {
			editor.alignment = (value == "on");
			if (editor.alignment) alignBars(pane);
			else unalignBars(pane);
		}
	}
	else if (option == "animation") editor.setAnimation(value == "on");
	else if (option == "autobrackets") editor.setAutocomplete(value == "on");
	else if (option == "autocomplete") editor.setAutocompletion(value == "on");
}

//--------------------------------------------------------------
bool ofApp::openHelp(int pane, const std::string& filePath, int newPane)
{
	// a new pane beside or below the active one (as Ctrl+v and Ctrl+h do), which becomes the active one
	if (pane != session.whichPane) setActivePane(pane);
	if (newPane == 1) addPane(86);
	else if (newPane == 2) addPane(72);
	bool fileOpened = editors[session.whichPane].loadHelpFile(filePath, newPane);
	if (!fileOpened && newPane > 0) removePane();
	return fileOpened;
}

//--------------------------------------------------------------
void ofApp::storeEditorNoteSpans(int lineNum, int instNdx, int barNdx, int numNotes)
{
	// stores which token of the line of the editor every note of an instrument in a bar comes from, so that the token can be
	// highlighted while its note plays (see \editor.animate)
	// a note of a bar that copies another bar (e.g. \1 or \1.\inst) comes from the copying token and from the note of the copied bar
	// if the notes can't be matched to the tokens of the line (e.g. a line in a function), nothing is stored for the bar
	std::vector<std::vector<EditorNoteSpan>>& spans = editorNoteSpans[std::make_pair(instNdx, barNdx)];
	spans.clear();
	auto editorIt = editors.find(session.whichPane);
	if (editorIt == editors.end() || editorIt->second.allLines.find(lineNum) == editorIt->second.allLines.end()) return;
	const std::string& str = editorIt->second.allLines[lineNum].str;
	std::vector<PositionedToken> tokens = positionedTokens(str);
	// the melodic line starts after the name of the instrument, which can be at the start of the line or after \bar (e.g. \bar 1 { \inst c'1 })
	std::string instName = "\\" + session.sharedData.instruments[instNdx].getName();
	size_t first = 0;
	while (first < tokens.size() && tokens[first].text != instName && tokens[first].text.substr(0, instName.size() + 1) != instName + ".") first++;
	if (first >= tokens.size()) return;
	first++;
	// the melodic line ends at the end of the line, or at a closing bracket that doesn't belong to a tuplet (e.g. the end of a bar in one line)
	// in a bar written in one line, it also ends where the next statement starts, at the name of another instrument or group, or a command of the bar
	auto startsStatement = [&](const std::string& word) {
		if (word == "\\time" || word == "\\tempo" || word == "\\beatat") return true;
		if (word.size() < 2 || word[0] != '\\') return false;
		std::string name = word.substr(1, word.find('.') == std::string::npos ? std::string::npos : word.find('.') - 1);
		return session.sharedData.instrumentIndexes.find("\\" + name) != session.sharedData.instrumentIndexes.end() || session.instGroups.find("\\" + name) != session.instGroups.end();
	};
	size_t last = first;
	int braceDepth = 0;
	while (last < tokens.size()) {
		if (tokens[last].text == "{") braceDepth++;
		else if (tokens[last].text == "}") {
			if (braceDepth == 0) break;
			braceDepth--;
		}
		else if (braceDepth == 0 && startsStatement(tokens[last].text)) break;
		last++;
	}
	std::vector<PositionedToken> melodic(tokens.begin() + first, tokens.begin() + last);
	// with \bars, the bars are separated by |, and the notes are those of the bar being parsed
	if (session.parsingBars) {
		std::vector<PositionedToken> segment;
		int barCounter = 1;
		for (const PositionedToken& t : melodic) {
			if (t.text == "|") barCounter++;
			else if (barCounter == session.barsIterCounter) segment.push_back(t);
		}
		melodic = segment;
	}
	// commands with arguments in curly brackets make notes that are not written as they play (e.g. \euclide {16 5 4 c' cis'} or \transpose { c d e 2 })
	// so their arguments are not notes, and while their notes play, the command is highlighted as a whole
	// \euclide (and other such commands) from its backslash to its closing bracket, and \transpose with the contents of its curly brackets
	// \transpose without curly brackets has its arguments up to the number of semitones (e.g. \transpose \1 2)
	std::vector<int> groupEnd(melodic.size(), -1); // the index of the last token of a command with curly brackets
	std::vector<int> groupStart(melodic.size(), -1); // the index of the first token that is highlighted for it
	std::vector<bool> insideGroup(melodic.size(), false);
	std::vector<bool> skipToken(melodic.size(), false); // \transpose with notes only, and its number of semitones
	for (size_t i = 0; i < melodic.size(); i++) {
		const std::string& t = melodic[i].text;
		if (t.empty() || t[0] != '\\' || insideGroup[i] || t == "\\tuplet" || t == "\\tup") continue;
		bool isTranspose = (t == "\\transpose");
		size_t end = melodic.size();
		// a command that takes a name can have it before its curly brackets (e.g. \apply \1.\inst {c' e'}, see CommandSpec)
		size_t open = i + 1;
		if (i + 2 < melodic.size() && melodic[i+1].text != "{" && melodic[i+2].text == "{") open = i + 2;
		if (open < melodic.size() && melodic[open].text == "{") {
			int depth = 0;
			for (size_t j = open; j < melodic.size(); j++) {
				if (melodic[j].text == "{") depth++;
				else if (melodic[j].text == "}" && --depth == 0) {
					end = j;
					break;
				}
			}
		}
		else if (isTranspose) {
			for (size_t j = i + 1; j < melodic.size(); j++) {
				if (session.isFloat(melodic[j].text)) {
					end = j;
					break;
				}
			}
		}
		if (end >= melodic.size()) continue;
		if (isTranspose) {
			// with notes only (e.g. \transpose { c4 r e r -12 }), its notes and rests are highlighted as in any melodic line
			// and only \transpose and its number of semitones are not (with a bar name in it, it is highlighted as a whole)
			bool hasCommand = false;
			for (size_t j = i + 1; j <= end; j++) {
				if (!melodic[j].text.empty() && melodic[j].text[0] == '\\') hasCommand = true;
			}
			if (!hasCommand) {
				size_t amountNdx = (melodic[end].text == "}" ? end - 1 : end);
				skipToken[i] = true;
				if (amountNdx > i && session.isFloat(melodic[amountNdx].text)) skipToken[amountNdx] = true;
				continue;
			}
		}
		groupEnd[i] = (int)end;
		groupStart[i] = (int)(isTranspose ? i + 1 : i);
		for (size_t j = i + 1; j <= end; j++) insideGroup[j] = true;
	}
	std::vector<int> order;
	size_t ndx = 0;
	expandRepetitions(melodic, ndx, false, order);
	int pane = session.whichPane;
	int lineId = editorIt->second.getLineId(lineNum);
	auto spanOf = [&](int tokenNdx) {
		if (groupEnd[tokenNdx] >= 0) {
			// a command with curly brackets, from the first to the last of its highlighted tokens (see groupStart above)
			int start = melodic[groupStart[tokenNdx]].start;
			int length = melodic[groupEnd[tokenNdx]].start + (int)melodic[groupEnd[tokenNdx]].text.size() - start;
			return EditorNoteSpan{pane, lineId, start, length, str.substr(start, length)};
		}
		return EditorNoteSpan{pane, lineId, melodic[tokenNdx].start, (int)melodic[tokenNdx].text.size(), melodic[tokenNdx].text};
	};
	auto spansOf = [&](int tokenNdx) { return std::vector<EditorNoteSpan>{spanOf(tokenNdx)}; };
	// every token gives the notes it plays, each with its tokens
	// a command that is not known here (e.g. a variable or a function) gives the notes that are left, if it is the only one
	std::vector<std::vector<EditorNoteSpan>> result;
	int unknownPosition = -1; // where in result the notes of the unknown command go
	int unknownToken = -1;
	for (size_t k = 0; k < order.size(); k++) {
		const std::string& t = melodic[order[k]].text;
		if (t == "{" || t == "}" || t == "|" || t == "[" || t == "]") continue;
		// the arguments of a command with curly brackets (see groupEnd above)
		if (insideGroup[order[k]] || skipToken[order[k]]) continue;
		// commands that don't play notes, and their argument
		if (t == "\\ottava" || t == "\\ott" || t == "\\clef" || t == "\\tuplet" || t == "\\tup") {
			k++;
			continue;
		}
		if (t.empty() || t[0] != '\\') {
			// a note, a rest, or a chord, or the steps of the x - notation (e.g. x-x-), each with its own characters of the token
			std::vector<Step> steps = stepTokens(t);
			if (steps.size() > 1) {
				for (const Step& step : steps) {
					int start = melodic[order[k]].start + (int)step.start;
					result.push_back({EditorNoteSpan{pane, lineId, start, (int)step.length, t.substr(step.start, step.length)}});
				}
			}
			else {
				result.push_back(spansOf(order[k]));
			}
			continue;
		}
		// a bar that is copied, with the instrument of this line or another one, e.g. \1 or \1.\inst
		size_t dot = t.find('.');
		std::string barName = t.substr(0, dot);
		auto barIt = session.sharedData.barsIndexes.find(barName);
		if (barIt != session.sharedData.barsIndexes.end()) {
			int sourceInst = instNdx;
			if (dot != std::string::npos) {
				std::string sourceName = t.substr(dot + 1);
				if (!startsWith(sourceName, "\\")) sourceName = "\\" + sourceName;
				auto instIt = session.sharedData.instrumentIndexes.find(sourceName);
				if (instIt == session.sharedData.instrumentIndexes.end()) return;
				sourceInst = instIt->second;
			}
			auto sourceIt = editorNoteSpans.find(std::make_pair(sourceInst, barIt->second));
			int numSourceNotes = (int)session.sharedData.instruments[sourceInst].notes[barIt->second].size();
			for (int n = 0; n < numSourceNotes; n++) {
				std::vector<EditorNoteSpan> noteSpans = spansOf(order[k]);
				if (sourceIt != editorNoteSpans.end() && n < (int)sourceIt->second.size()) {
					noteSpans.insert(noteSpans.end(), sourceIt->second[n].begin(), sourceIt->second[n].end());
				}
				result.push_back(noteSpans);
			}
			continue;
		}
		// any other command
		if (unknownToken >= 0) return; // more than one, the notes can't be matched
		unknownToken = order[k];
		unknownPosition = (int)result.size();
	}
	if (unknownToken >= 0) {
		int remaining = numNotes - (int)result.size();
		if (remaining < 0) return;
		result.insert(result.begin() + unknownPosition, remaining, std::vector<EditorNoteSpan>{spanOf(unknownToken)});
	}
	if ((int)result.size() != numNotes) return;
	spans = result;
}

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
	// the tokens of the editor that play the notes of the instruments (\editor.animate)
	std::map<std::pair<int, int>, std::vector<std::vector<EditorNoteSpan>>> spans;
	for (auto& entry : editorNoteSpans) {
		if (entry.first.first == instNdx) continue;
		spans[std::make_pair(newIndex(entry.first.first), entry.first.second)] = entry.second;
	}
	editorNoteSpans = spans;
	{
		std::lock_guard<std::mutex> lock(activeEditorNotesMutex);
		activeEditorNotes.clear();
		activeEditorRests.clear();
	}
	// the lines of the editors that the bars of the instruments are written in
	for (auto& editor : editors) {
		std::map<int, std::map<int, int>> connected;
		for (auto& entry : editor.second.instsConnectedToLine) {
			if (entry.first == instNdx) continue;
			connected[newIndex(entry.first)] = entry.second;
		}
		editor.second.instsConnectedToLine = connected;
	}
	setScoreCoords();
}

//--------------------------------------------------------------
void ofApp::instrumentsCleared()
{
	// the tokens of the editor that play the notes of the instruments (\editor.animation)
	editorNoteSpans.clear();
	std::lock_guard<std::mutex> lock(activeEditorNotesMutex);
	activeEditorNotes.clear();
	activeEditorRests.clear();
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
