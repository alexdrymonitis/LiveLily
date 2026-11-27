#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <cstdint>
#include <algorithm>
#include <numeric>
#include <cmath>
#include "instrument.h"
#include "function.h"
#include "soundfont.h"
#include "clicktrack.h"
#ifdef USEPYO
#include "PyoClass.h"
#endif

/************************ glissandi ************************/
// pairs the notes where a glissando starts with the notes where it ends (e.g. two chords), by pitch, so that every start note
// has its own glissando, used by the sequencer and the score
// returns the index of the end note of every start note (in the order of the start notes), or -1 if one of the two has no notes
// with as many notes at both ends, the lowest goes to the lowest, the second lowest to the second lowest, etc.
// with more start notes, the start notes are split into as many groups (from low to high) as the end notes, e.g. 5 to 2 is 3 and 2
// with more end notes, the start notes go to end notes spread from the lowest to the highest end note (a single note goes to the closest)
inline std::vector<int> glissandoPairs(const std::vector<float>& start, const std::vector<float>& end)
{
	size_t n = start.size(), m = end.size();
	std::vector<int> pairs(n, -1);
	if (n == 0 || m == 0) return pairs;
	std::vector<int> startOrder(n), endOrder(m);
	std::iota(startOrder.begin(), startOrder.end(), 0);
	std::iota(endOrder.begin(), endOrder.end(), 0);
	std::stable_sort(startOrder.begin(), startOrder.end(), [&](int a, int b) { return start[a] < start[b]; });
	std::stable_sort(endOrder.begin(), endOrder.end(), [&](int a, int b) { return end[a] < end[b]; });
	for (size_t r = 0; r < n; r++) {
		size_t endRank;
		if (n == m) {
			endRank = r;
		}
		else if (n > m) {
			endRank = (r * m) / n;
		}
		else if (n == 1) {
			endRank = 0;
			for (size_t k = 1; k < m; k++) {
				if (std::abs(end[endOrder[k]] - start[0]) < std::abs(end[endOrder[endRank]] - start[0])) endRank = k;
			}
		}
		else {
			endRank = (size_t)std::lround((double)r * (double)(m - 1) / (double)(n - 1));
		}
		pairs[startOrder[r]] = endOrder[endRank];
	}
	return pairs;
}

/************************ ties across bars ************************/
// the tie at the end of bar fromBar of an instrument, followed by bar toBar
// returns 0 if there is no tie, 1 if the tie connects to the same notes at the start of toBar
// and 2 if the first notes of toBar are not the same, in which case the tie is ignored
inline int tieAcrossBars(Instrument& inst, int fromBar, int toBar)
{
	if (fromBar < 0 || toBar < 0) return 0;
	auto tiesIt = inst.tieIndexes.find(fromBar);
	if (tiesIt == inst.tieIndexes.end() || tiesIt->second.empty() || tiesIt->second.back() < 0) return 0;
	auto fromIt = inst.notes.find(fromBar);
	auto toIt = inst.notes.find(toBar);
	if (fromIt == inst.notes.end() || toIt == inst.notes.end() || fromIt->second.empty() || toIt->second.empty()) return 2;
	std::vector<float> lastNotes = fromIt->second.back();
	std::vector<float> firstNotes = toIt->second.front();
	std::sort(lastNotes.begin(), lastNotes.end());
	std::sort(firstNotes.begin(), firstNotes.end());
	return (!lastNotes.empty() && lastNotes[0] >= 0 && lastNotes == firstNotes) ? 1 : 2;
}

/************************ string helpers ************************/
// shared by the editor, the main app and the MusicXML loader

// whether a string starts with another string
inline bool startsWith(const std::string& a, const std::string& b)
{
	if (a.size() < b.size()) return false;
	return a.compare(0, b.size(), b) == 0;
}

// whether a string ends with another string
inline bool endsWith(const std::string& a, const std::string& b)
{
	if (a.size() < b.size()) return false;
	return a.compare(a.size() - b.size(), b.size(), b) == 0;
}

// replace all occurrences of a in str with b
// the search starts over from the beginning after every replacement
// so e.g. replacing two spaces with one collapses any number of spaces to one
// b must not contain a, otherwise the replacement never ends
inline std::string replaceCharInStr(std::string str, const std::string& a, const std::string& b)
{
	if (a.empty()) return str;
	auto it = str.find(a);
	while (it != std::string::npos) {
		str.replace(it, a.size(), b);
		it = str.find(a);
	}
	return str;
}

/************************* shared data **************************/
// the data shared between the main app (App) and the sequencer thread

// size of cosine table to control the transparency of the beat visualization rectangle
#define BEATVIZBRIGHTNESS 255

// vectors and variables that need to be shared between main and threaded classes
// the state of the view is in ViewState (view_state.h), held by the front end
struct SharedData
{
	// a map of Instruments()
	std::map<int, Instrument> instruments;
	// the synthesizer for instruments that play SoundFonts (see \instname.sndfont)
	SoundFontPlayer soundFonts;
	// the click track on the beats of \beatat (see \clicktrack)
	ClickTrack clickTrack;
	// maps of string and int to easily store data based on strings
	// but iterate over the data in the sequencer and score renderer fast based on ints
	std::map<std::string, int> instrumentIndexes;
	std::map<int, int> instrumentIndexesOrdered; // for sending to parts in specific order
	std::map<std::string, int> barsIndexes;
	std::map<std::string, int> loopsIndexes;
	// since string keys are sorted, we need a way to retrieve them in arbirtary order
	// e.g. a bar named "5" stored first, will be moved after a bar named "4" that is stored later on
	// the map below is ordered, but we store the index as a key, which is stored with an incrementing value anyway
	std::map<int, std::string> loopsOrdered;
	// same goes for bars
	std::map<int, std::string> barsOrdered;
	// the map below stores the bar indexes for every defined loop
	std::map<int, std::vector<int>> loopData;
	// the map below stores the strings of each defined bar, since these can be edited or deleted in the editor.
	// These can be used elsewhere, like sent to an AI model that is trained on LiveLily files
	// the key is the bar name and the value is another string assembled by the strings of the bar lines
	// separated with newline characters
	std::map<std::string, std::string> barLines;
	// the map below keeps track of how many variants of each loop we create so we can use this as a name extension
	std::map<int, int> loopsVariants;

	// the vector below will store unique OSC clients, one for each server
	// additional clients that send to the same server will be ignored
	// as this is intended for generic messages, like storeNotes()
	// it is defined here because the sequencer needs access to it too
	std::vector<int> grouppedOSCClients;
#ifdef USEPYO
	// the Pyo object is created here so that the sequencer has access to it
	Pyo pyo;
#endif

	int numInstruments{};

	// articulation strings so each program can treat them differently
	std::string articulSyms[8];
	// beat counter used in case we need to send it over OSC or for possible other reasons
	int beatCounter{};
	// animation variables
	bool animate{};
	bool setAnimation{};
	// the variable below is used by the serquencer to iterate over one loop
	unsigned thisBarIndex{};
	int loopIndex{};
	int tempLoopIndex{};
	int barCounter{}; // used for visualization on the score
	// two different tempo placeholders
	// one for the running sequencer (tempo)
	// and one to hold the tempo in ms without the conversion to the global minimum duration
	std::map<int, double> tempo;
	std::map<int, double> tempoMs;
	std::map<int, int> BPMTempi;
	std::map<int, int> BPMMultiplier;
	std::map<int, bool> beatAtDifferentThanDivisor;
	std::map<int, std::vector<int>> beatAtValues;
	unsigned beatAtValCounter{};
	std::map<int, int> tempoBaseForScore;
	std::map<int, bool> BPMDisplayHasDot;

	std::map<int, Function> functions;
	// guards the functions map above and Session::functionIndexes and Session::lastFunctionIndex
	// as these are accessed both from the main thread and from the sequencer thread
	// never call parseString() or parseCommand() while holding it, as these may lock it again
	std::mutex functionsMutex;
	// held by the sequencer thread for every step, and by the main thread while adding or removing instruments
	// so that the sequencer never goes through the instruments while one is added or removed
	std::recursive_mutex instrumentsMutex;

	std::map<int, int> numBeats;

	std::map<int, int> numerator;
	std::map<int, int> denominator;

	// MIDI clock
	unsigned long PPQN{};
	std::map<int, uint64_t> PPQNPerUs;
	unsigned long PPQNCounter{};
	uint64_t PPQNTimeStamp{};
};

#endif
