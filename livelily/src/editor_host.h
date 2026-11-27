#ifndef EDITOR_HOST_H
#define EDITOR_HOST_H

// what the editor asks of the app that holds it (Workspace, see workspace.h), so that the editor doesn't depend on the app (see doc/stage2_inventory.md)

#include "color.h"
#include <string>
#include <vector>

class Session;
struct EditorNoteSpan;

class EditorHost
{
	public:
		virtual ~EditorHost() = default;
		// the session whose code the editor runs: the names it colors (commandsMap, keywords, commandsToNotTokenize)
		// the pane whose code runs (whichPane), and Python (sharedData.pyo, initPyo())
		virtual Session& editorSession() = 0;
		// the colors of the app, and the coefficient every color is drawn with
		virtual Color editorForeground() = 0;
		virtual Color editorBackground() = 0;
		virtual float editorBrightness() = 0;
		// the width the text of a pane fits in when the score is shown beside the panes and takes the other half of the window, or -1
		virtual int scoreBesidePanesWidth() = 0;
		// runs lines of the active pane (LiveLily code), from a line on
		virtual void parseStrings(int index, int numLines) = 0;
		// the tokens of the notes that play, which a pane highlights (see \editor.animation)
		virtual std::vector<EditorNoteSpan> getActiveEditorNoteSpans(int pane) = 0;
		// alignment of the melodic lines of bars (see \editor.alignment)
		virtual void alignAfterEdit(int pane, bool space) = 0;
		virtual void alignAfterPaste(int pane) = 0;
		virtual bool startsMelodicLine(const std::string& word) = 0;
		// completion: commands, the methods of a command or a name, arguments, and the names and signatures of Python
		virtual std::vector<std::string> getCommandCompletions() = 0;
		virtual std::vector<std::string> getMethodCompletions(const std::string& name) = 0;
		virtual std::vector<std::string> getArgumentCompletions() = 0;
		virtual std::vector<std::string> pythonNames(const std::string& dotted) = 0;
		virtual std::vector<std::string> pythonSignature(const std::string& dotted) = 0;
		// what Python prints to be typed into a pane (see \pystr): text to type, and the pane it is typed into once all of it is there
		virtual void typeFromPython(const std::string& text) = 0;
		virtual void startTypingFromPython(int pane) = 0;
		virtual int pythonTypingPane() = 0;
		// a key typed in a pane from elsewhere (OSC, see \fromosc), handled as a key of the app: in the active pane, or in another pane
		virtual void keyPressed(int key) = 0;
		virtual void keyReleased(int key) = 0;
		virtual void keyPressedOsc(int key, int pane) = 0;
		virtual void keyReleasedOsc(int key, int pane) = 0;
		// the clipboard of the system
		virtual std::string clipboard() = 0;
		virtual void setClipboard(const std::string& text) = 0;
		// a dialog of the system for choosing a file to save to (starting with a file name) or to open, which returns its path, or an empty string
		virtual std::string chooseFileToSave(const std::string& defaultName, const std::string& message) = 0;
		virtual std::string chooseFileToOpen(const std::string& message) = 0;
		// the size of the window, in pixels (the completion pop-up stays inside it)
		virtual int windowWidth() = 0;
		virtual int windowHeight() = 0;
};

#endif
