//
// Created by PinkySmile on 02/10/2022.
//

#ifndef SOKULOBBIES_INLOBBYMENU_HPP
#define SOKULOBBIES_INLOBBYMENU_HPP


#include <thread>
#include <Socket.hpp>
#include <SokuLib.hpp>
#include "Connection.hpp"

class InLobbyMenu : public SokuLib::IMenu {
private:
	std::function<void (const std::string &ip, unsigned short port, bool spectate)> onConnectRequest;
	std::function<void (const std::string &msg)> onError;
	std::function<void (const std::string &msg)> onImpMsg;
	std::function<void (int32_t channel, const std::string &msg)> onMsg;
	std::function<unsigned short ()> onHostRequest;
	Connection &connection;
	SokuLib::MenuConnect *parent;
	bool _wasConnected = false;

public:
	InLobbyMenu(SokuLib::MenuConnect *parent, Connection &connection);
	~InLobbyMenu();
	void _() override;
	int onProcess() override;
	int onRender() override;
};


#endif //SOKULOBBIES_INLOBBYMENU_HPP
