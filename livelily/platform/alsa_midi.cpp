// the MIDI output of LiveLily (see midi.h) on Linux, with the ALSA sequencer, in place of JUCE's (see juce_midi.cpp, for macOS and Windows)
// JUCE 8 sends MIDI 2.0 events (UMP) whenever the ALSA library knows them, but kernels built without UMP in their sequencer
// (CONFIG_SND_SEQ_UMP, e.g. Debian's) drop them, so nothing arrives; this sends MIDI 1.0 events, which every kernel delivers
// the ports are listed in ALSA's order and named as RtMidi (and so ofxMidi) named them, so their numbers are those of the openFrameworks app
#include "midi.h"
#include <alsa/asoundlib.h>
#include <algorithm>
#include <map>
#include <mutex>

namespace {

// a port of another program that can receive MIDI
struct Destination
{
	std::string name;
	snd_seq_addr_t address;
};

class AlsaMidiOutput : public MidiOutput
{
	public:
		~AlsaMidiOutput() override
		{
			closeAll();
			if (seq != nullptr) snd_seq_close(seq);
		}

		std::vector<std::string> portNames() override
		{
			std::lock_guard<std::mutex> lock(mutex);
			std::vector<std::string> names;
			for (const Destination& d : destinations()) names.push_back(d.name);
			return names;
		}

		bool open(int port) override
		{
			if (port < 0) return false;
			std::lock_guard<std::mutex> lock(mutex);
			if (outs.find(port) != outs.end()) return true;
			// the port is kept even if it couldn't be opened, which then ignores what is sent to it, as with ofxMidi (see of_midi.cpp)
			Out& out = outs[port];
			std::vector<Destination> list = destinations();
			if (seq == nullptr || port >= (int)list.size()) return false;
			// a port of LiveLily for every port it sends to, as RtMidi did
			std::string name = "LiveLily out " + std::to_string(port);
			int ownPort = snd_seq_create_simple_port(seq, name.c_str(), SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
					SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
			if (ownPort < 0) return false;
			if (snd_seq_connect_to(seq, ownPort, list[port].address.client, list[port].address.port) < 0
					|| snd_midi_event_new(256, &out.encoder) < 0) {
				snd_seq_delete_simple_port(seq, ownPort);
				out.encoder = nullptr;
				return false;
			}
			out.ownPort = ownPort;
			return true;
		}

		bool isOpen(int port) override
		{
			std::lock_guard<std::mutex> lock(mutex);
			return outs.find(port) != outs.end();
		}

		void closeAll() override
		{
			std::lock_guard<std::mutex> lock(mutex);
			for (auto& it : outs) {
				if (it.second.ownPort >= 0) snd_seq_delete_simple_port(seq, it.second.ownPort);
				if (it.second.encoder != nullptr) snd_midi_event_free(it.second.encoder);
			}
			outs.clear();
		}

		// channels are 1 to 16, as with ofxMidi
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
		// a byte of a message given byte by byte (\midibyte), which the encoder of the port collects until the message is whole
		void sendByte(int port, unsigned char b) override { send(port, {b}); }

	private:
		struct Out
		{
			int ownPort = -1;
			snd_midi_event_t* encoder = nullptr;
		};
		// the sequencer's thread sends while the main thread may open ports, so the ports are guarded
		std::mutex mutex;
		snd_seq_t* seq = openSequencer();
		std::map<int, Out> outs;

		static snd_seq_t* openSequencer()
		{
			snd_seq_t* s = nullptr;
			if (snd_seq_open(&s, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) return nullptr;
			snd_seq_set_client_name(s, "LiveLily");
			return s;
		}

		static unsigned char status(int kind, int channel) { return (unsigned char)(kind | ((channel - 1) & 0x0F)); }
		static unsigned char byte(int value) { return (unsigned char)(value & 0x7F); }

		// the ports of the other programs (and of the system) that can receive MIDI, in ALSA's order
		std::vector<Destination> destinations()
		{
			std::vector<Destination> list;
			if (seq == nullptr) return list;
			snd_seq_client_info_t* client;
			snd_seq_port_info_t* port;
			snd_seq_client_info_alloca(&client);
			snd_seq_port_info_alloca(&port);
			int ownClient = snd_seq_client_id(seq);
			snd_seq_client_info_set_client(client, -1);
			while (snd_seq_query_next_client(seq, client) >= 0) {
				int clientId = snd_seq_client_info_get_client(client);
				// the system's timer and announcements, and LiveLily itself
				if (clientId == SND_SEQ_CLIENT_SYSTEM || clientId == ownClient) continue;
				snd_seq_port_info_set_client(port, clientId);
				snd_seq_port_info_set_port(port, -1);
				while (snd_seq_query_next_port(seq, port) >= 0) {
					unsigned int caps = snd_seq_port_info_get_capability(port);
					unsigned int wanted = SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
					if ((caps & wanted) != wanted || (caps & SND_SEQ_PORT_CAP_NO_EXPORT) != 0) continue;
					// the kinds of ports RtMidi lists
					unsigned int types = SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_SYNTH | SND_SEQ_PORT_TYPE_APPLICATION;
					if ((snd_seq_port_info_get_type(port) & types) == 0) continue;
					// as RtMidi names them: the program, the port, and their numbers (e.g. Midi Through:Midi Through Port-0 14:0)
					std::string name = std::string(snd_seq_client_info_get_name(client)) + ":" + snd_seq_port_info_get_name(port)
							+ " " + std::to_string(clientId) + ":" + std::to_string(snd_seq_port_info_get_port(port));
					list.push_back({name, *snd_seq_port_info_get_addr(port)});
				}
			}
			return list;
		}

		void send(int port, const std::vector<unsigned char>& bytes)
		{
			std::lock_guard<std::mutex> lock(mutex);
			auto it = outs.find(port);
			if (it == outs.end() || it->second.encoder == nullptr) return;
			const unsigned char* data = bytes.data();
			long left = (long)bytes.size();
			while (left > 0) {
				snd_seq_event_t event;
				snd_seq_ev_clear(&event);
				long used = snd_midi_event_encode(it->second.encoder, data, left, &event);
				if (used <= 0) {
					snd_midi_event_reset_encode(it->second.encoder);
					return;
				}
				data += used;
				left -= used;
				// a message isn't whole yet (e.g. the first byte of one given byte by byte)
				if (event.type == SND_SEQ_EVENT_NONE) continue;
				snd_seq_ev_set_source(&event, it->second.ownPort);
				snd_seq_ev_set_subs(&event);
				snd_seq_ev_set_direct(&event);
				snd_seq_event_output_direct(seq, &event);
			}
		}
};

}

std::unique_ptr<MidiOutput> makeMidiOutput()
{
	return std::make_unique<AlsaMidiOutput>();
}
