#ifndef SOKULOBBIES_INICONFIG_HPP
#define SOKULOBBIES_INICONFIG_HPP

#include <string>

namespace IniConfig {

// Updates a value in the [Lobby] section without changing the encoding of the
// rest of the file. Legacy ANSI files are upgraded to UTF-8 with a BOM when the
// new value cannot be represented by the active Windows code page.
bool writeLobbyString(const wchar_t *path, const std::wstring &key, const std::wstring &value);
bool writeLobbyUnsigned(const wchar_t *path, const std::wstring &key, unsigned value);

}

#endif
