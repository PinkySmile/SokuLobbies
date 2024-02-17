//
// Created by PinkySmile on 02/10/2022.
//

#include <iostream>
#include <fstream>
#include <filesystem>
#include <encodingConverter.hpp>
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
#include "AchievementsMenu.hpp"
#include "integration.hpp"
#include "createUTFTexture.hpp"

#define CRenderer_Unknown1 ((void (__thiscall *)(int, int))0x404AF0)
#define runOnUI(fct) do {                     \
        this->_queueMutex.lock();             \
        this->_workerQueue.emplace_back(fct); \
        this->_queueMutex.unlock();           \
} while (false)


extern bool activated;
#ifdef _DEBUG
extern bool debug;
#endif

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
	this->_title.texture.loadFromFile((folder / "assets/menu/title.png").string().c_str());
	this->_title.setSize(this->_title.texture.getSize());
	this->_title.setPosition({23, 6});
	this->_title.rect.width = this->_title.getSize().x;
	this->_title.rect.height = this->_title.getSize().y;

	this->_ui.texture.loadFromFile((folder / "assets/menu/lobbylist.png").string().c_str());
	this->_ui.setSize(this->_ui.texture.getSize());
	this->_ui.rect.width = this->_ui.getSize().x;
	this->_ui.rect.height = this->_ui.getSize().y;

	this->_hidden.texture.loadFromFile((folder / "assets/avatars/hidden.png").string().c_str());
	this->_hidden.setSize(this->_hidden.texture.getSize());
	this->_hidden.rect.width = this->_hidden.getSize().x;
	this->_hidden.rect.height = this->_hidden.getSize().y;

	this->_customizeSeat.texture.loadFromFile((folder / "assets/menu/customSeat.png").string().c_str());
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

	th123intl::ConvertCodePage(th123intl::GetTextCodePage(), (char *)SokuLib::profile1.name, CP_UTF8, this->_loadedSettings.name);
	// For now, we get the stuff from the INI
	this->_loadedSettings.settings.hostPref = static_cast<Lobbies::HostPreference>(hostPref);
	this->_loadedSettings.pos.x = 20;

	SokuLib::Vector2i size;

	this->_version.texture.createFromText((std::string("Version ") + modVersion).c_str(), lobbyData->getFont(12), {200, 20}, &size);
	this->_version.setSize(size.to<unsigned>());
	this->_version.setPosition({635 - size.x, 460 - size.y});
	this->_version.rect.width = size.x;
	this->_version.rect.height = size.y;

	this->_playerName.texture.createFromText(SokuLib::profile1.name, lobbyData->getFont(16), {200, 20}, &size);
	this->_playerName.setSize(size.to<unsigned>());
	this->_playerName.rect.width = size.x;
	this->_playerName.rect.height = size.y;

	this->_customizeTexts[0].texture.createFromText("Avatar", lobbyData->getFont(20), {600, 74});
	this->_customizeTexts[0].setSize(this->_customizeTexts[0].texture.getSize());
	this->_customizeTexts[0].rect.width = this->_customizeTexts[0].texture.getSize().x;
	this->_customizeTexts[0].rect.height = this->_customizeTexts[0].texture.getSize().y;
	this->_customizeTexts[0].setPosition({120, 98});

	this->_customizeTexts[1].texture.createFromText("Accessory", lobbyData->getFont(20), {600, 74});
	this->_customizeTexts[1].setSize(this->_customizeTexts[1].texture.getSize());
	this->_customizeTexts[1].rect.width = this->_customizeTexts[1].texture.getSize().x;
	this->_customizeTexts[1].rect.height = this->_customizeTexts[1].texture.getSize().y;
	this->_customizeTexts[1].setPosition({280, 98});

	this->_customizeTexts[2].texture.createFromText("Title", lobbyData->getFont(20), {600, 74});
	this->_customizeTexts[2].setSize(this->_customizeTexts[2].texture.getSize());
	this->_customizeTexts[2].rect.width = this->_customizeTexts[2].texture.getSize().x;
	this->_customizeTexts[2].rect.height = this->_customizeTexts[2].texture.getSize().y;
	this->_customizeTexts[2].setPosition({475, 98});

	this->_loadingText.texture.createFromText("Connecting to server...", lobbyData->getFont(16), {600, 74});
	this->_loadingText.setSize(this->_loadingText.texture.getSize());
	this->_loadingText.rect.width = this->_loadingText.texture.getSize().x;
	this->_loadingText.rect.height = this->_loadingText.texture.getSize().y;
	this->_loadingText.setPosition({164, 218});

	this->_messageBox.texture.loadFromGame("data/menu/21_Base.cv2");
	this->_messageBox.setSize(this->_messageBox.texture.getSize());
	this->_messageBox.rect.width = this->_messageBox.texture.getSize().x;
	this->_messageBox.rect.height = this->_messageBox.texture.getSize().y;
	this->_messageBox.setPosition({155, 203});

	this->_loadingGear.texture.loadFromGame("data/scene/logo/gear.bmp");
	this->_loadingGear.setSize(this->_loadingGear.texture.getSize());
	this->_loadingGear.rect.width = this->_loadingGear.texture.getSize().x;
	this->_loadingGear.rect.height = this->_loadingGear.texture.getSize().y;

	this->_lock.texture.loadFromFile((folder / "assets/menu/lock.png").string().c_str());
	this->_lock.setSize(this->_lock.texture.getSize());
	this->_lock.rect.width = this->_lock.texture.getSize().x;
	this->_lock.rect.height = this->_lock.texture.getSize().y;

	this->_unlock.texture.loadFromFile((folder / "assets/menu/unlock.png").string().c_str());
	this->_unlock.setSize(this->_unlock.texture.getSize());
	this->_unlock.rect.width = this->_unlock.texture.getSize().x;
	this->_unlock.rect.height = this->_unlock.texture.getSize().y;

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
			if (c->c && c->c->hasConnected() && !c->c->isInit() && c->c->isConnected())
				c->c->send(&ping, sizeof(ping));
		this->_connectionsMutex.unlock();
		for (int i = 0; i < 20 && this->_open; i++)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void LobbyMenu::_()
{
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

void LobbyMenu::execUiCallbacks()
{
	int i = 0;

	this->_queueMutex.lock();
	for (auto &fct : this->_workerQueue) {
		fct();
		if (i >= 10)
			break;
		i++;
	}
	this->_workerQueue.clear();
	this->_queueMutex.unlock();
}

int LobbyMenu::onProcess()
{
	try {
		int i = 0;

		this->_queueMutex.lock();
		for (auto &fct : this->_workerQueue) {
			fct();
			if (i >= 10)
				break;
			i++;
		}
		this->_workerQueue.clear();
		this->_queueMutex.unlock();
		inputBoxUpdate();
		if (inputBoxShown)
			return true;
		if (SokuLib::checkKeyOneshot(DIK_ESCAPE, 0, 0, 0)) {
			playSound(0x29);
			this->_open = false;
			return false;
		}
		if (this->_mainServer.isDisconnected()) {
			this->_loadingGear.setRotation(this->_loadingGear.getRotation() + 0.1);
			return true;
		}
		return (this->*_updateCallbacks[this->_menuState])();
	} catch (std::exception &e) {
		MessageBoxA(
			SokuLib::window,
			(
				"Error updating lobby menu.\n"
				"Please, report this error.\n"
				"\n"
				"Error:\n" +
				std::string(e.what())
			).c_str(),
			"SokuLobby error",
			MB_ICONERROR
		);
		return false;
	}
}

bool LobbyMenu::_normalMenuUpdate()
{
	if (SokuLib::inputMgrs.input.b == 1) {
		playSound(0x29);
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
		playSound(0x27);
	}
	if (SokuLib::inputMgrs.input.spellcard == 1) {
		setInputBoxCallbacks([this](const std::string &value){
			GuardedMutex m{this->_connectionsMutex};

			try {
				auto colon = value.find_last_of(':');
				auto ip = value.substr(0, colon);
				unsigned short port = colon == std::string::npos ? 10800 : std::stoul(value.substr(colon + 1));

				m.lock();
				this->_connections.emplace_back(new Entry{std::shared_ptr<Connection>(), ip, port});
				playSound(0x28);
			} catch (std::exception &e) {
				puts(e.what());
				playSound(0x29);
			}
		});
		openInputDialog("Enter lobby ip", "localhost:10800");
	}
	if (SokuLib::inputMgrs.input.a == 1) {
		playSound(0x28);
		switch (this->_menuCursor) {
		case MENUITEM_JOIN_LOBBY:
			this->_menuState = 1;
			break;
		case MENUITEM_CUSTOMIZE_AVATAR:
			this->_customCursor = 0;
			this->_avatarTop = 0;
			this->_refreshAvatarCustomText();
			this->_showcases.clear();
			this->_showcases.resize(lobbyData->avatars.size());
			this->_customizeTexts[0].tint = SokuLib::Color::White;
			this->_customizeTexts[1].tint = SokuLib::Color{0x80, 0x80, 0x80, 0xFF};
			this->_customizeTexts[2].tint = SokuLib::Color{0x80, 0x80, 0x80, 0xFF};
			this->_menuState = 2;
			break;
		case MENUITEM_CREATE_LOBBY:
			MessageBox(SokuLib::window, "Creating lobbies in game is not yet supported.\nUse the RunServer.bat helper provided with the mod to create a lobby.", "Partially implemented", MB_ICONINFORMATION);
			break;
		case MENUITEM_CUSTOMIZE_LOBBY:
		case MENUITEM_OPTIONS:
			MessageBox(SokuLib::window, "Not implemented", "Not implemented", MB_ICONINFORMATION);
			break;
		case MENUITEM_ACHIVEMENTS:
			SokuLib::activateMenu(new AchievementsMenu());
			break;
		case MENUITEM_STATISTICS:
			SokuLib::activateMenu(new StatsMenu());
			break;
		case MENUITEM_EXIT:
			activated = false;
			break;
		}
	}
	return true;
}

bool LobbyMenu::_joinLobbyUpdate()
{
	if (SokuLib::inputMgrs.input.b == 1) {
		playSound(0x29);
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
		playSound(0x27);
	}
	if (SokuLib::inputMgrs.input.a == 1) {
		if (this->_lobbyCtr < this->_connections.size() && this->_connections[this->_lobbyCtr]->c && this->_connections[this->_lobbyCtr]->c->isConnected()) {
			auto c = this->_connections[this->_lobbyCtr];

			if (c->passwd) {
				setInputBoxCallbacks([this, c](const std::string &value){
					if (c->c && c->c->isConnected()) {
						c->c->setPassword(value);
						SokuLib::activateMenu(new InLobbyMenu(this, this->_parent, c->c));
						this->_active = false;
						playSound(0x28);
					} else
						playSound(0x29);
				});
				openInputDialog("Enter password", "", '*');
			} else {
				SokuLib::activateMenu(new InLobbyMenu(this, this->_parent, this->_connections[this->_lobbyCtr]->c));
				this->_active = false;
				playSound(0x28);
			}
		} else
			playSound(0x29);
	}
	this->_connectionsMutex.unlock();
	return true;
}

void LobbyMenu::_updateTopAvatarOffset()
{
	SokuLib::Vector2i pos{78, static_cast<int>(130 - this->_avatarTop)};
	int size = 0;

	for (int i = 0; i < lobbyData->avatars.size(); i++) {
		auto &avatar = lobbyData->avatars[i];
		auto &showcase = this->_showcases[i];
		constexpr unsigned maxBottom = 362;

		if (pos.x + avatar.sprite.rect.width > 347) {
			pos.x = 78;
			pos.y += size;
			size = 0;
		}

		if (i == this->_customCursor) {
			if (pos.y < 130) {
				this->_avatarTop -= 130 - pos.y;
				return;
			}

			auto bottom = pos.y + (avatar.sprite.texture.getSize().y / 2) * avatar.scale / 2;

			if (bottom > maxBottom)
				this->_avatarTop += (bottom - maxBottom) * 2 / avatar.scale;
			return;
		}
		pos.x += avatar.sprite.rect.width;
		size = max(size, avatar.sprite.texture.getSize().y / 2);
	}
}

bool LobbyMenu::_customizeAvatarUpdate()
{
	if (SokuLib::inputMgrs.input.a == 1 && this->_customCursor >= 0) {
		if (!lobbyData->isLocked(lobbyData->avatars[this->_customCursor])) {
			playSound(0x28);
			this->_loadedSettings.player.avatar = this->_customCursor;
		} else
			playSound(0x29);
		return true;
	}
	if (SokuLib::inputMgrs.input.b == 1) {
		playSound(0x29);
		this->_menuState = 0;
		return true;
	}
	if (std::abs(SokuLib::inputMgrs.input.horizontalAxis) == 1 || (std::abs(SokuLib::inputMgrs.input.horizontalAxis) > 36 && std::abs(SokuLib::inputMgrs.input.horizontalAxis) % 6 == 0)) {
		playSound(0x27);
		if (this->_customCursor == 0 && SokuLib::inputMgrs.input.horizontalAxis < 0)
			this->_customCursor += lobbyData->avatars.size() - 1;
		else
			this->_customCursor = (this->_customCursor + (int)std::copysign(1, SokuLib::inputMgrs.input.horizontalAxis)) % lobbyData->avatars.size();
		this->_updateTopAvatarOffset();
		this->_refreshAvatarCustomText();
	}
	if (std::abs(SokuLib::inputMgrs.input.verticalAxis) == 1 || (std::abs(SokuLib::inputMgrs.input.verticalAxis) > 36 && std::abs(SokuLib::inputMgrs.input.verticalAxis) % 6 == 0)) {
		playSound(0x27);
		if (this->_customCursor < 4 && SokuLib::inputMgrs.input.verticalAxis < 0)
			this->_customCursor = 0;
		else if (this->_customCursor + 4 >= lobbyData->avatars.size() && SokuLib::inputMgrs.input.verticalAxis > 0)
			this->_customCursor = lobbyData->avatars.size() - 1;
		else
			this->_customCursor = this->_customCursor + (int)std::copysign(4, SokuLib::inputMgrs.input.verticalAxis);
		this->_updateTopAvatarOffset();
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
	try {
		this->_title.draw();
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
		this->_ui.draw();
		this->_connectionsMutex.lock();
		if (this->_menuState == 1)
			displaySokuCursor({312, static_cast<int>(120 + this->_lobbyCtr * 16)}, {220, 16});
		for (int i = 0; i < this->_connections.size(); i++) {
			auto &connection = *this->_connections[i];

			connection.name.setPosition({312, 120 + i * 16});
			connection.name.draw();
			connection.playerCount.setPosition({static_cast<int>(619 - connection.playerCount.getSize().x), 120 + i * 16});
			connection.playerCount.draw();
			if (!connection.first) {
				(connection.passwd ? this->_lock : this->_unlock).setPosition({292, 120 + i * 16});
				(connection.passwd ? this->_lock : this->_unlock).draw();
			}
		}
		this->_connectionsMutex.unlock();
		(this->*_renderCallbacks[this->_menuState])();
		inputBoxRender();
		this->_version.draw();
	} catch (std::exception &e) {
		MessageBoxA(
			SokuLib::window,
			(
				"Error updating lobby menu.\n"
				"Please, report this error.\n"
				"\n"
				"Error:\n" +
				std::string(e.what())
			).c_str(),
			"SokuLobby error",
			MB_ICONERROR
		);
	}
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

	SokuLib::Vector2i pos{78, static_cast<int>(130 - this->_avatarTop)};
	int size = 0;
#ifdef _DEBUG
	SokuLib::DrawUtils::RectangleShape rect;

	if (debug) {
		rect.setBorderColor(SokuLib::Color::White);
		rect.setFillColor(SokuLib::Color{0xFF, 0xFF, 0xFF, 0xA0});
	}
#endif
	for (int i = 0; i < lobbyData->avatars.size(); i++) {
		auto &avatar = lobbyData->avatars[i];
		auto &showcase = this->_showcases[i];
		constexpr int maxBottom = 362;

		if (pos.x + avatar.sprite.rect.width > 347) {
			pos.x = 78;
			pos.y += size;
			size = 0;
			if (pos.y >= maxBottom)
				break;
		}

		auto locked = lobbyData->isLocked(avatar);
		auto otherSprite = locked && avatar.hidden;

		if (otherSprite) {
			auto bottom = pos.y + this->_hidden.texture.getSize().y;

			this->_hidden.rect.width = this->_hidden.texture.getSize().x;
			if (bottom > maxBottom)
				this->_hidden.rect.height = this->_hidden.texture.getSize().y - (bottom - maxBottom);
			else
				this->_hidden.rect.height = this->_hidden.texture.getSize().y;
			this->_hidden.setSize({
				static_cast<unsigned int>(this->_hidden.rect.width),
				static_cast<unsigned int>(this->_hidden.rect.height)
			});
			this->_hidden.rect.left = 0;
			this->_hidden.rect.top = 0;

			auto realPos = pos;

			if (pos.y + (int)this->_hidden.texture.getSize().y <= 130);
			else if (pos.y < 130) {
				this->_hidden.rect.top += 130 - pos.y;
				this->_hidden.rect.height -= 130 - pos.y;
				pos.y = 130;
				this->_hidden.setSize({
					static_cast<unsigned int>(this->_hidden.rect.width),
					static_cast<unsigned int>(this->_hidden.rect.height)
				});
			}
			this->_hidden.setPosition(pos);
#ifdef _DEBUG
			if (debug) {
				rect.setSize(this->_hidden.getSize());
				rect.setPosition(pos);
				if (this->_hidden.getPosition().y >= 130)
					rect.draw();
			}
#endif

			if (this->_customCursor == i)
				displaySokuCursor(pos + SokuLib::Vector2i{8, 0}, this->_hidden.getSize());
			pos = realPos;
			size = max(size, this->_hidden.texture.getSize().y);
			if (this->_hidden.getPosition().y >= 130) {
				this->_hidden.draw();
				if (locked && bottom <= maxBottom) {
					this->_lock.setPosition(
						pos + SokuLib::Vector2i{
							static_cast<int>(this->_hidden.rect.width / 2 - this->_lock.getSize().x / 2),
							static_cast<int>(this->_hidden.texture.getSize().y / 2)
						}
					);
					if (this->_lock.getPosition().y >= 130)
						this->_lock.draw();
				}
			}
			pos.x += this->_hidden.getSize().x;
		} else {
			auto bottom = pos.y + (avatar.sprite.texture.getSize().y / 2) * avatar.scale / 2;

			if (bottom > maxBottom)
				avatar.sprite.rect.height = avatar.sprite.texture.getSize().y / 2 - (bottom - maxBottom) * 2 / avatar.scale;
			else
				avatar.sprite.rect.height = avatar.sprite.texture.getSize().y / 2;
			avatar.sprite.setSize({
				static_cast<unsigned int>(avatar.sprite.rect.width * avatar.scale / 2),
				static_cast<unsigned int>(avatar.sprite.rect.height * avatar.scale / 2)
			});
			avatar.sprite.rect.left = avatar.sprite.rect.width * showcase.anim;
			avatar.sprite.rect.top = (avatar.sprite.texture.getSize().y / 2) * (showcase.action / 4);

			auto realPos = pos;

			if (pos.y + (int)avatar.sprite.getSize().y <= 130);
			else if (pos.y < 130) {
				avatar.sprite.rect.top += 130 - pos.y;
				avatar.sprite.rect.height -= (130 - pos.y) * 2 / avatar.scale;
				pos.y = 130;
				avatar.sprite.setSize({
					static_cast<unsigned int>(avatar.sprite.rect.width * avatar.scale / 2),
					static_cast<unsigned int>(avatar.sprite.rect.height * avatar.scale / 2)
				});
			}
			avatar.sprite.setPosition(pos);
#ifdef _DEBUG
			if (debug) {
				rect.setSize(avatar.sprite.getSize());
				rect.setPosition(pos);
				if (avatar.sprite.getPosition().y >= 130)
					rect.draw();
			}
#endif

			if (this->_customCursor == i)
				displaySokuCursor(pos + SokuLib::Vector2i{8, 0}, avatar.sprite.getSize());
			pos = realPos;
			size = max(size, (avatar.sprite.texture.getSize().y / 2) * avatar.scale / 2);
			avatar.sprite.setMirroring(showcase.side, false);
			avatar.sprite.tint = locked ? SokuLib::Color{0x60, 0x60, 0x60, 0xFF} : SokuLib::Color::White;
			if (avatar.sprite.getPosition().y >= 130) {
				avatar.sprite.draw();
				if (locked && bottom <= maxBottom) {
					this->_lock.setPosition(
						pos + SokuLib::Vector2i{
							static_cast<int>(avatar.sprite.rect.width * avatar.scale / 2 / 2 - this->_lock.getSize().x / 2),
							static_cast<int>(avatar.sprite.texture.getSize().y / 2 * avatar.scale / 2 / 2)
						}
					);
					if (this->_lock.getPosition().y >= 130)
						this->_lock.draw();
				}
			}
			pos.x += avatar.sprite.getSize().x;
			avatar.sprite.rect.height = avatar.sprite.texture.getSize().y / 2;
			avatar.sprite.setSize({
				static_cast<unsigned int>(avatar.sprite.rect.width * avatar.scale),
				static_cast<unsigned int>(avatar.sprite.rect.height * avatar.scale)
			});
		}
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
				auto fct = [this]{
					this->_loadingText.texture.createFromText(
						"Connecting to server...",
						lobbyData->getFont(16),
						{600, 74}
					);
				};
				runOnUI(fct);
				this->_lastError.clear();
				this->_mainServer.connect(servHost, servPort);
				puts("Connected!");
			} catch (std::exception &e) {
				auto fct = [this, e]{
					this->_loadingText.texture.createFromText(
						("Connection failed:<br><color FF0000>" + std::string(e.what()) + "</color>").c_str(),
						lobbyData->getFont(16),
						{600, 74});
				};
				runOnUI(fct);
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
					auto fct = [c]{
						c->name.texture.createFromText("Connection to the lobby in queue...", lobbyData->getFont(16), {300, 74});
						c->name.setSize({
							c->name.texture.getSize().x,
							c->name.texture.getSize().y
						});
						c->name.rect.width = c->name.texture.getSize().x;
						c->name.rect.height = c->name.texture.getSize().y;
						c->name.tint = SokuLib::Color{0xA0, 0xA0, 0xA0, 0xFF};
					};
					runOnUI(fct);
				} else
					this->_connectionsMutex.unlock();
			}
		} catch (std::exception &e) {
			if (dynamic_cast<EOFException *>(&e))
				this->_mainServer.disconnect();
			auto fct = [this, e]{
				this->_loadingText.texture.createFromText(
					("<color FF0000>Error when communicating with master server:</color><br>" + std::string(e.what())).c_str(),
					lobbyData->getFont(16),
					{600, 74}
				);
			};
			runOnUI(fct);
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

					auto fct = [connection]{
						connection->name.texture.createFromText("Connecting to lobby...", lobbyData->getFont(16), {300, 74});
						connection->name.setSize({
							connection->name.texture.getSize().x,
							connection->name.texture.getSize().y
						});
						connection->name.rect.width = connection->name.texture.getSize().x;
						connection->name.rect.height = connection->name.texture.getSize().y;
						connection->name.tint = SokuLib::Color{0xFF, 0xFF, 0x00, 0xFF};
					};
					runOnUI(fct);

					connection->c = std::make_shared<Connection>(connection->ip, connection->port, this->_loadedSettings);
					connection->c->onError = [weak, this](const std::string &msg) {
						auto fct = [weak, msg, this]{
							auto c = weak.lock();

							std::cerr << "Error:" << msg << std::endl;
							if (!c)
								return;
							c->lastName = msg;
							c->name.texture.createFromText(c->lastName.c_str(), lobbyData->getFont(16), {300, 74});
							c->name.setSize({
								c->name.texture.getSize().x,
								c->name.texture.getSize().y
							});
							c->name.rect.width = c->name.texture.getSize().x;
							c->name.rect.height = c->name.texture.getSize().y;
							c->name.tint = SokuLib::Color::Red;
							if (this->_active)
								playSound(38);
						};
						runOnUI(fct);
					};
					connection->c->onImpMsg = [weak, this](const std::string &msg) {
						auto fct = [weak, msg, this]{
							auto c = weak.lock();

							std::cerr << "Broadcast: " << msg << std::endl;
							if (!c)
								return;
							c->lastName = msg;
							c->name.texture.createFromText(c->lastName.c_str(), lobbyData->getFont(16), {300, 74});
							c->name.setSize({
								c->name.texture.getSize().x,
								c->name.texture.getSize().y
							});
							c->name.rect.width = c->name.texture.getSize().x;
							c->name.rect.height = c->name.texture.getSize().y;
							c->name.tint = SokuLib::Color{0xFF, 0x80, 0x00};
							if (this->_active)
								playSound(23);
						};
						runOnUI(fct);
					};
					connection->c->startThread();
				}

				auto info = connection->c->getLobbyInfo();

				connection->passwd = info.hasPwd;
				if (connection->lastName != info.name) {
					auto fct = [info, connection]{
						SokuLib::Vector2i size;
						int texId = 0;

						connection->lastName = info.name;
						if (!createTextTexture(texId, convertEncoding<char, wchar_t, UTF8Decode, UTF16Encode>(info.name).c_str(), lobbyData->getFont(16), {300, 74}, &size))
							puts("Error creating text texture");
						connection->name.texture.setHandle(texId, {300, 74});
						connection->name.setSize({
							connection->name.texture.getSize().x,
							connection->name.texture.getSize().y
						});
						connection->name.rect.width = connection->name.texture.getSize().x;
						connection->name.rect.height = connection->name.texture.getSize().y;
						connection->name.tint = SokuLib::Color::White;
					};
					runOnUI(fct);
				}
				if (
					connection->lastPlayerCount.first != info.currentPlayers ||
					connection->lastPlayerCount.second != info.maxPlayers
				) {
					auto fct = [info, connection]{
						SokuLib::Vector2i size;

						connection->lastPlayerCount = {info.currentPlayers, info.maxPlayers};
						connection->playerCount.texture.createFromText((std::to_string(info.currentPlayers) + "/" + std::to_string(info.maxPlayers)).c_str(), lobbyData->getFont(16), {300, 74}, &size);
						connection->playerCount.setSize(size.to<unsigned>());
						connection->playerCount.rect.width = connection->playerCount.getSize().x;
						connection->playerCount.rect.height = connection->playerCount.getSize().y;
						connection->playerCount.tint = SokuLib::Color::White;
					};
					runOnUI(fct);
				}
			} catch (std::exception &e) {
				auto ptr = strdup(e.what());
				auto fct = [ptr, connection]{
					connection->lastName.clear();
					connection->lastPlayerCount = {0, 0};
					connection->name.texture.createFromText(ptr, lobbyData->getFont(16), {300, 74});
					connection->name.setSize({
						connection->name.texture.getSize().x,
						connection->name.texture.getSize().y
					});
					connection->name.rect.width = connection->name.texture.getSize().x;
					connection->name.rect.height = connection->name.texture.getSize().y;
					connection->name.tint = SokuLib::Color::Red;
					connection->playerCount.texture.destroy();
					connection->playerCount.setSize({0, 0});
					free(ptr);
				};

				runOnUI(fct);
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		i = (i + 1) % 200;
	}
}

void LobbyMenu::_refreshAvatarCustomText()
{
	auto &avatar = lobbyData->avatars[this->_customCursor];

	if (lobbyData->isLocked(avatar) && avatar.hidden)
		this->_customAvatarName.texture.createFromText("????????", lobbyData->getFont(16), {600, 74});
	else
		this->_customAvatarName.texture.createFromText(avatar.name.c_str(), lobbyData->getFont(16), {600, 74});
	this->_customAvatarName.setSize(this->_customAvatarName.texture.getSize());
	this->_customAvatarName.rect.width = this->_loadingText.texture.getSize().x;
	this->_customAvatarName.rect.height = this->_loadingText.texture.getSize().y;
	this->_customAvatarName.setPosition({354 + avatar.sprite.rect.width, 312});
	this->_customAvatarName.tint = lobbyData->isLocked(avatar) ? SokuLib::Color::Red : SokuLib::Color::Green;

	this->_customAvatarRequ.setPosition({354 + avatar.sprite.rect.width, 330});
	if (!avatar.requirement)
		this->_customAvatarRequ.texture.createFromText("Unlocked by default", lobbyData->getFont(12), {600, 74});
	else if (avatar.requirement->name.size() <= 22)
		this->_customAvatarRequ.texture.createFromText(("Unlocked by completing<br>\"<color 8080FF>" + avatar.requirement->name + "</color>\"").c_str(), lobbyData->getFont(12), {600, 74});
	else {
		auto name = avatar.requirement->name;
		int pos = 23;
		constexpr int offset = 6;

		while (pos && !isspace(name[pos]))
			pos--;
		name = name.substr(0, pos) + "<br>" + name.substr(pos + 1);
		this->_customAvatarRequ.texture.createFromText(("Unlocked by completing<br>\"<color 8080FF>" + name + "</color>\"").c_str(), lobbyData->getFont(12), {600, 74});
		this->_customAvatarName.setPosition({354 + avatar.sprite.rect.width, 312 - offset});
		this->_customAvatarRequ.setPosition({354 + avatar.sprite.rect.width, 330 - offset});
	}
	this->_customAvatarRequ.setSize(this->_customAvatarRequ.texture.getSize());
	this->_customAvatarRequ.rect.width = this->_customAvatarRequ.texture.getSize().x;
	this->_customAvatarRequ.rect.height = this->_customAvatarRequ.texture.getSize().y;
}

void LobbyMenu::_renderAvatarCustomText()
{
	auto &avatar = lobbyData->avatars[this->_customCursor];
	auto locked = lobbyData->isLocked(avatar);
	auto otherSprite = locked && avatar.hidden;
	auto &sprite = otherSprite ? this->_hidden : avatar.sprite;

	if (!otherSprite)
		avatar.sprite.setSize({
			static_cast<unsigned int>(avatar.sprite.rect.width * avatar.scale / 2),
			static_cast<unsigned int>(avatar.sprite.rect.height * avatar.scale / 2)
		});
	sprite.setPosition({
		static_cast<int>(384 - sprite.getSize().x / 2),
		static_cast<int>(360 - sprite.getSize().y)
	});
#ifdef _DEBUG
	if (debug) {
		SokuLib::DrawUtils::RectangleShape rect;

		rect.setBorderColor(SokuLib::Color::White);
		rect.setFillColor(SokuLib::Color{0xFF, 0xFF, 0xFF, 0xA0});
		rect.setSize(sprite.getSize());
		rect.setPosition(sprite.getPosition());
		rect.draw();
	}
#endif
	if (!otherSprite) {
		avatar.sprite.setMirroring(true, false);
		avatar.sprite.rect.left = 0;
		avatar.sprite.rect.top = 0;
		avatar.sprite.tint = lobbyData->isLocked(avatar) ? SokuLib::Color{0x60, 0x60, 0x60, 0xFF} : SokuLib::Color::White;
		avatar.sprite.draw();
		avatar.sprite.setSize({
			static_cast<unsigned int>(avatar.sprite.rect.width * avatar.scale),
			static_cast<unsigned int>(avatar.sprite.rect.height * avatar.scale)
		});
	} else
		sprite.draw();
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

	if (debug) {
		rect.setBorderColor(SokuLib::Color::White);
		rect.setFillColor(SokuLib::Color{0xFF, 0xFF, 0xFF, 0xA0});
		rect.setSize(avatar.sprite.getSize());
		rect.setPosition(avatar.sprite.getPosition());
		rect.draw();
	}
#endif
	avatar.sprite.rect.top = 0;
	avatar.sprite.rect.left = this->_showcases[this->_loadedSettings.player.avatar].anim * avatar.sprite.rect.width;
	avatar.sprite.setMirroring(false, false);
	avatar.sprite.draw();
	this->_playerName.setPosition({
		455 - static_cast<int>(this->_playerName.getSize().x / 2),
		20 + static_cast<int>(285 - avatar.sprite.getSize().y)
	});
	this->_playerName.draw();
}