#ifndef TOKENS_H
#define TOKENS_H

#include <string>
#include <vector>
#include <cctype>

// a token of a line of the editor and where it starts
// for finding the tokens of the notes of a bar (see storeEditorNoteSpans()), and for aligning bars (see alignment.cpp)
struct PositionedToken
{
	std::string text;
	int start;
};

// splits a line into tokens, as the parser does: at white space and at the brackets of repetitions ([ and ]), tuplets ({ and })
// and bars (|), but not inside chords (<...>) or quoted text, and up to a comment (%)
inline std::vector<PositionedToken> positionedTokens(const std::string& str)
{
	std::vector<PositionedToken> tokens;
	size_t i = 0;
	while (i < str.size()) {
		while (i < str.size() && isspace((unsigned char)str[i])) i++;
		if (i >= str.size() || str[i] == '%') break;
		char c = str[i];
		if (c == '[' || c == ']' || c == '{' || c == '}' || c == '|') {
			tokens.push_back({std::string(1, c), (int)i});
			i++;
			continue;
		}
		size_t start = i;
		bool insideQuotes = false;
		int chordDepth = 0;
		while (i < str.size()) {
			c = str[i];
			if (c == '"') {
				insideQuotes = !insideQuotes;
			}
			else if (!insideQuotes) {
				// a '<' after a backslash is a crescendo, and a '>' after a backslash or a hyphen a diminuendo or an accent
				bool notChordBracket = (i > 0 && (str[i-1] == '\\' || (c == '>' && str[i-1] == '-')));
				if (c == '<' && !notChordBracket) chordDepth++;
				else if (c == '>' && chordDepth > 0 && !notChordBracket) chordDepth--;
				else if (chordDepth == 0 && (isspace((unsigned char)c) || c == '[' || c == ']' || c == '{' || c == '}' || c == '|')) break;
			}
			i++;
		}
		tokens.push_back({str.substr(start, i - start), (int)start});
	}
	return tokens;
}

// a step of the x - notation of rhythm staffs (e.g. x - x x-x-): a hit (x, or X for an accented hit) or a rest (-), each one step of \score.seqres
// a hit can be followed by what follows a note: articulations (e.g. x->, x-.), a dynamic (e.g. x\f), text (e.g. x^"text"), a tie, or a slur
// (a - followed by an articulation symbol is an articulation, and any other - is a rest, so tenuto (--) can't be written on an x)
struct Step
{
	bool hit;
	std::string suffix;
	// where the step starts in its token, and how many characters it has (e.g. 2 for x- in x-x-, and 3 for x\f)
	size_t start = 0;
	size_t length = 1;
};

// the steps of a token written in the x - notation, which can be one step (x or -) or more steps joined (e.g. x-x-)
// or none, if the token is not written in it
inline std::vector<Step> stepTokens(const std::string& token)
{
	std::vector<Step> steps;
	if (token.empty() || (token[0] != 'x' && token[0] != 'X' && token[0] != '-')) return steps;
	static const std::string articulationSymbols = "^+!>._";
	size_t i = 0;
	while (i < token.size()) {
		char c = token[i];
		if (c == '-') {
			steps.push_back({false, "", i, 1});
			i++;
			continue;
		}
		if (c != 'x' && c != 'X') return std::vector<Step>();
		Step step = {true, (c == 'X' ? "->" : ""), i, 1};
		i++;
		while (i < token.size()) {
			if (token[i] == '-' && i + 1 < token.size() && articulationSymbols.find(token[i+1]) != std::string::npos) {
				step.suffix += token.substr(i, 2);
				i += 2;
			}
			else if (token[i] == '\\') {
				// a dynamic (e.g. \f or \<), up to the next step
				size_t end = i + 1;
				while (end < token.size() && token[end] != 'x' && token[end] != 'X' && token[end] != '-') end++;
				step.suffix += token.substr(i, end - i);
				i = end;
			}
			else if ((token[i] == '^' || token[i] == '_') && i + 1 < token.size() && token[i+1] == '"') {
				size_t close = token.find('"', i + 2);
				if (close == std::string::npos) return std::vector<Step>();
				step.suffix += token.substr(i, close - i + 1);
				i = close + 1;
			}
			else if (token[i] == '~' || token[i] == '(' || token[i] == ')') {
				step.suffix += token[i];
				i++;
			}
			else {
				break;
			}
		}
		step.length = i - step.start;
		steps.push_back(step);
	}
	return steps;
}

// the indexes of the notes of a melodic line in the order they play, with the repetitions of a token (e.g. c4*3) or of tokens
// in curly brackets expanded, from ndx on, up to a closing bracket if stopAtClosingBracket is true (in parser.cpp)
void expandRepetitions(const std::vector<PositionedToken>& tokens, size_t& ndx, bool stopAtClosingBracket, std::vector<int>& out);

#endif
