#ifndef MIDIFILE_H
#define MIDIFILE_H

#include <string>
#include <vector>
#include <map>
#include <set>

// converts a Standard MIDI File (.mid or .midi, format 0 or 1) to LiveLily code
// like MusicXMLLoader, it only reads the file and returns the lines of LiveLily code, which the editor then inserts
//
// a MIDI file stores when each sound starts and stops, but not the written note values
// these are inferred as follows (see the constants at the top of midifile.cpp):
// - note starts are quantized to a grid chosen for every beat (16ths or eighth-note triplets for performed files,
//   and the exact positions down to 32nds and 16th-note triplets for files that are already quantized)
// - the written value of a note is the time until the next note starts, as articulation and the release of a sound
//   change when it stops, but not when the next note starts
// - the time a note sounds only determines whether a rest is written after it (for silences of about a beat or more)
//   or whether the note is marked staccato (for shorter silences)
class MIDIFileLoader
{
	public:
		// tab is the string used for indentation (the lines inside bars)
		MIDIFileLoader(const std::string& tab);
		// returns the lines of LiveLily code, or no lines and an error that getError() returns
		std::vector<std::string> load(const std::string& filePath);
		std::string getError();
	private:
		// a note as read from the file, in ticks
		struct Note
		{
			long start;
			long end;
			int pitch;
		};
		// the notes of one staff, which is a track, or a channel of a track that uses several channels
		struct Staff
		{
			std::string name;
			std::vector<Note> notes;
			// the General MIDI program of the staff, or -1 if the file doesn't set one
			int program = -1;
			bool drums = false;
		};
		// a bar, in units of 1/48 of a quarter note
		struct Bar
		{
			int start;
			int length;
			int numerator;
			int denominator;
			// the length of the spans for quantization and for grouping note values, normally a beat
			int spanLength;
		};
		// a note or a chord after quantization, in units, with the notes that start together
		struct Event
		{
			int start;
			int end;
			std::set<int> pitches;
			bool staccato;
		};
		// a part of an event or a rest, within one bar, and within one tuplet span if it is in one
		struct Piece
		{
			int start;
			int end;
			std::set<int> pitches; // empty for a rest
			bool tiedToNext;
			bool staccato;
		};

		// reading the file
		bool parse(const std::string& data);
		// for a file whose notes follow a steady pulse that doesn't match its own beat (e.g. one saved without a tempo
		// and with a wrong resolution), finds that pulse, and moves the notes and the tempo to it
		// returns a comment for the LiveLily code if it did, or an empty string
		std::string fitToPulse();
		// for a file played without a steady beat (e.g. a performance recorded without a click), finds the beats
		// and moves the notes and the tempo to them, returns a comment for the LiveLily code if it did, or an empty string
		std::string trackBeats();
		// whether the note starts line up with the subdivisions of the file's own beat
		bool fitsOwnBeat(const std::vector<long>& onsets);
		// splits staves of pianos (and other keyboards) with notes on both sides of middle C into two staves
		void splitPianoStaves();
		// the average tempo (microseconds per quarter) from a position to another, in units
		int averageTempo(int from, int to);
		// building the bars from the time signatures, up to the end of the last note
		void makeBars(int lastEnd);
		int barOf(int pos);
		int spanStartOf(int pos);
		int spanLengthOf(int pos);
		// the grid of the span that contains a position, in units (e.g. 12 for 16ths, 16 for eighth-note triplets)
		int gridOf(int pos);
		int snapToGrid(double pos);
		// conversion of a staff to LiveLily
		std::vector<Event> quantize(const Staff& staff);
		std::string writeBar(const std::vector<Event>& events, int barNdx);
		std::string pitchName(int pitch, int keySignature);
		std::string chordOrNote(const std::set<int>& pitches, int keySignature);
		// splits a duration in units into note values that can be written, e.g. 60 at the start of a beat is a quarter tied to a 16th
		std::vector<std::string> noteValues(int length, int pos, int beat, bool inTuplet);
		std::string sanitizeName(const std::string& name, int index);

		double unitsOf(long ticks);
		int tempoAt(int pos);
		int keySignatureAt(int pos);

		std::string tabStr;
		std::string error;
		int division;
		std::vector<Staff> staves;
		// tempo (microseconds per quarter note), time signature (numerator, denominator) and key signature changes, by tick
		std::map<long, int> tempi;
		std::map<long, std::pair<int, int>> timeSignatures;
		std::map<long, int> keySignatures;
		std::vector<Bar> bars;
		// the pairs of staves that are the two hands of a piano, by their indexes
		std::vector<std::pair<size_t, size_t>> pianoGroups;
		// whether the beats of the file were detected (see trackBeats())
		bool beatTracked = false;
		// the grid of every span that has notes, by the start of the span, for the staff being converted
		std::map<int, int> spanGrids;
};

#endif
