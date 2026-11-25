// the headless test runner of LiveLily: the engine (Session) with no window and no openFrameworks
// livelily_headless --test file.lyv [--out dump.txt] [--data directory]
//     runs the file chunk by chunk, as the test mode of the app does (see src/testmode.cpp), and writes the same dump
//     except the sections of the editor (its highlights and its text), which need the editor of the app
// livelily_headless --play seconds file.lyv
//     runs the file, and if it starts the sequencer, plays it for real (on its thread) for some seconds, with the MIDI ports
//     and the audio devices of the system (with JUCE, see platform/), and writes the beats, bars, and notes the front end is told about,
//     with their times, to check the thread and its timing

#include "session.h"
#include "session_dump.h"
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

// what the engine asks of the user interface, kept so that it can be written to the dump
class HeadlessFrontend : public FrontendHost, public AudioCallback
{
	public:
		// the messages of the lines, as the editor of the app keeps them (color 0 note, 1 warning, 2 error, see Editor::setTraceback())
		struct Message { int color; std::string text; };
		std::map<int, Message> messages;
		bool sessionActive = true;
		float targetFrameRate = 60;
		bool showNotes = false;
		// the playback, for --play
		bool recording = false;
		std::mutex eventsMutex;
		std::vector<std::string> events;

		void record(const std::string& event)
		{
			if (!recording) return;
			std::lock_guard<std::mutex> lock(eventsMutex);
			events.push_back(std::to_string(Clock::millis()) + " " + event);
		}

		void notePlaying(int instNdx, int bar, int noteNdx, bool playing) override
		{
			if (playing) record("note inst " + std::to_string(instNdx) + " bar " + std::to_string(bar) + " note " + std::to_string(noteNdx));
		}
		void restPlaying(int, int, int) override {}
		void nothingPlaying(int) override {}
		void setLineMessage(int, int line, int errorCode, const std::string& text, const std::string& boundFunction) override
		{
			// the app shows the messages of functions the sequencer calls in its update(), which the test mode doesn't run
			if (!boundFunction.empty()) return;
			int color = (errorCode == 2 ? 1 : (errorCode == 3 ? 2 : 0));
			messages[line] = {color, text};
		}
		void clearLineMessage(int, int line, const std::string& boundFunction) override
		{
			if (boundFunction.empty()) messages.erase(line);
		}
		void showMouseCursor(bool) override {}
		float frameRate() override { return targetFrameRate; }
		void setFrameRate(int framesPerSecond) override { targetFrameRate = (float)framesPerSecond; }
		void setPaneLanguage(int, int) override {}
		bool openHelp(int, const std::string&, int) override { return true; }
		void setEditorOption(int, const std::string&, const std::string&) override {}
		void setPaneActive(int, bool active) override { sessionActive = active; }
		void sendKeysTo(int, const std::string&, int) override {}
		void sendLinesTo(int, const std::string&, int) override {}
		int tabSize(int) override { return 4; }
		void connectLineToBar(int, int, int, int) override {}
		std::string scoreViewCommand(const std::vector<std::string>& methodAndArgs) override
		{
			if (!methodAndArgs.empty() && methodAndArgs[0] == "show") showNotes = (methodAndArgs.size() < 2 || methodAndArgs[1] == "score");
			else if (!methodAndArgs.empty() && methodAndArgs[0] == "hide") showNotes = false;
			return "";
		}
		void setScoreNotes(int) override {}
		void setNotePositions(int) override {}
		void setNotePositions(int, int) override {}
		void calculateStaffPositions(int, bool) override {}
		void scoreMustUpdate() override {}
		void instrumentCreated(int) override {}
		void instrumentRemoved(int) override {}
		void instrumentsCleared() override {}
		void layoutInstrumentBar(int, int) override {}
		std::pair<int, int> mousePosition() override { return {0, 0}; }
		void setPaneFromOsc(int, const std::string&) override {}
		void storeEditorNoteSpans(int, int, int, int) override {}
		bool scoreShown() override { return showNotes; }
		void sequencerStarted(float, uint64_t) override { record("start"); }
		void beat(uint64_t, float, uint64_t) override { record("beat"); }
		void barStarted(uint64_t) override { record("bar"); }
		void loopChanged() override { record("loop changed"); }
		void setPythonTyping(bool) override {}
		// the audio of --play, as the app plays it: pyo's output, if pyo runs, and the click track (see ofApp::onAudioOut())
		Session* session = nullptr;
		void onAudioIn(const float* in, int, int) override
		{
#ifdef USEPYO
			if (session->pyoSet) session->sharedData.pyo.fillin(const_cast<float*>(in));
#endif
		}
		void onAudioOut(float* out, int numFrames, int numChannels, int sampleRate) override
		{
			bool pyoProcessed = false;
#ifdef USEPYO
			if (session->pyoSet) {
				session->sharedData.pyo.process(out);
				pyoProcessed = true;
			}
#endif
			if (!pyoProcessed) std::fill(out, out + (size_t)numFrames * numChannels, 0.0f);
			session->sharedData.clickTrack.process(out, numFrames, numChannels, sampleRate);
		}
};

// the lines of a file, with tabs replaced by the spaces of a tab, as the editor loads them (see Editor::loadTextFile())
bool loadLines(const std::string& path, std::vector<std::string>& lines)
{
	std::ifstream file(path);
	if (!file.is_open()) return false;
	std::string line;
	while (std::getline(file, line)) {
		std::string withoutTabs;
		for (char c : line) withoutTabs += (c == '\t' ? std::string("    ") : std::string(1, c));
		lines.push_back(withoutTabs);
	}
	if (lines.empty()) lines.push_back("");
	return true;
}

// runs lines of the file, as ofApp::parseStrings() runs lines of the editor
void parseStrings(Session& session, HeadlessFrontend& frontend, const std::vector<std::string>& lines, int index, int numLines)
{
	session.barError = false;
	// an unmatched curly bracket is an error, and then none of the lines is parsed (see Editor::findUnmatchedBracket())
	std::vector<std::string> chunkLines(lines.begin() + index, lines.begin() + index + numLines);
	for (const BracketChunk& chunk : CodeText::findBracketChunksInLines(chunkLines)) {
		if (chunk.errorLine > -1) {
			frontend.setLineMessage(session.whichPane, index + chunk.errorLine, 3, chunk.errorStr, "");
			return;
		}
	}
	std::vector<std::pair<int, std::string>> toParse;
	for (int i = index; i < index + numLines; i++) {
		if (frontend.sessionActive || lines[i] == "\\active") toParse.push_back(std::make_pair(i, lines[i]));
	}
	session.parseLines(toParse, numLines, false);
}

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
	HeadlessFrontend frontend;
	session.frontend = &frontend;
	session.audioCallback = &frontend;
	frontend.session = &session;
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
	session.setup(Color{255, 255, 255});

	std::vector<std::string> lines;
	bool loaded = loadLines(testFile, lines);

	if (playSeconds > 0) {
		if (!loaded) { std::cerr << "could not load " << testFile << std::endl; return 1; }
		for (const BracketChunk& chunk : CodeText::findBracketChunksInLines(lines)) parseStrings(session, frontend, lines, chunk.top, chunk.bottom - chunk.top + 1);
		// the messages of the lines (e.g. of \listmidiports, or errors), to the error stream, so that the output has only the playback
		for (const auto& message : frontend.messages) {
			if (!message.second.text.empty()) std::cerr << "line " << (message.first + 1) << ": " << message.second.text << std::endl;
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
	if (!loaded) {
		out << "could not load " << testFile << "\n";
		return 0;
	}
	out << "=== lines\n";
	for (const BracketChunk& chunk : CodeText::findBracketChunksInLines(lines)) {
		parseStrings(session, frontend, lines, chunk.top, chunk.bottom - chunk.top + 1);
		for (int lineNdx = chunk.top; lineNdx <= chunk.bottom; lineNdx++) {
			auto it = frontend.messages.find(lineNdx);
			if (it == frontend.messages.end() || it->second.text.empty()) continue;
			writeLineMessage(out, lineNdx, lines[lineNdx], it->second.color, it->second.text);
		}
	}
	if (session.startSequencer) {
		session.startSequencer = false;
		std::vector<std::string> playback = session.sequencer.dryRun();
		out << "=== playback\n";
		for (const std::string& line : playback) out << line << "\n";
	}
	dumpSession(out, session, frontend.targetFrameRate, frontend.sessionActive);
	return 0;
}
