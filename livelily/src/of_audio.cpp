// the audio stream of LiveLily (see audio.h) with openFrameworks' ofSoundStream
#include "audio.h"
#include "ofSoundStream.h"
#include "ofSoundBuffer.h"

namespace {

class OfAudioStream : public AudioStream, public ofBaseSoundInput, public ofBaseSoundOutput
{
	public:
		std::vector<AudioDevice> devices() override
		{
			std::vector<AudioDevice> list;
			for (const ofSoundDevice& device : stream.getDeviceList()) list.push_back({device.deviceID, (int)device.api, device.name});
			return list;
		}

		bool setup(const AudioSettings& s, AudioCallback* cb) override
		{
			callback = cb;
			ofSoundStreamSettings settings;
			ofSoundDevice device;
			if (s.inDeviceSet && findDevice(s.inDevice, device)) settings.setInDevice(device);
			if (s.outDeviceSet && findDevice(s.outDevice, device)) settings.setOutDevice(device);
			settings.numInputChannels = s.inChannels;
			settings.numOutputChannels = s.outChannels;
			settings.sampleRate = s.sampleRate;
			settings.bufferSize = s.bufferSize;
			settings.numBuffers = s.numBuffers;
			if (s.inChannels > 0) settings.setInListener(this);
			settings.setOutListener(this);
			return stream.setup(settings);
		}

		void close() override { stream.close(); }

		void audioIn(ofSoundBuffer& buffer) override
		{
			if (callback) callback->onAudioIn(buffer.getBuffer().data(), (int)buffer.getNumFrames(), (int)buffer.getNumChannels());
		}

		void audioOut(ofSoundBuffer& buffer) override
		{
			if (callback) callback->onAudioOut(buffer.getBuffer().data(), (int)buffer.getNumFrames(), (int)buffer.getNumChannels(), (int)buffer.getSampleRate());
		}

	private:
		ofSoundStream stream;
		AudioCallback* callback = nullptr;

		// the device of the list of the system that a device stands for, by its number and its API
		bool findDevice(const AudioDevice& wanted, ofSoundDevice& found)
		{
			for (const ofSoundDevice& device : stream.getDeviceList()) {
				if (device.deviceID == wanted.id && (int)device.api == wanted.api) {
					found = device;
					return true;
				}
			}
			return false;
		}
};

}

std::unique_ptr<AudioStream> makeAudioStream()
{
	return std::make_unique<OfAudioStream>();
}
