#include "clock.h"
#include <atomic>
#include <chrono>

namespace {
	std::atomic<bool> isSimulated{false};
	std::atomic<uint64_t> simulatedMicros{0};
	// the start of the program, from which the time is counted
	const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

	uint64_t realMicros()
	{
		return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
	}
}

// the drawing code compares its time with the time stamps of the sequencer (e.g. the beat visualization and the piano roll) through Clock too
uint64_t Clock::micros()
{
	return isSimulated ? simulatedMicros.load() : realMicros();
}

uint64_t Clock::millis()
{
	return micros() / 1000;
}

void Clock::simulate(bool on)
{
	// the simulated time starts on the next whole second after the real time, ahead of every time stamp taken so far
	// and always at the same place within a millisecond and a second, so that what is timed in milliseconds (e.g. glissandi) is the same every time
	if (on && !isSimulated) simulatedMicros = ((realMicros() / 1000000) + 1) * 1000000;
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
