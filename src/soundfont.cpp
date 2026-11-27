#include <algorithm>
#include <fluidsynth.h>
#include "soundfont.h"

/********************* SoundFontPlayer class *********************/
//--------------------------------------------------------------
SoundFontPlayer::~SoundFontPlayer()
{
	// the audio output is deleted first, as it reads from the synthesizer
	if (driver) delete_fluid_audio_driver((fluid_audio_driver_t*)driver);
	if (synth) delete_fluid_synth((fluid_synth_t*)synth);
	if (settings) delete_fluid_settings((fluid_settings_t*)settings);
}

//--------------------------------------------------------------
std::string SoundFontPlayer::start()
{
	if (synth) return "";
	settings = new_fluid_settings();
	if (!settings) return "could not start the SoundFont synthesizer";
	// enough channels for many instruments, every instrument gets its own (the number must be a multiple of 16)
	fluid_settings_setint((fluid_settings_t*)settings, "synth.midi-channels", numChannels);
	synth = new_fluid_synth((fluid_settings_t*)settings);
	if (!synth) {
		delete_fluid_settings((fluid_settings_t*)settings);
		settings = nullptr;
		return "could not start the SoundFont synthesizer";
	}
	// the synthesizer sends its sound to the audio output, with the audio driver FluidSynth uses by default on this system
	driver = new_fluid_audio_driver((fluid_settings_t*)settings, (fluid_synth_t*)synth);
	if (!driver) {
		delete_fluid_synth((fluid_synth_t*)synth);
		delete_fluid_settings((fluid_settings_t*)settings);
		synth = settings = nullptr;
		return "could not open the audio output for SoundFonts";
	}
	return "";
}

//--------------------------------------------------------------
std::string SoundFontPlayer::assign(const std::string& path, int bank, int preset, int& channel)
{
	std::string error = start();
	if (!error.empty()) return error;
	int soundFontId;
	auto it = soundFontIds.find(path);
	if (it != soundFontIds.end()) {
		soundFontId = it->second;
	}
	else {
		if (!fluid_is_soundfont(path.c_str())) return path + " is not a SoundFont file, or it doesn't exist";
		// the last argument is 0, as the presets are selected below, per channel
		soundFontId = fluid_synth_sfload((fluid_synth_t*)synth, path.c_str(), 0);
		if (soundFontId == FLUID_FAILED) return "could not load SoundFont " + path;
		soundFontIds[path] = soundFontId;
	}
	std::lock_guard<std::mutex> lock(channelMutex);
	if (channel < 0) {
		if (nextChannel >= numChannels) return "no more channels for SoundFonts, the maximum is " + std::to_string(numChannels) + " instruments";
		channel = nextChannel++;
	}
	ChannelProgram program = {soundFontId, bank, preset};
	if (!selectProgram(channel, program)) {
		return "SoundFont " + path + " has no preset " + std::to_string(preset) + " in bank " + std::to_string(bank);
	}
	channelPrograms[channel] = program;
	// the voices of the instrument play the new preset too
	auto voicesIt = voiceChannels.find(channel);
	if (voicesIt != voiceChannels.end()) {
		for (int voice : voicesIt->second) selectProgram(voice, program);
	}
	return "";
}

//--------------------------------------------------------------
bool SoundFontPlayer::selectProgram(int channel, const ChannelProgram& program)
{
	if (fluid_synth_program_select((fluid_synth_t*)synth, channel, program.soundFontId, program.bank, program.preset) == FLUID_FAILED) {
		return false;
	}
	// LiveLily sends glissandi and quarter tones as pitch bends over 12 semitones up and down, but SoundFonts bend 2 semitones by default
	fluid_synth_pitch_wheel_sens((fluid_synth_t*)synth, channel, 12);
	return true;
}

//--------------------------------------------------------------
int SoundFontPlayer::voiceChannel(int channel, int voice)
{
	if (voice <= 0 || !synth) return channel;
	std::lock_guard<std::mutex> lock(channelMutex);
	auto programIt = channelPrograms.find(channel);
	if (programIt == channelPrograms.end()) return channel;
	std::vector<int>& voices = voiceChannels[channel];
	while ((int)voices.size() < voice) {
		// without free channels, the voice plays on the channel of the instrument
		if (nextChannel >= numChannels) return channel;
		int newChannel = nextChannel++;
		selectProgram(newChannel, programIt->second);
		voices.push_back(newChannel);
	}
	return voices[voice-1];
}

//--------------------------------------------------------------
void SoundFontPlayer::noteOn(int channel, int key, int velocity)
{
	// the FluidSynth API can be called from any thread, so the sequencer thread can play notes while the main thread loads SoundFonts
	if (!synth || channel < 0 || key < 0 || key > 127) return;
	fluid_synth_noteon((fluid_synth_t*)synth, channel, key, std::max(0, std::min(127, velocity)));
}

//--------------------------------------------------------------
void SoundFontPlayer::noteOff(int channel, int key)
{
	if (!synth || channel < 0 || key < 0 || key > 127) return;
	fluid_synth_noteoff((fluid_synth_t*)synth, channel, key);
}

//--------------------------------------------------------------
void SoundFontPlayer::pitchBend(int channel, int value)
{
	if (!synth || channel < 0) return;
	// FluidSynth takes the pitch bend from 0 to 16383, with 8192 for no bend
	fluid_synth_pitch_bend((fluid_synth_t*)synth, channel, std::max(0, std::min(16383, value + 8192)));
}

//--------------------------------------------------------------
void SoundFontPlayer::controlChange(int channel, int controller, int value)
{
	if (!synth || channel < 0) return;
	fluid_synth_cc((fluid_synth_t*)synth, channel, controller, std::max(0, std::min(127, value)));
}

//--------------------------------------------------------------
void SoundFontPlayer::allSoundOff(int channel)
{
	if (!synth || channel < 0) return;
	fluid_synth_all_sounds_off((fluid_synth_t*)synth, channel);
}
