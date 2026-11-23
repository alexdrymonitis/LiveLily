#include "paths.h"
#include "system_command.h"
#include <cstdlib>
#include <fstream>
#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

fs::path Paths::executable(const char* argv0)
{
#if defined(_WIN32)
	wchar_t buffer[MAX_PATH];
	DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
	if (length > 0 && length < MAX_PATH) return fs::path(std::wstring(buffer, length));
#elif defined(__APPLE__)
	char buffer[4096];
	uint32_t size = sizeof(buffer);
	if (_NSGetExecutablePath(buffer, &size) == 0) return fs::weakly_canonical(buffer);
#else
	std::error_code error;
	fs::path self = fs::read_symlink("/proc/self/exe", error);
	if (!error) return self;
#endif
	return (argv0 ? fs::absolute(argv0) : fs::current_path());
}

fs::path Paths::resources(const fs::path& executablePath)
{
	fs::path dir = executablePath.parent_path();
	const fs::path candidates[] = {dir / "resources", dir / ".." / "Resources", dir / ".." / "share" / "livelily", dir / ".." / "resources"};
	for (const fs::path& candidate : candidates) {
		std::error_code error;
		if (fs::is_directory(candidate, error)) return fs::weakly_canonical(candidate);
	}
	return dir / "resources";
}

fs::path Paths::documents()
{
#if defined(_WIN32)
	PWSTR path = nullptr;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &path))) {
		fs::path documentsPath(path);
		CoTaskMemFree(path);
		return documentsPath;
	}
	if (path) CoTaskMemFree(path);
	return fs::path(homeDirectory()) / "Documents";
#else
	fs::path home = homeDirectory();
#if !defined(__APPLE__)
	// the Documents folder can have another name (e.g. in another language), which the XDG user directories give
	const char* configHome = std::getenv("XDG_CONFIG_HOME");
	fs::path dirsFile = (configHome && *configHome ? fs::path(configHome) : home / ".config") / "user-dirs.dirs";
	std::ifstream dirs(dirsFile);
	std::string line;
	while (std::getline(dirs, line)) {
		const std::string key = "XDG_DOCUMENTS_DIR=\"";
		if (line.compare(0, key.size(), key) != 0 || line.back() != '"') continue;
		std::string value = line.substr(key.size(), line.size() - key.size() - 1);
		if (value.compare(0, 5, "$HOME") == 0) return home / value.substr(value.size() > 5 && value[5] == '/' ? 6 : 5);
		if (!value.empty() && value[0] == '/') return fs::path(value);
	}
#endif
	return home / "Documents";
#endif
}

fs::path Paths::userFolder()
{
	return documents() / "LiveLily";
}

fs::path Paths::systemFolder()
{
#if defined(_WIN32)
	const char* programData = std::getenv("ProgramData");
	return fs::path(programData ? programData : "C:\\ProgramData") / "LiveLily";
#elif defined(__APPLE__)
	return "/Library/LiveLily";
#else
	return "/usr/local/share/livelily";
#endif
}

std::vector<fs::path> Paths::searchPaths(const fs::path& resourcesPath)
{
	return {userFolder(), systemFolder(), resourcesPath};
}

fs::path Paths::find(const std::vector<fs::path>& searchPaths, const fs::path& relative)
{
	for (const fs::path& dir : searchPaths) {
		std::error_code error;
		fs::path candidate = dir / relative;
		if (fs::is_regular_file(candidate, error)) return candidate;
	}
	return fs::path();
}
