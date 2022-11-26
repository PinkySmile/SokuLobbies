//
// Created by PinkySmile on 02/10/2022.
//

#include <filesystem>
#include <../directx/dinput.h>
#include "LobbyData.hpp"
#include "AchievementsMenu.hpp"
#include "InputBox.hpp"
#include "data.hpp"
#include "StatsMenu.hpp"

#define ACH_PER_PAGE 15

void displaySokuCursor(SokuLib::Vector2i pos, SokuLib::Vector2u size);

AchievementsMenu::AchievementsMenu()
{
	std::filesystem::path folder = profileFolderPath;
	SokuLib::Vector2i size;

	this->_title.texture.loadFromFile((folder / "assets/menu/titleAchievements.png").string().c_str());
	this->_title.setSize(this->_title.texture.getSize());
	this->_title.setPosition({23, 6});
	this->_title.rect.width = this->_title.getSize().x;
	this->_title.rect.height = this->_title.getSize().y;

	if (lobbyData->achievementsLocked) {
		this->_loadingText.texture.createFromText("Achievements have been locked because<br>the save file has been tampered with.", lobbyData->getFont(16), {600, 74});
		this->_loadingText.setSize({
			this->_loadingText.texture.getSize().x,
			this->_loadingText.texture.getSize().y
		});
		this->_loadingText.rect.width = this->_loadingText.texture.getSize().x;
		this->_loadingText.rect.height = this->_loadingText.texture.getSize().y;
		this->_loadingText.setPosition({164, 218});

		this->_messageBox.texture.loadFromGame("data/menu/21_Base.cv2");
		this->_messageBox.setSize({
			this->_messageBox.texture.getSize().x,
			this->_messageBox.texture.getSize().y
		});
		this->_messageBox.rect.width = this->_messageBox.texture.getSize().x;
		this->_messageBox.rect.height = this->_messageBox.texture.getSize().y;
		this->_messageBox.setPosition({155, 203});
#ifndef _DEBUG
		return;
#endif
	}

	this->_ui.texture.loadFromFile((folder / "assets/menu/list.png").string().c_str());
	this->_ui.setSize(this->_ui.texture.getSize());
	this->_ui.rect.width = this->_ui.getSize().x;
	this->_ui.rect.height = this->_ui.getSize().y;

	this->_hiddenSprite.texture.createFromText("Goal hidden until completion", lobbyData->getFont(14), {400, 100}, &size);
	size.x += 20;
	this->_hiddenSprite.rect.width = size.x;
	this->_hiddenSprite.rect.height = size.y;
	this->_hiddenSprite.setSize(size.to<unsigned>());
	this->_hiddenSprite.setPosition({0, -20});
	this->_hiddenSprite.tint = SokuLib::Color{0x40, 0x40, 0x40};

	for (auto &ach : lobbyData->achievements)
		ach.nameSpriteFull.tint = ach.awarded ? SokuLib::Color::White : SokuLib::Color{0x80, 0x80, 0x80};

	this->_updateAchRightPanel();
}

AchievementsMenu::~AchievementsMenu()
{
}

void AchievementsMenu::_()
{
	puts("_ !");
	*(int *)0x882a94 = 0x16;
}

int AchievementsMenu::onProcess()
{
	if (SokuLib::checkKeyOneshot(DIK_ESCAPE, 0, 0, 0)) {
		SokuLib::playSEWaveBuffer(0x29);
		return false;
	}
#ifndef _DEBUG
	if (lobbyData->achievementsLocked) {
		if (SokuLib::inputMgrs.input.a == 1 || SokuLib::inputMgrs.input.b == 1) {
			SokuLib::playSEWaveBuffer(0x29);
			return false;
		}
		return true;
	}
#endif
	if (SokuLib::inputMgrs.input.b == 1) {
		SokuLib::playSEWaveBuffer(0x29);
		return false;
	}
#ifdef _DEBUG
	if (SokuLib::inputMgrs.input.changeCard == 1) {
		lobbyData->achievementAwardQueue.clear();
		lobbyData->achievementAwardQueue.push_back(&lobbyData->achievements[this->_selected]);
	}
	if (SokuLib::inputMgrs.input.spellcard == 1) {
		auto &achievement = lobbyData->achievements[this->_selected];

		achievement.awarded = !achievement.awarded;
		achievement.nameSpriteFull.tint = achievement.awarded ? SokuLib::Color::White : SokuLib::Color{0x80, 0x80, 0x80, 0xFF};
	}
#endif

	if (std::abs(SokuLib::inputMgrs.input.horizontalAxis) == 1 || (std::abs(SokuLib::inputMgrs.input.horizontalAxis) > 36 && std::abs(SokuLib::inputMgrs.input.horizontalAxis) % 6 == 0)) {
		int result = this->_selected + std::copysign(ACH_PER_PAGE, SokuLib::inputMgrs.input.horizontalAxis);

		if (result < 0)
			result = 0;
		else if (result >= lobbyData->achievements.size())
			result = lobbyData->achievements.size() - 1;
		if (result != this->_selected)
			SokuLib::playSEWaveBuffer(0x27);
		this->_selected = result;
		this->_updateAchRightPanel();
	}
	if (std::abs(SokuLib::inputMgrs.input.verticalAxis) == 1 || (std::abs(SokuLib::inputMgrs.input.verticalAxis) > 36 && std::abs(SokuLib::inputMgrs.input.verticalAxis) % 6 == 0)) {
		SokuLib::playSEWaveBuffer(0x27);
		this->_selected = static_cast<unsigned>(this->_selected + std::copysign(1, SokuLib::inputMgrs.input.verticalAxis) + lobbyData->achievements.size()) % lobbyData->achievements.size();
		this->_updateAchRightPanel();
	}
	return true;
}

int AchievementsMenu::onRender()
{
	this->_title.draw();
	if (lobbyData->achievementsLocked) {
		this->_messageBox.draw();
		this->_loadingText.draw();
#ifndef _DEBUG
		return 0;
#endif
	}
	this->_ui.draw();
	for (unsigned i = 0; i + this->_top < lobbyData->achievements.size() && i < ACH_PER_PAGE; i++) {
		auto &ach = lobbyData->achievements[i + this->_top];

		if (i + this->_top == this->_selected)
			displaySokuCursor(ach.nameSpriteFull.getPosition() - SokuLib::Vector2i{4, 0}, {300, 16});
		ach.nameSpriteFull.draw();
	}

	auto &ach = lobbyData->achievements[this->_selected];

	ach.nameSpriteTitle.draw();
	if (!ach.hidden || ach.awarded) {
		ach.descSprite.draw();
		this->_panelRightSprite.draw();
	} else
		this->_hiddenSprite.draw();
	return 0;
}

void AchievementsMenu::_updateAchRightPanel()
{
	if (this->_selected < this->_top)
		this->_top = this->_selected;
	else if (this->_selected >= this->_top + ACH_PER_PAGE)
		this->_top = this->_selected - ACH_PER_PAGE + 1;
	for (unsigned i = 0; i + this->_top < lobbyData->achievements.size() && i < ACH_PER_PAGE; i++)
		lobbyData->achievements[i + this->_top].nameSpriteFull.setPosition({50, static_cast<int>(110 + i * 20)});

	auto &ach = lobbyData->achievements[this->_selected];
	auto &desc = !ach.hidden || ach.awarded ? ach.descSprite : this->_hiddenSprite;

	desc.setPosition({315, ach.nameSpriteTitle.getPosition().y + 30});
	if (ach.hidden && !ach.awarded)
		return;

	std::string str = "Rewards:";

	for (auto &reward : ach.rewards) {
		str += "<br>";
		str += reward.dump(3);
	}
	for (auto p = str.find('\n'); p != std::string::npos; p = str.find('\n'))
		str.replace(p, 1, "<br>");
	this->_panelRightSprite.texture.createFromText(str.c_str(), lobbyData->getFont(12), {600, 474});
	this->_panelRightSprite.setSize({
		this->_panelRightSprite.texture.getSize().x,
		this->_panelRightSprite.texture.getSize().y
	});
	this->_panelRightSprite.rect.width = this->_panelRightSprite.texture.getSize().x;
	this->_panelRightSprite.rect.height = this->_panelRightSprite.texture.getSize().y;
	this->_panelRightSprite.setPosition({315, static_cast<int>(desc.getPosition().y + desc.getSize().y)});
}
