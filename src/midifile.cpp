#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstdint>
#include "midifile.h"

// all positions and durations after reading the file are in units of 1/48 of a quarter note
// so that 64ths (3), 32nds (6), 16ths (12), 16th-note triplets (8) and eighth-note triplets (16) are whole numbers
static const int UNITS_PER_QUARTER = 48;

// the parameters that resolve the ambiguity between the time a note sounds and its written value
// a note that sounds for at least this part of the time until the next note is a normal note that fills that time
static const double LEGATO_RATIO = 0.75;
// a note followed by a silence of at least this many units (5/6 of a quarter, about a beat) is a shorter note and a rest
// shorter silences are articulation, so the note fills the time until the next note
static const double REST_MIN_SILENCE = 40;
// a note that fills the time until the next note, but sounds for less than this part of it, is marked staccato
static const double STACCATO_RATIO = 0.6;
// a note that starts this many units before a beat belongs to that beat (performed notes are often a little early)
static const double ONSET_TOLERANCE = 4;
// the largest distance from a grid (in units) for which the notes of a beat are considered already quantized
static const double EXACT_TOLERANCE = 0.51;

// the note values that can be written, in units, with their LiveLily durations
static const std::vector<std::pair<int, std::string>> NOTE_VALUES = {
	{288, "1."}, {192, "1"}, {144, "2."}, {96, "2"}, {72, "4."}, {48, "4"}, {36, "8."},
	{24, "8"}, {18, "16."}, {12, "16"}, {9, "32."}, {6, "32"}, {3, "64"}
};

/********************* MIDIFileLoader class *********************/
//--------------------------------------------------------------
MIDIFileLoader::MIDIFileLoader(const std::string& tab)
{
	tabStr = tab;
	division = 480;
}

//--------------------------------------------------------------
std::string MIDIFileLoader::getError()
{
	return error;
}

//--------------------------------------------------------------
double MIDIFileLoader::unitsOf(long ticks)
{
	return (double)ticks * (double)UNITS_PER_QUARTER / (double)division;
}

//--------------------------------------------------------------
// big-endian integers and variable-length quantities of the MIDI file format
// they return 0 past the end of the data, and set ok to false
static uint32_t midiReadBE(const std::string& data, size_t pos, int numBytes, bool& ok)
{
	if (pos + numBytes > data.size()) {
		ok = false;
		return 0;
	}
	uint32_t value = 0;
	for (int i = 0; i < numBytes; i++) value = (value << 8) | (unsigned char)data[pos+i];
	return value;
}

static uint32_t midiReadVarLen(const std::string& data, size_t& pos, size_t end, bool& ok)
{
	uint32_t value = 0;
	for (int i = 0; i < 4; i++) {
		if (pos >= end) {
			ok = false;
			return 0;
		}
		unsigned char c = data[pos++];
		value = (value << 7) | (c & 0x7F);
		if (!(c & 0x80)) return value;
	}
	return value;
}

//--------------------------------------------------------------
bool MIDIFileLoader::parse(const std::string& data)
{
	bool ok = true;
	if (data.size() < 14 || data.compare(0, 4, "MThd") != 0) {
		error = "not a MIDI file";
		return false;
	}
	uint32_t headerLength = midiReadBE(data, 4, 4, ok);
	int format = midiReadBE(data, 8, 2, ok);
	int numTracks = midiReadBE(data, 10, 2, ok);
	int div = midiReadBE(data, 12, 2, ok);
	if (div & 0x8000) {
		error = "MIDI files with SMPTE time are not supported";
		return false;
	}
	if (format > 1) {
		error = "format 2 MIDI files are not supported";
		return false;
	}
	if (div == 0) {
		error = "damaged MIDI file";
		return false;
	}
	division = div;
	// the notes of every track, by channel, and the names of the tracks
	struct TrackData {
		std::string name;
		std::string instrumentName;
		std::map<int, std::vector<Note>> channels;
		std::map<int, int> programs;
	};
	std::vector<TrackData> tracks;
	size_t pos = 8 + headerLength;
	for (int t = 0; t < numTracks && pos + 8 <= data.size(); ) {
		uint32_t chunkLength = midiReadBE(data, pos + 4, 4, ok);
		bool isTrack = (data.compare(pos, 4, "MTrk") == 0);
		size_t start = pos + 8;
		size_t end = std::min(data.size(), start + (size_t)chunkLength);
		pos = start + chunkLength;
		// chunks other than tracks are skipped, as the format allows
		if (!isTrack) continue;
		// a track that goes past the end of the file has lost its last events (e.g. note offs), so the file is not converted
		if (start + chunkLength > data.size()) {
			error = "damaged MIDI file";
			return false;
		}
		t++;
		TrackData track;
		// the note that is sounding on every channel and pitch, as an index to the notes of its channel
		std::map<int, size_t> sounding;
		size_t p = start;
		long tick = 0;
		unsigned char runningStatus = 0;
		while (p < end) {
			bool okLocal = true;
			tick += midiReadVarLen(data, p, end, okLocal);
			if (!okLocal || p >= end) break;
			unsigned char byte = data[p];
			if (byte == 0xFF) {
				// meta event: type, length and data
				if (p + 2 > end) break;
				unsigned char type = data[p+1];
				p += 2;
				uint32_t length = midiReadVarLen(data, p, end, okLocal);
				if (!okLocal || p + length > end) break;
				std::string payload = data.substr(p, length);
				p += length;
				if (type == 0x03 && track.name.empty()) track.name = payload;
				else if (type == 0x04 && track.instrumentName.empty()) track.instrumentName = payload;
				else if (type == 0x51 && length == 3) {
					tempi[tick] = ((unsigned char)payload[0] << 16) | ((unsigned char)payload[1] << 8) | (unsigned char)payload[2];
				}
				else if (type == 0x58 && length >= 2) {
					int numerator = (unsigned char)payload[0];
					int denominatorPower = (unsigned char)payload[1];
					if (numerator > 0 && denominatorPower <= 6) timeSignatures[tick] = std::make_pair(numerator, 1 << denominatorPower);
				}
				else if (type == 0x59 && length >= 1) {
					keySignatures[tick] = (signed char)payload[0];
				}
				else if (type == 0x2F) break;
				// meta and system exclusive events cancel the running status
				runningStatus = 0;
				continue;
			}
			if (byte == 0xF0 || byte == 0xF7) {
				p++;
				uint32_t length = midiReadVarLen(data, p, end, okLocal);
				if (!okLocal) break;
				p += length;
				runningStatus = 0;
				continue;
			}
			if (byte & 0x80) {
				if (byte >= 0xF0) break; // system common and real-time messages don't belong in a MIDI file
				runningStatus = byte;
				p++;
			}
			else if (runningStatus == 0) {
				break;
			}
			int type = runningStatus & 0xF0;
			int channel = runningStatus & 0x0F;
			int numDataBytes = (type == 0xC0 || type == 0xD0) ? 1 : 2;
			if (p + numDataBytes > end) break;
			int data1 = (unsigned char)data[p];
			int data2 = (numDataBytes == 2 ? (unsigned char)data[p+1] : 0);
			p += numDataBytes;
			if (type == 0xC0 && track.programs.find(channel) == track.programs.end()) track.programs[channel] = data1;
			int key = channel * 128 + data1;
			bool noteOn = (type == 0x90 && data2 > 0);
			bool noteOff = (type == 0x80 || (type == 0x90 && data2 == 0));
			if (noteOn || noteOff) {
				// a note that is still sounding ends here, also when the same note starts again before its note off
				auto it = sounding.find(key);
				if (it != sounding.end()) {
					track.channels[channel][it->second].end = tick;
					sounding.erase(it);
				}
				if (noteOn) {
					track.channels[channel].push_back({tick, -1, data1});
					sounding[key] = track.channels[channel].size() - 1;
				}
			}
		}
		// notes without a note off end at the end of the track
		for (auto& s : sounding) {
			track.channels[s.first / 128][s.second].end = tick;
		}
		tracks.push_back(track);
	}
	// one staff for every track with notes, or for every channel of a track with notes on several channels
	for (size_t t = 0; t < tracks.size(); t++) {
		std::string base = tracks[t].name;
		if (base.empty()) base = tracks[t].instrumentName;
		for (auto& channel : tracks[t].channels) {
			if (channel.second.empty()) continue;
			Staff staff;
			if (tracks[t].channels.size() == 1) {
				staff.name = (base.empty() ? "Track" + std::to_string(t+1) : base);
			}
			else {
				staff.name = base + "Ch" + std::to_string(channel.first + 1);
			}
			staff.notes = channel.second;
			auto programIt = tracks[t].programs.find(channel.first);
			if (programIt != tracks[t].programs.end()) staff.program = programIt->second;
			// channel 10 (9 counting from 0) is for drums in General MIDI
			staff.drums = (channel.first == 9);
			std::sort(staff.notes.begin(), staff.notes.end(), [](const Note& a, const Note& b) {
				return a.start < b.start || (a.start == b.start && a.pitch < b.pitch);
			});
			staves.push_back(staff);
		}
	}
	if (staves.empty()) {
		error = "the MIDI file has no notes";
		return false;
	}
	splitPianoStaves();
	// instrument names must be unique, and they are written with a backslash, so they are made of letters and digits
	std::set<std::string> names;
	for (size_t i = 0; i < staves.size(); i++) {
		std::string name = sanitizeName(staves[i].name, (int)i + 1);
		std::string unique = name;
		for (int n = 2; names.count(unique); n++) unique = name + std::to_string(n);
		names.insert(unique);
		staves[i].name = unique;
	}
	return true;
}

//--------------------------------------------------------------
// how well note starts line up with a pulse of a period in ticks, from 0 (not at all) to 1 (all on the pulse)
// and the phase of the pulse, in ticks
static std::pair<double, double> pulseFit(const std::vector<long>& onsets, double period)
{
	double c = 0, s = 0;
	for (long o : onsets) {
		double angle = 2 * M_PI * (double)o / period;
		c += std::cos(angle);
		s += std::sin(angle);
	}
	double phase = std::atan2(s, c) / (2 * M_PI) * period;
	if (phase < 0) phase += period;
	return std::make_pair(std::hypot(c, s) / onsets.size(), phase);
}

//--------------------------------------------------------------
std::string MIDIFileLoader::fitToPulse()
{
	// tempo and time signature changes are at positions of the file's own timing, so files with them are left as they are
	if (tempi.size() > 1 || timeSignatures.size() > 1) return "";
	// the start of every chord, and how many notes start there, which tells where the beats are
	std::map<long, int> notesAt;
	for (const Staff& staff : staves) {
		for (const Note& note : staff.notes) notesAt[note.start]++;
	}
	if (notesAt.size() < 16) return "";
	std::vector<long> onsets;
	for (auto& n : notesAt) onsets.push_back(n.first);
	// a file whose notes line up with the subdivisions of its own beat (16ths, 32nds, or triplets) is fine
	if (fitsOwnBeat(onsets)) return "";
	// the period that the notes line up with best, first roughly on the start of the file, and then precisely on all of it
	// as a small error in the period adds up over a long file
	std::vector<long> firstOnsets(onsets.begin(), onsets.begin() + std::min(onsets.size(), (size_t)300));
	double bestFit = 0, bestPeriod = 0;
	std::vector<std::pair<double, double>> fits;
	for (double period = division / 8.0; period <= division * 1.5; period *= 1.002) {
		double fit = pulseFit(firstOnsets, period).first;
		fits.push_back({period, fit});
		bestFit = std::max(bestFit, fit);
	}
	if (bestFit < 0.8) return "";
	// halves and thirds of the pulse fit as well as the pulse itself, so the pulse is the longest period that fits almost as well
	for (auto& f : fits) {
		if (f.second >= 0.9 * bestFit) bestPeriod = f.first;
	}
	std::vector<long> someOnsets(onsets.begin(), onsets.begin() + std::min(onsets.size(), (size_t)5000));
	double period = bestPeriod;
	double fit = 0;
	for (double p = bestPeriod * 0.995; p <= bestPeriod * 1.005; p += bestPeriod * 0.00002) {
		double f = pulseFit(someOnsets, p).first;
		if (f > fit) {
			fit = f;
			period = p;
		}
	}
	if (fit < 0.8) return "";
	double phase = pulseFit(someOnsets, period).second;
	// the note value of the pulse: a 16th or an eighth, whichever gives a tempo closer to 100 BPM
	int tempo = (tempi.empty() ? 500000 : tempi.begin()->second);
	double secondsPerTick = tempo / 1000000.0 / division;
	int pulsesPerQuarter = 4;
	double bpm16 = 60.0 / (4 * period * secondsPerTick);
	double bpm8 = 60.0 / (2 * period * secondsPerTick);
	if (std::fabs(std::log(bpm8 / 100)) < std::fabs(std::log(bpm16 / 100))) pulsesPerQuarter = 2;
	// the beats are on the pulses where most notes start together (chords, and bass notes under melodies)
	std::vector<int> weights(pulsesPerQuarter, 0);
	auto pulseIndex = [&](long tick) { return (long)std::lround((tick - phase) / period); };
	for (auto& n : notesAt) {
		long k = pulseIndex(n.first);
		weights[((k % pulsesPerQuarter) + pulsesPerQuarter) % pulsesPerQuarter] += n.second;
	}
	int beatOffset = (int)(std::max_element(weights.begin(), weights.end()) - weights.begin());
	// the first bar starts at the beat at or before the first note
	long firstPulse = pulseIndex(onsets.front());
	long startPulse = firstPulse - ((((firstPulse - beatOffset) % pulsesPerQuarter) + pulsesPerQuarter) % pulsesPerQuarter);
	double origin = phase + startPulse * period;
	// one pulse becomes a 16th or an eighth of the file's own quarter note
	double scale = (division / (double)pulsesPerQuarter) / period;
	for (Staff& staff : staves) {
		for (Note& note : staff.notes) {
			note.start = std::max(0L, std::lround((note.start - origin) * scale));
			note.end = std::max(note.start, std::lround((note.end - origin) * scale));
		}
	}
	int newTempo = (int)std::lround(tempo * pulsesPerQuarter * period / division);
	tempi.clear();
	tempi[0] = newTempo;
	if (!timeSignatures.empty()) {
		std::pair<int, int> ts = timeSignatures.begin()->second;
		timeSignatures.clear();
		timeSignatures[0] = ts;
	}
	keySignatures.clear();
	std::string pulseName = (pulsesPerQuarter == 4 ? "16th" : "eighth");
	return "% the timing of this MIDI file doesn't follow its own beat, so its notes were placed on the steady pulse they follow, as " +
		pulseName + " notes at " + std::to_string((int)std::lround(60000000.0 / newTempo)) + " BPM, and the time signature might need changing";
}

//--------------------------------------------------------------
void MIDIFileLoader::splitPianoStaves()
{
	// a piano part on one track has the notes of both hands, which are written on two staves, split at middle C
	// a staff is split if it is a piano or another keyboard (by its name or its General MIDI program), or if it has no program
	// and if it has many notes on both sides of middle C
	static const std::vector<std::string> keyboardNames = {"piano", "pno", "klavier", "keyboard", "keys", "clavier", "harpsichord", "cembalo", "celesta"};
	pianoGroups.clear();
	std::vector<Staff> result;
	for (Staff& staff : staves) {
		std::string lowerName;
		for (char c : staff.name) lowerName += tolower((unsigned char)c);
		bool keyboardName = false;
		for (const std::string& n : keyboardNames) {
			if (lowerName.find(n) != std::string::npos) keyboardName = true;
		}
		bool keyboard = !staff.drums && (keyboardName || (staff.program >= 0 && staff.program <= 7) || staff.program < 0);
		size_t below = 0;
		for (const Note& note : staff.notes) {
			if (note.pitch < 60) below++;
		}
		size_t above = staff.notes.size() - below;
		bool bothSides = (staff.notes.size() >= 8 && below >= 0.15 * staff.notes.size() && above >= 0.15 * staff.notes.size());
		if (!keyboard || !bothSides) {
			result.push_back(staff);
			continue;
		}
		Staff upper = staff, lower = staff;
		upper.notes.clear();
		lower.notes.clear();
		for (const Note& note : staff.notes) {
			if (note.pitch >= 60) upper.notes.push_back(note);
			else lower.notes.push_back(note);
		}
		upper.name = staff.name + "1";
		lower.name = staff.name + "2";
		pianoGroups.push_back(std::make_pair(result.size(), result.size() + 1));
		result.push_back(upper);
		result.push_back(lower);
	}
	staves = result;
}

//--------------------------------------------------------------
bool MIDIFileLoader::fitsOwnBeat(const std::vector<long>& onsets)
{
	// 16ths, 32nds, or triplets of the file's quarter note
	for (double subdivision : {4.0, 8.0, 3.0, 6.0, 12.0}) {
		if (pulseFit(onsets, division / subdivision).first >= 0.5) return true;
	}
	return false;
}

//--------------------------------------------------------------
int MIDIFileLoader::averageTempo(int from, int to)
{
	// the tempo changes are by tick, and positions are in units, so the time of every tempo is weighted by its length in units
	if (to <= from) return tempoAt(from);
	double total = 0;
	int pos = from;
	int tempo = tempoAt(from);
	for (auto& t : tempi) {
		int at = (int)std::lround(unitsOf(t.first));
		if (at <= from) continue;
		if (at >= to) break;
		total += (double)tempo * (at - pos);
		pos = at;
		tempo = t.second;
	}
	total += (double)tempo * (to - pos);
	return (int)std::lround(total / (to - from));
}

//--------------------------------------------------------------
std::string MIDIFileLoader::trackBeats()
{
	// the start of every chord, and how many notes start there
	std::map<long, int> notesAt;
	for (const Staff& staff : staves) {
		for (const Note& note : staff.notes) notesAt[note.start]++;
	}
	if (notesAt.size() < 16) return "";
	std::vector<long> onsets;
	for (auto& n : notesAt) onsets.push_back(n.first);
	if (fitsOwnBeat(onsets)) return "";
	// the time of every tick in seconds, from the tempo changes of the file
	std::vector<std::pair<long, double>> tempoSegments; // tick, seconds at that tick
	std::vector<int> segmentTempi;
	{
		long tick = 0;
		double seconds = 0;
		int tempo = 500000;
		for (auto& t : tempi) {
			seconds += (double)(t.first - tick) * tempo / 1000000.0 / division;
			tick = t.first;
			tempo = t.second;
			tempoSegments.push_back({tick, seconds});
			segmentTempi.push_back(tempo);
		}
		if (tempoSegments.empty() || tempoSegments.front().first > 0) {
			tempoSegments.insert(tempoSegments.begin(), {0, 0.0});
			segmentTempi.insert(segmentTempi.begin(), 500000);
		}
	}
	auto secondsOf = [&](long tick) {
		size_t s = 0;
		while (s + 1 < tempoSegments.size() && tempoSegments[s+1].first <= tick) s++;
		return tempoSegments[s].second + (double)(tick - tempoSegments[s].first) * segmentTempi[s] / 1000000.0 / division;
	};
	// the onset strength, every 10 ms, smoothed a little and normalized
	// beats are marked by long notes and by bass notes more than by short and high notes (e.g. a melody in 16ths)
	// so every note counts by the time it sounds (up to a second), and up to twice as much if it is low
	const double frameRate = 100;
	double lastSecond = secondsOf(onsets.back());
	int numFrames = (int)(lastSecond * frameRate) + 300;
	if (numFrames > 100 * 60 * 60) return ""; // longer than an hour
	std::vector<double> envelope(numFrames, 0);
	for (const Staff& staff : staves) {
		for (const Note& note : staff.notes) {
			double start = secondsOf(note.start);
			double weight = std::min(1.0, secondsOf(note.end) - start) * (1.0 + std::max(0.0, std::min(1.0, (72 - note.pitch) / 24.0)));
			int frame = (int)std::lround(start * frameRate);
			for (int k = -6; k <= 6; k++) {
				if (frame + k >= 0 && frame + k < numFrames) envelope[frame + k] += weight * std::exp(-0.5 * (k * k) / 4.0);
			}
		}
	}
	double mean = 0, var = 0;
	for (double e : envelope) mean += e;
	mean /= numFrames;
	for (double e : envelope) var += (e - mean) * (e - mean);
	double deviation = std::sqrt(var / numFrames);
	if (deviation <= 0) return "";
	for (double& e : envelope) e /= deviation;
	auto autocorrelation = [&](int lag, int from, int to) {
		double r = 0;
		for (int n = std::max(0, from); n < std::min(to, numFrames - lag); n++) r += envelope[n] * envelope[n + lag];
		return r;
	};
	// the beat period of the whole file, from the autocorrelation of the onset strength, preferring periods around 100 BPM (60 frames)
	int period = 60;
	double bestScore = -1;
	for (int lag = 30; lag <= 150; lag++) {
		double octaves = std::log2(lag / 60.0);
		double score = autocorrelation(lag, 0, numFrames) * std::exp(-0.5 * octaves * octaves);
		if (score > bestScore) {
			bestScore = score;
			period = lag;
		}
	}
	// the tempo of a performance changes, so the period is also found every half second, in a window of six seconds around it
	// preferring periods close to the period of the whole file, so that it doesn't jump to half or double the tempo
	const int hop = 50;
	std::vector<int> localPeriods;
	for (int center = 0; center < numFrames; center += hop) {
		int best = period;
		double bestLocal = -1e300;
		for (int lag = std::max(30, (int)(period / 1.6)); lag <= std::min(150, (int)(period * 1.6)); lag++) {
			double octaves = std::log2((double)lag / period) / 0.5;
			double score = autocorrelation(lag, center - 300, center + 300) * std::exp(-0.5 * octaves * octaves);
			if (score > bestLocal) {
				bestLocal = score;
				best = lag;
			}
		}
		localPeriods.push_back(best);
	}
	// smoothed with the median of every five periods
	std::vector<int> smoothPeriods(localPeriods.size());
	for (size_t i = 0; i < localPeriods.size(); i++) {
		std::vector<int> near(localPeriods.begin() + (i >= 2 ? i - 2 : 0), localPeriods.begin() + std::min(localPeriods.size(), i + 3));
		std::sort(near.begin(), near.end());
		smoothPeriods[i] = near[near.size() / 2];
	}
	auto periodAt = [&](int frame) { return smoothPeriods[std::min(smoothPeriods.size() - 1, (size_t)(frame / hop))]; };
	// the beats, with dynamic programming: every frame gets the best score of a sequence of beats ending on it
	// which is the onset strength at the frame, plus the best score of a previous beat, minus a penalty that grows
	// with how much the distance from it differs from the local period (see Ellis, Beat Tracking by Dynamic Programming, 2007)
	const double tightness = 100;
	std::vector<double> score(numFrames, 0);
	std::vector<int> previous(numFrames, -1);
	for (int n = 0; n < numFrames; n++) {
		int localPeriod = periodAt(n);
		double best = 0;
		int bestPrev = -1;
		for (int p = n - 2 * localPeriod; p <= n - localPeriod / 2; p++) {
			if (p < 0) continue;
			double distance = std::log((double)(n - p) / localPeriod);
			double s = score[p] - tightness * distance * distance;
			if (bestPrev < 0 || s > best) {
				best = s;
				bestPrev = p;
			}
		}
		score[n] = envelope[n] + (bestPrev >= 0 ? best : 0);
		previous[n] = bestPrev;
	}
	// the last beat is the best one around the last note, and the beats before it are followed back
	int lastFrame = (int)std::lround(lastSecond * frameRate);
	int end = lastFrame;
	for (int n = std::max(0, lastFrame - periodAt(lastFrame)); n <= std::min(numFrames - 1, lastFrame + periodAt(lastFrame) / 2); n++) {
		if (score[n] > score[end]) end = n;
	}
	std::vector<double> beats;
	for (int n = end; n >= 0; n = previous[n]) beats.insert(beats.begin(), n / frameRate);
	if (beats.size() < 4) return "";
	// beats before the first one, if the first note is earlier, and after the last one, so that all the notes are inside the beats
	double firstSecond = secondsOf(onsets.front());
	while (beats.front() > firstSecond + 0.05) beats.insert(beats.begin(), beats.front() - (beats[1] - beats[0]));
	double lastNoteEnd = lastSecond;
	for (const Staff& staff : staves) {
		for (const Note& note : staff.notes) lastNoteEnd = std::max(lastNoteEnd, secondsOf(note.end));
	}
	while (beats.back() < lastNoteEnd + 0.01) beats.push_back(beats.back() + (beats.back() - beats[beats.size() - 2]));
	// the first bar starts at the beat at or before the first note
	size_t firstBeat = 0;
	while (firstBeat + 1 < beats.size() && beats[firstBeat + 1] <= firstSecond + 0.05) firstBeat++;
	// the position of a time in quarter notes from the first beat, between the beats around it
	auto quartersOf = [&](double seconds) {
		size_t k = firstBeat;
		while (k + 2 < beats.size() && beats[k + 1] <= seconds) k++;
		double length = beats[k + 1] - beats[k];
		return (double)(k - firstBeat) + (seconds - beats[k]) / length;
	};
	for (Staff& staff : staves) {
		for (Note& note : staff.notes) {
			double start = quartersOf(secondsOf(note.start));
			double endQ = quartersOf(secondsOf(note.end));
			note.start = std::max(0L, std::lround(start * division));
			note.end = std::max(note.start, std::lround(endQ * division));
		}
	}
	// the tempo of every beat
	tempi.clear();
	for (size_t k = firstBeat; k + 1 < beats.size(); k++) {
		tempi[(long)(k - firstBeat) * division] = (int)std::lround((beats[k + 1] - beats[k]) * 1000000.0);
	}
	if (!timeSignatures.empty()) {
		std::pair<int, int> ts = timeSignatures.begin()->second;
		timeSignatures.clear();
		timeSignatures[0] = ts;
	}
	if (!keySignatures.empty()) {
		int key = keySignatures.begin()->second;
		keySignatures.clear();
		keySignatures[0] = key;
	}
	beatTracked = true;
	// the range of tempi of the detected beats, leaving out the slowest and fastest tenth, so that a single odd beat doesn't show
	std::vector<double> beatLengths;
	for (size_t k = firstBeat; k + 1 < beats.size(); k++) beatLengths.push_back(beats[k + 1] - beats[k]);
	std::sort(beatLengths.begin(), beatLengths.end());
	double fastest = beatLengths[beatLengths.size() / 10];
	double slowest = beatLengths[beatLengths.size() - 1 - beatLengths.size() / 10];
	std::string range = std::to_string((int)std::lround(60.0 / slowest));
	if (std::lround(60.0 / slowest) != std::lround(60.0 / fastest)) range = "between " + range + " and " + std::to_string((int)std::lround(60.0 / fastest));
	else range = "at " + range;
	return "% this MIDI file was played without a steady beat, so its beats were detected (" + range +
		" BPM), and the time signature might need changing";
}

//--------------------------------------------------------------
std::string MIDIFileLoader::sanitizeName(const std::string& name, int index)
{
	std::string out;
	for (char c : name) {
		if (isalnum((unsigned char)c)) out += c;
	}
	if (out.empty()) out = "Track" + std::to_string(index);
	if (isdigit((unsigned char)out[0])) out = "T" + out;
	// LiveLily commands are written in lower case (e.g. \bar, \time), so a capital first letter avoids clashes with them
	out[0] = toupper((unsigned char)out[0]);
	return out;
}

//--------------------------------------------------------------
void MIDIFileLoader::makeBars(int lastEnd)
{
	// the bars follow the time signatures of the file, with 4/4 until the first one
	// a time signature that changes in the middle of a bar takes effect at the next bar
	bars.clear();
	int pos = 0;
	int numerator = 4, denominator = 4;
	auto tsIt = timeSignatures.begin();
	while ((pos < lastEnd || bars.empty()) && bars.size() < 100000) {
		while (tsIt != timeSignatures.end() && unitsOf(tsIt->first) <= pos + 1) {
			numerator = tsIt->second.first;
			denominator = std::min(tsIt->second.second, 64);
			++tsIt;
		}
		Bar bar;
		bar.start = pos;
		bar.numerator = numerator;
		bar.denominator = denominator;
		bar.length = numerator * (4 * UNITS_PER_QUARTER) / denominator;
		bool compound = (denominator >= 8 && numerator % 3 == 0 && numerator > 3);
		if (denominator <= 4) bar.spanLength = UNITS_PER_QUARTER;
		else if (compound) bar.spanLength = 3 * (4 * UNITS_PER_QUARTER) / denominator;
		else bar.spanLength = (4 * UNITS_PER_QUARTER) / denominator;
		bars.push_back(bar);
		pos += bar.length;
	}
}

//--------------------------------------------------------------
int MIDIFileLoader::barOf(int pos)
{
	// the last bar that starts at or before the position
	auto it = std::upper_bound(bars.begin(), bars.end(), pos, [](int p, const Bar& b) { return p < b.start; });
	if (it == bars.begin()) return 0;
	return (int)(it - bars.begin()) - 1;
}

//--------------------------------------------------------------
int MIDIFileLoader::spanStartOf(int pos)
{
	const Bar& bar = bars[barOf(pos)];
	int rel = std::max(0, std::min(pos - bar.start, bar.length - 1));
	return bar.start + (rel / bar.spanLength) * bar.spanLength;
}

//--------------------------------------------------------------
int MIDIFileLoader::spanLengthOf(int pos)
{
	const Bar& bar = bars[barOf(pos)];
	int start = spanStartOf(pos);
	return std::min(bar.spanLength, bar.start + bar.length - start);
}

//--------------------------------------------------------------
int MIDIFileLoader::gridOf(int pos)
{
	int start = spanStartOf(pos);
	auto it = spanGrids.find(start);
	if (it != spanGrids.end()) return it->second;
	return std::min(12, spanLengthOf(pos));
}

//--------------------------------------------------------------
int MIDIFileLoader::snapToGrid(double pos)
{
	int p = std::max(0, (int)std::floor(pos));
	int start = spanStartOf(p);
	int grid = gridOf(p);
	return start + (int)std::lround((pos - start) / grid) * grid;
}

//--------------------------------------------------------------
int MIDIFileLoader::tempoAt(int pos)
{
	int tempo = 500000; // 120 BPM
	for (auto& t : tempi) {
		if (unitsOf(t.first) > pos + 1) break;
		tempo = t.second;
	}
	return tempo;
}

//--------------------------------------------------------------
int MIDIFileLoader::keySignatureAt(int pos)
{
	int key = 0;
	for (auto& k : keySignatures) {
		if (unitsOf(k.first) > pos + 1) break;
		key = k.second;
	}
	return key;
}

//--------------------------------------------------------------
std::vector<MIDIFileLoader::Event> MIDIFileLoader::quantize(const Staff& staff)
{
	spanGrids.clear();
	int lastBarEnd = bars.back().start + bars.back().length;
	// the notes of every span, by the start of the span
	// a note that starts a little before a span belongs to it
	std::map<int, std::vector<size_t>> notesOfSpans;
	for (size_t i = 0; i < staff.notes.size(); i++) {
		int pos = std::min(lastBarEnd - 1, (int)std::floor(unitsOf(staff.notes[i].start) + ONSET_TOLERANCE));
		notesOfSpans[spanStartOf(std::max(0, pos))].push_back(i);
	}
	// the quantized start of every note
	std::vector<int> starts(staff.notes.size(), 0);
	int exactSpans = 0;
	for (auto& span : notesOfSpans) {
		int spanStart = span.first;
		int spanLength = spanLengthOf(spanStart);
		std::vector<double> rel;
		for (size_t i : span.second) rel.push_back(unitsOf(staff.notes[i].start) - spanStart);
		// the grids to choose from, from the coarsest, in the order they are preferred
		std::vector<int> grids;
		if (spanLength == UNITS_PER_QUARTER) grids = {12, 16, 6, 8};
		else grids = {12, 6};
		grids.erase(std::remove_if(grids.begin(), grids.end(), [spanLength](int g) { return g > spanLength || spanLength % g != 0; }), grids.end());
		if (grids.empty()) grids = {spanLength};
		auto maxError = [&rel](int g) {
			double err = 0;
			for (double r : rel) err = std::max(err, std::fabs(r - std::round(r / g) * g));
			return err;
		};
		auto squaredError = [&rel](int g) {
			double err = 0;
			for (double r : rel) err += std::pow(r - std::round(r / g) * g, 2);
			return err;
		};
		int grid = 0;
		// a file that is already quantized keeps its exact positions
		for (int g : grids) {
			if (maxError(g) <= EXACT_TOLERANCE) {
				grid = g;
				exactSpans++;
				break;
			}
		}
		// a performed file uses 16ths, or eighth-note triplets when they fit the notes of the beat clearly better
		if (grid == 0) {
			grid = grids[0];
			if (std::find(grids.begin(), grids.end(), 16) != grids.end()) {
				bool onTripletPosition = false;
				for (double r : rel) {
					int q = (int)std::lround(r / 16) * 16;
					if (q == 16 || q == 32) onTripletPosition = true;
				}
				if (onTripletPosition && squaredError(16) < 0.5 * squaredError(grid)) grid = 16;
			}
		}
		spanGrids[spanStart] = grid;
		for (size_t k = 0; k < span.second.size(); k++) {
			starts[span.second[k]] = spanStart + std::max(0, (int)std::lround(rel[k] / grid) * grid);
		}
	}
	// notes that start together form a chord, and the longest one is the time the chord sounds
	std::map<int, Event> eventsMap;
	std::map<int, double> soundingTimes;
	for (size_t i = 0; i < staff.notes.size(); i++) {
		Event& event = eventsMap[starts[i]];
		event.start = starts[i];
		event.pitches.insert(staff.notes[i].pitch);
		event.staccato = false;
		double sounding = unitsOf(std::max(staff.notes[i].end, staff.notes[i].start) - staff.notes[i].start);
		soundingTimes[starts[i]] = std::max(soundingTimes[starts[i]], sounding);
	}
	std::vector<Event> events;
	for (auto& e : eventsMap) events.push_back(e.second);
	// the time until the next note, or for the last note, until the end of the bar where it stops sounding
	auto nextStart = [&](size_t i) {
		if (i + 1 < events.size()) return events[i+1].start;
		double soundEnd = events[i].start + soundingTimes[events[i].start];
		const Bar& bar = bars[barOf(std::max(events[i].start, (int)std::ceil(soundEnd) - 1))];
		return bar.start + bar.length;
	};
	// a file exported from notation software has its notes exactly on a grid
	// and it plays every note for the same part of its value (e.g. 95%), unless the note has an articulation
	// so the written value of a note is the time it sounds divided by that part, when this gives a value on the grid
	// and a note whose value is not on the grid this way is articulated (e.g. a staccato quarter played as long as an eighth)
	bool exactFile = (!notesOfSpans.empty() && exactSpans >= 0.9 * notesOfSpans.size());
	double typicalRatio = 1.0;
	if (exactFile) {
		std::vector<double> ratios;
		for (size_t i = 0; i + 1 < events.size(); i++) {
			double ratio = soundingTimes[events[i].start] / (events[i+1].start - events[i].start);
			if (ratio >= 0.8 && ratio <= 1.05) ratios.push_back(ratio);
		}
		if (!ratios.empty()) {
			std::sort(ratios.begin(), ratios.end());
			typicalRatio = ratios[ratios.size() / 2];
		}
	}
	// the written value of every note or chord
	for (size_t i = 0; i < events.size(); i++) {
		Event& event = events[i];
		int start = event.start;
		double sounding = soundingTimes[start];
		int grid = gridOf(start);
		int minEnd = start + grid;
		int next = nextStart(i);
		int interval = next - start;
		double ratio = sounding / interval;
		double silence = interval - sounding;
		if (exactFile) {
			double writtenEnd = start + sounding / typicalRatio;
			int snapped = snapToGrid(writtenEnd);
			if (std::fabs(writtenEnd - snapped) <= 0.6 && snapped >= minEnd && snapped < next) {
				// the note as written, followed by a rest
				event.end = snapped;
			}
			else {
				event.end = next;
				event.staccato = (snapped < next && ratio < STACCATO_RATIO);
			}
			continue;
		}
		// a performed file: the release of a note only makes it shorter than its value
		// so the end of a note that is followed by a rest is rounded up, rather than to the nearest grid position
		int shortenedEnd = std::min(next, std::max(minEnd, snapToGrid(start + sounding * 1.1 + grid / 4.0)));
		// a note that starts off the beat and would be tied over the next beat, but stops sounding before it
		// is a short note followed by a rest, and not a syncopation (e.g. "r8 c8 r8 c8" and not "r8 c4 c4")
		int spanStart = spanStartOf(start);
		int nextBeat = spanStart + spanLengthOf(start);
		bool syncopation = (start != spanStart && next > nextBeat && start + sounding <= nextBeat + 3);
		if (ratio >= LEGATO_RATIO) {
			event.end = next;
		}
		else if (silence >= REST_MIN_SILENCE || syncopation) {
			event.end = shortenedEnd;
		}
		else {
			event.end = next;
			event.staccato = (ratio < STACCATO_RATIO);
		}
	}
	return events;
}

//--------------------------------------------------------------
std::string MIDIFileLoader::pitchName(int pitch, int keySignature)
{
	// sharps in keys with sharps, flats in keys with flats, and the most common spelling of each note in C major
	static const char* sharps[12] = {"c", "cis", "d", "dis", "e", "f", "fis", "g", "gis", "a", "ais", "b"};
	static const char* flats[12] = {"c", "des", "d", "ees", "e", "f", "ges", "g", "aes", "a", "bes", "b"};
	static const char* neutral[12] = {"c", "cis", "d", "ees", "e", "f", "fis", "g", "aes", "a", "bes", "b"};
	const char** names = (keySignature > 0 ? sharps : (keySignature < 0 ? flats : neutral));
	std::string name = names[pitch % 12];
	// c is the C below middle C (MIDI note 48), c' is middle C
	int octave = pitch / 12 - 4;
	if (octave > 0) name += std::string(octave, '\'');
	else if (octave < 0) name += std::string(-octave, ',');
	return name;
}

//--------------------------------------------------------------
std::string MIDIFileLoader::chordOrNote(const std::set<int>& pitches, int keySignature)
{
	if (pitches.empty()) return "r";
	if (pitches.size() == 1) return pitchName(*pitches.begin(), keySignature);
	std::string chord = "<";
	for (int p : pitches) {
		if (chord.size() > 1) chord += " ";
		chord += pitchName(p, keySignature);
	}
	return chord + ">";
}

//--------------------------------------------------------------
std::vector<std::string> MIDIFileLoader::noteValues(int length, int pos, int beat, bool inTuplet)
{
	// splits a duration into the fewest note values that can be written (and tied), with each value starting
	// at a position that suits it: a value can't start in the middle of its own (undotted) length or of a beat
	// e.g. at the second 16th of a beat, three 16ths are a 16th and an eighth, and not a dotted 16th and two more values
	// inside a tuplet any position is fine, as the tuplet groups its notes
	static const std::set<int> dottedValues = {288, 144, 72, 36, 18, 9};
	auto aligned = [&](int value, int at) {
		if (inTuplet) return true;
		int base = (dottedValues.count(value) ? value * 2 / 3 : value);
		int align = (value == beat ? beat : std::min(base, beat));
		return align > 0 && at % align == 0;
	};
	// fewest values from every point of the duration to its end, computed from the end
	// a dotted value counts a little more than a plain one, so that between two ways with the same number of values
	// the one with plain values is chosen, as it splits the duration at the beat (e.g. "8~8" over a beat, and not "8.~16")
	// costs are in tenths: 10 for a value, 11 for a dotted one
	std::vector<int> fewest(length + 1, INT_MAX);
	std::vector<int> choice(length + 1, 0);
	fewest[length] = 0;
	for (int x = length - 1; x >= 0; x--) {
		for (const auto& v : NOTE_VALUES) {
			if (v.first > length - x || fewest[x + v.first] == INT_MAX || !aligned(v.first, pos + x)) continue;
			int cost = fewest[x + v.first] + (dottedValues.count(v.first) ? 11 : 10);
			if (cost < fewest[x]) {
				fewest[x] = cost;
				choice[x] = v.first;
			}
		}
	}
	std::vector<std::string> values;
	if (fewest[0] == INT_MAX) {
		// shouldn't happen, as all durations are multiples of a 64th, but write something that adds up
		for (int x = 0; x + 3 <= length; x += 3) values.push_back("64");
		return values;
	}
	for (int x = 0; x < length; x += choice[x]) {
		for (const auto& v : NOTE_VALUES) {
			if (v.first == choice[x]) values.push_back(v.second);
		}
	}
	return values;
}

//--------------------------------------------------------------
std::string MIDIFileLoader::writeBar(const std::vector<Event>& events, int barNdx)
{
	const Bar& bar = bars[barNdx];
	int barStart = bar.start;
	int barEnd = bar.start + bar.length;
	int keySignature = keySignatureAt(barStart);
	// the notes of the bar, and rests between them
	std::vector<Piece> pieces;
	int pos = barStart;
	for (const Event& e : events) {
		if (e.end <= barStart || e.start >= barEnd) continue;
		int start = std::max(e.start, barStart);
		int end = std::min(e.end, barEnd);
		if (start > pos) pieces.push_back({pos, start, {}, false, false});
		// a note that continues in the next bar is tied to it, and only its last part is marked staccato
		pieces.push_back({start, end, e.pitches, e.end > end, e.staccato && e.end <= end});
		pos = end;
	}
	if (pos < barEnd) pieces.push_back({pos, barEnd, {}, false, false});
	// the parts of the bar with triplets, which are written as tuplets
	// a part is a beat for eighth-note triplets and half a beat for 16th-note triplets
	// and it is a tuplet only if a note or a rest starts or ends at a triplet position in it
	// e.g. 16th-note triplets in the first half of a beat and an eighth in the second are \tuplet 3/2 { c16 d16 e16 } f8
	std::set<int> boundaries;
	for (const Piece& piece : pieces) {
		boundaries.insert(piece.start);
		boundaries.insert(piece.end);
	}
	std::vector<std::pair<int, int>> tuplets;
	for (int s = barStart; s < barEnd; s += bar.spanLength) {
		auto it = spanGrids.find(s);
		if (it == spanGrids.end() || (it->second != 16 && it->second != 8)) continue;
		int partLength = (it->second == 8 ? bar.spanLength / 2 : bar.spanLength);
		for (int part = s; part < s + bar.spanLength; part += partLength) {
			bool triplets = false;
			for (int boundary : boundaries) {
				if (boundary > part && boundary < part + partLength && (boundary - part) % 12 != 0) triplets = true;
			}
			if (triplets) tuplets.push_back({part, part + partLength});
		}
	}
	// neighbouring tuplets with a note that goes from one to the other are one tuplet
	// e.g. quarter-note triplets over two beats are \tuplet 3/2 { c4 d4 e4 }, and not two tuplets with a tie
	for (size_t i = 0; i + 1 < tuplets.size(); ) {
		bool joined = false;
		if (tuplets[i].second == tuplets[i+1].first) {
			for (const Piece& piece : pieces) {
				if (!piece.pitches.empty() && piece.start < tuplets[i].second && piece.end > tuplets[i].second) joined = true;
			}
		}
		if (joined) {
			tuplets[i].second = tuplets[i+1].second;
			tuplets.erase(tuplets.begin() + i + 1);
		}
		else {
			i++;
		}
	}
	auto tupletOf = [&tuplets](int pos) {
		for (size_t i = 0; i < tuplets.size(); i++) {
			if (pos >= tuplets[i].first && pos < tuplets[i].second) return (int)i;
		}
		return -1;
	};
	// notes and rests are split at the edges of the tuplets
	std::set<int> tupletEdges;
	for (auto& t : tuplets) {
		tupletEdges.insert(t.first);
		tupletEdges.insert(t.second);
	}
	std::vector<Piece> split;
	for (const Piece& piece : pieces) {
		Piece remaining = piece;
		for (int edge : tupletEdges) {
			if (edge <= remaining.start || edge >= remaining.end) continue;
			// the part before the edge is tied to the part after it, and only the last part is marked staccato
			Piece first = remaining;
			first.end = edge;
			first.tiedToNext = !remaining.pitches.empty();
			first.staccato = false;
			split.push_back(first);
			remaining.start = edge;
		}
		split.push_back(remaining);
	}
	std::string out;
	bool tupletOpen = false;
	int tupletEnd = 0;
	for (const Piece& piece : split) {
		if (tupletOpen && piece.start >= tupletEnd) {
			out += " }";
			tupletOpen = false;
		}
		int tuplet = tupletOf(piece.start);
		if (tuplet >= 0 && !tupletOpen) {
			out += " \\tuplet 3/2 {";
			tupletOpen = true;
			tupletEnd = tuplets[tuplet].second;
		}
		std::vector<std::string> values;
		if (tuplet >= 0) {
			// in a triplet, three notes take the time of two, so their written values are 3/2 longer
			int tupletStart = tuplets[tuplet].first;
			values = noteValues((piece.end - piece.start) * 3 / 2, (piece.start - tupletStart) * 3 / 2, bar.spanLength * 3 / 2, true);
		}
		else {
			values = noteValues(piece.end - piece.start, piece.start - barStart, bar.spanLength, false);
		}
		std::string name = chordOrNote(piece.pitches, keySignature);
		for (size_t i = 0; i < values.size(); i++) {
			out += " " + name + values[i];
			bool last = (i + 1 == values.size());
			if (piece.pitches.empty()) continue;
			if (!last) out += "~";
			else if (piece.staccato) out += "-.";
			else if (piece.tiedToNext) out += "~";
		}
	}
	if (tupletOpen) out += " }";
	return out;
}

//--------------------------------------------------------------
std::vector<std::string> MIDIFileLoader::load(const std::string& filePath)
{
	std::vector<std::string> lines;
	error.clear();
	staves.clear();
	tempi.clear();
	timeSignatures.clear();
	keySignatures.clear();
	std::ifstream file(filePath.c_str(), std::ios::binary);
	if (!file.is_open()) {
		error = "could not load file";
		return lines;
	}
	std::stringstream contents;
	contents << file.rdbuf();
	file.close();
	if (!parse(contents.str())) return lines;
	beatTracked = false;
	std::string pulseComment = fitToPulse();
	if (pulseComment.empty()) pulseComment = trackBeats();
	// the bars cover all the notes, with a bar more, as quantization can move the end of a note a little later
	long lastTick = 0;
	for (const Staff& staff : staves) {
		for (const Note& note : staff.notes) lastTick = std::max(lastTick, std::max(note.end, note.start));
	}
	makeBars((int)std::ceil(unitsOf(lastTick)) + 4 * UNITS_PER_QUARTER);
	// quantize every staff, and find the last bar with notes
	std::vector<std::vector<Event>> allEvents;
	std::vector<std::map<int, int>> allSpanGrids;
	int lastEnd = 0;
	for (const Staff& staff : staves) {
		allEvents.push_back(quantize(staff));
		allSpanGrids.push_back(spanGrids);
		for (const Event& e : allEvents.back()) lastEnd = std::max(lastEnd, e.end);
	}
	int numBars = barOf(std::max(0, lastEnd - 1)) + 1;
	// the clef of every bar of every staff, from the average pitch of its notes
	// it changes only for bars that are clearly higher or lower, so that it doesn't change back and forth
	std::vector<std::vector<std::string>> clefs(staves.size(), std::vector<std::string>(numBars));
	for (size_t s = 0; s < staves.size(); s++) {
		double total = 0;
		for (const Note& n : staves[s].notes) total += n.pitch;
		std::string clef = (total / staves[s].notes.size() >= 60 ? "treble" : "bass");
		for (int b = 0; b < numBars; b++) {
			double sum = 0;
			int count = 0;
			for (const Event& e : allEvents[s]) {
				if (e.start < bars[b].start || e.start >= bars[b].start + bars[b].length) continue;
				for (int p : e.pitches) {
					sum += p;
					count++;
				}
			}
			if (count > 0) {
				double average = sum / count;
				if (average >= 64) clef = "treble";
				else if (average < 56) clef = "bass";
			}
			clefs[s][b] = clef;
		}
	}
	if (!pulseComment.empty()) {
		lines.push_back(pulseComment);
		lines.push_back("");
	}
	lines.push_back("\\score.show");
	lines.push_back("\\score.animate");
	lines.push_back("");
	std::string insts = "\\insts.init";
	for (const Staff& staff : staves) insts += " " + staff.name;
	lines.push_back(insts);
	// the two staves of a piano are connected
	for (auto& group : pianoGroups) {
		lines.push_back("\\group \\" + staves[group.first].name + " \\" + staves[group.second].name);
	}
	lines.push_back("");
	for (int b = 0; b < numBars; b++) {
		const Bar& bar = bars[b];
		lines.push_back("\\bar " + std::to_string(b + 1) + " {");
		lines.push_back(tabStr + "\\time " + std::to_string(bar.numerator) + "/" + std::to_string(bar.denominator));
		// the tempo is written for the beat of the time signature, e.g. a dotted quarter in 6/8
		// a file with detected beats has a tempo for every beat, so the tempo of a bar is their average
		double quarterBPM = 60000000.0 / (beatTracked ? averageTempo(bar.start, bar.start + bar.length) : tempoAt(bar.start));
		bool compound = (bar.denominator >= 8 && bar.numerator % 3 == 0 && bar.numerator > 3);
		std::string tempoUnit;
		double bpm;
		if (compound) {
			tempoUnit = std::to_string(bar.denominator / 2) + ".";
			bpm = quarterBPM * bar.denominator / 12.0;
		}
		else {
			tempoUnit = std::to_string(bar.denominator);
			bpm = quarterBPM * bar.denominator / 4.0;
		}
		lines.push_back(tabStr + "\\tempo " + tempoUnit + " = " + std::to_string(std::max(1L, std::lround(bpm))));
		for (size_t s = 0; s < staves.size(); s++) {
			spanGrids = allSpanGrids[s];
			lines.push_back(tabStr + "\\" + staves[s].name + " \\clef " + clefs[s][b] + writeBar(allEvents[s], b));
		}
		lines.push_back("}");
		lines.push_back("");
	}
	return lines;
}
