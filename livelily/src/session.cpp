// the engine functions of Session (see session.h) that are not the parser (parser.cpp): helpers of the commands
// storing bars, loops, lists and instruments, sending to score parts, pyo and audio
#include "session.h"
#include "clock.h"
#include <sstream>
#include <algorithm>
#include <cmath>

//--------------------------------------------------------------
std::string Session::tieWarning(int loopNdx)
{
	auto loopIt = sharedData.loopData.find(loopNdx);
	if (loopIt == sharedData.loopData.end() || loopIt->second.empty()) return "";
	const std::vector<int>& loop = loopIt->second;
	std::vector<std::string> ignoredTies;
	// the last bar of a loop is followed by its first bar, and a bar that plays alone by itself
	for (size_t i = 0; i < loop.size(); i++) {
		int fromBar = loop[i];
		int toBar = loop[(i + 1) % loop.size()];
		for (auto& inst : sharedData.instruments) {
			if (tieAcrossBars(inst.second, fromBar, toBar) == 2) {
				std::string barName = (sharedData.barsOrdered.find(fromBar) != sharedData.barsOrdered.end() ? sharedData.barsOrdered[fromBar] : std::to_string(fromBar));
				std::string tie = "\\" + inst.second.getName() + " in " + barName;
				if (std::find(ignoredTies.begin(), ignoredTies.end(), tie) == ignoredTies.end()) ignoredTies.push_back(tie);
			}
		}
	}
	if (ignoredTies.empty()) return "";
	std::string warning = "the tie at the end of ";
	for (size_t i = 0; i < ignoredTies.size(); i++) {
		if (i > 0) warning += (i == ignoredTies.size() - 1 ? " and " : ", ");
		warning += ignoredTies[i];
	}
	warning += " will be ignored, as the next note is not the same";
	return warning;
}

//--------------------------------------------------------------
void Session::applyMutesNow()
{
	// while the sequencer plays, they apply at the start of the loop (see Sequencer::checkMute())
	if (sequencer.isThreadRunning()) return;
	for (auto& inst : sharedData.instruments) {
		if (inst.second.isToBeMuted()) inst.second.setMute(true);
		else if (inst.second.isToBeUnmuted()) inst.second.setMute(false);
	}
}

//--------------------------------------------------------------
std::pair<std::pair<std::string, std::string>, std::pair<int, double>> Session::getArithmeticResult(std::string strOperand1, std::string strOperand2, int operationType)
{
	// operationType is 0 for multiplication, 1 for division, 2 for addition, and 3 for subtraction
	// the result is a pair of an error and a string result (for adding strings), and a pair of the result type (0 for int, 1 for float) and the result
	// integers are calculated as integers, so that they stay exact, and anything with a float is calculated as a float
	// isFloat() is true for integers too, as they are floats without a decimal point
	bool numeric1 = isFloat(strOperand1);
	bool numeric2 = isFloat(strOperand2);
	if (!numeric1 || !numeric2) {
		// only addition can handle strings, by joining them
		if (operationType == 2) {
			return std::make_pair(std::make_pair(std::string(""), strOperand1 + strOperand2), std::make_pair(0, 0.0));
		}
		return std::make_pair(std::make_pair(std::string("only addition is possible with non numbers"), std::string("")), std::make_pair(0, 0.0));
	}
	bool integers = (isNumber(strOperand1) && isNumber(strOperand2));
	if (integers) {
		long long operand1 = std::stoll(strOperand1);
		long long operand2 = std::stoll(strOperand2);
		switch (operationType) {
			case 0:
				return std::make_pair(std::make_pair(std::string(""), std::string("")), std::make_pair(0, (double)(operand1 * operand2)));
			case 1:
				if (operand2 == 0) {
					return std::make_pair(std::make_pair(std::string("division by zero"), std::string("")), std::make_pair(0, 0.0));
				}
				// an integer division that doesn't divide exactly gives a float (e.g. 7 / 2 is 3.5, not 3)
				if (operand1 % operand2 == 0) {
					return std::make_pair(std::make_pair(std::string(""), std::string("")), std::make_pair(0, (double)(operand1 / operand2)));
				}
				return std::make_pair(std::make_pair(std::string(""), std::string("")), std::make_pair(1, (double)operand1 / (double)operand2));
			case 2:
				return std::make_pair(std::make_pair(std::string(""), std::string("")), std::make_pair(0, (double)(operand1 + operand2)));
			default:
				return std::make_pair(std::make_pair(std::string(""), std::string("")), std::make_pair(0, (double)(operand1 - operand2)));
		}
	}
	double operand1 = std::stod(strOperand1);
	double operand2 = std::stod(strOperand2);
	double result = 0;
	switch (operationType) {
		case 0:
			result = operand1 * operand2;
			break;
		case 1:
			if (operand2 == 0) {
				return std::make_pair(std::make_pair(std::string("division by zero"), std::string("")), std::make_pair(1, 0.0));
			}
			result = operand1 / operand2;
			break;
		case 2:
			result = operand1 + operand2;
			break;
		default:
			result = operand1 - operand2;
			break;
	}
	return std::make_pair(std::make_pair(std::string(""), std::string("")), std::make_pair(1, result));
}

//--------------------------------------------------------------
std::string Session::formatNumber(double value, bool isFloatValue)
{
	// integers are written as they are, and floats with up to 15 significant digits and without trailing zeros
	// so that e.g. 3.14 + 2 is 5.14, and not 5.140000 or 5.1400000000000006
	if (!isFloatValue) return std::to_string(std::llround(value));
	std::ostringstream stream;
	stream << std::setprecision(15) << value;
	std::string str = stream.str();
	// very large or small floats are written with an exponent (e.g. 1e+20), which LiveLily doesn't read as a number
	if (str.find('e') != std::string::npos || str.find("inf") != std::string::npos || str.find("nan") != std::string::npos) {
		std::ostringstream fixedStream;
		fixedStream << std::fixed << std::setprecision(15) << value;
		str = fixedStream.str();
		if (str.find('.') != std::string::npos) {
			str.erase(str.find_last_not_of('0') + 1);
			if (str.back() == '.') str.pop_back();
		}
	}
	return str;
}

//--------------------------------------------------------------
std::string Session::stripValFromResult(std::pair<std::pair<std::string, std::string>, std::pair<int, double>> p)
{
	// the string result of adding strings, or the number result
	if (!p.first.second.empty()) return p.first.second;
	return formatNumber(p.second.second, p.second.first == 1);
}

//--------------------------------------------------------------
void Session::clearBars()
{
	for (auto it = sharedData.barsIndexes.begin(); it != sharedData.barsIndexes.end(); ++it) {
		{ auto keywordIt = std::find(keywords.begin(), keywords.end(), it->first); if (keywordIt != keywords.end()) keywords.erase(keywordIt); }
		commandsMap[livelily][it->first[1]].erase(it->first);
	}
	sharedData.barsIndexes.clear();
	sharedData.barsOrdered.clear();
	sharedData.barLines.clear();
	sharedData.distBetweenBeats.clear();
}

//--------------------------------------------------------------
void Session::clearLoops()
{
	for (auto it = sharedData.loopsIndexes.begin(); it != sharedData.loopsIndexes.end(); ++it) {
		{ auto keywordIt = std::find(keywords.begin(), keywords.end(), it->first); if (keywordIt != keywords.end()) keywords.erase(keywordIt); }
		commandsMap[livelily][it->first[1]].erase(it->first);
	}
	sharedData.loopData.clear();
	sharedData.loopsIndexes.clear();
	sharedData.loopsOrdered.clear();
	sharedData.loopsVariants.clear();
	// the place where playback stopped belongs to the loops that are cleared, so the loops defined after them play from their start
	sharedData.loopIndex = sharedData.tempLoopIndex = 0;
	sharedData.thisBarIndex = 0;
}

//--------------------------------------------------------------
void Session::clearLists()
{
	for (auto it = listIndexes.begin(); it != listIndexes.end(); ++it) {
		{ auto keywordIt = std::find(keywords.begin(), keywords.end(), it->first); if (keywordIt != keywords.end()) keywords.erase(keywordIt); }
		commandsMap[livelily][it->first[1]].erase(it->first);
	}
	listIndexes.clear();
	listMap.clear();
}

//--------------------------------------------------------------
void Session::clearFunctions()
{
	for (auto it = functionIndexes.begin(); it != functionIndexes.end(); ++it) {
		{ auto keywordIt = std::find(keywords.begin(), keywords.end(), it->first); if (keywordIt != keywords.end()) keywords.erase(keywordIt); }
		commandsMap[livelily][it->first[1]].erase(it->first);
	}
	{
		std::lock_guard<std::mutex> lock(sharedData.functionsMutex);
		functionIndexes.clear();
		sharedData.functions.clear();
		lastFunctionIndex = 0;
		storingFunctionIndex = 0;
	}
	storingFunction = false;
}

//--------------------------------------------------------------
void Session::clearInstruments()
{
	// the instruments merged to one staff go with the instruments
	mergeGroups.clear();
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		std::string s = it->second.getName();
		{ auto keywordIt = std::find(keywords.begin(), keywords.end(), "\\"+s); if (keywordIt != keywords.end()) keywords.erase(keywordIt); }
		commandsMap[livelily][s[0]].erase("\\"+s);
	}
	sequencer.clearDelayedVisuals();
	sharedData.instruments.clear();
	sharedData.instrumentIndexes.clear();
	sharedData.instrumentIndexesOrdered.clear();
	sharedData.numInstruments = 0;
	sharedData.grouppedOSCClients.clear();
	instrumentOSCHostPorts.clear();
	instGroups.clear();
	// what the front end shows of the instruments (e.g. the highlights of their notes in the editor)
	frontend->instrumentsCleared();
}

//--------------------------------------------------------------
int Session::msToBPM(unsigned long ms)
{
	return (int)((1000 / ms) * 60);
}

//--------------------------------------------------------------
bool Session::isQuoted(const std::string& s)
{
	return s.size() >= 2 && s.front() == '"' && s.back() == '"';
}

//--------------------------------------------------------------
void Session::sendToParts(OscMessage m, bool delay)
{
	for (auto it = sharedData.grouppedOSCClients.begin(); it != sharedData.grouppedOSCClients.end(); ++it) {
		if (sharedData.instruments[*it].hasDelay() && delay) {
			std::pair<OscMessage, unsigned long> p = std::make_pair(m, Clock::millis());
			sharedData.instruments[*it].oscFifo.push_back(p);
		}
		else {
			sharedData.instruments[*it].scorePartSender->send(m);
		}
	}
}

//--------------------------------------------------------------
void Session::sendBarToParts(int barIndex)
{
	OscMessage m;
	m.setAddress("/bar");
	m.addStringArg(sharedData.barsOrdered[barIndex]);
	sendToParts(m, false);
	m.clear();
	for (auto inst = sharedData.instruments.begin(); inst != sharedData.instruments.end(); ++inst) {
		if (inst->second.sendToPart) {
			m.setAddress("/line");
			m.addStringArg(inst->second.barLines[barIndex]);
			inst->second.scorePartSender->send(m);
			m.clear();
		}
	}
	m.setAddress("/bar");
	m.addIntArg(0); // add an int argument to separate it from any possible bar name
	sendToParts(m, false);
}

//--------------------------------------------------------------
void Session::sendLoopToParts()
{
	int lastLoopNdx = getLastLoopIndex();
	OscMessage m;
	m.setAddress("/loop");
	m.addStringArg(sharedData.loopsOrdered[lastLoopNdx]);
	m.addIntArg(lastLoopNdx);
	for (unsigned i = 0; i < sharedData.loopData[lastLoopNdx].size(); i++) {
		m.addIntArg(sharedData.loopData[lastLoopNdx].at(i));
	}
	sendToParts(m, false);
}

//--------------------------------------------------------------
void Session::sendSizeToPart(int instNdx, int size)
{
	OscMessage m;
	m.setAddress("/size");
	m.addIntArg(size);
	sharedData.instruments[instNdx].scorePartSender->send(m);
}

//--------------------------------------------------------------
void Session::sendNumBarsToPart(int instNdx, int numBars)
{
	OscMessage m;
	m.setAddress("/numbars");
	m.addIntArg(numBars);
	sharedData.instruments[instNdx].scorePartSender->send(m);
}

//--------------------------------------------------------------
void Session::sendAccOffsetToPart(int instNdx, float accOffset)
{
	OscMessage m;
	m.setAddress("/accoffset");
	m.addFloatArg(accOffset);
	sharedData.instruments[instNdx].scorePartSender->send(m);
}

//--------------------------------------------------------------
void Session::sendLoopIndexToParts()
{
	OscMessage m;
	m.setAddress("/loopndx");
	m.addIntArg(sharedData.loopIndex);
	sendToParts(m, false);
	m.clear();
}

//--------------------------------------------------------------
void Session::sendScoreChangeToPart(int instNdx, bool scoreChange)
{
	OscMessage m;
	m.setAddress("/scorechange");
	m.addBoolArg(scoreChange);
	sharedData.instruments[instNdx].scorePartSender->send(m);
	m.clear();
}

//--------------------------------------------------------------
void Session::sendChangeBeatColorToPart(int instNdx, bool changeBeatColor)
{
	OscMessage m;
	m.setAddress("/beatcolor");
	m.addBoolArg(changeBeatColor);
	sharedData.instruments[instNdx].scorePartSender->send(m);
	m.clear();
}

//--------------------------------------------------------------
void Session::sendFullscreenToPart(int instNdx, bool fullscreen)
{
	OscMessage m;
	m.setAddress("/fullscreen");
	m.addBoolArg(fullscreen);
	sharedData.instruments[instNdx].scorePartSender->send(m);
	m.clear();
}

//--------------------------------------------------------------
void Session::sendCursorToPart(int instNdx, bool cursor)
{
	OscMessage m;
	m.setAddress("/cursor");
	m.addBoolArg(cursor);
	sharedData.instruments[instNdx].scorePartSender->send(m);
	m.clear();
}

//--------------------------------------------------------------
void Session::printVector(std::vector<int> v)
{
	for (int elem : v) {
		cout << elem << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
void Session::printVector(std::vector<std::string> v)
{
	for (std::string elem : v) {
		cout << elem << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
void Session::printVector(std::vector<float> v)
{
	for (float elem : v) {
		cout << elem << " ";
	}
	cout << endl;
}

//--------------------------------------------------------------
bool Session::isNumber(std::string str)
{
	if (str.empty()) return false;
	// first check if there is a hyphen in the beginning
	int loopStart = 0;
	if (str[0] == '-') loopStart = 1;
	// a hyphen alone is not a number
	if (loopStart >= (int)str.length()) return false;
	for (int i = loopStart; i < (int)str.length(); i++) {
		if (!isdigit(str[i])) {
			return false;
		}
	}
	return true;
}

//--------------------------------------------------------------
bool Session::isFloat(std::string str)
{
	if (str.empty()) return false;
	// first check if there is a hyphen in the beginning
	int loopStart = 0;
	int dotCounter = 0;
	if (str[0] == '-') loopStart = 1;
	for (int i = loopStart; i < (int)str.length(); i++) {
		if (!isdigit(str[i])) {
			if (str[i] == '.') {
				if (dotCounter == 0) dotCounter++;
				else return false;
			}
			else return false;
		}
	}
	// a hyphen or a dot alone, or both, is not a number
	bool hasDigit = false;
	for (char c : str) {
		if (isdigit(c)) hasDigit = true;
	}
	return hasDigit;
}

//-------------------------------------------------------------
std::vector<int> Session::findRepetitionInt(std::string str, int multIndex)
{
	std::vector<int> numRepNumDigits(2);
	int index = findNextStrCharIdx(str, " ", multIndex);
	int numDigits = index - multIndex - 1;
	if (isNumber(str.substr(multIndex+1, numDigits))) {
		numRepNumDigits[0] = stoi(str.substr(multIndex+1, numDigits));
		numRepNumDigits[1] = numDigits;
	}
	return numRepNumDigits;
}

//--------------------------------------------------------------
int Session::findNextStrCharIdx(std::string str, std::string compareStr, int index)
{
	while (index < (int)str.size()) {
		if (str.substr(index, 1).compare(compareStr) == 0) {
			break;
		}
		index++;
	}
	return index;
}

//---------------------------------------------------------------
std::vector<int> Session::findIndexesOfCharInStr(std::string str, std::string charToFind)
{
	size_t pos = str.find(charToFind, 0);
	std::vector<int> tokensIndexes;
	while (pos != std::string::npos) {
		tokensIndexes.push_back((int)pos);
		pos = str.find(charToFind, pos+1);
	}
	return tokensIndexes;
}

//--------------------------------------------------------------
int Session::findMatchingBrace(const std::string& s, size_t openPos)
{
	int depth = 0;
	for (size_t i = openPos; i < s.length(); ++i) {
		if (s[i] == '{') ++depth;
		else if (s[i] == '}') {
			--depth;
			if (depth == 0) return i;
		}
	}
	return -1;
}

//--------------------------------------------------------------
std::string Session::genStrFromVec(const std::vector<std::string>& vec)
{
	// the items are joined with a space, but not where an item ends or the next one starts with a line break or a tab
	// as the tokens of a definition in many lines (e.g. the lines of a bar, see tokenizeLine()) keep these
	// e.g. "c'4\n" and "}" are joined to "c'4\n}", and not "c'4\n }"
	auto isLineSpace = [](char c) { return c == '\n' || c == '\t'; };
	std::string result;
	for (size_t i = 0; i < vec.size(); i++) {
		if (i > 0 && !(result.size() > 0 && isLineSpace(result.back())) && !(vec[i].size() > 0 && isLineSpace(vec[i].front()))) {
			result += ' ';
		}
		result += vec[i];
	}
	return result;
}

//---------------------------------------------------------------
std::vector<std::string> Session::tokenizeString(std::string str, std::string delimiter, bool addDelimiter)
{
	size_t prev = 0, pos;
	std::vector<std::string> tokens;
	while ((pos = str.find_first_of(delimiter, prev)) != std::string::npos)
	{
		if (pos > prev) {
			tokens.push_back(str.substr(prev, pos-prev));
		}
		if (addDelimiter) tokens.push_back(str.substr(pos, 1));
		prev = pos+1;
	}
	if (prev < str.length()) {
		tokens.push_back(str.substr(prev, std::string::npos));
	}
	return tokens;
}

//---------------------------------------------------------------
std::map<size_t, std::string> Session::tokenizeStringWithNdxs(std::string str, std::string delimiter)
{
	size_t start = 0;
	size_t end = str.find(delimiter);
	std::map<size_t, std::string> m;
	std::vector<std::string> tokens;
	std::vector<size_t> ndxs;
	while (end != std::string::npos) {
		m[start] = str.substr(start,end);
		start += end + 1;
		end = str.substr(start).find(delimiter);
	}
	// the last token is not extracted in the loop above because end has reached std::string::npos
	// so we extract it here by simply passing a substd::string from the last start point to the end
	m[start] = str.substr(start);
	return m;
}

//--------------------------------------------------------------
std::vector<std::string> Session::tokenizeChord(std::string input, bool includeAngleBrackets)
{
	bool isString = false;
	int quotesCounter = 0;
	std::vector<std::string> tokens;
	std::string token;
	for (size_t i = 0; i < input.size(); i++) {
		if (isspace(input[i])) {
			if (!token.empty() && !isString) {
				tokens.push_back(token);
				token.clear();
			}
			else {
				token += input[i];
			}
		}
		// we continue when we run into the chord opening and closing characters
		// so that they are not concatenated to the token
		// a '<' after a backslash is a crescendo (\<), not a chord opening, even if a dynamic follows it (e.g. \<\mp)
		else if (input[i] == '<' && (i == 0 || input[i-1] != '\\')) {
			if (includeAngleBrackets) token += input[i];
			continue;
		}
		else if (input[i] == '>' && (i > 0 && input[i-1] != '\\' && input[i-1] != '-')) {
			if (includeAngleBrackets) token += input[i];
			continue;
		}
		else {
			token += input[i];
			if (input[i] == '"') {
				quotesCounter++;
				if (quotesCounter == 1) {
					isString = true;
				}
				else {
					isString = false;
					quotesCounter = 0;
				}
			}
		}
	}
	if (!token.empty()) tokens.push_back(token);
	return tokens;
}

//--------------------------------------------------------------
// the following function and its overloaded version is inspired by
// https://www.geeksforgeeks.org/check-for-balanced-parentheses-in-an-expression/
bool Session::areBracketsBalanced(std::string str)
{
	// Declare a stack to hold the previous brackets.
	size_t i;
	std::stack<char> temp;
	for (i = 0; i < str.length(); i++) {
		if (str[i] == '{' || str[i] == '[' || str[i] == '(' || str[i] == '<') {
			if (str[i] == '<') {
				if (i == 0 || (i > 0 && str[i-1] != '\\')) {
					temp.push(str[i]);
				}
			}
			else {
				temp.push(str[i]);
			}
		}
		else if (str[i] == '}' || str[i] == ']' || str[i] == ')' || str[i] == '>') {
			if (str[i] == '>') {
				if ((i == 0 || (i > 0 && (str[i-1] != '\\' && str[i-1] != '-'))) && temp.empty()) {
					return false;
				}
			}
			else if (temp.empty()) {
				return false;
			}
			if (!temp.empty() && ((temp.top() == '{' && str[i] == '}')
					|| (temp.top() == '[' && str[i] == ']')
					|| (temp.top() == '(' && str[i] == ')')
					|| (temp.top() == '<' && str[i] == '>'))) {
				if (str[i] == '>') {
					if (i > 0 && (str[i-1] != '\\' && str[i-1] != '-')) {
						temp.pop();
					}
				}
				else {
					temp.pop();
				}
			}
		}
		else if (str[i] == '%') {
			break;
		}
	}
	if (temp.empty()) {
		// If stack is empty return true
		return true;
	}
	return false;
}

//--------------------------------------------------------------
bool Session::areBracketsBalanced(std::vector<std::string> v)
{
	// Declare a stack to hold the previous brackets.
	size_t i, j;
	std::stack<std::string> strStack;
	// first check for curly brackets, as these are tokenized and are separate strings in the vector
	for (i = 0; i < v.size(); i++) {
		if (v[i] == "{") {
			strStack.push(v[i]);
		}
		else if (v[i] == "}") {
			if (strStack.empty()) {
				return false;
			}
			if (strStack.top() == "{" && v[i] == "}") {
				strStack.pop();
			}
		}
	}
	if (!strStack.empty()) {
		return false;
	}
	// a boolean to break a nested loop from the inner loop
	bool breakLoop = false;
	std::stack<char> charStack;
	// then check for square and angle brackets, as these are included in a string of a vector with other characters
	// round brackets are slurs, which can start in a bar and stop in the next one, so they don't have to be balanced in a line
	for (i = 0; i < v.size(); i++) {
		for (j = 0; j < v[i].length(); j++) {
			if (v[i][j] ==  '[' || v[i][j] == '<') {
				if (v[i][j] == '<') {
					if (j == 0 || (j > 0 && v[i][j-1] != '\\')) {
						charStack.push(v[i][j]);
					}
				}
				else {
					charStack.push(v[i][j]);
				}
			}
			else if (v[i][j] == ']' || v[i][j] == '>') {
				if (v[i][j] == '>') {
					if ((j == 0 || (j > 0 && (v[i][j-1] != '\\' && v[i][j-1] != '-'))) && charStack.empty()) {
						return false;
					}
				}
				else if (charStack.empty()) {
					return false;
				}
				if (!charStack.empty() && ((charStack.top() == '[' && v[i][j] == ']')
						|| (charStack.top() == '<' && v[i][j] == '>'))) {
					if (v[i][j] == '>') {
						if (j > 0 && (v[i][j-1] != '\\' && v[i][j-1] != '-')) {
							charStack.pop();
						}
					}
					else {
						charStack.pop();
					}
				}
			}
			else if (v[i][j] == '%') {
				breakLoop = true;
				break;
			}
		}
		if (breakLoop) break;
	}
	if (charStack.empty()) {
		// If stack is empty return true
		return true;
	}
	return false;
}

//--------------------------------------------------------------
CmdOutput Session::traverseList(int numLines)
{
	// executes the lines of \list.traverse (see isList() and parseString()) once for every item of the list, in turn
	// with every $ replaced by the item, and stops at the first error, which is shown at the line it occurs
	CmdOutput cmdOutput = CmdOutput();
	// the lines are copied, as executing them might start another \list.traverse
	std::vector<std::pair<int, std::string>> lines = traverseLines;
	std::list<std::string> items = listMap[lastListIndex];
	traverseLines.clear();
	// every line shows what it outputs (e.g. \print), for the last item, or the first error
	for (const std::pair<int, std::string>& line : lines) releaseEditorTraceback(line.first);
	for (const std::string& item : items) {
		for (const std::pair<int, std::string>& line : lines) {
			std::string strToExecute = replaceCharInStr(line.second, "$", item);
			// as in parseString(), the command at the start of the line gets the whole line, and evaluates its arguments when it runs
			std::vector<std::string> tokens = tokenizeLine(strToExecute);
			if (tokens.empty()) continue;
			CmdInput cmdInput = {tokens, std::vector<std::string>(), false, true};
			cmdOutput = parseCommand(cmdInput, line.first, numLines);
			if (cmdOutput.errorCode == 3) {
				setEditorTraceback(cmdOutput.errorCode, cmdOutput.errorStr, line.first);
				return cmdOutput;
			}
			if (cmdOutput.errorCode > 0) setEditorTraceback(cmdOutput.errorCode, cmdOutput.errorStr, line.first);
		}
	}
	return cmdOutput;
}

//--------------------------------------------------------------
void Session::storeList(std::string str)
{
	std::vector<std::string> items = tokenizeString(str, " ");
	for (std::string item : items) {
		if (item.compare("{") == 0) continue;
		if (item.compare("}") == 0) continue;
		if (startsWith(item, "{")) listMap[lastListIndex].push_back(item.substr(1));
		else if (endsWith(item, "}")) listMap[lastListIndex].push_back(item.substr(0, item.size()-1));
		else listMap[lastListIndex].push_back(item);
	}
}

//--------------------------------------------------------------
int Session::storeNewBar(std::string barName)
{
	int barIndex = getLastLoopIndex() + 1;
	if (sharedData.numerator.find(barIndex) == sharedData.numerator.end()) {
		sharedData.numerator[barIndex] = 4;
		sharedData.denominator[barIndex] = 4;
	}
	sharedData.barsIndexes[barName] = barIndex;
	sharedData.loopsIndexes[barName] = barIndex;
	sharedData.loopsOrdered[barIndex] = barName;
	sharedData.barsOrdered[barIndex] = barName;
	sharedData.loopsVariants[barIndex] = 0;
	sharedData.tempLoopIndex = barIndex;
	int prevBarIndex = getPrevBarIndex();
	if (prevBarIndex < 0) {
		sharedData.numerator[barIndex] = 4;
		sharedData.denominator[barIndex] = 4;
		sharedData.numBeats[barIndex] = sharedData.numerator[barIndex] * (MINDUR / sharedData.denominator[barIndex]);
		sharedData.tempoMs[barIndex] = 500;
		sharedData.BPMTempi[barIndex] = 120;
		sharedData.BPMMultiplier[barIndex] = 1;
		sharedData.beatAtDifferentThanDivisor[barIndex] = false;
		sharedData.beatAtValues[barIndex] = {1};
		sharedData.tempoBaseForScore[barIndex] = 4;
		sharedData.BPMDisplayHasDot[barIndex] = false;
		sharedData.tempo[barIndex] = sharedData.tempoMs[barIndex] / (MINDUR / sharedData.denominator[barIndex]);
		sharedData.PPQNPerUs[barIndex] = (uint64_t)(sharedData.tempoMs[barIndex] / (float)sharedData.PPQN) * 1000;
	}
	else {
		sharedData.numerator[barIndex] = sharedData.numerator[prevBarIndex];
		sharedData.denominator[barIndex] = sharedData.denominator[prevBarIndex];
		sharedData.numBeats[barIndex] = sharedData.numBeats[prevBarIndex];
		sharedData.tempoMs[barIndex] = sharedData.tempoMs[prevBarIndex];
		sharedData.BPMTempi[barIndex] = sharedData.BPMTempi[prevBarIndex];
		sharedData.BPMMultiplier[barIndex] = sharedData.BPMMultiplier[prevBarIndex];
		sharedData.beatAtDifferentThanDivisor[barIndex] = sharedData.beatAtDifferentThanDivisor[prevBarIndex];
		sharedData.beatAtValues[barIndex] = sharedData.beatAtValues[prevBarIndex];
		sharedData.tempoBaseForScore[barIndex] = sharedData.tempoBaseForScore[prevBarIndex];
		sharedData.BPMDisplayHasDot[barIndex] = sharedData.BPMDisplayHasDot[prevBarIndex];
		sharedData.tempo[barIndex] = sharedData.tempo[prevBarIndex];
		sharedData.PPQNPerUs[barIndex] = sharedData.PPQNPerUs[prevBarIndex];
	}
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		it->second.setMeter(barIndex, sharedData.numerator[barIndex],
				sharedData.denominator[barIndex], sharedData.numBeats[barIndex]);
	}
	keywords.push_back(barName);
	// store the bar name
	// the rest of the bar data will be stored in parseMelodicLine()
	// sharedData.barLines is a map of string and string, so the key is the bar name
	sharedData.barLines[barName] = "\\bar " + barName.substr(1) + " {\n";
	return barIndex;
}

//--------------------------------------------------------------
void Session::storeNewLoop(std::string loopName)
{
	int loopIndex = getLastLoopIndex() + 1;
	sharedData.loopsIndexes[loopName] = loopIndex;
	sharedData.loopsOrdered[loopIndex] = loopName;
	sharedData.loopsVariants[loopIndex] = 0;
	sharedData.tempLoopIndex = loopIndex;
	partsReceivedOKCounters[loopIndex] = 0;
	keywords.push_back(loopName);
}

//--------------------------------------------------------------
void Session::deleteLastBar()
{
	//if (barError) return; // if a bar error has been raised, nothing has been stored
	int bar = getLastBarIndex();
	for (std::map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		if (it->second.hasPassed() && it->second.notes.find(bar) != it->second.notes.end()) {
			it->second.notes.erase(bar);
			it->second.midiNotes.erase(bar);
			it->second.durs.erase(bar);
			it->second.dursWithoutSlurs.erase(bar);
			it->second.midiDursWithoutSlurs.erase(bar);
			it->second.pitchBendVals.erase(bar);
			it->second.dynamics.erase(bar);
			it->second.midiVels.erase(bar);
			it->second.dynamicsRamps.erase(bar);
			it->second.glissandi.erase(bar);
			it->second.midiGlissDurs.erase(bar);
			it->second.midiDynamicsRampDurs.erase(bar);
			it->second.articulations.erase(bar);
			it->second.midiArticulationVals.erase(bar);
			it->second.isSlurred.erase(bar);
			it->second.text.erase(bar);
			it->second.textIndexes.erase(bar);
			it->second.slurIndexes.erase(bar);
			it->second.isWholeBarSlurred.erase(bar);
			it->second.tieIndexes.erase(bar);
			it->second.scoreNotes.erase(bar);
			it->second.scoreDurs.erase(bar);
			it->second.scoreDotIndexes.erase(bar);
			it->second.scoreDotsCounter.erase(bar);
			it->second.scoreAccidentals.erase(bar);
			it->second.scoreNaturalSignsNotWritten.erase(bar);
			it->second.scoreOctaves.erase(bar);
			it->second.scoreOttavas.erase(bar);
			it->second.scoreGlissandi.erase(bar);
			it->second.scoreArticulations.erase(bar);
			it->second.scoreDynamics.erase(bar);
			it->second.scoreDynamicsIndexes.erase(bar);
			it->second.scoreDynamicsRampStart.erase(bar);
			it->second.scoreDynamicsRampEnd.erase(bar);
			it->second.scoreDynamicsRampDir.erase(bar);
			it->second.scoreTupletRatios.erase(bar);
			it->second.scoreTupletStartStop.erase(bar);
			it->second.scoreTexts.erase(bar);
			it->second.passed = false;
			it->second.deleteNotesBar(bar);
		}
	}
	if (sharedData.barsIndexes.find(sharedData.loopsOrdered[bar]) != sharedData.barsIndexes.end()) {
		sharedData.barsIndexes.erase(sharedData.loopsOrdered[bar]);
	}
	if (sharedData.loopsIndexes.find(sharedData.loopsOrdered[bar]) != sharedData.loopsIndexes.end()) {
		sharedData.loopsIndexes.erase(sharedData.loopsOrdered[bar]);
	}
	if (sharedData.numerator.find(bar) != sharedData.numerator.end()) {
		sharedData.numerator.erase(bar);
		sharedData.denominator.erase(bar);
	}
	if (sharedData.tempo.find(bar) != sharedData.tempo.end()) {
		sharedData.tempo.erase(bar);
	}
	if (sharedData.tempoMs.find(bar) != sharedData.tempoMs.end()) {
		sharedData.tempoMs.erase(bar);
	}
	if (sharedData.BPMTempi.find(bar) != sharedData.BPMTempi.end()) {
		sharedData.BPMTempi.erase(bar);
	}
	if (sharedData.BPMMultiplier.find(bar) != sharedData.BPMMultiplier.end()) {
		sharedData.BPMMultiplier.erase(bar);
	}
	if (sharedData.beatAtDifferentThanDivisor.find(bar) != sharedData.beatAtDifferentThanDivisor.end()) {
		sharedData.beatAtDifferentThanDivisor.erase(bar);
	}
	if (sharedData.beatAtValues.find(bar) != sharedData.beatAtValues.end()) {
		sharedData.beatAtValues.erase(bar);
	}
	commandsMap[livelily][sharedData.loopsOrdered[bar].substr(1)[0]].erase(sharedData.loopsOrdered[bar]);
	sharedData.loopsOrdered.erase(bar);
	sharedData.loopsVariants.erase(bar);
	// sharedData.barLines is a map<string, string> so we get the correct key
	// from the sharedData.barsOrdered map using the bar index as the key
	sharedData.barLines.erase(sharedData.barsOrdered[bar]);
	sharedData.tempLoopIndex = getLastLoopIndex();
	if (!sequencer.isThreadRunning()) {
		sharedData.loopIndex = sharedData.tempLoopIndex;
	}
}

//--------------------------------------------------------------
void Session::deleteLastLoop()
{
	std::map<int, std::string>::reverse_iterator it = sharedData.loopsOrdered.rbegin();
	commandsMap[livelily][it->second.substr(1)[0]].erase(it->second);
	sharedData.loopsIndexes.erase(it->second);
	sharedData.loopsOrdered.erase(it->first);
	sharedData.loopsVariants.erase(it->first);
	sharedData.tempLoopIndex = getLastLoopIndex();
	if (!sequencer.isThreadRunning()) {
		sharedData.loopIndex = sharedData.tempLoopIndex;
	}
	//sendPushPopPattern();
}

//--------------------------------------------------------------
int Session::getBaseDurValue(std::string str, int denominator)
{
	int base = 0;
	int val;
	if (str.find(".") != std::string::npos) {
		base = stoi(str.substr(0, str.find(".")));
		val = denominator / base;
		// get the number of dots, copied from
		// https://stackoverflow.com/questions/3867890/count-character-occurrences-in-a-std::string-in-c#3871346
		std::string::difference_type n = std::count(str.begin(), str.end(), '.');
		int half = val / 2;
		for (int i = 0; i < (int)n; i++) {
			val += half;
			half /= 2;
		}
	}
	else {
		base = stoi(str);
		val = denominator / base;
	}
	return val;
}

//--------------------------------------------------------------
std::pair<int, std::string> Session::getBaseDurError(std::string str)
{
	if (!isNumber(str)) {
		if (str.find(".") == std::string::npos) {
			return std::make_pair(3, str + " is not a number");
		}
		else {
			if (!isNumber(str.substr(0, str.find(".")))) {
				return std::make_pair(3, str + " is not a number");
			}
		}
	}
	if (stoi(str) == 0) {
		return std::make_pair(3, "got zero duration");
	}
	return std::make_pair(0, "");
}

//--------------------------------------------------------------
CmdOutput Session::genError(std::string str)
{
	CmdOutput error = {3, str, false, std::vector<std::string>()};
	return error;
}

//--------------------------------------------------------------
CmdOutput Session::genWarning(std::string str)
{
	CmdOutput warning = {2, str, false, std::vector<std::string>()};
	return warning;
}

//--------------------------------------------------------------
CmdOutput Session::genNote(std::string str)
{
	CmdOutput note = {1, str, false, std::vector<std::string>()};
	return note;
}

//--------------------------------------------------------------
CmdOutput Session::genOutput(std::string str, bool keepArgs)
{
	CmdOutput output = {0, "", keepArgs, std::vector<std::string>({str})};
	return output;
}

//--------------------------------------------------------------
CmdOutput Session::genOutput(std::vector<std::string> v, bool keepArgs)
{
	CmdOutput output = {0, "", keepArgs, v};
	return output;
}

//--------------------------------------------------------------
CmdOutput Session::genOutputFuncs(std::pair<int, std::string> p)
{
	switch (p.first) {
		case 0:
			return genOutput(p.second);
		case 1:
			return genNote(p.second);
		case 2:
			return genWarning(p.second);
		case 3:
			return genError(p.second);
		default:
			CmdOutput cmdOutput = CmdOutput();
			return cmdOutput;
	}
}

//--------------------------------------------------------------
CmdInput Session::genCmdInput(std::string str)
{
	std::vector<std::string> v = tokenizeString(str, " ");
	CmdInput cmdInput = {v, std::vector<std::string>(), false, true};
	return cmdInput;
}

//--------------------------------------------------------------
CmdInput Session::genCmdInput(std::vector<std::string> v)
{
	CmdInput cmdInput = {v, std::vector<std::string>(), false, true};
	return cmdInput;
}

//--------------------------------------------------------------
void Session::initPyo()
{
	if (pyoSet) return;
#ifdef USEPYO
	// initialize a pyo server
	sharedData.pyo.setup(inChannels, outChannels, bufferSize, sampleRate);
	// empty the first comment about wxPython
	sharedData.pyo.getStdout();
	// then set the debug flag to 1
	sharedData.pyo.setDebug(1);
	// the audio stream, with the input device if one is set, but without input channels, as before
	// (pyo's input, audioIn(), is not used)
	AudioSettings settings = audioSettings();
	settings.inDeviceSet = inSoundDeviceSet;
	settings.inDevice = inSoundDevice;
	// the click track might have set up a stream without input, which pyo replaces
	if (soundStreamSet) audioStream->close();
	audioStream->setup(settings, audioCallback);
	soundStreamSet = true;
#endif
	pyoSet = true;
}

//--------------------------------------------------------------
CmdOutput Session::functionFuncs(std::vector<std::string>& originalCommands)
{
	// the initial command is \functionname which might be followed by a method
	// to separate the method we create a new vector that will copy the originalCommands vector
	// except from the first item where we trim the \functionname. part
	bool hasDot = false;
	CmdOutput cmdOutput = CmdOutput();
	std::vector<std::string> commands;
	for (unsigned i = 0; i < originalCommands.size(); i++) {
		if (!i) {
			// separate the name of the function from a possible method
			std::vector<std::string> tokens = tokenizeString(originalCommands[i], ".");
			for (unsigned j = 0; j < tokens.size(); j++) {
				commands.push_back(tokens[j]);
			}
			if (tokens.size() > 1) hasDot = true;
		}
		else {
			commands.push_back(originalCommands[i]);
		}
	}
	for (unsigned i = 1; i < commands.size(); i++) {
		if (commands[i].compare("setargs") == 0) {
			if (commands.size() < i+2) {
				return genError("\"setargs\" method takes at least one argument");
			}
			if (!hasDot) {
				return genError("setargs must be concatenated to the function name with a dot");
			}
			sharedData.functions[lastFunctionIndex].resetArgumentIndex();
			for (unsigned j = i+1; j < commands.size(); j++) {
				sharedData.functions[lastFunctionIndex].setArgument(commands[j]);
				if (sharedData.functions[lastFunctionIndex].getArgError() > 0) {
					switch (sharedData.functions[lastFunctionIndex].getArgError()) {
						case 1:
							return genError("too many arguments");
						case 2:
							return genError("argument size too big");
						default:
							return genError("something went wrong");
					}
				}
			}
			return genOutput("noexec");
		}
		else if (commands[i].compare("bind") == 0) {
			if (commands.size() < i+2) {
				return genError("\"bind\" method takes at least one argument");
			}
			if (!hasDot) {
				return genError("setargs must be concatenated to the function name with a dot");
			}
			if (sharedData.instrumentIndexes.find(commands[i+1]) == sharedData.instrumentIndexes.end() && \
				!(commands[i+1].compare("beat") == 0 || commands[i+1].compare("barstart") == 0 || \
				  commands[i+1].compare("framerate") == 0 || commands[i+1].compare("fr") == 0 || \
				  commands[i+1].compare("loopstart") == 0)) {
				return genError(commands[i+1] + (std::string)" is not a defined instrument");
			}
			int instNdx = -1;
			int callingStep = 1;
			int everyOtherStep = 0;
			int numTimes = 0;
			bool callingStepSet = false;
			bool settingEvery = false;
			bool settingTimes = false;
			bool sendValWarning = false;
			if (commands.size() >= i+2) {
				if (commands[i+1].compare("beat") == 0) {
					instNdx = (int)sharedData.instruments.size();
				}
				else if (commands[i+1].compare("barstart") == 0) {
					instNdx = (int)sharedData.instruments.size() + 1;
				}
				else if (commands[i+1].compare("framerate") == 0 || commands[i+1].compare("fr") == 0) {
					instNdx = (int)sharedData.instruments.size() + 2;
				}
				else if (commands[i+1].compare("loopstart") == 0) {
					instNdx = (int)sharedData.instruments.size() + 3;
				}
				// if only a function and an instrument are defined
				// then we trigger the function at every step
				else if (commands.size() == i+2){
					instNdx = sharedData.instrumentIndexes[commands[i+1]];
					everyOtherStep = 1;
				}
			}
			if (commands.size() > i+2) {
				for (unsigned j = i+2; j < commands.size(); j++) {
					if (isNumber(commands[j])) {
						int val = stoi(commands[j]);
						if (val < 0) return genError("value can't be negative");
						if (val == 0) sendValWarning = true;
						if (settingEvery) {
							everyOtherStep = val;
							settingEvery = false;
						}
						else if (settingTimes) {
							numTimes = val;
							settingTimes = false;
						}
						else {
							callingStep = val;
							callingStepSet = true;
						}
					}
					else if (commands[j].compare("every") == 0) {
						if (commands.size() < j + 2) {
							return genError("argument \"every\" to \"bind\" takes a number after it");
						}
						if (!isNumber(commands[j+1])) {
							return genError("argument \"every\" to \"bind\" takes a number after it");
						}
						if (!callingStepSet) callingStep = 1;
						settingEvery = true;
					}
					else if (commands[j].compare("times") == 0) {
						if (commands.size() < j + 2) {
							return genError("argument \"times\" to \"bind\" takes a number after it");
						}
						if (!isNumber(commands[j+1])) {
							return genError("argument \"times\" to \"bind\" takes a number after it");
						}
						if (!callingStepSet) callingStep = 1;
						settingTimes = true;
					}
					else {
						return genError(commands[j] + ": unkonw argument");
					}
				}
			}
			sharedData.functions[lastFunctionIndex].setBind(instNdx, callingStep, everyOtherStep, numTimes);
			if (sendValWarning) {
				return genWarning("starting step must be 1 or greater, setting to 1");
			}
			else return genOutput("noexec");
		}
		else if (commands[i].compare("unbind") == 0) {
			if (commands.size() > i+1) {
				return genError("\"unbind\" method takes no arguments");
			}
			if (!hasDot) {
				return genError("setargs must be concatenated to the function name with a dot");
			}
			sharedData.functions[lastFunctionIndex].releaseBind();
			return genOutput("");
		}
		else if (commands[i].compare("onrelease") == 0) {
			if (commands.size() < i+2) {
				return genError("\"onrelease\" method takes at least one argument");
			}
			if (!hasDot) {
				return genError("setargs must be concatenated to the function name with a dot");
			}
			if (isNumber(commands[i+1])) {
				sharedData.functions[lastFunctionIndex].onUnbindFunc(stoi(commands[i+1]));
			}
			else if (commands[i+1].compare("release") == 0) {
				sharedData.functions[lastFunctionIndex].onUnbindFunc(-1);
			}
			else if (functionIndexes.find(commands[i+1]) == functionIndexes.end()) {
				// read the line and store a function with it
			}
			else {
				sharedData.functions[lastFunctionIndex].onUnbindFunc(functionIndexes[commands[i+1]]);
			}
			return genOutput("noexec");
		}
	}
	return cmdOutput;
}

//--------------------------------------------------------------
int Session::getLastLoopIndex()
{
	int loopIndex = -1;
	// since every bar is copied to the loops map, but loops are not copied to the bars map
	// we query the loopsIndexes map instead of the barIndexes map
	// so that the keys between bars and loops are the same
	if (sharedData.loopsOrdered.size() > 0) {
		std::map<int, std::string>::reverse_iterator it = sharedData.loopsOrdered.rbegin();
		loopIndex = it->first;
	}
	return loopIndex;
}

//--------------------------------------------------------------
int Session::getLastBarIndex()
{
	int barIndex = -1;
	int lastLoopIndex = getLastLoopIndex();
	// since loop indexes are not stored as bar indexes, once we get the last loop index
	// we query whether this is also a bar index, and if not, we subtract one until we find a bar index
	if (lastLoopIndex > 0) {
		while (sharedData.barsIndexes.find(sharedData.loopsOrdered[lastLoopIndex]) == sharedData.barsIndexes.end()) {
			lastLoopIndex--;
		}
		barIndex = lastLoopIndex;
	}
	return barIndex;
}

//--------------------------------------------------------------
int Session::getPrevBarIndex()
{
	int prevBarIndex = getLastBarIndex();
	prevBarIndex--;
	if (prevBarIndex > 0) {
		while (sharedData.barsIndexes.find(sharedData.loopsOrdered[prevBarIndex]) == sharedData.barsIndexes.end()) {
			prevBarIndex--;
		}
	}
	return prevBarIndex;
}

//--------------------------------------------------------------
int Session::getPlayingBarIndex()
{
	if (sharedData.loopData.find(sharedData.loopIndex) == sharedData.loopData.end()) {
		return -1;
	}
	if (sharedData.thisBarIndex >= sharedData.loopData[sharedData.loopIndex].size()) {
		return -1;
	}
	return sharedData.loopData[sharedData.loopIndex][sharedData.thisBarIndex];
}

//--------------------------------------------------------------
CmdOutput Session::stripLineFromBar(std::vector<std::string> tokens, int lineNum, int numLines)
{
	// the output of calling a bar is a vector of strings with single tokens
	// so we run through it to determine which strings we'll use to assemble a new string vector
	bool startPushing = false;
	std::vector<std::string> newTokens;
	for (auto it = tokens.begin(); it != tokens.end(); ++it) {
		size_t newlineNdx = (*it).find("\n");
		size_t tabNdx = (*it).find("\t");
		if (startPushing) {
			// don't include the newline character
			if (newlineNdx != std::string::npos) {
				if (!(*it).substr(0, newlineNdx).empty() && sharedData.instrumentIndexes.find((*it).substr(0, newlineNdx)) == sharedData.instrumentIndexes.end()) {
					newTokens.push_back((*it).substr(0, newlineNdx));
				}
				startPushing = false;
				break;
			}
			else {
				if (!(*it).empty() && sharedData.instrumentIndexes.find(*it) == sharedData.instrumentIndexes.end()) {
					newTokens.push_back(*it);
				}
			}
		}
		if (tabNdx != std::string::npos && (*it).substr(tabNdx+1) == "\\" + sharedData.instruments[lastInstrumentIndex].getName()) {
			newTokens.push_back((*it).substr(tabNdx+1));
			startPushing = true;
		}
	}
	return evaluateTokens(newTokens, lineNum, numLines);
}

std::vector<std::string> Session::detectRepetitions(std::vector<std::string> tokens)
{
	// join the tokens back into one expression.
	std::string expression;

	for (size_t i = 0; i < tokens.size(); ++i) {
		if (i > 0) expression += " ";
		expression += tokens[i];
	}
	size_t pos = 0;
	// skip whitespace
	auto skipWhitespace = [&]() {
		while (pos < expression.size() &&
			   std::isspace(static_cast<unsigned char>(expression[pos]))) {
			++pos;
		}
	};
	// parse an integer coefficient following '*'.
	auto parseMultiplier = [&]() -> int {
		skipWhitespace();
		if (pos >= expression.size() || expression[pos] != '*') return 1;
		++pos;
		skipWhitespace();
		size_t start = pos;
		while (pos < expression.size() &&
			   std::isdigit(static_cast<unsigned char>(expression[pos]))) {
			++pos;
		}
		if (start == pos) return 1;
		return std::stoi(expression.substr(start, pos - start));
	};
	// parse a sequence until ']' (or until the end).
	std::function<std::vector<std::string>(bool)> parseSequence;
	// Parse one item: [ ... ]*N or token* N
	std::function<std::vector<std::string>()> parseItem;

	parseSequence = [&](bool stopAtClosingBracket)
			-> std::vector<std::string> {
		std::vector<std::string> result;
		while (pos < expression.size()) {
			skipWhitespace();
			if (pos >= expression.size()) break;
			if (expression[pos] == ']') {
				if (stopAtClosingBracket) break;
				// unmatched closing bracket.
				++pos;
				continue;
			}
			std::vector<std::string> item = parseItem();
			result.insert(
				result.end(),
				item.begin(),
				item.end()
			);
		}
		return result;
	};

	parseItem = [&]() -> std::vector<std::string> {
		std::vector<std::string> item;
		skipWhitespace();
		if (pos >= expression.size()) return item;
		// bracketed group
		if (expression[pos] == '[') {
			++pos; // move pointer on opening square bracket
			// recursively parse everything inside the brackets.
			std::vector<std::string> contents = parseSequence(true);
			// move pointer on closing square bracket
			if (pos < expression.size() && expression[pos] == ']') ++pos;
			// read the multiplier immediately following ']'.
			int repetitions = parseMultiplier();
			for (int i = 0; i < repetitions; ++i) {
				item.insert(
					item.end(),
					contents.begin(),
					contents.end()
				);
			}
			return item;
		}
		// regular token or chord
		size_t start = pos;
		std::string token;
		//if (expression[pos] == '<') {
		//	// a chord: keep everything from '<' through '>'
		//	// as one token, including internal whitespace.
		//	++pos;
		//	while (pos < expression.size() && expression[pos] != '>') {
		//		++pos;
		//	}
		//	if (pos < expression.size()) {
		//		++pos; // move pointer on closing angle bracket
		//		// include the duration and any following suffix,
		//		// e.g. <c' e'>2 or <c' e'>4.
		//		while (pos < expression.size() &&
		//				!std::isspace(static_cast<unsigned char>(expression[pos])) &&
		//				expression[pos] != '[' &&
		//				expression[pos] != ']') {
		//			++pos;
		//		}
		//	}
		//	token = expression.substr(start, pos - start);
		//}
		//else {
		//	// ordinary token
		//	while (pos < expression.size() &&
		//			!std::isspace(static_cast<unsigned char>(expression[pos])) &&
		//			expression[pos] != '[' &&
		//			expression[pos] != ']') {
		//		++pos;
		//	}
		//	token = expression.substr(start, pos - start);
		//}
		//// handle multipliers on regular tokens, while preserving
		//// chords as single tokens.
		//size_t star = token.find('*');
		//if (star != std::string::npos &&
		//		star + 1 < token.size() &&
		//		isNumber(token.substr(star + 1))) {
		//	int repetitions = std::stoi(token.substr(star + 1));
		//	token = token.substr(0, star);
		//	for (int i = 0; i < repetitions; ++i)
		//		item.push_back(token);
		//}
		//else {
		//	item.push_back(token);
		//}
		//return item;
		bool insideQuotes = false;
		int angleBracketDepth = 0;
		while (pos < expression.size()) {
			char c = expression[pos];
			// toggle quoted-string state.
			if (c == '"') {
				insideQuotes = !insideQuotes;
				++pos;
				continue;
			}
			// ignore whitespace and bracket delimiters inside quotes.
			if (!insideQuotes) {
				// preserve chords, including spaces inside <...>.
				// a '<' after a backslash is a crescendo (\<), and a '>' after a backslash or a hyphen
				// is a diminuendo (\>) or an accent (->), so they don't open or close a chord
				// otherwise a crescendo after a note would merge all the following tokens into one chord token
				bool notChordBracket = (pos > 0 && (expression[pos-1] == '\\' || (c == '>' && expression[pos-1] == '-')));
				if (c == '<' && !notChordBracket) {
					++angleBracketDepth;
					++pos;
					continue;
				}
				if (c == '>' && angleBracketDepth > 0 && !notChordBracket) {
					--angleBracketDepth;
					++pos;
					continue;
				}
				// stop at whitespace or repetition-group delimiters
				// only when outside chords and quoted strings.
				if (angleBracketDepth == 0) {
					if (std::isspace(static_cast<unsigned char>(c)) || c == '[' || c == ']') {
						break;
					}
				}
			}
			++pos;
		}
		token = expression.substr(start, pos - start);
		// ------------------------------------------------------------
		// handle token multipliers, e.g.:
		//
		// c'2*3
		// c'2^"foo bar"*2
		// <c' e'>2*2
		// ------------------------------------------------------------
		size_t star = token.find_last_of('*');
		if (star != std::string::npos &&
				star + 1 < token.size() &&
				isNumber(token.substr(star + 1))) {
			int repetitions = std::stoi(token.substr(star + 1));
			token = token.substr(0, star);
			for (int i = 0; i < repetitions; ++i)
				item.push_back(token);
		}
		else {
			item.push_back(token);
		}
		return item;
	};
	return parseSequence(false);
}

//--------------------------------------------------------------
CmdOutput Session::scoreCommands(std::vector<std::string>& originalCommands, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	// the initial command is \score. followed by a method
	// to separate the method we create a new vector that will copy the originalCommands vector
	// except from the first item where we trim the \score. part
	std::vector<std::string> commands;
	for (unsigned i = 0; i < originalCommands.size(); i++) {
		if (!i) {
			// remove the "\score." part of the command so we isolate the method
			std::string thisCommand = originalCommands[i].substr(originalCommands[i].find(".")+1);
			commands.push_back(thisCommand);
		}
		else {
			commands.push_back(originalCommands[i]);
		}
	}

	// the methods that concern only what is shown go to the front end (see scoreViewCommand())
	// a command given as their argument (e.g. \score.movex \rand 1 10) is evaluated first, and the method gets its value
	static const std::vector<std::string> viewMethods = {"colors", "show", "hide", "beattype", "movex", "movey", "recenter", "numbars", "update"};
	if (std::find(viewMethods.begin(), viewMethods.end(), commands[0]) != viewMethods.end()) {
		const std::string& method = commands[0];
		if ((method == "movex" || method == "movey" || method == "numbars") && commands.size() >= 2 && !isNumber(commands[1])) {
			std::vector<std::string> v = {commands.begin()+1, commands.end()};
			cmdOutput = parseCommand(genCmdInput(v), lineNum, numLines);
			if (cmdOutput.errorCode > 0) return cmdOutput;
			if (!isNumber(cmdOutput.outputVec[0])) return genError("\"" + method + "\" takes a number as an argument");
			commands = {commands[0], cmdOutput.outputVec[0]};
		}
		std::string error = frontend->scoreViewCommand(commands);
		if (!error.empty()) return genError(error);
		return cmdOutput;
	}

	if (commands[0].compare("play") == 0) {
		if (commands.size() > 1) {
			return genError("\"play\" subcommand takes no arguments");
		}
		// reset animation in case it is on
		if (sharedData.setAnimation) {
			for (std::map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				it->second.setAnimation(true);
			}
			sharedData.animate = true;
		}
		startSequencer = true;
		// ties at the end of bars that are followed by other notes are ignored
		std::string warning = tieWarning(sharedData.loopIndex);
		if (!warning.empty()) return genWarning(warning);
	}

	else if (commands[0].compare("stop") == 0) {
		if (commands.size() > 1) {
			return genError("\"stop\" subcommand takes no arguments");
		}
		sequencer.stop();
	}

	else if (commands[0].compare("stopnow") == 0) {
		if (commands.size() > 1) {
			return genError("\"stopnow\" subcommand takes no arguments");
		}
		sequencer.stopNow();
		if (sharedData.animate) {
			for (std::map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				it->second.setAnimation(false);
			}
			sharedData.animate = false;
			sharedData.setAnimation = true;
		}
		OscMessage m;
		m.setAddress("/play");
		m.addIntArg(0);
		sendToParts(m, true);
		m.clear();
	}

	else if (commands[0].compare("reset") == 0) {
		if (commands.size() > 1) {
			return genError("\"reset\" subcommand takes no arguments");
		}
		for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
			it->second.resetBarDataCounter();
		}
		sequencer.setSequencerRunning(false);
	}

	else if (commands[0].compare("finish") == 0) {
		if (commands.size() > 1) {
			return genError("\"finish\" subcommand takes no arguments");
		}
		sequencer.setFinish(true);
	}

	else if (commands[0].compare("beatsin") == 0) {
		if (commands.size() < 2) {
			return genError("\"beatsin\" takes a number for beat countdown");
		}
		else if (commands.size() > 2) {
			return genError("\"beatsin\" takes one argument only");
		}
		if (!isNumber(commands[1])) {
			return genError("\"beatsin\" takes a number as an argument");
		}
		int countdown = stoi(commands[1]);
		if (countdown < 0) {
			return genError("can't set a negative countdown");
		}
		sequencer.setCountdown(countdown);
	}

	else if (commands[0].compare("animation") == 0) {
		if (commands.size() != 2 || (commands[1] != "on" && commands[1] != "off")) {
			return genError("\\score.animation takes one argument, \"on\" or \"off\"");
		}
		bool animationState = (commands[1] == "on");
		if (animationState) {
			if (sequencer.isThreadRunning()) {
				for (std::map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
					it->second.setAnimation(animationState);
				}
				sharedData.animate = animationState;
			}
			else {
				sharedData.setAnimation = animationState;
			}
		}
		else {
			for (std::map<int, Instrument>::iterator it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
				it->second.setAnimation(animationState);
			}
			sharedData.animate = animationState;
			sharedData.setAnimation = animationState;
		}
	}

	else if (commands[0].compare("seqres") == 0) {
		if (commands.size() != 2) {
			return genError("\"seqres\" takes one argument, the sequencer resolution");
		}
		if (!isNumber(commands[1])) {
			return genError("\"seqres\" takes a number as argument, the sequencer resolution");
		}
		int res = std::stoi(commands[1]);
		if (res <= 0) {
			return genError("sequencer resolution must be greater than zero");
		}
		// an array to test against durations
		std::vector<int> dursArr{1, 2, 4, 8, 16, 32, 64};
		if (find(dursArr.begin(), dursArr.end(), res) == dursArr.end()) {
			return genError(std::to_string(res) + " is not a valid resolutions");
		}
		seqResolution = res;
	}

	else if (commands[0].compare("correct") == 0) {
		if (commands.size() != 2) {
			return genError("\"correct\" takes one argument, \"onoctave\" or \"all\"");
		}
		if (commands[1].compare("onoctave") == 0) {
			correctOnSameOctaveOnly = true;
		}
		else if (commands[1].compare("all") == 0) {
			correctOnSameOctaveOnly = false;
		}
		else {
			return genError( commands[1] + (std::string)": unknown argument to \"correct\"");
		}
	}

	else {
		return genError("unknown method to \\score");
	}
	return cmdOutput;
}

//--------------------------------------------------------------
CmdOutput Session::maestroCommands(std::vector<std::string>& originalCommands, int lineNum, int numLines)
{
	CmdOutput cmdOutput = CmdOutput();
	// the initial command is \maestro. followed by a method
	// to separate the method we create a new vector that will copy the originalCommands vector
	// except from the first item where we trim the \maestro. part
	std::vector<std::string> commands;
	for (unsigned i = 0; i < originalCommands.size(); i++) {
		if (!i) {
			// remove the "\maestro." part of the command so we isolate the method
			std::string thisCommand = originalCommands[i].substr(originalCommands[i].find(".")+1);
			commands.push_back(thisCommand);
		}
		else {
			commands.push_back(originalCommands[i]);
		}
	}

	for (unsigned i = 0; i < commands.size(); i++) {
		if (commands[i].compare("mainaddr") == 0) {
			if (commands.size() < i + 2) {
				return genError("\"mainaddr\" takes one argument, the OSC address to receive sensor data");
			}
			maestroAddress = commands[i+1];
			if (!maestroToggleSet) receivingMaestro = true;
			if (!oscReceiverIsSet) {
				oscReceiver->setup(OSCPORT);
				oscReceiverIsSet = true;
			}
			i++;
		}

		else if (commands[i].compare("toggleaddr") == 0) {
			if (commands.size() < i + 2) {
				return genError("\"toggleaddr\" takes one argument, the OSC address to receive a 0 or 1");
			}
			maestroToggleAddress = commands[i+1];
			maestroToggleSet = true;
			if (!oscReceiverIsSet) {
				oscReceiver->setup(OSCPORT);
				oscReceiverIsSet = true;
			}
			i++;
		}

		else if (commands[i].compare("levareaddr") == 0) {
			if (commands.size() < i + 2) {
				return genError("\"reset\" takes one argument, the OSC address to receive sensor data");
			}
			maestroLevareAddress = commands[i+1];
			if (!oscReceiverIsSet) {
				oscReceiver->setup(OSCPORT);
				oscReceiverIsSet = true;
			}
			i++;
		}

		else if (commands[i].compare("valndx") == 0) {
			if (commands.size() < i + 2) {
				return genError("\"valndx\" takes one argument, the index of the OSC data packet");
			}
			if (!isNumber(commands[i+1])) {
				return genError("argument to \"valndx\" must be a number");
			}
			int val = stoi(commands[i+1]);
			if (val < 0) {
				return genError("OSC data packet index must be positive");
			}
			maestroValNdx = val;
			if (!oscReceiverIsSet) {
				oscReceiver->setup(OSCPORT);
				oscReceiverIsSet = true;
			}
			i++;
		}

		else if (commands[i].compare("valthresh") == 0) {
			if (commands.size() < i + 2) {
				return genError("\"valthresh\" takes one argument, the threshold for triggering beats");
			}
			if (!isFloat(commands[i+1])) {
				return genError("argument to \"valthresh\" must be a number");
			}
			float val = stof(commands[i+1]);
			if (val < 0) {
				return genError("maestro beat threshold must be positive");
			}
			maestroValThresh = val;
			if (!oscReceiverIsSet) {
				oscReceiver->setup(OSCPORT);
				oscReceiverIsSet = true;
			}
			i++;
		}
	}
	return cmdOutput;
}

//--------------------------------------------------------------
void Session::initializeInstrument(std::string instName)
{
	int maxInstIndex = (int)sharedData.instruments.size();
	sharedData.instrumentIndexes[instName] = maxInstIndex;
	sharedData.instruments[maxInstIndex] = Instrument();
	sharedData.instruments[maxInstIndex].setID(maxInstIndex);
	sharedData.instruments[maxInstIndex].setName(instName.substr(1)); // remove the back slash at the beginning
	sharedData.instruments[maxInstIndex].setMute(false);
	sharedData.instruments[maxInstIndex].setColor(colorNameMap[instrumentColors[maxInstIndex%(int)instrumentColors.size()]]);

	// score part OSC stuff
	sharedData.instruments[maxInstIndex].sendToPart = false;

	// an instrument added while the score is animated (e.g. with \insts.add while the sequencer plays) is animated too
	if (sharedData.animate) sharedData.instruments[maxInstIndex].setAnimation(true);
	// so does an instrument created after \notedur or \accoffset was called
	for (const auto& noteDur : noteDurSettings) sharedData.instruments[maxInstIndex].setDuration(noteDur.first, noteDur.second);
	if (accOffsetSet) sharedData.instruments[maxInstIndex].setAccidentalsOffsetCoef(accOffsetCoef);
	sharedData.instrumentIndexesOrdered[maxInstIndex] = maxInstIndex;
	sharedData.numInstruments++;
	keywords.push_back(instName);
	// what the front end shows of the instrument (e.g. its name left of the score, and its staff)
	frontend->instrumentCreated(maxInstIndex);
}

//--------------------------------------------------------------
void Session::addedInstruments(int numInstsBefore)
{
	int numInsts = (int)sharedData.instruments.size();
	int numAdded = numInsts - numInstsBefore;
	if (numAdded <= 0) return;
	// functions bound to the beat, the start of the bar, etc. are bound to the number of instruments and above (see functionFuncs())
	// so these bindings move with the number of instruments
	{
		std::lock_guard<std::mutex> lock(sharedData.functionsMutex);
		for (auto& func : sharedData.functions) {
			int bound = func.second.getBoundInst();
			if (bound >= numInstsBefore) func.second.setBoundInst(bound + numAdded);
		}
	}
	// an empty bar (a whole rest) for every bar defined before the new instruments, for the sequencer and the score
	// only the new instruments are laid out, the bars of the other instruments don't change
	for (auto barIt = sharedData.barsOrdered.begin(); barIt != sharedData.barsOrdered.end(); ++barIt) {
		int bar = barIt->first;
		for (int instNdx = numInstsBefore; instNdx < numInsts; instNdx++) {
			Instrument& inst = sharedData.instruments[instNdx];
			inst.createEmptyMelody(bar);
			inst.setPassed(false);
			if (sharedData.numerator.find(bar) == sharedData.numerator.end()) continue;
			inst.setMeter(bar, sharedData.numerator[bar], sharedData.denominator[bar], sharedData.numBeats[bar]);
			inst.setScoreNotes(bar, sharedData.numerator[bar], sharedData.denominator[bar], sharedData.numBeats[bar],
					sharedData.BPMTempi[bar], sharedData.tempoBaseForScore[bar], sharedData.BPMDisplayHasDot[bar], sharedData.BPMMultiplier[bar]);
			inst.setNotePositions(bar);
		}
	}
}

//--------------------------------------------------------------
void Session::removeInstrument(int instNdx)
{
	auto removedIt = sharedData.instruments.find(instNdx);
	if (removedIt == sharedData.instruments.end()) return;
	int numInsts = (int)sharedData.instruments.size();
	std::string name = "\\" + removedIt->second.getName();
	int removedGroup = removedIt->second.getGroup();
	sequencer.silenceInstrument(removedIt->second);
	// the highlights of instruments with a delay that haven't been shown yet refer to the indexes of the instruments, which change
	sequencer.clearDelayedVisuals();
	// its name is not a command any more
	auto mapIt = commandsMap[livelily].find(name[1]);
	if (mapIt != commandsMap[livelily].end()) mapIt->second.erase(name);
	auto keywordIt = std::find(keywords.begin(), keywords.end(), name);
	if (keywordIt != keywords.end()) keywords.erase(keywordIt);
	// the indexes of the instruments after it go one down, in everything that refers to instruments by their index
	auto newIndex = [instNdx](int ndx) { return (ndx > instNdx ? ndx - 1 : ndx); };
	// groups of instruments (\group), by their names
	for (auto groupIt = instGroups.begin(); groupIt != instGroups.end();) {
		auto& members = groupIt->second;
		members.erase(std::remove(members.begin(), members.end(), name), members.end());
		if (members.size() < 2) {
			auto groupMapIt = commandsMap[livelily].find(groupIt->first[1]);
			if (groupMapIt != commandsMap[livelily].end()) groupMapIt->second.erase(groupIt->first);
			groupIt = instGroups.erase(groupIt);
		}
		else {
			++groupIt;
		}
	}
	// instruments merged to one staff (\merge)
	for (auto& group : mergeGroups) {
		group.erase(std::remove(group.begin(), group.end(), instNdx), group.end());
		for (int& member : group) member = newIndex(member);
	}
	mergeGroups.erase(std::remove_if(mergeGroups.begin(), mergeGroups.end(), [](const std::vector<int>& group) { return group.size() < 2; }), mergeGroups.end());
	// functions bound to it are unbound, and the bindings to the beat, bars, etc. (the number of instruments and above) move too
	{
		std::lock_guard<std::mutex> lock(sharedData.functionsMutex);
		for (auto& func : sharedData.functions) {
			int bound = func.second.getBoundInst();
			if (bound == instNdx) func.second.setBoundInst(-1);
			else if (bound > instNdx) func.second.setBoundInst(bound - 1);
		}
	}
	// the instruments themselves, with their IDs and the IDs their staves are connected to (\group)
	sharedData.instruments.erase(removedIt);
	for (int ndx = instNdx + 1; ndx < numInsts; ndx++) {
		auto nodeHolder = sharedData.instruments.extract(ndx);
		if (nodeHolder.empty()) continue;
		nodeHolder.key() = ndx - 1;
		sharedData.instruments.insert(std::move(nodeHolder));
		sharedData.instruments[ndx - 1].setID(ndx - 1);
	}
	for (auto& inst : sharedData.instruments) {
		int group = inst.second.getGroup();
		// the instrument after the removed one connects to the instrument the removed one was connected to
		if (group == instNdx) inst.second.setGroup(removedGroup >= 0 ? newIndex(removedGroup) : -1);
		else if (group > instNdx) inst.second.setGroup(group - 1);
	}
	// the indexes by name, and in the order of the score parts
	sharedData.instrumentIndexes.clear();
	for (auto& inst : sharedData.instruments) sharedData.instrumentIndexes["\\" + inst.second.getName()] = inst.first;
	std::map<int, int> ordered;
	int order = 0;
	for (auto& entry : sharedData.instrumentIndexesOrdered) {
		if (entry.second == instNdx) continue;
		ordered[order++] = newIndex(entry.second);
	}
	sharedData.instrumentIndexesOrdered = ordered;
	std::vector<int> oscClients;
	for (int client : sharedData.grouppedOSCClients) {
		if (client != instNdx) oscClients.push_back(newIndex(client));
	}
	sharedData.grouppedOSCClients = oscClients;
	if (lastInstrumentIndex == instNdx) lastInstrumentIndex = 0;
	else lastInstrumentIndex = newIndex(lastInstrumentIndex);
	sharedData.numInstruments = (int)sharedData.instruments.size();
	// what the front end shows of the instruments (e.g. their names, the highlights in the editor, and the staves)
	frontend->instrumentRemoved(instNdx);
}

//--------------------------------------------------------------
void Session::fillInMissingInsts(int barIndex)
{
	for (auto it = sharedData.instruments.begin(); it != sharedData.instruments.end(); ++it) {
		if (!it->second.hasPassed()) {
			it->second.createEmptyMelody(barIndex);
			it->second.setPassed(true);
			// since all instruments need to set the boolean below to true
			// for parseStrings() to know that ;all bars of a \bars commands have finished
			// it's ok to set it to true for missing instruments right from the start
			if (parsingBars) it->second.setMultiBarsDone(true);
		}
	}
}

//--------------------------------------------------------------
bool Session::isReservedName(const std::string& name)
{
	return reservedNames.find(name) != reservedNames.end();
}

//--------------------------------------------------------------
int Session::mergeHostOf(int instNdx)
{
	for (const auto& group : mergeGroups) {
		if (group.size() < 2) continue;
		if (std::find(group.begin() + 1, group.end(), instNdx) != group.end()) return group.front();
	}
	return instNdx;
}

//--------------------------------------------------------------
void Session::updateMergedVoices()
{
	for (auto& inst : sharedData.instruments) inst.second.setVoice(1, 1);
	for (const auto& group : mergeGroups) {
		for (size_t v = 0; v < group.size(); v++) {
			auto it = sharedData.instruments.find(group[v]);
			if (it != sharedData.instruments.end()) it->second.setVoice((int)v + 1, (int)group.size());
		}
	}
	// the stems and the positions of the notes change, so all bars are laid out again
	for (auto barIt = sharedData.barsIndexes.begin(); barIt != sharedData.barsIndexes.end(); ++barIt) {
		frontend->setNotePositions(barIt->second);
	}
	if (!sharedData.barsIndexes.empty()) frontend->calculateStaffPositions(getLastBarIndex(), false);
}

//--------------------------------------------------------------
void Session::startSoundStream()
{
	// the click track plays through the sound stream of pyo, and if pyo hasn't started, through a stream of its own
	// which pyo replaces when it starts (see initPyo())
	if (soundStreamSet) return;
	soundStreamSet = audioStream->setup(audioSettings(), audioCallback);
}

//--------------------------------------------------------------
AudioSettings Session::audioSettings()
{
	AudioSettings settings;
	settings.outDeviceSet = outSoundDeviceSet;
	settings.outDevice = outSoundDevice;
	settings.inChannels = 0;
	settings.outChannels = outChannels;
	settings.sampleRate = sampleRate;
	settings.bufferSize = bufferSize;
	settings.numBuffers = 4;
	return settings;
}
