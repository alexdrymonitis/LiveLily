// the audio stream of the test mode of the headless runner of LiveLily (see main.cpp), which never plays, as the tests don't play sound

#include "audio.h"

namespace {

class NullAudioStream : public AudioStream
{
	public:
		std::vector<AudioDevice> devices() override { return {}; }
		// the stream is taken as set up, but it never calls back
		bool setup(const AudioSettings&, AudioCallback*) override { return true; }
		void close() override {}
};

}

std::unique_ptr<AudioStream> makeTestAudioStream() { return std::make_unique<NullAudioStream>(); }
