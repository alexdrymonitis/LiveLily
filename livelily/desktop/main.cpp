// the JUCE desktop app of LiveLily: a window with LiveLily in it (see main_component.h)
#include "main_component.h"
#include "paths.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace {

class MainWindow : public juce::DocumentWindow
{
	public:
		explicit MainWindow(const std::string& resources)
			: DocumentWindow("LiveLily", juce::Colours::black, juce::DocumentWindow::allButtons, false)
		{
			// the window goes on the screen once it has its size and its place, or it shows at its first size in a corner for a moment
			setUsingNativeTitleBar(true);
			component = new MainComponent(resources);
			setContentOwned(component, true);
			setResizable(true, false);
			centreWithSize(getWidth(), getHeight());
			addToDesktop();
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
			std::string resources = Paths::resources(executable).string();
			// test mode: livelily_desktop --test file.lyv [--out dump.txt] [--screenshot image.png], with paths relative to the current directory
			// LiveLily is drawn offscreen, with no window (see MainComponent::Test)
			MainComponent::Test test;
			juce::StringArray args = getCommandLineParameterArray();
			for (int i = 0; i + 1 < args.size(); i++) {
				if (args[i] == "--test") test.file = args[++i].toStdString();
				else if (args[i] == "--out") test.out = args[++i].toStdString();
				else if (args[i] == "--screenshot") test.screenshot = args[++i].toStdString();
			}
			if (!test.file.empty()) {
				test.file = std::filesystem::absolute(test.file).string();
				test.out = std::filesystem::absolute(test.out.empty() ? test.file + ".dump" : test.out).string();
				if (!test.screenshot.empty()) test.screenshot = std::filesystem::absolute(test.screenshot).string();
				testComponent = std::make_unique<MainComponent>(resources, test);
				return;
			}
			window = std::make_unique<MainWindow>(resources);
		}

		void shutdown() override
		{
			window.reset();
			testComponent.reset();
		}

		// a request of the system to quit (e.g. Cmd+Q on macOS) asks to save first, as quitting in LiveLily does
		void systemRequestedQuit() override
		{
			if (window && window->component) window->component->requestQuit();
			else quit();
		}

	private:
		std::unique_ptr<MainWindow> window;
		std::unique_ptr<MainComponent> testComponent;
};

}

START_JUCE_APPLICATION(LiveLilyApplication)
