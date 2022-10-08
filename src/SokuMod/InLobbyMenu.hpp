//
// Created by PinkySmile on 02/10/2022.
//

#ifndef SOKULOBBIES_INLOBBYMENU_HPP
#define SOKULOBBIES_INLOBBYMENU_HPP


#include <thread>
#include <Socket.hpp>
#include <SokuLib.hpp>
#include "Connection.hpp"
#include "LobbyMenu.hpp"

class InLobbyMenu : public SokuLib::IMenu {
private:
	struct PlayerData {
		SokuLib::DrawUtils::Sprite name;
	};

	std::function<void (const std::string &ip, unsigned short port, bool spectate)> onConnectRequest;
	std::function<void (const std::string &msg)> onError;
	std::function<void (const std::string &msg)> onImpMsg;
	std::function<void (int32_t channel, const std::string &msg)> onMsg;
	std::function<void (const Player &)> onPlayerJoin;
	std::function<unsigned short ()> onHostRequest;
	std::function<void (const Lobbies::PacketOlleh &)> onConnect;
	SokuLib::Vector2i _translate{0, 0};
	Connection &connection;
	SokuLib::MenuConnect *parent;
	LobbyMenu *_menu;
	bool _wasConnected = false;
	uint8_t _background = 0;
	std::string _music;
	SokuLib::SWRFont _defaultFont16;
	SokuLib::DrawUtils::Sprite _loadingText;
	SokuLib::DrawUtils::Sprite _messageBox;
	SokuLib::DrawUtils::Sprite _loadingGear;
	SokuLib::DrawUtils::Sprite _inBattle;
	std::map<uint32_t, PlayerData> _extraPlayerData;

public:
	InLobbyMenu(LobbyMenu *menu, SokuLib::MenuConnect *parent, Connection &connection);
	~InLobbyMenu();
	void _() override;
	int onProcess() override;
	int onRender() override;
};


#endif //SOKULOBBIES_INLOBBYMENU_HPP
