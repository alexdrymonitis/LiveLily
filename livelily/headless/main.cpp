// the headless runner of LiveLily: the engine (Session) and the panes (Workspace) with no window and no openFrameworks
// livelily_headless --test file.lyv [--out dump.txt] [--data directory]
//     runs the file in a pane, chunk by chunk, and types the keys of its "% test keys:" lines, as the test mode of the app does
//     (see src/test_run.h), and writes the same dump; the panes are as large as in the app's test mode, so the keys move the same way
//     but the score isn't drawn, so the screen (the last part of the dump) is written, though it isn't the app's
// livelily_headless --play seconds file.lyv [--data directory]
//     runs the file, and if it starts the sequencer, plays it for real (on its thread) for some seconds, with the MIDI ports
//     and the audio devices of the system (with JUCE, see platform/), and writes the beats, bars, and notes the front end is told about,
//     with their times, to check the thread and its timing

#include "session.h"
#include "workspace.h"
#include "test_run.h"
#include "codetext.h"
#include "clock.h"
#include "paths.h"
#include "platform.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

// the audio stream of test mode, which never plays (see platform.cpp)
std::unique_ptr<AudioStream> makeTestAudioStream();

namespace {

// the front end of the headless runner: what the engine, the editors, and the workspace ask of the user interface
// most of which the workspace answers, as in the app, and the rest is what a front end without a window gives
class HeadlessFrontend : public FrontendHost, public EditorHost, public WorkspaceHost, public AudioCallback
{
	public:
		Session& session;
		Workspace workspace{session, *this};
		float targetFrameRate = 60;
		bool showNotes = false;
		std::string clipboardText;
		// the playback, for --play
		bool recording = false;
		std::mutex eventsMutex;
		std::vector<std::string> events;

		explicit HeadlessFrontend(Session& session) : session(session) {}

		void record(const std::string& event)
		{
			if (!recording) return;
			std::lock_guard<std::mutex> lock(eventsMutex);
			events.push_back(std::to_string(Clock::millis()) + " " + event);
		}

		// the size of the window and of a cell of the app in test mode (1350 by 900 pixels, and DroidSansMono at 18 points)
		// so that the panes have the same numbers of lines and characters, and the keys move the cursor the same way
		static constexpr int windowW = 1350;
		static constexpr int windowH = 900;
		static constexpr int fontSize = 18;
		static constexpr float cellW = 11;
		static constexpr float cellH = 22;
		static constexpr float characterOffset = 4;

		// FrontendHost (see frontend.h)
		void notePlaying(int instNdx, int bar, int noteNdx, bool playing) override
		{
			workspace.storeActiveEditorElement(instNdx, bar, noteNdx, playing);
			if (playing) record("note inst " + std::to_string(instNdx) + " bar " + std::to_string(bar) + " note " + std::to_string(noteNdx));
		}
		void restPlaying(int instNdx, int bar, int noteNdx) override { workspace.storeActiveEditorRest(instNdx, bar, noteNdx); }
		void nothingPlaying(int instNdx) override { workspace.clearActiveEditorElement(instNdx); }
		void setLineMessage(int pane, int line, int errorCode, const std::string& text, const std::string& boundFunction) override
		{
			workspace.setLineMessage(pane, line, errorCode, text, boundFunction);
		}
		void clearLineMessage(int pane, int line, const std::string& boundFunction) override { workspace.clearLineMessage(pane, line, boundFunction); }
		void showMouseCursor(bool) override {}
		float frameRate() override { return targetFrameRate; }
		void setFrameRate(int framesPerSecond) override { targetFrameRate = (float)framesPerSecond; }
		void setPaneLanguage(int pane, int language) override { workspace.setPaneLanguage(pane, language); }
		bool openHelp(int pane, const std::string& filePath, int newPane) override { return workspace.openHelp(pane, filePath, newPane); }
		void setEditorOption(int pane, const std::string& option, const std::string& value) override { workspace.setEditorOption(pane, option, value); }
		void setPaneActive(int pane, bool active) override { workspace.setPaneActive(pane, active); }
		void sendKeysTo(int pane, const std::string& host, int targetPane) override { workspace.sendKeysTo(pane, host, targetPane); }
		void sendLinesTo(int pane, const std::string& host, int targetPane) override { workspace.sendLinesTo(pane, host, targetPane); }
		int tabSize(int pane) override { return workspace.tabSize(pane); }
		void connectLineToBar(int pane, int line, int instNdx, int bar) override { workspace.connectLineToBar(pane, line, instNdx, bar); }
		// the score isn't drawn, but whether its area is shown beside the panes (with the score, the piano roll, the oscilloscope
		// or the sequencer), which takes half of the window, and whether that is the score, are kept as in the app (see ofApp::setShowScore())
		// as the panes are laid out by them, and the sequencer asks whether the score is shown
		std::string scoreViewCommand(const std::vector<std::string>& methodAndArgs) override
		{
			bool shown = scoreArea;
			std::string method = (methodAndArgs.empty() ? std::string() : methodAndArgs[0]);
			std::string what = (methodAndArgs.size() > 1 ? methodAndArgs[1] : std::string());
			// the bar count, the tempo, and the beat are shown with the score, and don't show or hide it
			bool partOfScore = (what == "barcount" || what == "tempo" || what == "beat");
			if (method == "show" && !partOfScore) {
				bool view = (what.empty() || what == "score" || what == "pianoroll" || what == "scope" || what == "seq" || what == "sequencer");
				if (view) {
					scoreArea = true;
					showNotes = (what.empty() || what == "score");
				}
			}
			else if (method == "hide" && !partOfScore) {
				scoreArea = false;
				showNotes = false;
			}
			// the panes get the half of the window the score doesn't take, or all of it again, as in the app
			if (scoreArea != shown) workspace.layout();
			return "";
		}
		bool scoreArea = false;
		void setScoreNotes(int) override {}
		void setNotePositions(int) override {}
		void setNotePositions(int, int) override {}
		void calculateStaffPositions(int, bool) override {}
		void scoreMustUpdate() override {}
		void instrumentCreated(int) override {}
		void instrumentRemoved(int instNdx) override { workspace.instrumentRemoved(instNdx); }
		void instrumentsCleared() override { workspace.instrumentsCleared(); }
		void layoutInstrumentBar(int, int) override {}
		std::pair<int, int> mousePosition() override { return {0, 0}; }
		void setPaneFromOsc(int, const std::string&) override {}
		void storeEditorNoteSpans(int lineNum, int instNdx, int barNdx, int numNotes) override
		{
			workspace.storeEditorNoteSpans(lineNum, instNdx, barNdx, numNotes);
		}
		bool scoreShown() override { return showNotes; }
		void sequencerStarted(float, uint64_t) override { record("start"); }
		void beat(uint64_t, float, uint64_t) override { record("beat"); }
		void barStarted(uint64_t) override { record("bar"); }
		void loopChanged() override { record("loop changed"); }
		void setPythonTyping(bool) override {}

		// EditorHost (see editor_host.h)
		Session& editorSession() override { return session; }
		Color editorForeground() override { return Colors::white; }
		Color editorBackground() override { return Colors::black; }
		float editorBrightness() override { return 1; }
		int scoreBesidePanesWidth() override { return (scoreArea ? windowW / 2 : -1); }
		void parseStrings(int index, int numLines) override { workspace.parseStrings(index, numLines); }
		std::vector<EditorNoteSpan> getActiveEditorNoteSpans(int pane) override { return workspace.getActiveEditorNoteSpans(pane); }
		void alignAfterEdit(int pane, bool space) override { workspace.alignAfterEdit(pane, space); }
		void alignAfterPaste(int pane) override { workspace.alignAfterPaste(pane); }
		bool startsMelodicLine(const std::string& word) override { return workspace.startsMelodicLine(word); }
		std::vector<std::string> getCommandCompletions() override { return workspace.getCommandCompletions(); }
		std::vector<std::string> getMethodCompletions(const std::string& name) override { return workspace.getMethodCompletions(name); }
		std::vector<std::string> getArgumentCompletions() override { return workspace.getArgumentCompletions(); }
		std::vector<std::string> pythonNames(const std::string& dotted) override { return workspace.pythonNames(dotted); }
		std::vector<std::string> pythonSignature(const std::string& dotted) override { return workspace.pythonSignature(dotted); }
		void typeFromPython(const std::string&) override {}
		void startTypingFromPython(int) override {}
		int pythonTypingPane() override { return -1; }
		void keyPressed(int key) override { workspace.keyPressed(key); }
		void keyReleased(int key) override { workspace.keyReleased(key); }
		void keyPressedOsc(int key, int pane) override { workspace.keyPressedInPane(key, pane); }
		void keyReleasedOsc(int key, int pane) override { workspace.keyReleasedInPane(key, pane); }
		std::string clipboard() override { return clipboardText; }
		void setClipboard(const std::string& text) override { clipboardText = text; }
		std::string chooseFileToSave(const std::string&, const std::string&) override { return ""; }
		std::string chooseFileToOpen(const std::string&) override { return ""; }
		int windowWidth() override { return windowW; }
		int windowHeight() override { return windowH; }

		// WorkspaceHost (see workspace.h)
		Metrics workspaceMetrics() override { return {(float)windowW, (float)windowH, cellW, cellH}; }
		// the score is beside the panes, on the right, while it is shown; it is never moved, which the app does with Ctrl+P
		// (so the test files that show the score and type keys are compared only up to the editor's sections, see tests/compare.cmake)
		ScorePlacement scorePlacement() override { return {scoreArea, 0, false}; }
		void swapScorePosition(int) override {}
		void revealActivePane() override {}
		void applyFont(bool, bool layout) override
		{
			for (auto& editor : workspace.editors) editor.second.setFontSize(fontSize, cellW, cellH, characterOffset);
			if (layout) workspace.layout();
		}
		void quitNow() override { quit = true; }
		bool quit = false;
		void changeFontSize(int) override {}
		void changeScoreSize(int) override {}
		void changeBrightness(float) override {}
		void toggleFullscreen() override {}
		void hideScore() override
		{
			bool shown = scoreArea;
			scoreArea = showNotes = false;
			if (shown) workspace.layout();
		}
		void belowAreaMoved(float) override {}
		void belowAreaReset() override {}
		Color belowAreaBackground() override { return Colors::black; }

		// AudioCallback (see audio.h): the audio of --play, as the app plays it: pyo's output, if pyo runs, and the click track
		void onAudioIn(const float* in, int, int) override
		{
#ifdef USEPYO
			if (session.pyoSet) session.sharedData.pyo.fillin(const_cast<float*>(in));
#endif
		}
		void onAudioOut(float* out, int numFrames, int numChannels, int sampleRate) override
		{
			bool pyoProcessed = false;
#ifdef USEPYO
			if (session.pyoSet) {
				session.sharedData.pyo.process(out);
				pyoProcessed = true;
			}
#endif
			if (!pyoProcessed) std::fill(out, out + (size_t)numFrames * numChannels, 0.0f);
			session.sharedData.clickTrack.process(out, numFrames, numChannels, sampleRate);
		}
};

int usage()
{
	std::cerr << "usage: livelily_headless --test file.lyv [--out dump.txt] [--data directory]\n       livelily_headless --play seconds file.lyv [--data directory]" << std::endl;
	return 2;
}

}

int main(int argc, char* argv[])
{
	std::string testFile, outFile, dataDirectory;
	double playSeconds = 0;
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg == "--test" && i + 1 < argc) testFile = argv[++i];
		else if (arg == "--out" && i + 1 < argc) outFile = argv[++i];
		else if (arg == "--data" && i + 1 < argc) dataDirectory = argv[++i];
		else if (arg == "--play" && i + 2 < argc) { playSeconds = std::atof(argv[++i]); testFile = argv[++i]; }
		else return usage();
	}
	if (testFile.empty()) return usage();
	testFile = std::filesystem::absolute(testFile).string();
	if (outFile.empty()) outFile = testFile + ".dump";

	PlatformScope platform;
	Session session;
	HeadlessFrontend frontend(session);
	session.frontend = &frontend;
	session.audioCallback = &frontend;
	Editor::host = &frontend;
	// relative paths (e.g. of a SoundFont) start from the user's folder of LiveLily, unless --data gives another one, and \help looks
	// in the search paths, which in test mode are only the resources of LiveLily, as in the app (see paths.h)
	if (dataDirectory.empty()) dataDirectory = Paths::userFolder().string();
	session.dataDirectory = (std::filesystem::path(dataDirectory) / "").string();
	std::filesystem::path resourcesPath = Paths::resources(Paths::executable(argv[0]));
	session.searchPaths = (playSeconds > 0 ? Paths::searchPaths(resourcesPath) : std::vector<std::filesystem::path>{resourcesPath});
	// a fixed seed, so that \rand gives the same values every time, as in the test mode of the app
	srand(1);
	// the made-up MIDI ports of test mode, as in the app (see test_midi.cpp), and an audio stream that never plays
	// and the real ones to play with --play
	if (playSeconds <= 0) {
		session.midi = makeTestMidiOutput();
		session.audioStream = makeTestAudioStream();
	}
	else platform.findDevices();
	session.setup(Colors::white);
	// one pane, as the app starts with
	frontend.workspace.setupFirstPane();
	frontend.applyFont(true, true);

	if (playSeconds > 0) {
		Editor& editor = frontend.workspace.editors[session.whichPane];
		if (!editor.loadFile(testFile)) { std::cerr << "could not load " << testFile << std::endl; return 1; }
		if (!editor.allLines.empty()) {
			for (const BracketChunk& chunk : editor.findBracketChunks(editor.allLines.begin()->first, editor.allLines.rbegin()->first)) {
				frontend.workspace.parseStrings(chunk.top, chunk.bottom - chunk.top + 1);
			}
		}
		// the messages of the lines (e.g. of \listmidiports, or errors), to the error stream, so that the output has only the playback
		for (const auto& line : editor.allLines) {
			if (!line.second.tracebackStr.empty()) std::cerr << "line " << (line.first + 1) << ": " << line.second.tracebackStr << std::endl;
		}
		if (session.soundStreamSet) std::cerr << "audio: " << session.audioStream->currentDevice() << std::endl;
		if (!session.startSequencer) { std::cerr << "the file doesn't start the sequencer" << std::endl; return 1; }
		frontend.recording = true;
		uint64_t start = Clock::millis();
		frontend.record("play");
		session.sequencer.start();
		std::this_thread::sleep_for(std::chrono::duration<double>(playSeconds));
		session.sequencer.stopNow();
		session.sequencer.waitForThread();
		// the audio thread calls the session, so it stops before the session goes, as in ofApp::exit()
		if (session.soundStreamSet) session.audioStream->close();
		frontend.recording = false;
		for (const std::string& event : frontend.events) {
			size_t space = event.find(' ');
			std::cout << (std::stoull(event.substr(0, space)) - start) << event.substr(space) << "\n";
		}
		return 0;
	}

	std::ofstream out(outFile);
	if (!out.is_open()) {
		std::cerr << "livelily headless: could not write to " << outFile << std::endl;
		return 1;
	}
	// the file runs as in the app's test mode, whose frames, between the keys it types, fill the grids of the panes
	std::vector<KeyEvent> keys;
	frontend.workspace.fillGrids();
	if (!TestRun::runFile(frontend.workspace, session, testFile, out, keys)) return 0;
	frontend.workspace.fillGrids();
	for (const KeyEvent& event : keys) {
		if (event.press) frontend.workspace.keyPressed(event.key);
		else frontend.workspace.keyReleased(event.key);
		frontend.workspace.fillGrids();
		if (frontend.quit) break;
	}
	frontend.workspace.fillGrids();
	TestRun::writeState(frontend.workspace, session, out, frontend.targetFrameRate);
	TestRun::writeScreen(frontend.workspace, out);
	return 0;
}
