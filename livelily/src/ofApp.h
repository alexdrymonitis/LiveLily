#ifndef OF_APP_H
#define OF_APP_H

#include "ofMain.h"
#include "app.h"
#include "app_window.h"
#include "of_canvas.h"
#include "keys.h"
#include <string>
#include <vector>

// the openFrameworks front end of LiveLily: its window, in which LiveLily (App, see app.h) draws, with openFrameworks' fonts
// and the canvas of openFrameworks (see of_canvas.h), and which passes on the keys, the mouse, and the size of the window
// in test mode, it runs a test file, types its keys one a frame, and writes the dump (see test_run.h and testmode.cpp)
class ofApp : public ofBaseApp, public AppWindow
{
	public:
		// what the views draw with (see canvas.h), made before LiveLily, which loads fonts with it
		OfCanvas canvas;
		// LiveLily, which draws in this window
		App app{*this};

		// openFrameworks' functions
		void setup() override;
		void update() override;
		void draw() override;
		void exit() override;
		void keyPressed(int key) override;
		void keyReleased(int key) override;
		void mouseMoved(int x, int y) override;
		void windowResized(int w, int h) override;

		// the window (see app_window.h)
		FontMetrics loadEditorFont(int size) override;
		void paintGrids(const std::vector<GridPaint>& paints, float brightness) override;
		void paintGrid(const CellGrid& grid, const GridGeometry& geometry, float brightness) override;
		std::string clipboard() override { return ofGetWindowPtr()->getClipboardString(); }
		void setClipboard(const std::string& text) override { ofGetWindowPtr()->setClipboardString(text); }
		std::string chooseFileToSave(const std::string& defaultName, const std::string& message) override;
		std::string chooseFileToOpen(const std::string& message) override;
		void showMouseCursor(bool show) override { if (show) ofShowCursor(); else ofHideCursor(); }
		std::pair<int, int> mousePosition() override { return {mouseX, mouseY}; }
		float frameRate() override { return ofGetFrameRate(); }
		void setFrameRate(int framesPerSecond) override { ofSetFrameRate(framesPerSecond); }
		void toggleFullscreen() override { ofToggleFullscreen(); }
		void quit() override { ofExit(); }

		// test mode, set from the command line (see main.cpp and testmode.cpp)
		std::string testFile;
		std::string testOut;
		bool testDone = false;
		// with --screenshot, the window is saved to this file after the test has run
		std::string testScreenshot;
		uint64_t testDoneFrame = 0;
		void runTest();
		// adds the state and the grids of the panes and of the area below them to the dump, as they were drawn (see draw())
		void appendTestEnd();
		bool testScreenWritten = false;
		// the keys a test file types after it has run (see runTest()), pressed one by one in update()
		std::vector<KeyEvent> testKeys;
		size_t testKeyNdx = 0;

	private:
		// the font of the editor, which the grids are painted with (see of_grid_painter.h)
		ofTrueTypeFont font;
		// the position of the mouse over the window (\mousex, \mousey, \mouse)
		int mouseX = 0;
		int mouseY = 0;
};

#endif
