#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main( ){

	ofSetupOpenGL(1080, 720, OF_WINDOW); // OF_FULLSCREEN);			// <-------- setup the GL context

	ofAppGLFWWindow* win;
	win = dynamic_cast<ofAppGLFWWindow *> (ofGetWindowPtr());
	win->setWindowIcon("livelily_logo.png");
	// this kicks off the running of my app
	// can be OF_WINDOW or OF_FULLSCREEN
	// pass in width and height too:
	ofRunApp( new ofApp());

}
