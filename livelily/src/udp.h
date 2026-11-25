#ifndef UDP_H
#define UDP_H

#include <string>

// a UDP socket over IPv4, with the system's sockets (POSIX, or Winsock on Windows), for OSC (see osc_udp.cpp)
// a socket either sends to one host and port, or receives on a port
class UdpSocket
{
	public:
		UdpSocket() = default;
		~UdpSocket();
		UdpSocket(const UdpSocket&) = delete;
		UdpSocket& operator=(const UdpSocket&) = delete;

		// sends to a host (a name or an address) and a port, also to a broadcast address, as ofxOsc did
		// returns false if the host isn't found or the socket can't be made
		bool openSender(const std::string& host, int port);
		// sends one datagram, returns false if it couldn't be sent
		bool send(const std::string& datagram);

		// receives on a port of every interface, which other programs may share (as ofxOsc did)
		// returns false if the socket can't be made or bound
		bool openReceiver(int port);
		// the next datagram that has arrived, without waiting: returns false if there is none
		bool receive(std::string& datagram);

		bool isOpen() const;
		void close();

	private:
		// the socket of the system (an int on POSIX, a SOCKET on Windows), and the address it sends to
		long long handle = -1;
		unsigned char destination[16] = {};
		bool hasDestination = false;
};

#endif
