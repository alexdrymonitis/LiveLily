#ifndef OF_APP_H
#define OF_APP_H

#include "ofMain.h"
#include "osc.h"
#include "serial.h"
#include "audio.h"
#include "frontend.h"
#include "session.h"
#include "of_instrument_display.h"
#include "of_grid_painter.h"
#include "keys.h"
#include "view_state.h"
#include "workspace.h"

// what is drawn of an instrument (its staff and its notes), which the front end attaches to every instrument
using ScoreView = OfInstrumentDisplay;
#include <map>
#include <vector>
#include <utility> // to add pair
#include <mutex>
#include <atomic>
#include <thread>
#include "editor.h"
#include "function.h"
#include "sequencer.h"
#include "utils.h"
#include "instrument.h"
#include "color.h"
#include "of_color.h"
#include "ofTimer.h"
#ifdef USEPYO
#include "PyoClass.h"
#endif

class ofApp : public ofBaseApp, public AudioCallback, public FrontendHost, public EditorHost, public WorkspaceHost
{
	public:
		// the engine of LiveLily (see session.h): the state of the session, the parser, and the sequencer
		// declared first, so that it is destroyed after everything else of the app (e.g. the sequencer's thread calls the app)
		Session session;
		// the state of the view: the window, the positions of the staffs, the piano roll, the beat visualization and the area below the panes
		ViewState view;
		// basic OF program structure
		//---------------------------------
		void setup();
		void update();
		void draw();
		void drawScore();
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
		// what the workspace asks of the app (see workspace.h)
		Metrics workspaceMetrics() override { return {(float)view.screenWidth, (float)view.screenHeight, oneCharacterWidth, cursorHeight}; }
		ScorePlacement scorePlacement() override;
		void applyFont(bool reload, bool layout) override
		{
			if (reload) fontLoaded = false;
			setFontSize(layout);
		}
		void changeFontSize(int delta) override;
		void changeScoreSize(int delta) override;
		void changeBrightness(float delta) override { brightnessCoeff = std::min(1.f, std::max(0.f, brightnessCoeff + delta)); }
		void toggleFullscreen() override
		{
			ofToggleFullscreen();
			fullScreen = !fullScreen;
		}
		void hideScore() override { setShowScore(-1); }
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
		void belowAreaMoved(float prevTop) override { moveScoreWithTraceback(prevTop); }
		void belowAreaReset() override { resetScoreYOffset(); }
		Color belowAreaBackground() override { return toColor(backgroundColor); }
		// draws text with many lines, with the line height of the editor
		void drawLines(const std::string& str, float x, float y);
		void moveScoreWithTraceback(float prevTracebackYCoord);
		void drawPianoRoll();
		// sets the range of the keys of the piano roll (MIDI notes), from a white key to a white key, and the size of the keys
		void setPianoRollRange(int lowestKey, int highestKey);
		// a note name with its octave (e.g. c1, fis4, a5) as a MIDI note, or -1
		int noteNameToMidi(const std::string& name);
		// the place of a note on the keyboard of the piano roll, in widths of white keys from the lowest white key
		// for the lane it rolls in (and the black key, for a black key), as on a piano, where the three white keys from C to E
		// have five equal lanes, and the four white keys from F to B have seven, so black keys lean towards the outer keys of their group
		std::pair<float, float> pianoRollLane(int note);
		void drawScope();
		void drawSequencer();

		void sendBeatVizInfo(int bar);
		void moveCursorOnShiftReturn();
		//---------------------------------
		// overloaded keyPressed and keyReleased functions
		// so out-of-focus editors can receive input from OSC
		void keyPressedOsc(int key, int thisEditor) override;
		void keyReleasedOsc(int key, int thisEditor) override;
		// OF keyboard input functions
		void keyPressed(int key) override;
		void keyReleased(int key) override;
		// the following two functions are used to check for key modifiers received from a Python pane
		pyStdoutStrKeyModifier checkPyStdoutKeyModifier(int ndx);
		void handleKeyModifier(pyStdoutStrKeyModifier modifierStruct);
		//---------------------------------
		// moves the score away from the active pane, if it covers it (with Alt+number and Ctrl+w)
		void revealActivePane() override;
		// what the engine asks of the user interface (see frontend.h)
		void notePlaying(int instNdx, int bar, int noteNdx, bool playing) override { workspace.storeActiveEditorElement(instNdx, bar, noteNdx, playing); }
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
		void showMouseCursor(bool show) override { if (show) ofShowCursor(); else ofHideCursor(); }
		float frameRate() override { return ofGetFrameRate(); }
		void setFrameRate(int framesPerSecond) override { ofSetFrameRate(framesPerSecond); }
		void setPaneLanguage(int pane, int language) override { workspace.setPaneLanguage(pane, language); }
		bool openHelp(int pane, const std::string& filePath, int newPane) override { return workspace.openHelp(pane, filePath, newPane); }
		void setEditorOption(int pane, const std::string& option, const std::string& value) override { workspace.setEditorOption(pane, option, value); }
		void setPaneActive(int pane, bool active) override { workspace.setPaneActive(pane, active); }
		void sendKeysTo(int pane, const std::string& host, int targetPane) override { workspace.sendKeysTo(pane, host, targetPane); }
		void sendLinesTo(int pane, const std::string& host, int targetPane) override { workspace.sendLinesTo(pane, host, targetPane); }
		int tabSize(int pane) override { return workspace.tabSize(pane); }
		void connectLineToBar(int pane, int line, int instNdx, int bar) override { workspace.connectLineToBar(pane, line, instNdx, bar); }
		std::string scoreViewCommand(const std::vector<std::string>& methodAndArgs) override;
		void scoreMustUpdate() override { mustUpdateScore = true; }
		void instrumentCreated(int instNdx) override;
		void instrumentRemoved(int instNdx) override;
		void instrumentsCleared() override;
		void layoutInstrumentBar(int instNdx, int bar) override;
		bool scoreShown() override;
		void sequencerStarted(float beatStepsPerMs, uint64_t beatRampStart) override;
		void beat(uint64_t timeStamp, float stepsPerMs, uint64_t rampStart) override;
		void barStarted(uint64_t timeStamp) override;
		void loopChanged() override;
		void setPythonTyping(bool charByChar) override;
		// what the editor asks of the app (see editor_host.h); the other methods of EditorHost are the app's own methods of the same names
		Session& editorSession() override { return session; }
		Color editorForeground() override { return toColor(foregroundColor); }
		Color editorBackground() override { return toColor(backgroundColor); }
		float editorBrightness() override { return brightnessCoeff; }
		int scoreBesidePanesWidth() override;
		void typeFromPython(const std::string& text) override { view.pyStdoutStr += text; }
		void startTypingFromPython(int pane) override { view.whichPyPane = pane; view.typePyStr = true; }
		int pythonTypingPane() override { return view.whichPyPane; }
		std::string clipboard() override { return ofGetWindowPtr()->getClipboardString(); }
		void setClipboard(const std::string& text) override { ofGetWindowPtr()->setClipboardString(text); }
		std::string chooseFileToSave(const std::string& defaultName, const std::string& message) override;
		std::string chooseFileToOpen(const std::string& message) override;
		int windowWidth() override { return ofGetWidth(); }
		int windowHeight() override { return ofGetHeight(); }
		std::pair<int, int> mousePosition() override { return {mouseX, mouseY}; }
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
		void setScoreCoords();
		void resetScoreYOffset();
		void setScoreNotes(int barIndex) override;
		void setNotePositions(int bar) override;
		void setNotePositions(int bar, int numBars) override;
		void swapScorePosition(int orientation) override;
		void setScoreSizes();
		void calculateStaffPositions(int bar, bool windowChanged) override;
		void setShowScore(int ndx);
		bool isScoreVisible();
		// test mode, set from the command line (see main.cpp and testmode.cpp)
		std::string testFile;
		std::string testOut;
		bool testDone = false;
		// with --screenshot, the window is saved to this file after the test has run
		std::string testScreenshot;
		uint64_t testDoneFrame = 0;
		void runTest();
		// adds the state and the grids of the panes and of the area below them to the dump, as they were drawn (see draw())
		void appendTestEnd();
		bool testScreenWritten = false;
		// the keys a test file types after it has run (see runTest()), pressed one by one in update()
		std::vector<KeyEvent> testKeys;
		size_t testKeyNdx = 0;
		//---------------------------------
		// the sequencer functions of the system
		int setPatternIndex(bool increment);
		unsigned setLoopIter(int ptrnIdx);
		int setInstIndex(int ptrnIdx, int i);
		int setBar(int ptrnIdx, int idx, int i);
		//---------------------------------
		// release memory on exit
		void exit();
		//---------------------------------
		// rest of OF functions
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void mouseEntered(int x, int y);
		void mouseExited(int x, int y);
		void resizeWindow();
		void windowResized(int w, int h);
		void gotMessage(ofMessage msg);
		void dragEvent(ofDragInfo dragInfo);
		//---------------------------------
		// audio functions
		// called by the audio stream (see audio.h) from the audio thread
		void onAudioIn(const float* in, int numFrames, int numChannels) override;
		void onAudioOut(float* out, int numFrames, int numChannels, int sampleRate) override;

		// a mutex to avoid accessing the same data by different threads at the same time
		ofMutex mutex;

		float screenSplitHorizontal;
		float oneCharacterWidth;
		int halfCharacterWidth;


		// oscilloscope drawing stuff
		// the last output buffer, copied by the audio thread for the oscilloscope, and its number of channels
		std::vector<float> scopeSamples;
		int scopeChannels = 0;
		std::mutex scopeMutex;
		std::vector<ofPolyline> scopeWaveforms;
		float scopeRms;

		// paints the area below the panes (Workspace::belowGrid), whose rows start at its top, with the text at the bottom of every row
		void paintBelowGrid();




		// the map of vectors below is used to determine the number of lines to execute
		// if we start from the first line of a chunk that is not indented
		std::map<int, std::vector<std::string>> noIndentCheck;

		// moves note heads of the voices of a staff that sound together so they don't collide, as LilyPond does by default
		void applyMergeLayout(int bar);




		enum errorTypeNdx {note, warning, error};

		int notesID; // used mainly for debugging note and staff objects

		bool fullScreen;
		bool isWindowResized;
		uint64_t windowResizeTimeStamp;

		ofColor backgroundColor;
		ofColor foregroundColor;
		ofColor beatColor;
		ofColor scoreBackgroundColor;
		float brightnessCoeff;

		ofTrueTypeFont font;
		ofTrueTypeFont instFont;
		bool fontLoaded;


		std::map<int, std::string> allStrings;
		std::map<int, int> allStringStartPos;
		std::map<int, std::vector<int>> allStringTabs;
		//std::vector<std::vector<int>> curlyIndexes;


		int fontSize; // the size of the font of the editor
		int instFontSize; // the size of the font of the instrument display
		float cursorHeight;
		// from the baseline of the text to the bottom of its row in the editor (see setFontSize())
		float characterOffset = 0;


		// global line width
		int lineWidth;


		// STDERR

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
		// map that stores which editors can receive data from OSC
		std::map<int, bool> fromOsc;
		// and a map with the OSC address for each editor
		std::map<int, std::string> fromOscAddr;

		std::map<int, int> instNameWidths;
		bool showBarCount;
		bool showTempo;

		bool exiting;

		// mouse's X and Y coords
		int mouseX;
		int mouseY;
};

#endif
