#ifndef __pyoclass_h_
#define __pyoclass_h_

#include <string>
#include <vector>
#include <atomic>
#include "m_pyo.h"

typedef int callPtr(void *);

class Pyo {
	public:
		//Pyo();
		~Pyo();
		void setup(int inChannels, int outChannels, int bufferSize, int sampleRate);
		void process(float *buffer);
		void fillin(float *buffer);
		void clear();
		int loadfile(const char *file, int add);
		int exec(const char *msg);
		int exec_main(const char *msg);
		int value(const char *name, float value);
		int value(const char *name, float *value, int len);
		int set(const char *name, float value);
		int set(const char *name, float *value, int len);
		void setDebug(int debugVal);
		std::vector<std::string> getStdout();
		std::string getErrorMsg();
		bool isProcessing();
		// whether the interpreter has been created (see setup())
		bool isSetup() const { return interpreter != nullptr; }
		// for the autocompletion of Python panes (see ofApp::refreshPythonTable())
		// the number of executions of code (exec(), exec_main(), and loadfile(), from any thread), so that what is known of the interpreter
		// is asked again only after code that might have changed it has been executed
		unsigned getExecutions() const { return executions.load(); }
		// the names of __main__ and the builtins, one per line
		std::string globalNames();
		// the attributes of the object of a dotted name (e.g. np or np.random), one per line, which is looked up without running any code of it
		std::string names(const std::string& dotted);
		// the parameters of the callable of a dotted name (e.g. Sine), one per line, as name=default or name, without self, *args, and **kwargs
		std::string signature(const std::string& dotted);
		// the help that help(name) stored when code was executed (see Editor::loadHelpText()), as the name and its help text
		// in the first line and the lines after it, or an empty string, which is taken only once
		std::string takeHelp();

	private:
		//PyGILState_STATE gstate; // to acquire and release the Global Interpreter Lock (GIL)
		int inChannels;
		int outChannels;
		int bufferSize;
		int sampleRate;
		int debug;
		// null until setup() is called, which happens only when Python is used (e.g. with \python)
		// so that the destructor ends the interpreter only if it has been created
		PyThreadState *interpreter = nullptr;
		// whether the helper functions of autocompletion are defined in the interpreter (see names() and signature())
		bool helpersDefined = false;
		std::atomic<unsigned> executions{0};
		float *pyoInBuffer = nullptr;
		float *pyoOutBuffer = nullptr;
		callPtr *pyoCallback = nullptr;
		void *pyoId = nullptr;
		bool processing = false;
		char pyoMsg[262144];
};

#endif
