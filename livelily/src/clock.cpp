#include "clock.h"
#include "ofUtils.h"
#include <atomic>

namespace {
	std::atomic<bool> isSimulated{false};
	std::atomic<uint64_t> simulatedMicros{0};
}

// the real time is the time of openFrameworks for now, which the drawing code compares with the time stamps of the sequencer
// (e.g. the piano roll), and it will be replaced by std::chrono when the drawing code no longer uses openFrameworks
uint64_t Clock::micros()
{
	return isSimulated ? simulatedMicros.load() : ofGetElapsedTimeMicros();
}

uint64_t Clock::millis()
{
	return micros() / 1000;
}

void Clock::simulate(bool on)
{
	// the simulated time starts on the next whole second after the real time, ahead of every time stamp taken so far
	// and always at the same place within a millisecond and a second, so that what is timed in milliseconds (e.g. glissandi) is the same every time
	if (on && !isSimulated) simulatedMicros = ((ofGetElapsedTimeMicros() / 1000000) + 1) * 1000000;
	isSimulated = on;
}

void Clock::advance(uint64_t us)
{
	simulatedMicros += us;
}

bool Clock::simulated()
{
	return isSimulated;
}
