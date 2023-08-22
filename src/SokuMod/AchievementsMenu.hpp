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
#include "LobbyData.hpp"

class AchievementsMenu : public SokuLib::IMenu {
private:
	unsigned _top = 0;
	unsigned _selected = 0;
	SokuLib::DrawUtils::Sprite _title;
	SokuLib::DrawUtils::Sprite _ui;
	SokuLib::DrawUtils::Sprite _loadingText;
	SokuLib::DrawUtils::Sprite _messageBox;
	SokuLib::DrawUtils::Sprite _hiddenSprite;
	SokuLib::DrawUtils::Sprite _titles;
	SokuLib::DrawUtils::Sprite _reward;
	SokuLib::DrawUtils::Sprite _panRightSpriteTxt[6];
	std::map<std::string, std::vector<nlohmann::json>> _rewards;
	std::vector<std::unique_ptr<SokuLib::DrawUtils::Sprite>> _extraSprites;
	std::vector<SokuLib::DrawUtils::Sprite *> _rewardSprites;
	std::vector<std::pair<std::pair<unsigned, unsigned>, LobbyData::Avatar *>> _avatars;

	void _updateAchRightPanel();
	void _renderRightPanelRewards();
	SokuLib::DrawUtils::Sprite *getRewardText(const std::string &type);

public:
	AchievementsMenu();
	~AchievementsMenu();
	void _() override;
	int onProcess() override;
	int onRender() override;
};

#endif //SOKULOBBIES_ACHIEVEMENTMENU_HPP
