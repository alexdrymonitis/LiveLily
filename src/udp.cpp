#include "udp.h"
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mutex>
using SocketHandle = SOCKET;
#define LIVELILY_INVALID_SOCKET INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
#define LIVELILY_INVALID_SOCKET (-1)
#endif

static_assert(sizeof(sockaddr_in) <= 16, "the destination of UdpSocket holds a sockaddr_in");

namespace {

#ifdef _WIN32
// Winsock is started once, the first time a socket is made, and stays for the life of the program
void startSockets()
{
	static std::once_flag once;
	std::call_once(once, [] {
		WSADATA data;
		WSAStartup(MAKEWORD(2, 2), &data);
	});
}
void closeSocket(SocketHandle s) { closesocket(s); }
bool setNonBlocking(SocketHandle s)
{
	u_long on = 1;
	return ioctlsocket(s, FIONBIO, &on) == 0;
}
#else
void startSockets() {}
void closeSocket(SocketHandle s) { ::close(s); }
bool setNonBlocking(SocketHandle s)
{
	int flags = fcntl(s, F_GETFL, 0);
	return flags != -1 && fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
}
#endif

SocketHandle toSocket(long long handle) { return (SocketHandle)handle; }

}

UdpSocket::~UdpSocket()
{
	close();
}

bool UdpSocket::isOpen() const
{
	return toSocket(handle) != LIVELILY_INVALID_SOCKET;
}

void UdpSocket::close()
{
	if (isOpen()) closeSocket(toSocket(handle));
	handle = (long long)LIVELILY_INVALID_SOCKET;
	hasDestination = false;
}

bool UdpSocket::openSender(const std::string& host, int port)
{
	close();
	if (host.empty() || port < 0 || port > 65535) return false;
	startSockets();
	addrinfo hints{};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	addrinfo* found = nullptr;
	if (getaddrinfo(host.c_str(), nullptr, &hints, &found) != 0 || found == nullptr) return false;
	sockaddr_in address;
	std::memcpy(&address, found->ai_addr, sizeof(address));
	freeaddrinfo(found);
	address.sin_port = htons((unsigned short)port);

	SocketHandle s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == LIVELILY_INVALID_SOCKET) return false;
	int on = 1;
	setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&on, sizeof(on));
	handle = (long long)s;
	std::memcpy(destination, &address, sizeof(address));
	hasDestination = true;
	return true;
}

bool UdpSocket::send(const std::string& datagram)
{
	if (!isOpen() || !hasDestination) return false;
	auto sent = sendto(toSocket(handle), datagram.data(), (int)datagram.size(), 0, (const sockaddr*)destination, (int)sizeof(sockaddr_in));
	return sent == (decltype(sent))datagram.size();
}

bool UdpSocket::openReceiver(int port)
{
	close();
	if (port < 0 || port > 65535) return false;
	startSockets();
	SocketHandle s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == LIVELILY_INVALID_SOCKET) return false;
	int on = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
#ifdef SO_REUSEPORT
	setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (const char*)&on, sizeof(on));
#endif
	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons((unsigned short)port);
	if (bind(s, (const sockaddr*)&address, sizeof(address)) != 0 || !setNonBlocking(s)) {
		closeSocket(s);
		return false;
	}
	handle = (long long)s;
	return true;
}

bool UdpSocket::receive(std::string& datagram)
{
	if (!isOpen() || hasDestination) return false;
	// the largest datagram of UDP over IPv4
	char buffer[65536];
	auto received = recv(toSocket(handle), buffer, (int)sizeof(buffer), 0);
	// nothing has arrived (or an error, e.g. on Windows the refusal of an earlier datagram, which loses nothing)
	if (received < 0) return false;
	datagram.assign(buffer, (size_t)received);
	return true;
}
