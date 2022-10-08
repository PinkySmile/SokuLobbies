//
// Created by PinkySmile on 02/10/2022.
//

#ifndef SOKULOBBIES_LOBBYMENU_HPP
#define SOKULOBBIES_LOBBYMENU_HPP


#include <mutex>
#include <thread>
#include <SokuLib.hpp>
#include "Player.hpp"

class Connection;
class LobbyMenu : public SokuLib::IMenu {
private:
	SokuLib::DrawUtils::Sprite title;
	std::thread _netThread;
	Player _loadedSettings;
	std::mutex _connectionsMutex;
	SokuLib::MenuConnect *_parent;
	std::vector<std::shared_ptr<Connection>> _connections;
	volatile bool _open = true;
	bool _active = true;

	void _netLoop();

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
