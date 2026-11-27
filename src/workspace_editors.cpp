// what the editors and the engine ask of the workspace (see workspace.h): running the lines of a pane, the messages of lines
// (also from the sequencer's thread), highlighting the notes that play, and the suggestions of autocompletion
// the front end implements EditorHost and FrontendHost, and passes these on to the workspace
#include "workspace.h"
#include "session.h"
#include "osc.h"
#include "tokens.h"
#include "utils.h"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <set>
#include <thread>

namespace {

// the strings of the interpreter have one name per line (see Pyo::names() and Pyo::signature())
[[maybe_unused]] std::vector<std::string> pythonLines(const std::string& str)
{
	std::vector<std::string> lines;
	size_t start = 0;
	while (start < str.size()) {
		size_t end = str.find('\n', start);
		if (end == std::string::npos) end = str.size();
		if (end > start) lines.push_back(str.substr(start, end - start));
		start = end + 1;
	}
	return lines;
}

}

//--------------------------------------------------------------
std::vector<EditorNoteSpan> Workspace::getActiveEditorNoteSpans(int paneNdx)
{
	// the tokens in a pane of the notes that play, for highlighting them
	std::vector<EditorNoteSpan> spans;
	if (!session.sequencer.isThreadRunning()) {
		std::lock_guard<std::mutex> lock(activeEditorNotesMutex);
		activeEditorNotes.clear();
		return spans;
	}
	std::map<int, std::pair<int, int>> active;
	std::set<int> rests;
	{
		std::lock_guard<std::mutex> lock(activeEditorNotesMutex);
		active = activeEditorNotes;
		rests = activeEditorRests;
	}
	// a rest is a token that starts with r (e.g. r4 or r8.), as no note name starts with r, or a - of the x - notation of rhythm staffs
	auto isRestToken = [](const std::string& text) { return !text.empty() && (text[0] == 'r' || text == "-"); };
	for (auto& note : active) {
		auto it = editorNoteSpans.find(std::make_pair(note.first, note.second.first));
		if (it == editorNoteSpans.end() || note.second.second < 0 || note.second.second >= (int)it->second.size()) continue;
		const std::vector<EditorNoteSpan>& noteSpans = it->second[note.second.second];
		// a rest is highlighted only if it is written as a rest, in the line or in a copied bar, also inside \transpose
		// (the tokens of \transpose and of the copied bar are highlighted together with it)
		// and not if a command makes it (e.g. \euclide, whose token is the whole command)
		if (rests.count(note.first)) {
			bool hasRestToken = false;
			bool written = true;
			for (const EditorNoteSpan& span : noteSpans) {
				if (isRestToken(span.text)) {
					hasRestToken = true;
				}
				else if (span.text != "\\transpose") {
					// a copied bar, e.g. \1 or \1.\inst
					std::string barName = span.text.substr(0, span.text.find('.'));
					if (session.sharedData.barsIndexes.find(barName) == session.sharedData.barsIndexes.end()) written = false;
				}
			}
			if (!written || !hasRestToken) continue;
			// only the rest is highlighted, in the bar where it is written, and not the token of the bar that copies it (e.g. \1)
			for (const EditorNoteSpan& span : noteSpans) {
				if (span.pane == paneNdx && isRestToken(span.text)) spans.push_back(span);
			}
			continue;
		}
		for (const EditorNoteSpan& span : noteSpans) {
			if (span.pane == paneNdx) spans.push_back(span);
		}
	}
	return spans;
}

//--------------------------------------------------------------
void Workspace::storeEditorNoteSpans(int lineNum, int instNdx, int barNdx, int numNotes)
{
	// stores which token of the line of the editor every note of an instrument in a bar comes from, so that the token can be
	// highlighted while its note plays (see \editor.animate)
	// a note of a bar that copies another bar (e.g. \1 or \1.\inst) comes from the copying token and from the note of the copied bar
	// if the notes can't be matched to the tokens of the line (e.g. a line in a function), nothing is stored for the bar
	std::vector<std::vector<EditorNoteSpan>>& spans = editorNoteSpans[std::make_pair(instNdx, barNdx)];
	spans.clear();
	auto editorIt = editors.find(session.whichPane);
	if (editorIt == editors.end() || editorIt->second.allLines.find(lineNum) == editorIt->second.allLines.end()) return;
	const std::string& str = editorIt->second.allLines[lineNum].str;
	std::vector<PositionedToken> tokens = positionedTokens(str);
	// the melodic line starts after the name of the instrument, which can be at the start of the line or after \bar (e.g. \bar 1 { \inst c'1 })
	std::string instName = "\\" + session.sharedData.instruments[instNdx].getName();
	size_t first = 0;
	while (first < tokens.size() && tokens[first].text != instName && tokens[first].text.substr(0, instName.size() + 1) != instName + ".") first++;
	if (first >= tokens.size()) return;
	first++;
	// the melodic line ends at the end of the line, or at a closing bracket that doesn't belong to a tuplet (e.g. the end of a bar in one line)
	// in a bar written in one line, it also ends where the next statement starts, at the name of another instrument or group, or a command of the bar
	auto startsStatement = [&](const std::string& word) {
		if (word == "\\time" || word == "\\tempo" || word == "\\beatat") return true;
		if (word.size() < 2 || word[0] != '\\') return false;
		std::string name = word.substr(1, word.find('.') == std::string::npos ? std::string::npos : word.find('.') - 1);
		return session.sharedData.instrumentIndexes.find("\\" + name) != session.sharedData.instrumentIndexes.end() || session.instGroups.find("\\" + name) != session.instGroups.end();
	};
	size_t last = first;
	int braceDepth = 0;
	while (last < tokens.size()) {
		if (tokens[last].text == "{") braceDepth++;
		else if (tokens[last].text == "}") {
			if (braceDepth == 0) break;
			braceDepth--;
		}
		else if (braceDepth == 0 && startsStatement(tokens[last].text)) break;
		last++;
	}
	std::vector<PositionedToken> melodic(tokens.begin() + first, tokens.begin() + last);
	// with \bars, the bars are separated by |, and the notes are those of the bar being parsed
	if (session.parsingBars) {
		std::vector<PositionedToken> segment;
		int barCounter = 1;
		for (const PositionedToken& t : melodic) {
			if (t.text == "|") barCounter++;
			else if (barCounter == session.barsIterCounter) segment.push_back(t);
		}
		melodic = segment;
	}
	// commands with arguments in curly brackets make notes that are not written as they play (e.g. \euclide {16 5 4 c' cis'} or \transpose { c d e 2 })
	// so their arguments are not notes, and while their notes play, the command is highlighted as a whole
	// \euclide (and other such commands) from its backslash to its closing bracket, and \transpose with the contents of its curly brackets
	// \transpose without curly brackets has its arguments up to the number of semitones (e.g. \transpose \1 2)
	std::vector<int> groupEnd(melodic.size(), -1); // the index of the last token of a command with curly brackets
	std::vector<int> groupStart(melodic.size(), -1); // the index of the first token that is highlighted for it
	std::vector<bool> insideGroup(melodic.size(), false);
	std::vector<bool> skipToken(melodic.size(), false); // \transpose with notes only, and its number of semitones
	for (size_t i = 0; i < melodic.size(); i++) {
		const std::string& t = melodic[i].text;
		if (t.empty() || t[0] != '\\' || insideGroup[i] || t == "\\tuplet" || t == "\\tup") continue;
		bool isTranspose = (t == "\\transpose");
		size_t end = melodic.size();
		// a command that takes a name can have it before its curly brackets (e.g. \apply \1.\inst {c' e'}, see CommandSpec)
		size_t open = i + 1;
		if (i + 2 < melodic.size() && melodic[i+1].text != "{" && melodic[i+2].text == "{") open = i + 2;
		if (open < melodic.size() && melodic[open].text == "{") {
			int depth = 0;
			for (size_t j = open; j < melodic.size(); j++) {
				if (melodic[j].text == "{") depth++;
				else if (melodic[j].text == "}" && --depth == 0) {
					end = j;
					break;
				}
			}
		}
		else if (isTranspose) {
			for (size_t j = i + 1; j < melodic.size(); j++) {
				if (session.isFloat(melodic[j].text)) {
					end = j;
					break;
				}
			}
		}
		if (end >= melodic.size()) continue;
		if (isTranspose) {
			// with notes only (e.g. \transpose { c4 r e r -12 }), its notes and rests are highlighted as in any melodic line
			// and only \transpose and its number of semitones are not (with a bar name in it, it is highlighted as a whole)
			bool hasCommand = false;
			for (size_t j = i + 1; j <= end; j++) {
				if (!melodic[j].text.empty() && melodic[j].text[0] == '\\') hasCommand = true;
			}
			if (!hasCommand) {
				size_t amountNdx = (melodic[end].text == "}" ? end - 1 : end);
				skipToken[i] = true;
				if (amountNdx > i && session.isFloat(melodic[amountNdx].text)) skipToken[amountNdx] = true;
				continue;
			}
		}
		groupEnd[i] = (int)end;
		groupStart[i] = (int)(isTranspose ? i + 1 : i);
		for (size_t j = i + 1; j <= end; j++) insideGroup[j] = true;
	}
	std::vector<int> order;
	size_t ndx = 0;
	expandRepetitions(melodic, ndx, false, order);
	int pane = session.whichPane;
	int lineId = editorIt->second.getLineId(lineNum);
	auto spanOf = [&](int tokenNdx) {
		if (groupEnd[tokenNdx] >= 0) {
			// a command with curly brackets, from the first to the last of its highlighted tokens (see groupStart above)
			int start = melodic[groupStart[tokenNdx]].start;
			int length = melodic[groupEnd[tokenNdx]].start + (int)melodic[groupEnd[tokenNdx]].text.size() - start;
			return EditorNoteSpan{pane, lineId, start, length, str.substr(start, length)};
		}
		return EditorNoteSpan{pane, lineId, melodic[tokenNdx].start, (int)melodic[tokenNdx].text.size(), melodic[tokenNdx].text};
	};
	auto spansOf = [&](int tokenNdx) { return std::vector<EditorNoteSpan>{spanOf(tokenNdx)}; };
	// every token gives the notes it plays, each with its tokens
	// a command that is not known here (e.g. a variable or a function) gives the notes that are left, if it is the only one
	std::vector<std::vector<EditorNoteSpan>> result;
	int unknownPosition = -1; // where in result the notes of the unknown command go
	int unknownToken = -1;
	for (size_t k = 0; k < order.size(); k++) {
		const std::string& t = melodic[order[k]].text;
		if (t == "{" || t == "}" || t == "|" || t == "[" || t == "]") continue;
		// the arguments of a command with curly brackets (see groupEnd above)
		if (insideGroup[order[k]] || skipToken[order[k]]) continue;
		// commands that don't play notes, and their argument
		if (t == "\\ottava" || t == "\\ott" || t == "\\clef" || t == "\\tuplet" || t == "\\tup") {
			k++;
			continue;
		}
		if (t.empty() || t[0] != '\\') {
			// a note, a rest, or a chord, or the steps of the x - notation (e.g. x-x-), each with its own characters of the token
			std::vector<Step> steps = stepTokens(t);
			if (steps.size() > 1) {
				for (const Step& step : steps) {
					int start = melodic[order[k]].start + (int)step.start;
					result.push_back({EditorNoteSpan{pane, lineId, start, (int)step.length, t.substr(step.start, step.length)}});
				}
			}
			else {
				result.push_back(spansOf(order[k]));
			}
			continue;
		}
		// a bar that is copied, with the instrument of this line or another one, e.g. \1 or \1.\inst
		size_t dot = t.find('.');
		std::string barName = t.substr(0, dot);
		auto barIt = session.sharedData.barsIndexes.find(barName);
		if (barIt != session.sharedData.barsIndexes.end()) {
			int sourceInst = instNdx;
			if (dot != std::string::npos) {
				std::string sourceName = t.substr(dot + 1);
				if (!startsWith(sourceName, "\\")) sourceName = "\\" + sourceName;
				auto instIt = session.sharedData.instrumentIndexes.find(sourceName);
				if (instIt == session.sharedData.instrumentIndexes.end()) return;
				sourceInst = instIt->second;
			}
			auto sourceIt = editorNoteSpans.find(std::make_pair(sourceInst, barIt->second));
			int numSourceNotes = (int)session.sharedData.instruments[sourceInst].notes[barIt->second].size();
			for (int n = 0; n < numSourceNotes; n++) {
				std::vector<EditorNoteSpan> noteSpans = spansOf(order[k]);
				if (sourceIt != editorNoteSpans.end() && n < (int)sourceIt->second.size()) {
					noteSpans.insert(noteSpans.end(), sourceIt->second[n].begin(), sourceIt->second[n].end());
				}
				result.push_back(noteSpans);
			}
			continue;
		}
		// any other command
		if (unknownToken >= 0) return; // more than one, the notes can't be matched
		unknownToken = order[k];
		unknownPosition = (int)result.size();
	}
	if (unknownToken >= 0) {
		int remaining = numNotes - (int)result.size();
		if (remaining < 0) return;
		result.insert(result.begin() + unknownPosition, remaining, std::vector<EditorNoteSpan>{spanOf(unknownToken)});
	}
	if ((int)result.size() != numNotes) return;
	spans = result;
}

//--------------------------------------------------------------
void Workspace::storeActiveEditorElement(int instNdx, int barNdx, int dataCounter, bool state)
{
	// this function is called by the sequencer thread when a note of an instrument starts (state is true) or stops
	// the editors draw the tokens of the notes that play (see getActiveEditorNoteSpans()), on the main thread
	std::lock_guard<std::mutex> lock(activeEditorNotesMutex);
	if (state) {
		activeEditorNotes[instNdx] = std::make_pair(barNdx, dataCounter);
		activeEditorRests.erase(instNdx);
	}
	else {
		auto it = activeEditorNotes.find(instNdx);
		if (it != activeEditorNotes.end() && it->second == std::make_pair(barNdx, dataCounter)) {
			activeEditorNotes.erase(it);
			activeEditorRests.erase(instNdx);
		}
	}
}

//--------------------------------------------------------------
// a rest stays highlighted for its whole duration, as it has no release, until the next note or rest of the instrument replaces it
void Workspace::storeActiveEditorRest(int instNdx, int barNdx, int dataCounter)
{
	std::lock_guard<std::mutex> lock(activeEditorNotesMutex);
	activeEditorNotes[instNdx] = std::make_pair(barNdx, dataCounter);
	activeEditorRests.insert(instNdx);
}

//--------------------------------------------------------------
void Workspace::clearActiveEditorElement(int instNdx)
{
	std::lock_guard<std::mutex> lock(activeEditorNotesMutex);
	activeEditorNotes.erase(instNdx);
	activeEditorRests.erase(instNdx);
}

//--------------------------------------------------------------
// all LiveLily commands and the names defined in the session (instruments, bars, functions, etc.), with their backslash
// these are the names of commandsMap that start with a backslash, which is also used for the colors of the editor
std::vector<std::string> Workspace::getCommandCompletions()
{
	std::set<std::string> names;
	for (const auto& byChar : session.commandsMap[Session::livelily]) {
		for (const auto& cmd : byChar.second) {
			if (cmd.first.size() > 1 && cmd.first[0] == '\\') names.insert(cmd.first);
		}
	}
	return std::vector<std::string>(names.begin(), names.end());
}

//--------------------------------------------------------------
std::vector<std::string> Workspace::getArgumentCompletions()
{
	std::set<std::string> names;
	for (const auto& byChar : session.commandsMap[Session::livelily]) {
		for (const auto& cmd : byChar.second) {
			if (!cmd.first.empty() && cmd.first[0] != '\\') names.insert(cmd.first);
		}
	}
	return std::vector<std::string>(names.begin(), names.end());
}

//--------------------------------------------------------------
bool Workspace::startsMelodicLine(const std::string& word)
{
	return session.sharedData.instrumentIndexes.find(word) != session.sharedData.instrumentIndexes.end() || session.instGroups.find(word) != session.instGroups.end();
}

//--------------------------------------------------------------
// the methods of a command or of an object that is defined in the session, by its name with a backslash (e.g. \score or \violin)
std::vector<std::string> Workspace::getMethodCompletions(const std::string& name)
{
	static const std::map<std::string, std::vector<std::string>> commandMethods = {
		{"\\score", {"animation", "beatsin", "beattype", "colors", "correct", "finish", "hide", "movex", "movey",
					 "numbars", "play", "recenter", "reset", "seqres", "show", "stop", "stopnow", "update"}},
		{"\\editor", {"alignment", "animation", "autobrackets", "autocomplete"}},
		{"\\insts", {"add", "clear", "init", "remove"}},
		{"\\python", {"send"}},
		{"\\clicktrack", {"tune", "volume"}},
		{"\\maestro", {"levareaddr", "mainaddr", "toggleaddr", "valndx", "valthresh"}}
	};
	// groups of instruments take the methods of instruments
	static const std::vector<std::string> instrumentMethods = {"accoffset", "beatcolor", "cursor", "delay", "fullscreen", "midichan",
		"midiport", "numbars", "rhythm", "sendmidi", "sendto", "size", "sndfont", "transpose", "update"};
	static const std::vector<std::string> functionMethods = {"bind", "onrelease", "setargs", "unbind"};
	static const std::vector<std::string> listMethods = {"traverse"};
	static const std::vector<std::string> barLoopMethods = {"check", "goto", "locate"};
	static const std::vector<std::string> oscClientMethods = {"send", "setup"};
	static const std::vector<std::string> oscServerMethods = {"map", "setup"};
	static const std::vector<std::string> variableMethods = {"val"};
	auto it = commandMethods.find(name);
	if (it != commandMethods.end()) return it->second;
	// some of the maps below store names with their backslash and some without it
	std::string plainName = (startsWith(name, "\\") ? name.substr(1) : name);
	auto has = [&](const auto& m) { return m.find(name) != m.end() || m.find(plainName) != m.end(); };
	if (has(session.sharedData.instrumentIndexes) || has(session.instGroups)) return instrumentMethods;
	if (has(session.functionIndexes)) return functionMethods;
	if (has(session.listIndexes)) return listMethods;
	if (has(session.sharedData.barsIndexes) || has(session.sharedData.loopsIndexes)) return barLoopMethods;
	if (has(session.oscClients)) return oscClientMethods;
	if (has(session.oscServers)) return oscServerMethods;
	if (has(session.variables)) return variableMethods;
	return {};
}

std::vector<std::string> Workspace::pythonNames(const std::string& dotted)
{
#ifdef USEPYO
	if (!refreshPythonTable()) return {};
	if (dotted.empty()) return std::vector<std::string>(pythonTable.names.begin(), pythonTable.names.end());
	// a name that is not in the interpreter has no attributes to ask for
	if (pythonTable.names.find(dotted.substr(0, dotted.find('.'))) == pythonTable.names.end()) return {};
	auto it = pythonTable.attributes.find(dotted);
	if (it != pythonTable.attributes.end()) return it->second;
	std::vector<std::string> attributes = pythonLines(session.sharedData.pyo.names(dotted));
	pythonTable.attributes[dotted] = attributes;
	return attributes;
#else
	(void)dotted;
	return {};
#endif
}

std::vector<std::string> Workspace::pythonSignature(const std::string& dotted)
{
#ifdef USEPYO
	if (!refreshPythonTable() || dotted.empty()) return {};
	if (pythonTable.names.find(dotted.substr(0, dotted.find('.'))) == pythonTable.names.end()) return {};
	auto it = pythonTable.signatures.find(dotted);
	if (it != pythonTable.signatures.end()) return it->second;
	std::vector<std::string> params = pythonLines(session.sharedData.pyo.signature(dotted));
	pythonTable.signatures[dotted] = params;
	return params;
#else
	(void)dotted;
	return {};
#endif
}

bool Workspace::refreshPythonTable()
{
#ifdef USEPYO
	if (!session.pyoSet || !session.sharedData.pyo.isSetup()) return false;
	unsigned executions = session.sharedData.pyo.getExecutions();
	if (!pythonTable.made || pythonTable.executions != executions) {
		std::vector<std::string> names = pythonLines(session.sharedData.pyo.globalNames());
		pythonTable.names = std::set<std::string>(names.begin(), names.end());
		pythonTable.attributes.clear();
		pythonTable.signatures.clear();
		pythonTable.executions = executions;
		pythonTable.made = true;
	}
	return true;
#else
	return false;
#endif
}

//--------------------------------------------------------------
void Workspace::parseStrings(int index, int numLines)
{
	session.barError = false;
	// an unmatched curly bracket (e.g. a bar definition that is not finished) is an error
	// and in this case none of the lines is parsed
	std::string bracketError;
	int unmatchedBracketLine = editors[session.whichPane].findUnmatchedBracket(index, numLines, bracketError);
	if (unmatchedBracketLine > -1) {
		editors[session.whichPane].setTraceback(3, bracketError, unmatchedBracketLine);
		return;
	}
	// collect the lines to parse from the editor
	std::vector<std::pair<int, std::string>> lines;
	for (int i = index; i < (index+numLines); i++) {
		if (editors[session.whichPane].getSessionActivity()) {
			lines.push_back(std::make_pair(i, editors[session.whichPane].allLines[i].str));
			if (editors[session.whichPane].getSendLines()) {
				OscMessage m;
				m.setAddress("/livelily"+std::to_string(editors[session.whichPane].getSendLinesPaneNdx())+"/press");
				m.addStringArg(editors[session.whichPane].allLines[i].str);
				editors[session.whichPane].oscKeys->send(m);
			}
		}
		else {
			if (editors[session.whichPane].allLines[i].str == "\\active") {
				lines.push_back(std::make_pair(i, editors[session.whichPane].allLines[i].str));
			}
		}
	}
	session.parseLines(lines, numLines, false);
}

//--------------------------------------------------------------
void Workspace::applyPendingTracebacks()
{
	// take the queued tracebacks while holding the lock, and apply them after releasing it
	std::vector<PendingTraceback> tracebacks;
	{
		std::lock_guard<std::mutex> lock(pendingTracebacksMutex);
		if (pendingTracebacks.empty()) return;
		tracebacks.swap(pendingTracebacks);
	}
	for (const PendingTraceback& tb : tracebacks) {
		int pane = tb.pane;
		int lineNum = tb.lineNum;
		// tracebacks of bound functions go to the line where the function is defined
		// if that line still defines it, otherwise to the dummy line they were set with
		if (!tb.boundFunction.empty()) {
			std::pair<int, int> definition = findFunctionDefinitionLine(tb.boundFunction);
			if (definition.first > -1) {
				pane = definition.first;
				lineNum = definition.second;
			}
		}
		// the pane might have been closed and the line deleted since the traceback was queued
		auto editorIt = editors.find(pane);
		if (editorIt == editors.end()) continue;
		// setTraceback() uses operator[] which would create the line if it doesn't exist
		if (editorIt->second.allLines.find(lineNum) == editorIt->second.allLines.end()) continue;
		if (tb.release) editorIt->second.releaseTraceback(lineNum);
		else editorIt->second.setTraceback(tb.errorCode, tb.errorStr, lineNum);
	}
}

//--------------------------------------------------------------
std::pair<int, int> Workspace::findFunctionDefinitionLine(const std::string& funcName)
{
	int pane, defLine;
	{
		std::lock_guard<std::mutex> lock(session.sharedData.functionsMutex);
		auto funcIndexIt = session.functionIndexes.find(funcName);
		if (funcIndexIt == session.functionIndexes.end()) return std::make_pair(-1, -1);
		auto funcIt = session.sharedData.functions.find(funcIndexIt->second);
		if (funcIt == session.sharedData.functions.end()) return std::make_pair(-1, -1);
		pane = funcIt->second.getDefinitionPane();
		defLine = funcIt->second.getDefinitionLine();
	}
	auto editorIt = editors.find(pane);
	if (editorIt == editors.end()) return std::make_pair(-1, -1);
	std::map<int, line>& lines = editorIt->second.allLines;
	// first check the stored line, which is the common case and avoids searching the pane on every call
	auto lineIt = lines.find(defLine);
	if (lineIt != lines.end() && lineDefinesFunction(lineIt->second.str, funcName)) {
		return std::make_pair(pane, defLine);
	}
	// the definition has moved (e.g. lines were inserted or deleted above it), or its line was changed or deleted
	// so search the pane for it, and if it is defined more than once, pick the definition closest to the stored line
	int foundLine = -1;
	int minDistance = std::numeric_limits<int>::max();
	for (auto it = lines.begin(); it != lines.end(); ++it) {
		if (lineDefinesFunction(it->second.str, funcName)) {
			int distance = std::abs(it->first - defLine);
			if (distance < minDistance) {
				minDistance = distance;
				foundLine = it->first;
			}
		}
	}
	// the definition is no longer in the pane
	if (foundLine < 0) return std::make_pair(-1, -1);
	// store the new line, so the next lookup finds it without searching
	{
		std::lock_guard<std::mutex> lock(session.sharedData.functionsMutex);
		auto funcIndexIt = session.functionIndexes.find(funcName);
		if (funcIndexIt != session.functionIndexes.end()) {
			auto funcIt = session.sharedData.functions.find(funcIndexIt->second);
			// only if the function hasn't been redefined elsewhere in the meantime
			if (funcIt != session.sharedData.functions.end() && funcIt->second.getDefinitionPane() == pane && funcIt->second.getDefinitionLine() == defLine) {
				funcIt->second.setDefinitionLocation(pane, foundLine);
			}
		}
	}
	return std::make_pair(pane, foundLine);
}

//--------------------------------------------------------------
bool Workspace::lineDefinesFunction(const std::string& str, const std::string& funcName)
{
	// a definition line is: optional white space, "\function", white space, the function name (without the backslash)
	// followed by white space, an opening curly bracket, a comment, or the end of the line
	const std::string cmd = "\\function";
	size_t pos = str.find_first_not_of(" \t");
	if (pos == std::string::npos || str.compare(pos, cmd.size(), cmd) != 0) return false;
	pos += cmd.size();
	size_t nameStart = str.find_first_not_of(" \t", pos);
	// there must be white space between \function and the name
	if (nameStart == std::string::npos || nameStart == pos) return false;
	size_t nameEnd = str.find_first_of(" \t{%", nameStart);
	std::string name = str.substr(nameStart, nameEnd == std::string::npos ? std::string::npos : nameEnd - nameStart);
	return "\\" + name == funcName;
}

//--------------------------------------------------------------
// the editors are drawn on the main thread, so a message from another thread (e.g. a function called by the sequencer)
// or from a function the sequencer called, which goes to the line of its definition, is queued, and shown in update()
void Workspace::setLineMessage(int pane, int line, int errorCode, const std::string& text, const std::string& boundFunction)
{
	if (std::this_thread::get_id() == mainThreadId && boundFunction.empty()) {
		editors[pane].setTraceback(errorCode, text, line);
		return;
	}
	std::lock_guard<std::mutex> lock(pendingTracebacksMutex);
	pendingTracebacks.push_back({pane, line, false, errorCode, text, boundFunction});
}

//--------------------------------------------------------------
void Workspace::clearLineMessage(int pane, int line, const std::string& boundFunction)
{
	if (std::this_thread::get_id() == mainThreadId && boundFunction.empty()) {
		editors[pane].releaseTraceback(line);
		return;
	}
	std::lock_guard<std::mutex> lock(pendingTracebacksMutex);
	pendingTracebacks.push_back({pane, line, true, 0, "", boundFunction});
}

//--------------------------------------------------------------
void Workspace::setPaneLanguage(int pane, int language)
{
	editors[pane].setLanguage(language);
}

//--------------------------------------------------------------
bool Workspace::openHelp(int pane, const std::string& filePath, int newPane)
{
	// a new pane beside or below the active one (as Ctrl+v and Ctrl+h do), which becomes the active one
	if (pane != session.whichPane) setActivePane(pane);
	if (newPane == 1) addPane(true);
	else if (newPane == 2) addPane(false);
	bool fileOpened = editors[session.whichPane].loadHelpFile(filePath, newPane);
	if (!fileOpened && newPane > 0) removePane();
	return fileOpened;
}

//--------------------------------------------------------------
void Workspace::setEditorOption(int pane, const std::string& option, const std::string& value)
{
	Editor& editor = editors[pane];
	if (option == "alignment") {
		// the width of a unit of the grid applies at once if alignment is on, and the next time it is turned on otherwise
		if (value == "short" || value == "long") {
			editor.alignShort = (value == "short");
			if (editor.alignment) alignBars(pane);
		}
		else {
			editor.alignment = (value == "on");
			if (editor.alignment) alignBars(pane);
			else unalignBars(pane);
		}
	}
	else if (option == "animation") editor.setAnimation(value == "on");
	else if (option == "autobrackets") editor.setAutocomplete(value == "on");
	else if (option == "autocomplete") editor.setAutocompletion(value == "on");
}

//--------------------------------------------------------------
void Workspace::setPaneActive(int pane, bool active)
{
	editors[pane].setSessionActivity(active);
}

//--------------------------------------------------------------
void Workspace::sendKeysTo(int pane, const std::string& host, int targetPane)
{
	editors[pane].oscKeys->setup(host, OSCPORT);
	editors[pane].setSendKeys(targetPane);
}

//--------------------------------------------------------------
void Workspace::sendLinesTo(int pane, const std::string& host, int targetPane)
{
	editors[pane].oscKeys->setup(host, OSCPORT);
	editors[pane].setSendLines(targetPane);
}

//--------------------------------------------------------------
int Workspace::tabSize(int pane)
{
	return editors[pane].getTabSize();
}

//--------------------------------------------------------------
void Workspace::connectLineToBar(int pane, int line, int instNdx, int bar)
{
	editors[pane].connectLineToBar(line, instNdx, bar);
}

//--------------------------------------------------------------
void Workspace::fillGrids()
{
	// as App::draw() fills them, which paints them in between, with the views of the score
	for (auto& editor : editors) {
		editor.second.drawText();
		editor.second.drawPaneSeparator();
		editor.second.takePaints();
	}
	fillBelowArea();
	editors[session.whichPane].drawCompletions();
	editors[session.whichPane].takePaints();
}

//--------------------------------------------------------------
void Workspace::instrumentRemoved(int instNdx)
{
	// the instruments after the removed one get the indexes before (see Session::removeInstrument())
	auto newIndex = [instNdx](int ndx) { return (ndx > instNdx ? ndx - 1 : ndx); };
	// the tokens of the editor that play the notes of the instruments (\editor.animate)
	std::map<std::pair<int, int>, std::vector<std::vector<EditorNoteSpan>>> spans;
	for (auto& entry : editorNoteSpans) {
		if (entry.first.first == instNdx) continue;
		spans[std::make_pair(newIndex(entry.first.first), entry.first.second)] = entry.second;
	}
	editorNoteSpans = spans;
	{
		std::lock_guard<std::mutex> lock(activeEditorNotesMutex);
		activeEditorNotes.clear();
		activeEditorRests.clear();
	}
	// the lines of the editors that the bars of the instruments are written in
	for (auto& editor : editors) {
		std::map<int, std::map<int, int>> connected;
		for (auto& entry : editor.second.instsConnectedToLine) {
			if (entry.first == instNdx) continue;
			connected[newIndex(entry.first)] = entry.second;
		}
		editor.second.instsConnectedToLine = connected;
	}
}

//--------------------------------------------------------------
void Workspace::instrumentsCleared()
{
	// the tokens of the editor that play the notes of the instruments (\editor.animation)
	editorNoteSpans.clear();
	std::lock_guard<std::mutex> lock(activeEditorNotesMutex);
	activeEditorNotes.clear();
	activeEditorRests.clear();
}
