#ifndef FRONTEND_H
#define FRONTEND_H

#include <string>

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
		// the message (a note, a warning, or an error, see errorCode) shown at a line of the code that ran in a pane, or none
		// called from any thread: boundFunction is the name of the function the sequencer called (e.g. bound to an instrument)
		// whose message goes to the line where the function is defined, or empty
		// errorCode is 1 for a note, 2 for a warning, 3 for an error (see Editor::setTraceback())
		virtual void setLineMessage(int pane, int line, int errorCode, const std::string& text, const std::string& boundFunction) = 0;
		virtual void clearLineMessage(int pane, int line, const std::string& boundFunction) = 0;
};

#endif
