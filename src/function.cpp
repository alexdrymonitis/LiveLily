#include <cstring> // strcpy
#include "function.h"

/**************************************************************/
/******************** Function class **************************/
//--------------------------------------------------------------
Function::Function()
{
	definitionPane = definitionLine = -1;
	callingStep = initCallingStep = 1;
	stepIncrement = 0;
	stepCounter = 0;
	sequencerStep = 0;
	repetitionCounter = 0;
	numRepeatTimesSet = false;
	boundInstNdx = -1;
	numArgs = 0;
	nameError = false;
	argErrorType = 0;
	onUnbindFuncNdx = -1;
	onUnbindSet = false;
}

//--------------------------------------------------------------
void Function::appendLine(const std::string& line)
{
	if (!body.empty()) body += '\n';
	body += line;
}

//--------------------------------------------------------------
std::string Function::printStr()
{
	return body;
}

//--------------------------------------------------------------
void Function::setIndex(int index)
{
	funcNdx = index;
}

//--------------------------------------------------------------
int Function::getIndex()
{
	return funcNdx;
}

//--------------------------------------------------------------
void Function::setName(std::string funcName)
{
	if (funcName.size() > 50) {
		nameError = true;
		return;
	}
	nameError = false;
	strcpy(name, funcName.c_str());
}

//--------------------------------------------------------------
std::string Function::getName()
{
	std::string str(name);
	return str;
}

//--------------------------------------------------------------
bool Function::getNameError()
{
	return nameError;
}

//--------------------------------------------------------------
void Function::setArgument(std::string str)
{
	if (numArgs > 50) {
		argErrorType = 1;
		return;
	}
	if (str.size() > 20) {
		argErrorType = 2;
		return;
	}
	argErrorType = 0;
	strcpy(arguments[numArgs++], str.c_str());
}

//--------------------------------------------------------------
void Function::resetArgumentIndex()
{
	numArgs = 0;
}

//--------------------------------------------------------------
int Function::getNumArgs()
{
	return numArgs;
}

//--------------------------------------------------------------
std::string Function::getArgument(int ndx)
{
	if (ndx >= 50) return "";
	std::string str(arguments[ndx]);
	return str;
}

//--------------------------------------------------------------
int Function::getArgError()
{
	return argErrorType;
}

//--------------------------------------------------------------
void Function::setBind(int boundInstindex, int callStep, int stepIncr, int numTimes)
{
	boundInstNdx = boundInstindex;
	// - 1 because LiveLily is 1-based, but C++ is 0-based
	callingStep = callStep - 1;
	initCallingStep = callingStep;
	stepIncrement = stepIncr;
	numRepeatTimes = numTimes;
	if (numRepeatTimes > 0) numRepeatTimesSet = true;
}

//--------------------------------------------------------------
int Function::releaseBind()
{
	boundInstNdx = -1;
	stepIncrement = 0;
	stepCounter = 0;
	repetitionCounter = 0;
	numRepeatTimesSet = false;
	if (onUnbindSet) return onUnbindFuncNdx;
	else return -1;
}

//--------------------------------------------------------------
void Function::onUnbindFunc(int funcNdx)
{
	onUnbindFuncNdx = funcNdx;
	onUnbindSet = true;
}

//--------------------------------------------------------------
int Function::getBoundInst()
{
	return boundInstNdx;
}

//--------------------------------------------------------------
int Function::getCallingStep()
{
	return callingStep;
}

//--------------------------------------------------------------
void Function::addStepIncrement(int seqStep)
{
	callingStep += stepIncrement;
	sequencerStep = seqStep + 1; // + 1 so that the modulo in resetCallingStep() works properly
}

//--------------------------------------------------------------
void Function::resetCallingStep()
{
	if (initCallingStep != callingStep && callingStep > 0) {
		callingStep %= sequencerStep;
		initCallingStep = callingStep;
	}
}

//--------------------------------------------------------------
int Function::incrementCallCounter()
{
	int releaseBindFuncNdx = -1;
	if (numRepeatTimesSet) {
		repetitionCounter++;
		if (repetitionCounter == numRepeatTimes) {
			releaseBindFuncNdx = releaseBind();
		}
	}
	return releaseBindFuncNdx;
}

//--------------------------------------------------------------
void Function::setStepCounter(int num)
{
	// gets the sequence steps of the bound instrument
	stepCounter = num;
}

//--------------------------------------------------------------
void Function::clear()
{
	body.clear();
}

//--------------------------------------------------------------
void Function::setDefinitionLocation(int pane, int line)
{
	definitionPane = pane;
	definitionLine = line;
}

//--------------------------------------------------------------
int Function::getDefinitionPane()
{
	return definitionPane;
}

//--------------------------------------------------------------
int Function::getDefinitionLine()
{
	return definitionLine;
}
