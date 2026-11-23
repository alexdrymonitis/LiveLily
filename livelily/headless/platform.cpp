// the platform of the headless build of LiveLily (see main.cpp), with no openFrameworks:
// MIDI with RtMidi (the library ofxMidi wraps, so the ports are listed and opened as in the app)
// and OSC, serial, and audio that do nothing, as the tests don't send to the network or play sound

#include "midi.h"
#include "osc.h"
#include "serial.h"
#include "audio.h"
#include "RtMidi.h"
#include <algorithm>
#include <map>

namespace {

class RtMidiOutput : public MidiOutput
{
	public:
		std::vector<std::string> portNames() override
		{
			std::vector<std::string> names;
			try {
				RtMidiOut lister;
				for (unsigned i = 0; i < lister.getPortCount(); i++) names.push_back(lister.getPortName(i));
			}
			catch (const RtMidiError&) {}
			return names;
		}
		bool open(int port) override
		{
			if (port < 0) return false;
			if (isOpen(port)) return true;
			// the port is kept even if it couldn't be opened, which then ignores what is sent to it, as in the app (see of_midi.cpp)
			bool opened = false;
			std::unique_ptr<RtMidiOut> out;
			try {
				out = std::make_unique<RtMidiOut>();
				if ((unsigned)port < out->getPortCount()) {
					out->openPort((unsigned)port, "LiveLily");
					opened = true;
				}
			}
			catch (const RtMidiError&) {}
			outs[port] = std::move(out);
			return opened;
		}
		bool isOpen(int port) override { return outs.find(port) != outs.end(); }
		void closeAll() override
		{
			for (auto& out : outs) if (out.second) out.second->closePort();
			outs.clear();
		}
		// channels are 1 to 16, as in ofxMidi
		void noteOn(int port, int channel, int note, int velocity) override { send(port, {status(0x90, channel), byte(note), byte(velocity)}); }
		void noteOff(int port, int channel, int note, int velocity) override { send(port, {status(0x80, channel), byte(note), byte(velocity)}); }
		void pitchBend(int port, int channel, int value) override
		{
			// LiveLily gives -8192 to 8191 (0 for no bend, see midi.h), and MIDI has 0 to 16383 (8192 for no bend)
			int bend = std::clamp(value + 8192, 0, 16383);
			send(port, {status(0xE0, channel), (unsigned char)(bend & 0x7F), (unsigned char)((bend >> 7) & 0x7F)});
		}
		void programChange(int port, int channel, int program) override { send(port, {status(0xC0, channel), byte(program)}); }
		void controlChange(int port, int channel, int control, int value) override { send(port, {status(0xB0, channel), byte(control), byte(value)}); }
		void sendByte(int port, unsigned char b) override { send(port, {b}); }

	private:
		static unsigned char status(int kind, int channel) { return (unsigned char)(kind | ((channel - 1) & 0x0F)); }
		static unsigned char byte(int value) { return (unsigned char)(value & 0x7F); }
		void send(int port, std::vector<unsigned char> message)
		{
			auto it = outs.find(port);
			if (it == outs.end() || !it->second || !it->second->isPortOpen()) return;
			try { it->second->sendMessage(&message); }
			catch (const RtMidiError&) {}
		}
		std::map<int, std::unique_ptr<RtMidiOut>> outs;
};

class NullOscSender : public OscSender
{
	public:
		bool setup(const std::string&, int) override { return true; }
		void send(const OscMessage&) override {}
};

class NullOscReceiver : public OscReceiver
{
	public:
		bool setup(int) override { return true; }
		bool hasWaitingMessages() override { return false; }
		bool getNextMessage(OscMessage&) override { return false; }
};

class NullSerialPort : public SerialPort
{
	public:
		std::vector<SerialDevice> devices() override { return {}; }
		bool open(const std::string&, int) override { return false; }
		bool isOpen() override { return false; }
		void writeByte(unsigned char) override {}
		void writeBytes(const std::string&) override {}
		void close() override {}
};

class NullAudioStream : public AudioStream
{
	public:
		std::vector<AudioDevice> devices() override { return {}; }
		// the stream is taken as set up, but it never calls back
		bool setup(const AudioSettings&, AudioCallback*) override { return true; }
		void close() override {}
};

}

std::unique_ptr<MidiOutput> makeMidiOutput() { return std::make_unique<RtMidiOutput>(); }
std::shared_ptr<OscSender> makeOscSender() { return std::make_shared<NullOscSender>(); }
std::shared_ptr<OscReceiver> makeOscReceiver() { return std::make_shared<NullOscReceiver>(); }
std::unique_ptr<SerialPort> makeSerialPort() { return std::make_unique<NullSerialPort>(); }
std::unique_ptr<AudioStream> makeAudioStream() { return std::make_unique<NullAudioStream>(); }
