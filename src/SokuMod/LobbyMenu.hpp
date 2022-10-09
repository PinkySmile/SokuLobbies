//
// Created by PinkySmile on 02/10/2022.
//

#ifndef SOKULOBBIES_LOBBYMENU_HPP
#define SOKULOBBIES_LOBBYMENU_HPP


#include <mutex>
#include <thread>
#include <SokuLib.hpp>
#include "Player.hpp"
#include "Socket.hpp"

class Connection;
class LobbyMenu : public SokuLib::IMenu {
private:
	struct Entry {
		std::shared_ptr<Connection> c;
		std::string ip;
		unsigned short port;
		std::string lastName;
		bool first = true;
		std::pair<unsigned, unsigned> lastPlayerCount = {0, 0};
		SokuLib::DrawUtils::Sprite name;
		SokuLib::DrawUtils::Sprite playerCount;
	};

	SokuLib::DrawUtils::Sprite title;
	SokuLib::DrawUtils::Sprite ui;
	std::thread _netThread;
	std::thread _connectThread;
	Player _loadedSettings;
	std::mutex _connectionsMutex;
	SokuLib::MenuConnect *_parent;
	unsigned _menuCursor = 0;
	unsigned char _menuState = 0;
	//TODO: Make this a vector of shared_ptr to facilitate access in threads
	std::vector<std::shared_ptr<Entry>> _connections;
	int _lobbyCtr = 0;
	std::string _lastError;
	Socket _mainServer;
	volatile bool _open = true;
	bool _active = true;
	std::thread _masterThread;
	SokuLib::SWRFont _defaultFont12;
	SokuLib::SWRFont _defaultFont16;
	SokuLib::DrawUtils::Sprite _loadingText;
	SokuLib::DrawUtils::Sprite _messageBox;
	SokuLib::DrawUtils::Sprite _loadingGear;

	void _netLoop();
	void _masterServerLoop();
	void _connectLoop();

public:
	struct Avatar {
		SokuLib::DrawUtils::Sprite sprite;
		unsigned accessoriesPlacement;
		unsigned animationsStep;
		unsigned nbAnimations;

		Avatar() = default;
		Avatar(const Avatar &) { assert(false); }
	};
	struct Background {
		SokuLib::DrawUtils::Sprite bg;
		SokuLib::DrawUtils::Sprite fg;
		unsigned groundPos;
		float parallaxFactor;
		unsigned platformInterval;
		unsigned platformWidth;
		unsigned platformCount;

		Background() = default;
		Background(const Background &) { assert(false); }
	};

	std::vector<Avatar> avatars;
	std::vector<Background> backgrounds;

	LobbyMenu(SokuLib::MenuConnect *parent);
	~LobbyMenu();
	void _() override;
	int onProcess() override;
	int onRender() override;
	void setActive();
};


#endif //SOKULOBBIES_LOBBYMENU_HPP
