//
// Created by PinkySmile on 31/10/2020
//

#include <sstream>
#include <fstream>
#include <SokuLib.hpp>
#include <shlwapi.h>
#include "data.hpp"
#include "LobbyMenu.hpp"
#include "InLobbyMenu.hpp"
#include "LobbyData.hpp"
#include "InputBox.hpp"
#include "dinput.h"

typedef HRESULT(__stdcall* EndSceneFn)(IDirect3DDevice9*);

static EndSceneFn Original_EndScene;
static bool wasBlocking = false;
static void (*og_onUpdate)();
static int (SokuLib::Logo::*og_LogoOnProcess)();
static int (SokuLib::Title::*og_TitleOnProcess)();
static int (SokuLib::Battle::*og_BattleOnProcess)();
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
static void (SokuLib::KeymapManager::*s_origKeymapManager_SetInputs)();

wchar_t profilePath[MAX_PATH];
wchar_t profileFolderPath[MAX_PATH];
char servHost[64];
unsigned hostPref;
unsigned short servPort;
unsigned short hostPort;
bool hasSoku2 = false;
bool counted = false;
bool activated = true;
auto load = std::pair(false, false);
#ifdef _DEBUG
bool debug = true;
#endif
std::function<int ()> onGameEnd;
LobbyMenu *menu = nullptr;
PTOP_LEVEL_EXCEPTION_FILTER oldFilter = nullptr;

std::map<unsigned int, Character> characters{
	{ SokuLib::CHARACTER_REIMU,     {"Reimu",     "Reimu Hakurei",          "reimu"}},
	{ SokuLib::CHARACTER_MARISA,    {"Marisa",    "Marisa Kirisame",        "marisa"}},
	{ SokuLib::CHARACTER_SAKUYA,    {"Sakuya",    "Sakuya Izayoi",          "sakuya"}},
	{ SokuLib::CHARACTER_ALICE,     {"Alice",     "Alice Margatroid",       "alice"}},
	{ SokuLib::CHARACTER_PATCHOULI, {"Patchouli", "Patchouli Knowledge",    "patchouli"}},
	{ SokuLib::CHARACTER_YOUMU,     {"Youmu",     "Youmu Konpaku",          "youmu"}},
	{ SokuLib::CHARACTER_REMILIA,   {"Remilia",   "Remilia Scarlet",        "remilia"}},
	{ SokuLib::CHARACTER_YUYUKO,    {"Yuyuko",    "Yuyuko Saigyouji",       "yuyuko"}},
	{ SokuLib::CHARACTER_YUKARI,    {"Yukari",    "Yukari Yakumo",          "yukari"}},
	{ SokuLib::CHARACTER_SUIKA,     {"Suika",     "Suika Ibuki",            "suika"}},
	{ SokuLib::CHARACTER_REISEN,    {"Reisen",    "Reisen Undongein Inaba", "udonge"}},
	{ SokuLib::CHARACTER_AYA,       {"Aya",       "Aya Shameimaru",         "aya"}},
	{ SokuLib::CHARACTER_KOMACHI,   {"Komachi",   "Komachi Onozuka",        "komachi"}},
	{ SokuLib::CHARACTER_IKU,       {"Iku",       "Iku Nagae",              "iku"}},
	{ SokuLib::CHARACTER_TENSHI,    {"Tenshi",    "Tenshi Hinanawi",        "tenshi"}},
	{ SokuLib::CHARACTER_SANAE,     {"Sanae",     "Sanae Kochiya",          "sanae"}},
	{ SokuLib::CHARACTER_CIRNO,     {"Cirno",     "Cirno",                  "chirno"}},
	{ SokuLib::CHARACTER_MEILING,   {"Meiling",   "Hong Meiling",           "meirin"}},
	{ SokuLib::CHARACTER_UTSUHO,    {"Utsuho",    "Utsuho Reiuji",          "utsuho"}},
	{ SokuLib::CHARACTER_SUWAKO,    {"Suwako",    "Suwako Moriya",          "suwako"}},
	{ SokuLib::CHARACTER_NAMAZU,    {"Namazu",    "Giant Catfish",          "namazu"}},
	{ SokuLib::CHARACTER_RANDOM,    {"Random",    "Random Select",          "random_select"}},
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
	auto &opp = SokuLib::mainMode == SokuLib::BATTLE_MODE_VSSERVER ? battle.leftCharacterManager : battle.rightCharacterManager;
	auto data = lobbyData->loadedCharacterStats.find(mid);

	if (data == lobbyData->loadedCharacterStats.end()) {
		LobbyData::CharacterStatEntry entry{0, 0, 0, 0};

		lobbyData->loadedCharacterStats[mid] = entry;
		data = lobbyData->loadedCharacterStats.find(mid);
	}
	data->second.wins += chr.score >= 2;
	data->second.losses += chr.score < 2;

	auto &wins = lobbyData->achievementByRequ["win"];
	auto &loss = lobbyData->achievementByRequ["lose"];
	auto &play = lobbyData->achievementByRequ["play"];
	auto it = std::find_if(wins.begin(), wins.end(), [mid, &data](LobbyData::Achievement *achievement){
		return !achievement->awarded && achievement->requirement["chr"] == mid && achievement->requirement["count"] <= data->second.wins;
	});

	if (it != wins.end()) {
		(*it)->awarded = true;
		lobbyData->achievementAwardQueue.push_back(*it);
	}

	it = std::find_if(loss.begin(), loss.end(), [mid, &data](LobbyData::Achievement *achievement){
		return !achievement->awarded && achievement->requirement["chr"] == mid && achievement->requirement["count"] <= data->second.losses;
	});
	if (it != loss.end()) {
		(*it)->awarded = true;
		lobbyData->achievementAwardQueue.push_back(*it);
	}

	it = std::find_if(play.begin(), play.end(), [mid, &data](LobbyData::Achievement *achievement){
		return !achievement->awarded && achievement->requirement["chr"] == mid && achievement->requirement["count"] <= data->second.losses + data->second.wins;
	});
	if (it != play.end()) {
		(*it)->awarded = true;
		lobbyData->achievementAwardQueue.push_back(*it);
	}

	lobbyData->saveAchievements();

	data = lobbyData->loadedCharacterStats.find(oid);
	if (data == lobbyData->loadedCharacterStats.end()) {
		LobbyData::CharacterStatEntry entry{0, 0, 0, 0};

		lobbyData->loadedCharacterStats[oid] = entry;
		data = lobbyData->loadedCharacterStats.find(oid);
	}
	data->second.againstWins += chr.score >= 2;
	data->second.againstLosses += chr.score < 2;
	lobbyData->saveStats();
	if (lobbyData->achievementsLocked)
		return;
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
	static bool placed = false;
	auto ret = (This->*og_TitleOnProcess)();

	if (!placed) {
		placed = true;
		puts("Placed handler");
		oldFilter = SetUnhandledExceptionFilter(UnhandledExFilter);
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

int __fastcall BattleOnProcess(SokuLib::Battle *This)
{
	auto ret = (This->*og_BattleOnProcess)();
	auto &mgr = SokuLib::getBattleMgr();

	if (mgr.matchState > 3 && !counted) {
		counted = true;
		if (onGameEnd)
			ret = onGameEnd();
		else
			countGame();
	}
	if (ret != SokuLib::SCENE_BATTLE)
		counted = false;
	return ret;
}
int __fastcall BattleWatchOnProcess(SokuLib::BattleWatch *This)
{
	processCommon(false);
	return (This->*og_BattleWatchOnProcess)();
}
int __fastcall LoadingWatchOnProcess(SokuLib::LoadingWatch *This)
{
	processCommon(true);
	return (This->*og_LoadingWatchOnProcess)();
}
int __fastcall BattleClientOnProcess(SokuLib::BattleClient *This)
{
	processCommon(false);

	auto ret = (This->*og_BattleClientOnProcess)();
	auto &mgr = SokuLib::getBattleMgr();

	if (mgr.matchState > 4 && !counted) {
		counted = true;
		if (onGameEnd)
			ret = onGameEnd();
		else
			countGame();
	}
	if (ret != SokuLib::SCENE_BATTLECL)
		counted = false;
	return ret;
}
int __fastcall SelectClientOnProcess(SokuLib::SelectClient *This)
{
	processCommon(true);
	return (This->*og_SelectClientOnProcess)();
}
int __fastcall LoadingClientOnProcess(SokuLib::LoadingClient *This)
{
	processCommon(true);
	return (This->*og_LoadingClientOnProcess)();
}
int __fastcall BattleServerOnProcess(SokuLib::BattleServer *This)
{
	processCommon(false);

	auto ret = (This->*og_BattleServerOnProcess)();
	auto &mgr = SokuLib::getBattleMgr();

	if (mgr.matchState > 4 && !counted) {
		counted = true;
		if (onGameEnd)
			ret = onGameEnd();
		else
			countGame();
	}
	if (ret != SokuLib::SCENE_BATTLESV)
		counted = false;
	return ret;
}
int __fastcall SelectServerOnProcess(SokuLib::SelectServer *This)
{
	processCommon(true);
	return (This->*og_SelectServerOnProcess)();
}
int __fastcall LoadingServerOnProcess(SokuLib::LoadingServer *This)
{
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

		std::getline(str, idStr, ';');
		std::getline(str, codeName, ';');
		std::getline(str, shortName, ';');
		std::getline(str, fullName, ';');
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
	}
}

void onUpdate()
{
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
	wchar_t **arg_list = CommandLineToArgvW(GetCommandLineW(), &argc);

	wcsncpy(app_path, arg_list[0], MAX_PATH);
	PathRemoveFileSpecW(app_path);
	if (GetEnvironmentVariableW(L"SWRSTOYS", setting_path, sizeof(setting_path)) <= 0) {
		if (arg_list && argc > 1 && StrStrIW(arg_list[1], L"ini")) {
			wcscpy(setting_path, arg_list[1]);
			LocalFree(arg_list);
		} else {
			wcscpy(setting_path, app_path);
			PathAppendW(setting_path, L"\\SWRSToys.ini");
		}
		if (arg_list) {
			LocalFree(arg_list);
		}
	}
	printf("Config file is %S\n", setting_path);

	wchar_t moduleKeys[1024];
	wchar_t moduleValue[MAX_PATH];
	GetPrivateProfileStringW(L"Module", nullptr, nullptr, moduleKeys, sizeof(moduleKeys), setting_path);
	for (wchar_t *key = moduleKeys; *key; key += wcslen(key) + 1) {
		wchar_t module_path[MAX_PATH];

		GetPrivateProfileStringW(L"Module", key, nullptr, moduleValue, sizeof(moduleValue), setting_path);

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
		wcscpy(module_path, app_path);
		PathAppendW(module_path, moduleValue);
		while (auto result = wcschr(module_path, '/'))
			*result = '\\';
		PathRemoveFileSpecW(module_path);
		printf("Found Soku2 module folder at %S\n", module_path);
		PathAppendW(module_path, L"\\config\\info\\characters.csv");
		loadSoku2CSV(module_path);
		return;
	}
}

static void __fastcall KeymapManagerSetInputs(SokuLib::KeymapManager *This)
{
	(This->*s_origKeymapManager_SetInputs)();
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
	static bool b = false;
	
	b = !b;
	if (SokuLib::sceneId != SokuLib::SCENE_LOGO && lobbyData && b) {
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
		lobbyData->render();
	}

	//This is necessary, so we can fit in the hook...
	//That said, the return value is never even checked in soku.
	if (!Original_EndScene)
		(*(EndSceneFn**)pDevice)[42](pDevice);
	else
		Original_EndScene(pDevice);
	return 0x8a0e14;
}

extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16]) {
	return memcmp(SokuLib::targetHash, hash, 16) == 0;
}

extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule) {
	DWORD old;

#ifdef _DEBUG
	FILE *_;

	AllocConsole();
	freopen_s(&_, "CONOUT$", "w", stdout);
	freopen_s(&_, "CONOUT$", "w", stderr);
#endif
	wchar_t servHostW[sizeof(servHost)];

	loadSoku2Config();
	GetModuleFileNameW(hMyModule, profilePath, 1024);
	PathRemoveFileSpecW(profilePath);
	wcscpy(profileFolderPath, profilePath);
	PathAppendW(profilePath, L"SokuLobbies.ini");
	GetPrivateProfileStringW(L"Lobby", L"Host", L"pinkysmile.fr", servHostW, sizeof(servHost), profilePath);
	servPort = GetPrivateProfileIntW(L"Lobby", L"Port", 5254, profilePath);
	hostPort = GetPrivateProfileIntW(L"Lobby", L"HostPort", 10800, profilePath);

	bool hostlist = GetPrivateProfileIntW(L"Lobby", L"AcceptHostlist", 0, profilePath) != 0;

	// By default HOSTPREF_NO_PREF | HOSTPREF_ACCEPT_RELAY | HOSTPREF_ACCEPT_HOSTLIST
	hostPref = GetPrivateProfileIntW(L"Lobby", L"HostPref", hostlist ? Lobbies::HOSTPREF_HOST_ONLY : Lobbies::HOSTPREF_NO_PREF, profilePath);
	hostPref |= (GetPrivateProfileIntW(L"Lobby", L"AcceptRelay", 1, profilePath) != 0) * Lobbies::HOSTPREF_ACCEPT_RELAY;
	hostPref |= (GetPrivateProfileIntW(L"Lobby", L"IsRanked", 1, profilePath) != 0) * Lobbies::HOSTPREF_PREFER_RANKED;
	hostPref |= hostlist * Lobbies::HOSTPREF_ACCEPT_HOSTLIST;
	printf("%S %i %i\n", profilePath, hostlist, hostPref);
	wcstombs(servHost, servHostW, sizeof(servHost));

	// DWORD old;
	VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	og_LogoOnProcess         = SokuLib::TamperDword(&SokuLib::VTable_Logo.onProcess,         LogoOnProcess);
	og_TitleOnProcess        = SokuLib::TamperDword(&SokuLib::VTable_Title.onProcess,        TitleOnProcess);
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
	//ogBattleMgrOnRender  = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.onRender,  CBattleManager_OnRender);
	//ogBattleMgrOnProcess = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.onProcess, CBattleManager_OnProcess);
	VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, old, &old);

	VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
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
	VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, old, &old);

	FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
	return true;
}

extern "C" __declspec(dllexport) int getPriority()
{
	return -50000;
}

extern "C" int APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	return TRUE;
}