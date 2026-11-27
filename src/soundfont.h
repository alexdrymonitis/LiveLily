#ifndef SOUNDFONT_H
#define SOUNDFONT_H

#include <map>
#include <mutex>
#include <string>
#include <vector>

// plays SoundFonts (.sf2) with FluidSynth, for instruments that load one with \instname.sndfont /path/to/soundfont
// there is one synthesizer with one audio output for all instruments, and every instrument gets its own channel
// on it, with the preset it chose from its SoundFont, so instruments can use different SoundFonts and presets
// a SoundFont that more than one instrument uses is loaded once
// the sequencer sends the notes of these instruments here, instead of to a MIDI port (see Sequencer::midiNoteOn())
class SoundFontPlayer
{
	public:
		~SoundFontPlayer();
		// loads a SoundFont (unless it is already loaded) and selects a preset of it on a channel of the synthesizer
		// channel is the channel of the instrument, -1 for an instrument that doesn't have one yet, in which case it gets one
		// returns an error, or an empty string
		std::string assign(const std::string& path, int bank, int preset, int& channel);
		// MIDI messages, with channels of the synthesizer (from 0), keys and velocities from 0 to 127
		void noteOn(int channel, int key, int velocity);
		void noteOff(int channel, int key);
		// pitch bend from -8192 to 8191, with 0 for no bend, and a range of 12 semitones up and down
		void pitchBend(int channel, int value);
		void allSoundOff(int channel);
		void controlChange(int channel, int controller, int value);
		// the channel of a voice of an instrument, for the notes of chords with glissandi, which bend separately (see Sequencer::startGlissando())
		// voice 0 is the channel of the instrument, and every other voice gets a channel of its own (the first time it is used)
		// with the same SoundFont and preset as the instrument
		int voiceChannel(int channel, int voice);
	private:
		// starts the synthesizer and its audio output, the first time a SoundFont is loaded
		std::string start();
		// FluidSynth objects (fluid_settings_t, fluid_synth_t and fluid_audio_driver_t)
		// stored as void pointers so that this header doesn't need fluidsynth.h
		void* settings = nullptr;
		void* synth = nullptr;
		void* driver = nullptr;
		// the id of every loaded SoundFont, by its path
		std::map<std::string, int> soundFontIds;
		// the SoundFont, bank, and preset of every channel of an instrument, so that its voices can select them too
		struct ChannelProgram {
			int soundFontId;
			int bank;
			int preset;
		};
		std::map<int, ChannelProgram> channelPrograms;
		// the channels of the voices of every channel of an instrument (from voice 1)
		std::map<int, std::vector<int>> voiceChannels;
		// channels are given out from the main thread (assign()) and the sequencer thread (voiceChannel())
		std::mutex channelMutex;
		// selects a program on a channel, with the pitch bend range of LiveLily
		bool selectProgram(int channel, const ChannelProgram& program);
		int nextChannel = 0;
		static const int numChannels = 256;
};

#endif
