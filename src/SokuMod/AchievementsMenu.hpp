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
	unsigned _top = 0;
	unsigned _selected = 0;
	SokuLib::DrawUtils::Sprite _title;
	SokuLib::DrawUtils::Sprite _ui;
	SokuLib::DrawUtils::Sprite _loadingText;
	SokuLib::DrawUtils::Sprite _messageBox;
	SokuLib::DrawUtils::Sprite _panelRightSprite;
	SokuLib::DrawUtils::Sprite _hiddenSprite;

	void _updateAchRightPanel();

public:
	AchievementsMenu();
	~AchievementsMenu();
	void _() override;
	int onProcess() override;
	int onRender() override;
};

#endif //SOKULOBBIES_ACHIEVEMENTMENU_HPP
