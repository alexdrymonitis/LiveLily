#ifndef CLOCK_H
#define CLOCK_H

#include <cstdint>

// the time of the sequencer and the click track, in microseconds and milliseconds since the program started
// in test mode it can be simulated (see Sequencer::dryRun()), so that playback is stepped through without waiting
// and gives the same result every time
namespace Clock {
	uint64_t micros();
	uint64_t millis();
	// from simulate(true) on, micros() and millis() return a simulated time, which starts on a whole second after the real time and moves only with advance()
	void simulate(bool on);
	void advance(uint64_t us);
	bool simulated();
}

#endif
