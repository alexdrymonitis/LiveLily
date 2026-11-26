// the headless runner of LiveLily: LiveLily (App, see app.h) with a window that paints nothing, and no openFrameworks
// livelily_headless --test file.lyv [--out dump.txt] [--data directory]
//     runs the file in a pane, chunk by chunk, and types the keys of its "% test keys:" lines, as the test mode of the app does
//     (see src/test_run.h), and writes the same dump; the window and the cells of the font are as large as in the app's test mode
//     so the panes have the same numbers of lines and characters, and the keys move the same way, and the views are laid out as in the app
//     but nothing is painted, and the size of the cells doesn't change with the size of the font (Ctrl+= and Ctrl+-)
// livelily_headless --play seconds file.lyv [--data directory]
//     runs the file, and if it starts the sequencer, plays it for real (on its thread) for some seconds, with the MIDI ports
//     and the audio devices of the system (with JUCE, see platform/), and writes the beats, bars, and notes the front end is told about,
//     with their times, to check the thread and its timing

#include "app.h"
#include "app_window.h"
#include "canvas.h"
#include "test_run.h"
#include "clock.h"
#include "paths.h"
#include "platform.h"
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

// a window that paints nothing, as large as the app's in test mode (1350 by 900 pixels, and DroidSansMono at 18 points)
class HeadlessWindow : public AppWindow
{
	public:
		static constexpr int width = 1350;
		static constexpr int height = 900;
		FontMetrics loadEditorFont(int) override { return {11, 22, 4}; }
		void paintGrids(const std::vector<GridPaint>&, float) override {}
		void paintGrid(const CellGrid&, const GridGeometry&, float) override {}
		std::string clipboard() override { return clipboardText; }
		void setClipboard(const std::string& text) override { clipboardText = text; }
		std::string chooseFileToSave(const std::string&, const std::string&) override { return ""; }
		std::string chooseFileToOpen(const std::string&) override { return ""; }
		void showMouseCursor(bool) override {}
		std::pair<int, int> mousePosition() override { return {0, 0}; }
		float frameRate() override { return targetFrameRate; }
		void setFrameRate(int framesPerSecond) override { targetFrameRate = (float)framesPerSecond; }
		void toggleFullscreen() override {}
		void quit() override { quitting = true; }
		bool quitting = false;
	private:
		std::string clipboardText;
		float targetFrameRate = 60;
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
	bool testMode = (playSeconds <= 0);

	// JUCE's message manager, for the MIDI ports of --play, made before LiveLily and gone after it
	PlatformScope platform;
	// the views are laid out, but not drawn
	NullCanvas canvas;
	Canvas::current = &canvas;
	HeadlessWindow window;
	App app(window);
	Session& session = app.session;
	// an audio stream that never plays in test mode (the app's test mode, which has a window, plays sound)
	// and the MIDI ports of the system to play with --play (test mode has made-up ones, see App::setup())
	if (testMode) session.audioStream = makeTestAudioStream();
	else platform.findDevices();
	std::filesystem::path resourcesPath = Paths::resources(Paths::executable(argv[0]));
	app.setup(HeadlessWindow::width, HeadlessWindow::height, testMode, resourcesPath.string());
	// relative paths (e.g. of a SoundFont) start from the user's folder of LiveLily, unless --data gives another one
	if (dataDirectory.empty()) dataDirectory = Paths::userFolder().string();
	session.dataDirectory = (std::filesystem::path(dataDirectory) / "").string();
	// a fixed seed, so that \rand gives the same values every time, as in the test mode of the app, also with --play
	srand(1);

	if (!testMode) {
		Editor& editor = app.workspace.editors[session.whichPane];
		if (!editor.loadFile(testFile)) { std::cerr << "could not load " << testFile << std::endl; return 1; }
		if (!editor.allLines.empty()) {
			for (const BracketChunk& chunk : editor.findBracketChunks(editor.allLines.begin()->first, editor.allLines.rbegin()->first)) {
				app.workspace.parseStrings(chunk.top, chunk.bottom - chunk.top + 1);
			}
		}
		// the messages of the lines (e.g. of \listmidiports, or errors), to the error stream, so that the output has only the playback
		for (const auto& line : editor.allLines) {
			if (!line.second.tracebackStr.empty()) std::cerr << "line " << (line.first + 1) << ": " << line.second.tracebackStr << std::endl;
		}
		if (session.soundStreamSet) std::cerr << "audio: " << session.audioStream->currentDevice() << std::endl;
		if (!session.startSequencer) { std::cerr << "the file doesn't start the sequencer" << std::endl; return 1; }
		// the beats, bars, and notes, with their times, which the sequencer's thread tells
		std::mutex eventsMutex;
		std::vector<std::pair<uint64_t, std::string>> events;
		app.onPlayback = [&](const std::string& event) {
			std::lock_guard<std::mutex> lock(eventsMutex);
			events.push_back({Clock::millis(), event});
		};
		uint64_t start = Clock::millis();
		app.onPlayback("play");
		session.sequencer.start();
		std::this_thread::sleep_for(std::chrono::duration<double>(playSeconds));
		session.sequencer.stopNow();
		session.sequencer.waitForThread();
		// the audio thread calls LiveLily, so it stops before LiveLily goes (see App::exit())
		if (session.soundStreamSet) session.audioStream->close();
		app.onPlayback = nullptr;
		for (const auto& event : events) std::cout << (event.first - start) << " " << event.second << "\n";
		return 0;
	}

	std::ofstream out(outFile);
	if (!out.is_open()) {
		std::cerr << "livelily headless: could not write to " << outFile << std::endl;
		return 1;
	}
	// the file runs as in the app's test mode, which draws a frame between the keys it types
	std::vector<KeyEvent> keys;
	app.draw();
	if (!TestRun::runFile(app.workspace, session, testFile, out, keys)) return 0;
	app.draw();
	for (const KeyEvent& event : keys) {
		if (event.press) app.keyPressed(event.key);
		else app.keyReleased(event.key);
		app.draw();
		if (window.quitting) break;
	}
	app.draw();
	TestRun::writeState(app.workspace, session, out, window.frameRate());
	TestRun::writeScreen(app.workspace, out);
	return 0;
}
