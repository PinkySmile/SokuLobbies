#include "OptionsMenu.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <shellapi.h>
#include <../directx/dinput.h>
#include "IniConfig.hpp"
#include "InputBox.hpp"
#include "LobbyData.hpp"
#include "LobbyMenu.hpp"
#include "createUTFTexture.hpp"
#include "data.hpp"

void displaySokuCursor(SokuLib::Vector2i pos, SokuLib::Vector2u size);

namespace {

std::string decimalValue(unsigned value)
{
	return "Custom (" + std::to_string(value) + ")";
}

std::string keyName(unsigned key)
{
	if (key == VK_RETURN)
		return "Enter";
	if (key == VK_SPACE)
		return "Space";
	UINT scan = MapVirtualKeyA(key, MAPVK_VK_TO_VSC);
	if (key == VK_LEFT || key == VK_UP || key == VK_RIGHT || key == VK_DOWN || key == VK_PRIOR || key == VK_NEXT || key == VK_END || key == VK_HOME || key == VK_INSERT || key == VK_DELETE)
		scan |= 0x100;
	char buffer[64];
	if (GetKeyNameTextA(static_cast<LONG>(scan << 16), buffer, sizeof(buffer)))
		return buffer;
	char fallback[16];
	std::snprintf(fallback, sizeof(fallback), "VK 0x%02X", key & 0xFF);
	return fallback;
}

bool parseUnsignedInput(const std::wstring &input, unsigned minimum, unsigned maximum, unsigned &value)
{
	auto first = input.find_first_not_of(L" \t");
	auto last = input.find_last_not_of(L" \t");
	if (first == std::wstring::npos)
		return false;
	auto text = input.substr(first, last - first + 1);
	if (text.empty() || !std::all_of(text.begin(), text.end(), iswdigit))
		return false;
	wchar_t *end = nullptr;
	auto parsed = std::wcstoul(text.c_str(), &end, 10);
	if (!end || *end || parsed < minimum || parsed > maximum)
		return false;
	value = static_cast<unsigned>(parsed);
	return true;
}

std::string colorValue(unsigned value)
{
	char buffer[24];
	std::snprintf(buffer, sizeof(buffer), "Custom (#%06X)", value & 0xFFFFFF);
	return buffer;
}

std::string lobbyHostLabel(const std::string &host)
{
	if (host.size() <= 28)
		return host;
	return host.substr(0, 25) + "...";
}

std::wstring readLobbyString(const wchar_t *key)
{
	wchar_t buffer[256];
	GetPrivateProfileStringW(L"Lobby", key, L"", buffer, sizeof(buffer) / sizeof(*buffer), profilePath);
	return buffer;
}

std::string hostIpLabel(const std::wstring &value)
{
	if (value.empty())
		return "Auto";
	std::string label;
	label.reserve(value.size());
	for (wchar_t chr : value)
		label.push_back(static_cast<char>(chr));
	return lobbyHostLabel(label);
}

bool parseHostIp(const std::wstring &input, std::wstring &host)
{
	auto first = input.find_first_not_of(L" \t");
	if (first == std::wstring::npos) {
		host.clear();
		return true;
	}
	auto last = input.find_last_not_of(L" \t");
	host = input.substr(first, last - first + 1);
	if (host.size() >= 256)
		return false;
	for (wchar_t chr : host)
		if (chr < 0x21 || chr > 0x7E || chr == L'/' || chr == L'\\' || chr == L'[' || chr == L']')
			return false;
	return true;
}

bool parseLobbyHost(const std::wstring &input, std::string &host)
{
	auto first = input.find_first_not_of(L" \t");
	auto last = input.find_last_not_of(L" \t");
	if (first == std::wstring::npos)
		return false;
	auto value = input.substr(first, last - first + 1);
	if (value.empty() || value.size() >= sizeof(servHost))
		return false;
	host.clear();
	host.reserve(value.size());
	for (wchar_t chr : value) {
		if (chr < 0x21 || chr > 0x7E || chr == L'/' || chr == L'\\' || chr == L'[' || chr == L']')
			return false;
		host.push_back(static_cast<char>(chr));
	}
	return true;
}

bool saveUnsigned(const wchar_t *key, unsigned value)
{
	return IniConfig::writeLobbyUnsigned(profilePath, key, value);
}

bool saveString(const wchar_t *key, const wchar_t *value)
{
	return IniConfig::writeLobbyString(profilePath, key, value);
}

std::wstring localizedOptionName(const std::string &name)
{
	if (!chineseLanguage)
		return std::wstring(name.begin(), name.end());
	static const std::map<std::string, std::wstring> labels = {
		{"Language", L"\u8BED\u8A00"}, {"Chat Popup Mode", L"\u804A\u5929\u5F39\u51FA\u6A21\u5F0F"}, {"Text Bubbles", L"\u804A\u5929\u6C14\u6CE1"},
		{"Max Chat Messages", L"\u6700\u5927\u804A\u5929\u8BB0\u5F55"}, {"Chat Key", L"\u804A\u5929\u6846\u6309\u952E"}, {"Opponent Chat Color", L"\u5BF9\u624B\u804A\u5929\u989C\u8272"},
		{"Host IP", L"\u5BF9\u6218 IP"}, {"Chat Logs", L"\u804A\u5929\u65E5\u5FD7"}, {"Reset Config", L"\u91CD\u7F6E\u914D\u7F6E"}, {"Quick Messages", L"\u5FEB\u6377\u5BF9\u8BDD"}
	};
	auto found = labels.find(name);
	return found == labels.end() ? std::wstring(name.begin(), name.end()) : found->second;
}

std::wstring localizedChoice(const std::string &label)
{
	if (!chineseLanguage)
		return std::wstring(label.begin(), label.end());
	static const std::map<std::string, std::wstring> labels = {
		{"English", L"\u82F1\u6587"}, {"Chinese", L"\u4E2D\u6587"}, {"All", L"\u6240\u6709\u4EBA"}, {"Battle Players", L"\u5BF9\u6218\u73A9\u5BB6"}, {"Never", L"\u4ECE\u4E0D"},
		{"Off", L"\u5173\u95ED"}, {"On", L"\u5F00\u542F"}, {"Soft Blue", L"\u67D4\u548C\u84DD"}, {"Gold", L"\u91D1\u8272"}, {"Soft Pink", L"\u67D4\u548C\u7C89"},
		{"Soft Green", L"\u67D4\u548C\u7EFF"}, {"Orange", L"\u6A59\u8272"}, {"Purple", L"\u7D2B\u8272"}, {"White", L"\u767D\u8272"}, {"Auto", L"\u81EA\u52A8"},
		{"Open Folder", L"\u6253\u5F00\u6587\u4EF6\u5939"}, {"Restore Defaults", L"\u6062\u590D\u9ED8\u8BA4\u914D\u7F6E"}, {"Edit 1-9", L"\u7F16\u8F91 1-9"},
		{"Enter", L"\u56DE\u8F66"}, {"Space", L"\u7A7A\u683C"}
	};
	auto found = labels.find(label);
	return found == labels.end() ? std::wstring(label.begin(), label.end()) : found->second;
}

bool shouldUseSharpText(const std::wstring &text)
{
	return std::all_of(text.begin(), text.end(), [](wchar_t chr) {
		return chr < 0x80;
	});
}

void createLocalizedSprite(SokuLib::DrawUtils::Sprite &sprite, const std::wstring &text, unsigned fontSize, SokuLib::Vector2i bounds)
{
	SokuLib::Vector2i size;
	int textureId = 0;
	auto &font = lobbyData->getFont(fontSize);
	const auto originalOffsetX = font.description.offsetX;
	const auto originalShadow = font.description.shadow;
	const unsigned leftPadding = chineseLanguage ? 0 : 2;
	const bool sharp = shouldUseSharpText(text);

	font.description.offsetX = originalOffsetX + leftPadding;
	if (!sharp)
		font.description.shadow = 0;
	auto created = createTextTexture(textureId, text.c_str(), font, bounds, &size, sharp);
	font.description.offsetX = originalOffsetX;
	font.description.shadow = originalShadow;
	if (!created)
		return;
	size.x = (std::min)(bounds.x, size.x + static_cast<int>(leftPadding));
	sprite.texture.setHandle(textureId, bounds.to<unsigned>());
	sprite.setSize(size.to<unsigned>());
	sprite.rect = {0, 0, size.x, size.y};
}

void displayTranslucentCursor(SokuLib::Vector2i pos, SokuLib::Vector2u size)
{
	SokuLib::Sprite (&sprites)[3] = *(SokuLib::Sprite (*)[3])0x89A6C0;
	unsigned colors[3][4];

	for (unsigned sprite = 0; sprite < 3; sprite++)
		for (unsigned vertex = 0; vertex < 4; vertex++) {
			colors[sprite][vertex] = sprites[sprite].vertices[vertex].color;
			auto alpha = (colors[sprite][vertex] >> 24) & 0xFF;
			sprites[sprite].vertices[vertex].color =
				(colors[sprite][vertex] & 0x00FFFFFF) | ((alpha * 0x70 / 0xFF) << 24);
		}
	displaySokuCursor(pos, size);
	for (unsigned sprite = 0; sprite < 3; sprite++)
		for (unsigned vertex = 0; vertex < 4; vertex++)
			sprites[sprite].vertices[vertex].color = colors[sprite][vertex];
}

}

OptionsMenu::OptionsMenu(LobbyMenu *parent) :
	_parent(parent)
{
	SokuLib::Vector2i size;

	this->_background.setPosition({32, 42});
	this->_background.setSize({576, 390});
	this->_background.setFillColor(SokuLib::Color{0x10, 0x18, 0x28, 0xE8});
	this->_background.setBorderColor(SokuLib::Color{0xB0, 0xB8, 0xC8, 0xFF});

	this->_title.texture.createFromText("Lobby Options", lobbyData->getFont(24), {400, 40}, &size);
	this->_title.setSize(size.to<unsigned>());
	this->_title.rect.width = size.x;
	this->_title.rect.height = size.y;
	this->_title.setPosition({52, 54});

	this->_hint.texture.createFromText("A / Left / Right: Change    B / ESC: Back", lobbyData->getFont(12), {540, 20}, &size);
	this->_hint.setSize(size.to<unsigned>());
	this->_hint.rect.width = size.x;
	this->_hint.rect.height = size.y;
	this->_hint.setPosition({52, 402});

	this->_status.texture.createFromText("Could not save SokuLobbies.ini; value was not changed.", lobbyData->getFont(12), {540, 20}, &size);
	this->_status.setSize(size.to<unsigned>());
	this->_status.rect.width = size.x;
	this->_status.rect.height = size.y;
	this->_status.setPosition({52, 382});
	this->_status.tint = SokuLib::Color{0xFF, 0x80, 0x80, 0xFF};

	this->_addOption({
		"Language",
		{{"English", 0}, {"Chinese", 1}},
		[] { return chineseLanguage ? 1u : 0u; },
		[this](unsigned value) {
			if (!saveString(L"Language", value ? L"Chinese" : L"English"))
				return false;
			chineseLanguage = value != 0;
			this->_parent->onLanguageChanged();
			this->_refreshLanguage();
			return true;
		}
	});
	this->_addOption({
		"Chat Popup Mode",
		{{"All", CHAT_POPUP_ALL}, {"Battle Players", CHAT_POPUP_OPPONENTS}, {"Never", CHAT_POPUP_NEVER}},
		[] { return static_cast<unsigned>(chatPopupMode); },
		[](unsigned value) {
			const wchar_t *stored = value == CHAT_POPUP_NEVER ? L"Never" : value == CHAT_POPUP_OPPONENTS ? L"Battle" : L"All";
			if (!saveString(L"ChatPopupMode", stored))
				return false;
			chatPopupMode = static_cast<ChatPopupMode>(value);
			return true;
		}
	});
	this->_addOption({
		"Text Bubbles",
		{{"Off", 0}, {"On", 1}},
		[] { return showTextBubbles ? 1u : 0u; },
		[](unsigned value) {
			if (!saveUnsigned(L"ShowTextBubbles", value != 0))
				return false;
			showTextBubbles = value != 0;
			return true;
		}
	});
	this->_addOption({
		"Max Chat Messages",
		{{std::to_string(maxChatMessages), 0}},
		[] { return 0u; },
		[](unsigned) { return true; },
		{},
		[this] {
			setWideInputBoxCallbacks([this](const std::wstring &input) {
				unsigned value;
				if (!parseUnsignedInput(input, 1, 10000, value)) {
					this->_showStatus(chineseLanguage ? L"\u8BF7\u8F93\u5165 1 \u5230 10000 \u4E4B\u95F4\u7684\u6570\u5B57\u3002" : L"Enter a number from 1 to 10000.", SokuLib::Color{0xFF, 0x80, 0x80, 0xFF});
					playSound(0x29);
					return;
				}
				if (!saveUnsigned(L"MaxChatMessages", value)) {
					this->_showSaveError();
					return;
				}
				maxChatMessages = value;
				auto &option = *std::find_if(this->_options.begin(), this->_options.end(), [](const Option &item) { return item.name == "Max Chat Messages"; });
				option.choices[0].label = std::to_string(value);
				this->_refreshValue(option);
				playSound(0x28);
			});
			openWideInputDialog(chineseLanguage ? L"\u6700\u5927\u804A\u5929\u8BB0\u5F55\u6570\uFF081-10000\uFF09- Enter\uFF1A\u4FDD\u5B58 / ESC\uFF1A\u53D6\u6D88" : L"Max Chat Messages (1-10000) - Enter: Save / ESC: Cancel", std::to_wstring(maxChatMessages), 5);
		}
	});
	this->_addOption({
		"Chat Key",
		{{keyName(chatKey), 0}},
		[] { return 0u; },
		[](unsigned) { return true; },
		{},
		[this] {
			this->_capturingChatKey = true;
			this->_chatKeyCaptureArmed = false;
			this->_showStatus(chineseLanguage ? L"\u677E\u5F00\u5F53\u524D\u6309\u952E\u540E\u6309\u4E0B\u65B0\u7684\u804A\u5929\u952E\uFF0C\u6309 ESC \u53D6\u6D88\u3002" : L"Release the current key, then press a new chat key. ESC cancels.", SokuLib::Color{0xFF, 0xE0, 0x90, 0xFF});
		}
	});
	this->_addOption({
		"Opponent Chat Color",
		{{"Soft Blue", 0x7FA6D9}, {"Gold", 0xE0B85C}, {"Soft Pink", 0xD990B5}, {"Soft Green", 0x79B88A}, {"Orange", 0xD98B5F}, {"Purple", 0xFF00FF}, {"White", 0xFFFFFF}},
		[] { return opponentChatColor & 0xFFFFFF; },
		[](unsigned value) {
			wchar_t buffer[7];
			std::swprintf(buffer, 7, L"%06X", value & 0xFFFFFF);
			if (!saveString(L"OpponentChatColor", buffer))
				return false;
			opponentChatColor = value & 0xFFFFFF;
			return true;
		},
		colorValue
	});
	this->_addOption({
		"Host IP",
		{{hostIpLabel(readLobbyString(L"HostIP")), 0}},
		[] { return 0u; },
		[](unsigned) { return true; },
		{},
		[this] {
			auto current = readLobbyString(L"HostIP");
			setWideInputBoxCallbacks([this](const std::wstring &value) {
				std::wstring host;
				if (!parseHostIp(value, host)) {
					this->_showStatus(chineseLanguage ? L"\u5BF9\u6218 IP \u65E0\u6548\uFF0C\u8BF7\u8F93\u5165 IPv4 \u5730\u5740\uFF0C\u53EF\u9644\u5E26\u7AEF\u53E3\u3002" : L"Invalid Host IP. Use an IPv4 address with an optional port.", SokuLib::Color{0xFF, 0x80, 0x80, 0xFF});
					playSound(0x29);
					return;
				}
				if (!IniConfig::writeLobbyString(profilePath, L"HostIP", host)) {
					this->_showSaveError();
					return;
				}
				auto found = std::find_if(this->_options.begin(), this->_options.end(), [](const Option &option) {
					return option.name == "Host IP";
				});
				if (found != this->_options.end()) {
					found->choices[0].label = hostIpLabel(host);
					this->_refreshValue(*found);
				}
				this->_showStatus(chineseLanguage ? (host.empty() ? L"\u5BF9\u6218 IP \u5DF2\u8BBE\u4E3A\u81EA\u52A8\u83B7\u53D6\u3002" : L"\u5BF9\u6218 IP \u5DF2\u4FDD\u5B58\u3002") : (host.empty() ? L"Host IP set to automatic." : L"Host IP saved."), SokuLib::Color{0x90, 0xE0, 0xA0, 0xFF});
				playSound(0x28);
			});
			openWideInputDialog(chineseLanguage ? L"\u5BF9\u6218 IP\uFF08\u7559\u7A7A\u4E3A\u81EA\u52A8\uFF09- Ctrl+V / Enter\uFF1A\u4FDD\u5B58 / ESC\uFF1A\u53D6\u6D88" : L"Host IP (blank = Auto) - Ctrl+V / Enter: Save / ESC: Cancel", current, 255);
		}
	});
	this->_addOption({
		"Chat Logs",
		{{"Open Folder", 0}},
		[] { return 0u; },
		[](unsigned) { return true; },
		{},
		[this] {
			try {
				auto folder = std::filesystem::path(profileFolderPath) / "chatlog";
				std::filesystem::create_directories(folder);
				auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(SokuLib::window, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
				if (result <= 32) {
					this->_showStatus(chineseLanguage ? L"\u65E0\u6CD5\u6253\u5F00\u804A\u5929\u65E5\u5FD7\u6587\u4EF6\u5939\u3002" : L"Could not open the chat log folder.", SokuLib::Color{0xFF, 0x80, 0x80, 0xFF});
					playSound(0x29);
				} else
					playSound(0x28);
			} catch (...) {
				this->_showStatus(chineseLanguage ? L"\u65E0\u6CD5\u6253\u5F00\u804A\u5929\u65E5\u5FD7\u6587\u4EF6\u5939\u3002" : L"Could not open the chat log folder.", SokuLib::Color{0xFF, 0x80, 0x80, 0xFF});
				playSound(0x29);
			}
		}
	});
	this->_addOption({
		"Reset Config",
		{{"Restore Defaults", 0}},
		[] { return 0u; },
		[](unsigned) { return true; },
		{},
		[this] {
			const auto message = chineseLanguage
				? L"\u662F\u5426\u4F7F\u7528 SokuLobbies-default.ini \u66FF\u6362\u5F53\u524D\u914D\u7F6E\uFF1F\n\n\u5B8C\u6210\u540E\u5FC5\u987B\u91CD\u542F\u6E38\u620F\u3002"
				: L"Replace SokuLobbies.ini with SokuLobbies-default.ini?\n\nThe game must be restarted afterwards.";
			const auto title = chineseLanguage ? L"\u91CD\u7F6E\u5927\u5385\u914D\u7F6E" : L"Reset Lobby Configuration";
			if (MessageBoxW(SokuLib::window, message, title, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
				return;
			auto source = std::filesystem::path(profileFolderPath) / L"SokuLobbies-default.ini";
			if (!std::filesystem::exists(source)) {
				this->_showStatus(chineseLanguage ? L"\u627E\u4E0D\u5230 SokuLobbies-default.ini\u3002" : L"SokuLobbies-default.ini was not found.", SokuLib::Color{0xFF, 0x80, 0x80, 0xFF});
				playSound(0x29);
				return;
			}
			if (!CopyFileW(source.c_str(), profilePath, FALSE)) {
				this->_showStatus(chineseLanguage ? L"\u65E0\u6CD5\u6062\u590D\u9ED8\u8BA4\u914D\u7F6E\u3002" : L"Could not restore the default configuration.", SokuLib::Color{0xFF, 0x80, 0x80, 0xFF});
				playSound(0x29);
				return;
			}
			this->_showStatus(chineseLanguage ? L"\u5DF2\u6062\u590D\u9ED8\u8BA4\u914D\u7F6E\uFF0C\u91CD\u542F\u6E38\u620F\u540E\u751F\u6548\u3002" : L"Defaults restored. Restart the game to apply them.", SokuLib::Color{0x90, 0xE0, 0xA0, 0xFF});
			playSound(0x28);
		}
	});
	this->_addOption({
		"Quick Messages",
		{{"Edit 1-9", 0}},
		[] { return 0u; },
		[](unsigned) { return true; },
		{},
		[this] {
			this->_editingMessages = true;
			this->_statusTimer = 0;
			playSound(0x28);
		}
	});
	// Keep the menu layout predictable while grouping related controls.
	std::stable_sort(this->_options.begin(), this->_options.end(), [](const Option &left, const Option &right) {
		auto rank = [](const std::string &name) {
			if (name == "Chat Popup Mode") return 0;
			if (name == "Text Bubbles") return 1;
			if (name == "Quick Messages") return 2;
			if (name == "Opponent Chat Color") return 3;
			if (name == "Chat Key") return 4;
			if (name == "Host IP") return 5;
			if (name == "Language") return 6;
			if (name == "Max Chat Messages") return 7;
			if (name == "Chat Logs") return 8;
			if (name == "Reset Config") return 9;
			return 100;
		};
		return rank(left.name) < rank(right.name);
	});
	this->_initMessageEditor();
	this->_refreshLanguage();
}

OptionsMenu::~OptionsMenu()
{
	if (inputBoxShown)
		closeInputDialog();
}

void OptionsMenu::_addOption(Option option)
{
	SokuLib::Vector2i size;
	auto current = option.get();
	auto found = std::find_if(option.choices.begin(), option.choices.end(), [current](const Choice &choice) {
		return choice.value == current;
	});
	if (found == option.choices.end()) {
		auto label = option.formatCustom ? option.formatCustom(current) : decimalValue(current);
		option.choices.push_back({std::move(label), current});
		option.index = static_cast<unsigned>(option.choices.size() - 1);
	} else
		option.index = static_cast<unsigned>(std::distance(option.choices.begin(), found));
	createLocalizedSprite(option.labelSprite, localizedOptionName(option.name), 16, {300, 24});
	this->_options.emplace_back(std::move(option));
	this->_refreshValue(this->_options.back());
}

void OptionsMenu::_refreshValue(Option &option)
{
	SokuLib::Vector2i size;
	createLocalizedSprite(option.valueSprite, localizedChoice(option.choices[option.index].label), 16, {260, 24});
}

void OptionsMenu::_refreshLanguage()
{
	createLocalizedSprite(this->_title, chineseLanguage ? L"\u5927\u5385\u9009\u9879" : L"Lobby Options", 24, {400, 40});
	createLocalizedSprite(this->_hint, chineseLanguage ? L"\u786E\u8BA4 / \u5DE6\u53F3\uFF1A\u4FEE\u6539    \u8FD4\u56DE\u952E / ESC\uFF1A\u8FD4\u56DE" : L"A / Left / Right: Change    B / ESC: Back", 12, {540, 20});
	createLocalizedSprite(this->_messagesTitle, chineseLanguage ? L"\u5FEB\u6377\u5BF9\u8BDD" : L"Quick Messages", 22, {400, 36});
	createLocalizedSprite(this->_messagesHint, chineseLanguage ? L"\u786E\u8BA4 / Enter\uFF1A\u7F16\u8F91    \u8FD4\u56DE\u952E / ESC\uFF1A\u8FD4\u56DE" : L"A / Enter: Edit    B / ESC: Back", 12, {540, 20});
	for (auto &option : this->_options) {
		createLocalizedSprite(option.labelSprite, localizedOptionName(option.name), 16, {300, 24});
		this->_refreshValue(option);
	}
	for (unsigned i = 0; i < this->_messageValues.size(); i++)
		this->_refreshMessageValue(i);
}

void OptionsMenu::_showSaveError()
{
	this->_showStatus(chineseLanguage ? L"\u65E0\u6CD5\u4FDD\u5B58 SokuLobbies.ini\uFF0C\u672C\u6B21\u4FEE\u6539\u672A\u751F\u6548\u3002" : L"Could not save SokuLobbies.ini; value was not changed.", SokuLib::Color{0xFF, 0x80, 0x80, 0xFF});
	playSound(0x29);
}

void OptionsMenu::_showStatus(const wchar_t *message, const SokuLib::Color &color)
{
	createLocalizedSprite(this->_status, message, 12, {540, 20});
	this->_status.tint = color;
	this->_statusTimer = 300;
}

void OptionsMenu::_updateChatKeyCapture()
{
	bool anyKeyDown = false;
	for (unsigned key = VK_BACK; key <= 0xFE; key++)
		if (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) {
			anyKeyDown = true;
			if (!this->_chatKeyCaptureArmed)
				continue;
			if (key == VK_ESCAPE) {
				this->_capturingChatKey = false;
				this->_showStatus(chineseLanguage ? L"\u5DF2\u53D6\u6D88\u4FEE\u6539\u804A\u5929\u952E\u3002" : L"Chat key change cancelled.", SokuLib::Color{0xD0, 0xD0, 0xD0, 0xFF});
				playSound(0x29);
				return;
			}
			if (!saveUnsigned(L"ChatKey", key)) {
				this->_capturingChatKey = false;
				this->_showSaveError();
				return;
			}
			chatKey = key;
			auto found = std::find_if(this->_options.begin(), this->_options.end(), [](const Option &option) { return option.name == "Chat Key"; });
			if (found != this->_options.end()) {
				found->choices[0].label = keyName(key);
				this->_refreshValue(*found);
			}
			this->_capturingChatKey = false;
			this->_showStatus(chineseLanguage ? L"\u804A\u5929\u952E\u5DF2\u66F4\u65B0\u3002" : L"Chat key updated.", SokuLib::Color{0x90, 0xE0, 0xA0, 0xFF});
			playSound(0x28);
			return;
		}
	if (!this->_chatKeyCaptureArmed && !anyKeyDown) {
		this->_chatKeyCaptureArmed = true;
		this->_showStatus(chineseLanguage ? L"\u8BF7\u6309\u4E0B\u65B0\u7684\u804A\u5929\u952E\uFF0C\u6309 ESC \u53D6\u6D88\u3002" : L"Press the new chat key. ESC cancels.", SokuLib::Color{0xFF, 0xE0, 0x90, 0xFF});
	}
}

void OptionsMenu::_initMessageEditor()
{
	SokuLib::Vector2i size;
	this->_messagesTitle.texture.createFromText("Quick Messages", lobbyData->getFont(22), {400, 36}, &size);
	this->_messagesTitle.setSize(size.to<unsigned>());
	this->_messagesTitle.rect.width = size.x;
	this->_messagesTitle.rect.height = size.y;
	this->_messagesTitle.setPosition({52, 58});
	this->_messagesHint.texture.createFromText("A / Enter: Edit    B / ESC: Back", lobbyData->getFont(12), {540, 20}, &size);
	this->_messagesHint.setSize(size.to<unsigned>());
	this->_messagesHint.rect.width = size.x;
	this->_messagesHint.rect.height = size.y;
	this->_messagesHint.setPosition({52, 402});
	for (unsigned i = 0; i < 9; i++) {
		this->_messageLabels.emplace_back();
		auto &label = this->_messageLabels.back();
		label.texture.createFromText(std::to_string(i + 1).c_str(), lobbyData->getFont(16), {32, 24}, &size);
		label.setSize(size.to<unsigned>());
		label.rect.width = size.x;
		label.rect.height = size.y;
		this->_messageValues.emplace_back();
		this->_refreshMessageValue(i);
	}
}

void OptionsMenu::_refreshMessageValue(unsigned index)
{
	std::wstring value = quickMessages[index].empty() ? (chineseLanguage ? L"\uFF08\u672A\u914D\u7F6E\uFF09" : L"(not configured)") : quickMessages[index];
	if (value.size() > 48) {
		value.resize(45);
		if (!value.empty() && value.back() >= 0xD800 && value.back() <= 0xDBFF)
			value.pop_back();
		value += L"...";
	}
	SokuLib::Vector2i size;
	int textureId = 0;
	auto &font = lobbyData->getFont(16);
	const auto originalShadow = font.description.shadow;
	const bool sharp = shouldUseSharpText(value);

	if (!sharp)
		font.description.shadow = 0;
	auto created = createTextTexture(textureId, value.c_str(), font, {450, 24}, &size, sharp);
	font.description.shadow = originalShadow;
	if (!created)
		return;
	auto &sprite = this->_messageValues[index];
	sprite.texture.setHandle(textureId, {450, 24});
	sprite.setSize(size.to<unsigned>());
	sprite.rect = {0, 0, size.x, size.y};
}

void OptionsMenu::_openMessageEditor(unsigned index)
{
	std::wstring title = chineseLanguage
		? L"\u5FEB\u6377\u5BF9\u8BDD " + std::to_wstring(index + 1) + L" - Enter\uFF1A\u4FDD\u5B58 / ESC\uFF1A\u53D6\u6D88"
		: L"Quick Message " + std::to_wstring(index + 1) + L" - Enter: Save / ESC: Cancel";
	setWideInputBoxCallbacks([this, index](const std::wstring &value) {
		wchar_t key[32];
		std::swprintf(key, 32, L"QuickMessage%u", index + 1);
		if (!IniConfig::writeLobbyString(profilePath, key, value)) {
			this->_showSaveError();
			return;
		}
		quickMessages[index] = value;
		this->_refreshMessageValue(index);
		playSound(0x28);
	});
	openWideInputDialog(title.c_str(), quickMessages[index], 512);
}

void OptionsMenu::_updateMessageEditor()
{
	if (SokuLib::checkKeyOneshot(DIK_ESCAPE, 0, 0, 0) || SokuLib::inputMgrs.input.b == 1) {
		this->_editingMessages = false;
		playSound(0x29);
		return;
	}
	if (SokuLib::inputMgrs.input.a == 1 || SokuLib::checkKeyOneshot(DIK_RETURN, 0, 0, 0)) {
		this->_openMessageEditor(this->_messageCursor);
		return;
	}
	if (std::abs(SokuLib::inputMgrs.input.verticalAxis) == 1 ||
		(std::abs(SokuLib::inputMgrs.input.verticalAxis) > 36 && std::abs(SokuLib::inputMgrs.input.verticalAxis) % 6 == 0)) {
		if (SokuLib::inputMgrs.input.verticalAxis > 0)
			this->_messageCursor = (this->_messageCursor + 1) % 9;
		else
			this->_messageCursor = (this->_messageCursor + 8) % 9;
		playSound(0x27);
	}
}

void OptionsMenu::_renderMessageEditor()
{
	this->_background.draw();
	this->_messagesTitle.draw();
	for (unsigned i = 0; i < 9; i++) {
		int y = 108 + static_cast<int>(i) * 30;
		this->_messageLabels[i].setPosition({76, y});
		this->_messageValues[i].setPosition({116, y});
		this->_messageLabels[i].draw();
		this->_messageValues[i].draw();
	}
	displayTranslucentCursor({58, 104 + static_cast<int>(this->_messageCursor) * 30}, {518, 23});
	if (this->_statusTimer)
		this->_status.draw();
	else
		this->_messagesHint.draw();
	inputBoxRender();
}

void OptionsMenu::_applyValue(Option &option, int delta)
{
	int next = static_cast<int>(option.index) + delta;
	if (next < 0)
		next += static_cast<int>(option.choices.size());
	else
		next %= static_cast<int>(option.choices.size());
	if (!option.apply(option.choices[static_cast<unsigned>(next)].value)) {
		this->_showSaveError();
		return;
	}
	option.index = static_cast<unsigned>(next);
	this->_refreshValue(option);
	playSound(0x28);
}

void OptionsMenu::_()
{
	*(int *)0x882a94 = 0x16;
}

int OptionsMenu::onProcess()
{
	if (this->_statusTimer)
		this->_statusTimer--;
	if (this->_capturingChatKey) {
		this->_updateChatKeyCapture();
		return true;
	}
	inputBoxUpdate();
	if (inputBoxShown) {
		if (SokuLib::inputMgrs.input.b == 1) {
			closeInputDialog();
			playSound(0x29);
		}
		return true;
	}
	if (this->_editingMessages) {
		this->_updateMessageEditor();
		return true;
	}
	if (SokuLib::checkKeyOneshot(DIK_ESCAPE, 0, 0, 0) || SokuLib::inputMgrs.input.b == 1) {
		playSound(0x29);
		return false;
	}
	if (this->_options.empty())
		return true;
	auto &option = this->_options[this->_cursor];
	if (SokuLib::inputMgrs.input.a == 1 || SokuLib::checkKeyOneshot(DIK_RETURN, 0, 0, 0)) {
		if (option.confirm)
			option.confirm();
		else
			this->_applyValue(option, 1);
		return true;
	}
	if (std::abs(SokuLib::inputMgrs.input.horizontalAxis) == 1 ||
		(std::abs(SokuLib::inputMgrs.input.horizontalAxis) > 36 && std::abs(SokuLib::inputMgrs.input.horizontalAxis) % 6 == 0)) {
		if (option.confirm)
			option.confirm();
		else
			this->_applyValue(option, SokuLib::inputMgrs.input.horizontalAxis < 0 ? -1 : 1);
	}
	if (std::abs(SokuLib::inputMgrs.input.verticalAxis) == 1 ||
		(std::abs(SokuLib::inputMgrs.input.verticalAxis) > 36 && std::abs(SokuLib::inputMgrs.input.verticalAxis) % 6 == 0)) {
		if (SokuLib::inputMgrs.input.verticalAxis > 0)
			this->_cursor = (this->_cursor + 1) % this->_options.size();
		else
			this->_cursor = (this->_cursor + this->_options.size() - 1) % this->_options.size();
		playSound(0x27);
	}
	return true;
}

int OptionsMenu::onRender()
{
	if (this->_editingMessages) {
		this->_renderMessageEditor();
		return 0;
	}
	this->_background.draw();
	this->_title.draw();
	for (unsigned i = 0; i < this->_options.size(); i++) {
		auto &option = this->_options[i];
		int y = 106 + static_cast<int>(i) * 27;
		option.labelSprite.setPosition({64, y});
		option.valueSprite.setPosition({560 - static_cast<int>(option.valueSprite.getSize().x), y});
		option.labelSprite.draw();
		option.valueSprite.draw();
	}
	displayTranslucentCursor({50, 102 + static_cast<int>(this->_cursor) * 27}, {526, 22});
	if (this->_statusTimer)
		this->_status.draw();
	else
		this->_hint.draw();
	inputBoxRender();
	return 0;
}
