// the keys of the workspace (see workspace.h): what every key does in the active pane (the modes of the editor, as in Vim
// the panes, the shell, the bindings of keys), and asking to save the files before quitting
// a front end sends every key that is pressed and released here (with the codes of keys.h)
#include "workspace.h"
#include "session.h"
#include "osc.h"
#include <algorithm>
#include <string>

//--------------------------------------------------------------
void Workspace::keyPressed(int key)
{
	// the keys typed in a pane are sent to a pane of LiveLily on another computer (see \sendkeys)
	if (editors[session.whichPane].getSendKeys()) {
		OscMessage m;
		m.setAddress("/livelily"+std::to_string(editors[session.whichPane].getSendKeysPaneNdx())+"/press");
		m.addIntArg(key);
		editors[session.whichPane].oscKeys->send(m);
	}
	executeKeyPressed(key);
}

//--------------------------------------------------------------
void Workspace::keyReleased(int key)
{
	if (editors[session.whichPane].getSendKeys()) {
		OscMessage m;
		m.setAddress("/livelily"+std::to_string(editors[session.whichPane].getSendKeysPaneNdx())+"/release");
		m.addIntArg(key);
		editors[session.whichPane].oscKeys->send(m);
	}
	executeKeyReleased(key);
}

//--------------------------------------------------------------
// a key typed in a pane that may not be the active one (from OSC, or typed from Python), which is made active only for this key
// so that a pane out of focus can be typed in
void Workspace::keyPressedInPane(int key, int pane)
{
	int whichPaneBackup = session.whichPane;
	session.whichPane = pane;
	executeKeyPressed(key);
	session.whichPane = whichPaneBackup;
}

//--------------------------------------------------------------
void Workspace::keyReleasedInPane(int key, int pane)
{
	int whichPaneBackup = session.whichPane;
	session.whichPane = pane;
	executeKeyReleased(key);
	session.whichPane = whichPaneBackup;
}

//--------------------------------------------------------------
// the function that is called when a key is pressed
// or when a key press is simulated from OSC
void Workspace::executeKeyPressed(int key)
{
	// while asking to save before quitting, the keys answer the question (see quitPromptKey())
	// except the modifier keys, and the arrow keys while typing a path
	bool modifierKey = (key == 0 || key == 1 || key == 2 || key == 4 || (key >= 3680 && key <= 3685));
	bool arrowKey = (key >= 57356 && key <= 57361);
	if (quitPromptState > 0 && !modifierKey && !(arrowKey && quitPromptState == 2)) {
		quitPromptKey(key);
		return;
	}
	// the output of a shell command that doesn't fit the area below the panes scrolls with the up and down arrows
	// and with Page Up and Page Down by the lines that are shown (see drawShell()), until Esc clears it
	if (editors[session.whichPane].showingShell() && !editors[session.whichPane].isTypingShell() && shellTotalLines > shellVisibleLines &&
			(key == 57357 || key == 57359 || key == 57360 || key == 57361)) {
		if (key == 57357) shellScroll--;
		else if (key == 57359) shellScroll++;
		else if (key == 57360) shellScroll -= shellVisibleLines;
		else shellScroll += shellVisibleLines;
		shellScroll = std::max(0, std::min(shellScroll, shellTotalLines - shellVisibleLines));
		return;
	}
	switch (key) {
		case 0: // caps lock
			break;
		case 1:
			editors[session.whichPane].setShiftPressed(true);
			shiftPressed = true;
			break;
		case 2:
			editors[session.whichPane].setCtrlPressed(true);
			ctrlPressed = true;
			break;
		case 4:
			editors[session.whichPane].setAltPressed(true);
			altPressed = true;
			break;
		// below are codes of the Ctl, Alt, and Shift keys
		// that are sent together with the values above
		// left ctl
		case 3680:
			break;
		// right ctl
		case 3683:
			break;
		// left alt
		case 3684:
			break;
		// right alt
		case 3685:
			break;
		// left shift
		case 3682:
			break;
		// right shift
		case 3681:
			break;
		case 57357:
			if (!editors[session.whichPane].isTypingShell()) {
				setNormalModeShift();
				editors[session.whichPane].upArrow(-1); // -1 for single increment
				editors[session.whichPane].setShiftPressed(shiftPressed);
			}
			break;
		case 57359:
			if (!editors[session.whichPane].isTypingShell()) {
				setNormalModeShift();
				editors[session.whichPane].downArrow(-1); // -1 for single decrement
				editors[session.whichPane].setShiftPressed(shiftPressed);
			}
			break;
		case 57358:
			if (!editors[session.whichPane].isTypingShell()) {
				setNormalModeShift();
				editors[session.whichPane].rightArrow();
				editors[session.whichPane].setShiftPressed(shiftPressed);
			}
			else {
				shellStrCursorPos++;
				if (shellStrCursorPos > (int)shellStr.size()) shellStrCursorPos = (int)shellStr.size();
			}
			break;
		case 57356:
			if (!editors[session.whichPane].isTypingShell()) {
				setNormalModeShift();
				editors[session.whichPane].leftArrow();
				editors[session.whichPane].setShiftPressed(shiftPressed);
			}
			else {
				shellStrCursorPos--;
				if (shellStrCursorPos < 0) shellStrCursorPos = 0;
			}
			break;
		case 57360:
			if (!editors[session.whichPane].isTypingShell()) {
				setNormalModeShift();
				editors[session.whichPane].pageUp();
				editors[session.whichPane].setShiftPressed(shiftPressed);
			}
			break;
		case 57361:
			if (!editors[session.whichPane].isTypingShell()) {
				setNormalModeShift();
				editors[session.whichPane].pageDown();
				editors[session.whichPane].setShiftPressed(shiftPressed);
			}
			break;
		case 27: // esc
			editors[session.whichPane].setInserting(false);
			editors[session.whichPane].setTypingShell(false);
			editors[session.whichPane].setShowingShell(false);
			// the output of a command is cleared, and the panes get back the lines they gave to it (see layoutPanesForShell())
			shellStr = "";
			shellLsOutputStr = "";
			shellScroll = 0;
			restorePaneLayout();
			editors[session.whichPane].resetMaxNumLines();
			if (editors[session.whichPane].getVisualMode()) editors[session.whichPane].setVisualMode(false);
			if (editors[session.whichPane].highlightManyChars) editors[session.whichPane].highlightManyChars = false;
			// if we have typed commands and their output is greater than two lines
			// then sharedData.tracebackYCoord will have changed so we need to reset it
			belowTop = belowBase;
			// the same applies to the number of lines for commands and their output
			numShellLines = numShellLsOutputLines = 0;
			// and the sizes of the areas are calculated again the next time they are shown
			shellNumLinesShown = tracebackNumLinesShown = -1;
			//resetScoreYOffset();
		default:
		{
			// keys that wait for a second key (g and the first key of a two-key binding) wait for the very next key only
			// so their state is reset here, and what it was is kept for the checks in normal mode below
			// (a two-key binding is also cancelled when its first key is released, see executeKeyReleased())
			bool gWasPressed = gPressed;
			bool dWasPressed = dPressed;
			dPressed = false;
			// after Ctrl+w in normal mode: h, j, k, and l move to the pane on the left, below, above, and on the right
			// q and c close the pane, and any other key (e.g. Esc) cancels Ctrl+w
			bool ctrlWWasPressed = ctrlWPressed;
			ctrlWPressed = false;
			if (ctrlWWasPressed && !editors[session.whichPane].getInserting() && !editors[session.whichPane].isTypingShell()) {
				switch (key) {
					case 'h': moveToPane(0); break;
					case 'j': moveToPane(1); break;
					case 'k': moveToPane(2); break;
					case 'l': moveToPane(3); break;
					case 'q':
					case 'c':
						if (editors[session.whichPane].isHelpFileOpen()) editors[session.whichPane].closeHelpFile();
						else requestClosePane();
						break;
					default:
						break;
				}
				return;
			}
			editors[session.whichPane].setDPressed(false);
			int pendingBindingKey = editorBindingKey;
			int pendingBindingArg = editorBindingArg;
			bool pendingBindingIsDollar = editorBindingIsDollar;
			gPressed = false;
			editorBindingArg = editorBindingKey = -1;
			editorBindingIsDollar = false;
			if (key == 61 && editors[session.whichPane].isCtrlPressed() && !editors[session.whichPane].isAltPressed()) { // +
				host.changeFontSize(2);
			}
			else if (key == 45 && editors[session.whichPane].isCtrlPressed() && !editors[session.whichPane].isAltPressed()) { // -
				host.changeFontSize(-2);
			}
			else if (key == 43 && editors[session.whichPane].isCtrlPressed()) { // + with shift pressed
				host.changeScoreSize(2);
			}
			else if (key == 95 && editors[session.whichPane].isCtrlPressed()) { // - with shift pressed
				host.changeScoreSize(-2);
			}
			else if (key == 61 && editors[session.whichPane].isCtrlPressed() && editors[session.whichPane].isAltPressed()) { // + (actually =)
				host.changeBrightness(0.01f);
			}
			else if (key == 45 && editors[session.whichPane].isCtrlPressed() && editors[session.whichPane].isAltPressed()) { // -
				host.changeBrightness(-0.01f);
			}
			else if (key == 102 && editors[session.whichPane].isCtrlPressed()) { // f
				if (editors[session.whichPane].getInserting()) {
					host.toggleFullscreen();
				}
				else {
					setNormalModeShift();
					editors[session.whichPane].pageDown();
					editors[session.whichPane].setShiftPressed(shiftPressed);
				}
			}
			else if (char(key) == 'q' && editors[session.whichPane].isCtrlPressed()) {
				requestQuit();
			}
			else if (char(key) == 'n' && editors[session.whichPane].isCtrlPressed()) {
				requestNewSession();
			}
			// add panes. V for vertical, H for horizontal (with shift pressed)
			else if (((key == 86) || (key == 72)) && editors[session.whichPane].isCtrlPressed()) {
				addPane(key == 86);
			}
			// remove panes. Ctrl+w, where if only one pane, we quit
			// in normal mode Ctrl+w waits for a key to move to another pane, or to close this one, as in Vim (see above)
			else if (key == 119 && editors[session.whichPane].isCtrlPressed() && !editors[session.whichPane].getInserting() && !editors[session.whichPane].isTypingShell()) {
				ctrlWPressed = true;
			}
			else if (key == 119 && editors[session.whichPane].isCtrlPressed()) {
				if (editors[session.whichPane].isHelpFileOpen()) {
					editors[session.whichPane].closeHelpFile();
				}
				else {
					requestClosePane();
				}
			}
			// set which editor is active with keys 0-9 and alt pressed
			else if ((key >= 49) && (key <= 57) && altPressed) {
				if ((key - 49) < (int)editors.size()) {
					setActivePane(key - 49);
					host.revealActivePane();
				}
			}
			// swap score position with Ctrl+P for horizontal
			// Ctrl+Shift+P for vertical
			// Ctrl+Alt+Shift+P for full screen
			else if ((key == 112) && ctrlPressed) {
				host.swapScorePosition(0);
			}
			else if ((key == 80) && ctrlPressed && !altPressed) {
				host.swapScorePosition(1);
			}
			else if ((key == 80) && ctrlPressed && altPressed) {
				host.swapScorePosition(2);
			}
			else if (key == 105 && !editors[session.whichPane].getInserting() && !editors[session.whichPane].isTypingShell()) { // i
				editors[session.whichPane].setInserting(true);
				shellStr = "";
				editors[session.whichPane].resetMaxNumLines();
			}
			else if (!editors[session.whichPane].getInserting() && ((key == 58 && !editors[session.whichPane].getVisualMode()) || editors[session.whichPane].isTypingShell())) { // :
				typeShellCommand(key);
			}
			else if (!editors[session.whichPane].getInserting() && !editors[session.whichPane].isTypingShell()) {
				// bindings are not checked for Ctrl+key and Enter
				bool bindingKey = !editors[session.whichPane].isCtrlPressed() && key != 13;
				// if we are holding a key bound with a second key and we now press the second key
				// this is checked first, so that the second key can be a reserved key, without the function it has in normal mode
				if (bindingKey && pendingBindingKey >= 0 && (key == pendingBindingArg || (pendingBindingIsDollar && key >= 48 && key <= 57))) {
					if (pendingBindingIsDollar) {
						shellStr = editors[session.whichPane].bindings[pendingBindingKey].actionVec[0] + " " + std::to_string(key - 48);
					}
					else {
						shellStr = session.genStrFromVec(editors[session.whichPane].bindings[pendingBindingKey].actionVec);
					}
					executingBind = true;
					shellTabCounter = 0;
					shellStrCursorPos = (int)shellStr.size();
					editors[session.whichPane].setTypingShell(true);
					editors[session.whichPane].setShowingShell(true);
				}
				// enable executing chunks of code in normal mode too
				else if ((editors[session.whichPane].isCtrlPressed() || editors[session.whichPane].isShiftPressed()) && key == 13) {
					editors[session.whichPane].allOtherKeys(key);
				}
				// Ctrl+r redoes and u undoes in normal mode, as in Vim (in insert mode these are Ctrl+y and Ctrl+z, see Editor::allOtherKeys())
				else if (key == 114 && editors[session.whichPane].isCtrlPressed()) { // r
					editors[session.whichPane].redo();
				}
				else if (key == 117 && !editors[session.whichPane].isCtrlPressed()) { // u
					editors[session.whichPane].undo();
				}
				else if ((key == 106 || key == 74)) { // j  or J
					setNormalModeShift();
					editors[session.whichPane].downArrow(-1); // -1 for single increment
					editors[session.whichPane].setShiftPressed(shiftPressed);
				}
				else if ((key == 107 || key == 75)) { // k or K
					setNormalModeShift();
					editors[session.whichPane].upArrow(-1); // -1 for single decrement
					editors[session.whichPane].setShiftPressed(shiftPressed);
				}
				else if ((key == 104 || key == 72)) { // h or H
					setNormalModeShift();
					editors[session.whichPane].leftArrow();
					editors[session.whichPane].setShiftPressed(shiftPressed);
				}
				else if ((key == 108 || key == 76)) { // l or L
					setNormalModeShift();
					editors[session.whichPane].rightArrow();
					editors[session.whichPane].setShiftPressed(shiftPressed);
				}
				else if ((key == 98 && editors[session.whichPane].isCtrlPressed())) { // b
					setNormalModeShift();
					editors[session.whichPane].pageUp();
					editors[session.whichPane].setShiftPressed(shiftPressed);
				}
				else if (key == 121) { // y
					editors[session.whichPane].copyString(1); // 1 is index for yanking a string
				}
				else if (key == 112) { // p
					editors[session.whichPane].pasteString(1); //  1 is index for pasting a yanked string
				}
				else if (key == 118 && editors[session.whichPane].isCtrlPressed()) {
					// Ctrl+v selects a block of columns, as in Vim (in insert mode Ctrl+v pastes, see Editor::allOtherKeys())
					editors[session.whichPane].startVisualSelection(2);
				}
				else if (key == 118) {
					// letter v
					editors[session.whichPane].startVisualSelection(0); // immitating Vim's Visual mode
				}
				else if (key == 86 && !editors[session.whichPane].isCtrlPressed()) {
					// V selects whole lines, as in Vim (Ctrl+V adds a pane, see above)
					editors[session.whichPane].startVisualSelection(1);
				}
				else if (key == 68 && !editors[session.whichPane].isCtrlPressed()) { // D
					// in visual mode D deletes whole lines, otherwise it deletes from the cursor to the end of the line, as in Vim
					// in visual mode D deletes the lines of the selection, as in Vim (see Editor::deleteVisualLines())
					if (editors[session.whichPane].getVisualMode()) {
						editors[session.whichPane].deleteVisualLines();
					}
					else {
						editors[session.whichPane].deleteToLineEnd();
					}
				}
				else if (key == 73 && !editors[session.whichPane].isCtrlPressed()) { // I
					// after Ctrl+v, I inserts the same text in all lines of the block, otherwise it inserts at the start of the line
					if (editors[session.whichPane].getVisualMode() && editors[session.whichPane].getSelectionMode() == 2) {
						editors[session.whichPane].startBlockInsert();
					}
					else {
						editors[session.whichPane].insertAtLineStart();
					}
				}
				else if (key == 65) {
					// capital A when in normal mode sets pane to insert mode and moves cursor to end of line
					editors[session.whichPane].setCursorPos(editors[session.whichPane].maxCursorPos()+1);
					editors[session.whichPane].setInserting(true);
				}
				else if (key == 100) { // d
					if (editors[session.whichPane].highlightManyChars) {
						// the selection is yanked without clearing it (false), so that deleteString() deletes all of it
						// and not only the character under the cursor (deleteString() clears it afterwards)
						editors[session.whichPane].copyString(1, false); // 1 is index for yanking a string
						editors[session.whichPane].deleteString();
						if (editors[session.whichPane].getVisualMode()) editors[session.whichPane].setVisualMode(false);
					}
					// right after v, before the cursor moves, the selection is the character under the cursor, as in Vim
					else if (editors[session.whichPane].getVisualMode()) {
						editors[session.whichPane].copyString(1); // 1 is index for yanking a string
						editors[session.whichPane].deleteString();
						editors[session.whichPane].setVisualMode(false);
					}
					// without a selection, dd deletes the line, as in Vim
					else if (dWasPressed) {
						editors[session.whichPane].deleteCurrentLine();
						if (editors[session.whichPane].getVisualMode()) editors[session.whichPane].setVisualMode(false);
						editors[session.whichPane].setDPressed(false);
					}
					else {
						dPressed = true;
						editors[session.whichPane].setDPressed(true);
					}
				}
				else if (key == 120 && !editors[session.whichPane].isCtrlPressed()) { // x
					// x deletes the character under the cursor, as in Vim
					editors[session.whichPane].copyString(1); // 1 is index for yanking a string
					editors[session.whichPane].deleteString();
				}
				else if (key == 48) { // 0
					editors[session.whichPane].setCursorPos(0);
				}
				else if (key == 36) { // dollar sign
					editors[session.whichPane].setCursorPos(editors[session.whichPane].maxCursorPos());
				}
				else if (key == 103) { // g, twice moves the cursor to the first line
					if (gWasPressed) {
						setNormalModeShift();
						editors[session.whichPane].upArrow(0);
						editors[session.whichPane].setShiftPressed(shiftPressed);
					}
					else {
						gPressed = true;
					}
				}
				else if (key == 71) { // G moves the cursor to the last line
					// G is typed with Shift, which makes the editor select text when moving the cursor
					// so Shift is considered pressed only in visual mode, where the selection extends
					setNormalModeShift();
					editors[session.whichPane].downArrow(editors[session.whichPane].lineCount-1);
					editors[session.whichPane].setShiftPressed(shiftPressed);
				}
				// bound keys (the keys of normal mode above are reserved and can't be bound)
				else if (bindingKey && editors[session.whichPane].bindings.find(key) != editors[session.whichPane].bindings.end()) {
					// if the binding is two keys, we execute on the press of the second
					if (editors[session.whichPane].bindings[key].bindingVec[0].size() > 0) {
						editorBindingIsDollar = editors[session.whichPane].bindings[key].bindingVec[0].compare("$") == 0;
						editorBindingKey = key;
						editorBindingArg = static_cast<int>(editors[session.whichPane].bindings[key].bindingVec[0][0]);
					}
					// otherwise we execute on the press of the first
					else {
						shellStr = session.genStrFromVec(editors[session.whichPane].bindings[key].actionVec);
						executingBind = true;
						shellTabCounter = 0;
						shellStrCursorPos = (int)shellStr.size();
						editors[session.whichPane].setTypingShell(true);
						editors[session.whichPane].setShowingShell(true);
					}
				}
			}
			else if (editors[session.whichPane].getInserting()) {
				editors[session.whichPane].allOtherKeys(key);
			}
			break;
		}
	}
}

//--------------------------------------------------------------
// the function that is called when a key is released
// or when a key release is simulated from OSC
void Workspace::executeKeyReleased(int key)
{
	switch (key) {
		case 1:
			editors[session.whichPane].setShiftPressed(false);
			shiftPressed = false;
			break;
		case 2:
			editors[session.whichPane].setCtrlPressed(false);
			ctrlPressed = false;
			break;
		case 4:
			editors[session.whichPane].setAltPressed(false);
			altPressed = false;
			break;
		default:
			// a two-key binding works only while its first key is held, so releasing the first key cancels it
			if (key == editorBindingKey) {
				editorBindingArg = editorBindingKey = -1;
				editorBindingIsDollar = false;
			}
			break;
	}
}

//--------------------------------------------------------------
// Shift selects text when moving the cursor only in insert mode
// in normal mode the selection extends only in visual mode, whether Shift is pressed or not
// (keys like G, J, K, H, and L are typed with Shift, and they should only move the cursor)
// after moving the cursor, the Shift state of the editor is restored with setShiftPressed(shiftPressed)
void Workspace::setNormalModeShift()
{
	if (!editors[session.whichPane].getInserting()) editors[session.whichPane].setShiftPressed(editors[session.whichPane].getVisualMode());
}

//--------------------------------------------------------------
void Workspace::requestQuit()
{
	quitPromptFor = QuitPromptFor::quit;
	quitPromptPanes.clear();
	for (auto& editor : editors) {
		// a help file is not saved, it replaces the text of its pane only while it is open
		if (editor.second.isFileEdited() && !editor.second.isHelpFileOpen()) quitPromptPanes.push_back(editor.first);
	}
	quitPromptNext();
}

//--------------------------------------------------------------
void Workspace::requestClosePane()
{
	// the last pane quits, which asks for every pane
	if (editors.size() <= 1) {
		requestQuit();
		return;
	}
	Editor& editor = editors[session.whichPane];
	if (!editor.isFileEdited() || editor.isHelpFileOpen()) {
		removePane();
		return;
	}
	quitPromptFor = QuitPromptFor::closePane;
	quitPromptPanes = {session.whichPane};
	quitPromptNext();
}

//--------------------------------------------------------------
void Workspace::requestNewSession()
{
	quitPromptPanes.clear();
	for (auto& editor : editors) {
		if (editor.second.isFileEdited() && !editor.second.isHelpFileOpen()) quitPromptPanes.push_back(editor.first);
	}
	quitPromptFor = QuitPromptFor::newSession;
	quitPromptNext();
}

//--------------------------------------------------------------
void Workspace::newSession()
{
	// the sequencer stops first, and its thread ends, as it would go on playing the bars and loops that are cleared below
	// (as quitting does, see App::exit())
	session.sequencer.stopNow();
	session.sequencer.waitForThread();
	// remove all panes but one
	while (editors.size() > 1) {
		removePane();
	}
	{
		// the sequencer might be playing, so the instruments are not cleared while it goes through them
		std::lock_guard<std::recursive_mutex> instrumentsLock(session.sharedData.instrumentsMutex);
		session.clearBars();
		session.clearLoops();
		session.clearLists();
		session.clearFunctions();
		session.clearInstruments();
	}
	// hide score
	host.hideScore();
	// an empty pane, with a new file without a name and a new history
	editors[session.whichPane].newFile();
}

//--------------------------------------------------------------
void Workspace::quitPromptNext()
{
	if (quitPromptPanes.empty()) {
		quitPromptState = 0;
		QuitPromptFor what = quitPromptFor;
		quitPromptFor = QuitPromptFor::quit;
		if (what == QuitPromptFor::quit) {
			host.quitNow();
			return;
		}
		// the shell that asked is cleared, and the pane is closed, or a new session starts
		shellStr.clear();
		shellLsOutputStr.clear();
		numShellLines = numShellLsOutputLines = 0;
		editors[session.whichPane].setTypingShell(false);
		editors[session.whichPane].setShowingShell(false);
		if (what == QuitPromptFor::closePane) removePane();
		else newSession();
		return;
	}
	int pane = quitPromptPanes.front();
	// quitting can be called while typing in the shell of another pane (with :qa), which stops here
	for (auto& editor : editors) {
		editor.second.setTypingShell(false);
		editor.second.setShowingShell(false);
	}
	// the pane that is asked for becomes the active one, so it can be seen which file it is
	if (pane != session.whichPane && editors.find(pane) != editors.end()) setActivePane(pane);
	quitPromptState = 1;
	shellStr = "save " + editors[session.whichPane].getFileName() + "? y/n";
	shellStrColor = Colors::yellow;
	shellStrCursorPos = (int)shellStr.size();
	shellLsOutputStr.clear();
	numShellLsOutputLines = 0;
	numShellLines = 1;
	editors[session.whichPane].setTypingShell(false);
	editors[session.whichPane].setShowingShell(true);
}

//--------------------------------------------------------------
void Workspace::quitPromptKey(int key)
{
	// Esc cancels quitting, or closing the pane
	if (key == 27) {
		quitPromptState = 0;
		quitPromptFor = QuitPromptFor::quit;
		quitPromptPanes.clear();
		shellStr.clear();
		shellLsOutputStr.clear();
		numShellLines = numShellLsOutputLines = 0;
		editors[session.whichPane].setTypingShell(false);
		editors[session.whichPane].setShowingShell(false);
		return;
	}
	if (quitPromptState == 1) {
		if (key == 'n' || key == 'N') {
			quitPromptPanes.erase(quitPromptPanes.begin());
			quitPromptNext();
		}
		else if (key == 'y' || key == 'Y') {
			// a file that has a path is saved to it, otherwise (or if it can't be saved there) the path is asked for
			if (editors[session.whichPane].fileLoaded) editors[session.whichPane].saveExistingFile();
			if (!editors[session.whichPane].isFileEdited()) {
				quitPromptPanes.erase(quitPromptPanes.begin());
				quitPromptNext();
				return;
			}
			// the path is typed after :save, as when saving with :save, so Tab completion works (see completeShellCommand())
			quitPromptState = 2;
			shellStr = ":save ";
			shellStrColor = Colors::white;
			shellStrCursorPos = (int)shellStr.size();
			shellTabCounter = 0;
			numShellLines = 1;
			editors[session.whichPane].setTypingShell(true);
			editors[session.whichPane].setShowingShell(true);
		}
		return;
	}
	// typing the path
	if (key == 13) {
		std::string path = shellStr.substr(std::min(shellStr.size(), (size_t)6));
		path.erase(std::remove(path.begin(), path.end(), '\n'), path.end());
		if (path.find_first_not_of(" \t") == std::string::npos) return;
		// :save expands ~ and relative paths, and saves the file
		executeShellCommand();
		if (!editors[session.whichPane].isFileEdited()) {
			quitPromptPanes.erase(quitPromptPanes.begin());
			quitPromptNext();
		}
		else {
			// the file couldn't be saved there (e.g. a directory that doesn't exist), so the path is asked for again
			shellStr = ":save ";
			shellStrColor = Colors::white;
			shellStrCursorPos = (int)shellStr.size();
			shellLsOutputStr = "could not save " + editors[session.whichPane].getFileName() + " there, type another path, or Esc to cancel quitting";
			numShellLsOutputLines = 1;
			numShellLines = 1;
			editors[session.whichPane].setTypingShell(true);
			editors[session.whichPane].setShowingShell(true);
		}
		return;
	}
	// the :save at the start of the line can't be deleted, and : would start a new command
	if ((key == 8 && shellStrCursorPos <= 6) || (key == 127 && shellStrCursorPos < 6) || key == ':') return;
	typeShellCommand(key);
	// the text typed so far keeps the cursor after :save
	if (shellStrCursorPos < 6) shellStrCursorPos = 6;
}
