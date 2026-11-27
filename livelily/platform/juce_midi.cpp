// the MIDI output of LiveLily (see midi.h) with JUCE on macOS and Windows (Linux has alsa_midi.cpp), one juce::MidiOutput for every open port
#include "midi.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <algorithm>
#include <map>
#include <mutex>

namespace {

class JuceMidiOutput : public MidiOutput
{
	public:
		std::vector<std::string> portNames() override
		{
			std::vector<std::string> names;
			for (const juce::MidiDeviceInfo& device : juce::MidiOutput::getAvailableDevices()) names.push_back(device.name.toStdString());
			return names;
		}
		bool open(int port) override
		{
			if (port < 0) return false;
			std::lock_guard<std::mutex> lock(mutex);
			if (outs.find(port) != outs.end()) return true;
			juce::Array<juce::MidiDeviceInfo> devices = juce::MidiOutput::getAvailableDevices();
			std::unique_ptr<juce::MidiOutput> out;
			if (port < devices.size()) out = juce::MidiOutput::openDevice(devices[port].identifier);
			bool opened = (out != nullptr);
			// the port is kept even if it couldn't be opened, which then ignores what is sent to it, as ofxMidi did in the openFrameworks app
			outs[port] = std::move(out);
			return opened;
		}
		bool isOpen(int port) override
		{
			std::lock_guard<std::mutex> lock(mutex);
			return outs.find(port) != outs.end();
		}
		void closeAll() override
		{
			std::lock_guard<std::mutex> lock(mutex);
			outs.clear();
		}
		// channels are 1 to 16, as with ofxMidi and in JUCE
		void noteOn(int port, int channel, int note, int velocity) override
		{
			send(port, juce::MidiMessage::noteOn(ch(channel), byte(note), (juce::uint8)byte(velocity)));
		}
		void noteOff(int port, int channel, int note, int velocity) override
		{
			send(port, juce::MidiMessage::noteOff(ch(channel), byte(note), (juce::uint8)byte(velocity)));
		}
		void pitchBend(int port, int channel, int value) override
		{
			// LiveLily gives -8192 to 8191 (0 for no bend, see midi.h), and MIDI has 0 to 16383 (8192 for no bend)
			send(port, juce::MidiMessage::pitchWheel(ch(channel), std::clamp(value + 8192, 0, 16383)));
		}
		void programChange(int port, int channel, int program) override
		{
			send(port, juce::MidiMessage::programChange(ch(channel), byte(program)));
		}
		void controlChange(int port, int channel, int control, int value) override
		{
			send(port, juce::MidiMessage::controllerEvent(ch(channel), byte(control), byte(value)));
		}
		void sendByte(int port, unsigned char b) override
		{
			send(port, juce::MidiMessage(&b, 1));
		}

	private:
		static int ch(int channel) { return ((channel - 1) & 0x0F) + 1; }
		static int byte(int value) { return value & 0x7F; }
		// the sequencer's thread sends while the main thread may open ports, so the ports are guarded
		void send(int port, const juce::MidiMessage& message)
		{
			std::lock_guard<std::mutex> lock(mutex);
			auto it = outs.find(port);
			if (it != outs.end() && it->second) it->second->sendMessageNow(message);
		}
		std::mutex mutex;
		std::map<int, std::unique_ptr<juce::MidiOutput>> outs;
};

}

std::unique_ptr<MidiOutput> makeMidiOutput()
{
	return std::make_unique<JuceMidiOutput>();
}
