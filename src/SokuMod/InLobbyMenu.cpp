//
// Created by PinkySmile on 02/10/2022.
//

#include <dinput.h>
#include <filesystem>
#include <random>
#include "InLobbyMenu.hpp"
#include "LobbyData.hpp"
#include "data.hpp"
#include "SmallHostlist.hpp"
#include "encodingConverter.hpp"
#include "createUTFTexture.hpp"

#define CHAT_CHARACTER_LIMIT 512
#define BOX_TEXTURE_SIZE {0x2000, 30}
#define TEXTURE_MAX_SIZE 344
#define CURSOR_ENDX 637
#define CURSOR_STARTX 293
#define CURSOR_STARTY 184
#define MAX_LINE_SIZE 342
#define SCROLL_AMOUNT 20
#define CHAT_FONT_HEIGHT 14

struct CDesignSprite {
	void *vftable; // =008576ac
	float UNKNOWN_1[2];
	float x;
	float y;
	uint8_t active;
	uint8_t UNKNOWN_2[3];
	int32_t UNKNOWN_3;
};

auto &messageBox = *(CDesignSprite**)0x89a390;
InLobbyMenu *activeMenu = nullptr;
static WNDPROC Original_WndProc = nullptr;
static std::mutex ptrMutex;
static std::mt19937 random;

static LRESULT __stdcall Hooked_WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (
		uMsg == WM_IME_NOTIFY &&
		wParam == IMN_PRIVATE && (
			lParam == 1 || lParam == 2 ||
			lParam == 16|| lParam == 17||
			lParam == 26|| lParam == 27|| lParam == 28
		)
	)
		return 0;

	unsigned eventList[] = {
		WM_IME_STARTCOMPOSITION,
		WM_IME_ENDCOMPOSITION,
		WM_INPUTLANGCHANGE,
		WM_IME_COMPOSITION,
		WM_IME_NOTIFY,
		WM_KEYDOWN,
		WM_KEYUP
	};
	const size_t size = sizeof(eventList) / sizeof(*eventList);

	if (std::find(eventList, eventList + size, uMsg) == eventList + size)
		return CallWindowProc(Original_WndProc, hWnd, uMsg, wParam, lParam);
	if (uMsg == WM_IME_NOTIFY) {
		if (wParam == IMN_SETCONVERSIONMODE) {
			ptrMutex.lock();
			if (!activeMenu) {
				ptrMutex.unlock();
				return CallWindowProc(Original_WndProc, hWnd, uMsg, wParam, lParam);
			}
			printf("WM_IME_NOTIFY IMN_SETCONVERSIONMODE %x\n", (UINT)lParam);
			activeMenu->immCtx = nullptr;
			activeMenu->immComposition.clear();
			activeMenu->compositionCursor = 0;
			ptrMutex.unlock();
		}
		return CallWindowProc(Original_WndProc, hWnd, uMsg, wParam, lParam);
	}
	ptrMutex.lock();
	if (!activeMenu) {
		ptrMutex.unlock();
		return CallWindowProc(Original_WndProc, hWnd, uMsg, wParam, lParam);
	}
	if (uMsg == WM_INPUTLANGCHANGE) {
		printf("WM_INPUTLANGCHANGE %x %x\n", (UINT)wParam, (UINT)lParam);
		activeMenu->immComposition.clear();
	} else if (uMsg == WM_IME_STARTCOMPOSITION) {
		printf("WM_IME_STARTCOMPOSITION %x %x\n", (UINT)wParam, (UINT)lParam);
		activeMenu->immCtx = ImmGetContext(SokuLib::window);
		// This disables the windows builtin IME window.
		// For now we keep it because part of its features are not properly supported.
		//ptrMutex.unlock();
		//return 0;
	} else if (uMsg == WM_IME_ENDCOMPOSITION) {
		MSG compositionMsg;

		printf("WM_IME_ENDCOMPOSITION %x %x\n", (UINT)wParam, (UINT)lParam);
		if (
			::PeekMessage(&compositionMsg, hWnd, WM_IME_STARTCOMPOSITION, WM_IME_COMPOSITION, PM_NOREMOVE) &&
			compositionMsg.message == WM_IME_COMPOSITION &&
			(compositionMsg.lParam & GCS_RESULTSTR)
		) {
			ptrMutex.unlock();
			return CallWindowProc(Original_WndProc, hWnd, uMsg, wParam, lParam);
		}
		ImmReleaseContext(SokuLib::window, activeMenu->immCtx);
		activeMenu->immCtx = nullptr;
		activeMenu->immComposition.clear();
		activeMenu->compositionCursor = 0;
		// This disables the windows builtin IME window.
		// For now we keep it because part of its features are not properly supported.
		//ptrMutex.unlock();
		//return 0;
	} else if (uMsg == WM_IME_COMPOSITION) {
		printf("WM_IME_COMPOSITION %x %x\n", (UINT)wParam, (UINT)lParam);
		activeMenu->onKeyReleased();
		if (lParam & GCS_RESULTSTR) {
			auto required = ImmGetCompositionStringW(activeMenu->immCtx, GCS_RESULTSTR, nullptr, 0);
			auto immStr = (wchar_t *)malloc(required);
			bool v = false;

			printf("GCS_RESULTSTR Required %li\n", required);
			assert(required % 2 == 0);
			ImmGetCompositionStringW(activeMenu->immCtx, GCS_RESULTSTR, immStr, required);
			activeMenu->addString(immStr, required / 2);
			free(immStr);
			activeMenu->onCompositionResult();
			activeMenu->immComposition.clear();
			activeMenu->textChanged |= 3;
		}
		//required = ImmGetCompositionStringW(activeMenu->immCtx, GCS_COMPATTR, nullptr, 0);
		//printf("GCS_COMPATTR: Required %li\n", required);
		if (lParam & GCS_COMPSTR) {
			auto required = ImmGetCompositionStringW(activeMenu->immCtx, GCS_COMPSTR, nullptr, 0);

			activeMenu->immComposition.resize(required);
			ImmGetCompositionStringW(
				activeMenu->immCtx,
				GCS_COMPSTR,
				activeMenu->immComposition.data(),
				activeMenu->immComposition.size() * sizeof(*activeMenu->immComposition.data())
			);
			activeMenu->textChanged |= 2;
		}
		if (lParam & GCS_CURSORPOS) {
			activeMenu->compositionCursor = ImmGetCompositionStringW(
				activeMenu->immCtx,
				GCS_CURSORPOS,
				nullptr,
				0
			);
			activeMenu->textChanged |= 4;
		}
	} else if (uMsg == WM_KEYDOWN) {
		BYTE keyboardState[256];
		wchar_t old[2];

		memcpy(old, activeMenu->keyBuffer, sizeof(old));
		GetKeyboardState(keyboardState);
		activeMenu->keyBufferUsed = ToUnicode((UINT)wParam, lParam, keyboardState, activeMenu->keyBuffer, 2, 0);

		auto key = MapVirtualKey(wParam, MAPVK_VK_TO_CHAR);

		printf("KEYDOWN %u %x %x %i %i %i\n", key, (UINT)wParam, (UINT)lParam, activeMenu->keyBuffer[0], activeMenu->keyBuffer[1], activeMenu->keyBufferUsed);
		if (activeMenu->keyBufferUsed > 0)
			activeMenu->onKeyPressed(UTF16Decode(std::wstring(activeMenu->keyBuffer, activeMenu->keyBuffer + activeMenu->keyBufferUsed))[0]);
	} else if (uMsg == WM_KEYUP)
		activeMenu->onKeyReleased();
	ptrMutex.unlock();
	return CallWindowProc(Original_WndProc, hWnd, uMsg, wParam, lParam);
}

InLobbyMenu::InLobbyMenu(LobbyMenu *menu, SokuLib::MenuConnect *parent, Connection &connection) :
	_connection(connection),
	_parent(parent),
	_menu(menu)
{
	SokuLib::FontDescription desc;
	bool hasEnglishPatch = (*(int *)0x411c64 == 1);

	desc.r1 = 255;
	desc.r2 = 255;
	desc.g1 = 255;
	desc.g2 = 255;
	desc.b1 = 255;
	desc.b2 = 255;
	desc.height = CHAT_FONT_HEIGHT + hasEnglishPatch * 2;
	desc.weight = FW_NORMAL;
	desc.italic = 0;
	desc.shadow = 1;
	desc.bufferSize = 1000000;
	desc.charSpaceX = 0;
	desc.charSpaceY = hasEnglishPatch * -2;
	desc.offsetX = 0;
	desc.offsetY = 0;
	desc.useOffset = 0;
	strcpy(desc.faceName, "Tahoma");
	desc.weight = FW_REGULAR;

	this->_chatFont.create();
	this->_chatFont.setIndirect(desc);

	this->_inBattle.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/lobby/inarcade.png").string().c_str());
	this->_inBattle.setSize({
		this->_inBattle.texture.getSize().x,
		this->_inBattle.texture.getSize().y
	});
	this->_inBattle.rect.width = this->_inBattle.texture.getSize().x;
	this->_inBattle.rect.height = this->_inBattle.texture.getSize().y;
	this->_inBattle.setPosition({174, 218});

	this->_chatSeat.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/lobby/chat_seat.png").string().c_str());
	this->_chatSeat.setSize({
		this->_chatSeat.texture.getSize().x,
		this->_chatSeat.texture.getSize().y
	});
	this->_chatSeat.rect.width = this->_chatSeat.texture.getSize().x;
	this->_chatSeat.rect.height = this->_chatSeat.texture.getSize().y;
	this->_chatSeat.setPosition({290, 0});
	this->_chatSeat.tint = SokuLib::Color{0xFF, 0xFF, 0xFF, 0};

	this->_loadingText.texture.createFromText("Joining Lobby...", lobbyData->getFont(16), {300, 74});
	this->_loadingText.setSize({
		this->_loadingText.texture.getSize().x,
		this->_loadingText.texture.getSize().y
	});
	this->_loadingText.rect.width = this->_loadingText.texture.getSize().x;
	this->_loadingText.rect.height = this->_loadingText.texture.getSize().y;
	this->_loadingText.setPosition({174, 218});

	this->_messageBox.texture.loadFromGame("data/menu/21_Base.cv2");
	this->_messageBox.setSize({
		this->_messageBox.texture.getSize().x,
		this->_messageBox.texture.getSize().y
	});
	this->_messageBox.rect.width = this->_messageBox.texture.getSize().x;
	this->_messageBox.rect.height = this->_messageBox.texture.getSize().y;
	this->_messageBox.setPosition({155, 203});

	this->_loadingGear.texture.loadFromGame("data/scene/logo/gear.bmp");
	this->_loadingGear.setSize({
		this->_loadingGear.texture.getSize().x,
		this->_loadingGear.texture.getSize().y
	});
	this->_loadingGear.rect.width = this->_loadingGear.texture.getSize().x;
	this->_loadingGear.rect.height = this->_loadingGear.texture.getSize().y;

	this->_textCursor.setSize({1, 14});
	this->_textCursor.setPosition({CURSOR_STARTX, CURSOR_STARTY});
	this->_textCursor.setFillColor(SokuLib::Color::White);
	this->_textCursor.setBorderColor(SokuLib::Color::Transparent);

	this->_textSprite[0].rect.width = CURSOR_ENDX - CURSOR_STARTX;
	this->_textSprite[0].rect.height = 18;
	this->_textSprite[0].setSize(SokuLib::Vector2i{
		this->_textSprite[0].rect.width,
		this->_textSprite[0].rect.height
	}.to<unsigned>());
	this->_textSprite[0].setPosition({CURSOR_STARTX - (*(int *)0x411c64 == 1) * 2, CURSOR_STARTY});

	auto &bg = lobbyData->backgrounds.front();

	this->onConnectRequest = connection.onConnectRequest;
	this->onError = connection.onError;
	this->onImpMsg = connection.onImpMsg;
	this->onMsg = connection.onMsg;
	this->onHostRequest = connection.onHostRequest;
	this->onConnect = connection.onConnect;
	this->_machines.reserve((bg.fg.getSize().x / 200 - 1) * bg.platformCount + 1);

	int id = 0;

	for (int i = ' '; i < 0x100; i++)
		this->_getTextSize(i);
	for (int j = 1; j < bg.platformCount + 1; j++)
		for (int i = 200; i < bg.fg.getSize().x; i += 200)
			this->_machines.emplace_back(
				id++,
				SokuLib::Vector2i{i, static_cast<int>(bg.fg.getSize().y - bg.groundPos + j * bg.platformInterval)},
				&lobbyData->arcades.intro,
				lobbyData->arcades.skins[1]
			);
	this->_machines.emplace_back(
		UINT32_MAX,
		SokuLib::Vector2i{static_cast<int>(bg.fg.getSize().x - 50), static_cast<int>(bg.fg.getSize().y - bg.groundPos)},
		&lobbyData->arcades.hostlist,
		lobbyData->arcades.skins[0]
	);
	connection.onPlayerJoin = [this](const Player &r){
		SokuLib::Vector2i size;

		this->_extraPlayerData[r.id].name.texture.createFromText(r.name.c_str(), lobbyData->getFont(16), {200, 20}, &size);
		this->_extraPlayerData[r.id].name.setSize(size.to<unsigned>());
		this->_extraPlayerData[r.id].name.rect.width = size.x;
		this->_extraPlayerData[r.id].name.rect.height = size.y;
	};
	connection.onConnect = [this, &connection](const Lobbies::PacketOlleh &r){
		this->_background = r.bg;
		connection.getMe()->pos.y = lobbyData->backgrounds[r.bg].fg.getSize().y - lobbyData->backgrounds[r.bg].groundPos;

		SokuLib::Vector2i size;

		this->_roomName = std::string(r.name, strnlen(r.name, sizeof(r.name)));
		this->_extraPlayerData[r.id].name.texture.createFromText(std::string(r.realName, strnlen(r.realName, sizeof(r.realName))).c_str(), lobbyData->getFont(16), {200, 20}, &size);
		this->_extraPlayerData[r.id].name.setSize(size.to<unsigned>());
		this->_extraPlayerData[r.id].name.rect.width = size.x;
		this->_extraPlayerData[r.id].name.rect.height = size.y;
		this->_music = "data/bgm/" + std::string(r.music, strnlen(r.music, sizeof(r.music))) + ".ogg";
		SokuLib::playBGM(this->_music.c_str());
	};
	connection.onConnectRequest = [this](const std::string &ip, unsigned short port, bool spectate){
		SokuLib::playSEWaveBuffer(57);
		this->_connection.getMe()->battleStatus = 2;
		this->_parent->joinHost(ip.c_str(), port, spectate);
	};
	connection.onError = [this](const std::string &msg){
		SokuLib::playSEWaveBuffer(38);
		this->_wasConnected = true;
		MessageBox(SokuLib::window, msg.c_str(), "Internal Error", MB_ICONERROR);
	};
	connection.onImpMsg = [this](const std::string &msg){
		SokuLib::playSEWaveBuffer(23);
		MessageBox(SokuLib::window, msg.c_str(), "Notification from server", MB_ICONINFORMATION);
	};
	connection.onMsg = [this](int32_t channel, int32_t player, const std::string &msg){
		SokuLib::playSEWaveBuffer(49);
		this->_addMessageToList(channel, player, msg);
	};
	connection.onHostRequest = [this]{
		SokuLib::playSEWaveBuffer(57);
		this->_connection.getMe()->battleStatus = 2;
		if (this->_parent->choice == 0)
			this->_parent->setupHost(hostPort, true);
		return hostPort;
	};
	connection.onArcadeEngage = [this](const Player &, uint32_t id){
		if (id >= this->_machines.size() - 1)
			return;

		auto &machine = this->_machines[id];

		machine.mutex.lock();
		machine.playerCount++;
		if (machine.playerCount == 1) {
			machine.animation = 0;
			machine.animationCtr = 0;
			machine.animIdle = false;
			machine.currentAnim = &lobbyData->arcades.select;
		} else if (machine.playerCount == 2) {
			machine.animation = 0;
			machine.animationCtr = 0;
			machine.animIdle = false;
			machine.currentAnim = &lobbyData->arcades.game[random() % lobbyData->arcades.game.size()];
		}
		machine.mutex.unlock();
	};
	connection.onArcadeLeave = [this](const Player &, uint32_t id){
		if (id >= this->_machines.size() - 1)
			return;

		auto &machine = this->_machines[id];

		machine.mutex.lock();
		machine.playerCount--;
		if (machine.playerCount == 1) {
			machine.animation = 0;
			machine.animationCtr = 0;
			machine.animIdle = false;
			machine.currentAnim = &lobbyData->arcades.select;
		} else if (machine.playerCount == 0) {
			machine.animIdle = true;
			machine.animationCtr = 0;
			machine.currentAnim = &lobbyData->arcades.intro;
			machine.animation = machine.currentAnim->frameCount - 1;
		}
		machine.mutex.unlock();
	};
	connection.connect();
	ptrMutex.lock();
	activeMenu = this;
	ptrMutex.unlock();
	if (!Original_WndProc) {
		Original_WndProc = (WNDPROC) SetWindowLongPtr(SokuLib::window, GWL_WNDPROC, (LONG_PTR) Hooked_WndProc);
		//ImmDisableIME(GetCurrentThreadId());
	}
}

InLobbyMenu::~InLobbyMenu()
{
	ptrMutex.lock();
	activeMenu = nullptr;
	ptrMutex.unlock();
	if (this->immCtx)
		ImmReleaseContext(SokuLib::window, this->immCtx);
	this->_unhook();
	this->_connection.disconnect();
	this->_menu->setActive();
	if (!this->_music.empty())
		SokuLib::playBGM("data/bgm/op2.ogg");
	if (this->_hostThread.joinable())
		this->_hostThread.join();
}

void InLobbyMenu::_()
{
	Lobbies::PacketArcadeLeave leave{0};

	this->_connection.send(&leave, sizeof(leave));
	*(*(char **)0x89a390 + 20) = false;
	this->_parent->choice = 0;
	this->_parent->subchoice = 0;
	messageBox->active = false;
	*(int *)0x882a94 = 0x16;
	SokuLib::playBGM(this->_music.c_str());
}

int InLobbyMenu::onProcess()
{
	if (!this->_connection.isInit() && !this->_wasConnected) {
		this->_loadingGear.setRotation(this->_loadingGear.getRotation() + 0.1);
		return this->_connection.isConnected();
	}
	if (!this->_connection.isInit())
		return false;

	this->_connection.meMutex.lock();
	auto inputs = SokuLib::inputMgrs.input;
	auto me = this->_connection.getMe();

	memset(&SokuLib::inputMgrs.input, 0, sizeof(SokuLib::inputMgrs.input));
	// We call MenuConnect::onProcess directly because we don't want to trigger any hook.
	// After all, we are not technically inside the connect menu.
	reinterpret_cast<void (__thiscall *)(SokuLib::MenuConnect *)>(0x449160)(this->_parent);
	SokuLib::inputMgrs.input = inputs;
	if (this->_hostlist && !this->_hostlist->update()) {
		SokuLib::playBGM(this->_music.c_str());
		this->_hostlist.reset();
		this->_currentMachine = nullptr;
		SokuLib::playSEWaveBuffer(0x29);
		this->_connection.meMutex.unlock();
		return true;
	}
	this->updateChat();
	if (this->_parent->choice > 0) {
		if (
			this->_parent->subchoice == 5 || //Already Playing
			this->_parent->subchoice == 10   //Connect Failed
		) {
			Lobbies::PacketArcadeLeave leave{0};

			this->_connection.send(&leave, sizeof(leave));
			*(*(char **)0x89a390 + 20) = false;
			this->_addMessageToList(0xFF0000, 0, "Failed connecting to opponent: " + std::string(this->_parent->subchoice == 5 ? "They are already playing" : "Connection failed"));
			this->_parent->choice = 0;
			this->_parent->subchoice = 0;
			messageBox->active = false;
		}
	}
	if (!this->_editingText && SokuLib::checkKeyOneshot(DIK_ESCAPE, 0, 0, 0)) {
		SokuLib::playSEWaveBuffer(0x29);
		if (!this->_editingText) {
			this->_connection.meMutex.unlock();
			this->_unhook();
			this->_connection.disconnect();
			if (this->_parent->choice == SokuLib::MenuConnect::CHOICE_HOST) {
				memset(&SokuLib::inputMgrs.input, 0, sizeof(SokuLib::inputMgrs.input));
				SokuLib::inputMgrs.input.b = 1;
				reinterpret_cast<void (__thiscall *)(SokuLib::MenuConnect *)>(0x449160)(this->_parent);
				SokuLib::inputMgrs.input = inputs;
			} else
				SokuLib::playSEWaveBuffer(0x29);
			this->_parent->choice = 0;
			this->_parent->subchoice = 0;
			return false;
		} else
			this->_editingText = false;
	}

	auto &bg = lobbyData->backgrounds[this->_background];

	if (!this->_currentMachine && !this->_editingText) {
		if (SokuLib::inputMgrs.input.b == 1) {
			SokuLib::playSEWaveBuffer(0x29);
			this->_connection.meMutex.unlock();
			this->_unhook();
			this->_connection.disconnect();
			return false;
		}
		if (SokuLib::inputMgrs.input.a == 1) {
			for (auto &machine : this->_machines) {
				if (me->pos.x < machine.pos.x - machine.skin.sprite.getSize().x / 2)
					continue;
				if (me->pos.y < machine.pos.y)
					continue;
				if (me->pos.x > machine.pos.x + machine.skin.sprite.getSize().x / 2)
					continue;
				if (me->pos.y > machine.pos.y + machine.skin.sprite.getSize().y)
					continue;
				this->_currentMachine = &machine;
				SokuLib::playSEWaveBuffer(0x28);
				if (machine.id == UINT32_MAX) {
					this->_hostlist.reset(new SmallHostlist(0.6, {128, 48}, this->_parent));
					SokuLib::playBGM("data/bgm/op2.ogg");
					break;
				}

				Lobbies::PacketGameRequest packet{machine.id};

				me->battleStatus = 1;
				this->_connection.send(&packet, sizeof(packet));
				printf("%i\n", me->settings.hostPref);
				if (me->settings.hostPref & Lobbies::HOSTPREF_ACCEPT_HOSTLIST)
					this->_startHosting();
				break;
			}
		}

		bool dirChanged;

		if (SokuLib::inputMgrs.input.horizontalAxis) {
			auto newDir = me->dir;

			if (SokuLib::inputMgrs.input.horizontalAxis < 0 && me->pos.x < PLAYER_H_SPEED) {
				SokuLib::playSEWaveBuffer(0x29);
				this->_connection.meMutex.unlock();
				this->_connection.disconnect();
				return false;
			}
			newDir &= 0b01100;
			newDir |= 0b00001 << (SokuLib::inputMgrs.input.horizontalAxis < 0);
			if (SokuLib::inputMgrs.input.horizontalAxis < 0)
				newDir |= 0b10000;
			if (me->pos.x >= bg.fg.getSize().x - 20)
				newDir &= 0b11110;
			dirChanged = newDir != me->dir;
			me->dir = newDir;
		} else {
			dirChanged = (me->dir & 0b00011) != 0;
			me->dir &= 0b11100;
		}
		if (SokuLib::inputMgrs.input.verticalAxis) {
			auto newDir = me->dir;

			newDir &= 0b10011;
			newDir |= 0b00100 << (SokuLib::inputMgrs.input.verticalAxis > 0);
			if (me->pos.y <= bg.fg.getSize().y - bg.groundPos)
				newDir &= 0b10111;
			dirChanged = newDir != me->dir;
			me->dir = newDir;
		} else {
			dirChanged |= (me->dir & 0b01100) != 0;
			me->dir &= 0b10011;
		}
		if (dirChanged) {
			Lobbies::PacketMove l{0, me->dir};

			this->_connection.send(&l, sizeof(l));
		}
	} else {
		if (me->dir & 0b1111) {
			me->dir &= 0b10000;

			Lobbies::PacketMove m{0, me->dir};

			this->_connection.send(&m, sizeof(m));
		}
		if (SokuLib::inputMgrs.input.b == 1 && !this->_editingText && !this->_hostlist) {
			Lobbies::PacketArcadeLeave l{0};

			this->_connection.send(&l, sizeof(l));
			me->battleStatus = 0;
			this->_currentMachine = nullptr;
			if (this->_parent->choice == SokuLib::MenuConnect::CHOICE_HOST) {
				memset(&SokuLib::inputMgrs.input, 0, sizeof(SokuLib::inputMgrs.input));
				SokuLib::inputMgrs.input.b = 1;
				reinterpret_cast<void (__thiscall *)(SokuLib::MenuConnect *)>(0x449160)(this->_parent);
				SokuLib::inputMgrs.input = inputs;
			} else
				SokuLib::playSEWaveBuffer(0x29);
			this->_parent->choice = 0;
			this->_parent->subchoice = 0;
			messageBox->active = false;
		}
	}
	for (auto &machine : this->_machines) {
		machine.mutex.lock();
		if (machine.animIdle)
			goto checkSkinAnim;
		machine.animationCtr++;
		if (machine.animationCtr < 60 / machine.currentAnim->frameRate)
			goto checkSkinAnim;
		machine.animationCtr = 0;
		machine.animation++;
		if (machine.animation < machine.currentAnim->frameCount)
			goto checkSkinAnim;
		if (machine.currentAnim->loop)
			machine.animation = 0;
		else {
			machine.animIdle = true;
			machine.animation--;
		}
	checkSkinAnim:
		machine.skinAnimationCtr++;
		if (machine.skinAnimationCtr < 60 / machine.skin.frameRate)
			goto done;
		machine.skinAnimationCtr = 0;
		machine.skinAnimation++;
		if (machine.skinAnimation < machine.skin.frameCount)
			goto done;
		machine.skinAnimation = 0;
	done:
		machine.mutex.unlock();
	}

	this->_connection.updatePlayers(lobbyData->avatars);
	if (me->pos.x < 320)
		this->_translate.x = 0;
	else if (me->pos.x > bg.fg.getSize().x - 320)
		this->_translate.x = 640 - bg.fg.getSize().x;
	else
		this->_translate.x = 320 - me->pos.x;
	if (me->pos.y < 140)
		this->_translate.y = 0;
	else
		this->_translate.y = me->pos.y - 140;
	this->_connection.meMutex.unlock();
	return true;
}

int InLobbyMenu::onRender()
{
	if (!this->_connection.isInit() && !this->_wasConnected) {
		this->_messageBox.draw();
		this->_loadingText.draw();
		this->_loadingGear.setRotation(-this->_loadingGear.getRotation());
		this->_loadingGear.setPosition({412, 227});
		this->_loadingGear.draw();
		this->_loadingGear.setRotation(-this->_loadingGear.getRotation());
		this->_loadingGear.setPosition({412 + 23, 227 - 18});
		this->_loadingGear.draw();
		return 0;
	}

	auto &bg = lobbyData->backgrounds[this->_background];

	bg.bg.setPosition({
		static_cast<int>(this->_translate.x / bg.parallaxFactor),
		static_cast<int>(this->_translate.y / bg.parallaxFactor) - static_cast<int>(bg.bg.getSize().y) + 480
	});
	bg.bg.draw();
	bg.fg.setPosition({
		this->_translate.x,
		this->_translate.y - static_cast<int>(bg.fg.getSize().y) + 480
	});
	bg.fg.draw();
	for (auto &machine : this->_machines) {
		SokuLib::Vector2i pos{
			static_cast<int>(machine.pos.x) - static_cast<int>(machine.skin.sprite.getSize().x / 2) + this->_translate.x,
			480 - static_cast<int>(machine.pos.y + machine.skin.sprite.getSize().y) + this->_translate.y
		};

		machine.mutex.lock();
		machine.skin.sprite.setPosition(pos);
		machine.skin.sprite.rect.left = machine.skinAnimation * machine.skin.sprite.rect.width;
		machine.skin.sprite.draw();
		pos += machine.skin.animationOffsets;
		machine.currentAnim->sprite.setPosition(pos);
		if (machine.currentAnim->tilePerLine) {
			machine.currentAnim->sprite.rect.left = machine.animation % machine.currentAnim->tilePerLine * machine.currentAnim->size.x;
			machine.currentAnim->sprite.rect.top = machine.animation / machine.currentAnim->tilePerLine * machine.currentAnim->size.y;
		}
		machine.currentAnim->sprite.draw();
		machine.mutex.unlock();
	}

	auto players = this->_connection.getPlayers();
	SokuLib::DrawUtils::RectangleShape rect2;
#ifdef _DEBUG
	SokuLib::DrawUtils::RectangleShape rect;

	rect.setBorderColor(SokuLib::Color::White);
	rect.setFillColor(SokuLib::Color{0xFF, 0xFF, 0xFF, 0xA0});
#endif
	rect2.setBorderColor(SokuLib::Color::Black);
	rect2.setFillColor(SokuLib::Color{0x00, 0x00, 0x00, 0xA0});
	for (auto &player : players) {
		if (player.player.avatar < lobbyData->avatars.size()) {
			auto &avatar = lobbyData->avatars[player.player.avatar];

			avatar.sprite.setPosition({
				static_cast<int>(player.pos.x) - static_cast<int>(avatar.sprite.getSize().x / 2) + this->_translate.x,
				480 - static_cast<int>(player.pos.y + avatar.sprite.getSize().y) + this->_translate.y
			});
			avatar.sprite.rect.top = avatar.sprite.rect.height * player.animation;
			avatar.sprite.rect.left = player.currentAnimation * avatar.sprite.rect.width;
			avatar.sprite.setMirroring((player.dir & 0b10000) == 0, false);
		#ifdef _DEBUG
			rect.setSize(avatar.sprite.getSize());
			rect.setPosition(avatar.sprite.getPosition());
			rect.draw();
		#endif
			avatar.sprite.draw();
		} else {
			rect2.setSize({64, 64});
			rect2.setPosition({
				static_cast<int>(player.pos.x) - 32 + this->_translate.x,
				480 - static_cast<int>(player.pos.y + 64) + this->_translate.y
			});
			rect2.draw();
		}
	}
	for (auto &player : players) {
		auto &avatar = lobbyData->avatars[player.player.avatar];

		this->_extraPlayerData[player.id].name.setPosition({
			static_cast<int>(player.pos.x) - static_cast<int>(this->_extraPlayerData[player.id].name.getSize().x / 2) + this->_translate.x,
			500 - static_cast<int>(player.pos.y + avatar.sprite.getSize().y) + this->_translate.y
		});
		this->_extraPlayerData[player.id].name.draw();

		if (player.battleStatus) {
			this->_inBattle.setPosition({
				static_cast<int>(player.pos.x) - static_cast<int>(this->_inBattle.getSize().x / 2) + this->_translate.x,
				480 - static_cast<int>(player.pos.y + avatar.sprite.getSize().y + this->_inBattle.getSize().y) + this->_translate.y
			});
			this->_inBattle.draw();
		}
	}
	if (this->_currentMachine)
		this->_renderMachineOverlay();
	if (!this->_hostlist)
		this->renderChat();
	return 0;
}

void InLobbyMenu::_unhook()
{
	this->_connection.onConnectRequest = this->onConnectRequest;
	this->_connection.onError = this->onError;
	this->_connection.onImpMsg = this->onImpMsg;
	this->_connection.onMsg = this->onMsg;
	this->_connection.onHostRequest = this->onHostRequest;
	this->_connection.onConnect = this->onConnect;
	this->_connection.onPlayerJoin = this->onPlayerJoin;
	this->_connection.onArcadeEngage = this->onArcadeEngage;
	this->_connection.onArcadeLeave = this->onArcadeLeave;
}

void InLobbyMenu::_addMessageToList(unsigned int channel, unsigned player, const std::string &msg)
{
	this->_chatTimer = 900;
	this->_chatMessages.emplace_front();

	auto *m = &this->_chatMessages.front();
	std::string line;
	std::string word;
	std::string token;
	unsigned startPos = 0;
	unsigned pos = 0;
	unsigned wordPos = 0;
	unsigned skip = 0;
	unsigned short emoteId;
	unsigned char emoteCtr = 0;
	auto pushText = [&]{
		if (line.empty())
			return;

		m->text.emplace_back();

		auto &txt = m->text.back();
		int texId = 0;

		if (player == 0)
			txt.sprite.tint = channel;
		//txt.sprite.texture.createFromText(line.c_str(), this->_chatFont, {350, 300}, &txt.realSize);
		if (!createTextTexture(texId, convertEncoding<char, wchar_t, UTF8Decode, UTF16Encode>(line).c_str(), this->_chatFont, {350, 300}, &txt.realSize))
			puts("Error creating text texture");
		txt.sprite.texture.setHandle(texId, {350, 300});
		txt.sprite.rect.width = txt.realSize.x;
		txt.sprite.rect.height = txt.realSize.y;
		txt.pos.x = startPos;
		if (!m->emotes.empty())
			txt.pos.y = (txt.realSize.y - EMOTE_SIZE) / 2;
		printf("Created sprite %x %i,%i %ux%u (%ux%u) %08x\n", texId, txt.pos.x, txt.pos.y, txt.realSize.x, txt.realSize.y, txt.sprite.texture.getSize().x, txt.sprite.texture.getSize().y, txt.sprite.tint);
		startPos = pos;
		line.clear();
	};
	auto nextLine = [&]{
		pushText();
		this->_chatMessages.emplace_front();
		m = &this->_chatMessages.front();
		pos = 0;
	};

	line.reserve(msg.size());
	word.reserve(msg.size());
	for (unsigned char c : msg) {
		if (emoteCtr) {
			emoteId |= (c & 0x7F) << ((2 - emoteCtr) * 7);
			emoteCtr--;
			if (!emoteCtr) {
				if (pos + EMOTE_SIZE > MAX_LINE_SIZE) {
					nextLine();
					startPos = 0;
				}
				m->emotes.emplace_back();
				m->emotes.back().id = emoteId;
				m->emotes.back().pos.x = startPos;
				pos += EMOTE_SIZE;
				startPos = pos;
				for (auto &g : m->text)
					g.pos.y = (g.realSize.y - EMOTE_SIZE) / 2;
			}
		} else if (skip) {
			if ((c & 0b11000000) == 0x80) {
				skip--;
				token += c;
			} else
				skip = 0;
			word += c;
			if (skip != 0)
				continue;
			wordPos += this->_getTextSize(UTF8Decode(token)[0]);
		} else if (c == 1) {
			line += word;
			pos += wordPos;
			wordPos = 0;
			word.clear();
			pushText();
			emoteId = 0;
			emoteCtr = 2;
			continue;
		} else if (c == '\n') {
			line += word;
			pos += wordPos;
			word.clear();
			wordPos = 0;
			nextLine();
			startPos = 0;
			continue;
		} else if (c >= 0x80) {
			skip = c >= 0xC0;
			skip += c >= 0xE0;
			skip += c >= 0xF0;
			token.clear();
			token += c;
			word += c;
			continue;
		} else if (isspace(c)) {
			if (word.empty()) {
				if (pos == 0)
					continue;
			} else {
				line += word;
				pos += wordPos;
				word.clear();
				wordPos = 0;
			}
			line += ' ';
			pos += this->_getTextSize(' ');
		} else {
			word += c;
			wordPos += this->_getTextSize(c);
		}
		if (pos + wordPos > MAX_LINE_SIZE) {
			if (pos == 0) {
				line = word.substr(0, word.size() - 1);
				word.erase(word.begin(), word.end() - 1);
				wordPos = this->_getTextSize(UTF8Decode(word)[0]);
			}
			nextLine();
			startPos = 0;
		}
	}
	line += word;
	pushText();
}

void InLobbyMenu::onKeyPressed(unsigned chr)
{
	if (chr == 0x7F || chr < 32 || !this->_editingText)
		return;
	this->_textMutex.lock();
	if (this->_lastPressed && this->_textTimer == 0) {
		std::basic_string<unsigned> s{&this->_lastPressed, &this->_lastPressed + 1};
		auto result = UTF16Encode(s);

		if (result.size() + this->_buffer.size() <= CHAT_CHARACTER_LIMIT) {
			this->_buffer.insert(this->_buffer.begin() + this->_textCursorPosIndex, result.begin(), result.end());
			this->_updateTextCursor(this->_textCursorPosIndex + 1);
			this->textChanged |= 1;
			SokuLib::playSEWaveBuffer(0x27);
		} else
			SokuLib::playSEWaveBuffer(0x29);
	}
	this->_lastPressed = chr;
	this->_textTimer = 0;
	this->_textMutex.unlock();
}

void InLobbyMenu::onKeyReleased()
{
	this->_textMutex.lock();
	if (this->_lastPressed && this->_textTimer == 0) {
		std::basic_string<unsigned> s{&this->_lastPressed, &this->_lastPressed + 1};
		auto result = UTF16Encode(s);

		if (result.size() + this->_buffer.size() <= CHAT_CHARACTER_LIMIT) {
			this->_buffer.insert(this->_buffer.begin() + this->_textCursorPosIndex, result.begin(), result.end());
			this->_updateTextCursor(this->_textCursorPosIndex + 1);
			SokuLib::playSEWaveBuffer(0x27);
			this->textChanged |= 1;
		} else
			SokuLib::playSEWaveBuffer(0x29);
	}
	this->_lastPressed = 0;
	this->_textTimer = 0;
	this->_textMutex.unlock();
}

void InLobbyMenu::_inputBoxUpdate()
{
	if (GetForegroundWindow() != SokuLib::window)
		return;
	for (size_t i = 0; i < 256; i++) {
		int j = GetAsyncKeyState(i);
		BYTE current = j >> 8 | j & 1;

		if (current & 0x80)
			this->_timers[i]++;
		else
			this->_timers[i] = 0;
	}
	if (this->_timers[VK_RETURN] == 1)
		this->_returnPressed = true;
	if (this->_timers[VK_PRIOR] == 1 || (this->_timers[VK_PRIOR] > 36 && this->_timers[VK_PRIOR] % 6 == 0)) {
		SokuLib::playSEWaveBuffer(0x27);
		this->_chatOffset += SCROLL_AMOUNT;
		this->_chatTimer = max(this->_chatTimer, 180);
	}
	if (this->_timers[VK_NEXT] == 1 || (this->_timers[VK_NEXT] > 36 && this->_timers[VK_NEXT] % 6 == 0)) {
		if (this->_chatOffset < SCROLL_AMOUNT)
			this->_chatOffset = 0;
		else
			this->_chatOffset -= SCROLL_AMOUNT;
		this->_chatTimer = max(this->_chatTimer, 180);
		SokuLib::playSEWaveBuffer(0x27);
	}
	if (!this->_editingText) {
		if (this->_returnPressed && this->_timers[VK_RETURN] == 0) {
			this->_editingText = true;
			this->_initInputBox();
			SokuLib::playSEWaveBuffer(0x28);
		}
		return;
	}
	if (this->_timers[VK_UP] == 1 || (this->_timers[VK_UP] > 36 && this->_timers[VK_UP] % 6 == 0)) {
		SokuLib::playSEWaveBuffer(0x27);
		this->_chatOffset += SCROLL_AMOUNT;
		this->_chatTimer = max(this->_chatTimer, 180);
		return;
	}
	if (this->_timers[VK_DOWN] == 1 || (this->_timers[VK_DOWN] > 36 && this->_timers[VK_DOWN] % 6 == 0)) {
		if (this->_chatOffset < SCROLL_AMOUNT)
			this->_chatOffset = 0;
		else
			this->_chatOffset -= SCROLL_AMOUNT;
		this->_chatTimer = max(this->_chatTimer, 180);
		SokuLib::playSEWaveBuffer(0x27);
		return;
	}
	if (this->_returnPressed) {
		if (this->_timers[VK_RETURN] == 0) {
			if (this->immComposition.empty()) {
				if (this->_buffer.size() != 1) {
					this->_sendMessage(std::wstring{this->_buffer.begin(), this->_buffer.end() - 1});
					SokuLib::playSEWaveBuffer(0x28);
				} else
					SokuLib::playSEWaveBuffer(0x29);
				this->_editingText = false;
				this->_chatOffset = 0;
			}
			this->_returnPressed = false;
		}
		return;
	}
	this->_textMutex.lock();
	if (this->immComposition.empty()) {
		if (this->_timers[VK_HOME] == 1) {
			SokuLib::playSEWaveBuffer(0x27);
			this->_updateTextCursor(0);
		}
		if (this->_timers[VK_END] == 1) {
			SokuLib::playSEWaveBuffer(0x27);
			this->_updateTextCursor(this->_buffer.size() - 1);
		}
		if (this->_timers[VK_BACK] == 1 || (this->_timers[VK_BACK] > 36 && this->_timers[VK_BACK] % 6 == 0)) {
			if (this->_textCursorPosIndex != 0) {
				this->_buffer.erase(this->_buffer.begin() + this->_textCursorPosIndex - 1);
				this->_updateTextCursor(this->_textCursorPosIndex - 1);
				this->textChanged |= 1;
				SokuLib::playSEWaveBuffer(0x27);
			}
		}
		if (this->_timers[VK_DELETE] == 1 || (this->_timers[VK_DELETE] > 36 && this->_timers[VK_DELETE] % 6 == 0)) {
			if (this->_textCursorPosIndex < this->_buffer.size() - 1) {
				this->_buffer.erase(this->_buffer.begin() + this->_textCursorPosIndex);
				SokuLib::playSEWaveBuffer(0x27);
				this->textChanged |= 1;
			}
		}
		if (this->_timers[VK_LEFT] == 1 || (this->_timers[VK_LEFT] > 36 && this->_timers[VK_LEFT] % 3 == 0)) {
			if (this->_textCursorPosIndex != 0) {
				this->_updateTextCursor(this->_textCursorPosIndex - 1);
				SokuLib::playSEWaveBuffer(0x27);
			}
		}
		if (this->_timers[VK_RIGHT] == 1 || (this->_timers[VK_RIGHT] > 36 && this->_timers[VK_RIGHT] % 3 == 0)) {
			if (this->_textCursorPosIndex != this->_buffer.size() - 1) {
				this->_updateTextCursor(this->_textCursorPosIndex + 1);
				SokuLib::playSEWaveBuffer(0x27);
			}
		}
		if (this->_lastPressed) {
			this->_textTimer++;
			if (this->_textTimer == 1 || (this->_textTimer > 36 && this->_textTimer % 6 == 0)) {
				std::basic_string<unsigned> s{&this->_lastPressed, &this->_lastPressed + 1};
				auto result = UTF16Encode(s);

				if (result.size() + this->_buffer.size() <= CHAT_CHARACTER_LIMIT) {
					this->_buffer.insert(this->_buffer.begin() + this->_textCursorPosIndex, result.begin(), result.end());
					this->_updateTextCursor(this->_textCursorPosIndex + 1);
					SokuLib::playSEWaveBuffer(0x27);
					this->textChanged |= 1;
				} else
					SokuLib::playSEWaveBuffer(0x29);
			}
		}
	}
	if (this->textChanged)
		this->_updateCompositionSprite();
	this->_textMutex.unlock();
}

void InLobbyMenu::_initInputBox()
{
	int ret;

	SokuLib::playSEWaveBuffer(0x28);
	memset(this->_timers, 0, sizeof(this->_timers));
	this->_lastPressed = 0;
	this->_textTimer = 0;
	this->_buffer.clear();
	this->_buffer.push_back(0);

	this->textChanged = 3;
	this->_updateCompositionSprite();

	this->_textCursor.setPosition({CURSOR_STARTX, CURSOR_STARTY});
	this->_textCursorPosSize = 0;
	this->_textCursorPosIndex = 0;
	this->_textSprite[0].rect.left = 0;
	this->_editingText = true;
	this->_returnPressed = false;

	CANDIDATEFORM candidate;
	RECT rect;

	GetWindowRect(SokuLib::window, &rect);

	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;
	float xRatio = width / 640.f;
	float yRatio = height / 480.f;
	POINT result = {
		static_cast<LONG>(this->_textCursor.getPosition().x * xRatio),
		static_cast<LONG>(this->_textCursor.getPosition().y * yRatio + this->_textCursor.getSize().y)
	};

	candidate.dwIndex = 0;
	candidate.dwStyle = CFS_CANDIDATEPOS;
	candidate.ptCurrentPos = result;
	ImmSetCandidateWindow(this->immCtx, &candidate);
}

void InLobbyMenu::_updateTextCursor(int pos)
{
	int computedSize = getTextSize(this->_buffer.substr(0, pos).c_str(), this->_chatFont, BOX_TEXTURE_SIZE).x;
	int newX = this->_textCursor.getPosition().x + computedSize - this->_textCursorPosSize;

	if (newX > CURSOR_ENDX) {
		//TODO
		this->_textSprite[0].rect.left += newX - CURSOR_ENDX;
		this->_textCursor.setPosition({CURSOR_ENDX, CURSOR_STARTY});
	} else if (newX < CURSOR_STARTX) {
		//TODO
		this->_textSprite[0].rect.left += newX - CURSOR_STARTX;
		this->_textCursor.setPosition({CURSOR_STARTX, CURSOR_STARTY});
	} else
		this->_textCursor.setPosition({newX, CURSOR_STARTY});
	this->_textCursorPosIndex = pos;
	this->_textCursorPosSize = computedSize;

	CANDIDATEFORM candidate;
	RECT rect;

	GetWindowRect(SokuLib::window, &rect);

	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;
	float xRatio = width / 640.f;
	float yRatio = height / 480.f;
	POINT result = {
		static_cast<LONG>(this->_textCursor.getPosition().x * xRatio),
		static_cast<LONG>(this->_textCursor.getPosition().y * yRatio + this->_textCursor.getSize().y)
	};

	candidate.dwIndex = 0;
	candidate.dwStyle = CFS_CANDIDATEPOS;
	candidate.ptCurrentPos = result;
	ImmSetCandidateWindow(this->immCtx, &candidate);
}

void InLobbyMenu::_sendMessage(const std::wstring &msg)
{
	std::string encoded;
	std::wstring token;
	std::wstring currentEmote;
	bool colon = false;
	bool skip = false;

	encoded.reserve(msg.size());
	token.reserve(msg.size());
	for (auto c : msg) {
		if (!skip && c == ':') {
			colon = !colon;
			if (colon)
				continue;

			auto it = lobbyData->emotesByName.find(convertEncoding<wchar_t, char, UTF16Decode, UTF8Encode>(currentEmote));

			if (it == lobbyData->emotesByName.end() || lobbyData->isLocked(*it->second)) {
				token += L':';
				token += currentEmote;
				token += L':';
			} else {
				auto nb = it->second->id;

				encoded += convertEncoding<wchar_t, char, UTF16Decode, UTF8Encode>(token);
				encoded += '\x01';
				for (int i = 0; i < 2; i++) {
					encoded += static_cast<char>(nb & 0x7F | 0x80);
					nb >>= 7;
				}
				token.clear();
			}
			currentEmote.clear();
		} else
			(colon ? currentEmote : token) += c;
	}
	encoded += convertEncoding<wchar_t, char, UTF16Decode, UTF8Encode>(token);
	if (colon) {
		encoded += ':';
		encoded += convertEncoding<wchar_t, char, UTF16Decode, UTF8Encode>(currentEmote);
	}

	size_t pos = encoded.find("bgs");

	if (
		pos != std::string::npos &&
		(pos == 0 || !isalpha(encoded[pos-1])) &&
		(pos + 3 == encoded.size() - 1 || !isalpha(encoded[pos + 3]))
	) {
		encoded.erase(encoded.begin() + pos, encoded.begin() + pos + 3);
		encoded.insert(pos, "GGs, thanks for the games. It was very nice playing with you, let's play again later");
	}

	Lobbies::PacketMessage msgPacket{0, 0, encoded};

	this->_connection.send(&msgPacket, sizeof(msgPacket));
}

void InLobbyMenu::updateChat()
{
	this->_inputBoxUpdate();
	if (this->_editingText)
		this->_chatTimer = 300;
	if (this->_chatTimer) {
		this->_chatTimer--;

		unsigned char alpha = this->_chatTimer > 120 ? 255 : (this->_chatTimer * 255 / 120);

		this->_chatSeat.tint.a = alpha;

		auto remaining = this->_chatOffset;
		SokuLib::Vector2i pos{292, 180};

		for (auto &msg : this->_chatMessages) {
			if (pos.y <= 3) {
				msg.farUp = true;
				break;
			}

			int maxSize = msg.emotes.empty() ? 0 : EMOTE_SIZE;

			msg.farUp = false;
			msg.farDown = true;
			for (auto &text : msg.text) {
				if (remaining <= text.realSize.y - text.pos.y) {
					auto p = pos;

					if (!remaining)
						p += text.pos;
					else
						p.y += min(0, text.pos.y + static_cast<int>(remaining));
					this->_updateMessageSprite(p, remaining < -text.pos.y ? 0 : remaining + text.pos.y, text.realSize, text.sprite, alpha);
					msg.farDown = false;
				} else
					text.sprite.tint.a = 0;
				maxSize = max(maxSize, text.realSize.y);
			}
			for (auto &emote : msg.emotes) {
				emote.offset = pos;
				emote.cutRemain = remaining;
			}
			if (remaining <= EMOTE_SIZE && !msg.emotes.empty())
				msg.farDown = false;
			if (remaining > maxSize)
				remaining -= maxSize;
			else {
				pos.y -= maxSize - remaining;
				remaining = 0;
			}
		}
	}
}

void InLobbyMenu::renderChat()
{
	if (this->_chatSeat.tint.a) {
		this->_chatSeat.draw();
		for (auto &msg: this->_chatMessages) {
			if (msg.farUp)
				break;
			if (msg.farDown)
				continue;
			for (auto &text : msg.text) {
				//SokuLib::SpriteEx s;
				//auto handle = text.sprite.texture.releaseHandle();

				//text.sprite.texture.setHandle(handle, text.sprite.texture.getSize());
				//s.setTexture(
				//	handle,
				//	text.sprite.rect.left,
				//	text.sprite.rect.top,
				//	text.sprite.rect.width,
				//	text.sprite.rect.height,
				//	0, 0
				//);
				//s.loadTransform();
				//s.translate(text.sprite.getPosition().x, text.sprite.getPosition().y, 0);
				//s.saveTransform();
				//for (auto &c : s.vertices)
				//	c.color = text.sprite.tint;
				//reinterpret_cast<void(__fastcall*)(int, int, int)>(0x404b80)(0x896b4c, 0, 2);
				//s.render();
				//reinterpret_cast<void(__fastcall*)(int, int, int)>(0x404b80)(0x896b4c, 0, 1);
				text.sprite.draw();
			}
			for (auto &emote : msg.emotes) {
				auto &emoteObj = lobbyData->emotes[emote.id < lobbyData->emotes.size() ? emote.id : 0];
				auto pos = emote.pos + emote.offset;

				emoteObj.sprite.tint.a = this->_chatSeat.tint.a;
				emoteObj.sprite.rect.top = 0;
				emoteObj.sprite.rect.height = EMOTE_SIZE - emote.cutRemain;
				pos.y -= emoteObj.sprite.rect.height;
				if (pos.y < 3) {
					emoteObj.sprite.rect.height -= 3 - pos.y;
					emoteObj.sprite.rect.top = 3 - pos.y;
					pos.y = 3;
				}
				emoteObj.sprite.setSize({EMOTE_SIZE, static_cast<unsigned int>(emoteObj.sprite.rect.height)});
				emoteObj.sprite.setPosition(pos);
				emoteObj.sprite.draw();
			}
		}
	}
	if (this->_editingText) {
		for (auto &sprite : this->_textSprite) {
			//SokuLib::SpriteEx s;
			//auto handle = sprite.texture.releaseHandle();
	
			//sprite.texture.setHandle(handle, sprite.texture.getSize());
			//s.setTexture(
			//	handle,
			//	sprite.rect.left,
			//	sprite.rect.top,
			//	sprite.rect.width,
			//	sprite.rect.height,
			//	0, 0
			//);
			//s.loadTransform();
			//s.translate(sprite.getPosition().x, sprite.getPosition().y, 0);
			//s.saveTransform();
			//for (auto &c : s.vertices)
			//	c.color = sprite.tint;
			//reinterpret_cast<void(__fastcall*)(int, int, int)>(0x404b80)(0x896b4c, 0, 2);
			//s.render();
			//reinterpret_cast<void(__fastcall*)(int, int, int)>(0x404b80)(0x896b4c, 0, 1);
			sprite.draw();
		}
		this->_textCursor.draw();
	}
}

bool InLobbyMenu::isInputing()
{
	return this->_editingText;
}

void InLobbyMenu::_updateMessageSprite(SokuLib::Vector2i pos, unsigned int remaining, SokuLib::Vector2i realSize, SokuLib::DrawUtils::Sprite &sprite, unsigned char alpha)
{
	sprite.tint.a = alpha;
	sprite.rect.top = 0;
	sprite.rect.width = realSize.x;
	sprite.rect.height = realSize.y - remaining;
	pos.y -= sprite.rect.height;
	if (pos.y < 3) {
		sprite.rect.height -= 3 - pos.y;
		sprite.rect.top = 3 - pos.y;
		pos.y = 3;
	}
	sprite.setSize({
		static_cast<unsigned int>(sprite.rect.width),
		static_cast<unsigned int>(sprite.rect.height)
	});
	sprite.setPosition(pos);
}

void InLobbyMenu::_renderMachineOverlay()
{
	if (this->_currentMachine->id != UINT32_MAX)
		return;
	this->_hostlist->render();
	this->_currentMachine->skin.overlay.draw();
}

void InLobbyMenu::_startHosting()
{
	this->_parent->setupHost(hostPort, true);
	if (this->_hostThread.joinable())
		this->_hostThread.join();
	this->_hostThread = std::thread{[this]{
		puts("Putting hostlist");

		nlohmann::json data = {
			{"profile_name", SokuLib::profile1.name},
			{"message", "SokuLobbies " VERSION_STR ": Waiting in " + this->_roomName},
			{"port", hostPort}
		};

		try {
			puts(lobbyData->httpRequest("https://konni.delthas.fr/games", "PUT", data.dump()).c_str());
		} catch (std::exception &e) {
			MessageBoxA(SokuLib::window, e.what(), "Hostlist error", MB_ICONERROR);
		}
	}};
}

void InLobbyMenu::addString(wchar_t *str, size_t size)
{
	this->_textMutex.lock();

	std::wstring result{str, str + size};
	auto base = UTF16Decode(result);

	this->_buffer.insert(this->_buffer.begin() + this->_textCursorPosIndex, str, str + size);
	if (this->_buffer.size() > CHAT_CHARACTER_LIMIT)
		this->_buffer.resize(CHAT_CHARACTER_LIMIT);
	this->_updateTextCursor(this->_textCursorPosIndex + base.size());
	this->textChanged |= 1;
	SokuLib::playSEWaveBuffer(0x27);
	this->_textMutex.unlock();
}

void InLobbyMenu::_updateCompositionSprite()
{
	int ret;

	if (this->textChanged & 1) {
		if (!createTextTexture(ret, this->_buffer.data(), this->_chatFont, BOX_TEXTURE_SIZE, nullptr))
			puts("Error creating text texture");
		this->_textSprite[0].texture.setHandle(ret, BOX_TEXTURE_SIZE);
	}
	this->textChanged = false;
}

void InLobbyMenu::onCompositionResult()
{
	this->_returnPressed = false;
	this->_timers[VK_RETURN]++;
}

int InLobbyMenu::_getTextSize(unsigned int i)
{
	auto it = this->_textSize.find(i);

	if (it != this->_textSize.end())
		return it->second;

	int size = getTextSize(UTF16Encode(std::basic_string<unsigned>(&i, &i + 1)).c_str(), this->_chatFont, {32, 20}).x;

	this->_textSize[i] = size;
	return size;
}

InLobbyMenu::ArcadeMachine::ArcadeMachine(unsigned id, SokuLib::Vector2i pos, LobbyData::ArcadeAnimation *currentAnim, LobbyData::ArcadeSkin &skin):
	id(id),
	pos(pos),
	currentAnim(currentAnim),
	skin(skin)
{
}

InLobbyMenu::ArcadeMachine::ArcadeMachine(const InLobbyMenu::ArcadeMachine &):
	skin(*(LobbyData::ArcadeSkin*)nullptr)
{
	assert(false);
}
