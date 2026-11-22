// the serial port of LiveLily (see serial.h) with openFrameworks' ofSerial
#include "serial.h"
#include "ofSerial.h"

namespace {

class OfSerialPort : public SerialPort
{
	public:
		std::vector<SerialDevice> devices() override
		{
			// listDevices() also prints the devices to the console, as before
			serial.listDevices();
			std::vector<SerialDevice> list;
			for (auto& device : serial.getDeviceList()) list.push_back({device.getDeviceID(), device.getDevicePath()});
			return list;
		}
		bool open(const std::string& path, int baudRate) override { return serial.setup(path, baudRate); }
		bool isOpen() override { return serial.isInitialized(); }
		void writeByte(unsigned char byte) override { serial.writeByte(byte); }
		void writeBytes(const std::string& bytes) override { serial.writeBytes(bytes.data(), bytes.size()); }
		void close() override { serial.close(); }
	private:
		ofSerial serial;
};

}

std::unique_ptr<SerialPort> makeSerialPort()
{
	return std::make_unique<OfSerialPort>();
}
