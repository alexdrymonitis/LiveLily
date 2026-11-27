// the serial port of LiveLily (see serial.h), in-house: termios on Linux and macOS, the Win32 API on Windows
// the ports are 8 data bits, no parity, one stop bit, with no flow control, and LiveLily only writes to them
//
// the devices are those ofSerial listed: on Linux /dev/ttyACM*, /dev/ttyUSB*, /dev/rfcomm* and /dev/ttyS*, in this order,
// so that boards on USB come before the serial ports of the motherboard (often 32 of them, ttyS0 to ttyS31),
// on macOS /dev/cu.* and /dev/tty.* (those of Arduinos, usbserial and usbmodem, first), and on Windows the COM ports
// the system has (in the registry, e.g. COM3); the numbers in their names are in numerical order (ttyS2 before ttyS10)
#include "serial.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <filesystem>
#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/serial.h>
#include <sys/ioctl.h>
#endif
#endif

namespace {

#ifndef _WIN32
// the speeds of termios, and 9600 for any other, as ofSerial did
speed_t termiosSpeed(int baud)
{
	switch (baud) {
		case 300: return B300;
		case 1200: return B1200;
		case 2400: return B2400;
		case 4800: return B4800;
		case 9600: return B9600;
		case 19200: return B19200;
		case 38400: return B38400;
		case 57600: return B57600;
		case 115200: return B115200;
		case 230400: return B230400;
#ifdef B460800
		case 460800: return B460800;
#endif
#ifdef B921600
		case 921600: return B921600;
#endif
		default: return B9600;
	}
}

bool startsWith(const std::string& s, const std::string& prefix)
{
	return s.size() > prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}
#endif

// names in order, with the numbers in them in numerical order (ttyS2 before ttyS10, COM3 before COM10)
bool naturalLess(const std::string& a, const std::string& b)
{
	size_t i = 0, j = 0;
	while (i < a.size() && j < b.size()) {
		if (isdigit((unsigned char)a[i]) && isdigit((unsigned char)b[j])) {
			size_t iEnd = i, jEnd = j;
			while (iEnd < a.size() && isdigit((unsigned char)a[iEnd])) iEnd++;
			while (jEnd < b.size() && isdigit((unsigned char)b[jEnd])) jEnd++;
			unsigned long long x = std::stoull(a.substr(i, std::min<size_t>(iEnd - i, 18)));
			unsigned long long y = std::stoull(b.substr(j, std::min<size_t>(jEnd - j, 18)));
			if (x != y) return x < y;
			i = iEnd;
			j = jEnd;
		}
		else {
			if (a[i] != b[j]) return a[i] < b[j];
			i++;
			j++;
		}
	}
	return (a.size() - i) < (b.size() - j);
}

class SystemSerialPort : public SerialPort
{
	public:
		~SystemSerialPort() override { close(); }

		std::vector<SerialDevice> devices() override
		{
			std::vector<std::string> paths;
#ifdef _WIN32
			HKEY key;
			if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &key) == ERROR_SUCCESS) {
				for (DWORD i = 0;; i++) {
					char name[256], value[256];
					DWORD nameSize = sizeof(name), valueSize = sizeof(value), type = 0;
					if (RegEnumValueA(key, i, name, &nameSize, nullptr, &type, (LPBYTE)value, &valueSize) != ERROR_SUCCESS) break;
					if (type == REG_SZ) paths.push_back(std::string(value, strnlen(value, valueSize)));
				}
				RegCloseKey(key);
			}
			std::sort(paths.begin(), paths.end(), naturalLess);
#else
#ifdef __APPLE__
			const std::vector<std::string> prefixes = {"cu.", "tty."};
#else
			const std::vector<std::string> prefixes = {"ttyACM", "ttyUSB", "rfcomm", "ttyS"};
#endif
			// the devices of every prefix, in the order of the prefixes
			std::vector<std::vector<std::string>> byPrefix(prefixes.size());
			std::error_code error;
			for (const auto& entry : std::filesystem::directory_iterator("/dev", error)) {
				std::string name = entry.path().filename().string();
				for (size_t p = 0; p < prefixes.size(); p++) {
					if (startsWith(name, prefixes[p])) {
						byPrefix[p].push_back("/dev/" + name);
						break;
					}
				}
			}
			for (std::vector<std::string>& group : byPrefix) {
				std::sort(group.begin(), group.end(), naturalLess);
				paths.insert(paths.end(), group.begin(), group.end());
			}
#ifdef __APPLE__
			std::stable_partition(paths.begin(), paths.end(), [](const std::string& path) {
				return path.find("usbserial") != std::string::npos || path.find("usbmodem") != std::string::npos;
			});
#endif
#endif
			std::vector<SerialDevice> list;
			for (size_t i = 0; i < paths.size(); i++) list.push_back({(int)i, paths[i]});
			return list;
		}

		bool open(const std::string& path, int baudRate) override
		{
			close();
#ifdef _WIN32
			// COM10 and above can only be opened with the prefix of devices
			std::string device = (path.rfind("\\\\.\\", 0) == 0 ? path : "\\\\.\\" + path);
			HANDLE h = CreateFileA(device.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
			if (h == INVALID_HANDLE_VALUE) return false;
			DCB dcb{};
			dcb.DCBlength = sizeof(dcb);
			if (!GetCommState(h, &dcb)) {
				CloseHandle(h);
				return false;
			}
			dcb.BaudRate = (DWORD)baudRate;
			dcb.ByteSize = 8;
			dcb.Parity = NOPARITY;
			dcb.StopBits = ONESTOPBIT;
			dcb.fBinary = TRUE;
			dcb.fParity = FALSE;
			dcb.fOutxCtsFlow = FALSE;
			dcb.fOutxDsrFlow = FALSE;
			dcb.fDtrControl = DTR_CONTROL_ENABLE;
			dcb.fRtsControl = RTS_CONTROL_ENABLE;
			dcb.fOutX = FALSE;
			dcb.fInX = FALSE;
			if (!SetCommState(h, &dcb)) {
				CloseHandle(h);
				return false;
			}
			// a write waits at most a second, so that a device that doesn't read can't stop LiveLily
			COMMTIMEOUTS timeouts{};
			timeouts.ReadIntervalTimeout = MAXDWORD;
			timeouts.WriteTotalTimeoutConstant = 1000;
			SetCommTimeouts(h, &timeouts);
			handle = h;
			return true;
#else
			int f = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
			if (f < 0) return false;
			termios options;
			if (tcgetattr(f, &options) != 0) {
				::close(f);
				return false;
			}
			// raw bytes: no translation of characters, no echo, no signals
			cfmakeraw(&options);
			options.c_cflag |= (CLOCAL | CREAD);
			options.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
			options.c_cflag |= CS8;
			options.c_iflag &= ~(IXON | IXOFF | IXANY);
			speed_t speed = termiosSpeed(baudRate);
			cfsetispeed(&options, speed);
			cfsetospeed(&options, speed);
			if (tcsetattr(f, TCSANOW, &options) != 0) {
				::close(f);
				return false;
			}
#ifdef __linux__
			// the bytes leave without the driver's delay, as ofSerial asked for
			serial_struct settings;
			if (ioctl(f, TIOCGSERIAL, &settings) == 0) {
				settings.flags |= ASYNC_LOW_LATENCY;
				ioctl(f, TIOCSSERIAL, &settings);
			}
#endif
			fd = f;
			return true;
#endif
		}

		bool isOpen() override
		{
#ifdef _WIN32
			return handle != INVALID_HANDLE_VALUE;
#else
			return fd >= 0;
#endif
		}

		void writeByte(unsigned char byte) override { write((const char*)&byte, 1); }
		void writeBytes(const std::string& bytes) override { write(bytes.data(), bytes.size()); }

		void close() override
		{
#ifdef _WIN32
			if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
			handle = INVALID_HANDLE_VALUE;
#else
			if (fd >= 0) ::close(fd);
			fd = -1;
#endif
		}

	private:
#ifdef _WIN32
		HANDLE handle = INVALID_HANDLE_VALUE;
#else
		int fd = -1;
#endif

		void write(const char* data, size_t size)
		{
			if (!isOpen()) return;
#ifdef _WIN32
			DWORD written = 0;
			WriteFile(handle, data, (DWORD)size, &written, nullptr);
#else
			// the port doesn't block, so when the device's buffer is full, this waits until it takes more, at most a second, as ofSerial did
			// (with ten seconds)
			size_t done = 0;
			while (done < size) {
				ssize_t n = ::write(fd, data + done, size - done);
				if (n > 0) {
					done += (size_t)n;
					continue;
				}
				if (n < 0 && errno != EAGAIN && errno != EINTR) return;
				fd_set fds;
				FD_ZERO(&fds);
				FD_SET(fd, &fds);
				timeval timeout{1, 0};
				int ready = select(fd + 1, nullptr, &fds, nullptr, &timeout);
				if (ready == 0 || (ready < 0 && errno != EINTR)) return;
			}
#endif
		}
};

}

std::unique_ptr<SerialPort> makeSerialPort()
{
	return std::make_unique<SystemSerialPort>();
}
