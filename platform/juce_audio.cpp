// the audio stream of LiveLily (see audio.h) with JUCE's audio devices
// the devices of every type of the system (e.g. ALSA and JACK on Linux, CoreAudio on macOS, WASAPI, DirectSound and ASIO on Windows)
// are listed together, each by its type and its name; a device's api is the index of its type, and its id its index among the type's devices
#include "audio.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <algorithm>
#if JUCE_LINUX
#include <dlfcn.h>
#endif

namespace {

// JUCE looks for a JACK server whenever it lists the devices, and when none runs, the JACK library prints that it can't connect
// so the library gets functions for its errors and notes that print nothing, before JUCE loads it (the same library, as it is loaded once)
// JUCE gives it its own function for errors once it connects to a server
void quietJack()
{
#if JUCE_LINUX
	static bool done = false;
	if (done) return;
	done = true;
	void* jack = dlopen("libjack.so.0", RTLD_LAZY);
	if (jack == nullptr) jack = dlopen("libjack.so", RTLD_LAZY);
	if (jack == nullptr) return;
	using SetFunction = void (*)(void (*)(const char*));
	if (auto setError = (SetFunction)dlsym(jack, "jack_set_error_function")) setError([](const char*) {});
	if (auto setInfo = (SetFunction)dlsym(jack, "jack_set_info_function")) setInfo([](const char*) {});
#endif
}

class JuceAudioStream : public AudioStream, private juce::AudioIODeviceCallback
{
	public:
		JuceAudioStream() { quietJack(); }
		~JuceAudioStream() override { close(); }

		std::vector<AudioDevice> devices() override
		{
			std::vector<AudioDevice> list;
			const juce::OwnedArray<juce::AudioIODeviceType>& types = manager.getAvailableDeviceTypes();
			for (int t = 0; t < types.size(); t++) {
				juce::StringArray names = deviceNames(types[t]);
				for (int i = 0; i < names.size(); i++) {
					list.push_back({i, t, (types[t]->getTypeName() + ": " + names[i]).toStdString()});
				}
			}
			return list;
		}

		bool setup(const AudioSettings& s, AudioCallback* cb) override
		{
			close();
			const juce::OwnedArray<juce::AudioIODeviceType>& types = manager.getAvailableDeviceTypes();
			if (types.isEmpty()) return false;
			// the type of the device that is set (the output's, if both are), or else JACK if a JACK server runs
			// (which usually holds the sound card, so that the system's other devices can't play), or else the first type of the system
			int typeIndex = 0;
			if (s.outDeviceSet) typeIndex = s.outDevice.api;
			else if (s.inDeviceSet) typeIndex = s.inDevice.api;
			else {
				for (int t = 0; t < types.size(); t++) {
					if (types[t]->getTypeName() == "JACK" && !deviceNames(types[t]).isEmpty()) typeIndex = t;
				}
			}
			if (typeIndex < 0 || typeIndex >= types.size()) return false;
			juce::AudioIODeviceType* type = types[typeIndex];
			juce::StringArray names = deviceNames(type);
			juce::StringArray outNames = type->getDeviceNames(false);
			juce::StringArray inNames = type->getDeviceNames(true);
			// a device that is set, if it has outputs (or inputs), or else the type's default one
			auto chosen = [&](bool set, const AudioDevice& device, const juce::StringArray& ofKind, bool input) {
				if (set && device.api == typeIndex && device.id >= 0 && device.id < names.size() && ofKind.contains(names[device.id])) {
					return names[device.id];
				}
				int defaultIndex = type->getDefaultDeviceIndex(input);
				return (defaultIndex >= 0 && defaultIndex < ofKind.size() ? ofKind[defaultIndex] : juce::String());
			};
			juce::String outName = (s.outChannels > 0 ? chosen(s.outDeviceSet, s.outDevice, outNames, false) : juce::String());
			juce::String inName = (s.inChannels > 0 ? chosen(s.inDeviceSet, s.inDevice, inNames, true) : juce::String());
			if (outName.isEmpty() && inName.isEmpty()) return false;
			device.reset(type->createDevice(outName, inName));
			if (device == nullptr) return false;
			juce::BigInteger inBits, outBits;
			inBits.setRange(0, std::max(0, s.inChannels), true);
			outBits.setRange(0, std::max(0, s.outChannels), true);
			juce::String error = device->open(inBits, outBits, s.sampleRate, s.bufferSize);
			if (error.isNotEmpty()) {
				device.reset();
				return false;
			}
			callback = cb;
			inChannels = std::max(0, s.inChannels);
			outChannels = std::max(0, s.outChannels);
			device->start(this);
			return true;
		}

		std::string currentDevice() override
		{
			if (device == nullptr) return "";
			return (device->getTypeName() + ": " + device->getName() + ", " + juce::String((int)device->getCurrentSampleRate()) + " Hz, "
					+ juce::String(device->getCurrentBufferSizeSamples()) + " frames").toStdString();
		}

		void close() override
		{
			if (device == nullptr) return;
			device->stop();
			device->close();
			device.reset();
		}

	private:
		// used only for its list of the types of devices of the system
		juce::AudioDeviceManager manager;
		std::unique_ptr<juce::AudioIODevice> device;
		AudioCallback* callback = nullptr;
		int inChannels = 0;
		int outChannels = 2;
		int sampleRate = 44100;
		// LiveLily's samples are interleaved, and JUCE's have a buffer for each channel
		std::vector<float> inInterleaved, outInterleaved;

		// the devices of a type, those with outputs first, then those with inputs only
		static juce::StringArray deviceNames(juce::AudioIODeviceType* type)
		{
			type->scanForDevices();
			juce::StringArray names = type->getDeviceNames(false);
			for (const juce::String& name : type->getDeviceNames(true)) names.addIfNotAlreadyThere(name);
			return names;
		}

		void audioDeviceAboutToStart(juce::AudioIODevice* starting) override
		{
			sampleRate = (int)starting->getCurrentSampleRate();
			int frames = starting->getCurrentBufferSizeSamples();
			inInterleaved.assign((size_t)frames * inChannels, 0.0f);
			outInterleaved.assign((size_t)frames * outChannels, 0.0f);
		}

		void audioDeviceStopped() override {}

		void audioDeviceIOCallbackWithContext(const float* const* in, int numIn, float* const* out, int numOut, int numFrames,
				const juce::AudioIODeviceCallbackContext&) override
		{
			bool fits = (callback != nullptr && (size_t)numFrames * inChannels <= inInterleaved.size()
					&& (size_t)numFrames * outChannels <= outInterleaved.size());
			if (fits && inChannels > 0) {
				for (int c = 0; c < inChannels; c++) {
					for (int f = 0; f < numFrames; f++) inInterleaved[(size_t)f * inChannels + c] = (c < numIn && in[c] ? in[c][f] : 0.0f);
				}
				callback->onAudioIn(inInterleaved.data(), numFrames, inChannels);
			}
			if (fits && outChannels > 0) callback->onAudioOut(outInterleaved.data(), numFrames, outChannels, sampleRate);
			for (int c = 0; c < numOut; c++) {
				if (out[c] == nullptr) continue;
				for (int f = 0; f < numFrames; f++) out[c][f] = (fits && c < outChannels ? outInterleaved[(size_t)f * outChannels + c] : 0.0f);
			}
		}
};

}

std::unique_ptr<AudioStream> makeAudioStream()
{
	return std::make_unique<JuceAudioStream>();
}
