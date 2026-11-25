#include "platform.h"
#include <juce_audio_devices/juce_audio_devices.h>

struct PlatformScope::Impl
{
	juce::ScopedJuceInitialiser_GUI juce;
};

PlatformScope::PlatformScope() : impl(std::make_unique<Impl>()) {}

PlatformScope::~PlatformScope() = default;

void PlatformScope::findDevices()
{
	// asking for the ports starts looking for them
	juce::MidiOutput::getAvailableDevices();
	juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
}
