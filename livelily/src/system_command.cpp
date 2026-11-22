#include "system_command.h"
#include <cstdio>

std::string systemCommandOutput(const std::string& command)
{
	std::string output;
#ifdef _WIN32
	FILE* pipe = _popen(command.c_str(), "r");
#else
	FILE* pipe = popen(command.c_str(), "r");
#endif
	if (!pipe) return output;
	char buffer[256];
	size_t numRead;
	while ((numRead = fread(buffer, 1, sizeof(buffer), pipe)) > 0) output.append(buffer, numRead);
#ifdef _WIN32
	_pclose(pipe);
#else
	pclose(pipe);
#endif
	return output;
}
