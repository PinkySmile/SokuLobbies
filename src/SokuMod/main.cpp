//
// Created by PinkySmile on 31/10/2020
//

#include <sstream>
#include <fstream>
#include <memory>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cwctype>
#include <filesystem>
#include <SokuLib.hpp>
#include <shlwapi.h>
#include "data.hpp"
#include "LobbyMenu.hpp"
#include "InLobbyMenu.hpp"
#include "LobbyData.hpp"
#include "InputBox.hpp"
#include "Blocklist.hpp"
#include "integration.hpp"
#include "dinput.h"

typedef HRESULT(__stdcall* EndSceneFn)(IDirect3DDevice9*);

static void (__fastcall *og_call48AD33)(SokuLib::CharacterManager *);
static EndSceneFn Original_EndScene;
static bool wasBlocking = false;
static void (*og_onUpdate)();
static int (SokuLib::Logo::*og_LogoOnProcess)();
static int (SokuLib::Title::*og_TitleOnProcess)();
static int (SokuLib::Battle::*og_BattleOnProcess)();
static int (SokuLib::Select::*og_SelectOnProcess)();
static int (SokuLib::MenuConnect::*og_ConnectOnProcess)();
static int (SokuLib::MenuConnect::*og_ConnectOnRender)();
static int (SokuLib::MenuConnect::*og_ConnectOnUnknown)();
static int (SokuLib::BattleWatch::*og_BattleWatchOnProcess)();
static int (SokuLib::BattleWatch::*og_BattleWatchOnRender)();
static int (SokuLib::LoadingWatch::*og_LoadingWatchOnProcess)();
static int (SokuLib::LoadingWatch::*og_LoadingWatchOnRender)();
static int (SokuLib::BattleClient::*og_BattleClientOnProcess)();
static int (SokuLib::BattleClient::*og_BattleClientOnRender)();
static int (SokuLib::SelectClient::*og_SelectClientOnProcess)();
static int (SokuLib::SelectClient::*og_SelectClientOnRender)();
static int (SokuLib::LoadingClient::*og_LoadingClientOnProcess)();
static int (SokuLib::LoadingClient::*og_LoadingClientOnRender)();
static int (SokuLib::BattleServer::*og_BattleServerOnProcess)();
static int (SokuLib::BattleServer::*og_BattleServerOnRender)();
static int (SokuLib::SelectServer::*og_SelectServerOnProcess)();
static int (SokuLib::SelectServer::*og_SelectServerOnRender)();
static int (SokuLib::LoadingServer::*og_LoadingServerOnProcess)();
static int (SokuLib::LoadingServer::*og_LoadingServerOnRender)();
static int (SokuLib::BattleManager::*og_BattleMgrOnProcess)();
static void (SokuLib::KeymapManager::*s_origKeymapManager_SetInputs)();
static int (__stdcall *s_origRecvFrom)(SOCKET, char *, int, int, sockaddr *, int *);

static int __stdcall BlocklistRecvFrom(SOCKET socket, char *buffer, int length, int flags, sockaddr *from, int *fromLength)
{
	while (true) {
		auto result = s_origRecvFrom(socket, buffer, length, flags, from, fromLength);
		if (result <= 0 || !from || !fromLength || *fromLength < sizeof(sockaddr_in) || from->sa_family != AF_INET)
			return result;
		auto source = reinterpret_cast<sockaddr_in *>(from);
		auto sourceAddress = source->sin_addr.s_addr;
		if (result < static_cast<int>(offsetof(SokuLib::PacketInitRequ, name)))
			return result;
		auto &request = *reinterpret_cast<SokuLib::PacketInitRequ *>(buffer);
		if (
			request.type != SokuLib::INIT_REQUEST ||
			request.reqType != SokuLib::PLAY_REQU ||
			request.nameLength > result - offsetof(SokuLib::PacketInitRequ, name)
		)
			return result;
		auto address = sourceAddress;
		auto isRelayed = (address & htonl(0xFF000000)) == htonl(0x7F000000);
		// PLAY_REQU is sent by the joining player and received only by the
		// host. mainMode is not reliable here because it may not switch to
		// VSSERVER until after this handshake has completed.
		bool blocked = !isRelayed && Blocklist::containsIp(address);
		std::string matchedName;
		const std::array<UINT, 5> codePages{CP_UTF8, 932U, 936U, 950U, th123intl::GetTextCodePage()};
		for (auto codePage : codePages) {
			std::string utf8Name;
			try {
				th123intl::ConvertCodePage(
					codePage,
					std::string_view(request.name, request.nameLength),
					CP_UTF8,
					utf8Name
				);
				if (Blocklist::contains(utf8Name)) {
					blocked = true;
					matchedName = std::move(utf8Name);
				}
			} catch (...) {
			}
		}
		if (!blocked)
			return result;

		SokuLib::PacketInitError rejection{};
		rejection.type = SokuLib::INIT_ERROR;
		// GAME_STATE_INVALID makes the lobby client automatically retry as a
		// spectator. Use the terminal error so a blocked play request exits
		// the connection flow instead of entering a play/spectate retry loop.
		rejection.reason = SokuLib::ERROR_SPECTATE_DISABLED;
		SokuLib::DLL::ws2_32.sendto(
			socket,
			reinterpret_cast<char *>(&rejection),
			sizeof(rejection),
			0,
			from,
			*fromLength
		);
		printf("Rejected blocked opponent: %s\n", matchedName.empty() ? "<blocked IP>" : matchedName.c_str());
		WSASetLastError(WSAEWOULDBLOCK);
		return SOCKET_ERROR;
	}
}
using CheckKeyOneshot = bool (__cdecl *)(int, int, int, int);
static CheckKeyOneshot s_origCheckKeyOneshot = nullptr;
static constexpr unsigned CHECK_KEY_HOOK_SIZE = 8;
static constexpr unsigned char CHECK_KEY_PROLOGUE[CHECK_KEY_HOOK_SIZE] = {
	0x8B, 0x44, 0x24, 0x10,
	0x8B, 0x4C, 0x24, 0x0C
};

static bool __cdecl CheckKeyOneshotHook(int key, int arg2, int arg3, int arg4)
{
	auto pressed = s_origCheckKeyOneshot(key, arg2, arg3, arg4);

	if (key != DIK_ESCAPE || !activeMenu)
		return pressed;
	return activeMenu->filterNativeEscape(pressed);
}

static bool canInstallCheckKeyOneshotHook()
{
	return memcmp(
		reinterpret_cast<const void *>(SokuLib::ADDR_CHECK_KEY_ONESHOT),
		CHECK_KEY_PROLOGUE,
		CHECK_KEY_HOOK_SIZE
	) == 0;
}

static bool installCheckKeyOneshotHook()
{
	auto target = reinterpret_cast<unsigned char *>(SokuLib::ADDR_CHECK_KEY_ONESHOT);
	auto gateway = static_cast<unsigned char *>(VirtualAlloc(
		nullptr,
		CHECK_KEY_HOOK_SIZE + 5,
		MEM_COMMIT | MEM_RESERVE,
		PAGE_EXECUTE_READWRITE
	));

	if (!gateway)
		return false;
	memcpy(gateway, target, CHECK_KEY_HOOK_SIZE);
	gateway[CHECK_KEY_HOOK_SIZE] = 0xE9;
	*reinterpret_cast<int *>(gateway + CHECK_KEY_HOOK_SIZE + 1) =
		reinterpret_cast<int>(target + CHECK_KEY_HOOK_SIZE) -
		reinterpret_cast<int>(gateway + CHECK_KEY_HOOK_SIZE + 5);
	s_origCheckKeyOneshot = reinterpret_cast<CheckKeyOneshot>(gateway);

	target[0] = 0xE9;
	*reinterpret_cast<int *>(target + 1) =
		reinterpret_cast<int>(CheckKeyOneshotHook) - reinterpret_cast<int>(target + 5);
	memset(target + 5, 0x90, CHECK_KEY_HOOK_SIZE - 5);
	return true;
}

static std::wstring decodeIniBytes(const char *bytes, size_t size, unsigned codePage, DWORD flags = 0)
{
	std::wstring result;
	if (!size)
		return result;
	auto resultSize = MultiByteToWideChar(codePage, flags, bytes, static_cast<int>(size), nullptr, 0);
	if (!resultSize)
		return result;
	result.resize(resultSize);
	if (!MultiByteToWideChar(codePage, flags, bytes, static_cast<int>(size), result.data(), resultSize))
		result.clear();
	return result;
}

static void loadQuickMessages()
{
	const wchar_t *defaults[9] = {
		L"Good game!", L"Thanks for playing!", L"Please wait a moment.", L"Let's play again!",
		L"", L"", L"", L"", L""
	};
	for (unsigned i = 0; i < 9; i++)
		quickMessages[i] = defaults[i];

	std::ifstream file(std::filesystem::path(profilePath), std::ios::binary);
	if (!file)
		return;
	std::string bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	auto trim = [](std::wstring &value) {
		value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](wchar_t c) { return !std::iswspace(c); }));
		value.erase(std::find_if(value.rbegin(), value.rend(), [](wchar_t c) { return !std::iswspace(c); }).base(), value.end());
	};
	auto parseWideText = [&](const std::wstring &text) {
		std::wistringstream stream(text);
		std::wstring line;
		bool inLobbySection = false;

		while (std::getline(stream, line)) {
			if (!line.empty() && line.back() == L'\r')
				line.pop_back();
			trim(line);
			if (line.empty() || line[0] == L';' || line[0] == L'#')
				continue;
			if (line.front() == L'[' && line.back() == L']') {
				auto section = line.substr(1, line.size() - 2);
				trim(section);
				inLobbySection = _wcsicmp(section.c_str(), L"Lobby") == 0;
				continue;
			}
			if (!inLobbySection)
				continue;
			auto separator = line.find(L'=');
			if (separator == std::wstring::npos)
				continue;
			auto key = line.substr(0, separator);
			auto value = line.substr(separator + 1);
			trim(key);
			trim(value);
			for (unsigned i = 0; i < 9; i++) {
				auto expected = L"QuickMessage" + std::to_wstring(i + 1);
				if (_wcsicmp(key.c_str(), expected.c_str()) == 0) {
					quickMessages[i] = value.substr(0, 512);
					break;
				}
			}
		}
	};

	if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE) {
		std::wstring text;
		text.reserve((bytes.size() - 2) / 2);
		for (size_t i = 2; i + 1 < bytes.size(); i += 2)
			text += static_cast<wchar_t>(static_cast<unsigned char>(bytes[i]) | (static_cast<unsigned char>(bytes[i + 1]) << 8));
		parseWideText(text);
		return;
	}
	if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFE && static_cast<unsigned char>(bytes[1]) == 0xFF) {
		std::wstring text;
		text.reserve((bytes.size() - 2) / 2);
		for (size_t i = 2; i + 1 < bytes.size(); i += 2)
			text += static_cast<wchar_t>((static_cast<unsigned char>(bytes[i]) << 8) | static_cast<unsigned char>(bytes[i + 1]));
		parseWideText(text);
		return;
	}
	if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF && static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF) {
		parseWideText(decodeIniBytes(bytes.data() + 3, bytes.size() - 3, CP_UTF8, MB_ERR_INVALID_CHARS));
		return;
	}

	auto trimBytes = [](std::string &value) {
		auto whitespace = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r'; };
		value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char c) { return !whitespace(c); }));
		value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char c) { return !whitespace(c); }).base(), value.end());
	};
	auto decodeValue = [&](const std::string &value) {
		auto result = decodeIniBytes(value.data(), value.size(), CP_UTF8, MB_ERR_INVALID_CHARS);
		if (!result.empty() || value.empty())
			return result;
		result = decodeIniBytes(value.data(), value.size(), 936);
		if (!result.empty())
			return result;
		return decodeIniBytes(value.data(), value.size(), CP_ACP);
	};
	std::istringstream stream(bytes);
	std::string line;
	bool inLobbySection = false;
	while (std::getline(stream, line)) {
		trimBytes(line);
		if (line.empty() || line[0] == ';' || line[0] == '#')
			continue;
		if (line.front() == '[' && line.back() == ']') {
			auto section = line.substr(1, line.size() - 2);
			trimBytes(section);
			inLobbySection = _stricmp(section.c_str(), "Lobby") == 0;
			continue;
		}
		if (!inLobbySection)
			continue;
		auto separator = line.find('=');
		if (separator == std::string::npos)
			continue;
		auto key = line.substr(0, separator);
		auto value = line.substr(separator + 1);
		trimBytes(key);
		trimBytes(value);
		for (unsigned i = 0; i < 9; i++) {
			auto expected = "QuickMessage" + std::to_string(i + 1);
			if (_stricmp(key.c_str(), expected.c_str()) == 0) {
				quickMessages[i] = decodeValue(value).substr(0, 512);
				break;
			}
		}
	}
}

LARGE_INTEGER timer_frequency;
unsigned &currentFrame = *(unsigned *)0x8985D8;
unsigned char soku2Major = 0;
unsigned char soku2Minor = 0;
char soku2Letter = 0;
bool soku2Force = false;
wchar_t profilePath[MAX_PATH];
wchar_t profileFolderPath[MAX_PATH];
char modVersion[16] = "unknown";
char servHost[64];
char redirectIp[64];
char *wineVersion = nullptr;
unsigned hostPref;
unsigned chatKey;
bool chineseLanguage = false;
std::wstring quickMessages[9];
unsigned lobbyJoinTries;
unsigned lobbyJoinInterval;
unsigned maxChatMessages;
unsigned opponentChatColor = 0x7FA6D9;
bool showTextBubbles = true;
ChatPopupMode chatPopupMode = CHAT_POPUP_ALL;
unsigned short servPort;
unsigned short hostPort;
bool hasSoku2 = false;
bool counted = false;
bool activated = true;
bool init = false;
auto load = std::pair(false, false);
#ifdef _DEBUG
bool debug = true;
#endif
std::function<int ()> onGameEnd;
std::vector<unsigned short> deck;
std::vector<std::pair<unsigned, unsigned short>> cardsUsed;
std::vector<std::pair<unsigned, unsigned short>> cardsBurnt;
LobbyMenu *menu = nullptr;
PTOP_LEVEL_EXCEPTION_FILTER oldFilter = nullptr;
std::pair<bool, bool> selectedRandom{false, false};

std::map<unsigned int, Character> characters{
	{ SokuLib::CHARACTER_REIMU,     {"Reimu",     "Reimu Hakurei",          "reimu",         12}},
	{ SokuLib::CHARACTER_MARISA,    {"Marisa",    "Marisa Kirisame",        "marisa",        12}},
	{ SokuLib::CHARACTER_SAKUYA,    {"Sakuya",    "Sakuya Izayoi",          "sakuya",        12}},
	{ SokuLib::CHARACTER_ALICE,     {"Alice",     "Alice Margatroid",       "alice",         12}},
	{ SokuLib::CHARACTER_PATCHOULI, {"Patchouli", "Patchouli Knowledge",    "patchouli",     15}},
	{ SokuLib::CHARACTER_YOUMU,     {"Youmu",     "Youmu Konpaku",          "youmu",         12}},
	{ SokuLib::CHARACTER_REMILIA,   {"Remilia",   "Remilia Scarlet",        "remilia",       12}},
	{ SokuLib::CHARACTER_YUYUKO,    {"Yuyuko",    "Yuyuko Saigyouji",       "yuyuko",        12}},
	{ SokuLib::CHARACTER_YUKARI,    {"Yukari",    "Yukari Yakumo",          "yukari",        12}},
	{ SokuLib::CHARACTER_SUIKA,     {"Suika",     "Suika Ibuki",            "suika",         12}},
	{ SokuLib::CHARACTER_REISEN,    {"Reisen",    "Reisen Undongein Inaba", "udonge",        12}},
	{ SokuLib::CHARACTER_AYA,       {"Aya",       "Aya Shameimaru",         "aya",           12}},
	{ SokuLib::CHARACTER_KOMACHI,   {"Komachi",   "Komachi Onozuka",        "komachi",       12}},
	{ SokuLib::CHARACTER_IKU,       {"Iku",       "Iku Nagae",              "iku",           12}},
	{ SokuLib::CHARACTER_TENSHI,    {"Tenshi",    "Tenshi Hinanawi",        "tenshi",        12}},
	{ SokuLib::CHARACTER_SANAE,     {"Sanae",     "Sanae Kochiya",          "sanae",         12}},
	{ SokuLib::CHARACTER_CIRNO,     {"Cirno",     "Cirno",                  "chirno",        12}},
	{ SokuLib::CHARACTER_MEILING,   {"Meiling",   "Hong Meiling",           "meirin",        12}},
	{ SokuLib::CHARACTER_UTSUHO,    {"Utsuho",    "Utsuho Reiuji",          "utsuho",        12}},
	{ SokuLib::CHARACTER_SUWAKO,    {"Suwako",    "Suwako Moriya",          "suwako",        12}},
	{ SokuLib::CHARACTER_NAMAZU,    {"Namazu",    "Giant Catfish",          "namazu",        0}},
	{ SokuLib::CHARACTER_RANDOM,    {"Random",    "Random Select",          "random_select", 0}},
};

void playSound(int se)
{
	if (
		(SokuLib::mainMode == SokuLib::BATTLE_MODE_VSSERVER || SokuLib::mainMode == SokuLib::BATTLE_MODE_VSCLIENT) &&
		(
			SokuLib::sceneId == SokuLib::SCENE_BATTLE ||
			SokuLib::sceneId == SokuLib::SCENE_BATTLECL ||
			SokuLib::sceneId == SokuLib::SCENE_BATTLESV ||
			SokuLib::newSceneId == SokuLib::SCENE_BATTLE ||
			SokuLib::newSceneId == SokuLib::SCENE_BATTLECL ||
			SokuLib::newSceneId == SokuLib::SCENE_BATTLESV
		)
	)
		return;
	SokuLib::playSEWaveBuffer(se);
}

LONG WINAPI UnhandledExFilter(PEXCEPTION_POINTERS ExPtr)
{
	puts("Crash !");

	auto name = L".crash";
	std::ofstream file{name};

	if (!file)
		printf("%S: %s\n", name, strerror(errno));
	file.close();

	auto attr = GetFileAttributesW(name);

	if (attr == INVALID_FILE_ATTRIBUTES)
		puts("Fuck");
	else if ((attr & FILE_ATTRIBUTE_HIDDEN) == 0)
		SetFileAttributesW(name, attr | FILE_ATTRIBUTE_HIDDEN);
	return oldFilter ? oldFilter(ExPtr) : 0;
}

static void __fastcall coinUsed(SokuLib::CharacterManager *This)
{
	auto &battlemgr = SokuLib::getBattleMgr();

	if (!lobbyData) {
		og_call48AD33(This);
		return;
	}
	if (SokuLib::mainMode == SokuLib::BATTLE_MODE_VSSERVER && This == &battlemgr.rightCharacterManager)
		goto ok;
	if (SokuLib::mainMode == SokuLib::BATTLE_MODE_VSCLIENT && This == &battlemgr.leftCharacterManager)
		goto ok;
#ifdef _DEBUG
	if (This == &battlemgr.leftCharacterManager)
		goto ok;
#endif
	og_call48AD33(This);
	return;

ok:
	puts("Coin used");
	cardsUsed.emplace_back(currentFrame, 12);
	printf("Card %i used on frame %i\n", 12, currentFrame);
	og_call48AD33(This);
}

static void __fastcall onCardUsed(SokuLib::CharacterManager *This)
{
	auto &battlemgr = SokuLib::getBattleMgr();

	if (!lobbyData)
		return;
	if (SokuLib::mainMode == SokuLib::BATTLE_MODE_VSSERVER && This == &battlemgr.rightCharacterManager)
		goto ok;
	if (SokuLib::mainMode == SokuLib::BATTLE_MODE_VSCLIENT && This == &battlemgr.leftCharacterManager)
		goto ok;
#ifdef _DEBUG
	if (This == &battlemgr.leftCharacterManager)
		goto ok;
#endif
	return;

ok:
	// Special case for Utsuho's 1SC
	if (
		This->characterIndex == SokuLib::CHARACTER_UTSUHO &&
		This->objectBase.action == SokuLib::ACTION_USING_SC_ID_206 &&
		This->objectBase.actionBlockId == 3
	) {
		puts("Okuu 1SC consume card");
		cardsBurnt.emplace_back(currentFrame, This->hand[0].id);
		return;
	}

	auto cost = max(1, This->hand[0].cost - (This->effectiveWeather == SokuLib::WEATHER_CLOUDY));

	printf("Card %i used, costing %i\n", This->hand[0].id, cost);
	for (unsigned i = 0; (!i || i < cost) && i < This->hand.size; i++) {
		(i == 0 ? cardsUsed : cardsBurnt).emplace_back(currentFrame, This->hand[i].id);
		printf("Card %i used%s on frame %i\n", This->hand[i].id, i == 0 ? "" : " as fuel", currentFrame);
	}
}

void countGame()
{
	puts("End game");
	if (!lobbyData)
		return;
#ifndef _DEBUG
	if (SokuLib::mainMode != SokuLib::BATTLE_MODE_VSSERVER && SokuLib::mainMode != SokuLib::BATTLE_MODE_VSCLIENT)
		return;
#endif

	auto &battle = SokuLib::getBattleMgr();
	auto mid = SokuLib::mainMode == SokuLib::BATTLE_MODE_VSSERVER ? SokuLib::rightChar : SokuLib::leftChar;
	auto oid = SokuLib::mainMode == SokuLib::BATTLE_MODE_VSSERVER ? SokuLib::leftChar : SokuLib::rightChar;
	auto &chr = SokuLib::mainMode == SokuLib::BATTLE_MODE_VSSERVER ? battle.rightCharacterManager : battle.leftCharacterManager;

	auto data = lobbyData->loadedCharacterStats.find(mid);
	if (data == lobbyData->loadedCharacterStats.end()) {
		LobbyData::CharacterStatEntry entry{0, 0, 0, 0};

		lobbyData->loadedCharacterStats[mid] = entry;
		data = lobbyData->loadedCharacterStats.find(mid);
	}

	auto dataAgainst = lobbyData->loadedCharacterStats.find(oid);
	if (dataAgainst == lobbyData->loadedCharacterStats.end()) {
		LobbyData::CharacterStatEntry entry{0, 0, 0, 0};

		lobbyData->loadedCharacterStats[oid] = entry;
		dataAgainst = lobbyData->loadedCharacterStats.find(oid);
	}

	auto muIt = lobbyData->loadedMatchupStats.find({mid, oid});
	if (muIt == lobbyData->loadedMatchupStats.end()) {
		LobbyData::MatchupStatEntry entry{0, 0};

		lobbyData->loadedMatchupStats[{mid, oid}] = entry;
		muIt = lobbyData->loadedMatchupStats.find({mid, oid});
	}

	auto cardsIt = lobbyData->loadedCharacterCardUsage.find(mid);
	if (cardsIt == lobbyData->loadedCharacterCardUsage.end()) {
		lobbyData->loadedCharacterCardUsage[mid] = {0};
		cardsIt = lobbyData->loadedCharacterCardUsage.find(mid);
	}

	// My stats
	data->second.wins += chr.score >= 2;
	data->second.losses += chr.score < 2;
	// Random select
	if (SokuLib::mainMode == SokuLib::BATTLE_MODE_VSSERVER ? selectedRandom.second : selectedRandom.first) {
		auto rdata = lobbyData->loadedCharacterStats.find(SokuLib::CHARACTER_RANDOM);
		if (rdata == lobbyData->loadedCharacterStats.end()) {
			LobbyData::CharacterStatEntry entry{0, 0, 0, 0};

			lobbyData->loadedCharacterStats[SokuLib::CHARACTER_RANDOM] = entry;
			rdata = lobbyData->loadedCharacterStats.find(SokuLib::CHARACTER_RANDOM);
		}
		rdata->second.wins += chr.score >= 2;
		rdata->second.losses += chr.score < 2;
	}

	// Against stats
	dataAgainst->second.againstWins += chr.score >= 2;
	dataAgainst->second.againstLosses += chr.score < 2;
	// Random select
	if (SokuLib::mainMode == SokuLib::BATTLE_MODE_VSSERVER ? selectedRandom.first : selectedRandom.second) {
		auto rdataAgainst = lobbyData->loadedCharacterStats.find(SokuLib::CHARACTER_RANDOM);
		if (rdataAgainst == lobbyData->loadedCharacterStats.end()) {
			LobbyData::CharacterStatEntry entry{0, 0, 0, 0};

			lobbyData->loadedCharacterStats[SokuLib::CHARACTER_RANDOM] = entry;
			rdataAgainst = lobbyData->loadedCharacterStats.find(SokuLib::CHARACTER_RANDOM);
		}
		rdataAgainst->second.againstWins += chr.score >= 2;
		rdataAgainst->second.againstLosses += chr.score < 2;
	}

	// Matchup stats
	muIt->second.wins += chr.score >= 2;
	muIt->second.losses += chr.score < 2;

	// Cards stats
	bool usedAll = true;

	cardsIt->second.nbGames++;
	for (auto card : cardsBurnt) {
		auto cardIt = cardsIt->second.cards.find(card.second);

		if (cardIt == cardsIt->second.cards.end()) {
			cardsIt->second.cards[card.second] = {0, 0, 0};
			cardIt = cardsIt->second.cards.find(card.second);
		}
		cardIt->second.burnt++;
	}
	for (auto card : cardsUsed) {
		auto cardIt = cardsIt->second.cards.find(card.second);

		if (cardIt == cardsIt->second.cards.end()) {
			cardsIt->second.cards[card.second] = {0, 0, 0};
			cardIt = cardsIt->second.cards.find(card.second);
		}
		cardIt->second.used++;
	}
	for (auto card : deck) {
		auto cardIt = cardsIt->second.cards.find(card);

		if (cardIt == cardsIt->second.cards.end()) {
			cardsIt->second.cards[card] = {0, 0, 0};
			cardIt = cardsIt->second.cards.find(card);
		}
		cardIt->second.inDeck++;
	}
	lobbyData->saveStats();


	// Wins achievements
	auto &wins = lobbyData->achievementByRequ["win"];
	auto it = std::find_if(wins.begin(), wins.end(), [mid, &data](LobbyData::Achievement *achievement){
		return !achievement->awarded && achievement->requirement["chr"] == mid && achievement->requirement["count"] <= data->second.wins;
	});

	if (it != wins.end()) {
		(*it)->awarded = true;
		lobbyData->achievementAwardQueue.push_back(*it);
	}

	// Losses achievements
	auto &loss = lobbyData->achievementByRequ["lose"];
	it = std::find_if(loss.begin(), loss.end(), [mid, &data](LobbyData::Achievement *achievement){
		return !achievement->awarded && achievement->requirement["chr"] == mid && achievement->requirement["count"] <= data->second.losses;
	});
	if (it != loss.end()) {
		(*it)->awarded = true;
		lobbyData->achievementAwardQueue.push_back(*it);
	}

	// Play achievements
	auto &play = lobbyData->achievementByRequ["play"];
	it = std::find_if(play.begin(), play.end(), [mid, &data](LobbyData::Achievement *achievement){
		return !achievement->awarded && achievement->requirement["chr"] == mid && achievement->requirement["count"] <= data->second.losses + data->second.wins;
	});
	if (it != play.end()) {
		(*it)->awarded = true;
		lobbyData->achievementAwardQueue.push_back(*it);
	}

	// Win move
	if (chr.score >= 2) {
		printf("Won! %i %i %i %i\n", chr.score, mid, chr.objectBase.action, chr.objectBase.actionBlockId);
		auto &win_move = lobbyData->achievementByRequ["win_move"];
		it = std::find_if(win_move.begin(), win_move.end(), [mid, &chr](LobbyData::Achievement *achievement){
			if (achievement->awarded)
				return false;
			if (achievement->requirement.contains("stage") && SokuLib::stageId != achievement->requirement["stage"])
				return false;
			if (achievement->requirement.contains("chr") && achievement->requirement["chr"] != mid)
				return false;
			if (achievement->requirement["action"] != chr.objectBase.action)
				return false;
			if (achievement->requirement.contains("block_blacklist")) {
				auto list = achievement->requirement["block_blacklist"].get<std::vector<int>>();

				if (std::find(list.begin(), list.end(), chr.objectBase.actionBlockId) != list.end())
					return false;
			}
			if (achievement->requirement.contains("block_whitelist")) {
				auto list = achievement->requirement["block_whitelist"].get<std::vector<int>>();

				if (std::find(list.begin(), list.end(), chr.objectBase.actionBlockId) == list.end())
					return false;
			}
			return true;
		});
		if (it != win_move.end()) {
			(*it)->awarded = true;
			lobbyData->achievementAwardQueue.push_back(*it);
		}
	}

	// All cards achievements
	auto textures = lobbyData->cardsTextures.find(mid);
	bool allCards = true;
	for (const auto &elem : textures->second) {
		auto cardIt = cardsIt->second.cards.find(elem.first);

		if (cardIt == cardsIt->second.cards.end() || cardIt->second.used == 0) {
			allCards = false;
			break;
		}
	}
	if (allCards) {
		auto &cards = lobbyData->achievementByRequ["cards"];
		auto itCardsAch = std::find_if(cards.begin(), cards.end(), [mid](LobbyData::Achievement *achievement){
			return !achievement->awarded && achievement->requirement["chr"] == mid;
		});

		if (itCardsAch != cards.end()) {
			(*itCardsAch)->awarded = true;
			lobbyData->achievementAwardQueue.push_back(*itCardsAch);
		}
	}

	bool defaultAllMax = true;
	for (int i = 0; i < characters[mid].nbSkills / 3; i++)
		if (chr.skillMap[i].notUsed || chr.skillMap[i].level < 4) {
			defaultAllMax = false;
			break;
		}
	if (defaultAllMax) {
		auto &winMaxDefault = lobbyData->achievementByRequ["win_max_default"];
		auto winMaxDAch = std::find_if(winMaxDefault.begin(), winMaxDefault.end(), [](LobbyData::Achievement *achievement){
			return !achievement->awarded;
		});

		if (winMaxDAch != winMaxDefault.end()) {
			(*winMaxDAch)->awarded = true;
			lobbyData->achievementAwardQueue.push_back(*winMaxDAch);
		}
	}

	std::map<unsigned, unsigned> cardsUsed;
	auto &useCards = lobbyData->achievementByRequ["use_card"];
	for (auto &data : lobbyData->loadedCharacterCardUsage) {
		for (auto &card : data.second.cards) {
			if (cardsUsed.find(card.first) == cardsUsed.end())
				cardsUsed[card.first] = 0;
			cardsUsed[card.first] += card.second.used;
		}
	}
	for (auto &card : cardsIt->second.cards) {
		auto useCardIt = std::find_if(useCards.begin(), useCards.end(), [mid, &card](LobbyData::Achievement *achievement){
			return !achievement->awarded &&
			       achievement->requirement.contains("chr") &&
			       achievement->requirement["chr"] == mid &&
			       card.second.used > achievement->requirement["count"] &&
			       achievement->requirement["id"] == card.first;
		});

		if (useCardIt != useCards.end()) {
			(*useCardIt)->awarded = true;
			lobbyData->achievementAwardQueue.push_back(*useCardIt);
		}
	}
	for (auto &card : cardsUsed) {
		printf("Card %u was used %u times.\n", card.first, card.second);
		auto useCardIt = std::find_if(useCards.begin(), useCards.end(), [mid, &card](LobbyData::Achievement *achievement){
			return !achievement->awarded &&
			       card.second > achievement->requirement["count"] &&
			       achievement->requirement["id"] == card.first;
		});

		if (useCardIt != useCards.end()) {
			(*useCardIt)->awarded = true;
			lobbyData->achievementAwardQueue.push_back(*useCardIt);
		}
	}
	lobbyData->saveAchievements();
}

int __fastcall ConnectOnProcess(SokuLib::MenuConnect *This)
{
	if (!menu)
		try {
			menu = new LobbyMenu(This);
		} catch (std::exception &e) {
			MessageBoxA(
				SokuLib::window,
				(
					"Error while loading lobby data.\n"
					"Please make sure the assets are properly placed near the mod dll.\n"
					"If they are, please try reinstalling the mod to fix it (save the .dat files in the folder which contains your progression).\n"
					"If reinstalling doesn't fix the issue, contact the mod author and provide this error.\n"
					"\n"
					"Error:\n" +
					std::string(e.what())
				).c_str(),
				"SokuLobby error",
				MB_ICONERROR
			);
			return false;
		}

	auto res = (activated ? menu->onProcess() : (This->*og_ConnectOnProcess)()) & 0xFF;

	if (*(byte*)0x0448e4a != 0x30 && SokuLib::inputMgrs.input.changeCard == 1 && !inputBoxShown) {
		playSound(0x28);
		activated = !activated;
		if (activated)
			menu->_refreshName();
	}
	if (!res) {
		activated = true;
		delete menu;
		menu = nullptr;
	}
	return res;
}

int __fastcall ConnectOnRender(SokuLib::MenuConnect *This)
{
	return activated ? (menu ? menu->onRender() : 0) : (This->*og_ConnectOnRender)();
}

int __fastcall ConnectOnUnknown(SokuLib::MenuConnect *This)
{
	return activated ? (menu ? menu->_(), 0 : 0) : (This->*og_ConnectOnUnknown)();
}

int __fastcall LogoOnProcess(SokuLib::Logo *This)
{
	if (!load.second) {
		load.first = true;
		load.second = true;
		std::thread{[]{
			try {
				lobbyData = std::make_unique<LobbyData>();
			} catch (std::exception &e) {
				MessageBoxA(
					SokuLib::window,
					(
						"Error while loading lobby data.\n"
						"Statistic saving, achievements and blank card rewards are now disabled.\n"
						"To try to load data again, go to the lobby screen.\n"
						"If loading succeeds, it will be enabled again.\n"
						"\n"
						"Error:\n" +
						std::string(e.what())
					).c_str(),
					"SokuLobby error",
					MB_ICONERROR
				);
			}
			load.first = false;
		}}.detach();
	}

	auto res = (This->*og_LogoOnProcess)();

	if (load.first)
		return SokuLib::SCENE_LOGO;
	return res;
}

int __fastcall TitleOnProcess(SokuLib::Title *This)
{
	static LARGE_INTEGER startCtr;
	static bool placed = false;
	static bool needReset = true;
	auto ret = (This->*og_TitleOnProcess)();

	if (!placed) {
		QueryPerformanceFrequency(&timer_frequency);
		QueryPerformanceCounter(&startCtr);
		placed = true;
		puts("Placed handler");
		oldFilter = SetUnhandledExceptionFilter(UnhandledExFilter);
	}

	if (ret != SokuLib::SCENE_TITLE || !lobbyData) {
		needReset = true;
	} else if (!needReset && SokuLib::menuManager.empty()) {
		static LARGE_INTEGER counter;

		QueryPerformanceCounter(&counter);

		unsigned long long time = (counter.QuadPart - startCtr.QuadPart) / timer_frequency.QuadPart;
		bool needSave = false;

		for (auto &ach : lobbyData->achievementByRequ["title_straight"])
			if (!ach->awarded && time / 60 >= ach->requirement["time"]) {
				ach->awarded = true;
				lobbyData->achievementAwardQueue.push_back(ach);
				needSave = true;
			}
		if (needSave)
			lobbyData->saveAchievements();
	} else {
		needReset = false;
		QueryPerformanceCounter(&startCtr);
	}
	return ret;
}

void processCommon(bool showChat)
{
	if (!activeMenu)
		return;
	try {
		activeMenu->updateChat(!showChat);
	} catch (std::exception &e) {
		MessageBoxA(
			SokuLib::window,
			(
				"Error updating chat. You have been kicked from the lobby.\n"
				"Please, report this error.\n"
				"\n"
				"Error:\n" +
				std::string(e.what())
			).c_str(),
			"SokuLobby error",
			MB_ICONERROR
		);
		delete activeMenu;
		activeMenu = nullptr;
	}
}
void renderCommon()
{
	if (!activeMenu)
		return;
	try {
		activeMenu->renderChat();
	} catch (std::exception &e) {
		MessageBoxA(
			SokuLib::window,
			(
				"Error rendering chat. You have been kicked from the lobby.\n"
				"Please, report this error.\n"
				"\n"
				"Error:\n" +
				std::string(e.what())
			).c_str(),
			"SokuLobby error",
			MB_ICONERROR
		);
		delete activeMenu;
		activeMenu = nullptr;
	}
}
void selectCommon()
{
	static bool a = false;
	auto &scene = SokuLib::currentScene->to<SokuLib::Select>();

	if (scene.leftSelectionStage == 3 && scene.rightSelectionStage == 3) {
		if (!a)
			selectedRandom = {
				SokuLib::leftChar == SokuLib::CHARACTER_RANDOM,
				SokuLib::rightChar == SokuLib::CHARACTER_RANDOM
			};
		a = true;
	} else
		a = false;
}

int __fastcall SelectOnProcess(SokuLib::Select *This)
{
	selectCommon();
	return (This->*og_SelectOnProcess)();
}

void rollbackEvents()
{
	while (!cardsBurnt.empty() && cardsBurnt.back().first >= currentFrame) {
		printf("Rolling back card %u burnt on frame %u\n", cardsBurnt.back().second, cardsBurnt.back().first);
		cardsBurnt.pop_back();
	}
	while (!cardsUsed.empty() && cardsUsed.back().first >= currentFrame) {
		printf("Rolling back card %u used on frame %u\n", cardsUsed.back().second, cardsUsed.back().first);
		cardsUsed.pop_back();
	}
}

int __fastcall CBattleManager_OnProcess(SokuLib::BattleManager *mgr)
{
	int result = (mgr->*og_BattleMgrOnProcess)();

	if (!init) {
		auto &chr = SokuLib::mainMode == SokuLib::BATTLE_MODE_VSSERVER ? mgr->rightCharacterManager : mgr->leftCharacterManager;

		deck.clear();
		cardsUsed.clear();
		cardsBurnt.clear();
		for (unsigned i = 0; i < chr.deckInfo.deck.size; i++)
			deck.push_back(chr.deckInfo.deck[i]);
		init = true;
	}
	if (mgr->matchState > 4 && !counted) {
		counted = true;
		countGame();
	}
	return result;
}

int __fastcall BattleOnProcess(SokuLib::Battle *This)
{
	auto &mgr = SokuLib::getBattleMgr();
	auto ret = (This->*og_BattleOnProcess)();

	if (ret != SokuLib::SCENE_BATTLE) {
		counted = false;
		init = false;
	}
	return ret;
}
int __fastcall BattleWatchOnProcess(SokuLib::BattleWatch *This)
{
	if (activeMenu)
		activeMenu->routePendingHotkeys();
	processCommon(false);
	return (This->*og_BattleWatchOnProcess)();
}
int __fastcall LoadingWatchOnProcess(SokuLib::LoadingWatch *This)
{
	if (activeMenu)
		activeMenu->routePendingHotkeys();
	processCommon(true);
	return (This->*og_LoadingWatchOnProcess)();
}
int __fastcall BattleClientOnProcess(SokuLib::BattleClient *This)
{
	auto &mgr = SokuLib::getBattleMgr();
	if (activeMenu)
		activeMenu->routePendingHotkeys();
	auto ret = (This->*og_BattleClientOnProcess)();

	processCommon(false);
	if (ret != SokuLib::SCENE_BATTLECL) {
		counted = false;
		init = false;
	}
	return ret;
}
int __fastcall SelectClientOnProcess(SokuLib::SelectClient *This)
{
	if (activeMenu)
		activeMenu->routePendingHotkeys();
	processCommon(true);
	selectCommon();
	return (This->*og_SelectClientOnProcess)();
}
int __fastcall LoadingClientOnProcess(SokuLib::LoadingClient *This)
{
	if (activeMenu)
		activeMenu->routePendingHotkeys();
	processCommon(true);
	return (This->*og_LoadingClientOnProcess)();
}
int __fastcall BattleServerOnProcess(SokuLib::BattleServer *This)
{
	auto &mgr = SokuLib::getBattleMgr();
	if (activeMenu)
		activeMenu->routePendingHotkeys();
	auto ret = (This->*og_BattleServerOnProcess)();

	processCommon(false);
	if (ret != SokuLib::SCENE_BATTLESV) {
		counted = false;
		init = false;
	}
	return ret;
}
int __fastcall SelectServerOnProcess(SokuLib::SelectServer *This)
{
	if (activeMenu)
		activeMenu->routePendingHotkeys();
	processCommon(true);
	selectCommon();
	return (This->*og_SelectServerOnProcess)();
}
int __fastcall LoadingServerOnProcess(SokuLib::LoadingServer *This)
{
	if (activeMenu)
		activeMenu->routePendingHotkeys();
	processCommon(true);
	return (This->*og_LoadingServerOnProcess)();
}


int __fastcall BattleWatchOnRender(SokuLib::BattleWatch *This)
{
	auto ret = (This->*og_BattleWatchOnRender)();

	renderCommon();
	return ret;
}
int __fastcall LoadingWatchOnRender(SokuLib::LoadingWatch *This)
{
	auto ret = (This->*og_LoadingWatchOnRender)();

	renderCommon();
	return ret;
}
int __fastcall BattleClientOnRender(SokuLib::BattleClient *This)
{
	auto ret = (This->*og_BattleClientOnRender)();

	renderCommon();
	return ret;
}
int __fastcall SelectClientOnRender(SokuLib::SelectClient *This)
{
	auto ret = (This->*og_SelectClientOnRender)();

	renderCommon();
	return ret;
}
int __fastcall LoadingClientOnRender(SokuLib::LoadingClient *This)
{
	auto ret = (This->*og_LoadingClientOnRender)();

	renderCommon();
	return ret;
}
int __fastcall BattleServerOnRender(SokuLib::BattleServer *This)
{
	auto ret = (This->*og_BattleServerOnRender)();

	renderCommon();
	return ret;
}
int __fastcall SelectServerOnRender(SokuLib::SelectServer *This)
{
	auto ret = (This->*og_SelectServerOnRender)();

	renderCommon();
	return ret;
}
int __fastcall LoadingServerOnRender(SokuLib::LoadingServer *This)
{
	auto ret = (This->*og_LoadingServerOnRender)();

	renderCommon();
	return ret;
}
void getSoku2Version(wchar_t *path)
{
	std::ifstream stream{path};
	std::string line;

	printf("Loading file %S\n", path);
	while (std::getline(stream, line)) {
		auto pos = line.find("set_version(");

		if (pos == std::string::npos)
			continue;
		printf("Line is %s\n", line.c_str());
		line = line.substr(pos + strlen("set_version(") + 1);
		pos = line.find('"');
		if (pos == std::string::npos) {
			MessageBox(nullptr, "Cannot parse Soku2 version: Missing closing \" in set_version line.", "SokuLobbies Init error", MB_ICONERROR);
			return;
		}
		line = line.substr(0, pos);
		printf("Version string is %s\n", line.c_str());
		pos = line.find('.');
		if (pos == std::string::npos) {
			MessageBox(nullptr, "Cannot parse Soku2 version: Cannot find . chaarcter in version string.", "SokuLobbies Init error", MB_ICONERROR);
			return;
		}
		try {
			soku2Major = std::stoul(line.substr(0, pos));
			line = line.substr(pos + 1);
			soku2Minor = std::stoul(line, &pos);
			if (pos != line.size()) {
				if (pos != line.size() - 1) {
					printf("%zu %zu\n", pos, line.size() - 1);
					MessageBox(nullptr, "Cannot parse Soku2 version: Trailing letters found.", "SokuLobbies Init error", MB_ICONWARNING);
					return;
				}
				soku2Letter = line.back();
			}
		} catch (std::exception &e) {
			MessageBox(nullptr, ("Cannot parse Soku2 version: " + std::string(e.what()) + ".").c_str(), "SokuLobbies Init error", MB_ICONERROR);
			soku2Major = 0;
			soku2Minor = 0;
			soku2Letter = 0;
			return;
		}
		printf("Soku2 version is %i.%i%c\n", soku2Major, soku2Minor, soku2Letter);
		return;
	}
	MessageBox(nullptr, "Cannot parse Soku2 version: The set_version line was not found.", "SokuLobbies Init error", MB_ICONERROR);
}

void loadSoku2CSV(LPWSTR path)
{
	std::ifstream stream{path};
	std::string line;

	if (stream.fail()) {
		printf("%S: %s\n", path, strerror(errno));
		return;
	}
	while (std::getline(stream, line)) {
		std::stringstream str{line};
		unsigned id;
		std::string idStr;
		std::string codeName;
		std::string shortName;
		std::string fullName;
		std::string skills;

		std::getline(str, idStr, ';');
		std::getline(str, codeName, ';');
		std::getline(str, shortName, ';');
		std::getline(str, fullName, ';');
		std::getline(str, skills, '\n');
		if (str.fail()) {
			printf("Skipping line %s: Stream failed\n", line.c_str());
			continue;
		}
		try {
			id = std::stoi(idStr);
		} catch (...){
			printf("Skipping line %s: Invalid id\n", line.c_str());
			continue;
		}
		characters[id].firstName = shortName;
		characters[id].fullName = fullName;
		characters[id].codeName = codeName;
		characters[id].nbSkills = (std::count(skills.begin(), skills.end(), ',') + 1 - skills.empty()) * 3;
	}
}

void onUpdate()
{
#ifdef _DEBUG
	if (SokuLib::checkKeyOneshot(DIK_F4, false, false, false) && !activeMenu) {
		try {
			lobbyData = std::make_unique<LobbyData>();
		} catch (std::exception &e) {
			MessageBoxA(SokuLib::window, e.what(), "SokuLobby error", MB_ICONERROR);
		}
	}
	if (SokuLib::checkKeyOneshot(DIK_F9, false, false, false))
		debug = !debug;
#endif
	if (SokuLib::sceneId != SokuLib::SCENE_LOGO && lobbyData)
		lobbyData->update();
	if (og_onUpdate)
		og_onUpdate();
}

void loadSoku2Config()
{
	puts("Looking for Soku2 config...");

	int argc;
	wchar_t app_path[MAX_PATH];
	wchar_t setting_path[MAX_PATH];
	auto arg_list = std::unique_ptr<wchar_t *, decltype(&LocalFree)>(CommandLineToArgvW(GetCommandLineW(), &argc), LocalFree);

	if (!arg_list)
		return;

	wcsncpy(app_path, arg_list.get()[0], MAX_PATH);
	PathRemoveFileSpecW(app_path);
	if (GetEnvironmentVariableW(L"SWRSTOYS", setting_path, sizeof(setting_path)) <= 0) {
		if (arg_list && argc > 1 && StrStrIW(arg_list.get()[1], L"ini")) {
			wcscpy(setting_path, arg_list.get()[1]);
		} else {
			wcscpy(setting_path, app_path);
			PathAppendW(setting_path, L"\\SWRSToys.ini");
		}
	}
	printf("Config file is %S\n", setting_path);

	wchar_t moduleKeys[1024];
	wchar_t moduleValue[MAX_PATH];
	GetPrivateProfileStringW(L"Module", nullptr, nullptr, moduleKeys, sizeof(moduleKeys) / sizeof(*moduleKeys), setting_path);
	for (wchar_t *key = moduleKeys; *key; key += wcslen(key) + 1) {
		wchar_t module_path[MAX_PATH];

		GetPrivateProfileStringW(L"Module", key, nullptr, moduleValue, sizeof(moduleValue) / sizeof(*moduleValue), setting_path);

		wchar_t *filename = wcsrchr(moduleValue, '/');

		printf("Check %S\n", moduleValue);
		if (!filename)
			filename = app_path;
		else
			filename++;
		for (int i = 0; filename[i]; i++)
			filename[i] = tolower(filename[i]);
		if (wcscmp(filename, L"soku2.dll") != 0)
			continue;

		hasSoku2 = true;
		PathAppendW(app_path, moduleValue);
		while (auto result = wcschr(app_path, '/'))
			*result = '\\';
		PathRemoveFileSpecW(app_path);
		wcscpy(module_path, app_path);
		printf("Found Soku2 module folder at %S\n", module_path);
		PathAppendW(module_path, L"\\config\\info\\characters.csv");
		loadSoku2CSV(module_path);
		wcscpy(module_path, app_path);
		PathAppendW(module_path, L"\\config\\SOKU2.lua");
		getSoku2Version(module_path);
		return;
	}
}

static void __fastcall KeymapManagerSetInputs(SokuLib::KeymapManager *This)
{
	(This->*s_origKeymapManager_SetInputs)();
	if (activeMenu)
		activeMenu->setMappedEscapeDown(This->input.b != 0);
	if (activeMenu && activeMenu->isInputing()) {
		std::lock_guard<std::mutex> lock(activeMenu->keyTimersMutex);

		if (activeMenu->isEmotePickerOpen()) {
			if (This->input.horizontalAxis == -1)
				activeMenu->keysPressed[VK_LEFT] = true;
			else if (This->input.horizontalAxis == 1)
				activeMenu->keysPressed[VK_RIGHT] = true;
			if (This->input.verticalAxis == -1)
				activeMenu->keysPressed[VK_UP] = true;
			else if (This->input.verticalAxis == 1)
				activeMenu->keysPressed[VK_DOWN] = true;
			if (This->input.a == 1)
				activeMenu->keysPressed[VK_RETURN] = true;
			if (This->input.changeCard == 1)
				activeMenu->keysPressed[VK_PRIOR] = true;
			if (This->input.spellcard == 1)
				activeMenu->keysPressed[VK_NEXT] = true;
		}
	}
	if (
		(activeMenu && activeMenu->isInputing()) ||
		(wasBlocking && (
			This->input.horizontalAxis ||
			This->input.verticalAxis ||
			This->input.a ||
			This->input.b ||
			This->input.c ||
			This->input.d ||
			This->input.changeCard ||
			This->input.spellcard
		))
	) {
		memset(&This->input, 0, sizeof(This->input));
		wasBlocking = true;
	} else
		wasBlocking = false;
}

int __stdcall Hooked_EndScene(IDirect3DDevice9* pDevice)
{
	if (SokuLib::sceneId != SokuLib::SCENE_LOGO && lobbyData)
		lobbyData->render();

	//This is necessary, so we can fit in the hook...
	//That said, the return value is never even checked in soku.
	if (!Original_EndScene)
		(*(EndSceneFn**)pDevice)[42](pDevice);
	else
		Original_EndScene(pDevice);
	return 0x8a0e14;
}

void getModVersionStr()
{
	DWORD  verHandle = 0;
	UINT   size      = 0;
	LPBYTE lpBuffer  = nullptr;
	DWORD  verSize   = GetFileVersionInfoSizeW(profilePath, &verHandle);

	if (verSize == 0)
		return;

	std::vector<char> verData(verSize);

	if (!GetFileVersionInfoW(profilePath, verHandle, verSize, verData.data()))
		return;

	if (!VerQueryValueA(verData.data(), "\\", (void **)&lpBuffer, &size))
		return;
	if (!size)
		return;

	auto verInfo = (VS_FIXEDFILEINFO *)lpBuffer;

	if (verInfo->dwSignature != 0xFEEF04BD)
		return;
	sprintf_s(
		modVersion,
		"%d.%d.%d.%d",
		( verInfo->dwFileVersionMS >> 16 ) & 0xffff,
		( verInfo->dwFileVersionMS >>  0 ) & 0xffff,
		( verInfo->dwFileVersionLS >> 16 ) & 0xffff,
		( verInfo->dwFileVersionLS >>  0 ) & 0xffff
	);
	if (( verInfo->dwFileVersionLS >>  0 ) & 0xffff)
		return;
	*strrchr(modVersion, '.') = 0;
}

void __declspec(naked) rollbackChecker()
{
	__asm {
		pushad
		pushfd
		call rollbackEvents
		popfd
		popad
		ret
	}
}

extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16]) {
	return memcmp(SokuLib::targetHash, hash, 16) == 0;
}

extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule) {
	DWORD old;
	if (!canInstallCheckKeyOneshotHook())
		return false;

#ifdef _DEBUG
	FILE *_;

	AllocConsole();
	freopen_s(&_, "CONOUT$", "w", stdout);
	freopen_s(&_, "CONOUT$", "w", stderr);
#endif
	wchar_t servHostW[sizeof(servHost)];
	wchar_t redirectIpW[sizeof(redirectIp)];

	loadSoku2Config();
	GetModuleFileNameW(hMyModule, profilePath, 1024);
	getModVersionStr();
	PathRemoveFileSpecW(profilePath);
	wcscpy(profileFolderPath, profilePath);
	Blocklist::initialize(profileFolderPath);
	PathAppendW(profilePath, L"SokuLobbies.ini");
	GetPrivateProfileStringW(L"Lobby", L"Host", L"pinkysmile.fr", servHostW, sizeof(servHost) / sizeof(*servHost), profilePath);
	GetPrivateProfileStringW(L"Lobby", L"RedirectIp", L"localhost", redirectIpW, sizeof(redirectIp) / sizeof(*redirectIp), profilePath);
	servPort = GetPrivateProfileIntW(L"Lobby", L"Port", 5254, profilePath);
	hostPort = GetPrivateProfileIntW(L"Lobby", L"HostPort", 10800, profilePath);
	chatKey = GetPrivateProfileIntW(L"Lobby", L"ChatKey", VK_RETURN, profilePath);
	{
		wchar_t language[16];
		GetPrivateProfileStringW(L"Lobby", L"Language", L"Chinese", language, sizeof(language) / sizeof(*language), profilePath);
		chineseLanguage = _wcsicmp(language, L"Chinese") == 0 || _wcsicmp(language, L"\u4E2D\u6587") == 0;
	}
	loadQuickMessages();
	lobbyJoinTries = GetPrivateProfileIntW(L"Lobby", L"JoinTries", 15, profilePath);
	lobbyJoinInterval = GetPrivateProfileIntW(L"Lobby", L"JoinInterval", 1, profilePath);
	maxChatMessages = GetPrivateProfileIntW(L"Lobby", L"MaxChatMessages", 100, profilePath);
	showTextBubbles = GetPrivateProfileIntW(L"Lobby", L"ShowTextBubbles", 1, profilePath) != 0;
	{
		wchar_t popupMode[32];
		GetPrivateProfileStringW(L"Lobby", L"ChatPopupMode", L"All", popupMode, sizeof(popupMode) / sizeof(*popupMode), profilePath);
		if (_wcsicmp(popupMode, L"Battle") == 0 || _wcsicmp(popupMode, L"Opponents") == 0 || wcscmp(popupMode, L"1") == 0)
			chatPopupMode = CHAT_POPUP_OPPONENTS;
		else if (_wcsicmp(popupMode, L"Never") == 0 || wcscmp(popupMode, L"2") == 0)
			chatPopupMode = CHAT_POPUP_NEVER;
		else
			chatPopupMode = CHAT_POPUP_ALL;
	}
	{
		wchar_t colorBuffer[32];
		GetPrivateProfileStringW(L"Lobby", L"OpponentChatColor", L"7FA6D9", colorBuffer, sizeof(colorBuffer) / sizeof(*colorBuffer), profilePath);
		std::wstring color{colorBuffer};
		color.erase(color.begin(), std::find_if(color.begin(), color.end(), [](wchar_t c){ return !std::iswspace(c); }));
		color.erase(std::find_if(color.rbegin(), color.rend(), [](wchar_t c){ return !std::iswspace(c); }).base(), color.end());
		if (!color.empty() && color.front() == L'#')
			color.erase(color.begin());
		else if (color.size() >= 2 && color[0] == L'0' && (color[1] == L'x' || color[1] == L'X'))
			color.erase(0, 2);
		if (color.size() == 6 && std::all_of(color.begin(), color.end(), [](wchar_t c){ return std::iswxdigit(c) != 0; }))
			opponentChatColor = std::wcstoul(color.c_str(), nullptr, 16);
	}
	lobbyJoinTries += !lobbyJoinTries;
	lobbyJoinInterval += !lobbyJoinInterval;

	bool hostlist = GetPrivateProfileIntW(L"Lobby", L"AcceptHostlist", 0, profilePath) != 0;

	if (hostlist)
		hostPref = Lobbies::HOSTPREF_NO_PREF;
	else
		hostPref = GetPrivateProfileIntW(L"Lobby", L"HostPref", Lobbies::HOSTPREF_NO_PREF, profilePath) & 3;
	hostPref |= (GetPrivateProfileIntW(L"Lobby", L"AcceptRelay", 1, profilePath) != 0) * Lobbies::HOSTPREF_ACCEPT_RELAY;
	hostPref |= (GetPrivateProfileIntW(L"Lobby", L"IsRanked", 1, profilePath) != 0) * Lobbies::HOSTPREF_PREFER_RANKED;
	hostPref |= hostlist * Lobbies::HOSTPREF_ACCEPT_HOSTLIST;
	printf("%S %i %i\n", profilePath, hostlist, hostPref);
	wcstombs(servHost, servHostW, sizeof(servHost));
	wcstombs(redirectIp, redirectIpW, sizeof(redirectIp));

	// DWORD old;
	VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	og_LogoOnProcess         = SokuLib::TamperDword(&SokuLib::VTable_Logo.onProcess,         LogoOnProcess);
	og_TitleOnProcess        = SokuLib::TamperDword(&SokuLib::VTable_Title.onProcess,        TitleOnProcess);
	og_SelectOnProcess       = SokuLib::TamperDword(&SokuLib::VTable_Select.onProcess,       SelectOnProcess);
	og_BattleOnProcess       = SokuLib::TamperDword(&SokuLib::VTable_Battle.onProcess,       BattleOnProcess);
	og_ConnectOnProcess      = SokuLib::TamperDword(&SokuLib::VTable_ConnectMenu.onProcess,  ConnectOnProcess);
	og_ConnectOnRender       = SokuLib::TamperDword(&SokuLib::VTable_ConnectMenu.onRender,   ConnectOnRender);
	og_ConnectOnUnknown      = SokuLib::union_cast<int (SokuLib::MenuConnect::*)()>(SokuLib::TamperDword(&SokuLib::VTable_ConnectMenu.unknown, ConnectOnUnknown));
	og_BattleWatchOnProcess  = SokuLib::TamperDword(&SokuLib::VTable_BattleWatch.onProcess,  BattleWatchOnProcess);
	og_BattleWatchOnRender   = SokuLib::TamperDword(&SokuLib::VTable_BattleWatch.onRender,   BattleWatchOnRender);
	og_LoadingWatchOnProcess = SokuLib::TamperDword(&SokuLib::VTable_LoadingWatch.onProcess, LoadingWatchOnProcess);
	og_LoadingWatchOnRender  = SokuLib::TamperDword(&SokuLib::VTable_LoadingWatch.onRender,  LoadingWatchOnRender);
	og_BattleClientOnProcess = SokuLib::TamperDword(&SokuLib::VTable_BattleClient.onProcess, BattleClientOnProcess);
	og_BattleClientOnRender  = SokuLib::TamperDword(&SokuLib::VTable_BattleClient.onRender,  BattleClientOnRender);
	og_LoadingClientOnProcess= SokuLib::TamperDword(&SokuLib::VTable_LoadingClient.onProcess,LoadingClientOnProcess);
	og_LoadingClientOnRender = SokuLib::TamperDword(&SokuLib::VTable_LoadingClient.onRender, LoadingClientOnRender);
	og_SelectClientOnProcess = SokuLib::TamperDword(&SokuLib::VTable_SelectClient.onProcess, SelectClientOnProcess);
	og_SelectClientOnRender  = SokuLib::TamperDword(&SokuLib::VTable_SelectClient.onRender,  SelectClientOnRender);
	og_BattleServerOnProcess = SokuLib::TamperDword(&SokuLib::VTable_BattleServer.onProcess, BattleServerOnProcess);
	og_BattleServerOnRender  = SokuLib::TamperDword(&SokuLib::VTable_BattleServer.onRender,  BattleServerOnRender);
	og_LoadingServerOnProcess= SokuLib::TamperDword(&SokuLib::VTable_LoadingServer.onProcess,LoadingServerOnProcess);
	og_LoadingServerOnRender = SokuLib::TamperDword(&SokuLib::VTable_LoadingServer.onRender, LoadingServerOnRender);
	og_SelectServerOnProcess = SokuLib::TamperDword(&SokuLib::VTable_SelectServer.onProcess, SelectServerOnProcess);
	og_SelectServerOnRender  = SokuLib::TamperDword(&SokuLib::VTable_SelectServer.onRender,  SelectServerOnRender);
	og_BattleMgrOnProcess    = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.onProcess,CBattleManager_OnProcess);
	s_origRecvFrom           = SokuLib::TamperDword(&SokuLib::DLL::ws2_32.recvfrom, BlocklistRecvFrom);
	//og_BattleMgrOnRender  = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.onRender,  CBattleManager_OnRender);
	VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, old, &old);

	VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	if (!installCheckKeyOneshotHook()) {
		VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, old, &old);
		return false;
	}
	s_origKeymapManager_SetInputs = SokuLib::union_cast<void (SokuLib::KeymapManager::*)()>(SokuLib::TamperNearJmpOpr(0x40A45D, KeymapManagerSetInputs));

	if (*((uint8_t*)0x40104c) != 0xe8) {
		memcpy((void *)0x40104c, "\xe8\x00\x00\x00\x00\x50\x90", 7);
		SokuLib::TamperNearJmpOpr(0x40104c, Hooked_EndScene);
		Original_EndScene = nullptr;
	} else
		Original_EndScene = (EndSceneFn)SokuLib::TamperNearJmpOpr(0x40104c, Hooked_EndScene);
	if (*(unsigned char *)0x407E6A != 0x90) {
		*(unsigned char *)0x407E41 = 0x28;
		memset((void *)0x407E6A, 0x90, 52);
		SokuLib::TamperNearCall(0x407E6B, onUpdate);
		og_onUpdate = nullptr;
	} else
		og_onUpdate = SokuLib::TamperNearJmpOpr(0x407E6B, onUpdate);
	new SokuLib::Trampoline(0x469C77, (void (*)())onCardUsed, 7);
	// Check for GR hook, and if so, hook on top to check for rollbacks
	if (*(unsigned char *)0x482701 == 0xE9) {
		// First, we force the hook into a call, instead of a JMP
		*(char *)0x482701 = 0xE8;
		// GR doesn't clean that byte so we nop it
		*(char *)0x482706 = 0x90;
		// We grab the start of the hook in memory
		int hookAddr = *(int *)0x482702 + 0x482706;
		// And we replace the footer so that it calls our function instead of jumping back
		SokuLib::TamperNearJmp(hookAddr + 0x1A, rollbackChecker);
	}
	og_call48AD33 = SokuLib::TamperNearJmpOpr(0x48AD33, coinUsed);
	VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, old, &old);

	FlushInstructionCache(GetCurrentProcess(), nullptr, 0);

	HMODULE hntdll = GetModuleHandle("ntdll.dll");
	if(hntdll){
		auto wine_get_version = (char *(__cdecl *)(void)) GetProcAddress(hntdll, "wine_get_version");
		if (wine_get_version)
			wineVersion = wine_get_version();
	}

	return true;
}

extern "C" __declspec(dllexport) int getPriority()
{
	return -50000;
}

extern "C" int APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	return TRUE;
}
