#ifndef MIDI_H
#define MIDI_H

#include <memory>
#include <string>
#include <vector>

// the MIDI output of LiveLily, which the platform implements (see platform/alsa_midi.cpp on Linux, and platform/juce_midi.cpp)
// ports are numbered as in the list of portNames(), and a message to a port that is not open is ignored
// channels go from 1 to 16
class MidiOutput
{
	public:
		virtual ~MidiOutput() = default;
		// the names of the MIDI out ports of the system
		virtual std::vector<std::string> portNames() = 0;
		virtual bool open(int port) = 0;
		virtual bool isOpen(int port) = 0;
		virtual void closeAll() = 0;
		virtual void noteOn(int port, int channel, int note, int velocity) = 0;
		virtual void noteOff(int port, int channel, int note, int velocity) = 0;
		// from -8192 to 8191, with 0 for no bend
		virtual void pitchBend(int port, int channel, int value) = 0;
		virtual void programChange(int port, int channel, int program) = 0;
		virtual void controlChange(int port, int channel, int control, int value) = 0;
		// a single byte, e.g. of the MIDI clock
		virtual void sendByte(int port, unsigned char byte) = 0;
};

// the platform makes it (see platform/)
std::unique_ptr<MidiOutput> makeMidiOutput();

// the MIDI output of test mode (see test_midi.cpp): two made-up ports, which open but get nothing
// so that what a test file does with MIDI ports is the same on every machine, whatever ports it has
std::unique_ptr<MidiOutput> makeTestMidiOutput();

#endif
