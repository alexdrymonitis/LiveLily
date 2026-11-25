// the platform of the headless runner of LiveLily (see main.cpp) that isn't JUCE's (see platform/): serial ports that do nothing
// (until LiveLily has serial ports of its own), and the audio stream of test mode, which never plays, as the tests don't play sound

#include "serial.h"
#include "audio.h"

namespace {

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

std::unique_ptr<SerialPort> makeSerialPort() { return std::make_unique<NullSerialPort>(); }
std::unique_ptr<AudioStream> makeTestAudioStream() { return std::make_unique<NullAudioStream>(); }
