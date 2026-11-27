// the OSC senders and receivers of LiveLily (see osc.h), with OSC packets (osc_packet.cpp) over UDP (udp.cpp)
#include "osc.h"
#include "osc_packet.h"
#include "udp.h"
#include <deque>
#include <mutex>

namespace {

// the sequencer's thread sends with the same sender the main thread may set up again, so both are guarded
class UdpOscSender : public OscSender
{
	public:
		bool setup(const std::string& host, int port) override
		{
			std::lock_guard<std::mutex> lock(mutex);
			return socket.openSender(host, port);
		}
		void send(const OscMessage& m) override
		{
			std::string packet = OscPacket::encode(m);
			std::lock_guard<std::mutex> lock(mutex);
			socket.send(packet);
		}
	private:
		std::mutex mutex;
		UdpSocket socket;
};

// the app reads the receivers in every frame, from its main thread, so a receiver reads what has arrived when it is asked
// and keeps the messages of a bundle until they are all read
class UdpOscReceiver : public OscReceiver
{
	public:
		bool setup(int port) override
		{
			waiting.clear();
			return socket.openReceiver(port);
		}
		bool hasWaitingMessages() override
		{
			readArrived();
			return !waiting.empty();
		}
		bool getNextMessage(OscMessage& m) override
		{
			readArrived();
			if (waiting.empty()) return false;
			m = waiting.front();
			waiting.pop_front();
			return true;
		}
	private:
		UdpSocket socket;
		std::deque<OscMessage> waiting;
		void readArrived()
		{
			std::string datagram;
			std::vector<OscMessage> messages;
			// malformed packets are dropped, as ofxOsc did
			while (socket.receive(datagram)) OscPacket::decode(datagram.data(), datagram.size(), messages);
			waiting.insert(waiting.end(), messages.begin(), messages.end());
		}
};

}

std::shared_ptr<OscSender> makeOscSender()
{
	return std::make_shared<UdpOscSender>();
}

std::shared_ptr<OscReceiver> makeOscReceiver()
{
	return std::make_shared<UdpOscReceiver>();
}
