//
// Created by PinkySmile on 02/10/2022.
//

#include <fstream>
#include <filesystem>
#include <../directx/dinput.h>
#include "nlohmann/json.hpp"
#include "LobbyMenu.hpp"
#include "InLobbyMenu.hpp"

extern wchar_t profileFolderPath[MAX_PATH];

LobbyMenu::LobbyMenu(SokuLib::MenuConnect *parent) :
	_parent(parent),
	avatars()
{
	auto path = std::filesystem::path(profileFolderPath) / "assets/avatars/list.json";
	std::ifstream stream{path};
	nlohmann::json j;

	if (stream.fail())
		throw std::runtime_error("Cannot open file " + path.string());
	stream >> j;
	stream.close();
	this->avatars.reserve(j.size());
	for (auto &val : j) {
		this->avatars.emplace_back();

		auto &avatar = this->avatars.back();

		avatar.accessoriesPlacement = val["accessories"];
		avatar.nbAnimations = val["animations"];
		avatar.sprite.texture.loadFromFile((std::filesystem::path(profileFolderPath) / val["spritesheet"].get<std::string>()).string().c_str());
		avatar.sprite.rect.width = avatar.sprite.texture.getSize().x / avatar.nbAnimations;
		avatar.sprite.rect.height = avatar.sprite.texture.getSize().y / 2;
		avatar.sprite.setSize({
			static_cast<unsigned int>(avatar.sprite.rect.width * val["scale"].get<float>()),
			static_cast<unsigned int>(avatar.sprite.rect.height * val["scale"].get<float>())
		});
	}

	path = std::filesystem::path(profileFolderPath) / "assets/backgrounds/list.json";
	stream.open(path);
	if (stream.fail())
		throw std::runtime_error("Cannot open file " + path.string());
	stream >> j;
	stream.close();
	this->backgrounds.reserve(j.size());
	for (auto &val : j) {
		this->backgrounds.emplace_back();

		auto &bg = this->backgrounds.back();

		bg.groundPos = val["ground"];
		bg.parallaxFactor = val["parallax_factor"];
		bg.platformInterval = val["platform_interval"];
		bg.platformWidth = val["platform_width"];
		bg.platformCount = val["platform_count"];
		bg.fg.texture.loadFromFile((std::filesystem::path(profileFolderPath) / val["fg"].get<std::string>()).string().c_str());
		bg.fg.setSize(bg.fg.texture.getSize());
		bg.fg.rect.width = bg.fg.getSize().x;
		bg.fg.rect.height = bg.fg.getSize().y;
		bg.bg.texture.loadFromFile((std::filesystem::path(profileFolderPath) / val["bg"].get<std::string>()).string().c_str());
		bg.bg.setSize(bg.bg.texture.getSize());
		bg.bg.rect.width = bg.bg.getSize().x;
		bg.bg.rect.height = bg.bg.getSize().y;
	}

	this->title.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/menu/title.png").string().c_str());
	this->title.setSize(this->title.texture.getSize());
	this->title.setPosition({23, 6});
	this->title.rect.width = this->title.getSize().x;
	this->title.rect.height = this->title.getSize().y;

	//TODO: Save and load this in a file
	this->_loadedSettings.settings.hostPref = Lobbies::HOSTPREF_NO_PREF;
	this->_loadedSettings.player.title = 0;
	this->_loadedSettings.player.avatar = 0;
	this->_loadedSettings.player.head = 0;
	this->_loadedSettings.player.body = 0;
	this->_loadedSettings.player.back = 0;
	this->_loadedSettings.player.env = 0;
	this->_loadedSettings.player.feet = 0;
	this->_loadedSettings.name = SokuLib::profile1.name.operator std::string();

	//TODO: Actually have a list
	this->_connections.emplace_back(new Connection("localhost", 10900, this->_loadedSettings));
	this->_connections.back()->onError = [this](const std::string &msg){
		SokuLib::playSEWaveBuffer(38);
		MessageBox(SokuLib::window, msg.c_str(), "Internal Error", MB_ICONERROR);
	};
	this->_connections.back()->startThread();
	this->_netThread = std::thread(&LobbyMenu::_netLoop, this);
}

LobbyMenu::~LobbyMenu()
{
	this->_open = false;
	if (this->_netThread.joinable())
		this->_netThread.join();
}

void LobbyMenu::_netLoop()
{
	while (this->_open) {
		Lobbies::PacketPing ping;

		for (int i = 0; i < 20; i++) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			if (!this->_open)
				return;
		}
		if (!this->_active)
			continue;
		this->_connectionsMutex.lock();
		for (auto &c : this->_connections)
			c->send(&ping, sizeof(ping));
		this->_connectionsMutex.unlock();
	}
}

void LobbyMenu::_()
{
	puts("_ !");
	*(int *)0x882a94 = 0x16;
}

int LobbyMenu::onProcess()
{
	if (SokuLib::inputMgrs.input.b == 1 || SokuLib::checkKeyOneshot(DIK_ESCAPE, 0, 0, 0)) {
		SokuLib::playSEWaveBuffer(0x29);
		this->_open = false;
		return false;
	}
	if (SokuLib::inputMgrs.input.a == 1) {
		this->_connectionsMutex.lock();
		SokuLib::activateMenu(new InLobbyMenu(this, this->_parent, *this->_connections[0]));
		this->_active = false;
		this->_connectionsMutex.unlock();
		SokuLib::playSEWaveBuffer(0x28);
	}
	return true;
}

int LobbyMenu::onRender()
{
	this->title.draw();
	return 0;
}

void LobbyMenu::setActive()
{
	this->_active = true;
}
