//
// Created by PinkySmile on 02/10/2022.
//

#include <dinput.h>
#include <filesystem>
#include <random>
#include "InLobbyMenu.hpp"
#include "LobbyData.hpp"
#include "data.hpp"

#define TEXTURE_MAX_SIZE 344
#define CURSOR_ENDX 637
#define CURSOR_STARTX 293
#define CURSOR_STARTY 184
#define CURSOR_STEP 7
#define MAX_LINE_SIZE 342
#define SCROLL_AMOUNT 20
#define CHAT_FONT_HEIGHT 14

InLobbyMenu *activeMenu = nullptr;
static WNDPROC Original_WndProc = nullptr;
static std::mutex ptrMutex;
static std::mt19937 random;

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

	this->_textSprite.rect.width = 350;
	this->_textSprite.rect.height = 18;
	this->_textSprite.setSize({350, 18});
	this->_textSprite.setPosition({CURSOR_STARTX - (*(int *)0x411c64 == 1) * 2, CURSOR_STARTY});

	auto &bg = lobbyData->backgrounds.front();

	this->onConnectRequest = connection.onConnectRequest;
	this->onError = connection.onError;
	this->onImpMsg = connection.onImpMsg;
	this->onMsg = connection.onMsg;
	this->onHostRequest = connection.onHostRequest;
	this->onConnect = connection.onConnect;
	this->_machines.reserve((bg.fg.getSize().x / 200 - 1) * (bg.platformCount + 1));

	int id = 0;

	for (int j = 0; j < bg.platformCount + 1; j++)
		for (int i = 200; i < bg.fg.getSize().x; i += 200)
			this->_machines.emplace_back(
				id++,
				SokuLib::Vector2i{i, static_cast<int>(bg.fg.getSize().y - bg.groundPos + j * bg.platformInterval)},
				&lobbyData->arcades.intro,
				lobbyData->arcades.skins.front()
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

		this->_extraPlayerData[r.id].name.texture.createFromText(std::string(r.realName, strnlen(r.realName, sizeof(r.realName))).c_str(), lobbyData->getFont(16), {200, 20}, &size);
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
	connection.onArcadeEngage = [this](const Player &, uint32_t id){
		if (id >= this->_machines.size())
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
		if (id >= this->_machines.size())
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

	auto &bg = lobbyData->backgrounds[this->_background];

	if (me->battleStatus == 0 && !this->_editingText) {
		if (SokuLib::inputMgrs.input.b == 1) {
			SokuLib::playSEWaveBuffer(0x29);
			this->connection.meMutex.unlock();
			this->_unhook();
			this->connection.disconnect();
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

				Lobbies::PacketGameRequest packet{machine.id};

				me->battleStatus = 1;
				this->connection.send(&packet, sizeof(packet));
				SokuLib::playSEWaveBuffer(0x28);
				break;
			}
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

	this->connection.updatePlayers(lobbyData->avatars);
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

	auto players = this->connection.getPlayers();
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
			avatar.sprite.rect.top = avatar.sprite.rect.height * ((player.dir & 0b00011) != 0);
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
	this->connection.onArcadeEngage = this->onArcadeEngage;
	this->connection.onArcadeLeave = this->onArcadeLeave;
}

void InLobbyMenu::_addMessageToList(unsigned int channel, unsigned player, const std::string &msg)
{
	this->_chatTimer = 900;
	this->_chatMessages.emplace_front();

	auto *m = &this->_chatMessages.front();
	std::string line;
	std::string word;
	unsigned startPos = 0;
	unsigned pos = 0;
	unsigned wordPos = 0;
	bool skip = false;
	unsigned short emoteId;
	unsigned char emoteCtr = 0;
	auto pushText = [&]{
		if (line.empty())
			return;

		m->text.emplace_back();

		auto &txt = m->text.back();
		auto text = line;

		if (player == 0) {
			char color[40];

			sprintf(color, "<color %06x>", channel);
			text = color + text + "</color>";
		}
		txt.sprite.texture.createFromText(text.c_str(), lobbyData->getFont(CHAT_FONT_HEIGHT), {350, 300}, &txt.realSize);
		txt.sprite.rect.width = txt.realSize.x;
		txt.sprite.rect.height = txt.realSize.y;
		txt.pos.x = startPos;
		if (!m->emotes.empty())
			txt.pos.y = (txt.realSize.y - EMOTE_SIZE) / 2;
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
	for (auto c : msg) {
		unsigned char arraySection = c >> 4U;

		if (emoteCtr) {
			emoteId |= (c & 0x7F) << ((2 - emoteCtr) * 7);
			emoteCtr--;
			if (!emoteCtr) {
				if (pos + EMOTE_SIZE > MAX_LINE_SIZE)
					nextLine();
				m->emotes.emplace_back();
				m->emotes.back().id = emoteId;
				m->emotes.back().pos.x = startPos;
				pos += EMOTE_SIZE;
				startPos = pos;
				for (auto &g : m->text)
					g.pos.y = (g.realSize.y - EMOTE_SIZE) / 2;
			}
		} else if (skip) {
			skip = false;
			word += c;
			wordPos += CURSOR_STEP;
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
		} else if (arraySection == 0x8 || arraySection == 0x9 || arraySection == 0xE) {
			skip = true;
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
			pos += CURSOR_STEP;
		} else {
			word += c;
			wordPos += CURSOR_STEP;
		}
		if (pos + wordPos > MAX_LINE_SIZE) {
			if (pos == 0) {
				line = word.substr(0, word.size() - 1);
				word.erase(word.begin(), word.end() - 1);
				wordPos = CURSOR_STEP;
			}
			nextLine();
			startPos = 0;
		}
	}
	line += word;
	pushText();
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
	if (this->_timers[VK_PRIOR] == 1 || (this->_timers[VK_PRIOR] > 36 && this->_timers[VK_PRIOR] % 6 == 0)) {
		SokuLib::playSEWaveBuffer(0x27);
		this->_chatOffset += SCROLL_AMOUNT;
		this->_chatTimer = max(this->_chatTimer, 180);
		return;
	}
	if (this->_timers[VK_NEXT] == 1 || (this->_timers[VK_NEXT] > 36 && this->_timers[VK_NEXT] % 6 == 0)) {
		if (this->_chatOffset < SCROLL_AMOUNT)
			this->_chatOffset = 0;
		else
			this->_chatOffset -= SCROLL_AMOUNT;
		this->_chatTimer = max(this->_chatTimer, 180);
		SokuLib::playSEWaveBuffer(0x27);
		return;
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
			if (this->_buffer.size() != 1) {
				this->_sendMessage(std::string{this->_buffer.begin(), this->_buffer.end() - 1});
				SokuLib::playSEWaveBuffer(0x28);
			} else
				SokuLib::playSEWaveBuffer(0x29);
			this->_editingText = false;
			this->_returnPressed = false;
			this->_chatOffset = 0;
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
			this->_textSprite.texture.createFromText(this->_sanitizeInput().c_str(), lobbyData->getFont(CHAT_FONT_HEIGHT), {8 * this->_buffer.size(), 1800});
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
		this->_textSprite.texture.createFromText(this->_sanitizeInput().c_str(), lobbyData->getFont(CHAT_FONT_HEIGHT), {max(TEXTURE_MAX_SIZE, 8 * this->_buffer.size()), 18});
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

	this->_textSprite.texture.createFromText(this->_sanitizeInput().c_str(), lobbyData->getFont(CHAT_FONT_HEIGHT), {max(TEXTURE_MAX_SIZE, 8 * this->_buffer.size()), 20});
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
	std::string realMsg;
	std::string currentEmote;
	bool colon = false;
	bool skip = false;

	realMsg.reserve(msg.size());
	for (auto c : msg) {
		unsigned char arraySection = c >> 4U;

		if (!skip && c == ':') {
			colon = !colon;
			if (colon)
				continue;

			auto it = lobbyData->emotesByName.find(currentEmote);

			if (it == lobbyData->emotesByName.end() || lobbyData->isLocked(*it->second)) {
				realMsg += ':';
				realMsg += currentEmote;
				realMsg += ':';
			} else {
				auto nb = it->second->id;

				realMsg += '\x01';
				for (int i = 0; i < 2; i++) {
					realMsg += static_cast<char>(nb & 0x7F | 0x80);
					nb >>= 7;
				}
			}
			currentEmote.clear();
		} else
			(colon ? currentEmote : realMsg) += c;
		skip = !skip && (arraySection == 0x8 || arraySection == 0x9 || arraySection == 0xE);
	}
	if (colon) {
		realMsg += ':';
		realMsg += currentEmote;
	}

	size_t pos = realMsg.find("bgs");

	if (
		pos != std::string::npos &&
		(pos == 0 || !isalpha(realMsg[pos-1])) &&
		(pos + 3 == realMsg.size() - 1 || !isalpha(realMsg[pos + 3]))
	) {
		realMsg.erase(realMsg.begin() + pos, realMsg.begin() + pos + 3);
		realMsg.insert(pos, "GGs, thanks for the games. It was very nice playing with you, let's play again later");
	}

	Lobbies::PacketMessage msgPacket{0, 0, realMsg};

	this->connection.send(&msgPacket, sizeof(msgPacket));
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
			for (auto &text : msg.text)
				text.sprite.draw();
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
		this->_textSprite.draw();
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
