// test mode, in the app and in the headless runner alike (see test_run.h)
#include "test_run.h"
#include "workspace.h"
#include "session.h"
#include "session_dump.h"
#include "codetext.h"
#include <map>

namespace TestRun {

//--------------------------------------------------------------
bool runFile(Workspace& workspace, Session& session, const std::string& file, std::ostream& out, std::vector<KeyEvent>& keys)
{
	Editor& editor = workspace.editors[session.whichPane];
	if (!editor.loadFile(file)) {
		out << "could not load " << file << "\n";
		return false;
	}
	out << "=== lines\n";
	if (!editor.allLines.empty()) {
		std::vector<BracketChunk> chunks = editor.findBracketChunks(editor.allLines.begin()->first, editor.allLines.rbegin()->first);
		for (const BracketChunk& chunk : chunks) {
			workspace.parseStrings(chunk.top, chunk.bottom - chunk.top + 1);
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
	// keys to type after the file has run, from lines like "% test keys: v$d" (see keys.h), which the front end types
	// and the state and the screen are written once they have been typed (see writeState() and writeScreen())
	keys.clear();
	static const std::string keysPrefix = "% test keys: ";
	for (const auto& line : editor.allLines) {
		if (line.second.str.compare(0, keysPrefix.size(), keysPrefix) != 0) continue;
		std::string error;
		std::vector<KeyEvent> events = parseKeys(line.second.str.substr(keysPrefix.size()), error);
		if (!error.empty()) out << "line " << line.first + 1 << " keys error: " << error << "\n";
		keys.insert(keys.end(), events.begin(), events.end());
	}
	return true;
}

//--------------------------------------------------------------
void writeState(Workspace& workspace, Session& session, std::ostream& out, float frameRate)
{
	dumpSession(out, session, frameRate, workspace.editors[session.whichPane].getSessionActivity());
	// the tokens of the editor every note highlights, by instrument and bar (see storeEditorNoteSpans()), e.g. c'4 or \1+c'4 for a copied bar
	out << "=== editor highlights\n";
	for (const auto& bar : workspace.editorNoteSpans) {
		out << "inst " << bar.first.first << " bar " << bar.first.second << ":";
		for (const auto& note : bar.second) {
			out << " ";
			for (size_t k = 0; k < note.size(); k++) out << (k > 0 ? "+" : "") << escapedForDump(note[k].text) << "@" << note[k].start;
		}
		out << "\n";
	}
	// every highlighted token must be in the editor where the note highlights it, also after its line changed (e.g. by \editor.alignment)
	std::map<int, std::string> linesById;
	for (auto& line : workspace.editors[session.whichPane].allLines) linesById[workspace.editors[session.whichPane].getLineId(line.first)] = line.second.str;
	for (const auto& bar : workspace.editorNoteSpans) {
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
	for (const auto& line : workspace.editors[session.whichPane].allLines) out << line.second.str << "\n";
}

//--------------------------------------------------------------
void writeScreen(Workspace& workspace, std::ostream& out)
{
	// what the editor shows, which the snapshots compare too: every pane (its text, line numbers, and status line) and the area below the panes
	out << "=== screen\n";
	for (auto& editor : workspace.editors) {
		std::string pane = "pane " + std::to_string(editor.first + 1);
		dumpGrid(out, pane + " text", editor.second.getTextGrid());
		dumpGrid(out, pane + " line numbers", editor.second.getGutterGrid());
		dumpGrid(out, pane + " status line", editor.second.getStatusGrid());
	}
	dumpGrid(out, "below the panes", workspace.belowGrid);
}

}
