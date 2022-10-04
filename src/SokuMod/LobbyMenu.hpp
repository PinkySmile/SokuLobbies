//
// Created by PinkySmile on 02/10/2022.
//

#ifndef SOKULOBBIES_LOBBYMENU_HPP
#define SOKULOBBIES_LOBBYMENU_HPP


#include <mutex>
#include <thread>
#include <SokuLib.hpp>
#include "Connection.hpp"

class LobbyMenu : public SokuLib::IMenu {
private:
	std::thread _netThread;
	Player _loadedSettings;
	std::mutex _connectionsMutex;
	SokuLib::MenuConnect *_parent;
	std::vector<std::shared_ptr<Connection>> _connections;
	volatile bool _open = true;
	bool _active = true;

	void _netLoop();

public:
	LobbyMenu(SokuLib::MenuConnect *parent);
	~LobbyMenu();
	void _() override;
	int onProcess() override;
	int onRender() override;
	void setActive();
};


#endif //SOKULOBBIES_LOBBYMENU_HPP
