#ifndef SYSTEM_COMMAND_H
#define SYSTEM_COMMAND_H

#include <string>

// runs a command of the system's shell and returns what it prints to the standard output (as openFrameworks' ofSystem() did)
// used by \system and by the shell of the editor (e.g. :ls and :pwd)
std::string systemCommandOutput(const std::string& command);

// the home directory of the user (HOME, or USERPROFILE on Windows), for paths that start with ~, or an empty string if it isn't set
std::string homeDirectory();

#endif
