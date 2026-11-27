// the setup of a session: its defaults, and the tables of the names of the languages (for coloring, completion, and reserved names)
// which the front end calls once its FrontendHost is set (see session.h)

#include "session.h"
#include "clock.h"

//--------------------------------------------------------------
void Session::setup(Color foreground)
{
	sequencer.setup(&sharedData);
	sequencer.setMidiOutput(midi.get());
	sequencer.setFrontend(frontend);

	// define audio properties
	sampleRate = 48000;
	bufferSize = 256;
	inChannels = 2;
	outChannels = 2;
	//audioDevID = 1;
	inSoundDeviceSet = false;
	outSoundDeviceSet = false;
	pyoSet = false;

	startSequencer = false;

	parsingBar = false;
	parsingBars = false;
	parsingLoop = false;
	storingFunction = false;
	storingList = false;
	barsIterCounter = 0;
	numBarsToParse = 0;
	numBarsToParseSet = false;
	barsCmdNumBars = 0;
	firstInstForBarsIndex = 0;
	firstInstForBarsSet = false;
	checkIfAllPartsReceivedBarData = false;
	lastFunctionIndex = 0;
	storingFunctionIndex = 0;
	functionBodyDepth = 0;

	//oscReceiver->setup(OSCPORT);
	oscReceiverIsSet = false;
	// waiting time to receive a response from a score part server is one frame at 60 FPS
	scorePartResponseTime = (uint64_t)((1000.0 / 60.0) + 0.5);

	sharedData.numInstruments = 0;

	whichPane = 0;

	sharedData.tempoMs[0] = 500;
	sharedData.BPMMultiplier[0] = 1;
	sharedData.beatAtDifferentThanDivisor[0] = false;
	sharedData.beatAtValCounter = 0;
	sharedData.numerator[0] = 4;
	sharedData.denominator[0] = 4;
	sharedData.numBeats[0] = 4;

	barError = false;

	sharedData.loopIndex = 0;
	sharedData.tempLoopIndex = 0;
	sharedData.thisBarIndex = 0;
	sharedData.beatCounter = 0; // this is controlled by the sequencer
	sharedData.barCounter = 0; // also controlled by the sequencer

	seqResolution = 16;

	sharedData.animate = false;
	sharedData.setAnimation = false;

	maestroToggleSet = false;
	receivingMaestro = false;
	maestroValNdx = 0;
	maestroInitialized = false;
	maestroBeatsIn = 1;
	maestroValThresh = 7.5;
	maestroTimeStamp = Clock::millis();
	maestroAddress = "/maestro";
	maestroToggleAddress = "/toggle";
	maestroLevareAddress = "/levare";

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

	// store the articulation names std::strings
	std::string articulSymsLocal[8] = {"none", "marcato", "trill", "tenuto", \
								  "staccatissimo", "accent", "staccato", "portando"};
	for (int i = 0; i < 8; i++) {
		sharedData.articulSyms[i] = articulSymsLocal[i];
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
	for (const auto& command : commandTable()) commandsMap[livelily][command.first[1]][command.first] = Colors::fuchsia;
	commandsMap[livelily]['j'][""] = foreground;
	commandsMap[livelily]['q'][""] = foreground;
	commandsMap[livelily]['x'][""] = foreground;
	commandsMap[livelily]['z'][""] = foreground;

	// methods
	commandsMap[livelily]['a']["accoffset"] = Colors::violet;
	commandsMap[livelily]['a']["add"] = Colors::violet;
	commandsMap[livelily]['a']["alignment"] = Colors::violet;
	commandsMap[livelily]['a']["autobrackets"] = Colors::violet;
	commandsMap[livelily]['a']["autocomplete"] = Colors::violet;
	commandsMap[livelily]['a']["animation"] = Colors::violet;
	commandsMap[livelily]['b']["beatsin"] = Colors::violet;
	commandsMap[livelily]['b']["beattype"] = Colors::violet;
	commandsMap[livelily]['b']["bind"] = Colors::violet;
	commandsMap[livelily]['c']["check"] = Colors::violet;
	commandsMap[livelily]['c']["clear"] = Colors::violet;
	commandsMap[livelily]['c']["colors"] = Colors::violet;
	commandsMap[livelily]['c']["correct"] = Colors::violet;
	commandsMap[livelily]['c']["cursor"] = Colors::violet;
	commandsMap[livelily]['d']["delay"] = Colors::violet;
	commandsMap[livelily]['f']["finish"] = Colors::violet;
	commandsMap[livelily]['f']["fullscreen"] = Colors::violet;
	commandsMap[livelily]['g']["goto"] = Colors::violet;
	commandsMap[livelily]['h']["hide"] = Colors::violet;
	commandsMap[livelily]['i']["init"] = Colors::violet;
	commandsMap[livelily]['l']["levareaddr"] = Colors::violet;
	commandsMap[livelily]['l']["locate"] = Colors::violet;
	commandsMap[livelily]['m']["mainaddr"] = Colors::violet;
	commandsMap[livelily]['m']["map"] = Colors::violet;
	commandsMap[livelily]['m']["midichan"] = Colors::violet;
	commandsMap[livelily]['m']["midiport"] = Colors::violet;
	commandsMap[livelily]['m']["movex"] = Colors::violet;
	commandsMap[livelily]['m']["movey"] = Colors::violet;
	commandsMap[livelily]['n']["normal"] = Colors::violet;
	commandsMap[livelily]['n']["numbars"] = Colors::violet;
	commandsMap[livelily]['o']["onrelease"] = Colors::violet;
	commandsMap[livelily]['p']["play"] = Colors::violet;
	commandsMap[livelily]['r']["recenter"] = Colors::violet;
	commandsMap[livelily]['r']["release"] = Colors::violet;
	commandsMap[livelily]['r']["remove"] = Colors::violet;
	commandsMap[livelily]['s']["send"] = Colors::violet;
	commandsMap[livelily]['s']["sendmidi"] = Colors::violet;
	commandsMap[livelily]['s']["setargs"] = Colors::violet;
	commandsMap[livelily]['s']["setup"] = Colors::violet;
	commandsMap[livelily]['s']["sendto"] = Colors::violet;
	commandsMap[livelily]['s']["seqres"] = Colors::violet;
	commandsMap[livelily]['s']["show"] = Colors::violet;
	commandsMap[livelily]['s']["showbeat"] = Colors::violet;
	commandsMap[livelily]['s']["sndfont"] = Colors::violet;
	commandsMap[livelily]['s']["stop"] = Colors::violet;
	commandsMap[livelily]['s']["stopnow"] = Colors::violet;
	commandsMap[livelily]['t']["toggleaddr"] = Colors::violet;
	commandsMap[livelily]['t']["transpose"] = Colors::violet;
	commandsMap[livelily]['t']["traverse"] = Colors::violet;
	commandsMap[livelily]['t']["tune"] = Colors::violet;
	commandsMap[livelily]['u']["unbind"] = Colors::violet;
	commandsMap[livelily]['u']["update"] = Colors::violet;
	commandsMap[livelily]['v']["val"] = Colors::violet;
	commandsMap[livelily]['v']["valndx"] = Colors::violet;
	commandsMap[livelily]['v']["valthresh"] = Colors::violet;
	commandsMap[livelily]['v']["volume"] = Colors::violet;

	// arguments
	commandsMap[livelily]['a']["all"] = Colors::skyBlue;
	commandsMap[livelily]['a']["alto"] = Colors::skyBlue;
	commandsMap[livelily]['b']["barcount"] = Colors::skyBlue;
	commandsMap[livelily]['b']["barstart"] = Colors::skyBlue;
	commandsMap[livelily]['b']["bass"] = Colors::skyBlue;
	commandsMap[livelily]['b']["beat"] = Colors::skyBlue;
	commandsMap[livelily]['c']["char"] = Colors::skyBlue;
	commandsMap[livelily]['f']["fr"] = Colors::skyBlue;
	commandsMap[livelily]['f']["framerate"] = Colors::skyBlue;
	commandsMap[livelily]['h']["hor"] = Colors::skyBlue;
	commandsMap[livelily]['i']["immediately"] = Colors::skyBlue;
	commandsMap[livelily]['i']["invert"] = Colors::skyBlue;
	commandsMap[livelily]['l']["line"] = Colors::skyBlue;
	commandsMap[livelily]['l']["localhost"] = Colors::skyBlue;
	commandsMap[livelily]['l']["loopstart"] = Colors::skyBlue;
	commandsMap[livelily]['n']["notes"] = Colors::skyBlue;
	commandsMap[livelily]['o']["off"] = Colors::skyBlue;
	commandsMap[livelily]['o']["on"] = Colors::skyBlue;
	// the widths of the units of \editor.alignment
	commandsMap[livelily]['s']["short"] = Colors::skyBlue;
	commandsMap[livelily]['l']["long"] = Colors::skyBlue;
	commandsMap[livelily]['o']["onlast"] = Colors::skyBlue;
	commandsMap[livelily]['o']["onoctave"] = Colors::skyBlue;
	commandsMap[livelily]['p']["perc"] = Colors::skyBlue;
	commandsMap[livelily]['p']["percussion"] = Colors::skyBlue;
	commandsMap[livelily]['p']["pianoroll"] = Colors::skyBlue;
	commandsMap[livelily]['s']["sequencer"] = Colors::skyBlue;
	commandsMap[livelily]['s']["seq"] = Colors::skyBlue;
	commandsMap[livelily]['p']["python"] = Colors::skyBlue;
	commandsMap[livelily]['r']["reset"] = Colors::skyBlue;
	commandsMap[livelily]['r']["rhythm"] = Colors::skyBlue;
	commandsMap[livelily]['s']["scope"] = Colors::skyBlue;
	commandsMap[livelily]['t']["tempo"] = Colors::skyBlue;
	commandsMap[livelily]['t']["treble"] = Colors::skyBlue;
	commandsMap[livelily]['v']["vert"] = Colors::skyBlue;

	// color names, same color as arguments above
	commandsMap[livelily]['a']["aliceBlue"] = Colors::skyBlue;
	commandsMap[livelily]['a']["antiqueWhite"] = Colors::skyBlue;
	commandsMap[livelily]['a']["aqua"] = Colors::skyBlue;
	commandsMap[livelily]['a']["aquamarine"] = Colors::skyBlue;
	commandsMap[livelily]['a']["azure"] = Colors::skyBlue;
	commandsMap[livelily]['b']["black"] = Colors::skyBlue;
	commandsMap[livelily]['b']["blue"] = Colors::skyBlue;
	commandsMap[livelily]['b']["beige"] = Colors::skyBlue;
	commandsMap[livelily]['b']["bisque"] = Colors::skyBlue;
	commandsMap[livelily]['b']["blanchedAlmond"] = Colors::skyBlue;
	commandsMap[livelily]['b']["blueViolet"] = Colors::skyBlue;
	commandsMap[livelily]['b']["brown"] = Colors::skyBlue;
	commandsMap[livelily]['b']["burlyWood"] = Colors::skyBlue;
	commandsMap[livelily]['c']["cyan"] = Colors::skyBlue;
	commandsMap[livelily]['c']["cadetBlue"] = Colors::skyBlue;
	commandsMap[livelily]['c']["chartreuse"] = Colors::skyBlue;
	commandsMap[livelily]['c']["chocolate"] = Colors::skyBlue;
	commandsMap[livelily]['c']["coral"] = Colors::skyBlue;
	commandsMap[livelily]['c']["cornflowerBlue"] = Colors::skyBlue;
	commandsMap[livelily]['c']["cornsilk"] = Colors::skyBlue;
	commandsMap[livelily]['c']["crimson"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkBlue"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkCyan"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkGoldenRod"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkGray"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkGrey"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkGreen"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkKhaki"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkMagenta"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkOliveGreen"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkOrange"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkOrchid"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkRed"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkSalmon"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkSeaGreen"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkSlateBlue"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkSlateGray"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkSlateGrey"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkTurquoise"] = Colors::skyBlue;
	commandsMap[livelily]['d']["darkViolet"] = Colors::skyBlue;
	commandsMap[livelily]['d']["deepPink"] = Colors::skyBlue;
	commandsMap[livelily]['d']["deepSkyBlue"] = Colors::skyBlue;
	commandsMap[livelily]['d']["dimGray"] = Colors::skyBlue;
	commandsMap[livelily]['d']["dimGrey"] = Colors::skyBlue;
	commandsMap[livelily]['d']["dodgerBlue"] = Colors::skyBlue;
	commandsMap[livelily]['f']["fireBrick"] = Colors::skyBlue;
	commandsMap[livelily]['f']["floralWhite"] = Colors::skyBlue;
	commandsMap[livelily]['f']["forestGreen"] = Colors::skyBlue;
	commandsMap[livelily]['f']["fuchsia"] = Colors::skyBlue;
	commandsMap[livelily]['g']["green"] = Colors::skyBlue;
	commandsMap[livelily]['g']["gray"] = Colors::skyBlue;
	commandsMap[livelily]['g']["gainsboro"] = Colors::skyBlue;
	commandsMap[livelily]['g']["ghostWhite"] = Colors::skyBlue;
	commandsMap[livelily]['g']["gold"] = Colors::skyBlue;
	commandsMap[livelily]['g']["goldenRod"] = Colors::skyBlue;
	commandsMap[livelily]['g']["grey"] = Colors::skyBlue;
	commandsMap[livelily]['g']["greenYellow"] = Colors::skyBlue;
	commandsMap[livelily]['h']["honeyDew"] = Colors::skyBlue;
	commandsMap[livelily]['h']["hotPink"] = Colors::skyBlue;
	commandsMap[livelily]['i']["indianRed "] = Colors::skyBlue;
	commandsMap[livelily]['i']["indigo "] = Colors::skyBlue;
	commandsMap[livelily]['i']["ivory"] = Colors::skyBlue;
	commandsMap[livelily]['k']["khaki"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lavender"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lavenderBlush"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lawnGreen"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lemonChiffon"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lightBlue"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lightCoral"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lightCyan"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lightGoldenRodYellow"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lightGray"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lightGrey"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lightGreen"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lightPink"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lightSalmon"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lightSeaGreen"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lightSkyBlue"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lightSlateGray"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lightSlateGrey"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lightSteelBlue"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lightYellow"] = Colors::skyBlue;
	commandsMap[livelily]['l']["lime"] = Colors::skyBlue;
	commandsMap[livelily]['l']["limeGreen"] = Colors::skyBlue;
	commandsMap[livelily]['l']["linen"] = Colors::skyBlue;
	commandsMap[livelily]['m']["magenta"] = Colors::skyBlue;
	commandsMap[livelily]['m']["maroon"] = Colors::skyBlue;
	commandsMap[livelily]['m']["mediumAquaMarine"] = Colors::skyBlue;
	commandsMap[livelily]['m']["mediumBlue"] = Colors::skyBlue;
	commandsMap[livelily]['m']["mediumOrchid"] = Colors::skyBlue;
	commandsMap[livelily]['m']["mediumPurple"] = Colors::skyBlue;
	commandsMap[livelily]['m']["mediumSeaGreen"] = Colors::skyBlue;
	commandsMap[livelily]['m']["mediumSlateBlue"] = Colors::skyBlue;
	commandsMap[livelily]['m']["mediumSpringGreen"] = Colors::skyBlue;
	commandsMap[livelily]['m']["mediumTurquoise"] = Colors::skyBlue;
	commandsMap[livelily]['m']["mediumVioletRed"] = Colors::skyBlue;
	commandsMap[livelily]['m']["midnightBlue"] = Colors::skyBlue;
	commandsMap[livelily]['m']["mintCream"] = Colors::skyBlue;
	commandsMap[livelily]['m']["mistyRose"] = Colors::skyBlue;
	commandsMap[livelily]['m']["moccasin"] = Colors::skyBlue;
	commandsMap[livelily]['n']["navajoWhite"] = Colors::skyBlue;
	commandsMap[livelily]['n']["navy"] = Colors::skyBlue;
	commandsMap[livelily]['o']["oldLace"] = Colors::skyBlue;
	commandsMap[livelily]['o']["olive"] = Colors::skyBlue;
	commandsMap[livelily]['o']["oliveDrab"] = Colors::skyBlue;
	commandsMap[livelily]['o']["orange"] = Colors::skyBlue;
	commandsMap[livelily]['o']["orangeRed"] = Colors::skyBlue;
	commandsMap[livelily]['o']["orchid"] = Colors::skyBlue;
	commandsMap[livelily]['p']["paleGoldenRod"] = Colors::skyBlue;
	commandsMap[livelily]['p']["paleGreen"] = Colors::skyBlue;
	commandsMap[livelily]['p']["paleTurquoise"] = Colors::skyBlue;
	commandsMap[livelily]['p']["paleVioletRed"] = Colors::skyBlue;
	commandsMap[livelily]['p']["papayaWhip"] = Colors::skyBlue;
	commandsMap[livelily]['p']["peachPuff"] = Colors::skyBlue;
	commandsMap[livelily]['p']["peru"] = Colors::skyBlue;
	commandsMap[livelily]['p']["pink"] = Colors::skyBlue;
	commandsMap[livelily]['p']["plum"] = Colors::skyBlue;
	commandsMap[livelily]['p']["powderBlue"] = Colors::skyBlue;
	commandsMap[livelily]['p']["purple"] = Colors::skyBlue;
	commandsMap[livelily]['r']["red"] = Colors::skyBlue;
	commandsMap[livelily]['r']["rosyBrown"] = Colors::skyBlue;
	commandsMap[livelily]['r']["royalBlue"] = Colors::skyBlue;
	commandsMap[livelily]['s']["saddleBrown"] = Colors::skyBlue;
	commandsMap[livelily]['s']["salmon"] = Colors::skyBlue;
	commandsMap[livelily]['s']["sandyBrown"] = Colors::skyBlue;
	commandsMap[livelily]['s']["seaGreen"] = Colors::skyBlue;
	commandsMap[livelily]['s']["seaShell"] = Colors::skyBlue;
	commandsMap[livelily]['s']["sienna"] = Colors::skyBlue;
	commandsMap[livelily]['s']["silver"] = Colors::skyBlue;
	commandsMap[livelily]['s']["skyBlue"] = Colors::skyBlue;
	commandsMap[livelily]['s']["slateBlue"] = Colors::skyBlue;
	commandsMap[livelily]['s']["slateGray"] = Colors::skyBlue;
	commandsMap[livelily]['s']["slateGrey"] = Colors::skyBlue;
	commandsMap[livelily]['s']["snow"] = Colors::skyBlue;
	commandsMap[livelily]['s']["springGreen"] = Colors::skyBlue;
	commandsMap[livelily]['s']["steelBlue"] = Colors::skyBlue;
	commandsMap[livelily]['t']["tan"] = Colors::skyBlue;
	commandsMap[livelily]['t']["teal"] = Colors::skyBlue;
	commandsMap[livelily]['t']["thistle"] = Colors::skyBlue;
	commandsMap[livelily]['t']["tomato"] = Colors::skyBlue;
	commandsMap[livelily]['t']["turquoise"] = Colors::skyBlue;
	commandsMap[livelily]['v']["violet"] = Colors::skyBlue;
	commandsMap[livelily]['w']["white"] = Colors::skyBlue;
	commandsMap[livelily]['w']["wheat"] = Colors::skyBlue;
	commandsMap[livelily]['w']["whiteSmoke"] = Colors::skyBlue;
	commandsMap[livelily]['y']["yellow"] = Colors::skyBlue;
	commandsMap[livelily]['y']["yellowGreen"] = Colors::skyBlue;

	// dummy upper case letters (the LiveLily language doesn't have keywords with upper case letters)
	commandsMap[livelily]['A'][""] = foreground;
	commandsMap[livelily]['B'][""] = foreground;
	commandsMap[livelily]['C'][""] = foreground;
	commandsMap[livelily]['D'][""] = foreground;
	commandsMap[livelily]['E'][""] = foreground;
	commandsMap[livelily]['F'][""] = foreground;
	commandsMap[livelily]['G'][""] = foreground;
	commandsMap[livelily]['H'][""] = foreground;
	commandsMap[livelily]['I'][""] = foreground;
	commandsMap[livelily]['J'][""] = foreground;
	commandsMap[livelily]['K'][""] = foreground;
	commandsMap[livelily]['L'][""] = foreground;
	commandsMap[livelily]['M'][""] = foreground;
	commandsMap[livelily]['N'][""] = foreground;
	commandsMap[livelily]['O'][""] = foreground;
	commandsMap[livelily]['P'][""] = foreground;
	commandsMap[livelily]['Q'][""] = foreground;
	commandsMap[livelily]['R'][""] = foreground;
	commandsMap[livelily]['S'][""] = foreground;
	commandsMap[livelily]['T'][""] = foreground;
	commandsMap[livelily]['U'][""] = foreground;
	commandsMap[livelily]['V'][""] = foreground;
	commandsMap[livelily]['W'][""] = foreground;
	commandsMap[livelily]['X'][""] = foreground;
	commandsMap[livelily]['Y'][""] = foreground;
	commandsMap[livelily]['Z'][""] = foreground;
	// the commands of LiveLily are reserved names (see isReservedName()), which are stored before any name is defined
	for (const auto& byChar : commandsMap[livelily]) {
		for (const auto& cmd : byChar.second) {
			if (cmd.first.size() > 1 && cmd.first[0] == '\\') reservedNames.insert(cmd.first);
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
	commandsMap[python]['a']["as"] = Colors::violet;
	commandsMap[python]['a']["assert"] = Colors::violet;
	commandsMap[python]['a']["and"] = Colors::violet;
	commandsMap[python]['b']["break"] = Colors::violet;
	commandsMap[python]['c']["class"] = Colors::violet;
	commandsMap[python]['c']["continue"] = Colors::violet;
	commandsMap[python]['d']["def"] = Colors::violet;
	commandsMap[python]['d']["del"] = Colors::violet;
	commandsMap[python]['e']["elif"] = Colors::violet;
	commandsMap[python]['e']["else"] = Colors::violet;
	commandsMap[python]['e']["except"] = Colors::violet;
	commandsMap[python]['f']["for"] = Colors::violet;
	commandsMap[python]['f']["from"] = Colors::violet;
	commandsMap[python]['f']["finally"] = Colors::violet;
	commandsMap[python]['g']["global"] = Colors::violet;
	commandsMap[python]['h'][""] = foreground;
	commandsMap[python]['i']["if"] = Colors::violet;
	commandsMap[python]['i']["is"] = Colors::violet;
	commandsMap[python]['i']["import"] = Colors::violet;
	commandsMap[python]['i']["in"] = Colors::violet;
	commandsMap[python]['j'][""] = foreground;
	commandsMap[python]['k'][""] = foreground;
	commandsMap[python]['l']["lambda"] = Colors::violet;
	commandsMap[python]['m'][""] = foreground;
	commandsMap[python]['n']["not"] = Colors::violet;
	commandsMap[python]['n']["nonlocal"] = Colors::violet;
	commandsMap[python]['n']["not"] = Colors::violet;
	commandsMap[python]['o']["or"] = Colors::violet;
	commandsMap[python]['p']["pass"] = Colors::violet;
	commandsMap[python]['q'][""] = foreground;
	commandsMap[python]['r']["raise"] = Colors::violet;
	commandsMap[python]['r']["return"] = Colors::violet;
	commandsMap[python]['s'][""] = foreground;
	commandsMap[python]['t']["try"] = Colors::violet;
	commandsMap[python]['u'][""] = foreground;
	commandsMap[python]['v'][""] = foreground;
	commandsMap[python]['w']["while"] = Colors::violet;
	commandsMap[python]['w']["with"] = Colors::violet;
	commandsMap[python]['x'][""] = foreground;
	commandsMap[python]['y']["yield"] = Colors::violet;
	commandsMap[python]['z'][""] = foreground;
	commandsMap[python]['F']["False"] = Colors::violet;
	commandsMap[python]['J'][""] = foreground;
	commandsMap[python]['K'][""] = foreground;
	commandsMap[python]['N']["None"] = Colors::violet;
	commandsMap[python]['Q'][""] = foreground;
	commandsMap[python]['T']["True"] = Colors::violet;

	// Pyo classes
	commandsMap[python]['A']["Abs"] = Colors::deepSkyBlue;
	commandsMap[python]['A']["Atan2"] = Colors::deepSkyBlue;
	commandsMap[python]['A']["Adsr"] = Colors::deepSkyBlue;
	commandsMap[python]['A']["AllpassWV"] = Colors::deepSkyBlue;
	commandsMap[python]['A']["Allpass"] = Colors::deepSkyBlue;
	commandsMap[python]['A']["Allpass2"] = Colors::deepSkyBlue;
	commandsMap[python]['A']["Atone"] = Colors::deepSkyBlue;
	commandsMap[python]['A']["AToDB"] = Colors::deepSkyBlue;
	commandsMap[python]['A']["AtanTable"] = Colors::deepSkyBlue;
	commandsMap[python]['A']["AttackDetector"] = Colors::deepSkyBlue;
	commandsMap[python]['A']["Average"] = Colors::deepSkyBlue;
	commandsMap[python]['B']["Balance"] = Colors::deepSkyBlue;
	commandsMap[python]['B']["Biquad"] = Colors::deepSkyBlue;
	commandsMap[python]['B']["Biquadx"] = Colors::deepSkyBlue;
	commandsMap[python]['B']["Biquada"] = Colors::deepSkyBlue;
	commandsMap[python]['B']["BandSplit"] = Colors::deepSkyBlue;
	commandsMap[python]['B']["ButLP"] = Colors::deepSkyBlue;
	commandsMap[python]['B']["ButHP"] = Colors::deepSkyBlue;
	commandsMap[python]['B']["ButBP"] = Colors::deepSkyBlue;
	commandsMap[python]['B']["ButBR"] = Colors::deepSkyBlue;
	commandsMap[python]['B']["Blit"] = Colors::deepSkyBlue;
	commandsMap[python]['B']["BrownNoise"] = Colors::deepSkyBlue;
	commandsMap[python]['B']["Between"] = Colors::deepSkyBlue;
	commandsMap[python]['B']["Beat"] = Colors::deepSkyBlue;
	commandsMap[python]['B']["Bendin"] = Colors::deepSkyBlue;
	commandsMap[python]['B']["Binaural"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["Centroid"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["Cos"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["Ceil"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["Clip"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["Convolve"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["Chorus"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["ComplexRes"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["CarToPol"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["CvlVerb"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["ChenLee"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["CrossFM"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["CallAfter"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["CtlScan"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["CtlScan2"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["Change"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["Cloud"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["Count"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["Counter"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["CentsToTranspo"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["Clean_objects"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["Compare"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["ConotrolRead"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["ConotrolRec"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["ChebyTable"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["CosLogTable"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["CosTable"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["CurvyTable"] = Colors::deepSkyBlue;
	commandsMap[python]['C']["Compress"] = Colors::deepSkyBlue;
	commandsMap[python]['D']["Div"] = Colors::deepSkyBlue;
	commandsMap[python]['D']["Degrade"] = Colors::deepSkyBlue;
	commandsMap[python]['D']["Disto"] = Colors::deepSkyBlue;
	commandsMap[python]['D']["Delay"] = Colors::deepSkyBlue;
	commandsMap[python]['D']["Delay1"] = Colors::deepSkyBlue;
	commandsMap[python]['D']["DCBlock"] = Colors::deepSkyBlue;
	commandsMap[python]['D']["DBToA"] = Colors::deepSkyBlue;
	commandsMap[python]['D']["Denorm"] = Colors::deepSkyBlue;
	commandsMap[python]['D']["DataTable"] = Colors::deepSkyBlue;
	commandsMap[python]['D']["Dummy"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["Expseg"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["Exp"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["EventScale"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["EventGenerator"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["EventDummy"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["EventFilter"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["EventKey"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["EventSeq"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["EventSlide"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["EventIndex"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["EventMarkov"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["EventChoice"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["EventDrunk"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["EventNoise"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["EventCall"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["EventConditional"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["EventInstrument"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["Events"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["EQ"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["Euclide"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["ExpTable"] = Colors::deepSkyBlue;
	commandsMap[python]['E']["Expand"] = Colors::deepSkyBlue;
	commandsMap[python]['F']["Fader"] = Colors::deepSkyBlue;
	commandsMap[python]['F']["Follower"] = Colors::deepSkyBlue;
	commandsMap[python]['F']["Follower2"] = Colors::deepSkyBlue;
	commandsMap[python]['F']["Floor"] = Colors::deepSkyBlue;
	commandsMap[python]['F']["FreqShift"] = Colors::deepSkyBlue;
	commandsMap[python]['F']["FFT"] = Colors::deepSkyBlue;
	commandsMap[python]['F']["FrameAccum"] = Colors::deepSkyBlue;
	commandsMap[python]['F']["FrameDelta"] = Colors::deepSkyBlue;
	commandsMap[python]['F']["FastSine"] = Colors::deepSkyBlue;
	commandsMap[python]['F']["FM"] = Colors::deepSkyBlue;
	commandsMap[python]['F']["FToM"] = Colors::deepSkyBlue;
	commandsMap[python]['F']["FourBand"] = Colors::deepSkyBlue;
	commandsMap[python]['F']["Freeverb"] = Colors::deepSkyBlue;
	commandsMap[python]['G']["Gate"] = Colors::deepSkyBlue;
	commandsMap[python]['G']["Granulator"] = Colors::deepSkyBlue;
	commandsMap[python]['G']["Granule"] = Colors::deepSkyBlue;
	commandsMap[python]['H']["Hilbert"] = Colors::deepSkyBlue;
	commandsMap[python]['H']["HannTable"] = Colors::deepSkyBlue;
	commandsMap[python]['H']["HarmTable"] = Colors::deepSkyBlue;
	commandsMap[python]['H']["HRTF"] = Colors::deepSkyBlue;
	commandsMap[python]['H']["Harmonizer"] = Colors::deepSkyBlue;
	commandsMap[python]['I']["IRWinSync"] = Colors::deepSkyBlue;
	commandsMap[python]['I']["IRAverage"] = Colors::deepSkyBlue;
	commandsMap[python]['I']["IRPulse"] = Colors::deepSkyBlue;
	commandsMap[python]['I']["IRFM"] = Colors::deepSkyBlue;
	commandsMap[python]['I']["IFFTMatrix"] = Colors::deepSkyBlue;
	commandsMap[python]['I']["Input"] = Colors::deepSkyBlue;
	commandsMap[python]['I']["InputFader"] = Colors::deepSkyBlue;
	commandsMap[python]['I']["Iter"] = Colors::deepSkyBlue;
	commandsMap[python]['I']["IFFT"] = Colors::deepSkyBlue;
	commandsMap[python]['I']["Interp"] = Colors::deepSkyBlue;
	commandsMap[python]['L']["Log"] = Colors::deepSkyBlue;
	commandsMap[python]['L']["Log2"] = Colors::deepSkyBlue;
	commandsMap[python]['L']["Log10"] = Colors::deepSkyBlue;
	commandsMap[python]['L']["Linseg"] = Colors::deepSkyBlue;
	commandsMap[python]['L']["LFO"] = Colors::deepSkyBlue;
	commandsMap[python]['L']["Lorenz"] = Colors::deepSkyBlue;
	commandsMap[python]['L']["LogiMap"] = Colors::deepSkyBlue;
	commandsMap[python]['L']["Lookup"] = Colors::deepSkyBlue;
	commandsMap[python]['L']["Looper"] = Colors::deepSkyBlue;
	commandsMap[python]['L']["LinTable"] = Colors::deepSkyBlue;
	commandsMap[python]['L']["LogTable"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["MidiListener"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["MidiDispatcher"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["Mirror"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["Min"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["Max"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["MultiBand"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["Mix"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["MatrixMorph"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["MatrixPointer"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["MatrixRec"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["MatrixRecLoop"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["MidiAdsr"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["MidiDelAdsr"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["MidiCtl"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["Metro"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["MToF"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["MToT"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["Map"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["MoogLP"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["Mixer"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["MidiLinseg"] = Colors::deepSkyBlue;
	commandsMap[python]['M']["MML"] = Colors::deepSkyBlue;
	commandsMap[python]['N']["Noise"] = Colors::deepSkyBlue;
	commandsMap[python]['N']["Notein"] = Colors::deepSkyBlue;
	commandsMap[python]['N']["NextTrig"] = Colors::deepSkyBlue;
	commandsMap[python]['N']["NoteinRead"] = Colors::deepSkyBlue;
	commandsMap[python]['N']["NoteinRec"] = Colors::deepSkyBlue;
	commandsMap[python]['N']["NewTable"] = Colors::deepSkyBlue;
	commandsMap[python]['N']["NewMatrix"] = Colors::deepSkyBlue;
	commandsMap[python]['O']["OscListener"] = Colors::deepSkyBlue;
	commandsMap[python]['O']["OscDataReceive"] = Colors::deepSkyBlue;
	commandsMap[python]['O']["OscDataSend"] = Colors::deepSkyBlue;
	commandsMap[python]['O']["OscListReceive"] = Colors::deepSkyBlue;
	commandsMap[python]['O']["OscReceive"] = Colors::deepSkyBlue;
	commandsMap[python]['O']["OscSend"] = Colors::deepSkyBlue;
	commandsMap[python]['O']["Osc"] = Colors::deepSkyBlue;
	commandsMap[python]['O']["OscBank"] = Colors::deepSkyBlue;
	commandsMap[python]['O']["OscLoop"] = Colors::deepSkyBlue;
	commandsMap[python]['O']["OscTrig"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PyoBaseObject"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PyoObject"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PyoTableObject"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PyoMatrixObject"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PyoPVObject"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PeakAmp"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PolToCar"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVAnal"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVSynth"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVAddSynth"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVTranspose"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVVerb"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVGate"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVCross"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVMult"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVMorph"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVFilter"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVDelay"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVBuffer"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVShift"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVAmpMod"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVFreqMod"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVBufLoops"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVBufTabLoops"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PVMix"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["Phasor"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PinkNoise"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["Particle"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["Particle2"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["Pointer"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["Pointer2"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["Pulsar"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["Percent"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PyGuiControlSlider"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PyGuiVuMeter"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PyGuiGrapher"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PyGuiMultiSlider"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PyGuiSpectrum"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PyGuiScope"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PyGuiSndView"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PyGuiKeyboard"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["ParaTable"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PartialTable"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["Port"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["Phaser"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["PadSynthTable"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["Print"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["Pan"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["Pow"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["Programin"] = Colors::deepSkyBlue;
	commandsMap[python]['P']["Pattern"] = Colors::deepSkyBlue;
	commandsMap[python]['R']["RawMidi"] = Colors::deepSkyBlue;
	commandsMap[python]['R']["RandDur"] = Colors::deepSkyBlue;
	commandsMap[python]['R']["RandInt"] = Colors::deepSkyBlue;
	commandsMap[python]['R']["Randh"] = Colors::deepSkyBlue;
	commandsMap[python]['R']["Randi"] = Colors::deepSkyBlue;
	commandsMap[python]['R']["Record"] = Colors::deepSkyBlue;
	commandsMap[python]['R']["Resample"] = Colors::deepSkyBlue;
	commandsMap[python]['R']["RMS"] = Colors::deepSkyBlue;
	commandsMap[python]['R']["Round"] = Colors::deepSkyBlue;
	commandsMap[python]['R']["Reson"] = Colors::deepSkyBlue;
	commandsMap[python]['R']["Resonx"] = Colors::deepSkyBlue;
	commandsMap[python]['R']["RCOsc"] = Colors::deepSkyBlue;
	commandsMap[python]['R']["Rossler"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["Server"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["Spectrum"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["Scope"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["Sin"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["Sqrt"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["Sub"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["Sig"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SigTo"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SDelay"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SVF"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SVF2"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["STRev"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SmoothDelay"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["Sine"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SineLoop"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SumOsc"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SuperSaw"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["Stream"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SPan"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["Selector"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["Switch"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["Score"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SfPlayer"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SfMarkerLooper"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SfMarkerShuffler"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SampHold"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["Scale"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["Snap"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SawTable"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SyncTable"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SndTable"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SquareTable"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SharedTable"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SLMap"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SLMapFreq"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SLMapMul"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SLMapPhase"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SLMapQ"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SLMapDur"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["SLMapPan"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["Select"] = Colors::deepSkyBlue;
	commandsMap[python]['S']["Seq"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["Tan"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["Tanh"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["Tone"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["Touchin"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TableIndex"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TableMorph"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TablePut"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TableRead"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TableRec"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TableWrite"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TableScale"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TableFill"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TableScan"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["Thresh"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["Timer"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["Trig"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TrigBurst"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TrigChoice"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TrigEnv"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TrigExpseg"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TrigFunc"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TrigLinseg"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TrigRand"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TrigRandInt"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TrigTableRec"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TrigVal"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TrigXnoise"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TrigXnoiseMidi"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TranspoToCents"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TrackHold"] = Colors::deepSkyBlue;
	commandsMap[python]['T']["TriangleTable"] = Colors::deepSkyBlue;
	commandsMap[python]['U']["Urn"] = Colors::deepSkyBlue;
	commandsMap[python]['V']["Vocoder"] = Colors::deepSkyBlue;
	commandsMap[python]['V']["Vectral"] = Colors::deepSkyBlue;
	commandsMap[python]['V']["VarPort"] = Colors::deepSkyBlue;
	commandsMap[python]['V']["VoiceManager"] = Colors::deepSkyBlue;
	commandsMap[python]['W']["Wrap"] = Colors::deepSkyBlue;
	commandsMap[python]['W']["Waveguide"] = Colors::deepSkyBlue;
	commandsMap[python]['W']["WGVerb"] = Colors::deepSkyBlue;
	commandsMap[python]['W']["WinTable"] = Colors::deepSkyBlue;
	commandsMap[python]['X']["XNoise"] = Colors::deepSkyBlue;
	commandsMap[python]['X']["XNoiseDur"] = Colors::deepSkyBlue;
	commandsMap[python]['X']["XNoiseMidi"] = Colors::deepSkyBlue;
	commandsMap[python]['Y']["Yin"] = Colors::deepSkyBlue;
	commandsMap[python]['Z']["ZCross"] = Colors::deepSkyBlue;

	// push commands that won't let the editor tokenize based on digits
	commandsToNotTokenize.push_back("\\insts");
	commandsToNotTokenize.push_back("\\bar");
	commandsToNotTokenize.push_back("\\bars");
	commandsToNotTokenize.push_back("\\loop");
	commandsToNotTokenize.push_back("\\function");
	commandsToNotTokenize.push_back("\\list");
	commandsToNotTokenize.push_back("\\group");

	// a map with the color names, copied from the colorsExtendedExample
	colorNameMap["white"] = Colors::white;
	colorNameMap["gray"] = Colors::gray;
	colorNameMap["black"] = Colors::black;
	colorNameMap["red"] = Colors::red;
	colorNameMap["green"] = Colors::green;
	colorNameMap["blue"] = Colors::blue;
	colorNameMap["cyan"] = Colors::cyan;
	colorNameMap["magenta"] = Colors::magenta;
	colorNameMap["yellow"] = Colors::yellow;
	colorNameMap["aliceBlue"] = Colors::aliceBlue;
	colorNameMap["antiqueWhite"] = Colors::antiqueWhite;
	colorNameMap["aqua"] = Colors::aqua;
	colorNameMap["aquamarine"] = Colors::aquamarine;
	colorNameMap["azure"] = Colors::azure;
	colorNameMap["beige"] = Colors::beige;
	colorNameMap["bisque"] = Colors::bisque;
	colorNameMap["blanchedAlmond"] = Colors::blanchedAlmond;
	colorNameMap["blueViolet"] = Colors::blueViolet;
	colorNameMap["brown"] = Colors::brown;
	colorNameMap["burlyWood"] = Colors::burlyWood;
	colorNameMap["cadetBlue"] = Colors::cadetBlue;
	colorNameMap["chartreuse"] = Colors::chartreuse;
	colorNameMap["chocolate"] = Colors::chocolate;
	colorNameMap["coral"] = Colors::coral;
	colorNameMap["cornflowerBlue"] = Colors::cornflowerBlue;
	colorNameMap["cornsilk"] = Colors::cornsilk;
	colorNameMap["crimson"] = Colors::crimson;
	colorNameMap["darkBlue"] = Colors::darkBlue;
	colorNameMap["darkCyan"] = Colors::darkCyan;
	colorNameMap["darkGoldenRod"] = Colors::darkGoldenRod;
	colorNameMap["darkGray"] = Colors::darkGray;
	colorNameMap["darkGrey"] = Colors::darkGrey;
	colorNameMap["darkGreen"] = Colors::darkGreen;
	colorNameMap["darkKhaki"] = Colors::darkKhaki;
	colorNameMap["darkMagenta"] = Colors::darkMagenta;
	colorNameMap["darkOliveGreen"] = Colors::darkOliveGreen;
	colorNameMap["darkOrange"] = Colors::darkOrange;
	colorNameMap["darkOrchid"] = Colors::darkOrchid;
	colorNameMap["darkRed"] = Colors::darkRed;
	colorNameMap["darkSalmon"] = Colors::darkSalmon;
	colorNameMap["darkSeaGreen"] = Colors::darkSeaGreen;
	colorNameMap["darkSlateBlue"] = Colors::darkSlateBlue;
	colorNameMap["darkSlateGray"] = Colors::darkSlateGray;
	colorNameMap["darkSlateGrey"] = Colors::darkSlateGrey;
	colorNameMap["darkTurquoise"] = Colors::darkTurquoise;
	colorNameMap["darkViolet"] = Colors::darkViolet;
	colorNameMap["deepPink"] = Colors::deepPink;
	colorNameMap["deepSkyBlue"] = Colors::deepSkyBlue;
	colorNameMap["dimGray"] = Colors::dimGray;
	colorNameMap["dimGrey"] = Colors::dimGrey;
	colorNameMap["dodgerBlue"] = Colors::dodgerBlue;
	colorNameMap["fireBrick"] = Colors::fireBrick;
	colorNameMap["floralWhite"] = Colors::floralWhite;
	colorNameMap["forestGreen"] = Colors::forestGreen;
	colorNameMap["fuchsia"] = Colors::fuchsia;
	colorNameMap["gainsboro"] = Colors::gainsboro;
	colorNameMap["ghostWhite"] = Colors::ghostWhite;
	colorNameMap["gold"] = Colors::gold;
	colorNameMap["goldenRod"] = Colors::goldenRod;
	colorNameMap["grey"] = Colors::grey;
	colorNameMap["greenYellow"] = Colors::greenYellow;
	colorNameMap["honeyDew"] = Colors::honeyDew;
	colorNameMap["hotPink"] = Colors::hotPink;
	colorNameMap["indianRed "] = Colors::indianRed;
	colorNameMap["indigo "] = Colors::indigo;
	colorNameMap["ivory"] = Colors::ivory;
	colorNameMap["khaki"] = Colors::khaki;
	colorNameMap["lavender"] = Colors::lavender;
	colorNameMap["lavenderBlush"] = Colors::lavenderBlush;
	colorNameMap["lawnGreen"] = Colors::lawnGreen;
	colorNameMap["lemonChiffon"] = Colors::lemonChiffon;
	colorNameMap["lightBlue"] = Colors::lightBlue;
	colorNameMap["lightCoral"] = Colors::lightCoral;
	colorNameMap["lightCyan"] = Colors::lightCyan;
	colorNameMap["lightGoldenRodYellow"] = Colors::lightGoldenRodYellow;
	colorNameMap["lightGray"] = Colors::lightGray;
	colorNameMap["lightGrey"] = Colors::lightGrey;
	colorNameMap["lightGreen"] = Colors::lightGreen;
	colorNameMap["lightPink"] = Colors::lightPink;
	colorNameMap["lightSalmon"] = Colors::lightSalmon;
	colorNameMap["lightSeaGreen"] = Colors::lightSeaGreen;
	colorNameMap["lightSkyBlue"] = Colors::lightSkyBlue;
	colorNameMap["lightSlateGray"] = Colors::lightSlateGray;
	colorNameMap["lightSlateGrey"] = Colors::lightSlateGrey;
	colorNameMap["lightSteelBlue"] = Colors::lightSteelBlue;
	colorNameMap["lightYellow"] = Colors::lightYellow;
	colorNameMap["lime"] = Colors::lime;
	colorNameMap["limeGreen"] = Colors::limeGreen;
	colorNameMap["linen"] = Colors::linen;
	colorNameMap["maroon"] = Colors::maroon;
	colorNameMap["mediumAquaMarine"] = Colors::mediumAquaMarine;
	colorNameMap["mediumBlue"] = Colors::mediumBlue;
	colorNameMap["mediumOrchid"] = Colors::mediumOrchid;
	colorNameMap["mediumPurple"] = Colors::mediumPurple;
	colorNameMap["mediumSeaGreen"] = Colors::mediumSeaGreen;
	colorNameMap["mediumSlateBlue"] = Colors::mediumSlateBlue;
	colorNameMap["mediumSpringGreen"] = Colors::mediumSpringGreen;
	colorNameMap["mediumTurquoise"] = Colors::mediumTurquoise;
	colorNameMap["mediumVioletRed"] = Colors::mediumVioletRed;
	colorNameMap["midnightBlue"] = Colors::midnightBlue;
	colorNameMap["mintCream"] = Colors::mintCream;
	colorNameMap["mistyRose"] = Colors::mistyRose;
	colorNameMap["moccasin"] = Colors::moccasin;
	colorNameMap["navajoWhite"] = Colors::navajoWhite;
	colorNameMap["navy"] = Colors::navy;
	colorNameMap["oldLace"] = Colors::oldLace;
	colorNameMap["olive"] = Colors::olive;
	colorNameMap["oliveDrab"] = Colors::oliveDrab;
	colorNameMap["orange"] = Colors::orange;
	colorNameMap["orangeRed"] = Colors::orangeRed;
	colorNameMap["orchid"] = Colors::orchid;
	colorNameMap["paleGoldenRod"] = Colors::paleGoldenRod;
	colorNameMap["paleGreen"] = Colors::paleGreen;
	colorNameMap["paleTurquoise"] = Colors::paleTurquoise;
	colorNameMap["paleVioletRed"] = Colors::paleVioletRed;
	colorNameMap["papayaWhip"] = Colors::papayaWhip;
	colorNameMap["peachPuff"] = Colors::peachPuff;
	colorNameMap["peru"] = Colors::peru;
	colorNameMap["pink"] = Colors::pink;
	colorNameMap["plum"] = Colors::plum;
	colorNameMap["powderBlue"] = Colors::powderBlue;
	colorNameMap["purple"] = Colors::purple;
	colorNameMap["rosyBrown"] = Colors::rosyBrown;
	colorNameMap["royalBlue"] = Colors::royalBlue;
	colorNameMap["saddleBrown"] = Colors::saddleBrown;
	colorNameMap["salmon"] = Colors::salmon;
	colorNameMap["sandyBrown"] = Colors::sandyBrown;
	colorNameMap["seaGreen"] = Colors::seaGreen;
	colorNameMap["seaShell"] = Colors::seaShell;
	colorNameMap["sienna"] = Colors::sienna;
	colorNameMap["silver"] = Colors::silver;
	colorNameMap["skyBlue"] = Colors::skyBlue;
	colorNameMap["slateBlue"] = Colors::slateBlue;
	colorNameMap["slateGray"] = Colors::slateGray;
	colorNameMap["slateGrey"] = Colors::slateGrey;
	colorNameMap["snow"] = Colors::snow;
	colorNameMap["springGreen"] = Colors::springGreen;
	colorNameMap["steelBlue"] = Colors::steelBlue;
	colorNameMap["tan"] = Colors::tan;
	colorNameMap["teal"] = Colors::teal;
	colorNameMap["thistle"] = Colors::thistle;
	colorNameMap["tomato"] = Colors::tomato;
	colorNameMap["turquoise"] = Colors::turquoise;
	colorNameMap["violet"] = Colors::violet;
	colorNameMap["wheat"] = Colors::wheat;
	colorNameMap["whiteSmoke"] = Colors::whiteSmoke;
	colorNameMap["yellowGreen"] = Colors::yellowGreen;
}
