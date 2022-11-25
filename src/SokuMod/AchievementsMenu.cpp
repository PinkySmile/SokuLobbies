//
// Created by PinkySmile on 02/10/2022.
//

#include <iostream>
#include <fstream>
#include <filesystem>
#include <../directx/dinput.h>
#include "LobbyData.hpp"
#include "AchievementsMenu.hpp"
#include "InputBox.hpp"
#include "data.hpp"
#include "StatsMenu.hpp"

void displaySokuCursor(SokuLib::Vector2i pos, SokuLib::Vector2u size);

AchievementsMenu::AchievementsMenu()
{
	std::filesystem::path folder = profileFolderPath;

	inputBoxLoadAssets();
	this->title.texture.loadFromFile((folder / "assets/menu/titleAchievements.png").string().c_str());
	this->title.setSize(this->title.texture.getSize());
	this->title.setPosition({23, 6});
	this->title.rect.width = this->title.getSize().x;
	this->title.rect.height = this->title.getSize().y;

	this->ui.texture.loadFromFile((folder / "assets/menu/list.png").string().c_str());
	this->ui.setSize(this->ui.texture.getSize());
	this->ui.rect.width = this->ui.getSize().x;
	this->ui.rect.height = this->ui.getSize().y;

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
	}
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
	if (lobbyData->achievementsLocked) {
		if (SokuLib::inputMgrs.input.a == 1 || SokuLib::inputMgrs.input.b == 1) {
			SokuLib::playSEWaveBuffer(0x29);
			return false;
		}
		return true;
	}
	if (SokuLib::inputMgrs.input.b == 1) {
		SokuLib::playSEWaveBuffer(0x29);
		return false;
	}
	return true;
}

int AchievementsMenu::onRender()
{
	this->title.draw();
	if (lobbyData->achievementsLocked) {
		this->_messageBox.draw();
		this->_loadingText.draw();
		return 0;
	}
	this->ui.draw();
	return 0;
}