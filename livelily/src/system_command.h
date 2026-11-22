#ifndef SYSTEM_COMMAND_H
#define SYSTEM_COMMAND_H

#include <string>

// runs a command of the system's shell and returns what it prints to the standard output (as openFrameworks' ofSystem() did)
// used by \system and by the shell of the editor (e.g. :ls and :pwd)
std::string systemCommandOutput(const std::string& command);

#endif
