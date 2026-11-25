#ifndef WORKSPACE_H
#define WORKSPACE_H

#include "editor.h"
#include "cellgrid.h"
#include "color.h"
#include <map>
#include <string>
#include <vector>

class Session;

// what the panes ask of the front end that shows them: the size of the window and of a character cell, where the score is,
// and what the front end does when the panes change (see Workspace)
class WorkspaceHost
{
	public:
		virtual ~WorkspaceHost() = default;
		// the size of the window and of a cell of the editor's font, in the units the front end draws in (pixels for the app)
		struct Metrics
		{
			float width;
			float height;
			float cellWidth;
			float cellHeight;
		};
		virtual Metrics workspaceMetrics() = 0;
		// whether the score is shown beside the panes, its orientation (0 beside them, 1 below or above them)
		// and whether it is at the start of the window (on the left, or at the top), which leaves the other side to the panes
		struct ScorePlacement
		{
			bool visible;
			int orientation;
			bool atStart;
		};
		virtual ScorePlacement scorePlacement() = 0;
		// moves the score to the other side of the window, in the same orientation (it lays out the panes again)
		virtual void swapScorePosition(int orientation) = 0;
		// moves the score away from the active pane, if it covers it
		virtual void revealActivePane() = 0;
		// gives the panes the font of the editor, loaded again if reload is true, and lays out the panes if layout is true
		virtual void applyFont(bool reload, bool layout) = 0;
		// quits at once (:q! and :qa!, or after asking to save the files, see Workspace::requestQuit())
		virtual void quitNow() = 0;
		// what keys change in the front end: the size of the font of the editor (Ctrl+= and Ctrl+-), the size of the score
		// (Ctrl+Shift+= and Ctrl+Shift+-), the brightness (Ctrl+Alt+= and Ctrl+Alt+-), full screen (Ctrl+f in insert mode)
		// and hiding the score (Ctrl+n, which starts a new session)
		virtual void changeFontSize(int delta) = 0;
		virtual void changeScoreSize(int delta) = 0;
		virtual void changeBrightness(float delta) = 0;
		virtual void toggleFullscreen() = 0;
		virtual void hideScore() = 0;
		// the area below the panes has moved from a previous top (it has more or fewer lines), or is back at its base
		// so the score can move with it
		virtual void belowAreaMoved(float prevTop) = 0;
		virtual void belowAreaReset() = 0;
		// the background of the window, for the character under the cursor of the shell
		virtual Color belowAreaBackground() = 0;
};

// the panes of LiveLily (the editors), their layout in the window, and the area below them (the messages of the lines, or the shell)
// with no framework, for every front end (see doc/stage4_inventory.md)
// the panes are laid out by the order of their keys in editors, row by row (or column by column with paneSplitOrientation 1)
// with numPanes holding how many panes every row (or column) has
class Workspace
{
	public:
		Workspace(Session& session, WorkspaceHost& host) : session(session), host(host) {}

		std::map<int, Editor> editors;
		// rows (or columns) of panes, and how many panes each has
		std::map<int, int> numPanes;
		// 0 when rows of panes are stacked, 1 when columns of panes are side by side (set by the first split, see addPane())
		int paneSplitOrientation = 0;

		// the area below the panes: its top when it has the fewest lines (MINTRACEBACKLINES), and its top now
		// which moves up when it has more lines (see layoutPanesForShell())
		float belowBase = 0;
		float belowTop = 0;
		// the number of lines of the area below the panes for the messages of the lines and for the shell, -1 to calculate them again
		// (e.g. after the panes are laid out again, see layout())
		int tracebackNumLinesShown = 0;
		int shellNumLinesShown = 0;

		// sets the position and size of every pane, from the layout of the panes and the size of the window, leaving room to the score
		void layout();
		// sets the row and column of every pane from the layout of the panes
		void setPanesRowsAndCols();
		// adds a pane next to the active one, side by side (Ctrl+Shift+V, :vsplit) or one above the other (Ctrl+Shift+H, :split), as in Vim
		void addPane(bool sideBySide);
		// closes the active pane, or quits if it is the only one
		void removePane();
		// makes the pane in a direction from the active one active (0 left, 1 down, 2 up, 3 right), the one by the cursor if there are more
		void moveToPane(int direction);
		// makes a pane active, which gets the modifier keys that are held
		void setActivePane(int pane);

		// makes the panes shorter so that the shell area below them gets the number of lines it wants
		// as long as every row of panes keeps two lines, one for its text and one for the line with its file name
		// the rows shrink from the bottom up, and the row of the active pane last, so it keeps its height the longest
		// as with Ctrl+w followed by _ in Vim, which leaves one line to the other panes
		// returns the number of lines the shell area gets
		int layoutPanesForShell(int wanted);
		// gives the panes back the layout they had before the shell made them shorter
		void restorePaneLayout();

		// the area below the panes, as a grid of cells (see cellgrid.h), whose rows start at belowTop, which the front end paints
		CellGrid belowGrid;
		// fills belowGrid with the shell, if the active pane shows it, or else with the messages of the lines of the active pane
		// and makes room for them, making the panes shorter (see layoutPanesForShell())
		void fillBelowArea();

		// the shell: a command typed after a colon (an editor command, as in Vim, or a program), and its output
		// the command (or its output, or an error message), and the output of :ls and of Tab completion, which is shown below it
		std::string shellStr;
		std::string shellLsOutputStr;
		int numShellLines = 0;
		int numShellLsOutputLines = 0;
		// the number of Tab presses since the last other key (the second lists the matches, see completeShellCommand())
		int shellTabCounter = 0;
		int shellStrCursorPos = 0;
		// the width of the area below the panes in characters, which the lines of the shell are broken to fit
		size_t maxShellChars = 1;
		Color shellStrColor = Colors::white;
		// the output of a shell command that doesn't fit the area below the panes scrolls with the arrow keys (see ofApp::executeKeyPressed())
		// shellScroll is its first line that is shown, of shellTotalLines, of which shellVisibleLines are shown
		int shellScroll = 0;
		int shellTotalLines = 0;
		int shellVisibleLines = 2;
		// the pane that was active when the panes were made shorter for the shell, which keeps its height the longest (see layoutPanesForShell())
		int shellLayoutPane = -1;
		// whether the shell was shown in the last frame (see fillBelowArea())
		bool shellWasShown = false;
		// the command typed is the action of a binding (see :bind), which is executed as LiveLily code
		bool executingBind = false;
		// the commands of the editor, which are completed with the Tab key together with the programs in the directories of PATH
		// (see completeShellCommand()), other shell commands (e.g. :ls or :pwd) are executed by the shell
		std::vector<std::string> shellCommands = {":save", ":w", ":wq", ":wa", ":wqa", ":x", ":xa", ":load", ":q", ":q!", ":qa", ":qa!", ":set", ":bind", ":unbind", ":cd", ":vs", ":vsplit", ":sp", ":split"};
		// the working directory before the last :cd, for :cd -
		std::string prevWorkingDir;
		// a key typed in the shell: Return executes the command, Backspace and Delete erase, Tab completes, and a colon starts a command
		void typeShellCommand(int key);
		// completes the last word of a shell command with the Tab key
		void completeShellCommand();
		void executeShellCommand();
		// shows the output of a shell command below the panes
		void setShellOutput(const std::string& output, bool error);
		// lists strings in columns that fit the width of the area below the panes, as ls does in a terminal
		void formatShellLsOutput(std::vector<std::string> lsOutputTokens);
		// breaks the lines of a string of the shell to fit the width of the window (maxShellChars), and counts them
		// the strings are broken only when they are shown, so that they fit the window also after it is resized
		std::string wrapShellLines(const std::string& str, int& numLines);
		// runs a command of the system's shell, with its output and its errors in output, and returns its exit code
		int runCommand(const std::string& cmd, std::string& output);

		// the keys (with the codes of keys.h), pressed and released in the active pane, which also go to another LiveLily if its keys are sent
		// (see \sendkeys), or in a pane that may not be the active one (from OSC, or typed from Python)
		void keyPressed(int key);
		void keyReleased(int key);
		void keyPressedInPane(int key, int pane);
		void keyReleasedInPane(int key, int pane);
		// the modifier keys that are held, which the pane that becomes active gets
		bool shiftPressed = false;
		bool ctrlPressed = false;
		bool altPressed = false;
		// true after g is pressed in normal mode, so that a second g moves the cursor to the first line, as in Vim
		bool gPressed = false;
		// true after d is pressed in normal mode without a selection, so that a second d deletes the line, as in Vim
		bool dPressed = false;
		// true after Ctrl+w in normal mode, so that h, j, k, or l moves to the pane on the left, below, above, or on the right, as in Vim
		bool ctrlWPressed = false;
		// the first key of a binding of two keys that is held (see :bind), and the second key it waits for, or any digit with $
		int editorBindingKey = -1;
		int editorBindingArg = -1;
		bool editorBindingIsDollar = false;

		// quitting asks to save every pane with changes that are not saved, before quitting (see WorkspaceHost::quitNow())
		// with "save file? y/n" in the shell, where y saves the file (asking for a path if it has none, see quitPromptKey()) and n doesn't
		// Esc cancels quitting
		void requestQuit();
		// closing the active pane asks the same about it, if it has changes that are not saved (:q, Ctrl+w q, as in Vim)
		// and closing the last pane quits; y saves the file and closes the pane, n closes it without saving, and Esc keeps it
		void requestClosePane();
		// 0 when not quitting, 1 when asking whether to save a pane, 2 when asking for the path to save it to
		int quitPromptState = 0;
		// true when the question is for closing the active pane, not for quitting
		bool quitPromptClosesPane = false;
		// the panes that are left to ask for
		std::vector<int> quitPromptPanes;

	private:
		// what a key does, and what releasing it does
		void executeKeyPressed(int key);
		void executeKeyReleased(int key);
		// in normal mode, Shift makes cursor moves select text only in visual mode
		void setNormalModeShift();
		// the keys typed while asking to save before quitting
		void quitPromptKey(int key);
		// asks to save the next pane with changes that are not saved, or quits if there are none left
		void quitPromptNext();
		// the messages of the lines of the active pane, or the shell, in belowGrid (see fillBelowArea())
		void fillMessages();
		void fillShell();
		// writes the lines of a string (folded to the width of the window) to belowGrid, from a row on
		void writeBelowRows(const std::string& lines, int firstRow, Color color);

		Session& session;
		WorkspaceHost& host;
		// the number of lines, the vertical position, and the height of every pane before the shell made them shorter, by pane index
		// empty while the panes have the layout of layout()
		struct PaneBase
		{
			int lines;
			float y;
			float height;
		};
		std::map<int, PaneBase> paneBase;
};

#endif
