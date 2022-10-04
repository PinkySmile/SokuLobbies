//
// Created by PinkySmile on 02/10/2022.
//

#include "InLobbyMenu.hpp"
#include <dinput.h>

InLobbyMenu::InLobbyMenu(LobbyMenu *menu, SokuLib::MenuConnect *parent, Connection &connection) :
	connection(connection),
	parent(parent),
	_menu(menu)
{
	this->onConnectRequest = connection.onConnectRequest;
	this->onError = connection.onError;
	this->onImpMsg = connection.onImpMsg;
	this->onMsg = connection.onMsg;
	this->onHostRequest = connection.onHostRequest;
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
		MessageBox(SokuLib::window, msg.c_str(), "Notification from server", MB_ICONINFORMATION);
		SokuLib::playSEWaveBuffer(23);
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
	this->_menu->setActive();
}

void InLobbyMenu::_()
{
	Lobbies::PacketArcadeLeave leave{0};

	this->connection.send(&leave, sizeof(leave));
	*(*(char **)0x89a390 + 20) = false;
	this->parent->choice = 0;
	this->parent->subchoice = 0;
	*(int *)0x882a94 = 0x16;
}

int InLobbyMenu::onProcess()
{
	auto inputs = SokuLib::inputMgrs.input;

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
	if (SokuLib::inputMgrs.input.b == 1 || SokuLib::checkKeyOneshot(DIK_ESCAPE, 0, 0, 0)) {
		SokuLib::playSEWaveBuffer(0x29);
		this->connection.disconnect();
		return false;
	}
	if (this->connection.isInit() && SokuLib::inputMgrs.input.a == 1 && this->connection.getMe()->battleStatus == 0) {
		Lobbies::PacketGameRequest packet{0};

		this->connection.getMe()->battleStatus = 1;
		this->connection.send(&packet, sizeof(packet));
	}
	return !this->_wasConnected || this->connection.isInit();
}

int InLobbyMenu::onRender()
{
	return 0;
}
