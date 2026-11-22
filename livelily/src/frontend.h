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
		// the window: the mouse cursor over it (\cursor), and how many times a second it is drawn (\framerate)
		virtual void showMouseCursor(bool show) = 0;
		virtual float frameRate() = 0;
		virtual void setFrameRate(int framesPerSecond) = 0;
		// the language of the code of a pane: 0 for LiveLily, 1 for Python, 2 for Lua (\livelily, \python, \lua)
		virtual void setPaneLanguage(int pane, int language) = 0;
		// opens a help file in a pane, in place of its text, or in a new pane beside it (newPane is 1) or below it (newPane is 2)
		// and returns false if the file can't be opened (\help)
		virtual bool openHelp(int pane, const std::string& filePath, int newPane) = 0;
		// an option of the editor of a pane (\editor), which the parser has checked:
		// alignment (on, off, short, or long), animation, autobrackets, or autocomplete (on or off)
		virtual void setEditorOption(int pane, const std::string& option, const std::string& value) = 0;
		// whether the code of a pane runs (\active and \inactive)
		virtual void setPaneActive(int pane, bool active) = 0;
		// sends the keys typed in a pane, or the lines executed in it, to a pane of LiveLily on another computer (\sendkeys, \sendlines)
		virtual void sendKeysTo(int pane, const std::string& host, int targetPane) = 0;
		virtual void sendLinesTo(int pane, const std::string& host, int targetPane) = 0;
		// the number of spaces of a tab in a pane, for the lines the parser writes (e.g. a bar of one line split into lines)
		virtual int tabSize(int pane) = 0;
		// a line of a pane is the melodic line of an instrument in a bar, for the animation of the editor
		virtual void connectLineToBar(int pane, int line, int instNdx, int bar) = 0;
};

#endif
