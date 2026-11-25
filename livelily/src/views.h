#ifndef VIEWS_H
#define VIEWS_H

#include "canvas.h"
#include "color.h"
#include "view_state.h"
#include "score_view.h"
#include "workspace.h"
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

class Session;

// the views of LiveLily beside (or below) the panes: the score, the piano roll, the sequencer, and the oscilloscope
// their layout, what the engine tells them (see FrontendHost), and their drawing (see doc/stage4_inventory.md, step 7)
class Views
{
	public:
		// the views read some values of the front end: the brightness, the width of the lines, the size of a cell of the editor's font
		// and whether it is quitting (they draw with the canvas the front end sets, see canvas.h)
		Views(Session& session, Workspace& workspace, float& brightnessCoeff, int& lineWidth, float& oneCharacterWidth, float& cursorHeight,
				bool& exiting)
			: session(session), workspace(workspace), brightnessCoeff(brightnessCoeff), lineWidth(lineWidth),
			  oneCharacterWidth(oneCharacterWidth), cursorHeight(cursorHeight), exiting(exiting) {}

		// the state of the view: the window, the positions of the staffs, the piano roll, the beat visualization and the area below the panes
		ViewState view;

		// the size of the window, which the front end gives when it starts, before it lays out the panes
		void setWindow(int width, int height);
		// the state of the score, the beat, and the piano roll when LiveLily starts, once the panes are laid out
		// (the score takes the height of the panes), and the font of the names of the instruments
		void setup();

		// the views, which the front end draws in every frame
		void drawScore();
		void drawPianoRoll();
		void drawScope();
		void drawSequencer();

		// the area below the panes moved (it has more or fewer lines), so the score moves with it, or it is back at its base
		void moveScoreWithTraceback(float prevTracebackYCoord);
		void resetScoreYOffset();
		// sets the range of the keys of the piano roll (MIDI notes), from a white key to a white key, and the size of the keys
		void setPianoRollRange(int lowestKey, int highestKey);
		// a note name with its octave (e.g. c1, fis4, a5) as a MIDI note, or -1
		int noteNameToMidi(const std::string& name);
		// the place of a note on the keyboard of the piano roll, in widths of white keys from the lowest white key
		// for the lane it rolls in (and the black key, for a black key), as on a piano, where the three white keys from C to E
		// have five equal lanes, and the four white keys from F to B have seven, so black keys lean towards the outer keys of their group
		std::pair<float, float> pianoRollLane(int note);
		void sendBeatVizInfo(int bar);
		// moves the score away from the active pane, if it covers it (with Alt+number and Ctrl+w)
		void revealActivePane();

		// what the engine asks of the views (see FrontendHost in frontend.h)
		std::string scoreViewCommand(const std::vector<std::string>& methodAndArgs);
		void instrumentCreated(int instNdx);
		void instrumentRemoved(int instNdx);
		void layoutInstrumentBar(int instNdx, int bar);
		bool scoreShown();
		void sequencerStarted(float beatStepsPerMs, uint64_t beatRampStart);
		void beat(uint64_t timeStamp, float stepsPerMs, uint64_t rampStart);
		void barStarted(uint64_t timeStamp);
		void loopChanged();
		// what the panes ask of the views (see WorkspaceHost in workspace.h)
		WorkspaceHost::ScorePlacement scorePlacement();
		void changeScoreSize(int delta);
		// what the editors ask (see EditorHost in editor_host.h)
		int scoreBesidePanesWidth();

		// coordinates/positions/sizes functions
		void setScoreCoords();
		void setScoreNotes(int barIndex);
		void setNotePositions(int bar);
		void setNotePositions(int bar, int numBars);
		void swapScorePosition(int orientation);
		void setScoreSizes();
		void calculateStaffPositions(int bar, bool windowChanged);
		void setShowScore(int ndx);
		bool isScoreVisible();
		// moves note heads of the voices of a staff that sound together so they don't collide, as LilyPond does by default
		void applyMergeLayout(int bar);

		// oscilloscope drawing stuff
		// the last output buffer, copied by the audio thread for the oscilloscope, and its number of channels
		std::vector<float> scopeSamples;
		int scopeChannels = 0;
		std::mutex scopeMutex;
		// the waveforms of the channels, as points, which the front end makes from scopeSamples
		std::vector<std::vector<std::pair<float, float>>> scopeWaveforms;
		float scopeRms;

		Color backgroundColor = Colors::black;
		Color foregroundColor = Colors::white;
		Color beatColor = Colors::paleGreen;
		Color scoreBackgroundColor = Colors::white;
		// the font of the names of the instruments, and the font of the editor, for labels (e.g. of the oscilloscope)
		// which the front end loads again when its size changes
		std::shared_ptr<Font> instFont = noFont();
		std::shared_ptr<Font> labelFont = noFont();
		int instFontSize; // the size of the font of the instrument display

		// variables for the score
		bool showScore;
		int scoreType;
		// whether the oscilloscope is the view shown (see setShowScore()), which the audio thread reads to copy the output for it
		std::atomic<bool> showScope{false};
		// showNotes is in ViewState (view_state.h), and the sequencer asks for it with scoreShown()
		float scoreXOffset;
		float scoreYOffset;
		float scoreBackgroundWidth;
		float scoreBackgroundHeight;
		// variables to determine whether we call a command for moving the score
		bool scoreMoveXCommand;
		bool scoreMoveYCommand;
		int scoreOrientation;
		float scoreXStartPnt;
		int numBarsToDisplay;
		float notesLength; // used to derive the ratio when we display more than two bars horizontally
		bool mustUpdateScore;
		bool scoreUpdated;
		bool scoreChangeOnLastBar;
		std::map<int, int> instNameWidths;
		bool showBarCount;
		bool showTempo;

	private:
		Session& session;
		Workspace& workspace;
		std::map<int, Editor>& editors = workspace.editors;
		std::map<int, int>& numPanes = workspace.numPanes;
		int& paneSplitOrientation = workspace.paneSplitOrientation;
		// what the front end has, which the views read
		float& brightnessCoeff;
		int& lineWidth;
		float& oneCharacterWidth;
		float& cursorHeight;
		bool& exiting;
};

#endif
