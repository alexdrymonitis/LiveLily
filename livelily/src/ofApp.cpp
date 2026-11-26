#include "ofApp.h"
#include "of_grid_painter.h"
#include "of_color.h"

//--------------------------------------------------------------
void ofApp::setup()
{
	// the views draw with the canvas of this window (see canvas.h), which LiveLily also loads its fonts with
	Canvas::current = &canvas;
	ofTrueTypeFont::setGlobalDpi(72);
	ofSetWindowTitle("LiveLily");
	// LiveLily's resources (fonts, help files) are where openFrameworks loads files from (see main.cpp)
	app.setup(ofGetWindowWidth(), ofGetWindowHeight(), !testFile.empty(), ofToDataPath("", true));
	ofBackground(toOf(atBrightness(app.backgroundColor, app.brightnessCoeff)));
	ofSetLineWidth(app.lineWidth);
}

//--------------------------------------------------------------
void ofApp::update()
{
	// in test mode, the file is run once setup has finished and a frame has been drawn, and the program quits (see testmode.cpp)
	if (!testFile.empty()) {
		if (!testDone && ofGetFrameNum() > 1) {
			testDone = true;
			testDoneFrame = ofGetFrameNum();
			runTest();
			// the program quits after drawing a few frames, with the screen in the dump (see draw())
		}
		// the keys of the test, one event a frame, as they are typed; the screen is written a few frames after the last one
		else if (testDone && testKeyNdx < testKeys.size()) {
			const KeyEvent& event = testKeys[testKeyNdx++];
			if (event.press) app.keyPressed(event.key);
			else app.keyReleased(event.key);
			testDoneFrame = ofGetFrameNum();
		}
		return;
	}
	app.update();
}

//--------------------------------------------------------------
void ofApp::draw()
{
	app.draw();
	// in test mode, a few frames after the file has run, the screen as it is drawn is added to the dump, the window is saved
	// if a screenshot was asked for, and the program quits (see testmode.cpp)
	if (!testFile.empty() && testDone && testKeyNdx >= testKeys.size() && !testScreenWritten && ofGetFrameNum() > testDoneFrame + 3) {
		testScreenWritten = true;
		appendTestEnd();
		if (!testScreenshot.empty()) ofSaveScreen(testScreenshot);
		ofExit(0);
	}
}

//--------------------------------------------------------------
void ofApp::exit()
{
	// openFrameworks calls this when the window closes, also after LiveLily has quit (see App::exit(), which closes the window)
	if (!app.exiting) app.exit();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key)
{
	app.keyPressed(key);
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key)
{
	app.keyReleased(key);
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y)
{
	mouseX = x;
	mouseY = y;
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h)
{
	app.windowResized(w, h);
}

//--------------------------------------------------------------
AppWindow::FontMetrics ofApp::loadEditorFont(int size)
{
	font.load("fonts/DroidSansMono.ttf", size);
	FontMetrics metrics;
	metrics.cellWidth = font.stringWidth("a");
	metrics.cellHeight = font.stringHeight("q");
	float heightDiff = font.stringHeight("l") - font.stringHeight("o");
	metrics.cellHeight += heightDiff;
	// from the baseline of the text to the bottom of its row, which centers the letters in the rows of the editor
	metrics.characterOffset = font.stringHeight("j") - font.stringHeight("l");
	metrics.characterOffset /= 2;
	return metrics;
}

//--------------------------------------------------------------
void ofApp::paintGrids(const std::vector<GridPaint>& paints, float brightness)
{
	::paintGrids(paints, font, brightness);
}

//--------------------------------------------------------------
void ofApp::paintGrid(const CellGrid& grid, const GridGeometry& geometry, float brightness)
{
	::paintGrid(grid, geometry, font, brightness);
}

//--------------------------------------------------------------
std::string ofApp::chooseFileToSave(const std::string& defaultName, const std::string& message)
{
	ofFileDialogResult result = ofSystemSaveDialog(defaultName, message);
	return (result.bSuccess ? result.filePath : std::string());
}

//--------------------------------------------------------------
std::string ofApp::chooseFileToOpen(const std::string& message)
{
	ofFileDialogResult result = ofSystemLoadDialog(message);
	return (result.bSuccess ? result.filePath : std::string());
}
