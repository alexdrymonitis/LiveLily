#ifndef FUNCTION_H
#define FUNCTION_H

#include <string>

// class for storing data concerning livelily functions
class Function
{
	public:
		Function();
		void appendLine(const std::string& line);
		std::string printStr();
		void setIndex(int index);
		int getIndex();
		void setName(std::string funcName);
		std::string getName();
		bool getNameError();
		void setArgument(std::string str);
		void resetArgumentIndex();
		int getNumArgs();
		std::string getArgument(int ndx);
		int getArgError();
		void setBind(int boundInstindex, int callStep, int stepIncr, int numTimes);
		int releaseBind();
		void onUnbindFunc(int funcNdx);
		int getBoundInst();
		// changes the instrument (or other target) a function is bound to, when instruments are added or removed (-1 unbinds it)
		void setBoundInst(int ndx) { boundInstNdx = ndx; }
		int getCallingStep();
		void addStepIncrement(int seqStep);
		void resetCallingStep();
		int incrementCallCounter();
		void setStepCounter(int num);
		void clear();
		// the pane and line where the function was defined, used to show errors of bound functions on that line
		void setDefinitionLocation(int pane, int line);
		int getDefinitionPane();
		int getDefinitionLine();
	private:
		int funcNdx;
		int definitionPane;
		int definitionLine;
		int callingStep;
		int numRepeatTimes;
		bool numRepeatTimesSet;
		int repetitionCounter;
		int initCallingStep;
		int stepIncrement;
		int stepCounter;
		int sequencerStep;
		int boundInstNdx;
		int numArgs;
		bool nameError;
		int argErrorType;
		int onUnbindFuncNdx;
		bool onUnbindSet;
		// overhead for not needing to allocate memory durint runtime
		// up to 50 arguments and up to 20 chars for each arg (+1 for num terminating char)
		char arguments[50][21];
		char name[51]; // again overhead
		// the function body, with its lines separated by newline characters
		std::string body;
};

#endif
