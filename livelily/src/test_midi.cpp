// the MIDI output of test mode (see midi.h): the snapshots of the test files don't depend on the MIDI ports of the machine
// (e.g. \inst.midiport 3 is an error, and \openmidiport 0 opens a port, wherever the tests run)

#include "midi.h"
#include <set>

namespace {

class TestMidiOutput : public MidiOutput
{
	public:
		std::vector<std::string> portNames() override { return {"LiveLily test port 1", "LiveLily test port 2"}; }
		bool open(int port) override
		{
			if (port < 0 || port >= (int)portNames().size()) return false;
			opened.insert(port);
			return true;
		}
		bool isOpen(int port) override { return opened.count(port) > 0; }
		void closeAll() override { opened.clear(); }
		// nothing is sent, as the sequencer's dry run writes what it would send (see Sequencer::dryRun())
		void noteOn(int, int, int, int) override {}
		void noteOff(int, int, int, int) override {}
		void pitchBend(int, int, int) override {}
		void programChange(int, int, int) override {}
		void controlChange(int, int, int, int) override {}
		void sendByte(int, unsigned char) override {}
	private:
		std::set<int> opened;
};

}

std::unique_ptr<MidiOutput> makeTestMidiOutput()
{
	return std::make_unique<TestMidiOutput>();
}
