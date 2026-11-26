// the JUCE desktop app of LiveLily: a window with LiveLily in it (see main_component.h)
#include "main_component.h"
#include "paths.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace {

class MainWindow : public juce::DocumentWindow
{
	public:
		explicit MainWindow(const std::string& resources)
			: DocumentWindow("LiveLily", juce::Colours::black, juce::DocumentWindow::allButtons)
		{
			setUsingNativeTitleBar(true);
			component = new MainComponent(resources);
			setContentOwned(component, true);
			setResizable(true, false);
			centreWithSize(getWidth(), getHeight());
			setVisible(true);
			component->grabKeyboardFocus();
		}
		// the keys go to LiveLily whenever the window is active (the window isn't active yet when it is made visible)
		void activeWindowStatusChanged() override
		{
			if (isActiveWindow() && component) component->grabKeyboardFocus();
		}
		// closing the window asks to save the files that have changes, as quitting does (see Workspace::requestQuit())
		void closeButtonPressed() override { component->requestQuit(); }
		MainComponent* component = nullptr;
};

class LiveLilyApplication : public juce::JUCEApplication
{
	public:
		const juce::String getApplicationName() override { return "LiveLily"; }
		const juce::String getApplicationVersion() override { return "0.2"; }
		bool moreThanOneInstanceAllowed() override { return true; }

		void initialise(const juce::String&) override
		{
			// LiveLily's resources (fonts, help files), found from the program's path (see paths.h)
			std::filesystem::path executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName().toStdString();
			window = std::make_unique<MainWindow>(Paths::resources(executable).string());
		}

		void shutdown() override { window.reset(); }

		// a request of the system to quit (e.g. Cmd+Q on macOS) asks to save first, as quitting in LiveLily does
		void systemRequestedQuit() override
		{
			if (window && window->component) window->component->requestQuit();
			else quit();
		}

	private:
		std::unique_ptr<MainWindow> window;
};

}

START_JUCE_APPLICATION(LiveLilyApplication)
