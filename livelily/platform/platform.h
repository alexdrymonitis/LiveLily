#ifndef PLATFORM_H
#define PLATFORM_H

#include <memory>

// what the platform needs for the life of a program that isn't a JUCE app (the headless runner): JUCE's message manager
// whose thread is the one that makes this, the thread that lists and opens MIDI ports (the parser's)
// made before the session, so that it goes after it
class PlatformScope
{
	public:
		PlatformScope();
		~PlatformScope();
		// JUCE finds the MIDI ports through its message loop (macOS and Windows; Linux has its own MIDI, see alsa_midi.cpp), which a program without a JUCE app
		// doesn't run, so it runs it for a moment, so that the MIDI ports are found before the session lists or opens them
		void findDevices();
	private:
		struct Impl;
		std::unique_ptr<Impl> impl;
};

#endif
