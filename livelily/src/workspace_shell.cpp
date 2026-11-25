// the area below the panes of the workspace (see workspace.h): the messages of the lines of the active pane, or the shell
// (a command typed after a colon, as in Vim, and its output), which the front end paints from belowGrid
#include "workspace.h"
#include "session.h"
#include "system_command.h"
#include "clock.h"
#include "utils.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <sstream>
#ifndef _WIN32
#include <sys/wait.h>
#endif

//--------------------------------------------------------------
// the shell when it is shown, or else the messages of the lines (see fillShell() and fillMessages())
void Workspace::fillBelowArea()
{
	// when the shell stops being shown (e.g. a new error message replaces the output of a command)
	// the area below the panes goes back to its size for error messages, as it does when pressing Esc
	bool shellShown = editors[session.whichPane].showingShell();
	if (shellWasShown && !shellShown) {
		shellStr = "";
		numShellLines = numShellLsOutputLines = 0;
		shellScroll = 0;
		belowTop = belowBase;
		restorePaneLayout();
		editors[session.whichPane].resetMaxNumLines();
		// the sizes of the areas below the panes are calculated again in fillMessages() and fillShell()
		tracebackNumLinesShown = shellNumLinesShown = -1;
	}
	shellWasShown = shellShown;
	if (shellShown) fillShell();
	else fillMessages();
}

//--------------------------------------------------------------
void Workspace::fillMessages()
{
	int tracebackCounter = 0;
	// one more than the lines, for the line of the cursor, which is added last
	std::vector<uint64_t> stamps(editors[session.whichPane].allLines.size() + 1);
	std::vector<int> ndxs(editors[session.whichPane].allLines.size() + 1);
	int i = 0;
	int paneNdx = 0;
	//for (auto it = numPanes.begin(); it != numPanes.end(); ++it) {
	//	for (i = 0; i < it->second; i++) {
	//		editors[paneNdx++].resetMaxNumLines();
	//	}
	//}
	// we must first determine which traceback string was initiated first, so we can print this on top
	for (auto it = editors[session.whichPane].getAllLinesBegin(); it != editors[session.whichPane].getAllLinesEnd(); ++it) {
		if ((Clock::millis() - editors[session.whichPane].getTracebackTimeStamp(it->first)) < editors[session.whichPane].getTracebackDur() && editors[session.whichPane].getTracebackStr(it->first).size() > 0) {
			// exclude the line of the cursor as this is check below
			if (it->first != editors[session.whichPane].getCursorLineIndex()) {
				ndxs[i] = it->first;
				stamps[i] = editors[session.whichPane].getTracebackTimeStamp(it->first);
				i++;
			}
		}
		else {
			editors[session.whichPane].setTracebackTimeStamp(it->first, 0);
		}
	}
	// sort indexes based on time stamps (the smaller the time stamp, the higher it should be in the traceback)
	// and lines whose messages came in the same millisecond by their order in the pane, so that the order is the same every time
	{
		std::vector<std::pair<uint64_t, int>> byStamp;
		for (int j = 0; j < i; j++) byStamp.push_back(std::make_pair(stamps[j], ndxs[j]));
		std::sort(byStamp.begin(), byStamp.end());
		for (int j = 0; j < i; j++) {
			stamps[j] = byStamp[j].first;
			ndxs[j] = byStamp[j].second;
		}
	}
	// once we check the traceback messages of lines other than that where the cursor is
	// we check the line of the cursor too, which is shown last
	if (editors[session.whichPane].getTracebackStr(editors[session.whichPane].getCursorLineIndex()).size() > 0) {
		ndxs[i] = editors[session.whichPane].getCursorLineIndex();
		i++;
	}
	// the messages, folded at the width of the window like the output of the shell (see drawShell()), and the number of their rows
	maxShellChars = std::max((size_t)1, (size_t)(host.workspaceMetrics().width / host.workspaceMetrics().cellWidth));
	std::vector<std::string> tracebackStrs(i);
	std::vector<int> tracebackRows(i, 0);
	int numTracebackLines = 0;
	for (int j = 0; j < i; j++) {
		std::string tracebackStr = "";
		// if the stderr message is a warning or error in LiveLily (not Python), prepend the line number
		if (editors[session.whichPane].getTracebackColor(ndxs[j]) > 0 && editors[session.whichPane].getLanguage() != 1) {
			tracebackStr += "line " + std::to_string(ndxs[j]+1) + ": ";
		}
		tracebackStr += editors[session.whichPane].getTracebackStr(ndxs[j]);
		tracebackStrs[j] = wrapShellLines(tracebackStr, tracebackRows[j]);
		numTracebackLines += tracebackRows[j];
	}
	// minimum number of lines is 2
	// so the line that separates the panes from the traceback can fit one line of text
	numTracebackLines = std::max(numTracebackLines, MINTRACEBACKLINES);
	// if the number of lines in the traceback has changed, change the maximum number of lines in the editor
	if (numTracebackLines != tracebackNumLinesShown) {
		tracebackNumLinesShown = numTracebackLines;
		// if the number of lines of the traceback changes, reset the maximum number of lines of the editor
		// need to check which editors go till the bottom of the window
		paneNdx = 0;
		for (auto it = numPanes.begin(); it != numPanes.end(); ++it) {
			for (int i = 0; i < it->second; i++) {
				// with split orientation set to 0, we need to offset the number of lines
				// for the editors on the last row, so the last key of the numPanes std::map
				// with split orientation set to 1, we need to offset the number of lines
				// for the editors that are in the last column of every row
				int leftOperand = (paneSplitOrientation == 0 ? it->first : i);
				int rightOperand = (paneSplitOrientation == 0 ? (int)numPanes.size() : it->second);
				if (leftOperand == rightOperand - 1) {
					if (numTracebackLines > MINTRACEBACKLINES) {
						editors[paneNdx].offsetMaxNumLines(-(numTracebackLines-MINTRACEBACKLINES));
					}
					else {
						editors[paneNdx].resetMaxNumLines();
					}
				}
				paneNdx++;
			}
		}
		if (numTracebackLines > MINTRACEBACKLINES) {
			float prevTracebackYCoord = belowTop;
			belowTop = belowBase - (host.workspaceMetrics().cellHeight * (numTracebackLines - MINTRACEBACKLINES));
			belowTop = std::min(belowTop, belowBase);
			host.belowAreaMoved(prevTracebackYCoord);
		}
		else {
			belowTop = belowBase;
			host.belowAreaReset();
		}
	}
	// the messages, one under the other, in the color of their kind (a note, a warning, or an error)
	belowGrid.resize(numTracebackLines, (int)maxShellChars);
	for (int j = 0; j < i; j++) {
		Color color;
		switch (editors[session.whichPane].getTracebackColor(ndxs[j])) {
			case 1:
				color = Colors::orange;
				break;
			case 2:
				color = Colors::red;
				break;
			default:
				color = Colors::white;
				break;
		}
		writeBelowRows(tracebackStrs[j], tracebackCounter, color);
		tracebackCounter += tracebackRows[j];
	}
}

//--------------------------------------------------------------
void Workspace::fillShell()
{
	// the strings of the shell are broken into lines to fit the width of the window as it is now, so that they fit it also after it is resized
	// a command that is typed has no newlines, so its cursor is on the row of its position, and it can be on a row of its own
	// right after the last character of a row
	maxShellChars = std::max((size_t)1, (size_t)(host.workspaceMetrics().width / host.workspaceMetrics().cellWidth));
	std::string shellLines, shellLsOutputLines;
	if (!shellStr.empty()) {
		shellLines = wrapShellLines(shellStr, numShellLines);
		if (editors[session.whichPane].isTypingShell()) numShellLines = std::max(numShellLines, (int)(shellStrCursorPos / maxShellChars) + 1);
	}
	if (!shellLsOutputStr.empty()) shellLsOutputLines = wrapShellLines(shellLsOutputStr, numShellLsOutputLines);
	// at least two lines, so that the line that separates the panes from the area below them has one line of text under it
	int numShellLinesWanted = std::max(numShellLines + numShellLsOutputLines, MINTRACEBACKLINES);
	// the panes are made shorter when the number of lines changes, or when another pane becomes active, which then keeps its height the longest
	if (numShellLinesWanted != shellNumLinesShown || session.whichPane != shellLayoutPane) {
		shellNumLinesShown = numShellLinesWanted;
		shellLayoutPane = session.whichPane;
		shellVisibleLines = layoutPanesForShell(numShellLinesWanted);
		if (shellVisibleLines > MINTRACEBACKLINES) {
			float prevTracebackYCoord = belowTop;
			belowTop = belowBase - (host.workspaceMetrics().cellHeight * (shellVisibleLines - MINTRACEBACKLINES));
			belowTop = std::min(belowTop, belowBase);
			host.belowAreaMoved(prevTracebackYCoord);
		}
		else {
			belowTop = belowBase;
			host.belowAreaReset();
		}
	}
	// the lines of the command and of its output, of which the lines from shellScroll on are shown, as many as fit
	// (a command that is typed is short, so it is always shown from its start)
	shellTotalLines = numShellLines + numShellLsOutputLines;
	if (editors[session.whichPane].isTypingShell()) shellScroll = 0;
	shellScroll = std::max(0, std::min(shellScroll, shellTotalLines - shellVisibleLines));
	std::vector<std::string> rows;
	auto addRows = [&rows](const std::string& lines, int numLines) {
		size_t first = rows.size();
		size_t start = 0;
		while (start <= lines.size() && !lines.empty()) {
			size_t end = lines.find('\n', start);
			if (end == std::string::npos) end = lines.size();
			rows.push_back(lines.substr(start, end - start));
			start = end + 1;
		}
		// the row of the cursor after the last character of a full row has no characters
		while (rows.size() < first + (size_t)numLines) rows.push_back("");
	};
	addRows(shellLines, numShellLines);
	addRows(shellLsOutputLines, numShellLsOutputLines);
	std::string shownRows;
	for (int i = shellScroll; i < std::min((int)rows.size(), shellScroll + shellVisibleLines); i++) {
		if (i > shellScroll) shownRows += "\n";
		shownRows += rows[i];
	}
	// the area below the panes has already moved up to fit all the lines, so the first line is right below its top
	int numRows = std::max(shellVisibleLines, 1);
	int cursorRow = (int)(shellStrCursorPos / maxShellChars);
	if (editors[session.whichPane].isTypingShell()) numRows = std::max(numRows, cursorRow + 1);
	belowGrid.resize(numRows, (int)maxShellChars);
	writeBelowRows(shownRows, 0, shellStrColor);
	if (editors[session.whichPane].isTypingShell()) {
		// a block in the color of the text, with the character under it in the color of the background
		belowGrid.cursor.shape = GridCursor::Shape::block;
		belowGrid.cursor.row = cursorRow;
		belowGrid.cursor.col = (int)(shellStrCursorPos % maxShellChars);
		belowGrid.cursor.color = shellStrColor;
		if (shellStrCursorPos < (int)shellStr.size()) {
			belowGrid.cursor.charOnTop = shellStr.substr(shellStrCursorPos, 1);
			belowGrid.cursor.charColor = host.belowAreaBackground();
		}
	}
}

//--------------------------------------------------------------
void Workspace::writeBelowRows(const std::string& lines, int firstRow, Color color)
{
	// the lines of a string (folded to the width of the window), from a row of the area below the panes on
	size_t start = 0;
	int row = firstRow;
	while (start <= lines.size()) {
		size_t end = lines.find('\n', start);
		if (end == std::string::npos) end = lines.size();
		belowGrid.write(row, 0, lines.substr(start, end - start), color);
		row++;
		start = end + 1;
	}
}

//--------------------------------------------------------------
std::string Workspace::wrapShellLines(const std::string& str, int& numLines)
{
	std::string wrapped;
	numLines = 0;
	size_t start = 0;
	while (start <= str.size()) {
		size_t end = str.find('\n', start);
		if (end == std::string::npos) end = str.size();
		// every line of the string, broken into rows of maxShellChars characters, with an empty line as one row
		size_t lineStart = start;
		do {
			size_t rowLength = std::min(maxShellChars, end - lineStart);
			if (numLines > 0) wrapped += '\n';
			wrapped += str.substr(lineStart, rowLength);
			numLines++;
			lineStart += rowLength;
		} while (lineStart < end);
		start = end + 1;
	}
	return wrapped;
}

//--------------------------------------------------------------
void Workspace::typeShellCommand(int key)
{
	if (key == 13) {
		executeShellCommand();
		editors[session.whichPane].setTypingShell(false);
		shellTabCounter = 0;
		// a command without output (e.g. :w or :set wrap) leaves nothing to show
		// so the area below the panes shows the error messages again, which it doesn't while it shows the shell
		if (shellStr.empty() && numShellLsOutputLines == 0) {
			editors[session.whichPane].setShowingShell(false);
		}
	}
	else if (key == 8) {
		if (shellStrCursorPos > 0) {
			shellStr.erase(shellStrCursorPos-1, 1);
		}
		shellTabCounter = 0;
		shellStrCursorPos--;
		if (shellStrCursorPos < 0) {
			shellStrCursorPos = 0;
		}
		if (shellStr.empty()) {
			editors[session.whichPane].setTypingShell(false);
			editors[session.whichPane].setShowingShell(false);
		}
	}
	else if (key == 127) {
		if (shellStrCursorPos < (int)shellStr.size()) {
			shellStr.erase(shellStrCursorPos, 1);
		}
		if (shellStr.empty()) {
			editors[session.whichPane].setTypingShell(false);
			editors[session.whichPane].setShowingShell(false);
		}
	}
	else if (key == 105 && shellStr.size() == 0) {
		editors[session.whichPane].setTypingShell(false);
		editors[session.whichPane].setShowingShell(false);
		shellTabCounter = 0;
		editors[session.whichPane].resetMaxNumLines();
	}
	else if (key == 9) {
		completeShellCommand();
	}
	else {
		if (key == 58) {
			shellStr = "";
			shellLsOutputStr = "";
			shellStrCursorPos = 0;
			numShellLines = 1;
			shellStrColor = Colors::white;
			numShellLsOutputLines = 0;
			editors[session.whichPane].setTypingShell(true);
			editors[session.whichPane].setShowingShell(true);
		}
		if (shellStrCursorPos < (int)shellStr.size()) shellStr.insert(shellStr.begin()+shellStrCursorPos, char(key));
		else shellStr += char(key);
		shellTabCounter = 0;
		shellStrCursorPos++;
	}
}

//--------------------------------------------------------------
// completes the word that is typed last in the shell with the Tab key, as in a terminal
// the first word is a command, an editor command (see shellCommands in workspace.h) or a program in one of the directories of PATH
// the other words are paths, absolute, starting with ~ (the home directory), or relative to the working directory
// with one match the word is completed (with a white space after it, unless it is a directory, which gets a slash)
// with more matches the first Tab completes the part they all start with, and the second one lists them
void Workspace::completeShellCommand()
{
	namespace fs = std::filesystem;
	shellLsOutputStr.clear();
	numShellLsOutputLines = 0;
	// any newlines are removed first (long commands are broken into lines only when they are drawn, see drawShell())
	std::string line = shellStr;
	line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
	if (line.empty() || line[0] != ':') return;
	size_t wordStart = line.find_last_of(' ');
	bool completingCommand = (wordStart == std::string::npos);
	wordStart = (completingCommand ? 1 : wordStart + 1);
	std::string word = line.substr(wordStart);
	// the part of the word that is completed, and the matches, which replace it
	std::string prefix;
	std::vector<std::string> matches;
	std::vector<bool> matchIsDir;
	std::error_code ec;
	// a command that is a path (e.g. :~/scripts/run.sh or :./run.sh) is completed as a path, as in a terminal
	if (completingCommand && (word.find('/') != std::string::npos || startsWith(word, "~") || startsWith(word, "."))) {
		completingCommand = false;
	}
	if (completingCommand) {
		prefix = word;
		std::set<std::string> names;
		for (const std::string& cmd : shellCommands) {
			if (cmd.size() > 1 && startsWith(cmd.substr(1), prefix)) names.insert(cmd.substr(1));
		}
		const char* pathEnv = getenv("PATH");
		std::vector<std::string> pathDirs = session.tokenizeString(pathEnv ? std::string(pathEnv) : std::string(), ":");
		for (const std::string& dir : pathDirs) {
			if (dir.empty()) continue;
			for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
				std::string name = it->path().filename().string();
				if (!startsWith(name, prefix)) continue;
				fs::file_status status = it->status(ec);
				if (ec || fs::is_directory(status)) continue;
				// only programs, that are executable
				if ((status.permissions() & (fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec)) == fs::perms::none) continue;
				names.insert(name);
			}
			ec.clear();
		}
		// the directories and the programs of the working directory, so that a path can be typed without ./ (e.g. :Documents/run.sh)
		std::set<std::string> dirs;
		for (fs::directory_iterator it(fs::current_path(ec), fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
			std::string name = it->path().filename().string();
			if (prefix.empty() || !startsWith(name, prefix) || (name[0] == '.' && !startsWith(prefix, "."))) continue;
			fs::file_status status = it->status(ec);
			if (ec) {
				ec.clear();
				continue;
			}
			if (fs::is_directory(status)) dirs.insert(name);
			else if ((status.permissions() & (fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec)) != fs::perms::none) names.insert(name);
		}
		ec.clear();
		for (const std::string& name : names) {
			if (dirs.find(name) != dirs.end()) continue;
			matches.push_back(name);
			matchIsDir.push_back(false);
		}
		for (const std::string& dir : dirs) {
			matches.push_back(dir);
			matchIsDir.push_back(true);
		}
	}
	else {
		// the directory of the path is the part up to its last slash, and the rest is the start of a name in it
		size_t slash = word.find_last_of('/');
		std::string dirPart = (slash == std::string::npos ? "" : word.substr(0, slash + 1));
		prefix = (slash == std::string::npos ? word : word.substr(slash + 1));
		std::string dir = dirPart;
		if (startsWith(dir, "~")) {
			const char* home = getenv("HOME");
			dir = (home ? std::string(home) : std::string()) + dir.substr(1);
		}
		else if (!startsWith(dir, "/")) {
			dir = fs::current_path(ec).string() + "/" + dir;
		}
		// only ~ without a slash completes to the home directory
		if (word == "~") {
			prefix = "";
			dirPart = "~";
			matches.push_back("/");
			matchIsDir.push_back(true);
		}
		else {
			std::vector<std::pair<std::string, bool>> entries;
			// :cd takes a directory, so only directories are completed
			bool onlyDirs = startsWith(line, ":cd ");
			for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
				std::string name = it->path().filename().string();
				// hidden files are listed only when their dot is typed
				if (!startsWith(name, prefix) || (name[0] == '.' && !startsWith(prefix, "."))) continue;
				bool isDir = it->is_directory(ec);
				if (onlyDirs && !isDir) continue;
				entries.push_back(std::make_pair(name, isDir));
			}
			std::sort(entries.begin(), entries.end());
			for (const auto& entry : entries) {
				matches.push_back(entry.first);
				matchIsDir.push_back(entry.second);
			}
		}
		wordStart += dirPart.size();
	}
	if (matches.empty()) return;
	std::string completion;
	if (matches.size() == 1) {
		completion = matches[0] + (matchIsDir[0] ? (endsWith(matches[0], "/") ? "" : "/") : " ");
	}
	else {
		if (shellTabCounter > 0) {
			// the second Tab lists the matches, with a slash after directories
			std::vector<std::string> listed;
			for (size_t i = 0; i < matches.size(); i++) listed.push_back(matches[i] + (matchIsDir[i] ? "/" : ""));
			formatShellLsOutput(listed);
			shellTabCounter++;
			return;
		}
		// the first Tab completes the part all matches start with
		completion = matches[0];
		for (size_t i = 1; i < matches.size(); i++) {
			size_t j = 0;
			while (j < completion.size() && j < matches[i].size() && completion[j] == matches[i][j]) j++;
			completion = completion.substr(0, j);
		}
	}
	shellStr = line.substr(0, wordStart) + completion;
	shellStrCursorPos = (int)shellStr.size();
	shellTabCounter++;
}

//--------------------------------------------------------------
void Workspace::executeShellCommand()
{
	if (executingBind) {
		std::vector<std::string> v = session.tokenizeString(shellStr, " ");
		shellStr.clear();
		executingBind = false;
		session.parseCommand(CmdInput(v, std::vector<std::string>{}, false, true), 1, 1); // dummy second and third arguments
		return;
	}
	shellStrColor = Colors::white;
	// first extract the argument and expand it according to the symbols it uses
	// like the tilde character or a dot
	size_t whiteSpace = shellStr.find(" ");
	std::string arg;
	if (whiteSpace != std::string::npos || whiteSpace < shellStr.size()-1) {
		std::string shellStrLocal = shellStr.substr(whiteSpace+1);
		// remove newline characters from command
		shellStrLocal.erase(std::remove(shellStrLocal.begin(), shellStrLocal.end(), '\n'), shellStrLocal.end());
		// and the white spaces around the argument, as Tab completion adds a white space after a completed file name
		shellStrLocal.erase(0, shellStrLocal.find_first_not_of(" \t"));
		if (shellStrLocal.find_last_not_of(" \t") != std::string::npos) shellStrLocal.erase(shellStrLocal.find_last_not_of(" \t") + 1);
		else shellStrLocal.clear();
		if (startsWith(shellStrLocal, "~")) {
			arg = homeDirectory() + shellStrLocal.substr(1);
		}
		else if (startsWith(shellStrLocal, "/")) {
			arg = shellStrLocal;
		}
		else {
			if (startsWith(shellStr, ":bind") || startsWith(shellStr, ":unbind")) {
				arg = shellStrLocal;
			}
			else {
				std::string pwdStr = systemCommandOutput("pwd");
				// there's a newline character at the end of systemCommandOutput("pwd"), so we remove it below
				arg = pwdStr.substr(0, pwdStr.size()-1) + "/";
				if (startsWith(shellStrLocal, "./")) {
					arg += shellStrLocal.substr(2);
				}
				else {
					arg += shellStrLocal;
				}
			}
		}
		// paths like ../bin/data/file.lyv are tidied to /full/path/bin/data/file.lyv, which is also shown as the path of a loaded file
		if (!arg.empty() && startsWith(arg, "/") && !(startsWith(shellStr, ":bind") || startsWith(shellStr, ":unbind"))) {
			std::string normalized = std::filesystem::path(arg).lexically_normal().string();
			// a directory keeps no trailing slash, except the root directory
			if (normalized.size() > 1 && endsWith(normalized, "/")) normalized.pop_back();
			arg = normalized;
		}
	}
	// then check which command we're calling
	if (shellStr == ":save" || startsWith(shellStr, ":save ")) {
		if (shellStr.compare(":save") == 0) {
			shellStr = "save command called without an argument";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		if (!startsWith(shellStr, ":save ")) {
			shellStr = shellStr + ": not an editor command";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		if (arg.empty()) {
			shellStr = "save command called without an argument";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		editors[session.whichPane].saveFile(arg);
		shellStr.clear();
		numShellLines = 0;
	}
	else if (shellStr.compare(":w") == 0) {
		editors[session.whichPane].saveExistingFile();
		shellStr.clear();
		numShellLines = 0;
	}
	// :wq saves the file of the active pane and closes the pane, or quits if it is the only one, as :q does
	// a file that has no name yet is saved to the path typed after it (e.g. :wq ~/music/piece.lyv)
	// :x does the same, as in Vim, but writes the file only if it has changes that are not saved
	else if (shellStr == ":wq" || startsWith(shellStr, ":wq ") || shellStr == ":x" || startsWith(shellStr, ":x ")) {
		bool onlyIfEdited = startsWith(shellStr, ":x");
		Editor& editor = editors[session.whichPane];
		if (editor.isHelpFileOpen()) {
			// a help file is not saved, it is closed
			editor.closeHelpFile();
			shellStr.clear();
			numShellLines = 0;
			return;
		}
		bool hasPath = (whiteSpace != std::string::npos && shellStr.find_first_not_of(" \t\n", whiteSpace) != std::string::npos && !arg.empty());
		if (onlyIfEdited && !hasPath && !editor.isFileEdited()) {
			// nothing to save
		}
		else if (hasPath) editor.saveFile(arg);
		else if (editor.fileLoaded) editor.saveExistingFile();
		else {
			shellStr = "no file name, type the path after " + std::string(onlyIfEdited ? ":x" : ":wq");
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		// the pane is closed only if its file has been saved
		if (editor.isFileEdited()) {
			shellStr = "could not save " + editor.getFileName();
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		shellStr.clear();
		numShellLines = 0;
		editors[session.whichPane].setTypingShell(false);
		editors[session.whichPane].setShowingShell(false);
		removePane();
	}
	// :wa saves the files of all panes that have changes that are not saved, and :wqa saves them and quits
	// files without a name can't be saved this way, so they are listed, and :wqa asks for their paths (see requestQuit())
	// :xa is the same as :wqa, as in Vim
	else if (shellStr.compare(":wa") == 0 || shellStr.compare(":wqa") == 0 || shellStr.compare(":xa") == 0) {
		bool quitting = (shellStr.compare(":wqa") == 0 || shellStr.compare(":xa") == 0);
		std::vector<std::string> notSaved;
		for (auto& editor : editors) {
			if (!editor.second.isFileEdited() || editor.second.isHelpFileOpen()) continue;
			if (editor.second.fileLoaded) editor.second.saveExistingFile();
			if (editor.second.isFileEdited()) notSaved.push_back(editor.second.getFileName() + " (pane " + std::to_string(editor.first + 1) + ")");
		}
		shellStr.clear();
		numShellLines = 0;
		if (quitting) {
			// the files that are left unsaved are asked for, with their paths if they have no name
			requestQuit();
			return;
		}
		if (!notSaved.empty()) {
			std::string list;
			for (size_t i = 0; i < notSaved.size(); i++) list += (i > 0 ? ", " : "") + notSaved[i];
			shellStr = "not saved: " + list + ", save them with :save";
			shellStrColor = Colors::red;
			numShellLines = 1;
			}
	}

	else if (shellStr == ":load" || startsWith(shellStr, ":load ")) {
		if (shellStr.compare(":load") == 0) {
			shellStr = "load command called without an argument";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		if (!startsWith(shellStr, ":load ")) {
			shellStr = shellStr + ": not an editor command";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		if (arg.empty()) {
			shellStr = "load command called without an argument";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		editors[session.whichPane].loadFile(arg);
		shellStr.clear();
		numShellLines = 0;
	}
	else if (startsWith(shellStr, ":bindings")) {
		if (shellStr.size() > 9) {
			shellStr = shellStr + ": not an editor command";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		if (!arg.empty()) {
			shellStr = "bindings command takes no arguments";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		shellStr.clear();
		int counter = 0;
		for (auto it = editors[session.whichPane].bindings.begin(); it != editors[session.whichPane].bindings.end(); ++it) {
			if (counter > 0) shellStr += "\n";
			counter++;
			shellStr += {static_cast<char>(it->first)};
			if (!it->second.bindingVec.empty()) shellStr += session.genStrFromVec(it->second.bindingVec);
			shellStr += " -> ";
			shellStr += session.genStrFromVec(it->second.actionVec);
		}
		numShellLines = editors[session.whichPane].bindings.size();
	}
	else if (shellStr == ":bind" || startsWith(shellStr, ":bind ")) {
		if (whiteSpace == std::string::npos && shellStr.size() > 5) {
			shellStr = shellStr + ": not an editor command";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		if (arg.empty()) {
			shellStr = "bind command called without an argument";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		std::vector<std::string> v = session.tokenizeString(arg, " ");
		if (std::find(v.begin(), v.end(), "->") == v.end()) {
			shellStr = "wrong formatting for bind";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		bool foundArrow = false;
		std::vector<std::string> v1;
		std::vector<std::string> v2;
		for (std::string s : v) {
			if (s.compare("->") == 0) {
				if (v1.empty()) {
					shellStr = "wrong formatting for bind";
					shellStrColor = Colors::red;
					numShellLines = 1;
					return;
				}
				foundArrow = true;
			}
			else {
				if (!foundArrow) {
					v1.push_back(s);
				}
				else {
					v2.push_back(s);
				}
			}
		}
		if (v1[0].size() > 1) {
			shellStr = "binding key can be one character only";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		int key = static_cast<int>(v1[0][0]);
		// the keys of normal mode are reserved and can't be bound
		static const std::map<int, std::string> normalModeKeys = {
			{':', "types commands"}, {'i', "inserts text"},
			{'h', "moves the cursor left"}, {'H', "moves the cursor left"},
			{'j', "moves the cursor down"}, {'J', "moves the cursor down"},
			{'k', "moves the cursor up"}, {'K', "moves the cursor up"},
			{'l', "moves the cursor right"}, {'L', "moves the cursor right"},
			{'u', "undoes"}, {'y', "yanks"}, {'p', "pastes"}, {'v', "starts visual mode"},
			{'A', "inserts at the end of the line"}, {'d', "deletes"},
			{'0', "moves the cursor to the start of the line"}, {'$', "moves the cursor to the end of the line"},
			{'D', "deletes to the end of the line"}, {'V', "selects whole lines"}, {'I', "inserts at the start of the line"},
			{'x', "deletes the character under the cursor"},
			{'g', "moves the cursor to the first line when pressed twice"}, {'G', "moves the cursor to the last line"}
		};
		auto normalIt = normalModeKeys.find(key);
		if (normalIt != normalModeKeys.end()) {
			shellStr = v1[0] + " is reserved, it " + normalIt->second + " in normal mode";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		if (editors[session.whichPane].bindings.find(key) != editors[session.whichPane].bindings.end()) {
			shellStr = v1[0] + " is already bound";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		editors[session.whichPane].bindings[key] = {v1.size() > 1 ? std::vector<std::string>(v1.begin()+1, v1.end()) : std::vector<std::string>{""}, std::move(v2), false};
		shellStr.clear();
		shellStrColor = Colors::white;
		return;
	}
	else if (shellStr == ":unbind" || startsWith(shellStr, ":unbind ")) {
		if (whiteSpace == std::string::npos && shellStr.size() > 7) {
			shellStr = shellStr + ": not an editor command";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		if (arg.empty()) {
			shellStr = "unbind command called without an argument";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		std::vector<std::string> v = session.tokenizeString(arg, " ");
		int key = static_cast<int>(v[0][0]);
		auto it = editors[session.whichPane].bindings.find(key);
		if (it == editors[session.whichPane].bindings.end()) {
			shellStr = v[0] + " is not bound";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		editors[session.whichPane].bindings.erase(it);
		shellStr.clear();
		shellStrColor = Colors::white;
		return;
	}
	else if (shellStr == ":ls" || startsWith(shellStr, ":ls ")) {
		if (whiteSpace == std::string::npos && shellStr.size() > 3) {
			shellStr = shellStr + ": not an editor command";
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		// with options (e.g. :ls -a), the arguments are passed to ls as they are typed
		std::string rawArgs = (whiteSpace == std::string::npos ? std::string() : shellStr.substr(whiteSpace+1));
		rawArgs.erase(std::remove(rawArgs.begin(), rawArgs.end(), '\n'), rawArgs.end());
		std::string lsOutput = systemCommandOutput("ls -p " + (startsWith(rawArgs, "-") ? rawArgs : arg));
		shellStr.clear();
		numShellLines = 0;
		formatShellLsOutput(session.tokenizeString(lsOutput, "\n"));
	}
	else if (shellStr.compare(":pwd") == 0) {
		shellStr = systemCommandOutput("pwd");
		numShellLines = 1;
	}
	// :cd changes the working directory of LiveLily, as in Vim, which :pwd, :ls, :load, :save, other shell commands, and Tab completion use
	// :cd without an argument goes to the home directory, and :cd - to the previous working directory
	else if (shellStr == ":cd" || startsWith(shellStr, ":cd ")) {
		namespace fs = std::filesystem;
		std::string rawArg = (whiteSpace == std::string::npos ? std::string() : shellStr.substr(whiteSpace+1));
		rawArg.erase(std::remove(rawArg.begin(), rawArg.end(), '\n'), rawArg.end());
		rawArg.erase(0, rawArg.find_first_not_of(" \t"));
		if (rawArg.find_last_not_of(" \t") != std::string::npos) rawArg.erase(rawArg.find_last_not_of(" \t") + 1);
		std::error_code ec;
		std::string target;
		if (rawArg.empty()) {
			const char* home = getenv("HOME");
			target = (home ? std::string(home) : std::string());
		}
		else if (rawArg == "-") {
			if (prevWorkingDir.empty()) {
				shellStr = "no previous directory";
				shellStrColor = Colors::red;
				numShellLines = 1;
				return;
			}
			target = prevWorkingDir;
		}
		else {
			// the argument with ~ expanded, or relative to the working directory (see above)
			target = arg;
			target.erase(target.find_last_not_of(" \t") + 1);
		}
		std::string currentDir = fs::current_path(ec).string();
		if (target.empty() || !fs::is_directory(target, ec)) {
			shellStr = (rawArg.empty() ? std::string("no home directory") : rawArg + ": no such directory");
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		fs::current_path(target, ec);
		if (ec) {
			shellStr = rawArg + ": " + ec.message();
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		prevWorkingDir = currentDir;
		shellStr.clear();
		numShellLines = 0;
	}
	else if (shellStr.compare(":q") == 0) {
		if (editors[session.whichPane].isHelpFileOpen()) {
			editors[session.whichPane].closeHelpFile();
			// the command leaves nothing to show, so the area below the panes shows the error messages again (see typeShellCommand())
			shellStr.clear();
			numShellLines = 0;
		}
		else {
			// a pane with changes that are not saved asks whether to save them first, as quitting does
			requestClosePane();
		}
	}
	// :vs (or :vsplit) adds a pane side by side with the active one, and :sp (or :split) one below it, as in Vim
	// with a file as an argument, the file is opened in the new pane
	else if (shellStr == ":vs" || shellStr == ":vsplit" || shellStr == ":sp" || shellStr == ":split" ||
			startsWith(shellStr, ":vs ") || startsWith(shellStr, ":vsplit ") || startsWith(shellStr, ":sp ") || startsWith(shellStr, ":split ")) {
		bool vertical = startsWith(shellStr, ":vs");
		bool hasFile = (whiteSpace != std::string::npos && !arg.empty() && shellStr.find_first_not_of(" \t\n", whiteSpace) != std::string::npos);
		// the shell of this pane closes, as the new pane becomes the active one
		shellStr.clear();
		numShellLines = 0;
		editors[session.whichPane].setTypingShell(false);
		editors[session.whichPane].setShowingShell(false);
		addPane(vertical);
		// the new pane starts in normal mode, as a pane does in Vim
		editors[session.whichPane].setInserting(false);
		if (hasFile) editors[session.whichPane].loadFile(arg);
	}
	else if (shellStr.compare(":qa") == 0) {
		requestQuit();
	}
	// :q! and :qa! quit without asking to save the files that have changes that are not saved, as in Vim
	// :q! closes the pane when there are more panes (or the help file, if one is open), as :q does
	else if (shellStr.compare(":q!") == 0) {
		if (editors[session.whichPane].isHelpFileOpen()) {
			editors[session.whichPane].closeHelpFile();
			// the command leaves nothing to show, so the area below the panes shows the error messages again (see typeShellCommand())
			shellStr.clear();
			numShellLines = 0;
		}
		// the same test removePane() uses to quit instead of closing a pane
		else if (editors.size() <= 1) {
			host.quitNow();
		}
		else {
			removePane();
		}
	}
	else if (shellStr.compare(":qa!") == 0) {
		host.quitNow();
	}
	// editor options, as in Vim, for the active pane
	// :set wrap wraps long lines around the edge of the pane, and :set wrap! (or :set nowrap) scrolls them, which is the default
	else if (startsWith(shellStr, ":set ") || shellStr.compare(":set") == 0) {
		std::string option = (shellStr.size() > 5 ? shellStr.substr(5) : "");
		option.erase(0, option.find_first_not_of(" \t\n"));
		if (option.find_last_not_of(" \t\n") != std::string::npos) option.erase(option.find_last_not_of(" \t\n") + 1);
		if (option == "wrap") {
			editors[session.whichPane].setWrapLines(true);
		}
		else if (option == "wrap!" || option == "nowrap") {
			editors[session.whichPane].setWrapLines(false);
		}
		else {
			shellStr = (option.empty() ? std::string(":set needs an option") : option + ": unknown option");
			shellStrColor = Colors::red;
			numShellLines = 1;
			return;
		}
		shellStr.clear();
		numShellLines = 0;
	}
	else {
		// any other command is a shell command, typed after the colon (e.g. :ls -l or :git status)
		// its output and its error messages are shown below the panes
		std::string cmd = shellStr.substr(1);
		cmd.erase(std::remove(cmd.begin(), cmd.end(), '\n'), cmd.end());
		std::string output;
		int err = runCommand(cmd, output);
		if (err && output.empty()) output = cmd + ": failed with exit code " + std::to_string(err);
		setShellOutput(output, err != 0);
	}
}

//--------------------------------------------------------------
// shows the output of a shell command below the panes, with its lines broken to fit the width of the window
// the panes are made shorter for long output, and what doesn't fit scrolls with the arrow keys (see drawShell())
void Workspace::setShellOutput(const std::string& output, bool error)
{
	shellStrColor = (error ? Colors::red : Colors::white);
	std::vector<std::string> lines;
	std::string line;
	for (char c : output) {
		if (c == '\n') {
			lines.push_back(line);
			line.clear();
		}
		else if (c == '\t') {
			// tabs go to the next multiple of eight characters, as in a terminal
			line += std::string(8 - (line.size() % 8), ' ');
		}
		else if (c != '\r') {
			line += c;
		}
	}
	if (!line.empty()) lines.push_back(line);
	// break long lines to fit the width of the window
	std::vector<std::string> rows;
	size_t width = std::max((size_t)1, maxShellChars);
	for (const std::string& l : lines) {
		if (l.empty()) rows.push_back("");
		for (size_t i = 0; i < l.size(); i += width) rows.push_back(l.substr(i, width));
	}
	// a command without output (e.g. :touch file) leaves nothing to show, so the shell is hidden (see typeShellCommand())
	if (rows.empty()) {
		shellStr.clear();
		numShellLines = 0;
		return;
	}
	// all of the output is kept, and what doesn't fit the area below the panes scrolls with the arrow keys (see drawShell())
	shellScroll = 0;
	shellStr.clear();
	for (size_t i = 0; i < rows.size(); i++) {
		if (i > 0) shellStr += "\n";
		shellStr += rows[i];
	}
	numShellLines = (int)rows.size();
}

//--------------------------------------------------------------
void Workspace::formatShellLsOutput(std::vector<std::string> lsOutputTokens)
{
	if (lsOutputTokens.size() == 0) return;
	// first find the longest string
	size_t maxStrLength = 1;
	for (const std::string& s : lsOutputTokens) {
		if (s.size() > maxStrLength) maxStrLength = s.size();
	}
	// then determine how many columns fit in the width of the window, with two white spaces between them
	// the last column doesn't need the two white spaces, so they are added to the width of the window
	int numColumns = std::max(1, (int)((maxShellChars + 2) / (maxStrLength + 2)));
	// the strings are listed in columns, from top to bottom and then from left to right, as ls does in a terminal
	int numTokens = (int)lsOutputTokens.size();
	int numRows = (numTokens + numColumns - 1) / numColumns;
	// with the number of rows set, fewer columns might be enough (e.g. 5 strings in 4 columns need 2 rows, which need 3 columns only)
	numColumns = (numTokens + numRows - 1) / numRows;
	shellLsOutputStr.clear();
	shellScroll = 0;
	for (int row = 0; row < numRows; row++) {
		if (row > 0) shellLsOutputStr += "\n";
		for (int col = 0; col < numColumns; col++) {
			int ndx = (col * numRows) + row;
			if (ndx >= numTokens) break;
			const std::string& s = lsOutputTokens[ndx];
			shellLsOutputStr += s;
			// pad all but the last string of the row, to the start of the next column
			if (col < numColumns - 1 && ((col + 1) * numRows) + row < numTokens) {
				shellLsOutputStr += std::string((maxStrLength + 2) - s.size(), ' ');
			}
		}
	}
	numShellLsOutputLines = numRows;
}

//--------------------------------------------------------------
int Workspace::runCommand(const std::string& cmd, std::string& output) {
	// Runs a shell command, stores combined STDOUT+STDERR in `output`.
	// Returns the command's exit code, or -1 if it could not be run.
	namespace fs = std::filesystem;

	std::random_device rd;
	std::stringstream name;
	name << "cmdout_" << std::hex << rd() << rd();
	fs::path tmp = fs::temp_directory_path() / name.str();

	std::string full = "(" + cmd + ") > \"" + tmp.string() + "\" 2>&1";
#ifdef _WIN32
	full = "\"" + full + "\"";   // cmd.exe /c quoting quirk
#endif

	int status = std::system(full.c_str());

	int code;
#ifdef _WIN32
	code = status;
#else
	code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif

	std::ifstream file(tmp, std::ios::binary);
	std::stringstream buffer;
	buffer << file.rdbuf();
	output = buffer.str();

	file.close();
	std::error_code ec;
	fs::remove(tmp, ec);         // ignore cleanup errors
	return code;
}
