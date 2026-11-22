// test mode, for checking that changes to the parser don't change what LiveLily code does
// livelily --test file.lyv [--out dump.txt] (see main.cpp) runs the file with a hidden window, chunk by chunk
// as if every definition and every line were executed in turn in the editor
// and writes the errors, warnings, and notes of the lines, and the parsed state, to a text file
// if the file starts the sequencer (e.g. with \score.play), the sequencer plays a dry run at the end, on a simulated clock
// and what it would have sent (MIDI, OSC, pyo code, clicks, bound functions) is written with its time (see Sequencer::dryRun())
// the dumps of the example files are compared before and after a change to the parser (see tests/snapshots.sh)

#include "ofApp.h"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace {

std::string escaped(const std::string& str)
{
	std::string result;
	for (char c : str) {
		if (c == '\n') result += "\\n";
		else if (c == '\t') result += "\\t";
		else result += c;
	}
	return result;
}

template <typename T>
std::string join(const std::vector<T>& vec)
{
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(4);
	for (size_t i = 0; i < vec.size(); i++) oss << (i > 0 ? " " : "") << vec[i];
	return oss.str();
}

std::string join(const std::vector<bool>& vec)
{
	std::string result;
	for (size_t i = 0; i < vec.size(); i++) result += (i > 0 ? " " : "") + std::string(vec[i] ? "1" : "0");
	return result;
}

template <typename T>
std::string join(const std::vector<std::vector<T>>& vec)
{
	std::string result;
	for (size_t i = 0; i < vec.size(); i++) result += (i > 0 ? " | " : "") + join(vec[i]);
	return result;
}

// one line per bar of a map of bar data, e.g. "    durs 1: 64 64 128"
template <typename M>
void dumpBarMap(std::ostream& out, const std::string& label, const M& barMap)
{
	for (const auto& bar : barMap) out << "    " << label << " " << bar.first << ": " << join(bar.second) << "\n";
}

}

//--------------------------------------------------------------
void ofApp::runTest()
{
	std::ofstream out(testOut);
	if (!out.is_open()) {
		std::cerr << "livelily test: could not write to " << testOut << std::endl;
		return;
	}
	Editor& editor = editors[whichPane];
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
			static const char* kinds[] = {"note", "warning", "error"};
			for (int lineNdx = chunk.top; lineNdx <= chunk.bottom; lineNdx++) {
				auto it = editor.allLines.find(lineNdx);
				if (it == editor.allLines.end() || it->second.tracebackStr.empty()) continue;
				int color = it->second.tracebackColor;
				// the ports and devices listed depend on the machine (e.g. the process number in the name of a FluidSynth port)
				// so the notes of the commands that list them are not written, only that they listed something
				static const std::vector<std::string> listingCommands = {"\\listmidiports", "\\listserialports", "\\listaudiodev"};
				std::string firstToken = it->second.str.substr(0, it->second.str.find(' '));
				size_t firstChar = firstToken.find_first_not_of(" \t");
				if (firstChar != std::string::npos) firstToken = firstToken.substr(firstChar);
				bool listing = std::find(listingCommands.begin(), listingCommands.end(), firstToken) != listingCommands.end();
				std::string message = (listing && color == 0 ? std::string("(the list of this machine)") : escaped(it->second.tracebackStr));
				out << "line " << lineNdx + 1 << " " << (color >= 0 && color <= 2 ? kinds[color] : "?") << ": " << message << "\n";
			}
		}
	}
	// playback: what the sequencer sends, if the file started it, from the state at the end of the file
	if (startSequencer) {
		startSequencer = false;
		std::vector<std::string> playback = sequencer.dryRun();
		out << "=== playback\n";
		for (const std::string& line : playback) out << line << "\n";
	}
	dumpState(out);
}

//--------------------------------------------------------------
void ofApp::dumpState(std::ostream& out)
{
	out << std::fixed << std::setprecision(4);
	out << "=== bars\n";
	for (const auto& bar : sharedData.barsOrdered) {
		int ndx = bar.first;
		out << "bar " << ndx << " " << bar.second;
		if (sharedData.numerator.count(ndx)) out << " time " << sharedData.numerator[ndx] << "/" << sharedData.denominator[ndx];
		if (sharedData.tempoMs.count(ndx)) out << " tempoMs " << sharedData.tempoMs[ndx];
		if (sharedData.tempo.count(ndx)) out << " tempo " << sharedData.tempo[ndx];
		if (sharedData.beatAtValues.count(ndx)) out << " beatat " << join(sharedData.beatAtValues[ndx]);
		out << "\n";
		auto lines = sharedData.barLines.find(bar.second);
		if (lines != sharedData.barLines.end()) out << "  text " << escaped(lines->second) << "\n";
	}
	out << "=== loops\n";
	for (const auto& loop : sharedData.loopsOrdered) {
		out << "loop " << loop.first << " " << loop.second << ":";
		auto data = sharedData.loopData.find(loop.first);
		if (data != sharedData.loopData.end()) {
			for (int bar : data->second) {
				auto name = sharedData.loopsOrdered.find(bar);
				out << " " << (name != sharedData.loopsOrdered.end() ? name->second : std::to_string(bar));
			}
		}
		out << "\n";
	}
	out << "=== instruments\n";
	for (auto& instIt : sharedData.instruments) {
		Instrument& inst = instIt.second;
		out << "inst " << instIt.first << " " << inst.getName() << (inst.isRhythm() ? " rhythm" : "") << (inst.isMuted() ? " muted" : "")
			<< (inst.isToBeMuted() ? " tomute" : "") << (inst.isToBeUnmuted() ? " tounmute" : "");
		if (inst.getDelayTime() > 0) out << " delay " << inst.getDelayTime();
		out << "\n";
		dumpBarMap(out, "notes", inst.notes);
		dumpBarMap(out, "durs", inst.durs);
		dumpBarMap(out, "dynamics", inst.dynamics);
		dumpBarMap(out, "articulations", inst.articulations);
		dumpBarMap(out, "ties", inst.tieIndexes);
		dumpBarMap(out, "slurred", inst.isSlurred);
		// the start and stop notes of every slur of a bar (-1 for a slur that comes from the previous bar or goes on to the next one)
		for (const auto& bar : inst.slurIndexes) {
			out << "    slurs " << bar.first << ":";
			for (const auto& slur : bar.second) out << " (" << slur.first << "," << slur.second << ")";
			out << "\n";
		}
		dumpBarMap(out, "glissandi", inst.glissandi);
		dumpBarMap(out, "scoreNotes", inst.scoreNotes);
		dumpBarMap(out, "scoreOctaves", inst.scoreOctaves);
		dumpBarMap(out, "scoreAccidentals", inst.scoreAccidentals);
		dumpBarMap(out, "scoreDurs", inst.scoreDurs);
		dumpBarMap(out, "scoreDynamics", inst.scoreDynamics);
	}
	out << "=== settings\n";
	out << "ppqn " << sharedData.PPQN << "\n";
	out << "framerate " << ofGetTargetFrameRate() << "\n";
	out << "audio samplerate " << sampleRate << " buffersize " << bufferSize << " inchannels " << inChannels << " outchannels " << outChannels << "\n";
	out << "clicktrack " << (sharedData.clickTrack.isOn() ? "on" : "off") << " tune " << sharedData.clickTrack.getTune() << " volume " << sharedData.clickTrack.getVolume() << "\n";
	out << "accoffset " << (accOffsetSet ? std::to_string(accOffsetCoef) : std::string("default")) << "\n";
	out << "session " << (editors[whichPane].getSessionActivity() ? "active" : "inactive") << "\n";
	out << "instorder";
	for (const auto& ordered : sharedData.instrumentIndexesOrdered) out << " " << ordered.second;
	out << "\n";
	out << "notedur";
	for (const auto& noteDur : noteDurSettings) out << " " << noteDur.first << " " << noteDur.second;
	out << "\n";
	out << "=== lists\n";
	for (const auto& list : listIndexes) {
		out << "list " << list.first << ":";
		auto items = listMap.find(list.second);
		if (items != listMap.end()) {
			for (const std::string& item : items->second) out << " " << item;
		}
		out << "\n";
	}
	out << "=== functions\n";
	for (const auto& func : functionIndexes) out << "function " << func.first << "\n";
	// the tokens of the editor every note highlights, by instrument and bar (see storeEditorNoteSpans()), e.g. c'4 or \1+c'4 for a copied bar
	out << "=== editor highlights\n";
	for (const auto& bar : editorNoteSpans) {
		out << "inst " << bar.first.first << " bar " << bar.first.second << ":";
		for (const auto& note : bar.second) {
			out << " ";
			for (size_t k = 0; k < note.size(); k++) out << (k > 0 ? "+" : "") << escaped(note[k].text) << "@" << note[k].start;
		}
		out << "\n";
	}
	// every highlighted token must be in the editor where the note highlights it, also after its line changed (e.g. by \editor.alignment)
	std::map<int, std::string> linesById;
	for (auto& line : editors[whichPane].allLines) linesById[editors[whichPane].getLineId(line.first)] = line.second.str;
	for (const auto& bar : editorNoteSpans) {
		for (size_t n = 0; n < bar.second.size(); n++) {
			for (const EditorNoteSpan& span : bar.second[n]) {
				if (span.pane != whichPane) continue;
				auto line = linesById.find(span.lineId);
				std::string shown = (line == linesById.end() || span.start < 0 || span.start >= (int)line->second.size() ? std::string("(no line)") : line->second.substr(span.start, span.length));
				if (shown != span.text) {
					out << "highlight mismatch: inst " << bar.first.first << " bar " << bar.first.second << " note " << n + 1 << ": "
						<< escaped(span.text) << "@" << span.start << ", the editor has " << escaped(shown) << "\n";
				}
			}
		}
	}
	// the text of the editor at the end, which commands like \editor.alignment change
	out << "=== editor\n";
	for (const auto& line : editors[whichPane].allLines) out << line.second.str << "\n";
}
