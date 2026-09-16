#include "IniConfig.hpp"

#include <windows.h>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <mutex>
#include <vector>

namespace {

enum class Encoding {
	Utf8,
	Utf8Bom,
	Utf16Le,
	Utf16Be,
	Ansi
};

struct Line {
	std::wstring text;
	std::wstring ending;
};

std::mutex configMutex;

bool isValidUtf8(const char *data, int size)
{
	if (!size)
		return true;
	return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, size, nullptr, 0) != 0;
}

bool decodeBytes(const std::string &bytes, Encoding &encoding, std::wstring &text)
{
	const auto *raw = reinterpret_cast<const unsigned char *>(bytes.data());
	size_t offset = 0;
	UINT codePage = CP_UTF8;

	if (bytes.size() >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) {
		encoding = Encoding::Utf16Le;
		offset = 2;
		if ((bytes.size() - offset) % 2)
			return false;
		text.resize((bytes.size() - offset) / 2);
		for (size_t i = 0; i < text.size(); i++)
			text[i] = static_cast<wchar_t>(raw[offset + i * 2] | raw[offset + i * 2 + 1] << 8);
		return true;
	}
	if (bytes.size() >= 2 && raw[0] == 0xFE && raw[1] == 0xFF) {
		encoding = Encoding::Utf16Be;
		offset = 2;
		if ((bytes.size() - offset) % 2)
			return false;
		text.resize((bytes.size() - offset) / 2);
		for (size_t i = 0; i < text.size(); i++)
			text[i] = static_cast<wchar_t>(raw[offset + i * 2] << 8 | raw[offset + i * 2 + 1]);
		return true;
	}
	if (bytes.size() >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF) {
		encoding = Encoding::Utf8Bom;
		offset = 3;
	} else if (isValidUtf8(bytes.data(), static_cast<int>(bytes.size())))
		encoding = Encoding::Utf8;
	else {
		encoding = Encoding::Ansi;
		codePage = CP_ACP;
	}

	int count = MultiByteToWideChar(codePage, encoding == Encoding::Utf8 || encoding == Encoding::Utf8Bom ? MB_ERR_INVALID_CHARS : 0,
		bytes.data() + offset, static_cast<int>(bytes.size() - offset), nullptr, 0);
	if (count <= 0 && bytes.size() != offset)
		return false;
	text.resize(static_cast<size_t>(count));
	if (count)
		MultiByteToWideChar(codePage, encoding == Encoding::Utf8 || encoding == Encoding::Utf8Bom ? MB_ERR_INVALID_CHARS : 0,
			bytes.data() + offset, static_cast<int>(bytes.size() - offset), text.data(), count);
	return true;
}

bool encodeMultibyte(const std::wstring &text, UINT codePage, DWORD flags, std::string &bytes, bool *usedDefault = nullptr)
{
	BOOL replaced = FALSE;
	BOOL *replacedPtr = codePage == CP_UTF8 ? nullptr : &replaced;
	int count = WideCharToMultiByte(codePage, flags, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, replacedPtr);
	if (count <= 0 && !text.empty())
		return false;
	bytes.resize(static_cast<size_t>(count));
	if (count && WideCharToMultiByte(codePage, flags, text.data(), static_cast<int>(text.size()), bytes.data(), count, nullptr, replacedPtr) != count)
		return false;
	if (usedDefault)
		*usedDefault = replaced != FALSE;
	return true;
}

bool encodeText(const std::wstring &text, Encoding &encoding, std::string &bytes)
{
	bytes.clear();
	if (encoding == Encoding::Ansi) {
		bool replaced = false;
		if (!encodeMultibyte(text, CP_ACP, WC_NO_BEST_FIT_CHARS, bytes, &replaced))
			return false;
		if (!replaced)
			return true;
		encoding = Encoding::Utf8Bom;
	}
	if (encoding == Encoding::Utf8 || encoding == Encoding::Utf8Bom) {
		std::string body;
		if (!encodeMultibyte(text, CP_UTF8, WC_ERR_INVALID_CHARS, body))
			return false;
		if (encoding == Encoding::Utf8Bom)
			bytes.assign("\xEF\xBB\xBF", 3);
		bytes += body;
		return true;
	}
	bytes.reserve(2 + text.size() * 2);
	if (encoding == Encoding::Utf16Le)
		bytes.assign("\xFF\xFE", 2);
	else
		bytes.assign("\xFE\xFF", 2);
	for (wchar_t chr : text) {
		if (encoding == Encoding::Utf16Le) {
			bytes.push_back(static_cast<char>(chr & 0xFF));
			bytes.push_back(static_cast<char>((chr >> 8) & 0xFF));
		} else {
			bytes.push_back(static_cast<char>((chr >> 8) & 0xFF));
			bytes.push_back(static_cast<char>(chr & 0xFF));
		}
	}
	return true;
}

std::wstring trim(const std::wstring &value)
{
	auto first = value.find_first_not_of(L" \t");
	if (first == std::wstring::npos)
		return {};
	auto last = value.find_last_not_of(L" \t");
	return value.substr(first, last - first + 1);
}

bool equalsIgnoreCase(const std::wstring &left, const std::wstring &right)
{
	return _wcsicmp(left.c_str(), right.c_str()) == 0;
}

bool sectionName(const std::wstring &line, std::wstring &name)
{
	auto value = trim(line);
	if (value.size() < 2 || value.front() != L'[' || value.back() != L']')
		return false;
	name = trim(value.substr(1, value.size() - 2));
	return true;
}

std::vector<Line> splitLines(const std::wstring &text)
{
	std::vector<Line> lines;
	for (size_t pos = 0; pos < text.size();) {
		auto end = text.find_first_of(L"\r\n", pos);
		if (end == std::wstring::npos) {
			lines.push_back({text.substr(pos), {}});
			break;
		}
		auto next = end + 1;
		if (text[end] == L'\r' && next < text.size() && text[next] == L'\n')
			next++;
		lines.push_back({text.substr(pos, end - pos), text.substr(end, next - end)});
		pos = next;
	}
	if (text.empty())
		return {};
	return lines;
}

std::wstring updateLobbyValue(const std::wstring &text, const std::wstring &key, const std::wstring &value)
{
	auto lines = splitLines(text);
	std::wstring newline = L"\r\n";
	for (const auto &line : lines)
		if (!line.ending.empty()) {
			newline = line.ending;
			break;
		}

	size_t lobbyStart = lines.size();
	size_t lobbyEnd = lines.size();
	for (size_t i = 0; i < lines.size(); i++) {
		std::wstring section;
		if (!sectionName(lines[i].text, section))
			continue;
		if (lobbyStart == lines.size() && equalsIgnoreCase(section, L"Lobby"))
			lobbyStart = i;
		else if (lobbyStart != lines.size()) {
			lobbyEnd = i;
			break;
		}
	}
	if (lobbyStart == lines.size()) {
		if (!lines.empty() && lines.back().ending.empty())
			lines.back().ending = newline;
		if (!lines.empty() && !lines.back().text.empty())
			lines.push_back({L"", newline});
		lines.push_back({L"[Lobby]", newline});
		lines.push_back({key + L"=" + value, {}});
	} else {
		bool replaced = false;
		for (size_t i = lobbyStart + 1; i < lobbyEnd; i++) {
			auto first = lines[i].text.find_first_not_of(L" \t");
			if (first == std::wstring::npos || lines[i].text[first] == L';' || lines[i].text[first] == L'#')
				continue;
			auto equal = lines[i].text.find(L'=', first);
			if (equal == std::wstring::npos || !equalsIgnoreCase(trim(lines[i].text.substr(first, equal - first)), key))
				continue;
			auto valueStart = equal + 1;
			while (valueStart < lines[i].text.size() && (lines[i].text[valueStart] == L' ' || lines[i].text[valueStart] == L'\t'))
				valueStart++;
			lines[i].text.replace(valueStart, std::wstring::npos, value);
			replaced = true;
		}
		if (!replaced) {
			Line added{key + L"=" + value, newline};
			if (lobbyEnd == lines.size() && !lines.empty() && lines.back().ending.empty()) {
				lines.back().ending = newline;
				added.ending.clear();
			}
			lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(lobbyEnd), std::move(added));
		}
	}
	std::wstring result;
	for (const auto &line : lines)
		result += line.text + line.ending;
	return result;
}

bool writeAtomically(const wchar_t *path, const std::string &bytes)
{
	std::wstring target(path);
	auto slash = target.find_last_of(L"\\/");
	std::wstring folder = slash == std::wstring::npos ? L"." : target.substr(0, slash);
	wchar_t tempPath[MAX_PATH];
	if (!GetTempFileNameW(folder.c_str(), L"SLI", 0, tempPath))
		return false;

	HANDLE file = CreateFileW(tempPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	bool success = file != INVALID_HANDLE_VALUE;
	if (success) {
		DWORD written = 0;
		success = bytes.size() <= MAXDWORD && WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size();
		if (success)
			success = FlushFileBuffers(file) != FALSE;
		CloseHandle(file);
	}
	if (success) {
		DWORD attributes = GetFileAttributesW(path);
		if (attributes != INVALID_FILE_ATTRIBUTES)
			success = ReplaceFileW(path, tempPath, nullptr, REPLACEFILE_WRITE_THROUGH, nullptr, nullptr) != FALSE;
		else
			success = MoveFileExW(tempPath, path, MOVEFILE_WRITE_THROUGH) != FALSE;
	}
	if (!success)
		DeleteFileW(tempPath);
	return success;
}

}

namespace IniConfig {

bool writeLobbyString(const wchar_t *path, const std::wstring &key, const std::wstring &value)
{
	if (!path || key.empty() || key.find_first_of(L"=\r\n[]") != std::wstring::npos || value.find_first_of(L"\r\n") != std::wstring::npos)
		return false;
	std::lock_guard<std::mutex> lock(configMutex);
	std::ifstream input(path, std::ios::binary);
	std::string bytes;
	if (input) {
		bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
		input.close();
	} else if (GetLastError() != ERROR_FILE_NOT_FOUND && GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES)
		return false;

	Encoding encoding = bytes.empty() ? Encoding::Utf8Bom : Encoding::Utf8;
	std::wstring text;
	if (!decodeBytes(bytes, encoding, text))
		return false;
	// An ASCII-only file is valid both as ANSI and UTF-8. Once a non-ASCII
	// value is added, make the choice explicit so the next read cannot depend
	// on the user's active code page.
	if (encoding == Encoding::Utf8 && std::any_of(value.begin(), value.end(), [](wchar_t chr) { return chr > 0x7F; }))
		encoding = Encoding::Utf8Bom;
	text = updateLobbyValue(text, key, value);
	if (!encodeText(text, encoding, bytes))
		return false;
	return writeAtomically(path, bytes);
}

bool writeLobbyUnsigned(const wchar_t *path, const std::wstring &key, unsigned value)
{
	return writeLobbyString(path, key, std::to_wstring(value));
}

}
