//
// Created by PinkySmile on 31/10/2020
//

#include <SokuLib.hpp>
#include <shlwapi.h>
#include "LobbyMenu.hpp"

static int (SokuLib::MenuConnect::*og_ConnectOnProcess)();

int __fastcall ConnectOnProcess(SokuLib::MenuConnect *This)
{
	auto res = (This->*og_ConnectOnProcess)();

	if (SokuLib::inputMgrs.input.changeCard == 1) {
		SokuLib::activateMenu(new LobbyMenu(This));
		SokuLib::playSEWaveBuffer(0x28);
	}
	return res;
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

	// DWORD old;
	::VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	og_ConnectOnProcess = SokuLib::TamperDword(&SokuLib::VTable_ConnectMenu.onProcess, ConnectOnProcess);
	//ogBattleMgrOnRender  = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.onRender,  CBattleManager_OnRender);
	//ogBattleMgrOnProcess = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.onProcess, CBattleManager_OnProcess);
	::VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, old, &old);

	::VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	::VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, old, &old);

	::FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
	return true;
}

extern "C" int APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	return TRUE;
}