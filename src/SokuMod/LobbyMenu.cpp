//
// Created by PinkySmile on 02/10/2022.
//

#include <iostream>
#include <fstream>
#include <filesystem>
#include <../directx/dinput.h>
#include "nlohmann/json.hpp"
#include "LobbyMenu.hpp"
#include "LobbyData.hpp"
#include "InLobbyMenu.hpp"
#include "Exceptions.hpp"
#include "InputBox.hpp"
#include "GuardedMutex.hpp"
#include "data.hpp"
#include "StatsMenu.hpp"

#define CRenderer_Unknown1 ((void (__thiscall *)(int, int))0x404AF0)

enum MenuItems {
	MENUITEM_CREATE_LOBBY,
	MENUITEM_JOIN_LOBBY,
	MENUITEM_CUSTOMIZE_LOBBY,
	MENUITEM_CUSTOMIZE_AVATAR,
	MENUITEM_ACHIVEMENTS,
	MENUITEM_OPTIONS,
	MENUITEM_STATISTICS,
	MENUITEM_EXIT,
};

void displaySokuCursor(SokuLib::Vector2i pos, SokuLib::Vector2u size)
{
	SokuLib::Sprite (&CursorSprites)[3] = *(SokuLib::Sprite (*)[3])0x89A6C0;

	//0x443a50 -> Vanilla display cursor
	CursorSprites[0].scale.x = size.x * 0.00195313f;
	CursorSprites[0].scale.y = size.y / 16.f;
	pos.x -= 7;
	CursorSprites[0].render(pos.x, pos.y);
	CRenderer_Unknown1(0x896B4C, 2);
	CursorSprites[1].rotation = *(float *)0x89A450 * 4.00000000f;
	CursorSprites[1].render(pos.x, pos.y + 8.00000000f);
	CursorSprites[2].rotation = -*(float *)0x89A450 * 4.00000000f;
	CursorSprites[2].render(pos.x - 14.00000000f, pos.y - 1.00000000f);
	CRenderer_Unknown1(0x896B4C, 1);
}

LobbyMenu::LobbyMenu(SokuLib::MenuConnect *parent) :
	_parent(parent)
{
	if (!lobbyData)
		lobbyData = std::make_unique<LobbyData>();

	std::filesystem::path folder = profileFolderPath;

	inputBoxLoadAssets();
	this->title.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/menu/title.png").string().c_str());
	this->title.setSize(this->title.texture.getSize());
	this->title.setPosition({23, 6});
	this->title.rect.width = this->title.getSize().x;
	this->title.rect.height = this->title.getSize().y;

	this->ui.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/menu/lobbylist.png").string().c_str());
	this->ui.setSize(this->ui.texture.getSize());
	this->ui.rect.width = this->ui.getSize().x;
	this->ui.rect.height = this->ui.getSize().y;

	this->_customizeSeat.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/menu/customSeat.png").string().c_str());
	this->_customizeSeat.setSize(this->_customizeSeat.texture.getSize());
	this->_customizeSeat.rect.width = this->_customizeSeat.getSize().x;
	this->_customizeSeat.rect.height = this->_customizeSeat.getSize().y;

	std::ifstream s{folder / "settings.dat", std::ifstream::binary};

	if (s) {
		char version;

		s.read(&version, 1);
		s.read((char *)&this->_loadedSettings.settings, sizeof(this->_loadedSettings.settings));
		s.read((char *)&this->_loadedSettings.player, sizeof(this->_loadedSettings.player));
	} else {
		this->_loadedSettings.settings.hostPref = Lobbies::HOSTPREF_NO_PREF;
		this->_loadedSettings.player.title = 0;
		this->_loadedSettings.player.avatar = 0;
		this->_loadedSettings.player.head = 0;
		this->_loadedSettings.player.body = 0;
		this->_loadedSettings.player.back = 0;
		this->_loadedSettings.player.env = 0;
		this->_loadedSettings.player.feet = 0;
	}
	this->_loadedSettings.name = SokuLib::profile1.name.operator std::string();
	this->_loadedSettings.pos.x = 20;

	SokuLib::Vector2i size;

	this->_playerName.texture.createFromText(SokuLib::profile1.name, lobbyData->getFont(16), {200, 20}, &size);
	this->_playerName.setSize(size.to<unsigned>());
	this->_playerName.rect.width = size.x;
	this->_playerName.rect.height = size.y;

	this->_customizeTexts[0].texture.createFromText("Avatar", lobbyData->getFont(20), {600, 74});
	this->_customizeTexts[0].setSize({
		this->_customizeTexts[0].texture.getSize().x,
		this->_customizeTexts[0].texture.getSize().y
	});
	this->_customizeTexts[0].rect.width = this->_customizeTexts[0].texture.getSize().x;
	this->_customizeTexts[0].rect.height = this->_customizeTexts[0].texture.getSize().y;
	this->_customizeTexts[0].setPosition({120, 98});

	this->_customizeTexts[1].texture.createFromText("Accessory", lobbyData->getFont(20), {600, 74});
	this->_customizeTexts[1].setSize({
		this->_customizeTexts[1].texture.getSize().x,
		this->_customizeTexts[1].texture.getSize().y
	});
	this->_customizeTexts[1].rect.width = this->_customizeTexts[1].texture.getSize().x;
	this->_customizeTexts[1].rect.height = this->_customizeTexts[1].texture.getSize().y;
	this->_customizeTexts[1].setPosition({280, 98});

	this->_customizeTexts[2].texture.createFromText("Title", lobbyData->getFont(20), {600, 74});
	this->_customizeTexts[2].setSize({
		this->_customizeTexts[2].texture.getSize().x,
		this->_customizeTexts[2].texture.getSize().y
	});
	this->_customizeTexts[2].rect.width = this->_customizeTexts[2].texture.getSize().x;
	this->_customizeTexts[2].rect.height = this->_customizeTexts[2].texture.getSize().y;
	this->_customizeTexts[2].setPosition({475, 98});

	this->_loadingText.texture.createFromText("Connecting to server...", lobbyData->getFont(16), {600, 74});
	this->_loadingText.setSize({
		this->_loadingText.texture.getSize().x,
		this->_loadingText.texture.getSize().y
	});
	this->_loadingText.rect.width = this->_loadingText.texture.getSize().x;
	this->_loadingText.rect.height = this->_loadingText.texture.getSize().y;
	this->_loadingText.setPosition({164, 218});

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

	this->_netThread = std::thread(&LobbyMenu::_netLoop, this);
	this->_connectThread = std::thread(&LobbyMenu::_connectLoop, this);
	this->_masterThread = std::thread(&LobbyMenu::_masterServerLoop, this);
}

LobbyMenu::~LobbyMenu()
{
	auto path = std::filesystem::path(profileFolderPath) / "settings.dat";
	std::ofstream s{path, std::ofstream::binary};
	char version = 0;

	if (s) {
		s.write(&version, 1);
		s.write((char *)&this->_loadedSettings.settings, sizeof(this->_loadedSettings.settings));
		s.write((char *)&this->_loadedSettings.player, sizeof(this->_loadedSettings.player));
	} else {
		auto err = strerror(errno);
		auto str = new wchar_t[strlen(err)];

		for (int i = 0; err[i]; i++)
			str[i] = err[i];
		MessageBoxW(SokuLib::window, (L"Cannot open " + path.wstring() + L" for writing: " + str).c_str(), L"Saving error", MB_ICONERROR);
		delete[] str;
	}
	inputBoxUnloadAssets();
	this->_open = false;
	if (this->_netThread.joinable())
		this->_netThread.join();
	this->_mainServer.disconnect();
	if (this->_masterThread.joinable())
		this->_masterThread.join();
	if (this->_connectThread.joinable())
		this->_connectThread.join();
}

void LobbyMenu::_netLoop()
{
	while (this->_open) {
		Lobbies::PacketPing ping;

		this->_connectionsMutex.lock();
		for (auto &c : this->_connections)
			if (c->c && !c->c->isInit() && c->c->isConnected())
				c->c->send(&ping, sizeof(ping));
		this->_connectionsMutex.unlock();
		for (int i = 0; i < 20 && this->_open; i++)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void LobbyMenu::_()
{
	puts("_ !");
	*(int *)0x882a94 = 0x16;
}

bool (LobbyMenu::* const LobbyMenu::_updateCallbacks[])() = {
	&LobbyMenu::_normalMenuUpdate,
	&LobbyMenu::_joinLobbyUpdate,
	&LobbyMenu::_customizeAvatarUpdate
};
void (LobbyMenu::* const LobbyMenu::_renderCallbacks[])() = {
	&LobbyMenu::_dummyRender,
	&LobbyMenu::_dummyRender,
	&LobbyMenu::_customizeAvatarRender
};

int LobbyMenu::onProcess()
{
	inputBoxUpdate();
	if (inputBoxShown)
		return true;
	if (SokuLib::checkKeyOneshot(DIK_ESCAPE, 0, 0, 0)) {
		SokuLib::playSEWaveBuffer(0x29);
		this->_open = false;
		return false;
	}
	if (this->_mainServer.isDisconnected()) {
		this->_loadingGear.setRotation(this->_loadingGear.getRotation() + 0.1);
		return true;
	}
	return (this->*_updateCallbacks[this->_menuState])();
}

bool LobbyMenu::_normalMenuUpdate()
{
	if (SokuLib::inputMgrs.input.b == 1) {
		SokuLib::playSEWaveBuffer(0x29);
		this->_open = false;
		return false;
	}
	if (std::abs(SokuLib::inputMgrs.input.verticalAxis) == 1 || (std::abs(SokuLib::inputMgrs.input.verticalAxis) > 36 && std::abs(SokuLib::inputMgrs.input.verticalAxis) % 6 == 0)) {
		if (SokuLib::inputMgrs.input.verticalAxis < 0) {
			this->_menuCursor += 8;
			this->_menuCursor--;
		} else
			this->_menuCursor++;
		this->_menuCursor %= 8;
		SokuLib::playSEWaveBuffer(0x27);
	}
	if (SokuLib::inputMgrs.input.changeCard == 1) {
		setInputBoxCallbacks([this](const std::string &value){
			GuardedMutex m{this->_connectionsMutex};

			try {
				auto colon = value.find_last_of(':');
				auto ip = value.substr(0, colon);
				unsigned short port = colon == std::string::npos ? 10800 : std::stoul(value.substr(colon + 1));

				m.lock();
				this->_connections.emplace_back(new Entry{std::shared_ptr<Connection>(), ip, port});
				SokuLib::playSEWaveBuffer(0x28);
			} catch (std::exception &e) {
				puts(e.what());
				SokuLib::playSEWaveBuffer(0x29);
			}
		});
		openInputDialog("Enter lobby ip", "localhost:10800");
	}
	if (SokuLib::inputMgrs.input.a == 1) {
		SokuLib::playSEWaveBuffer(0x28);
		switch (this->_menuCursor) {
		case MENUITEM_JOIN_LOBBY:
			this->_menuState = 1;
			break;
		case MENUITEM_CUSTOMIZE_AVATAR:
			this->_customCursor = 0;
			this->_refreshAvatarCustomText();
			this->_showcases.resize(0);
			this->_showcases.resize(lobbyData->avatars.size());
			this->_customizeTexts[0].tint = SokuLib::Color::White;
			this->_customizeTexts[1].tint = SokuLib::Color{0x80, 0x80, 0x80, 0xFF};
			this->_customizeTexts[2].tint = SokuLib::Color{0x80, 0x80, 0x80, 0xFF};
			this->_menuState = 2;
			break;
		case MENUITEM_CREATE_LOBBY:
		case MENUITEM_CUSTOMIZE_LOBBY:
		case MENUITEM_ACHIVEMENTS:
		case MENUITEM_OPTIONS:
			MessageBox(SokuLib::window, "Not implemented", "Not implemented", MB_ICONINFORMATION);
			break;
		case MENUITEM_STATISTICS:
			SokuLib::activateMenu(new StatsMenu());
			break;
		case MENUITEM_EXIT:
			return false;
		}
	}
	return true;
}

bool LobbyMenu::_joinLobbyUpdate()
{
	if (SokuLib::inputMgrs.input.b == 1) {
		SokuLib::playSEWaveBuffer(0x29);
		this->_menuState = 0;
		return true;
	}
	this->_connectionsMutex.lock();
	if (std::abs(SokuLib::inputMgrs.input.verticalAxis) == 1 || (std::abs(SokuLib::inputMgrs.input.verticalAxis) > 36 && std::abs(SokuLib::inputMgrs.input.verticalAxis) % 6 == 0)) {
		if (SokuLib::inputMgrs.input.verticalAxis < 0) {
			this->_lobbyCtr += this->_connections.size();
			this->_lobbyCtr--;
		} else
			this->_lobbyCtr++;
		if (!this->_connections.empty())
			this->_lobbyCtr %= this->_connections.size();
		SokuLib::playSEWaveBuffer(0x27);
	}
	if (SokuLib::inputMgrs.input.a == 1) {
		if (this->_lobbyCtr < this->_connections.size() && this->_connections[this->_lobbyCtr]->c && this->_connections[this->_lobbyCtr]->c->isConnected()) {
			SokuLib::activateMenu(new InLobbyMenu(this, this->_parent, *this->_connections[this->_lobbyCtr]->c));
			this->_active = false;
			SokuLib::playSEWaveBuffer(0x28);
		} else
			SokuLib::playSEWaveBuffer(0x29);
	}
	this->_connectionsMutex.unlock();
	return true;
}

bool LobbyMenu::_customizeAvatarUpdate()
{
	if (SokuLib::inputMgrs.input.a == 1 && this->_customCursor >= 0) {
		SokuLib::playSEWaveBuffer(0x28);
		this->_loadedSettings.player.avatar = this->_customCursor;
		return true;
	}
	if (SokuLib::inputMgrs.input.b == 1) {
		SokuLib::playSEWaveBuffer(0x29);
		this->_menuState = 0;
		return true;
	}
	if (std::abs(SokuLib::inputMgrs.input.horizontalAxis) == 1 || (std::abs(SokuLib::inputMgrs.input.horizontalAxis) > 36 && std::abs(SokuLib::inputMgrs.input.horizontalAxis) % 6 == 0)) {
		SokuLib::playSEWaveBuffer(0x27);
		if (this->_customCursor == 0 && SokuLib::inputMgrs.input.horizontalAxis < 0)
			this->_customCursor += lobbyData->avatars.size() - 1 - (lobbyData->avatars.size() - 1) % 4;
		else
			this->_customCursor = (this->_customCursor + (int)std::copysign(1, SokuLib::inputMgrs.input.horizontalAxis)) % lobbyData->avatars.size();
		this->_refreshAvatarCustomText();
	}
	if (std::abs(SokuLib::inputMgrs.input.verticalAxis) == 1 || (std::abs(SokuLib::inputMgrs.input.verticalAxis) > 36 && std::abs(SokuLib::inputMgrs.input.verticalAxis) % 6 == 0)) {
		SokuLib::playSEWaveBuffer(0x27);
		if (this->_customCursor < 4 && SokuLib::inputMgrs.input.verticalAxis < 0)
			this->_customCursor = lobbyData->avatars.size() - 1 - this->_customCursor;
		else if (this->_customCursor + 4 >= lobbyData->avatars.size() && SokuLib::inputMgrs.input.verticalAxis > 0)
			this->_customCursor = this->_customCursor % 4;
		else
			this->_customCursor = this->_customCursor + (int)std::copysign(4, SokuLib::inputMgrs.input.verticalAxis);
		this->_refreshAvatarCustomText();
	}
	for (int i = 0; i < lobbyData->avatars.size(); i++) {
		auto &avatar = lobbyData->avatars[i];
		auto &showcase = this->_showcases[i];

		showcase.animCtr++;
		if (showcase.animCtr < avatar.animationsStep)
			continue;
		showcase.animCtr = 0;
		showcase.anim++;
		if (showcase.anim < avatar.nbAnimations)
			continue;
		showcase.anim = 0;
		showcase.action++;
		if (showcase.action < 8)
			continue;
		showcase.action = 0;
		showcase.side = !showcase.side;
	}
	return true;
}

int LobbyMenu::onRender()
{
	this->title.draw();
	if (this->_mainServer.isDisconnected()) {
		this->_messageBox.draw();
		this->_loadingText.draw();
		if (this->_lastError.empty()) {
			this->_loadingGear.setRotation(-this->_loadingGear.getRotation());
			this->_loadingGear.setPosition({412, 227});
			this->_loadingGear.draw();
			this->_loadingGear.setRotation(-this->_loadingGear.getRotation());
			this->_loadingGear.setPosition({412 + 23, 227 - 18});
			this->_loadingGear.draw();
		}
		return 0;
	}
	if (this->_menuCursor == 7)
		displaySokuCursor({50, 366}, {180, 16});
	else
		displaySokuCursor({50, static_cast<int>(126 + this->_menuCursor * 24)}, {180, 16});
	this->ui.draw();
	this->_connectionsMutex.lock();
	if (this->_menuState == 1)
		displaySokuCursor({312, static_cast<int>(120 + this->_lobbyCtr * 16)}, {220, 16});
	for (int i = 0; i < this->_connections.size(); i++) {
		this->_connections[i]->name.setPosition({312, 120 + i * 16});
		this->_connections[i]->name.draw();
		this->_connections[i]->playerCount.setPosition({static_cast<int>(619 - this->_connections[i]->playerCount.getSize().x), 120 + i * 16});
		this->_connections[i]->playerCount.draw();
	}
	this->_connectionsMutex.unlock();
	(this->*_renderCallbacks[this->_menuState])();
	inputBoxRender();
	return 0;
}

void LobbyMenu::_dummyRender()
{
}

void LobbyMenu::_customizeAvatarRender()
{
	this->_customizeSeat.draw();
	for (auto &sprite : this->_customizeTexts)
		sprite.draw();

	SokuLib::Vector2i pos{78, 130};
	int size = 0;
#ifdef _DEBUG
	SokuLib::DrawUtils::RectangleShape rect;

	rect.setBorderColor(SokuLib::Color::White);
	rect.setFillColor(SokuLib::Color{0xFF, 0xFF, 0xFF, 0xA0});
#endif
	for (int i = 0; i < lobbyData->avatars.size(); i++) {
		auto &avatar = lobbyData->avatars[i];
		auto &showcase = this->_showcases[i];

		avatar.sprite.setSize({
			static_cast<unsigned int>(avatar.sprite.rect.width * avatar.scale / 2),
			static_cast<unsigned int>(avatar.sprite.rect.height * avatar.scale / 2)
		});
		if (pos.x + avatar.sprite.rect.width > 347) {
			pos.x = 78;
			pos.y += size;
			size = 0;
		}
		avatar.sprite.setPosition(pos);
#ifdef _DEBUG
		rect.setSize(avatar.sprite.getSize());
		rect.setPosition(pos);
		rect.draw();
#endif
		if (this->_customCursor == i)
			displaySokuCursor(pos + SokuLib::Vector2i{8, 0}, avatar.sprite.getSize());
		pos.x += avatar.sprite.rect.width;
		size = max(size, avatar.sprite.rect.height);
		avatar.sprite.setMirroring(showcase.side, false);
		avatar.sprite.rect.left = avatar.sprite.rect.width * showcase.anim;
		avatar.sprite.rect.top = avatar.sprite.rect.height * (showcase.action / 4);
		avatar.sprite.draw();
		avatar.sprite.setSize({
			static_cast<unsigned int>(avatar.sprite.rect.width * avatar.scale),
			static_cast<unsigned int>(avatar.sprite.rect.height * avatar.scale)
		});
	}
	this->_renderAvatarCustomText();
	this->_renderCustomAvatarPreview();
}

void LobbyMenu::setActive()
{
	this->_active = true;
}

void LobbyMenu::_masterServerLoop()
{
	while (this->_open) {
		bool _locked = false;

		if (this->_mainServer.isDisconnected())
			try {
				this->_loadingText.texture.createFromText("Connecting to server...", lobbyData->getFont(16), {600, 74});
				this->_lastError.clear();
				this->_mainServer.connect(servHost, servPort);
				puts("Connected!");
			} catch (std::exception &e) {
				this->_loadingText.texture.createFromText(("Connection failed:<br><color FF0000>" + std::string(e.what()) + "</color>").c_str(), lobbyData->getFont(16), {600, 74});
				this->_lastError = e.what();
				goto fail;
			}

		try {
			char packet[6] = {0};
			std::vector<std::pair<unsigned short, std::string>> elems;

			packet[0] = 1;
			this->_mainServer.send(packet, 3);
			this->_mainServer.read(packet, 6);
			while (packet[0] || packet[1]) {
				struct in_addr addr;

				addr.S_un.S_un_b.s_b1 = packet[2];
				addr.S_un.S_un_b.s_b2 = packet[3];
				addr.S_un.S_un_b.s_b3 = packet[4];
				addr.S_un.S_un_b.s_b4 = packet[5];
				elems.emplace_back(packet[0] | packet[1] << 8, inet_ntoa(addr));
				this->_mainServer.read(packet, 6);
			}
			_locked = true;
			this->_connectionsMutex.lock();
			this->_connections.erase(std::remove_if(this->_connections.begin(), this->_connections.end(), [&elems](const std::shared_ptr<Entry> &e){
				return (!e->c || !e->c->isConnected()) && std::find_if(elems.begin(), elems.end(), [&e](const std::pair<unsigned short, std::string> &e1){
					return e->port == e1.first && e->ip == e1.second;
				}) == elems.end();
			}), this->_connections.end());
			this->_connectionsMutex.unlock();
			for (auto &elem : elems) {
				this->_connectionsMutex.lock();
				if (std::find_if(this->_connections.begin(), this->_connections.end(), [&elem](const std::shared_ptr<Entry> &e){
					return e->ip == elem.second && e->port == elem.first;
				}) == this->_connections.end()) {
					this->_connections.emplace_back(new Entry{std::shared_ptr<Connection>(), elem.second, elem.first});

					auto c = this->_connections.back();

					this->_connectionsMutex.unlock();
					c->name.texture.createFromText("Connection to the lobby in queue...", lobbyData->getFont(16), {300, 74});
					c->name.setSize({
						c->name.texture.getSize().x,
						c->name.texture.getSize().y
					});
					c->name.rect.width = c->name.texture.getSize().x;
					c->name.rect.height = c->name.texture.getSize().y;
					c->name.tint = SokuLib::Color{0xA0, 0xA0, 0xA0, 0xFF};
				} else
					this->_connectionsMutex.unlock();
			}
		} catch (std::exception &e) {
			if (dynamic_cast<EOFException *>(&e))
				this->_mainServer.disconnect();
			this->_loadingText.texture.createFromText(("<color FF0000>Error when communicating with master server:</color><br>" + std::string(e.what())).c_str(), lobbyData->getFont(16), {600, 74});
			this->_lastError = e.what();
			if (_locked)
				this->_connectionsMutex.unlock();
		}
	fail:
		for (int i = 0; i < 100 && this->_open; i++)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void LobbyMenu::_connectLoop()
{
	int i = 0;

	while (this->_open) {
		for (int j = 0; ; j++) {
			this->_connectionsMutex.lock();
			if (j >= this->_connections.size()) {
				this->_connectionsMutex.unlock();
				break;
			}

			auto connection = this->_connections[j];

			this->_connectionsMutex.unlock();

			auto weak = std::weak_ptr(connection);

			try {
				if (!connection->c || !connection->c->isConnected()) {
					if (i != 0 && !connection->first)
						continue;

					connection->first = false;
					connection->name.texture.createFromText("Connecting to lobby...", lobbyData->getFont(16), {300, 74});
					connection->name.setSize({
						connection->name.texture.getSize().x,
						connection->name.texture.getSize().y
					});
					connection->name.rect.width = connection->name.texture.getSize().x;
					connection->name.rect.height = connection->name.texture.getSize().y;
					connection->name.tint = SokuLib::Color{0xFF, 0xFF, 0x00, 0xFF};

					connection->c = std::make_shared<Connection>(connection->ip, connection->port, this->_loadedSettings);
					connection->c->onError = [weak, this](const std::string &msg) {
						auto c = weak.lock();

						c->lastName = msg;
						c->name.texture.createFromText(c->lastName.c_str(), lobbyData->getFont(16), {300, 74});
						c->name.setSize({
							c->name.texture.getSize().x,
							c->name.texture.getSize().y
						});
						c->name.rect.width = c->name.texture.getSize().x;
						c->name.rect.height = c->name.texture.getSize().y;
						c->name.tint = SokuLib::Color::Red;
						SokuLib::playSEWaveBuffer(38);
						std::cerr << "Error:" << msg << std::endl;
					};
					connection->c->onImpMsg = [weak, this](const std::string &msg) {
						auto c = weak.lock();

						c->lastName = msg;
						c->name.texture.createFromText(c->lastName.c_str(), lobbyData->getFont(16), {300, 74});
						c->name.setSize({
							c->name.texture.getSize().x,
							c->name.texture.getSize().y
						});
						c->name.rect.width = c->name.texture.getSize().x;
						c->name.rect.height = c->name.texture.getSize().y;
						c->name.tint = SokuLib::Color{0xFF, 0x80, 0x00};
						SokuLib::playSEWaveBuffer(23);
						std::cerr << "Broadcast: " << msg << std::endl;
					};
					connection->c->startThread();
				}

				auto info = connection->c->getLobbyInfo();

				if (connection->lastName != info.name) {
					connection->lastName = info.name;
					connection->name.texture.createFromText(connection->lastName.c_str(), lobbyData->getFont(16), {300, 74});
					connection->name.setSize({
						connection->name.texture.getSize().x,
						connection->name.texture.getSize().y
					});
					connection->name.rect.width = connection->name.texture.getSize().x;
					connection->name.rect.height = connection->name.texture.getSize().y;
					connection->name.tint = SokuLib::Color::White;
				}
				if (
					connection->lastPlayerCount.first != info.currentPlayers ||
					connection->lastPlayerCount.second != info.maxPlayers
				) {
					SokuLib::Vector2i size;

					connection->lastPlayerCount = {info.currentPlayers, info.maxPlayers};
					connection->playerCount.texture.createFromText((std::to_string(info.currentPlayers) + "/" + std::to_string(info.maxPlayers)).c_str(), lobbyData->getFont(16), {300, 74}, &size);
					connection->playerCount.setSize(size.to<unsigned>());
					connection->playerCount.rect.width = connection->playerCount.getSize().x;
					connection->playerCount.rect.height = connection->playerCount.getSize().y;
					connection->playerCount.tint = SokuLib::Color::White;
				}
			} catch (std::exception &e) {
				connection->lastName.clear();
				connection->lastPlayerCount = {0, 0};
				connection->name.texture.createFromText(e.what(), lobbyData->getFont(16), {300, 74});
				connection->name.setSize({
					connection->name.texture.getSize().x,
					connection->name.texture.getSize().y
				});
				connection->name.rect.width = connection->name.texture.getSize().x;
				connection->name.rect.height = connection->name.texture.getSize().y;
				connection->name.tint = SokuLib::Color::Red;
				connection->playerCount.texture.destroy();
				connection->playerCount.setSize({0, 0});
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		i = (i + 1) % 200;
	}
}

void LobbyMenu::_refreshAvatarCustomText()
{
	auto &avatar = lobbyData->avatars[this->_customCursor];

	this->_customAvatarName.texture.createFromText(avatar.name.c_str(), lobbyData->getFont(16), {600, 74});
	this->_customAvatarName.setSize(this->_customAvatarName.texture.getSize());
	this->_customAvatarName.rect.width = this->_loadingText.texture.getSize().x;
	this->_customAvatarName.rect.height = this->_loadingText.texture.getSize().y;
	this->_customAvatarName.setPosition({354 + avatar.sprite.rect.width, 312});
	this->_customAvatarName.tint = lobbyData->isLocked(avatar) ? SokuLib::Color::Red : SokuLib::Color::Green;

	this->_customAvatarRequ.texture.createFromText("Unlocked by default", lobbyData->getFont(12), {600, 74});
	this->_customAvatarRequ.setSize(this->_customAvatarRequ.texture.getSize());
	this->_customAvatarRequ.rect.width = this->_customAvatarRequ.texture.getSize().x;
	this->_customAvatarRequ.rect.height = this->_customAvatarRequ.texture.getSize().y;
	this->_customAvatarRequ.setPosition({354 + avatar.sprite.rect.width, 330});
}

void LobbyMenu::_renderAvatarCustomText()
{
	auto &avatar = lobbyData->avatars[this->_customCursor];

	avatar.sprite.setSize({
		static_cast<unsigned int>(avatar.sprite.rect.width * avatar.scale / 2),
		static_cast<unsigned int>(avatar.sprite.rect.height * avatar.scale / 2)
	});
	avatar.sprite.setPosition({352, static_cast<int>(360 - avatar.sprite.getSize().y)});
#ifdef _DEBUG
	SokuLib::DrawUtils::RectangleShape rect;

	rect.setBorderColor(SokuLib::Color::White);
	rect.setFillColor(SokuLib::Color{0xFF, 0xFF, 0xFF, 0xA0});
	rect.setSize(avatar.sprite.getSize());
	rect.setPosition({352, static_cast<int>(360 - avatar.sprite.getSize().y)});
	rect.draw();
#endif
	avatar.sprite.setMirroring(true, false);
	avatar.sprite.rect.left = 0;
	avatar.sprite.rect.top = 0;
	avatar.sprite.draw();
	avatar.sprite.setSize({
		static_cast<unsigned int>(avatar.sprite.rect.width * avatar.scale),
		static_cast<unsigned int>(avatar.sprite.rect.height * avatar.scale)
	});
	this->_customAvatarName.draw();
	this->_customAvatarRequ.draw();
}

void LobbyMenu::_renderCustomAvatarPreview()
{
	auto &avatar = lobbyData->avatars[this->_loadedSettings.player.avatar];

	avatar.sprite.setPosition({
		455 - static_cast<int>(avatar.sprite.getSize().x / 2),
		static_cast<int>(285 - avatar.sprite.getSize().y)
	});
#ifdef _DEBUG
	SokuLib::DrawUtils::RectangleShape rect;

	rect.setBorderColor(SokuLib::Color::White);
	rect.setFillColor(SokuLib::Color{0xFF, 0xFF, 0xFF, 0xA0});
	rect.setSize(avatar.sprite.getSize());
	rect.setPosition(avatar.sprite.getPosition());
	rect.draw();
#endif
	avatar.sprite.rect.top = 0;
	avatar.sprite.rect.left = this->_showcases[this->_customCursor].anim * avatar.sprite.rect.width;
	avatar.sprite.setMirroring(false, false);
	avatar.sprite.draw();
	this->_playerName.setPosition({
		455 - static_cast<int>(this->_playerName.getSize().x / 2),
		20 + static_cast<int>(285 - avatar.sprite.getSize().y)
	});
	this->_playerName.draw();
}