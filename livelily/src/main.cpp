#include "ofMain.h"
#include "ofApp.h"
#include <filesystem>

//========================================================================
int main(int argc, char* argv[]){

	// test mode, for checking the parser (see testmode.cpp):
	// livelily --test file.lyv [--out dump.txt] [--screenshot image.png]
	// runs the file chunk by chunk with a hidden window, writes the parsed state to the dump file, and quits
	std::string testFile, testOut, testScreenshot;
	for (int i = 1; i + 1 < argc; i++) {
		std::string arg = argv[i];
		if (arg == "--test") testFile = argv[++i];
		else if (arg == "--out") testOut = argv[++i];
		else if (arg == "--screenshot") testScreenshot = argv[++i];
	}
	if (!testFile.empty()) {
		// the paths are relative to the directory the program is called from
		testFile = std::filesystem::absolute(testFile).string();
		testOut = std::filesystem::absolute(testOut.empty() ? testFile + ".dump" : testOut).string();
		ofGLFWWindowSettings settings;
		settings.setSize(1350, 900);
		settings.visible = false;
		auto window = ofCreateWindow(settings);
		auto app = std::make_shared<ofApp>();
		app->testFile = testFile;
		app->testOut = testOut;
		if (!testScreenshot.empty()) app->testScreenshot = std::filesystem::absolute(testScreenshot).string();
		ofRunApp(window, app);
		return ofRunMainLoop();
	}

	ofSetupOpenGL(1350, 900, OF_WINDOW); // OF_FULLSCREEN);			// <-------- setup the GL context

	ofAppGLFWWindow* win;
	win = dynamic_cast<ofAppGLFWWindow *> (ofGetWindowPtr());
	win->setWindowIcon("livelily_logo.png");
	ofSetEscapeQuitsApp(false); // disable quitting on ESC press
	// this kicks off the running of my app
	// can be OF_WINDOW or OF_FULLSCREEN
	// pass in width and height too:
	ofRunApp( new ofApp());

}
