#include "osc.h"
#include <sstream>

namespace {
	// a number as a string, as openFrameworks' ofToString() writes it, which ofxOscMessage used
	template <typename T>
	std::string numberString(T value)
	{
		std::ostringstream out;
		out << value;
		return out.str();
	}
}

void OscMessage::addIntArg(int32_t value)
{
	Arg arg{OscArgType::Int32};
	arg.i = value;
	args.push_back(arg);
}

void OscMessage::addInt64Arg(int64_t value)
{
	Arg arg{OscArgType::Int64};
	arg.i = value;
	args.push_back(arg);
}

void OscMessage::addFloatArg(float value)
{
	Arg arg{OscArgType::Float};
	arg.d = value;
	args.push_back(arg);
}

void OscMessage::addDoubleArg(double value)
{
	Arg arg{OscArgType::Double};
	arg.d = value;
	args.push_back(arg);
}

void OscMessage::addStringArg(const std::string& value)
{
	Arg arg{OscArgType::String};
	arg.s = value;
	args.push_back(arg);
}

void OscMessage::addSymbolArg(const std::string& value)
{
	Arg arg{OscArgType::Symbol};
	arg.s = value;
	args.push_back(arg);
}

void OscMessage::addBoolArg(bool value)
{
	args.push_back(Arg{value ? OscArgType::True : OscArgType::False});
}

void OscMessage::addOtherArg()
{
	args.push_back(Arg{OscArgType::Other});
}

OscArgType OscMessage::getArgType(size_t index) const
{
	return (index < args.size() ? args[index].type : OscArgType::IndexOutOfBounds);
}

int64_t OscMessage::getArgAsInt64(size_t index) const
{
	switch (getArgType(index)) {
		case OscArgType::Int32:
		case OscArgType::Int64: return args[index].i;
		case OscArgType::Float: return (int64_t)(float)args[index].d;
		case OscArgType::Double: return (int64_t)args[index].d;
		case OscArgType::True: return 1;
		default: return 0;
	}
}

int32_t OscMessage::getArgAsInt32(size_t index) const
{
	switch (getArgType(index)) {
		case OscArgType::Int32:
		case OscArgType::Int64: return (int32_t)args[index].i;
		case OscArgType::Float: return (int32_t)(float)args[index].d;
		case OscArgType::Double: return (int32_t)args[index].d;
		case OscArgType::True: return 1;
		default: return 0;
	}
}

double OscMessage::getArgAsDouble(size_t index) const
{
	switch (getArgType(index)) {
		case OscArgType::Int32:
		case OscArgType::Int64: return (double)args[index].i;
		case OscArgType::Float:
		case OscArgType::Double: return args[index].d;
		case OscArgType::True: return 1;
		default: return 0;
	}
}

float OscMessage::getArgAsFloat(size_t index) const
{
	switch (getArgType(index)) {
		case OscArgType::Int32:
		case OscArgType::Int64: return (float)args[index].i;
		case OscArgType::Float:
		case OscArgType::Double: return (float)args[index].d;
		case OscArgType::True: return 1;
		default: return 0;
	}
}

std::string OscMessage::getArgAsString(size_t index) const
{
	switch (getArgType(index)) {
		case OscArgType::String:
		case OscArgType::Symbol: return args[index].s;
		case OscArgType::Int32: return numberString((int32_t)args[index].i);
		case OscArgType::Int64: return numberString(args[index].i);
		case OscArgType::Float: return numberString((float)args[index].d);
		case OscArgType::Double: return numberString(args[index].d);
		case OscArgType::True: return "true";
		case OscArgType::False: return "false";
		default: return "";
	}
}

bool OscMessage::getArgAsBool(size_t index) const
{
	switch (getArgType(index)) {
		case OscArgType::True: return true;
		case OscArgType::False: return false;
		case OscArgType::Int32:
		case OscArgType::Int64: return args[index].i > 0;
		case OscArgType::Float:
		case OscArgType::Double: return args[index].d > 0;
		case OscArgType::String:
		case OscArgType::Symbol: return args[index].s == "true";
		default: return false;
	}
}

void OscMessage::clear()
{
	address.clear();
	args.clear();
}
