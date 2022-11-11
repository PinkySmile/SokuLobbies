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

static bool wasBlocking = false;
static int (SokuLib::Logo::*og_LogoOnProcess)();
static int (SokuLib::MenuConnect::*og_ConnectOnProcess)();
static int (SokuLib::BattleWatch::*og_BattleWatchOnProcess)();
static int (SokuLib::BattleWatch::*og_BattleWatchOnRender)();
static int (SokuLib::LoadingWatch::*og_LoadingWatchOnProcess)();
static int (SokuLib::LoadingWatch::*og_LoadingWatchOnRender)();
static int (SokuLib::BattleClient::*og_BattleClientOnProcess)();
static int (SokuLib::BattleClient::*og_BattleClientOnRender)();
static int (SokuLib::SelectClient::*og_SelectClientOnProcess)();
static int (SokuLib::SelectClient::*og_SelectClientOnRender)();
static int (SokuLib::BattleServer::*og_BattleServerOnProcess)();
static int (SokuLib::BattleServer::*og_BattleServerOnRender)();
static int (SokuLib::SelectServer::*og_SelectServerOnProcess)();
static int (SokuLib::SelectServer::*og_SelectServerOnRender)();
static void (SokuLib::KeymapManager::*s_origKeymapManager_SetInputs)();

wchar_t profilePath[MAX_PATH];
wchar_t profileFolderPath[MAX_PATH];
char servHost[64];
unsigned short servPort;
bool hasSoku2 = false;
auto load = std::pair(false, false);

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

int __fastcall ConnectOnProcess(SokuLib::MenuConnect *This)
{
	auto res = (This->*og_ConnectOnProcess)();

	if (*(byte*)0x0448e4a != 0x30 && SokuLib::inputMgrs.input.changeCard == 1) {
		try {
			SokuLib::activateMenu(new LobbyMenu(This));
			SokuLib::playSEWaveBuffer(0x28);
		} catch (std::exception &e) {
			MessageBox(SokuLib::window, e.what(), "Loading error", MB_ICONERROR);
			SokuLib::playSEWaveBuffer(0x29);
		}
	}
	return res;
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

void processCommon(bool showChat)
{
	if (!activeMenu)
		return;
	if (showChat)
		activeMenu->updateChat();
}
void renderCommon(bool showChat)
{
	if (!activeMenu)
		return;
	if (showChat)
		activeMenu->renderChat();
}

int __fastcall BattleWatchOnProcess(SokuLib::BattleWatch *This)
{
	processCommon(true);
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
	return (This->*og_BattleClientOnProcess)();
}
int __fastcall SelectClientOnProcess(SokuLib::SelectClient *This)
{
	processCommon(true);
	return (This->*og_SelectClientOnProcess)();
}
int __fastcall BattleServerOnProcess(SokuLib::BattleServer *This)
{
	processCommon(false);
	return (This->*og_BattleServerOnProcess)();
}
int __fastcall SelectServerOnProcess(SokuLib::SelectServer *This)
{
	processCommon(true);
	return (This->*og_SelectServerOnProcess)();
}


int __fastcall BattleWatchOnRender(SokuLib::BattleWatch *This)
{
	auto ret = (This->*og_BattleWatchOnRender)();

	renderCommon(true);
	return ret;
}
int __fastcall LoadingWatchOnRender(SokuLib::LoadingWatch *This)
{
	auto ret = (This->*og_LoadingWatchOnRender)();

	renderCommon(true);
	return ret;
}
int __fastcall BattleClientOnRender(SokuLib::BattleClient *This)
{
	auto ret = (This->*og_BattleClientOnRender)();

	renderCommon(false);
	return ret;
}
int __fastcall SelectClientOnRender(SokuLib::SelectClient *This)
{
	auto ret = (This->*og_SelectClientOnRender)();

	renderCommon(true);
	return ret;
}
int __fastcall BattleServerOnRender(SokuLib::BattleServer *This)
{
	auto ret = (This->*og_BattleServerOnRender)();

	renderCommon(false);
	return ret;
}
int __fastcall SelectServerOnRender(SokuLib::SelectServer *This)
{
	auto ret = (This->*og_SelectServerOnRender)();

	renderCommon(true);
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

extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16]) {
	return true;
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
	wcstombs(servHost, servHostW, sizeof(servHost));

	// DWORD old;
	VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	og_LogoOnProcess         = SokuLib::TamperDword(&SokuLib::VTable_Logo.onProcess,         LogoOnProcess);
	og_ConnectOnProcess      = SokuLib::TamperDword(&SokuLib::VTable_ConnectMenu.onProcess,  ConnectOnProcess);
	og_BattleWatchOnProcess  = SokuLib::TamperDword(&SokuLib::VTable_BattleWatch.onProcess,  BattleWatchOnProcess);
	og_BattleWatchOnRender   = SokuLib::TamperDword(&SokuLib::VTable_BattleWatch.onRender,   BattleWatchOnRender);
	og_LoadingWatchOnProcess = SokuLib::TamperDword(&SokuLib::VTable_LoadingWatch.onProcess, LoadingWatchOnProcess);
	og_LoadingWatchOnRender  = SokuLib::TamperDword(&SokuLib::VTable_LoadingWatch.onRender,  LoadingWatchOnRender);
	og_BattleClientOnProcess = SokuLib::TamperDword(&SokuLib::VTable_BattleClient.onProcess, BattleClientOnProcess);
	og_BattleClientOnRender  = SokuLib::TamperDword(&SokuLib::VTable_BattleClient.onRender,  BattleClientOnRender);
	og_SelectClientOnProcess = SokuLib::TamperDword(&SokuLib::VTable_SelectClient.onProcess, SelectClientOnProcess);
	og_SelectClientOnRender  = SokuLib::TamperDword(&SokuLib::VTable_SelectClient.onRender,  SelectClientOnRender);
	og_BattleServerOnProcess = SokuLib::TamperDword(&SokuLib::VTable_BattleServer.onProcess, BattleServerOnProcess);
	og_BattleServerOnRender  = SokuLib::TamperDword(&SokuLib::VTable_BattleServer.onRender,  BattleServerOnRender);
	og_SelectServerOnProcess = SokuLib::TamperDword(&SokuLib::VTable_SelectServer.onProcess, SelectServerOnProcess);
	og_SelectServerOnRender  = SokuLib::TamperDword(&SokuLib::VTable_SelectServer.onRender,  SelectServerOnRender);
	//ogBattleMgrOnRender  = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.onRender,  CBattleManager_OnRender);
	//ogBattleMgrOnProcess = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.onProcess, CBattleManager_OnProcess);
	VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, old, &old);

	VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	s_origKeymapManager_SetInputs = SokuLib::union_cast<void (SokuLib::KeymapManager::*)()>(SokuLib::TamperNearJmpOpr(0x40A45D, KeymapManagerSetInputs));
	VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, old, &old);

	FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
	return true;
}

extern "C" int APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	return TRUE;
}