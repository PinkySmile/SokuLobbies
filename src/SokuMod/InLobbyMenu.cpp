//
// Created by PinkySmile on 02/10/2022.
//

#include "InLobbyMenu.hpp"
#include <dinput.h>
#include <filesystem>

extern wchar_t profileFolderPath[MAX_PATH];

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

	this->_inBattle.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/lobby/inarcade.png").string().c_str());
	this->_inBattle.setSize({
		this->_inBattle.texture.getSize().x,
		this->_inBattle.texture.getSize().y
	});
	this->_inBattle.rect.width = this->_inBattle.texture.getSize().x;
	this->_inBattle.rect.height = this->_inBattle.texture.getSize().y;
	this->_inBattle.setPosition({174, 218});

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
	connection.onMsg = [this](int32_t channel, const std::string &msg){
		SokuLib::playSEWaveBuffer(49);
		puts(msg.c_str());
	};
	connection.onHostRequest = [this]{
		SokuLib::playSEWaveBuffer(57);
		//TODO: Allow to change port
		this->connection.getMe()->battleStatus = 2;
		this->parent->setupHost(10800, true);
		return 10800;
	};
	connection.connect();
}

InLobbyMenu::~InLobbyMenu()
{
	this->connection.disconnect();
	this->connection.onConnectRequest = this->onConnectRequest;
	this->connection.onError = this->onError;
	this->connection.onImpMsg = this->onImpMsg;
	this->connection.onMsg = this->onMsg;
	this->connection.onHostRequest = this->onHostRequest;
	this->connection.onConnect = this->onConnect;
	this->connection.onPlayerJoin = this->onPlayerJoin;
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
		this->connection.meMutex.unlock();
		this->connection.disconnect();
		return false;
	}

	auto &bg = this->_menu->backgrounds[this->_background];

	if (me->battleStatus == 0) {
		if (SokuLib::inputMgrs.input.b == 1) {
			SokuLib::playSEWaveBuffer(0x29);
			this->connection.meMutex.unlock();
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
		if (SokuLib::inputMgrs.input.b == 1) {
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
	return 0;
}
