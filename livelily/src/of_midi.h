#ifndef OF_MIDI_H
#define OF_MIDI_H

#include "midi.h"
#include "ofxMidi.h"
#include <map>
#include <memory>

// the MIDI output of LiveLily (see midi.h) with openFrameworks' ofxMidi, one ofxMidiOut for every open port
class OfMidiOutput : public MidiOutput
{
	public:
		std::vector<std::string> portNames() override;
		bool open(int port) override;
		bool isOpen(int port) override;
		void closeAll() override;
		void noteOn(int port, int channel, int note, int velocity) override;
		void noteOff(int port, int channel, int note, int velocity) override;
		void pitchBend(int port, int channel, int value) override;
		void programChange(int port, int channel, int program) override;
		void controlChange(int port, int channel, int control, int value) override;
		void sendByte(int port, unsigned char byte) override;
	private:
		// the open ports, by their numbers in portNames()
		std::map<int, std::unique_ptr<ofxMidiOut>> outs;
		// an ofxMidiOut that is never opened, to list the ports
		ofxMidiOut lister;
		ofxMidiOut* out(int port);
};

#endif
