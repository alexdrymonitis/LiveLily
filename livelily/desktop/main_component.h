#ifndef MAIN_COMPONENT_H
#define MAIN_COMPONENT_H

#include "app.h"
#include "app_window.h"
#include "juce_canvas.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <memory>
#include <string>

// the window of the JUCE app of LiveLily: a component in which LiveLily (App, see app.h) draws with the JUCE canvas
// (see juce_canvas.h), every frame of a timer, which passes on the keys (with the codes of keys.h), the mouse, and its size
class MainComponent : public juce::Component, public AppWindow, private juce::Timer
{
	public:
		explicit MainComponent(const std::string& resourcesDirectory);
		~MainComponent() override;
		// what the close button of the window does: LiveLily asks to save the files that have changes, and then quits
		void requestQuit() { app.workspace.requestQuit(); }

		// juce::Component
		void paint(juce::Graphics& g) override;
		void resized() override;
		bool keyPressed(const juce::KeyPress& key) override;
		bool keyStateChanged(bool isKeyDown) override;
		void modifierKeysChanged(const juce::ModifierKeys& modifiers) override;
		void mouseMove(const juce::MouseEvent& event) override;
		void mouseDrag(const juce::MouseEvent& event) override { mouseMove(event); }
		// a click gives the keys to LiveLily too
		void mouseDown(const juce::MouseEvent&) override { grabKeyboardFocus(); }
		void visibilityChanged() override { if (isShowing()) grabKeyboardFocus(); }
		// the modifiers as they are when the window gets or loses the keys (e.g. Alt released while another window had them)
		void focusGained(FocusChangeType) override;
		void focusLost(FocusChangeType) override;

		// the window (see app_window.h)
		FontMetrics loadEditorFont(int size) override;
		void paintGrids(const std::vector<GridPaint>& paints, float brightness) override;
		void paintGrid(const CellGrid& grid, const GridGeometry& geometry, float brightness) override;
		std::string clipboard() override;
		void setClipboard(const std::string& text) override;
		std::string chooseFileToSave(const std::string& defaultName, const std::string& message) override;
		std::string chooseFileToOpen(const std::string& message) override;
		void showMouseCursor(bool show) override;
		std::pair<int, int> mousePosition() override { return {mouseX, mouseY}; }
		float frameRate() override { return targetFrameRate; }
		void setFrameRate(int framesPerSecond) override;
		void toggleFullscreen() override;
		void quit() override;

	private:
		std::string resources;
		// what the views draw with, made before LiveLily, which loads fonts with it
		JuceCanvas canvas;
		App app{*this};
		// the font of the editor, which the grids are painted with
		std::shared_ptr<Font> editorFont;
		float targetFrameRate = 60;
		int mouseX = 0;
		int mouseY = 0;
		bool started = false;
		// the modifiers that are held, as LiveLily was told (see modifierKeysChanged())
		juce::ModifierKeys heldModifiers;
		// the keys that are held, by JUCE's key code, with the code LiveLily was given for them, to tell it when they are released
		std::map<int, int> heldKeys;
		// whether a key has changed since the keys that are held were last checked (see releaseKeys())
		bool keysChanged = false;
		// while LiveLily handles a key, which may open a dialog or change the window (and so take the keys away from it and give them back)
		// what JUCE tells about the keys waits until the next frame, as LiveLily is in the middle of a key (see keyPressed())
		bool handlingKey = false;
		// the window lost the keys, or got them, while a key was handled, so the keys and the modifiers are checked in the next frame
		bool resyncKeys = false;
		// releases the keys that are held and sets the modifiers as they are now
		void syncKeys(bool released);
		// while a file dialog waits for the user, its loop of events runs, and LiveLily is in the middle of the key that opened it
		// so the frames stop and the keys wait (see chooseFileToSave())
		bool inDialog = false;
		// the last frame, shown while a file dialog waits for the user
		juce::Image frameInDialog;
		// a file dialog of the system (zenity or kdialog on Linux), which waits for the user
		std::string chooseFile(const std::string& message, const std::string& defaultName, bool save);
		// tells LiveLily the keys that are no longer down, once a frame, as the automatic repeat of a held key releases and presses it
		// at once, which isn't a release (e.g. the first key of a binding of two keys, held while the second is pressed)
		void releaseKeys();

		// the window gets the keys again after a file dialog, or after it changed to or from full screen, which makes the window anew
		// on Linux (its title bar changes): the window is brought to the front, for at most a second, until it is active
		void reclaimKeys();
		int framesToReclaimKeys = 0;
		// LiveLily takes the keys whenever its window is active (see reclaimKeys())
		void takeKeys();

		void timerCallback() override;
		// the code of keys.h of a key JUCE gives, as openFrameworks gave it, or -1 for a key LiveLily doesn't take
		static int keyCode(const juce::KeyPress& key);
};

#endif
