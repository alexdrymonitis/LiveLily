#ifndef PATHS_H
#define PATHS_H

// where LiveLily finds its files on every system (see doc/stage3_inventory.md):
// its resources (fonts, help, logo), which ship with it, and the search paths, where \help looks for help files, in this order:
//   1. the user's folder, Documents/LiveLily, the same on every system (like Pd's documents folder)
//   2. a folder for all the users: /usr/local/share/livelily (Linux), /Library/LiveLily (macOS), %ProgramData%\LiveLily (Windows)
//   3. the resources that ship with LiveLily
// the first one that has a file wins, so a user can put a file of their own in place of one that ships

#include <filesystem>
#include <string>
#include <vector>

namespace Paths {
	// the path of the running program (argv0 is used where the system can't tell)
	std::filesystem::path executable(const char* argv0 = nullptr);
	// the resources that ship with LiveLily, found from the program's path: next to it (Windows, Linux portable), in a macOS bundle
	// (Contents/Resources), in ../share/livelily (installed on Linux), or in ../resources (bin/ of the source, while developing)
	std::filesystem::path resources(const std::filesystem::path& executablePath);
	// the user's Documents folder (wherever the system keeps it), or the home directory if there is none
	std::filesystem::path documents();
	// the user's folder of LiveLily, Documents/LiveLily
	std::filesystem::path userFolder();
	// the folder of LiveLily for all the users of the machine
	std::filesystem::path systemFolder();
	// the search paths, in the order they are searched
	std::vector<std::filesystem::path> searchPaths(const std::filesystem::path& resourcesPath);
	// the first file a relative path (e.g. help/insts-help.lyv) names in the search paths, or an empty path
	std::filesystem::path find(const std::vector<std::filesystem::path>& searchPaths, const std::filesystem::path& relative);
}

#endif
