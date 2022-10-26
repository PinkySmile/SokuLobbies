//
// Created by PinkySmile on 02/10/2022.
//

#include "InLobbyMenu.hpp"
#include <dinput.h>
#include <filesystem>

#define TEXTURE_MAX_SIZE 344
#define CURSOR_ENDX 637
#define CURSOR_STARTX 293
#define CURSOR_STARTY 184
#define CURSOR_STEP 5

extern wchar_t profileFolderPath[MAX_PATH];

InLobbyMenu *activeMenu = nullptr;
static WNDPROC Original_WndProc = nullptr;
static std::mutex ptrMutex;

static LRESULT __stdcall Hooked_WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ptrMutex.lock();
	if (uMsg == WM_KEYDOWN && activeMenu) {
		BYTE keyboardState[256];

		GetKeyboardState(keyboardState);
		if(!(MapVirtualKey(wParam, MAPVK_VK_TO_CHAR) >> (sizeof(UINT) * 8 - 1) & 1)) {
			unsigned short chr = 0;
			int nb = ToAscii((UINT)wParam, lParam, keyboardState, &chr, 0);

			if (nb == 1)
				activeMenu->onKeyPressed(chr);
		}
	} else if (uMsg == WM_KEYUP && activeMenu)
		activeMenu->onKeyReleased();
	ptrMutex.unlock();
	return CallWindowProc(Original_WndProc, hWnd, uMsg, wParam, lParam);
}

InLobbyMenu::InLobbyMenu(LobbyMenu *menu, SokuLib::MenuConnect *parent, Connection &connection) :
	connection(connection),
	parent(parent),
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
	desc.height = 16 + hasEnglishPatch * 2;
	desc.weight = FW_NORMAL;
	desc.italic = 0;
	desc.shadow = 1;
	desc.bufferSize = 1000000;
	desc.charSpaceX = 0;
	desc.charSpaceY = hasEnglishPatch * -2;
	desc.offsetX = 0;
	desc.offsetY = 0;
	desc.useOffset = 0;
	strcpy(desc.faceName, "MonoSpatialModSWR");
	desc.weight = FW_REGULAR;
	this->_defaultFont16.create();
	this->_defaultFont16.setIndirect(desc);
	desc.height = 10 + hasEnglishPatch * 2;
	this->_defaultFont10.create();
	this->_defaultFont10.setIndirect(desc);

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

	this->_loadingText.texture.createFromText("Joining Lobby...", this->_defaultFont16, {300, 74});
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

	this->_textSprite.rect.width = 350;
	this->_textSprite.rect.height = 18;
	this->_textSprite.setSize({350, 18});
	this->_textSprite.setPosition({CURSOR_STARTX - hasEnglishPatch * 2, CURSOR_STARTY});

	this->onConnectRequest = connection.onConnectRequest;
	this->onError = connection.onError;
	this->onImpMsg = connection.onImpMsg;
	this->onMsg = connection.onMsg;
	this->onHostRequest = connection.onHostRequest;
	this->onConnect = connection.onConnect;
	connection.onPlayerJoin = [this](const Player &r){
		SokuLib::Vector2i size;

		this->_extraPlayerData[r.id].name.texture.createFromText(r.name.c_str(), this->_defaultFont16, {200, 20}, &size);
		this->_extraPlayerData[r.id].name.setSize(size.to<unsigned>());
		this->_extraPlayerData[r.id].name.rect.width = size.x;
		this->_extraPlayerData[r.id].name.rect.height = size.y;
	};
	connection.onConnect = [this, &connection](const Lobbies::PacketOlleh &r){
		this->_background = r.bg;
		connection.getMe()->pos.y = this->_menu->backgrounds[r.bg].fg.getSize().y - this->_menu->backgrounds[r.bg].groundPos;

		SokuLib::Vector2i size;

		this->_extraPlayerData[r.id].name.texture.createFromText(std::string(r.realName, strnlen(r.realName, sizeof(r.realName))).c_str(), this->_defaultFont16, {200, 20}, &size);
		this->_extraPlayerData[r.id].name.setSize(size.to<unsigned>());
		this->_extraPlayerData[r.id].name.rect.width = size.x;
		this->_extraPlayerData[r.id].name.rect.height = size.y;
		this->_music = "data/bgm/" + std::string(r.music, strnlen(r.music, sizeof(r.music))) + ".ogg";
		SokuLib::playBGM(this->_music.c_str());
	};
	connection.onConnectRequest = [this](const std::string &ip, unsigned short port, bool spectate){
		SokuLib::playSEWaveBuffer(57);
		this->connection.getMe()->battleStatus = 2;
		this->parent->joinHost(ip.c_str(), port, spectate);
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
		//TODO: Allow to change port
		this->connection.getMe()->battleStatus = 2;
		this->parent->setupHost(10800, true);
		return 10800;
	};
	connection.connect();
	activeMenu = this;
	if (!Original_WndProc)
		Original_WndProc = (WNDPROC)SetWindowLongPtr(SokuLib::window, GWL_WNDPROC, (LONG_PTR)Hooked_WndProc);
}

InLobbyMenu::~InLobbyMenu()
{
	activeMenu = nullptr;
	this->_unhook();
	this->connection.disconnect();
	this->_menu->setActive();
	this->_defaultFont16.destruct();
	if (!this->_music.empty())
		SokuLib::playBGM("data/bgm/op2.ogg");
}

void InLobbyMenu::_()
{
	Lobbies::PacketArcadeLeave leave{0};

	this->connection.send(&leave, sizeof(leave));
	*(*(char **)0x89a390 + 20) = false;
	this->parent->choice = 0;
	this->parent->subchoice = 0;
	*(int *)0x882a94 = 0x16;
	SokuLib::playBGM(this->_music.c_str());
}

int InLobbyMenu::onProcess()
{
	if (!this->connection.isInit() && !this->_wasConnected) {
		this->_loadingGear.setRotation(this->_loadingGear.getRotation() + 0.1);
		return this->connection.isConnected();
	}
	if (!this->connection.isInit())
		return false;

	this->connection.meMutex.lock();
	auto inputs = SokuLib::inputMgrs.input;
	auto me = this->connection.getMe();

	this->updateChat();
	memset(&SokuLib::inputMgrs.input, 0, sizeof(SokuLib::inputMgrs.input));
	(this->parent->*SokuLib::VTable_ConnectMenu.onProcess)();
	SokuLib::inputMgrs.input = inputs;
	if (this->parent->choice > 0) {
		if (
			this->parent->subchoice == 5 || //Already Playing
			this->parent->subchoice == 10   //Connect Failed
		) {
			Lobbies::PacketArcadeLeave leave{0};

			this->connection.send(&leave, sizeof(leave));
			*(*(char **)0x89a390 + 20) = false;
			this->parent->choice = 0;
			this->parent->subchoice = 0;
		}
	}
	if (SokuLib::checkKeyOneshot(DIK_ESCAPE, 0, 0, 0)) {
		SokuLib::playSEWaveBuffer(0x29);
		if (!this->_editingText) {
			this->connection.meMutex.unlock();
			this->_unhook();
			this->connection.disconnect();
			return false;
		} else
			this->_editingText = false;
	}

	auto &bg = this->_menu->backgrounds[this->_background];

	if (me->battleStatus == 0 && !this->_editingText) {
		if (SokuLib::inputMgrs.input.b == 1) {
			SokuLib::playSEWaveBuffer(0x29);
			this->connection.meMutex.unlock();
			this->_unhook();
			this->connection.disconnect();
			return false;
		}
		if (SokuLib::inputMgrs.input.a == 1) {
			Lobbies::PacketGameRequest packet{0};

			me->battleStatus = 1;
			this->connection.send(&packet, sizeof(packet));
			SokuLib::playSEWaveBuffer(0x28);
		}

		bool dirChanged;

		if (SokuLib::inputMgrs.input.horizontalAxis) {
			auto newDir = me->dir;

			if (SokuLib::inputMgrs.input.horizontalAxis < 0 && me->pos.x < PLAYER_H_SPEED) {
				SokuLib::playSEWaveBuffer(0x29);
				this->connection.meMutex.unlock();
				this->connection.disconnect();
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

			this->connection.send(&l, sizeof(l));
		}
	} else {
		if (me->dir & 0b1111) {
			me->dir &= 0b10000;

			Lobbies::PacketMove m{0, me->dir};

			this->connection.send(&m, sizeof(m));
		}
		if (SokuLib::inputMgrs.input.b == 1 && !this->_editingText) {
			Lobbies::PacketArcadeLeave l{0};

			this->connection.send(&l, sizeof(l));
			me->battleStatus = 0;
			SokuLib::playSEWaveBuffer(0x29);
		}
	}

	this->connection.updatePlayers(this->_menu->avatars);
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
	this->connection.meMutex.unlock();
	return true;
}

int InLobbyMenu::onRender()
{
	if (!this->connection.isInit() && !this->_wasConnected) {
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

	auto &bg = this->_menu->backgrounds[this->_background];

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

	auto players = this->connection.getPlayers();

	for (auto &player : players) {
		auto &avatar = this->_menu->avatars[player.player.avatar];

		avatar.sprite.setPosition({
			static_cast<int>(player.pos.x) - static_cast<int>(avatar.sprite.getSize().x / 2) + this->_translate.x,
			480 - static_cast<int>(player.pos.y + avatar.sprite.getSize().y) + this->_translate.y
		});
		avatar.sprite.rect.top = avatar.sprite.rect.height * ((player.dir & 0b00011) != 0);
		avatar.sprite.rect.left = player.currentAnimation * avatar.sprite.rect.width;
		avatar.sprite.setMirroring((player.dir & 0b10000) == 0, false);
		avatar.sprite.draw();
	}
	for (auto &player : players) {
		auto &avatar = this->_menu->avatars[player.player.avatar];

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
	this->renderChat();
	return 0;
}

void InLobbyMenu::_unhook()
{
	this->connection.onConnectRequest = this->onConnectRequest;
	this->connection.onError = this->onError;
	this->connection.onImpMsg = this->onImpMsg;
	this->connection.onMsg = this->onMsg;
	this->connection.onHostRequest = this->onHostRequest;
	this->connection.onConnect = this->onConnect;
	this->connection.onPlayerJoin = this->onPlayerJoin;
}

void InLobbyMenu::_addMessageToList(unsigned int channel, unsigned player, const std::string &msg)
{
	this->_chatTimer = 900;
	this->_chatMessages.emplace_front();

	auto &m = this->_chatMessages.front();
	auto text = msg;

	if (player == 0) {
		char color[40];

		sprintf(color, "<color %06x>", channel);
		text = color + text + "</color>";
	}
	m.sprite.texture.createFromText(text.c_str(), this->_defaultFont10, {350, 300}, &m.realSize);
	m.sprite.rect.width = m.realSize.x;
	m.sprite.rect.height = m.realSize.y;
}

void InLobbyMenu::onKeyPressed(int chr)
{
	if (chr >= 0x7F || chr < 32 || !this->_editingText)
		return;
	this->_textMutex.lock();
	if (this->_lastPressed && this->_textTimer == 0) {
		this->_buffer.insert(this->_buffer.begin() + this->_textCursorPos, this->_lastPressed);
		this->_updateTextCursor(this->_textCursorPos + 1);
		this->_textChanged = true;
		SokuLib::playSEWaveBuffer(0x27);
	}
	this->_lastPressed = chr;
	this->_textTimer = 0;
	this->_textMutex.unlock();
}

void InLobbyMenu::onKeyReleased()
{
	this->_textMutex.lock();
	if (this->_lastPressed && this->_textTimer == 0) {
		this->_buffer.insert(this->_buffer.begin() + this->_textCursorPos, this->_lastPressed);
		this->_updateTextCursor(this->_textCursorPos + 1);
		SokuLib::playSEWaveBuffer(0x27);
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
	if (!this->_editingText) {
		if (this->_returnPressed && this->_timers[VK_RETURN] == 0) {
			this->_editingText = true;
			this->_initInputBox();
			SokuLib::playSEWaveBuffer(0x28);
		}
		return;
	}

	if (this->_returnPressed) {
		if (this->_timers[VK_RETURN] == 0) {
			this->_sendMessage(std::string{this->_buffer.begin(), this->_buffer.end() - 1});
			this->_editingText = false;
			this->_returnPressed = false;
		}
		return;
	}
	this->_textMutex.lock();
	if (this->_timers[VK_HOME] == 1) {
		SokuLib::playSEWaveBuffer(0x27);
		this->_updateTextCursor(0);
	}
	if (this->_timers[VK_END] == 1) {
		SokuLib::playSEWaveBuffer(0x27);
		this->_updateTextCursor(this->_buffer.size() - 1);
	}
	if (this->_timers[VK_RETURN] == 1) {
		this->_returnPressed = true;
		return;
	}
	if (this->_timers[VK_BACK] == 1 || (this->_timers[VK_BACK] > 36 && this->_timers[VK_BACK] % 6 == 0)) {
		if (this->_textCursorPos != 0) {
			this->_buffer.erase(this->_buffer.begin() + this->_textCursorPos - 1);
			this->_updateTextCursor(this->_textCursorPos - 1);
			this->_textChanged = true;
			SokuLib::playSEWaveBuffer(0x27);
		}
	}
	if (this->_timers[VK_DELETE] == 1 || (this->_timers[VK_DELETE] > 36 && this->_timers[VK_DELETE] % 6 == 0)) {
		if (this->_textCursorPos < this->_buffer.size() - 1) {
			this->_buffer.erase(this->_buffer.begin() + this->_textCursorPos);
			SokuLib::playSEWaveBuffer(0x27);
			this->_textChanged = true;
			this->_textSprite.texture.createFromText(this->_sanitizeInput().c_str(), this->_defaultFont10, {8 * this->_buffer.size(), 1800});
		}
	}
	if (this->_timers[VK_LEFT] == 1 || (this->_timers[VK_LEFT] > 36 && this->_timers[VK_LEFT] % 3 == 0)) {
		if (this->_textCursorPos != 0) {
			this->_updateTextCursor(this->_textCursorPos - 1);
			SokuLib::playSEWaveBuffer(0x27);
		}
	}
	if (this->_timers[VK_RIGHT] == 1 || (this->_timers[VK_RIGHT] > 36 && this->_timers[VK_RIGHT] % 3 == 0)) {
		if (this->_textCursorPos != this->_buffer.size() - 1) {
			this->_updateTextCursor(this->_textCursorPos + 1);
			SokuLib::playSEWaveBuffer(0x27);
		}
	}
	if (this->_lastPressed) {
		this->_textTimer++;
		if (this->_textTimer == 1 || (this->_textTimer > 36 && this->_textTimer % 6 == 0)) {
			this->_buffer.insert(this->_buffer.begin() + this->_textCursorPos, this->_lastPressed);
			this->_updateTextCursor(this->_textCursorPos + 1);
			SokuLib::playSEWaveBuffer(0x27);
			this->_textChanged = true;
		}
	}
	if (this->_textChanged)
		this->_textSprite.texture.createFromText(this->_sanitizeInput().c_str(), this->_defaultFont10, {max(TEXTURE_MAX_SIZE, 8 * this->_buffer.size()), 18});
	this->_textChanged = false;
	this->_textMutex.unlock();
}

void InLobbyMenu::_initInputBox()
{
	SokuLib::playSEWaveBuffer(0x28);
	memset(this->_timers, 0, sizeof(this->_timers));
	this->_lastPressed = 0;
	this->_textTimer = 0;
	this->_buffer.clear();
	this->_buffer.push_back(0);

	this->_textSprite.texture.createFromText(this->_sanitizeInput().c_str(), this->_defaultFont10, {max(TEXTURE_MAX_SIZE, 8 * this->_buffer.size()), 20});
	this->_textSprite.rect.left = 0;

	this->_textCursorPos = 0;
	this->_textCursor.setPosition({CURSOR_STARTX, CURSOR_STARTY});
	this->_updateTextCursor(0);

	this->_editingText = true;
	this->_returnPressed = false;
}

void InLobbyMenu::_updateTextCursor(int pos)
{
	int diff = pos - this->_textCursorPos;
	int newX = this->_textCursor.getPosition().x + diff * CURSOR_STEP;

	if (newX > CURSOR_ENDX) {
		this->_textSprite.rect.left += newX - CURSOR_ENDX;
		this->_textCursor.setPosition({CURSOR_ENDX, CURSOR_STARTY});
	} else if (newX < CURSOR_STARTX) {
		this->_textSprite.rect.left += newX - CURSOR_STARTX;
		this->_textCursor.setPosition({CURSOR_STARTX, CURSOR_STARTY});
	} else
		this->_textCursor.setPosition({newX, CURSOR_STARTY});
	this->_textCursorPos = pos;
}

std::string InLobbyMenu::_sanitizeInput()
{
	std::string result{this->_buffer.begin(), this->_buffer.end() - 1};

	for (size_t pos = result.find('<'); pos != std::string::npos; pos = result.find('<'))
		result[pos] = '{';
	for (size_t pos = result.find('>'); pos != std::string::npos; pos = result.find('>'))
		result[pos] = '}';
	return result;
}

void InLobbyMenu::_sendMessage(const std::string &msg)
{
	Lobbies::PacketMessage msgPacket{0, 0, msg};

	if (this->_buffer.size() != 1) {
		this->connection.send(&msgPacket, sizeof(msgPacket));
		SokuLib::playSEWaveBuffer(0x28);
	} else
		SokuLib::playSEWaveBuffer(0x29);
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
			if (remaining > msg.realSize.y) {
				msg.sprite.rect.height = 0;
				remaining -= msg.realSize.y;
				continue;
			}
			if (pos.y <= 3) {
				msg.sprite.rect.width = 0;
				break;
			}
			msg.sprite.tint.a = alpha;
			msg.sprite.rect.top = 0;
			msg.sprite.rect.width = msg.realSize.x;
			msg.sprite.rect.height = msg.realSize.y - remaining;
			remaining = 0;
			pos.y -= msg.sprite.rect.height;
			if (pos.y < 3) {
				msg.sprite.rect.height -= 3 - pos.y;
				msg.sprite.rect.top = 3 - pos.y;
				pos.y = 3;
			}
			msg.sprite.setSize({
				static_cast<unsigned int>(msg.sprite.rect.width),
				static_cast<unsigned int>(msg.sprite.rect.height)
			});
			msg.sprite.setPosition(pos);
		}
	}
}

void InLobbyMenu::renderChat()
{
	if (this->_chatSeat.tint.a) {
		this->_chatSeat.draw();
		for (auto &msg: this->_chatMessages) {
			if (!msg.sprite.rect.height)
				continue;
			if (!msg.sprite.rect.width)
				break;
			msg.sprite.draw();
		}
	}
	if (this->_editingText) {
		this->_textSprite.draw();
		this->_textCursor.draw();
	}
}

bool InLobbyMenu::isInputing()
{
	return this->_editingText;
}
