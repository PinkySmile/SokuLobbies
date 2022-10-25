//
// Created by PinkySmile on 31/10/2020
//

#include <SokuLib.hpp>
#include <shlwapi.h>
#include "LobbyMenu.hpp"
#include "InLobbyMenu.hpp"

static bool wasBlocking = false;
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
	processCommon(true);
	return (This->*og_BattleClientOnProcess)();
}
int __fastcall SelectClientOnProcess(SokuLib::SelectClient *This)
{
	processCommon(true);
	return (This->*og_SelectClientOnProcess)();
}
int __fastcall BattleServerOnProcess(SokuLib::BattleServer *This)
{
	processCommon(true);
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

	GetModuleFileNameW(hMyModule, profilePath, 1024);
	PathRemoveFileSpecW(profilePath);
	wcscpy(profileFolderPath, profilePath);
	PathAppendW(profilePath, L"SokuLobbies.ini");
	GetPrivateProfileStringW(L"Lobby", L"Host", L"pinkysmile.fr", servHostW, sizeof(servHost), profilePath);
	servPort = GetPrivateProfileIntW(L"Lobby", L"Port", 5254, profilePath);
	wcstombs(servHost, servHostW, sizeof(servHost));

	// DWORD old;
	VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	og_ConnectOnProcess = SokuLib::TamperDword(&SokuLib::VTable_ConnectMenu.onProcess, ConnectOnProcess);
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