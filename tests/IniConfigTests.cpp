#include "../src/SokuMod/IniConfig.hpp"

#include <windows.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

static std::string readFile(const wchar_t *path)
{
	std::ifstream input(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

static void writeFile(const wchar_t *path, const std::string &bytes)
{
	std::ofstream output(path, std::ios::binary);
	output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

static std::string utf16(const std::string &ascii, bool littleEndian)
{
	std::string bytes = littleEndian ? std::string("\xFF\xFE", 2) : std::string("\xFE\xFF", 2);
	for (unsigned char chr : ascii) {
		bytes.push_back(littleEndian ? static_cast<char>(chr) : 0);
		bytes.push_back(littleEndian ? 0 : static_cast<char>(chr));
	}
	return bytes;
}

int wmain()
{
	wchar_t folder[MAX_PATH];
	wchar_t path[MAX_PATH];
	assert(GetTempPathW(MAX_PATH, folder));
	assert(GetTempFileNameW(folder, L"SLC", 0, path));

	writeFile(path, "[Lobby]\r\nQuickMessage1=old\r\n[Other]\r\nKeep=yes\r\n");
	assert(IniConfig::writeLobbyString(path, L"QuickMessage1", L"new"));
	auto bytes = readFile(path);
	assert(bytes == "[Lobby]\r\nQuickMessage1=new\r\n[Other]\r\nKeep=yes\r\n");

	writeFile(path, "# comment\n[Lobby]\nOne=1\n[Other]\nKeep=yes\n");
	assert(IniConfig::writeLobbyUnsigned(path, L"Two", 2));
	bytes = readFile(path);
	assert(bytes == "# comment\n[Lobby]\nOne=1\nTwo=2\n[Other]\nKeep=yes\n");

	writeFile(path, "[Other]\r\nKeep=yes");
	assert(IniConfig::writeLobbyString(path, L"QuickMessage1", L"hello"));
	bytes = readFile(path);
	assert(bytes == "[Other]\r\nKeep=yes\r\n\r\n[Lobby]\r\nQuickMessage1=hello");

	writeFile(path, "\xEF\xBB\xBF[Lobby]\r\nQuickMessage1=old\r\n");
	assert(IniConfig::writeLobbyString(path, L"QuickMessage1", L"\x4E2D\x6587"));
	bytes = readFile(path);
	assert(bytes.rfind("\xEF\xBB\xBF", 0) == 0);
	assert(bytes.find("\xE4\xB8\xAD\xE6\x96\x87") != std::string::npos);

	writeFile(path, "[Lobby]\r\nQuickMessage1=old\r\n");
	assert(IniConfig::writeLobbyString(path, L"QuickMessage1", L"\x4E2D\x6587"));
	bytes = readFile(path);
	assert(bytes.rfind("\xEF\xBB\xBF", 0) == 0);

	writeFile(path, utf16("[Lobby]\r\nValue=old\r\n", true));
	assert(IniConfig::writeLobbyString(path, L"Value", L"new"));
	bytes = readFile(path);
	assert(bytes.rfind("\xFF\xFE", 0) == 0);
	assert(bytes.find("n\0e\0w\0", 0, 6) != std::string::npos);

	writeFile(path, utf16("[Lobby]\r\nValue=old\r\n", false));
	assert(IniConfig::writeLobbyString(path, L"Value", L"new"));
	bytes = readFile(path);
	assert(bytes.rfind("\xFE\xFF", 0) == 0);
	assert(bytes.find("\0n\0e\0w", 0, 6) != std::string::npos);

	writeFile(path, "[Lobby]\r\nValue=one\r\nvalue=two\r\n");
	assert(IniConfig::writeLobbyString(path, L"VALUE", L"same"));
	bytes = readFile(path);
	assert(bytes == "[Lobby]\r\nValue=same\r\nvalue=same\r\n");

	assert(!IniConfig::writeLobbyString(path, L"Bad=Key", L"value"));
	assert(!IniConfig::writeLobbyString(path, L"Good", L"bad\nvalue"));

	std::string malformed("\xFF\xFE\x41", 3);
	writeFile(path, malformed);
	assert(!IniConfig::writeLobbyString(path, L"Value", L"unchanged"));
	assert(readFile(path) == malformed);

	DeleteFileW(path);
	assert(IniConfig::writeLobbyString(path, L"Created", L"\x4E2D\x6587"));
	bytes = readFile(path);
	assert(bytes.rfind("\xEF\xBB\xBF", 0) == 0);
	assert(bytes.find("[Lobby]\r\nCreated=") != std::string::npos);
	DeleteFileW(path);
	return 0;
}
