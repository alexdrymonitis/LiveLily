// test mode, for checking that changes to the parser don't change what LiveLily code does
// livelily --test file.lyv [--out dump.txt] (see main.cpp) runs the file with a hidden window, chunk by chunk
// as if every definition and every line were executed in turn in the editor
// and writes the errors, warnings, and notes of the lines, and the parsed state, to a text file
// if the file starts the sequencer (e.g. with \score.play), the sequencer plays a dry run at the end, on a simulated clock
// and what it would have sent (MIDI, OSC, pyo code, clicks, bound functions) is written with its time (see Sequencer::dryRun())
// a few frames later, when the window has been drawn, the screen is added: the grids of the panes and of the area below them (see cellgrid.h)
// the dumps of the example files are compared before and after a change to the parser (see tests/snapshots.sh)

#include "ofApp.h"
#include "session_dump.h"
#include <fstream>
#include <sstream>
#include <iomanip>


//--------------------------------------------------------------
void ofApp::runTest()
{
	std::ofstream out(testOut);
	if (!out.is_open()) {
		std::cerr << "livelily test: could not write to " << testOut << std::endl;
		return;
	}
	Editor& editor = editors[session.whichPane];
	if (!editor.loadFile(testFile)) {
		out << "could not load " << testFile << "\n";
		return;
	}
	out << "=== lines\n";
	if (!editor.allLines.empty()) {
		std::vector<BracketChunk> chunks = editor.findBracketChunks(editor.allLines.begin()->first, editor.allLines.rbegin()->first);
		for (const BracketChunk& chunk : chunks) {
			parseStrings(chunk.top, chunk.bottom - chunk.top + 1);
			// the errors, warnings, and notes (or outputs) of the lines of the chunk, as the editor shows them (see Editor::setTraceback())
			for (int lineNdx = chunk.top; lineNdx <= chunk.bottom; lineNdx++) {
				auto it = editor.allLines.find(lineNdx);
				if (it == editor.allLines.end() || it->second.tracebackStr.empty()) continue;
				writeLineMessage(out, lineNdx, it->second.str, it->second.tracebackColor, it->second.tracebackStr);
			}
		}
	}
	// playback: what the sequencer sends, if the file started it, from the state at the end of the file
	if (session.startSequencer) {
		session.startSequencer = false;
		std::vector<std::string> playback = session.sequencer.dryRun();
		out << "=== playback\n";
		for (const std::string& line : playback) out << line << "\n";
	}
	dumpState(out);
}

//--------------------------------------------------------------
void ofApp::dumpState(std::ostream& out)
{
	dumpSession(out, session, ofGetTargetFrameRate(), editors[session.whichPane].getSessionActivity());
	// the tokens of the editor every note highlights, by instrument and bar (see storeEditorNoteSpans()), e.g. c'4 or \1+c'4 for a copied bar
	out << "=== editor highlights\n";
	for (const auto& bar : editorNoteSpans) {
		out << "inst " << bar.first.first << " bar " << bar.first.second << ":";
		for (const auto& note : bar.second) {
			out << " ";
			for (size_t k = 0; k < note.size(); k++) out << (k > 0 ? "+" : "") << escapedForDump(note[k].text) << "@" << note[k].start;
		}
		out << "\n";
	}
	// every highlighted token must be in the editor where the note highlights it, also after its line changed (e.g. by \editor.alignment)
	std::map<int, std::string> linesById;
	for (auto& line : editors[session.whichPane].allLines) linesById[editors[session.whichPane].getLineId(line.first)] = line.second.str;
	for (const auto& bar : editorNoteSpans) {
		for (size_t n = 0; n < bar.second.size(); n++) {
			for (const EditorNoteSpan& span : bar.second[n]) {
				if (span.pane != session.whichPane) continue;
				auto line = linesById.find(span.lineId);
				std::string shown = (line == linesById.end() || span.start < 0 || span.start >= (int)line->second.size() ? std::string("(no line)") : line->second.substr(span.start, span.length));
				if (shown != span.text) {
					out << "highlight mismatch: inst " << bar.first.first << " bar " << bar.first.second << " note " << n + 1 << ": "
						<< escapedForDump(span.text) << "@" << span.start << ", the editor has " << escapedForDump(shown) << "\n";
				}
			}
		}
	}
	// the text of the editor at the end, which commands like \editor.alignment change
	out << "=== editor\n";
	for (const auto& line : editors[session.whichPane].allLines) out << line.second.str << "\n";
}

//--------------------------------------------------------------
void ofApp::appendTestScreen()
{
	// what the editor shows, which the snapshots compare too: every pane (its text, line numbers, and status line) and the area below the panes
	std::ofstream out(testOut, std::ios::app);
	if (!out.is_open()) return;
	out << "=== screen\n";
	for (auto& editor : editors) {
		std::string pane = "pane " + std::to_string(editor.first + 1);
		dumpGrid(out, pane + " text", editor.second.getTextGrid());
		dumpGrid(out, pane + " line numbers", editor.second.getGutterGrid());
		dumpGrid(out, pane + " status line", editor.second.getStatusGrid());
	}
	dumpGrid(out, "below the panes", belowGrid);
}
