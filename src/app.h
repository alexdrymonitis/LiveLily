#ifndef APP_H
#define APP_H

#include "app_window.h"
#include "canvas.h"
#include "osc.h"
#include "serial.h"
#include "audio.h"
#include "frontend.h"
#include "session.h"
#include "score_view.h"
#include "keys.h"
#include "view_state.h"
#include "workspace.h"
#include "views.h"

#include <map>
#include <vector>
#include <utility> // to add pair
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include "editor.h"
#include "function.h"
#include "sequencer.h"
#include "utils.h"
#include "instrument.h"
#include "color.h"
#ifdef USEPYO
#include "PyoClass.h"
#endif

// LiveLily, as every front end with a window has it (the openFrameworks app, the JUCE app, and the headless runner):
// the engine (the session), the panes (the workspace), and the views, what they ask of each other, the keys
// and what happens in every frame (OSC, typing from Python, the oscilloscope), and drawing them (see doc/stage4_inventory.md, step 8)
// the front end gives it a window (see app_window.h), sets the canvas the views draw with (see canvas.h), and calls setup(), update(), draw()
// and passes on the keys and the size of the window
class App : public AudioCallback, public FrontendHost, public EditorHost, public WorkspaceHost
{
	public:
		explicit App(AppWindow& window) : window(window) {}
		// the window of the front end, declared first, as everything else may reach it
		AppWindow& window;
		// the engine of LiveLily (see session.h): the state of the session, the parser, and the sequencer
		// declared first, so that it is destroyed after everything else of the app (e.g. the sequencer's thread calls the app)
		Session session;
		// the state of the view: the window, the positions of the staffs, the piano roll, the beat visualization and the area below the panes
		// LiveLily starts in a window of a size, in test mode (see test_run.h) or not, with its resources in a directory (see paths.h)
		void setup(int width, int height, bool testMode, const std::string& resourcesPath);
		bool testMode = false;
		// what happens in every frame, before it is drawn (not in test mode, whose frames only type keys)
		void update();
		// draws the panes, the views, and the area below the panes, with the window and the canvas
		void draw();
		// the events of the playback (beats, bars, notes), for the headless runner, which writes them with their times
		std::function<void(const std::string&)> onPlayback;
		// the panes, their layout and the area below them (see workspace.h), declared after the session, which it holds
		Workspace workspace{session, *this};
		// the names the app had for the state of the workspace, which it still uses
		std::map<int, Editor>& editors = workspace.editors; // keys are panes indexes
		std::map<int, int>& numPanes = workspace.numPanes; // rows and columns of panes
		int& paneSplitOrientation = workspace.paneSplitOrientation;
		int& tracebackNumLinesShown = workspace.tracebackNumLinesShown;
		int& shellNumLinesShown = workspace.shellNumLinesShown;
		std::string& shellStr = workspace.shellStr;
		std::string& shellLsOutputStr = workspace.shellLsOutputStr;
		int& numShellLines = workspace.numShellLines;
		int& numShellLsOutputLines = workspace.numShellLsOutputLines;
		int& shellTabCounter = workspace.shellTabCounter;
		int& shellStrCursorPos = workspace.shellStrCursorPos;
		size_t& maxShellChars = workspace.maxShellChars;
		Color& shellStrColor = workspace.shellStrColor;
		int& shellScroll = workspace.shellScroll;
		int& shellTotalLines = workspace.shellTotalLines;
		int& shellVisibleLines = workspace.shellVisibleLines;
		bool& executingBind = workspace.executingBind;
		CellGrid& belowGrid = workspace.belowGrid;
		// the views beside the panes: the score, the piano roll, the sequencer, the oscilloscope (see views.h)
		Views views{session, workspace, brightnessCoeff, lineWidth, oneCharacterWidth, cursorHeight, exiting};
		// the names the app had for the state of the views, which it still uses
		ViewState& view = views.view;
		std::vector<float>& scopeSamples = views.scopeSamples;
		int& scopeChannels = views.scopeChannels;
		std::mutex& scopeMutex = views.scopeMutex;
		std::vector<std::vector<std::pair<float, float>>>& scopeWaveforms = views.scopeWaveforms;
		float& scopeRms = views.scopeRms;
		Color& backgroundColor = views.backgroundColor;
		Color& foregroundColor = views.foregroundColor;
		Color& beatColor = views.beatColor;
		Color& scoreBackgroundColor = views.scoreBackgroundColor;
		std::shared_ptr<Font>& instFont = views.instFont;
		int& instFontSize = views.instFontSize;
		bool& showScore = views.showScore;
		int& scoreType = views.scoreType;
		std::atomic<bool>& showScope = views.showScope;
		float& scoreXOffset = views.scoreXOffset;
		float& scoreYOffset = views.scoreYOffset;
		float& scoreBackgroundWidth = views.scoreBackgroundWidth;
		float& scoreBackgroundHeight = views.scoreBackgroundHeight;
		bool& scoreMoveXCommand = views.scoreMoveXCommand;
		bool& scoreMoveYCommand = views.scoreMoveYCommand;
		int& scoreOrientation = views.scoreOrientation;
		float& scoreXStartPnt = views.scoreXStartPnt;
		int& numBarsToDisplay = views.numBarsToDisplay;
		float& notesLength = views.notesLength;
		bool& mustUpdateScore = views.mustUpdateScore;
		bool& scoreUpdated = views.scoreUpdated;
		bool& scoreChangeOnLastBar = views.scoreChangeOnLastBar;
		std::map<int, int>& instNameWidths = views.instNameWidths;
		bool& showBarCount = views.showBarCount;
		bool& showTempo = views.showTempo;
		// what the workspace asks of the app (see workspace.h)
		Metrics workspaceMetrics() override { return {(float)view.screenWidth, (float)view.screenHeight, oneCharacterWidth, cursorHeight}; }
		ScorePlacement scorePlacement() override { return views.scorePlacement(); }
		void applyFont(bool reload, bool layout) override
		{
			if (reload) fontLoaded = false;
			setFontSize(layout);
		}
		void changeFontSize(int delta) override;
		void changeScoreSize(int delta) override { views.changeScoreSize(delta); }
		void changeBrightness(float delta) override { brightnessCoeff = std::min(1.f, std::max(0.f, brightnessCoeff + delta)); }
		void toggleFullscreen() override
		{
			window.toggleFullscreen();
			fullScreen = !fullScreen;
		}
		void hideScore() override { views.setShowScore(-1); }
		// the state of the keys, which the workspace keeps (see workspace_keys.cpp)
		bool& shiftPressed = workspace.shiftPressed;
		bool& ctrlPressed = workspace.ctrlPressed;
		bool& altPressed = workspace.altPressed;
		bool& gPressed = workspace.gPressed;
		bool& dPressed = workspace.dPressed;
		int& editorBindingKey = workspace.editorBindingKey;
		int& editorBindingArg = workspace.editorBindingArg;
		bool& editorBindingIsDollar = workspace.editorBindingIsDollar;
		void quitNow() override { exit(); }
		void belowAreaMoved(float prevTop) override { views.moveScoreWithTraceback(prevTop); }
		void belowAreaReset() override { views.resetScoreYOffset(); }
		Color belowAreaBackground() override { return backgroundColor; }
		void moveCursorOnShiftReturn();
		//---------------------------------
		// overloaded keyPressed and keyReleased functions
		// so out-of-focus editors can receive input from OSC
		void keyPressedOsc(int key, int thisEditor) override;
		void keyReleasedOsc(int key, int thisEditor) override;
		// the keys of the window (with the codes of keys.h)
		void keyPressed(int key) override;
		void keyReleased(int key) override;
		// the following two functions are used to check for key modifiers received from a Python pane
		pyStdoutStrKeyModifier checkPyStdoutKeyModifier(int ndx);
		void handleKeyModifier(pyStdoutStrKeyModifier modifierStruct);
		//---------------------------------
		// moves the score away from the active pane, if it covers it (with Alt+number and Ctrl+w)
		void revealActivePane() override { views.revealActivePane(); }
		// what the engine asks of the user interface (see frontend.h)
		void notePlaying(int instNdx, int bar, int noteNdx, bool playing) override
		{
			workspace.storeActiveEditorElement(instNdx, bar, noteNdx, playing);
			if (playing && onPlayback) onPlayback("note inst " + std::to_string(instNdx) + " bar " + std::to_string(bar) + " note " + std::to_string(noteNdx));
		}
		void restPlaying(int instNdx, int bar, int noteNdx) override { workspace.storeActiveEditorRest(instNdx, bar, noteNdx); }
		void nothingPlaying(int instNdx) override { workspace.clearActiveEditorElement(instNdx); }
		void setLineMessage(int pane, int line, int errorCode, const std::string& text, const std::string& boundFunction) override
		{
			workspace.setLineMessage(pane, line, errorCode, text, boundFunction);
		}
		void clearLineMessage(int pane, int line, const std::string& boundFunction) override { workspace.clearLineMessage(pane, line, boundFunction); }
		void storeEditorNoteSpans(int lineNum, int instNdx, int barNdx, int numNotes) override { workspace.storeEditorNoteSpans(lineNum, instNdx, barNdx, numNotes); }
		// what the editors ask of the app, which the workspace answers (see workspace_editors.cpp and workspace_alignment.cpp)
		std::vector<EditorNoteSpan> getActiveEditorNoteSpans(int pane) override { return workspace.getActiveEditorNoteSpans(pane); }
		void parseStrings(int index, int numLines) override { workspace.parseStrings(index, numLines); }
		void alignAfterEdit(int pane, bool space) override { workspace.alignAfterEdit(pane, space); }
		void alignAfterPaste(int pane) override { workspace.alignAfterPaste(pane); }
		bool startsMelodicLine(const std::string& word) override { return workspace.startsMelodicLine(word); }
		std::vector<std::string> getCommandCompletions() override { return workspace.getCommandCompletions(); }
		std::vector<std::string> getMethodCompletions(const std::string& name) override { return workspace.getMethodCompletions(name); }
		std::vector<std::string> getArgumentCompletions() override { return workspace.getArgumentCompletions(); }
		std::vector<std::string> pythonNames(const std::string& dotted) override { return workspace.pythonNames(dotted); }
		std::vector<std::string> pythonSignature(const std::string& dotted) override { return workspace.pythonSignature(dotted); }
		std::map<std::pair<int, int>, std::vector<std::vector<EditorNoteSpan>>>& editorNoteSpans = workspace.editorNoteSpans;
		void showMouseCursor(bool show) override { window.showMouseCursor(show); }
		float frameRate() override { return window.frameRate(); }
		void setFrameRate(int framesPerSecond) override { window.setFrameRate(framesPerSecond); }
		void setPaneLanguage(int pane, int language) override { workspace.setPaneLanguage(pane, language); }
		bool openHelp(int pane, const std::string& filePath, int newPane) override { return workspace.openHelp(pane, filePath, newPane); }
		void setEditorOption(int pane, const std::string& option, const std::string& value) override { workspace.setEditorOption(pane, option, value); }
		void setPaneActive(int pane, bool active) override { workspace.setPaneActive(pane, active); }
		void sendKeysTo(int pane, const std::string& host, int targetPane) override { workspace.sendKeysTo(pane, host, targetPane); }
		void sendLinesTo(int pane, const std::string& host, int targetPane) override { workspace.sendLinesTo(pane, host, targetPane); }
		int tabSize(int pane) override { return workspace.tabSize(pane); }
		void connectLineToBar(int pane, int line, int instNdx, int bar) override { workspace.connectLineToBar(pane, line, instNdx, bar); }
		std::string scoreViewCommand(const std::vector<std::string>& methodAndArgs) override { return views.scoreViewCommand(methodAndArgs); }
		void scoreMustUpdate() override { mustUpdateScore = true; }
		void instrumentCreated(int instNdx) override { views.instrumentCreated(instNdx); }
		void instrumentRemoved(int instNdx) override { views.instrumentRemoved(instNdx); }
		void instrumentsCleared() override;
		void layoutInstrumentBar(int instNdx, int bar) override { views.layoutInstrumentBar(instNdx, bar); }
		bool scoreShown() override { return views.scoreShown(); }
		void sequencerStarted(float beatStepsPerMs, uint64_t beatRampStart) override
		{
			views.sequencerStarted(beatStepsPerMs, beatRampStart);
			if (onPlayback) onPlayback("start");
		}
		void beat(uint64_t timeStamp, float stepsPerMs, uint64_t rampStart) override
		{
			views.beat(timeStamp, stepsPerMs, rampStart);
			if (onPlayback) onPlayback("beat");
		}
		void barStarted(uint64_t timeStamp) override
		{
			views.barStarted(timeStamp);
			if (onPlayback) onPlayback("bar");
		}
		void loopChanged() override
		{
			views.loopChanged();
			if (onPlayback) onPlayback("loop changed");
		}
		void setPythonTyping(bool charByChar) override;
		// what the editor asks of the app (see editor_host.h); the other methods of EditorHost are the app's own methods of the same names
		Session& editorSession() override { return session; }
		Color editorForeground() override { return foregroundColor; }
		Color editorBackground() override { return backgroundColor; }
		float editorBrightness() override { return brightnessCoeff; }
		int scoreBesidePanesWidth() override { return views.scoreBesidePanesWidth(); }
		void typeFromPython(const std::string& text) override { view.pyStdoutStr += text; }
		void startTypingFromPython(int pane) override { view.whichPyPane = pane; view.typePyStr = true; }
		int pythonTypingPane() override { return view.whichPyPane; }
		std::string clipboard() override { return window.clipboard(); }
		void setClipboard(const std::string& text) override { window.setClipboard(text); }
		std::string chooseFileToSave(const std::string& defaultName, const std::string& message) override { return window.chooseFileToSave(defaultName, message); }
		std::string chooseFileToOpen(const std::string& message) override { return window.chooseFileToOpen(message); }
		int windowWidth() override { return view.screenWidth; }
		int windowHeight() override { return view.screenHeight; }
		std::pair<int, int> mousePosition() override { return window.mousePosition(); }
		void setPaneFromOsc(int pane, const std::string& address) override { fromOscAddr[pane] = address; fromOsc[pane] = true; }
		//---------------------------------
		//---------------------------------
		void sendCountdownToParts(int countdown);
		void sendStopCountdownToParts();
		void sendNewBarToParts(std::string barName, int barIndex);
		void resetEditorStrings(int index, int numLines);
		void createEmptyMelody(int index);
		void resizePatternVectors();
		void sendPushPopPattern();
		bool areWeInsideChord(size_t size, unsigned i, unsigned *chordNotesIndexes);
		//---------------------------------
		// coordinates/positions/sizes functions
		// set the global font size to calculate dimensions
		void setFontSize(bool calculateCoords);
		void setScoreNotes(int barIndex) override { views.setScoreNotes(barIndex); }
		void setNotePositions(int bar) override { views.setNotePositions(bar); }
		void setNotePositions(int bar, int numBars) override { views.setNotePositions(bar, numBars); }
		void swapScorePosition(int orientation) override { views.swapScorePosition(orientation); }
		void calculateStaffPositions(int bar, bool windowChanged) override { views.calculateStaffPositions(bar, windowChanged); }
		//---------------------------------
		// the sequencer functions of the system
		int setPatternIndex(bool increment);
		unsigned setLoopIter(int ptrnIdx);
		int setInstIndex(int ptrnIdx, int i);
		int setBar(int ptrnIdx, int idx, int i);
		//---------------------------------
		// stops the sequencer, the audio, MIDI and the serial port, tells the score parts, and closes the window
		void exit();
		//---------------------------------
		// the window has a new size, after which the panes and the views are laid out again (in update(), once it stops changing)
		void windowResized(int w, int h);
		void resizeWindow();
		//---------------------------------
		// audio functions
		// called by the audio stream (see audio.h) from the audio thread
		void onAudioIn(const float* in, int numFrames, int numChannels) override;
		void onAudioOut(float* out, int numFrames, int numChannels, int sampleRate) override;

		// a mutex to avoid accessing the same data by different threads at the same time
		std::mutex mutex;

		float screenSplitHorizontal = 0;
		float oneCharacterWidth = 11;
		int halfCharacterWidth = 5;



		// paints the area below the panes (Workspace::belowGrid), whose rows start at its top, with the text at the bottom of every row
		void paintBelowGrid();




		// the map of vectors below is used to determine the number of lines to execute
		// if we start from the first line of a chunk that is not indented
		std::map<int, std::vector<std::string>> noIndentCheck;





		enum errorTypeNdx {note, warning, error};

		int notesID = 0; // used mainly for debugging note and staff objects

		bool fullScreen = false;
		bool isWindowResized = false;
		uint64_t windowResizeTimeStamp = 0;

		float brightnessCoeff = 0.85;

		bool fontLoaded = false;


		std::map<int, std::string> allStrings;
		std::map<int, int> allStringStartPos;
		std::map<int, std::vector<int>> allStringTabs;
		//std::vector<std::vector<int>> curlyIndexes;


		int fontSize = 18; // the size of the font of the editor
		float cursorHeight = 22;
		// from the baseline of the text to the bottom of its row in the editor (see setFontSize())
		float characterOffset = 0;


		// global line width
		int lineWidth = 2;


		// STDERR

		// variables for the score
		// map that stores which editors can receive data from OSC
		std::map<int, bool> fromOsc;
		// and a map with the OSC address for each editor
		std::map<int, std::string> fromOscAddr;


		bool exiting = false;
};

#endif
