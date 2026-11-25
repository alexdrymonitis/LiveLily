#ifndef AUDIO_H
#define AUDIO_H

#include <memory>
#include <string>
#include <vector>

// the audio input and output of LiveLily (for pyo and the click track), which the platform implements
// (with openFrameworks' ofSoundStream for now, see of_audio.cpp)

// an audio device of the system, as AudioStream::devices() lists it
struct AudioDevice
{
	int id = -1;
	// the audio API of the device (e.g. ALSA or JACK), as the platform numbers it
	int api = -1;
	std::string name;
};

struct AudioSettings
{
	// the system's default devices are used unless a device is set
	bool inDeviceSet = false;
	AudioDevice inDevice;
	bool outDeviceSet = false;
	AudioDevice outDevice;
	int inChannels = 0;
	int outChannels = 2;
	int sampleRate = 44100;
	int bufferSize = 256;
	int numBuffers = 4;
};

// the samples are interleaved (the samples of all the channels of a frame, then those of the next frame)
class AudioCallback
{
	public:
		virtual ~AudioCallback() = default;
		virtual void onAudioIn(const float* in, int numFrames, int numChannels) = 0;
		virtual void onAudioOut(float* out, int numFrames, int numChannels, int sampleRate) = 0;
};

class AudioStream
{
	public:
		virtual ~AudioStream() = default;
		virtual std::vector<AudioDevice> devices() = 0;
		// starts the stream, which calls the callback from the audio thread, and returns false if it can't start
		virtual bool setup(const AudioSettings& settings, AudioCallback* callback) = 0;
		virtual void close() = 0;
		// the device the stream plays to, with its sample rate and its buffer size, as the platform names it, or empty if it isn't open
		virtual std::string currentDevice() { return ""; }
};

// the platform makes it (of_audio.cpp for now)
std::unique_ptr<AudioStream> makeAudioStream();

#endif
