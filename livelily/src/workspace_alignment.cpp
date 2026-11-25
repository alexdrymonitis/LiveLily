// aligning the melodic lines of bar definitions in the editor, so that notes that sound together are written under each other (see \editor.alignment)
// as a score engraver spaces notes, the shortest duration of a bar (in all its lines) is the unit of its grid
// and one unit is as wide as the longest note token of the bar, plus a space, so a note is placed at its onset in units, times that width
// or with \editor.alignment short, as wide as the shortest note token of the bar, plus a space, and a longer token is an offset at its onset
// (notes of tuplets that fall between the units are placed in proportion, between them)
// the width of a note token is its pitch, duration, articulations, and dynamics, without any text in quotes (e.g. ^"some text")
// the text, \ottava, \ott, and \clef with their arguments before a note, and the brackets of tuplets, are an offset at the onset of their note
// which moves the onsets after it in all the lines, but doesn't widen the unit of the bar
// so a line that starts with \clef, \ottava, or \ott starts the notes of all the lines after it
// only lines of notes, rests, and chords (and the commands above) are aligned, and a line with anything else (e.g. a copied bar, \euclide,
// a repetition, or a comment) is left as it is, without affecting the others
// in \bars, every bar (between the | separators) is aligned separately, with its own unit, so the separators are aligned too

#include "workspace.h"
#include "session.h"
#include "tokens.h"
#include <numeric>
#include <algorithm>

namespace {

// a time in a bar, as a fraction of a whole note
struct Frac {
	int64_t n = 0;
	int64_t d = 1;
};

Frac frac(int64_t n, int64_t d)
{
	if (d < 0) {
		n = -n;
		d = -d;
	}
	int64_t g = std::gcd(n < 0 ? -n : n, d);
	if (g == 0) g = 1;
	return {n / g, d / g};
}

Frac operator+(Frac a, Frac b) { return frac(a.n * b.d + b.n * a.d, a.d * b.d); }
Frac operator*(Frac a, Frac b) { return frac(a.n * b.n, a.d * b.d); }
bool operator<(Frac a, Frac b) { return a.n * b.d < b.n * a.d; }
bool operator==(Frac a, Frac b) { return a.n == b.n && a.d == b.d; }

// a note, rest, or chord, as the indexes of its tokens (with the commands before it and the closing brackets after it), its onset, and its duration
// note is the index in tokens of the token of the note itself
struct Cell {
	std::vector<int> tokens;
	size_t note = 0;
	Frac onset;
	Frac dur;
};

// a melodic line of a bar definition, as its tokens (the first is the name of the instrument) and its bars of cells
// (more bars in \bars, with the indexes of the | tokens between them)
struct MelodicLine {
	int lineNdx;
	std::string indent;
	std::vector<PositionedToken> tokens;
	std::vector<std::vector<Cell>> bars;
	std::vector<int> separators;
};

bool isNumber(const std::string& str)
{
	return !str.empty() && std::all_of(str.begin(), str.end(), [](char c) { return isdigit((unsigned char)c); });
}

// reads the cells of a melodic line, with the durations written in it (a missing duration is the previous one, with its dots)
// and gives false if the line has anything that can't be aligned from its text, or ends with a command that waits for its note
// a token of the x - notation of rhythm staffs (e.g. x, -, or x-x-) is a note as long as its steps, each one step of \score.seqres
bool readCells(MelodicLine& line, int seqResolution)
{
	const std::vector<PositionedToken>& t = line.tokens;
	line.bars.assign(1, std::vector<Cell>());
	Frac time = frac(0, 1);
	Frac scale = frac(1, 1);
	std::vector<Frac> outerScales; // the scales of the tuplets around the current one
	std::vector<int> pending; // the commands written before the next note
	int lastDen = -1;
	int lastDots = 0;
	for (size_t i = 1; i < t.size(); i++) {
		const std::string& s = t[i].text;
		if (s == "|") {
			if (!pending.empty() || !outerScales.empty()) return false;
			line.separators.push_back((int)i);
			line.bars.push_back(std::vector<Cell>());
			time = frac(0, 1);
			continue;
		}
		if (s == "\\ottava" || s == "\\ott" || s == "\\clef") {
			if (i + 1 >= t.size()) return false;
			pending.push_back((int)i);
			pending.push_back((int)(i + 1));
			i++;
			continue;
		}
		if (s == "\\tuplet" || s == "\\tup") {
			if (i + 2 >= t.size() || t[i+2].text != "{") return false;
			const std::string& ratio = t[i+1].text;
			size_t slash = ratio.find('/');
			if (slash == std::string::npos || !isNumber(ratio.substr(0, slash)) || !isNumber(ratio.substr(slash + 1))) return false;
			int num = std::stoi(ratio.substr(0, slash));
			int den = std::stoi(ratio.substr(slash + 1));
			if (num <= 0 || den <= 0) return false;
			// e.g. 3/2 is three notes in the time of two
			outerScales.push_back(scale);
			scale = scale * frac(den, num);
			pending.push_back((int)i);
			pending.push_back((int)(i + 1));
			pending.push_back((int)(i + 2));
			i += 2;
			continue;
		}
		if (s == "}") {
			if (outerScales.empty() || !pending.empty() || line.bars.back().empty()) return false;
			scale = outerScales.back();
			outerScales.pop_back();
			line.bars.back().back().tokens.push_back((int)i);
			continue;
		}
		std::vector<Step> steps = stepTokens(s);
		if (!steps.empty()) {
			Cell cell;
			cell.tokens = pending;
			cell.note = cell.tokens.size();
			cell.tokens.push_back((int)i);
			pending.clear();
			cell.onset = time;
			cell.dur = frac((int64_t)steps.size(), std::max(1, seqResolution)) * scale;
			time = time + cell.dur;
			line.bars.back().push_back(cell);
			continue;
		}
		if (s.empty() || s[0] == '\\' || s == "{" || s == "[" || s == "]" || s.find('*') != std::string::npos) return false;
		// the duration is the number after the pitch, a chord, or a rest (or a number alone, e.g. in rhythm staffs), with its dots
		size_t p = 0;
		if (s[0] == '<') {
			p = s.find('>');
			if (p == std::string::npos) return false;
			p++;
		}
		else if (isalpha((unsigned char)s[0])) {
			while (p < s.size() && (isalpha((unsigned char)s[p]) || s[p] == '\'' || s[p] == ',')) p++;
		}
		else if (!isdigit((unsigned char)s[0])) {
			return false;
		}
		int den = 0;
		size_t q = p;
		while (q < s.size() && isdigit((unsigned char)s[q])) {
			den = den * 10 + (s[q] - '0');
			if (den > 64) return false;
			q++;
		}
		int dots = 0;
		if (q > p) {
			while (q < s.size() && s[q] == '.') {
				dots++;
				q++;
			}
			if (den != 1 && den != 2 && den != 4 && den != 8 && den != 16 && den != 32 && den != 64) return false;
			lastDen = den;
			lastDots = dots;
		}
		else {
			if (lastDen < 0) return false;
			den = lastDen;
			dots = lastDots;
		}
		// a dot adds half of the value, so one dot is 3/2 of it and two dots 7/4
		Cell cell;
		cell.tokens = pending;
		cell.note = cell.tokens.size();
		cell.tokens.push_back((int)i);
		pending.clear();
		cell.onset = time;
		cell.dur = frac(((int64_t)1 << (dots + 1)) - 1, (int64_t)den << dots) * scale;
		time = time + cell.dur;
		line.bars.back().push_back(cell);
	}
	return pending.empty();
}

// the width of the text in quotes of a token (e.g. ^"some text"), with the character before the quotes
int textWidth(const std::string& token)
{
	int width = 0;
	size_t pos = 0;
	while ((pos = token.find('"', pos)) != std::string::npos) {
		size_t close = token.find('"', pos + 1);
		if (close == std::string::npos) close = token.size() - 1;
		width += (int)(close - pos + 1) + (pos > 0 ? 1 : 0);
		pos = close + 1;
	}
	return width;
}

// the width of the note token of a cell, without its text, which sets the width of the unit of the bar
int noteWidth(const MelodicLine& line, const Cell& cell)
{
	const std::string& token = line.tokens[cell.tokens[cell.note]].text;
	return (int)token.size() - textWidth(token);
}

// the number of units of the grid a note token is laid out over: one, or the number of steps of a token of the x - notation (e.g. 4 for x-x-)
// so that a token of many steps is as wide per step as other tokens, and doesn't widen the unit of the bar
int stepsOf(const MelodicLine& line, const Cell& cell)
{
	return std::max((size_t)1, stepTokens(line.tokens[cell.tokens[cell.note]].text).size());
}

// the width a note token needs from one unit of the grid, spread over its steps
int widthPerStep(const MelodicLine& line, const Cell& cell)
{
	int steps = stepsOf(line, cell);
	return (noteWidth(line, cell) + steps - 1) / steps;
}

// the width of what is written before the note token (commands and the opening of tuplets, each followed by a space)
int beforeWidth(const MelodicLine& line, const Cell& cell)
{
	int width = 0;
	for (size_t k = 0; k < cell.note; k++) width += (int)line.tokens[cell.tokens[k]].text.size() + 1;
	return width;
}

// the width of what is written after the note token: its text, and the closing brackets of tuplets, each after a space
int afterWidth(const MelodicLine& line, const Cell& cell)
{
	int width = textWidth(line.tokens[cell.tokens[cell.note]].text);
	for (size_t k = cell.note + 1; k < cell.tokens.size(); k++) width += (int)line.tokens[cell.tokens[k]].text.size() + 1;
	return width;
}

// a line with runs of spaces between its tokens made single spaces, outside quotes, keeping its indentation, without trailing spaces
std::string singleSpaces(const std::string& str)
{
	size_t first = str.find_first_not_of(" \t");
	if (first == std::string::npos) return str;
	std::string out = str.substr(0, first);
	bool insideQuotes = false;
	bool prevSpace = false;
	for (size_t i = first; i < str.size(); i++) {
		char c = str[i];
		if (c == '"') insideQuotes = !insideQuotes;
		if (!insideQuotes && c == ' ') {
			if (prevSpace) continue;
			prevSpace = true;
		}
		else {
			prevSpace = false;
		}
		out += c;
	}
	while (out.size() > first && out.back() == ' ') out.pop_back();
	return out;
}

}

//--------------------------------------------------------------
bool Workspace::alignBarLines(int pane, int firstLine, int lastLine, int cursorLine, int cursorCol, int& newCursorCol, bool keepInToken)
{
	Editor& editor = editors[pane];
	// the melodic lines that can be aligned: a line that starts with a name (with a backslash) that is not a command or another kind of name
	// is the line of an instrument or a group, also before the instruments are created (e.g. a file that is loaded and not executed yet)
	std::vector<MelodicLine> lines;
	for (int l = firstLine; l <= lastLine; l++) {
		auto it = editor.allLines.find(l);
		if (it == editor.allLines.end()) continue;
		const std::string& str = it->second.str;
		size_t first = str.find_first_not_of(" \t");
		if (first == std::string::npos || str.find('%') != std::string::npos) continue;
		MelodicLine line;
		line.lineNdx = l;
		line.indent = str.substr(0, first);
		line.tokens = positionedTokens(str);
		if (line.tokens.empty()) continue;
		// the steps of a compound token of the x - notation are separated, as if they were typed with spaces between them (e.g. x-x- is x - x -)
		std::vector<PositionedToken> separated;
		for (size_t k = 0; k < line.tokens.size(); k++) {
			std::vector<Step> steps = (k > 0 ? stepTokens(line.tokens[k].text) : std::vector<Step>());
			if (steps.size() < 2) {
				separated.push_back(line.tokens[k]);
				continue;
			}
			for (const Step& step : steps) {
				separated.push_back({line.tokens[k].text.substr(step.start, step.length), line.tokens[k].start + (int)step.start});
			}
		}
		line.tokens = separated;
		const std::string& name = line.tokens[0].text;
		if (name.size() < 2 || name[0] != '\\' || name.find('.') != std::string::npos || session.commandTable().count(name)) continue;
		Session::NameKind kind = session.nameKind(name);
		if (kind != Session::NameKind::none && kind != Session::NameKind::instrument && kind != Session::NameKind::group) continue;
		if (!readCells(line, session.seqResolution)) {
			// a line that is being edited and can't be read yet (e.g. c14 while typing c16) leaves the bar as it is
			// otherwise the other lines would be aligned without it, and move back when it can be read again
			if (keepInToken && l == cursorLine) return false;
			continue;
		}
		lines.push_back(line);
	}
	if (lines.empty()) return false;
	// every bar: its unit (its shortest duration), the width of a unit (its longest note token and a space), its onsets and the ends of its lines
	// and the offsets at its onsets (what is written before and after the note tokens that start there)
	size_t numBars = 0;
	int nameWidth = 0;
	for (const MelodicLine& line : lines) {
		numBars = std::max(numBars, line.bars.size());
		nameWidth = std::max(nameWidth, (int)(line.indent.size() + line.tokens[0].text.size()) + 1);
	}
	std::vector<Frac> unit(numBars, frac(1, 4));
	std::vector<int> unitWidth(numBars, 1);
	std::vector<Frac> barEnd(numBars, frac(0, 1));
	std::vector<std::vector<Frac>> times(numBars);
	std::vector<std::vector<int>> before(numBars);
	std::vector<std::vector<int>> after(numBars);
	for (size_t b = 0; b < numBars; b++) {
		bool hasCells = false;
		for (const MelodicLine& line : lines) {
			if (b >= line.bars.size()) continue;
			for (const Cell& cell : line.bars[b]) {
				times[b].push_back(cell.onset);
				times[b].push_back(cell.onset + cell.dur);
				if (barEnd[b] < cell.onset + cell.dur) barEnd[b] = cell.onset + cell.dur;
				if (!hasCells || cell.dur < unit[b]) unit[b] = cell.dur;
				// the longest note token, or with short, the shortest
				if (editor.alignShort) unitWidth[b] = (hasCells ? std::min(unitWidth[b], widthPerStep(line, cell) + 1) : widthPerStep(line, cell) + 1);
				else unitWidth[b] = std::max(unitWidth[b], widthPerStep(line, cell) + 1);
				hasCells = true;
			}
		}
		times[b].push_back(barEnd[b]);
		std::sort(times[b].begin(), times[b].end());
		times[b].erase(std::unique(times[b].begin(), times[b].end()), times[b].end());
		before[b].assign(times[b].size(), 0);
		after[b].assign(times[b].size(), 0);
		for (const MelodicLine& line : lines) {
			if (b >= line.bars.size()) continue;
			for (const Cell& cell : line.bars[b]) {
				size_t t = std::lower_bound(times[b].begin(), times[b].end(), cell.onset) - times[b].begin();
				before[b][t] = std::max(before[b][t], beforeWidth(line, cell));
				// with short, what a note token is longer than a unit is an offset at its onset, as text is
				int longer = std::max(0, noteWidth(line, cell) + 1 - stepsOf(line, cell) * unitWidth[b]);
				after[b][t] = std::max(after[b][t], afterWidth(line, cell) + longer);
			}
		}
	}
	std::vector<int> barStart(numBars, nameWidth);
	// the column where the note tokens that start at a time of a bar start: the time in units of the bar, times the width of a unit
	// plus the offsets of the onsets before it, and what is written before the note tokens at it
	auto column = [&](size_t b, Frac time) {
		Frac units = frac(time.n * unit[b].d, time.d * unit[b].n);
		int col = barStart[b] + (int)((units.n * unitWidth[b]) / units.d);
		for (size_t t = 0; t < times[b].size() && !(time < times[b][t]); t++) {
			if (times[b][t] < time) col += before[b][t] + after[b][t];
			else col += before[b][t];
		}
		return col;
	};
	// a bar starts after the previous one and its separator (| and a space)
	for (size_t b = 1; b < numBars; b++) barStart[b] = column(b - 1, barEnd[b-1]) + 2;
	bool changed = false;
	for (const MelodicLine& line : lines) {
		std::string out = line.indent + line.tokens[0].text;
		std::vector<int> newPos(line.tokens.size(), -1);
		newPos[0] = (int)line.indent.size();
		// pads the line to a column, and keeps a space if it is already past it (which the widths of the columns prevent)
		auto padTo = [&out](int col) {
			if ((int)out.size() < col) out.append(col - out.size(), ' ');
			else if (!out.empty() && out.back() != ' ') out += ' ';
		};
		for (size_t b = 0; b < line.bars.size(); b++) {
			for (const Cell& cell : line.bars[b]) {
				// what is written before the note token comes before its column, so that the note tokens are aligned
				padTo(column(b, cell.onset) - beforeWidth(line, cell));
				for (size_t k = 0; k < cell.tokens.size(); k++) {
					if (k > 0) out += ' ';
					newPos[cell.tokens[k]] = (int)out.size();
					out += line.tokens[cell.tokens[k]].text;
				}
			}
			if (b < line.separators.size()) {
				padTo(column(b, barEnd[b]));
				newPos[line.separators[b]] = (int)out.size();
				out += "|";
			}
		}
		// the cursor goes to the token after it, or if there is none, to the column of the end of the line (where its next note starts)
		// with keepInToken, it stays in the token it is in or right after, at the same place in it
		// or between the two tokens it is between, after the first one, and before the second one if they are far enough apart
		if (line.lineNdx == cursorLine) {
			newCursorCol = -1;
			if (keepInToken) {
				int prev = -1;
				for (size_t k = 0; k < line.tokens.size() && newCursorCol < 0; k++) {
					if (newPos[k] < 0) continue;
					int start = line.tokens[k].start;
					int end = start + (int)line.tokens[k].text.size();
					if (cursorCol >= start && cursorCol <= end) newCursorCol = newPos[k] + (cursorCol - start);
					else if (end < cursorCol) prev = (int)k;
					else if (prev >= 0) {
						int prevEnd = newPos[prev] + (int)line.tokens[prev].text.size();
						int gap = cursorCol - (line.tokens[prev].start + (int)line.tokens[prev].text.size());
						newCursorCol = std::max(prevEnd + 1, std::min(prevEnd + gap, newPos[k] - 1));
					}
				}
				// after the last token, the cursor goes to where the next note starts, as after a space (see below)
			}
			for (size_t k = 0; k < line.tokens.size() && !keepInToken; k++) {
				if (line.tokens[k].start >= cursorCol && newPos[k] >= 0) {
					newCursorCol = newPos[k];
					break;
				}
			}
			if (newCursorCol < 0) {
				size_t lastBar = line.bars.size() - 1;
				Frac end = (line.bars[lastBar].empty() ? frac(0, 1) : line.bars[lastBar].back().onset + line.bars[lastBar].back().dur);
				newCursorCol = (line.bars[lastBar].empty() && lastBar == 0 ? std::max(column(0, end), (int)out.size() + 1) : column(lastBar, end));
				if ((int)out.size() < newCursorCol) out.append(newCursorCol - out.size(), ' ');
			}
		}
		if (editor.allLines[line.lineNdx].str != out) {
			editor.allLines[line.lineNdx].str = out;
			changed = true;
			// the notes of the bars already executed highlight the tokens of this line where they were, so they follow the tokens
			std::map<int, int> newStarts;
			for (size_t k = 0; k < line.tokens.size(); k++) {
				if (newPos[k] >= 0) newStarts[line.tokens[k].start] = newPos[k];
			}
			moveHighlights(pane, line.lineNdx, newStarts);
		}
	}
	if (changed) editor.setFileEdited(true);
	return true;
}

//--------------------------------------------------------------
void Workspace::moveHighlights(int pane, int lineNdx, const std::map<int, int>& newStarts)
{
	int lineId = editors[pane].getLineId(lineNdx);
	for (auto& bar : editorNoteSpans) {
		for (auto& note : bar.second) {
			for (EditorNoteSpan& span : note) {
				if (span.pane != pane || span.lineId != lineId) continue;
				auto it = newStarts.find(span.start);
				if (it != newStarts.end()) span.start = it->second;
			}
		}
	}
}

//--------------------------------------------------------------
// the lines of a bar definition that can be melodic lines: between the line of \bar or \bars and its closing bracket
// (or its last line, if it is not closed yet, while it is being written), for definitions in more than one line
static bool barDefinitionLines(Editor& editor, const BracketChunk& chunk, int& firstLine, int& lastLine)
{
	auto top = editor.allLines.find(chunk.top);
	if (top == editor.allLines.end() || chunk.bottom <= chunk.top) return false;
	std::string command = Editor::definitionCommand(top->second.str);
	if (command != "\\bar" && command != "\\bars") return false;
	firstLine = chunk.top + 1;
	lastLine = (chunk.finished ? chunk.bottom - 1 : chunk.bottom);
	return firstLine <= lastLine;
}

//--------------------------------------------------------------
void Workspace::alignBars(int pane)
{
	Editor& editor = editors[pane];
	if (editor.allLines.empty()) return;
	editor.beforeChange(true);
	for (const BracketChunk& chunk : editor.findBracketChunks(editor.allLines.begin()->first, editor.allLines.rbegin()->first)) {
		int firstLine, lastLine, unused;
		if (barDefinitionLines(editor, chunk, firstLine, lastLine)) alignBarLines(pane, firstLine, lastLine, -1, 0, unused);
	}
	editor.afterChange(true);
}

//--------------------------------------------------------------
void Workspace::alignAfterEdit(int pane, bool space)
{
	Editor& editor = editors[pane];
	if (editor.allLines.empty()) return;
	int cursorLine = editor.getCursorLineIndex();
	for (const BracketChunk& chunk : editor.findBracketChunks(editor.allLines.begin()->first, editor.allLines.rbegin()->first)) {
		if (chunk.top > cursorLine || chunk.bottom < cursorLine) continue;
		int firstLine, lastLine;
		if (!barDefinitionLines(editor, chunk, firstLine, lastLine) || cursorLine < firstLine || cursorLine > lastLine) return;
		int newCursorCol = -1;
		if (alignBarLines(pane, firstLine, lastLine, cursorLine, editor.getCursorColumn(), newCursorCol, !space) && newCursorCol >= 0) {
			editor.setCursorColumn(newCursorCol);
		}
		return;
	}
}

//--------------------------------------------------------------
void Workspace::alignAfterPaste(int pane)
{
	Editor& editor = editors[pane];
	if (editor.allLines.empty()) return;
	int cursorLine = editor.getCursorLineIndex();
	for (const BracketChunk& chunk : editor.findBracketChunks(editor.allLines.begin()->first, editor.allLines.rbegin()->first)) {
		int firstLine, lastLine;
		if (!barDefinitionLines(editor, chunk, firstLine, lastLine)) continue;
		int newCursorCol = -1;
		if (cursorLine >= firstLine && cursorLine <= lastLine) {
			// the bar with the cursor keeps it in the token it is in, as after typing (see alignAfterEdit())
			if (alignBarLines(pane, firstLine, lastLine, cursorLine, editor.getCursorColumn(), newCursorCol, true) && newCursorCol >= 0) {
				editor.setCursorColumn(newCursorCol);
			}
		}
		else {
			alignBarLines(pane, firstLine, lastLine, -1, 0, newCursorCol);
		}
	}
}

//--------------------------------------------------------------
void Workspace::unalignBars(int pane)
{
	// the melodic lines of the bar definitions get single spaces between their tokens again
	Editor& editor = editors[pane];
	if (editor.allLines.empty()) return;
	editor.beforeChange(true);
	bool changed = false;
	for (const BracketChunk& chunk : editor.findBracketChunks(editor.allLines.begin()->first, editor.allLines.rbegin()->first)) {
		int firstLine, lastLine;
		if (!barDefinitionLines(editor, chunk, firstLine, lastLine)) continue;
		for (int l = firstLine; l <= lastLine; l++) {
			auto it = editor.allLines.find(l);
			if (it == editor.allLines.end()) continue;
			std::vector<PositionedToken> tokens = positionedTokens(it->second.str);
			if (tokens.empty() || tokens[0].text.size() < 2 || tokens[0].text[0] != '\\' || session.commandTable().count(tokens[0].text)) continue;
			std::string single = singleSpaces(it->second.str);
			if (single != it->second.str) {
				// the tokens are the same, closer together, so the highlighted tokens move with them
				std::vector<PositionedToken> after = positionedTokens(single);
				if (after.size() == tokens.size()) {
					std::map<int, int> newStarts;
					for (size_t k = 0; k < tokens.size(); k++) newStarts[tokens[k].start] = after[k].start;
					moveHighlights(pane, l, newStarts);
				}
				it->second.str = single;
				changed = true;
			}
		}
	}
	if (changed) editor.setFileEdited(true);
	editor.afterChange(true);
}
