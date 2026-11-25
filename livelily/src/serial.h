#ifndef SERIAL_H
#define SERIAL_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

// a serial device of the system, with its number in the list of devices and its path (e.g. /dev/ttyACM0)
struct SerialDevice
{
	int id;
	std::string path;
};

// the serial port of LiveLily (see \openserialport), which the platform implements
// (in-house, see serial_port.cpp)
class SerialPort
{
	public:
		virtual ~SerialPort() = default;
		virtual std::vector<SerialDevice> devices() = 0;
		virtual bool open(const std::string& path, int baudRate) = 0;
		virtual bool isOpen() = 0;
		virtual void writeByte(unsigned char byte) = 0;
		virtual void writeBytes(const std::string& bytes) = 0;
		virtual void close() = 0;
};

// serial_port.cpp makes it
std::unique_ptr<SerialPort> makeSerialPort();

#endif
