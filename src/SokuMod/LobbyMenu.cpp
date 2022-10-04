//
// Created by PinkySmile on 02/10/2022.
//

#include "LobbyMenu.hpp"
#include "InLobbyMenu.hpp"
#include <../directx/dinput.h>

LobbyMenu::LobbyMenu(SokuLib::MenuConnect *parent) :
	_parent(parent)
{
	this->_loadedSettings.settings.hostPref = Lobbies::HOSTPREF_NO_PREF;
	this->_loadedSettings.player.title = 0;
	this->_loadedSettings.player.avatar = 0;
	this->_loadedSettings.player.head = 0;
	this->_loadedSettings.player.body = 0;
	this->_loadedSettings.player.back = 0;
	this->_loadedSettings.player.env = 0;
	this->_loadedSettings.player.feet = 0;
	this->_loadedSettings.name = SokuLib::profile1.name.operator std::string();
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
	return 0;
}

void LobbyMenu::setActive()
{
	this->_active = true;
}
