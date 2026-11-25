#include "workspace.h"
#include "session.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

//--------------------------------------------------------------
void Workspace::setupFirstPane()
{
	editors.clear();
	editors[0] = Editor();
	editors[0].setActivity(true);
	editors[0].setID(0);
	editors[0].setPaneRow(0);
	editors[0].setPaneCol(0);
	numPanes.clear();
	numPanes[0] = 1;
	paneSplitOrientation = 0;
}

//--------------------------------------------------------------
void Workspace::layout()
{
	WorkspaceHost::Metrics m = host.workspaceMetrics();
	WorkspaceHost::ScorePlacement score = host.scorePlacement();
	// this is the minimum height of the area below the panes, used to calculate the size of the panes
	// MINTRACEBACKLINES lines of the current font, with their baselines one and two lines below the panes (see ofApp::drawTraceback() and drawShell())
	// and the part of the last line below its baseline, as low as the cursor reaches (0.15 of a line, see ofApp::drawShell())
	// the panes get a whole number of lines in the rest of the window, so the area below them is at least this high
	belowBase = m.height - (m.cellHeight * ((float)MINTRACEBACKLINES + 0.15f));
	// get the width and height for each pane
	// initializing 0 to avoid warning messages during compilation
	float paneWidth = 0, paneHeight = 0;
	// we compute the maximum number of lines for a pane with the height of the window
	int maxNumLines = belowBase / m.cellHeight;
	// we need an even number of lines so that more than one panes in a column can halve this number
	if (maxNumLines % 2) {
		if (maxNumLines > 1) maxNumLines--;
		else maxNumLines++;
	}
	// depending on the orientation, we must first create either the height or the width of the editors
	if (paneSplitOrientation == 0) {
		paneHeight = ((float)maxNumLines * m.cellHeight) / (float)numPanes.size();
	}
	else {
		paneWidth = m.width / (float)numPanes.size();
	}
	int paneNdx = 0;
	for (std::map<int, int>::iterator it = numPanes.begin(); it != numPanes.end(); ++it) {
		// each row sets the number of lines for its panes according to the number of rows (it->first) for paneSplitOrientation == 0
		// or number of columns (it->second) for paneSplitOrientation == 1
		bool calcAdditionalLines = true;
		int numLinesDivisor = 1;
		int widthDivisor = 1;
		// if we are showing the score and we have only one pane or two panes
		if (score.visible && ((numPanes.size() == 1 && numPanes[0] == 1) || (numPanes.size() == 2 && (numPanes[0] == 1 && numPanes[1] == 1)))) {
			if (numPanes.size() == 1) {
				if (score.orientation == 0) {
					widthDivisor = 2;
				}
				else {
					numLinesDivisor = 2;
				}
			}
			else {
				if (paneSplitOrientation == 1) {
					if (score.orientation == 1) {
						numLinesDivisor = 2;
						widthDivisor = it->second;
					}
					else {
						numLinesDivisor = it->second;
						widthDivisor = 2;
					}
				}
				else {
					if (score.orientation == 0) {
						numLinesDivisor = widthDivisor = 2;
					}
					else {
						numLinesDivisor = 2;
						widthDivisor = it->second;
					}
				}
			}
			calcAdditionalLines = false;
		}
		else {
			numLinesDivisor = paneSplitOrientation == 0 ? (int)numPanes.size() : it->second;
			widthDivisor = it->second;
		}
		int numLines = maxNumLines / (numLinesDivisor > 0 ? numLinesDivisor : 1);
		int additionalLines = 0;
		int numLinesMultiplier = paneSplitOrientation == 0 ? (int)numPanes.size() : it->second;
		if (calcAdditionalLines && numLines * numLinesMultiplier != maxNumLines) {
			additionalLines = maxNumLines - (numLines * numLinesMultiplier);
		}
		if (paneSplitOrientation == 0) {
			paneWidth = m.width / widthDivisor;
			for (int col = 0; col < it->second; col++) {
				editors[paneNdx].setFrameWidth(paneWidth);
				editors[paneNdx].setFrameXOffset(paneWidth * (float)col);
				editors[paneNdx].setFrameHeight(paneHeight);
				editors[paneNdx].setFrameYOffset(paneHeight * (float)(it->first));
				editors[paneNdx].setMaxNumLines(numLines);
				editors[paneNdx].setMaxCharactersPerString();
				paneNdx++;
			}
		}
		else {
			float paneYOffset = 0;
			float prevHeight = 0;
			for (int col = 0; col < it->second; col++) {
				if (col < additionalLines) paneHeight = (numLines + 1) * m.cellHeight;
				else paneHeight = numLines * m.cellHeight;
				paneYOffset += prevHeight;
				prevHeight = paneHeight;
				editors[paneNdx].setFrameHeight(paneHeight);
				editors[paneNdx].setFrameYOffset(paneYOffset);
				editors[paneNdx].setFrameWidth(paneWidth);
				editors[paneNdx].setFrameXOffset(paneWidth * (float)it->first);
				if (!col) editors[paneNdx].setMaxNumLines(numLines+additionalLines);
				else editors[paneNdx].setMaxNumLines(numLines);
				editors[paneNdx].setMaxCharactersPerString();
				paneNdx++;
			}
		}
	}
	// once all the editors have their dimensions and coordinates set
	// we correct the position of the area below them so that there is no gap
	// between the last line of an editor and that area
	belowBase = (float)maxNumLines * m.cellHeight;
	belowTop = belowBase;
	// the panes have a new layout, which the shell makes shorter again if it needs to (see layoutPanesForShell())
	paneBase.clear();
	// the area below the panes is reset, so its size is calculated again in ofApp::drawTraceback() and drawShell()
	// for the lines that are shown in it, which are broken again to fit the width of the window (e.g. after it is resized)
	tracebackNumLinesShown = shellNumLinesShown = -1;
}

//--------------------------------------------------------------
void Workspace::setPanesRowsAndCols()
{
	int key = 0;
	for (const auto& row : numPanes) {
		for (int col = 0; col < row.second; col++) {
			auto editIt = editors.find(key++);
			if (editIt == editors.end()) continue;
			editIt->second.setPaneRow(row.first);
			editIt->second.setPaneCol(col);
		}
	}
}

//--------------------------------------------------------------
void Workspace::moveToPane(int direction)
{
	Editor& current = editors[session.whichPane];
	float left = current.getFrameXOffset();
	float right = left + current.getFrameWidth();
	float top = current.getFrameYOffset();
	float bottom = top + current.getFrameHeight();
	// the position of the cursor, so that of more panes in a direction, the one next to the cursor is chosen, as in Vim
	float cursorX = current.getCursorScreenX();
	float cursorY = current.getCursorScreenY();
	const float tolerance = 2;
	int best = -1;
	float bestDistance = FLT_MAX;
	bool bestByCursor = false;
	for (auto& editor : editors) {
		if (editor.first == session.whichPane) continue;
		Editor& other = editor.second;
		float oLeft = other.getFrameXOffset();
		float oRight = oLeft + other.getFrameWidth();
		float oTop = other.getFrameYOffset();
		float oBottom = oTop + other.getFrameHeight();
		// the pane must be on that side of the active one, and overlap with it on the other axis
		float distance;
		bool overlaps, byCursor;
		switch (direction) {
			case 0:
				if (oRight > left + tolerance) continue;
				distance = left - oRight;
				overlaps = (oTop < bottom && oBottom > top);
				byCursor = (cursorY >= oTop && cursorY < oBottom);
				break;
			case 1:
				if (oTop < bottom - tolerance) continue;
				distance = oTop - bottom;
				overlaps = (oLeft < right && oRight > left);
				byCursor = (cursorX >= oLeft && cursorX < oRight);
				break;
			case 2:
				if (oBottom > top + tolerance) continue;
				distance = top - oBottom;
				overlaps = (oLeft < right && oRight > left);
				byCursor = (cursorX >= oLeft && cursorX < oRight);
				break;
			default:
				if (oLeft < right - tolerance) continue;
				distance = oLeft - right;
				overlaps = (oTop < bottom && oBottom > top);
				byCursor = (cursorY >= oTop && cursorY < oBottom);
				break;
		}
		if (!overlaps) continue;
		// the closest pane, and of panes equally close, the one by the cursor
		if (distance < bestDistance - tolerance || (std::abs(distance - bestDistance) <= tolerance && byCursor && !bestByCursor)) {
			best = editor.first;
			bestDistance = distance;
			bestByCursor = byCursor;
		}
	}
	if (best >= 0) {
		setActivePane(best);
		host.revealActivePane();
	}
}

//--------------------------------------------------------------
void Workspace::setActivePane(int activePane)
{
	editors[session.whichPane].setActivity(false);
	// set all hot keys to false
	editors[session.whichPane].setShiftPressed(false);
	editors[session.whichPane].setCtrlPressed(false);
	editors[session.whichPane].setAltPressed(false);
	if (editors[session.whichPane].highlightManyChars) editors[session.whichPane].highlightManyChars = false;
	session.whichPane = activePane;
	editors[session.whichPane].setActivity(true);
	// the pane that becomes active has the modifier keys that are held now, as the front end keeps them, also when it makes another pane
	// active by itself (e.g. swapScorePosition() while the font size changes with Ctrl held)
	editors[session.whichPane].setShiftPressed(shiftPressed);
	editors[session.whichPane].setCtrlPressed(ctrlPressed);
	editors[session.whichPane].setAltPressed(altPressed);
}

//--------------------------------------------------------------
// the first split sets whether rows of panes are stacked (paneSplitOrientation 0, after Ctrl+Shift+H) or columns are put side by side (1)
// a split in the other direction adds a new row (or column) after the one of the active pane
void Workspace::addPane(bool sideBySide)
{
	bool firstAddition = (editors.size() == 1);
	if (firstAddition) {
		if (sideBySide) paneSplitOrientation = 1;
		else paneSplitOrientation = 0;
	}
	int paneRowWithCursor = editors[session.whichPane].getPaneRow();
	bool sameRow = ((paneSplitOrientation == 0 && sideBySide) || (paneSplitOrientation == 1 && !sideBySide));
	// the key of the new pane, right after the active pane in its row, or after the last pane of its row, at the start of a new row
	int newKey;
	if (sameRow) {
		newKey = session.whichPane + 1;
		numPanes[paneRowWithCursor]++;
	}
	else {
		newKey = 0;
		std::map<int, int> rows;
		int rowNdx = 0;
		for (const auto& row : numPanes) {
			rows[rowNdx++] = row.second;
			if (row.first <= paneRowWithCursor) newKey += row.second;
			if (row.first == paneRowWithCursor) rows[rowNdx++] = 1;
		}
		numPanes = rows;
	}
	// the panes from the key of the new one on move one key up, starting from the last one
	for (int k = editors.rbegin()->first; k >= newKey; k--) {
		auto nodeHolder = editors.extract(k);
		if (nodeHolder.empty()) continue;
		nodeHolder.key() = k + 1;
		editors.insert(std::move(nodeHolder));
		editors[k+1].setID(k+1);
	}
	editors[newKey] = Editor();
	editors[newKey].setID(newKey);
	// the font of the new editor, without laying out the panes yet
	host.applyFont(false, false);
	setPanesRowsAndCols();
	editors[session.whichPane].setActivity(false);
	// release the Ctrl key from the previous pane before moving to the new one
	editors[session.whichPane].setCtrlPressed(false);
	// for safety, set also the shift and alt keys to released
	editors[session.whichPane].setShiftPressed(false);
	editors[session.whichPane].setAltPressed(false);
	session.whichPane = newKey;
	editors[session.whichPane].setActivity(true);
	// the new pane has the modifier keys that are held now (e.g. Ctrl, after Ctrl+Shift+V)
	editors[session.whichPane].setShiftPressed(shiftPressed);
	editors[session.whichPane].setCtrlPressed(ctrlPressed);
	editors[session.whichPane].setAltPressed(altPressed);
	// if we add a second pane with the score visible
	// if the panes are split vertically and the score is also vertical (opposite values)
	// or both are horizontal
	// we need to swap the position of the score so the new pane becomes visible
	WorkspaceHost::ScorePlacement score = host.scorePlacement();
	if (firstAddition && (score.visible && paneSplitOrientation != score.orientation)) {
		// the panes are laid out first, as swapScorePosition() places the score by them (e.g. with two rows of panes
		// and the score below or above them, it covers the row the active pane isn't in), and lays them out only if the score
		// changes orientation, which it doesn't here
		layout();
		// swap the score position without changing its orientation
		host.swapScorePosition(score.orientation);
		// with two panes side by side and the score beside them, the panes are laid out again for the side the score has moved to
		if (paneSplitOrientation == 1) {
			layout();
		}
	}
	else {
		layout();
	}
}

//--------------------------------------------------------------
// after removing a pane the keys of the panes after it go down by one, a row that is left empty is removed
// and the row and column of every pane are set again from the layout
void Workspace::removePane()
{
	if (editors.size() <= 1) {
		requestQuit();
		return;
	}
	int paneRowWithCursor = editors[session.whichPane].getPaneRow();
	int lastKey = editors.rbegin()->first;
	bool isLastPane = (lastKey == session.whichPane);
	editors.erase(session.whichPane);
	// the panes after the removed one move one key down
	for (int key = session.whichPane + 1; key <= lastKey; key++) {
		auto nodeHolder = editors.extract(key);
		if (nodeHolder.empty()) continue;
		nodeHolder.key() = key - 1;
		editors.insert(std::move(nodeHolder));
		editors[key-1].setID(key-1);
	}
	// the row of the removed pane has one pane less, and if it is left empty it is removed, and the rows after it move one row up
	numPanes[paneRowWithCursor]--;
	if (numPanes[paneRowWithCursor] <= 0) {
		std::map<int, int> rows;
		int rowNdx = 0;
		for (const auto& row : numPanes) {
			if (row.second > 0) rows[rowNdx++] = row.second;
		}
		numPanes = rows;
	}
	setPanesRowsAndCols();
	// if we deleted the last pane we must decrement the pane index
	// otherwise it stays as it is, and the next pane becomes active
	if (isLastPane) session.whichPane--;
	if (session.whichPane < 0) session.whichPane = 0;
	if (session.whichPane > editors.rbegin()->first) session.whichPane = editors.rbegin()->first;
	editors[session.whichPane].setActivity(true);
	// if we're left with one pane only, reset paneSplitOrientation
	// (with more panes in one row or column the orientation stays, as it sets whether they are side by side or one above the other)
	if (editors.size() == 1) {
		paneSplitOrientation = 0;
		WorkspaceHost::ScorePlacement score = host.scorePlacement();
		if (score.atStart) {
			host.swapScorePosition(score.orientation);
		}
	}
	host.applyFont(true, true);
}

//--------------------------------------------------------------
int Workspace::layoutPanesForShell(int wanted)
{
	float cellHeight = host.workspaceMetrics().cellHeight;
	int extra = wanted - MINTRACEBACKLINES;
	if (extra <= 0) {
		restorePaneLayout();
		return MINTRACEBACKLINES;
	}
	// the layout of the panes before they are made shorter, which is stored once, so that it can be given back (see restorePaneLayout())
	if (paneBase.empty()) {
		for (auto& editor : editors) {
			paneBase[editor.first] = {editor.second.getMaxNumLinesReset(), editor.second.getFrameYOffset(), editor.second.getFrameHeight()};
		}
	}
	// the panes are in columns of rows, which all reach the area below the panes
	// with split orientation 0 there is one column, with the rows of numPanes, and the panes of each row side by side
	// with split orientation 1, every item of numPanes is a column, with one pane in each of its rows
	// (the pane indexes follow the same order as in layout())
	std::vector<std::vector<std::vector<int>>> columns;
	int paneNdx = 0;
	if (paneSplitOrientation == 0) {
		columns.resize(1);
		for (const auto& row : numPanes) {
			std::vector<int> panes;
			for (int i = 0; i < row.second; i++) panes.push_back(paneNdx++);
			columns[0].push_back(panes);
		}
	}
	else {
		for (const auto& column : numPanes) {
			std::vector<std::vector<int>> rows;
			for (int i = 0; i < column.second; i++) rows.push_back({paneNdx++});
			columns.push_back(rows);
		}
	}
	const int minRowLines = 2; // one line of text and the line with the file name
	auto rowLines = [this](const std::vector<int>& row) {
		auto it = paneBase.find(row.front());
		return (it != paneBase.end() ? it->second.lines : minRowLines);
	};
	// the most lines the shell can get is what the column that can give the fewest lines can give
	for (const auto& column : columns) {
		int canGive = 0;
		for (const auto& row : column) canGive += std::max(0, rowLines(row) - minRowLines);
		extra = std::min(extra, canGive);
	}
	for (const auto& column : columns) {
		int numRows = (int)column.size();
		// the row of the active pane, if it is in this column
		int activeRow = -1;
		for (int r = 0; r < numRows; r++) {
			if (std::find(column[r].begin(), column[r].end(), session.whichPane) != column[r].end()) activeRow = r;
		}
		// the rows give lines from the bottom up, and the row of the active pane last
		std::vector<int> order;
		for (int r = numRows - 1; r >= 0; r--) {
			if (r != activeRow) order.push_back(r);
		}
		if (activeRow >= 0) order.push_back(activeRow);
		std::vector<int> taken(numRows, 0);
		int left = extra;
		for (int r : order) {
			taken[r] = std::min(left, std::max(0, rowLines(column[r]) - minRowLines));
			left -= taken[r];
		}
		// every row moves up by the lines the rows above it gave
		int above = 0;
		for (int r = 0; r < numRows; r++) {
			for (int pane : column[r]) {
				auto baseIt = paneBase.find(pane);
				if (baseIt == paneBase.end()) continue;
				if (taken[r] > 0) editors[pane].offsetMaxNumLines(-taken[r]);
				else editors[pane].resetMaxNumLines();
				editors[pane].setFrameYOffset(baseIt->second.y - (above * cellHeight));
				editors[pane].setFrameHeight(baseIt->second.height - (taken[r] * cellHeight));
			}
			above += taken[r];
		}
	}
	return MINTRACEBACKLINES + extra;
}

//--------------------------------------------------------------
void Workspace::restorePaneLayout()
{
	for (const auto& base : paneBase) {
		auto it = editors.find(base.first);
		if (it == editors.end()) continue;
		it->second.resetMaxNumLines();
		it->second.setFrameYOffset(base.second.y);
		it->second.setFrameHeight(base.second.height);
	}
	paneBase.clear();
}
