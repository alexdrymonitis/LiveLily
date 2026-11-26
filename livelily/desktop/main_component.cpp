#include "main_component.h"
#include "grid_painter.h"
#include "keys.h"
#include <cctype>

//--------------------------------------------------------------
MainComponent::MainComponent(const std::string& resourcesDirectory) : resources(resourcesDirectory), canvas(resourcesDirectory)
{
	setSize(1350, 900);
	setWantsKeyboardFocus(true);
	// the views draw with this canvas (see canvas.h), which LiveLily loads its fonts with too
	Canvas::current = &canvas;
	app.setup(getWidth(), getHeight(), false, resources);
	// drawn with JUCE's own renderer of each system, which keeps up with the frame rate (an OpenGL context delayed the keys on Linux
	// as its thread reads the events of the window's X connection, see doc/stage4_inventory.md)
	setFrameRate(60);
	started = true;
}

//--------------------------------------------------------------
MainComponent::~MainComponent()
{
	stopTimer();
	if (!app.exiting) app.exit();
	Canvas::current = nullptr;
}

//--------------------------------------------------------------
void MainComponent::timerCallback()
{
	if (inDialog) return;
	takeKeys();
	if (resyncKeys) syncKeys(false);
	releaseKeys();
	app.update();
	repaint();
}

//--------------------------------------------------------------
void MainComponent::paint(juce::Graphics& g)
{
	if (inDialog) {
		g.drawImageAt(frameInDialog, 0, 0);
		return;
	}
	Color background = atBrightness(app.backgroundColor, app.brightnessCoeff);
	g.fillAll(juce::Colour(background.r, background.g, background.b));
	canvas.setGraphics(&g);
	app.draw();
	canvas.setGraphics(nullptr);
}

//--------------------------------------------------------------
void MainComponent::resized()
{
	if (started) app.windowResized(getWidth(), getHeight());
}

//--------------------------------------------------------------
int MainComponent::keyCode(const juce::KeyPress& key)
{
	int code = key.getKeyCode();
	if (code == juce::KeyPress::escapeKey) return Key::escape;
	if (code == juce::KeyPress::returnKey) return Key::enter;
	if (code == juce::KeyPress::backspaceKey) return Key::backspace;
	if (code == juce::KeyPress::tabKey) return Key::tab;
	if (code == juce::KeyPress::deleteKey) return Key::del;
	if (code == juce::KeyPress::spaceKey) return Key::space;
	if (code == juce::KeyPress::leftKey) return Key::left;
	if (code == juce::KeyPress::rightKey) return Key::right;
	if (code == juce::KeyPress::upKey) return Key::up;
	if (code == juce::KeyPress::downKey) return Key::down;
	if (code == juce::KeyPress::pageUpKey) return Key::pageUp;
	if (code == juce::KeyPress::pageDownKey) return Key::pageDown;
	if (code == juce::KeyPress::homeKey) return Key::home;
	if (code == juce::KeyPress::endKey) return Key::end;
	if (code == juce::KeyPress::insertKey) return Key::insert;
	if (code >= juce::KeyPress::F1Key && code <= juce::KeyPress::F12Key) return Key::f1 + (code - juce::KeyPress::F1Key);
	juce::ModifierKeys mods = key.getModifiers();
	// letters are lower case, or upper case with Shift, also with Ctrl or Alt held, as openFrameworks gave them (e.g. Ctrl+Shift+V)
	if (code < 128 && std::isalpha(code)) return (mods.isShiftDown() ? std::toupper(code) : std::tolower(code));
	// any other character as it is typed (e.g. : or + with Shift), which JUCE gives when Ctrl and Alt aren't held
	juce::juce_wchar typed = key.getTextCharacter();
	if (typed >= 32 && typed != 127) return (int)typed;
	// with Ctrl or Alt, JUCE gives the key without Shift, which is applied as on a US keyboard
	if (code >= 32 && code < 127) {
		if (!mods.isShiftDown()) return code;
		static const std::string plain = "`1234567890-=[]\\;',./";
		static const std::string shifted = "~!@#$%^&*()_+{}|:\"<>?";
		size_t at = plain.find((char)code);
		return (at != std::string::npos ? (int)(unsigned char)shifted[at] : code);
	}
	return -1;
}

//--------------------------------------------------------------
bool MainComponent::keyPressed(const juce::KeyPress& key)
{
	if (inDialog) return true;
	// the modifiers of the key, in case a change of them was missed (e.g. Alt released in another window)
	modifierKeysChanged(key.getModifiers());
	int code = keyCode(key);
	if (code < 0) return false;
	heldKeys[key.getKeyCode()] = code;
	handlingKey = true;
	app.keyPressed(code);
	handlingKey = false;
	return true;
}

//--------------------------------------------------------------
bool MainComponent::keyStateChanged(bool)
{
	// JUCE has no release of a key, so the keys that are no longer down are released in the next frame (see releaseKeys())
	keysChanged = true;
	return false;
}

//--------------------------------------------------------------
void MainComponent::releaseKeys()
{
	if (!keysChanged) return;
	keysChanged = false;
	for (auto it = heldKeys.begin(); it != heldKeys.end();) {
		if (!juce::KeyPress::isKeyCurrentlyDown(it->first)) {
			app.keyReleased(it->second);
			it = heldKeys.erase(it);
		}
		else ++it;
	}
}

//--------------------------------------------------------------
void MainComponent::focusLost(FocusChangeType)
{
	// the keys held when another window takes them are released, as their releases go to that window
	if (handlingKey) resyncKeys = true;
	else syncKeys(true);
}

//--------------------------------------------------------------
void MainComponent::focusGained(FocusChangeType)
{
	if (handlingKey) resyncKeys = true;
	else syncKeys(false);
}

//--------------------------------------------------------------
void MainComponent::syncKeys(bool released)
{
	resyncKeys = false;
	if (released) {
		for (auto& key : heldKeys) app.keyReleased(key.second);
		heldKeys.clear();
		modifierKeysChanged(juce::ModifierKeys());
	}
	else {
		keysChanged = true;
		releaseKeys();
		modifierKeysChanged(juce::ModifierKeys::getCurrentModifiersRealtime());
	}
}

//--------------------------------------------------------------
void MainComponent::modifierKeysChanged(const juce::ModifierKeys& modifiers)
{
	if (handlingKey) {
		resyncKeys = true;
		return;
	}
	// the modifiers are keys of their own for LiveLily (see keys.h), pressed and released around the keys they modify
	auto change = [&](bool was, bool is, int code) {
		if (is && !was) app.keyPressed(code);
		else if (!is && was) app.keyReleased(code);
	};
	change(heldModifiers.isShiftDown(), modifiers.isShiftDown(), Key::shift);
	change(heldModifiers.isCtrlDown(), modifiers.isCtrlDown(), Key::control);
	change(heldModifiers.isAltDown(), modifiers.isAltDown(), Key::alt);
	heldModifiers = modifiers;
}

//--------------------------------------------------------------
void MainComponent::mouseMove(const juce::MouseEvent& event)
{
	mouseX = event.x;
	mouseY = event.y;
}

//--------------------------------------------------------------
AppWindow::FontMetrics MainComponent::loadEditorFont(int size)
{
	editorFont = canvas.loadFont("fonts/DroidSansMono.ttf", size);
	FontMetrics metrics;
	// as openFrameworks measured the font (see ofApp::loadEditorFont())
	metrics.cellWidth = editorFont->width("a");
	metrics.cellHeight = editorFont->height("q") + (editorFont->height("l") - editorFont->height("o"));
	metrics.characterOffset = (editorFont->height("j") - editorFont->height("l")) / 2;
	return metrics;
}

//--------------------------------------------------------------
void MainComponent::paintGrids(const std::vector<GridPaint>& paints, float brightness)
{
	if (editorFont) CanvasGrid::paintGrids(paints, *editorFont, brightness);
}

//--------------------------------------------------------------
void MainComponent::paintGrid(const CellGrid& grid, const GridGeometry& geometry, float brightness)
{
	if (editorFont) CanvasGrid::paintGrid(grid, geometry, *editorFont, brightness);
}

//--------------------------------------------------------------
std::string MainComponent::clipboard()
{
	return juce::SystemClipboard::getTextFromClipboard().toStdString();
}

//--------------------------------------------------------------
void MainComponent::setClipboard(const std::string& text)
{
	juce::SystemClipboard::copyTextToClipboard(juce::String::fromUTF8(text.c_str()));
}

//--------------------------------------------------------------
std::string MainComponent::chooseFile(const std::string& message, const std::string& defaultName, bool save)
{
	const bool native = true;
	juce::File start = juce::File::getCurrentWorkingDirectory();
	if (!defaultName.empty()) start = start.getChildFile(juce::String::fromUTF8(defaultName.c_str()));
	juce::FileChooser chooser(juce::String::fromUTF8(message.c_str()), start, "*", native);
	// the frames stop while the dialog waits, as LiveLily is in the middle of the key that opened it, and the last frame stays shown
	frameInDialog = createComponentSnapshot(getLocalBounds());
	inDialog = true;
	bool chosen = (save ? chooser.browseForFileToSave(true) : chooser.browseForFileToOpen());
	inDialog = false;
	frameInDialog = juce::Image();
	reclaimKeys();
	return (chosen ? chooser.getResult().getFullPathName().toStdString() : std::string());
}

//--------------------------------------------------------------
std::string MainComponent::chooseFileToSave(const std::string& defaultName, const std::string& message)
{
	return chooseFile(message, defaultName, true);
}

//--------------------------------------------------------------
std::string MainComponent::chooseFileToOpen(const std::string& message)
{
	return chooseFile(message, "", false);
}

//--------------------------------------------------------------
void MainComponent::showMouseCursor(bool show)
{
	setMouseCursor(show ? juce::MouseCursor::NormalCursor : juce::MouseCursor::NoCursor);
}

//--------------------------------------------------------------
void MainComponent::setFrameRate(int framesPerSecond)
{
	if (framesPerSecond <= 0) return;
	targetFrameRate = (float)framesPerSecond;
	startTimerHz(framesPerSecond);
}

//--------------------------------------------------------------
void MainComponent::toggleFullscreen()
{
	// the whole screen, without the title bar of the window (kiosk mode), or the window as it was
	// a window with the system's title bar keeps it in kiosk mode on Linux, so the window has JUCE's own title bar, which JUCE hides
	// while it is full screen, and gets the system's back afterwards
	auto* window = dynamic_cast<juce::DocumentWindow*>(getTopLevelComponent());
	if (window == nullptr) return;
	juce::Desktop& desktop = juce::Desktop::getInstance();
	if (desktop.getKioskModeComponent() == window) {
		desktop.setKioskModeComponent(nullptr, false);
		window->setUsingNativeTitleBar(true);
	}
	else {
		window->setUsingNativeTitleBar(false);
		desktop.setKioskModeComponent(window, false);
	}
	reclaimKeys();
}

//--------------------------------------------------------------
void MainComponent::reclaimKeys()
{
	framesToReclaimKeys = 60;
	grabKeyboardFocus();
}

//--------------------------------------------------------------
void MainComponent::takeKeys()
{
	if (hasKeyboardFocus(true)) {
		framesToReclaimKeys = 0;
		return;
	}
	juce::ComponentPeer* peer = getPeer();
	if (peer == nullptr) return;
	if (peer->isFocused()) {
		grabKeyboardFocus();
		framesToReclaimKeys = 0;
	}
	else if (framesToReclaimKeys > 0) {
		// the window (made anew, or left behind the dialog) is brought to the front, so that it gets the keys
		if (framesToReclaimKeys-- % 10 == 0) {
			if (auto* window = getTopLevelComponent()) window->toFront(true);
		}
	}
}

//--------------------------------------------------------------
void MainComponent::quit()
{
	juce::JUCEApplication::quit();
}
