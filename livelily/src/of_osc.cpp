// the OSC senders and receivers of LiveLily (see osc.h) with openFrameworks' ofxOsc
#include "osc.h"
#include "ofxOsc.h"

namespace {

ofxOscMessage toOfx(const OscMessage& m)
{
	ofxOscMessage out;
	out.setAddress(m.getAddress());
	for (size_t i = 0; i < m.getNumArgs(); i++) {
		switch (m.getArgType(i)) {
			case OscArgType::Int32: out.addIntArg(m.getArgAsInt32(i)); break;
			case OscArgType::Int64: out.addInt64Arg(m.getArgAsInt64(i)); break;
			case OscArgType::Float: out.addFloatArg(m.getArgAsFloat(i)); break;
			case OscArgType::Double: out.addDoubleArg(m.getArgAsDouble(i)); break;
			case OscArgType::String: out.addStringArg(m.getArgAsString(i)); break;
			case OscArgType::Symbol: out.addSymbolArg(m.getArgAsString(i)); break;
			case OscArgType::True: out.addBoolArg(true); break;
			case OscArgType::False: out.addBoolArg(false); break;
			default: break;
		}
	}
	return out;
}

OscMessage fromOfx(const ofxOscMessage& m)
{
	OscMessage out;
	out.setAddress(m.getAddress());
	for (size_t i = 0; i < m.getNumArgs(); i++) {
		switch (m.getArgType(i)) {
			case OFXOSC_TYPE_INT32: out.addIntArg(m.getArgAsInt32(i)); break;
			case OFXOSC_TYPE_INT64: out.addInt64Arg(m.getArgAsInt64(i)); break;
			case OFXOSC_TYPE_FLOAT: out.addFloatArg(m.getArgAsFloat(i)); break;
			case OFXOSC_TYPE_DOUBLE: out.addDoubleArg(m.getArgAsDouble(i)); break;
			case OFXOSC_TYPE_STRING: out.addStringArg(m.getArgAsString(i)); break;
			case OFXOSC_TYPE_SYMBOL: out.addSymbolArg(m.getArgAsSymbol(i)); break;
			case OFXOSC_TYPE_TRUE: out.addBoolArg(true); break;
			case OFXOSC_TYPE_FALSE: out.addBoolArg(false); break;
			default: out.addOtherArg(); break;
		}
	}
	return out;
}

class OfOscSender : public OscSender
{
	public:
		bool setup(const std::string& host, int port) override { return sender.setup(host, port); }
		void send(const OscMessage& m) override { sender.sendMessage(toOfx(m), false); }
	private:
		ofxOscSender sender;
};

class OfOscReceiver : public OscReceiver
{
	public:
		bool setup(int port) override { return receiver.setup(port); }
		bool hasWaitingMessages() override { return receiver.hasWaitingMessages(); }
		bool getNextMessage(OscMessage& m) override
		{
			ofxOscMessage received;
			if (!receiver.getNextMessage(received)) return false;
			m = fromOfx(received);
			return true;
		}
	private:
		ofxOscReceiver receiver;
};

}

std::shared_ptr<OscSender> makeOscSender()
{
	return std::make_shared<OfOscSender>();
}

std::shared_ptr<OscReceiver> makeOscReceiver()
{
	return std::make_shared<OfOscReceiver>();
}
