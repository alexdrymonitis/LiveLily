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
// (with openFrameworks' ofSerial for now, see of_serial.cpp)
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

// the platform makes it (of_serial.cpp for now)
std::unique_ptr<SerialPort> makeSerialPort();

#endif
