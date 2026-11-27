#ifndef OSC_H
#define OSC_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// OSC in LiveLily: messages, and the senders and receivers that the platform implements
// (in-house, over UDP, see osc_udp.cpp)

enum class OscArgType { Int32, Int64, Float, Double, String, Symbol, True, False, Other, IndexOutOfBounds };

// an OSC message, with the method names of ofxOscMessage, and its conversions between the types of its arguments
// (a number read as another kind of number or as a string, and a number, a boolean, or the string "true" read as a boolean)
class OscMessage
{
	public:
		void setAddress(const std::string& address) { this->address = address; }
		const std::string& getAddress() const { return address; }
		void addIntArg(int32_t value);
		void addInt64Arg(int64_t value);
		void addFloatArg(float value);
		void addDoubleArg(double value);
		void addStringArg(const std::string& value);
		void addSymbolArg(const std::string& value);
		void addBoolArg(bool value);
		// an argument the platform received but LiveLily doesn't read (e.g. a blob), which keeps the places of the others
		void addOtherArg();
		size_t getNumArgs() const { return args.size(); }
		OscArgType getArgType(size_t index) const;
		int32_t getArgAsInt32(size_t index) const;
		int64_t getArgAsInt64(size_t index) const;
		float getArgAsFloat(size_t index) const;
		double getArgAsDouble(size_t index) const;
		std::string getArgAsString(size_t index) const;
		std::string getArgAsSymbol(size_t index) const { return getArgAsString(index); }
		bool getArgAsBool(size_t index) const;
		void clear();
	private:
		struct Arg {
			OscArgType type;
			int64_t i = 0;
			double d = 0;
			std::string s;
		};
		std::string address;
		std::vector<Arg> args;
};

class OscSender
{
	public:
		virtual ~OscSender() = default;
		virtual bool setup(const std::string& host, int port) = 0;
		virtual void send(const OscMessage& m) = 0;
};

class OscReceiver
{
	public:
		virtual ~OscReceiver() = default;
		// returns false if it can't listen on the port (e.g. it is taken)
		virtual bool setup(int port) = 0;
		virtual bool hasWaitingMessages() = 0;
		virtual bool getNextMessage(OscMessage& m) = 0;
};

// osc_udp.cpp makes these
// they are shared, so that the objects that hold them (e.g. instruments) can be copied
std::shared_ptr<OscSender> makeOscSender();
std::shared_ptr<OscReceiver> makeOscReceiver();

#endif
