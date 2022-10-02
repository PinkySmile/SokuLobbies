//
// Created by PinkySmile on 02/10/2022.
//

#include "InLobbyMenu.hpp"
#include "dinput.h"

InLobbyMenu::InLobbyMenu(SokuLib::MenuConnect *parent, Connection &connection) :
	connection(connection),
	parent(parent)
{
	this->onConnectRequest = connection.onConnectRequest;
	this->onError = connection.onError;
	this->onImpMsg = connection.onImpMsg;
	this->onMsg = connection.onMsg;
	this->onHostRequest = connection.onHostRequest;
	connection.onConnectRequest = [this](const std::string &ip, unsigned short port, bool spectate){
		SokuLib::playSEWaveBuffer(57);
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
}

void InLobbyMenu::_()
{
	puts("_ !");
	*(int *)0x882a94 = 0x16;
}

int InLobbyMenu::onProcess()
{
	if (SokuLib::inputMgrs.input.b == 1 || SokuLib::checkKeyOneshot(DIK_ESCAPE, 0, 0, 0)) {
		SokuLib::playSEWaveBuffer(0x29);
		this->connection.disconnect();
		return false;
	}
	if (SokuLib::inputMgrs.input.a == 1 && this->connection.getMe()->battleStatus == 0) {
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
