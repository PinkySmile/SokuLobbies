//
// Created by PinkySmile on 02/10/2022.
//

#ifndef SOKULOBBIES_ACHIEVEMENTMENU_HPP
#define SOKULOBBIES_ACHIEVEMENTMENU_HPP


#include <mutex>
#include <thread>
#include <SokuLib.hpp>
#include "Player.hpp"
#include "Socket.hpp"

class AchievementsMenu : public SokuLib::IMenu {
private:
	SokuLib::DrawUtils::Sprite title;
	SokuLib::DrawUtils::Sprite ui;
	SokuLib::DrawUtils::Sprite _loadingText;
	SokuLib::DrawUtils::Sprite _messageBox;

public:
	AchievementsMenu();
	~AchievementsMenu();
	void _() override;
	int onProcess() override;
	int onRender() override;
};

#endif //SOKULOBBIES_LOBBYMENU_HPP
