#ifndef FRONTEND_H
#define FRONTEND_H

// what the engine (the parser and the sequencer) asks of the user interface, which the front end implements (ofApp for now)
// so that the engine doesn't depend on the front end (see doc/stage1_inventory.md)
class FrontendHost
{
	public:
		virtual ~FrontendHost() = default;
		// the note of an instrument that plays, which the editor highlights, by its bar and its index in the bar
		// called from the sequencer thread, when the note starts (playing is true) and when it stops
		virtual void notePlaying(int instNdx, int bar, int noteNdx, bool playing) = 0;
		// the same for a rest, which stays highlighted until the next note or rest replaces it
		virtual void restPlaying(int instNdx, int bar, int noteNdx) = 0;
		// nothing of an instrument is highlighted (e.g. a rest followed by a muted note)
		virtual void nothingPlaying(int instNdx) = 0;
};

#endif
