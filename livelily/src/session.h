#ifndef SESSION_H
#define SESSION_H

#include <cstdint>
#include <list>
#include <memory>
#include <map>
#include <set>
#include <utility>
#include <vector>
#include "midi.h"
#include "osc.h"
#include "serial.h"
#include <string>

// a variable of a session (x = 5), with its value as a string and its type
struct variable
{
	std::string varStr;
	int varType;
};

// the engine of LiveLily: the state of a session (and later the parser and the sequencer), without anything of the user interface
// (see doc/stage1_inventory.md, step 5)
// while the state moves here group by group, ofApp derives from Session, so the code that uses a moved member doesn't change
// when the parser has moved here too, ofApp will hold a Session instead of deriving from it
class Session
{
	public:
		virtual ~Session() = default;

		// variables (e.g. x = 5), by their names
		std::map<std::string, variable> variables;
		// the kinds of definitions, for checking that a name is free (see checkNameAvailability())
		std::map<std::string, int> structTypes = {{"bar", 1}, {"bars", 2}, {"loop", 3}, {"function", 4}, {"list", 5}};

		// lists (\list): their items by their indexes, and their indexes by their names
		std::map<int, std::list<std::string>> listMap;
		std::map<std::string, int> listIndexes;
		bool storingList;
		int lastListIndex;
		int listIndexCounter;
		bool traversingList;
		// the lines of \list.traverse, with their line numbers, and how many curly brackets are still open while they are collected
		std::vector<std::pair<int, std::string>> traverseLines;
		int traverseDepth = 0;

		// functions (\function): their indexes by their names (the Function objects are in SharedData)
		std::map<std::string, int> functionIndexes;
		bool storingFunction;
		// index of the function whose body is being stored, used only by the main thread
		// kept separate from lastFunctionIndex, which is also set when a function is called from the sequencer thread
		int storingFunctionIndex;
		// the curly bracket depth while storing a function body, the function's own brackets included
		int functionBodyDepth;
		int lastFunctionIndex;

		// the state of parsing bars (\bar), loops (\loop), and bars defined together (\bars)
		// whether there was an error in a bar, so that its melodic lines are not stored
		bool barError;
		bool parsingBar;
		bool parsingBars;
		bool parsingLoop;
		// the name of a \bars definition, until it is stored as a loop
		std::string barsName;
		// a counter of the instruments parsed, for first bars that are not complete
		int numInstsParsed;
		// the loop command sent to connected score parts
		std::string lastLoopCommand;
		// the name of the bars of \bars, and the counter of its passes, one for every bar
		std::string multiBarsName;
		int barsIterCounter;
		// whether all the instruments of \bars have the same number of bars, and that number
		bool numBarsToParseSet;
		int numBarsToParse;
		// the number of bars defined by a \time, \tempo or \beatat with more than one value separated by |, inside \bars
		// 0 if there is none, and the command that defined it, for error messages
		int barsCmdNumBars;
		std::string barsCmdName;
		int firstInstForBarsIndex;
		bool firstInstForBarsSet;

		// instruments (the Instrument objects are in SharedData)
		// groups of instruments (\group), by their names
		std::map<std::string, std::vector<std::string>> instGroups;
		// instruments merged to one staff with \merge, the first of each is the instrument whose staff they are drawn on (the host)
		// and the others are its voices, which are drawn on it (see applyMergeLayout())
		std::vector<std::vector<int>> mergeGroups;
		// the settings of \notedur and \accoffset, so that instruments created after these commands get them too
		// (each key of noteDurSettings is a \notedur method, e.g. staccato, and its value the duration as a fraction)
		std::map<std::string, float> noteDurSettings;
		bool accOffsetSet = false;
		float accOffsetCoef = 0;
		// the instrument whose line is parsed last, and the number of instruments
		std::string lastInstrument;
		int lastInstrumentIndex;
		int tempNumInstruments;
		// a warning of a command in a melodic line, which parseMelodicLine() returns if the line has no errors
		std::string melodicLineWarning;
		// the names of the notes, for parsing melodic lines
		char noteChars[8];
		// adding a natural after the same note with an accidental only in the same octave (\score.correct onoctave), or in any
		bool correctOnSameOctaveOnly;
		// the duration of a step of the step sequencer and of the x - notation of rhythm staffs (\score.seqres)
		int seqResolution;
		// dynamics from LilyPond notation (ppp to fff) as indexes, and as MIDI velocities, with a default 0 key mapped to 72
		std::vector<int> scoreDynamics = {-9, -6, -3, -1, 1, 3, 6, 9};
		std::map<int, int> mappedScoreDynamics = {{-9, 16}, {-6, 32}, {-3, 48}, {-1, 64}, {0, 72}, {1, 80}, {3, 96}, {6, 112}, {9, 127}};

		// the names of the commands of LiveLily (e.g. \merge), which can't be used as names of bars, loops, functions, lists
		// variables, instruments, groups, or OSC clients and servers, as these are called by their name with a backslash
		std::set<std::string> reservedNames;

		// input and output, through the interfaces of the platform (see midi.h, osc.h, serial.h)
		// the MIDI output, for the parser and the sequencer
		std::unique_ptr<MidiOutput> midi = makeMidiOutput();
		// OSC clients and servers (\oscclient, \oscserver), by their names, and the functions mapped to the addresses of servers
		std::map<std::string, std::shared_ptr<OscSender>> oscClients;
		std::map<std::string, std::shared_ptr<OscReceiver>> oscServers;
		std::map<std::string, std::vector<std::pair<std::string, std::string>>> oscServerFuncs;
		// the receiver of OSC messages to LiveLily itself (e.g. of the maestro, or keys typed in another LiveLily)
		std::shared_ptr<OscReceiver> oscReceiver = makeOscReceiver();
		bool oscReceiverIsSet;
		// the serial port, and the serial devices as \listserialports listed them last, which \openserialport opens by their number
		std::unique_ptr<SerialPort> serial = makeSerialPort();
		std::vector<SerialDevice> serialDeviceList;
		bool serialPortOpen;

		// score parts on other computers (\instname.sendto)
		// the IP and port of every instrument, so that instruments that send to the same server get the generic messages once
		std::map<int, std::pair<std::string, int>> instrumentOSCHostPorts;
		// time to wait for a positive/negative response from a server
		uint64_t scorePartResponseTime;
		// pairs of instrument and bar where a score part client had an error receiving the data of the bar
		std::vector<std::pair<int, int>> scorePartErrors;
		// how many times a loop is called, by its index, in case some score parts haven't received the data of a bar correctly
		std::map<int, int> partsReceivedOKCounters;
		// sending the data of the bars of \bars to score parts, one at each frame
		bool waitToSendBarDataToParts;
		bool sendBarDataToPartsBool;
		int sendBarDataToPartsCounter;
		int barDataOKFromPartsCounter;
		int numScorePartSenders;
		bool checkIfAllPartsReceivedBarData;
		uint64_t sendBarDataToPartsTimeStamp;

		// controlling LiveLily with an accelerometer (\maestro)
		std::string maestroAddress;
		std::string maestroToggleAddress;
		std::string maestroLevareAddress;
		int maestroValNdx;
		bool maestroToggleSet;
		bool receivingMaestro;
		bool maestroInitialized;
		int maestroBeatsIn;
		unsigned long maestroTimeStamp;
		float maestroValThresh;
		std::vector<float> maestroVec;
};

#endif
