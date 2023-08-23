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

struct ExtraCategory {
	std::string name;
	std::string sprite;
	bool fromGame;
};

static const char *rewardNames[] = {
	"Accessory (Not Implemented)",
	"Avatar",
	"Background (Not Implemented)",
	"Emote",
	"Prop (Not Implemented)",
	"Title (Not Implemented)",
	"Unknown"
};

static ExtraCategory extra[] = {
	{"meta", "assets/menu/meta_category.png", false},
	{"", "assets/menu/all_category.png", false}
};

AchievementsMenu::AchievementsMenu()
{
	std::filesystem::path folder = profileFolderPath;
	std::map<std::string, std::pair<int, int>> scores;
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

	this->_reward.texture.createFromText("Reward:", lobbyData->getFont(16), {400, 100}, &size);
	this->_reward.rect.width = size.x;
	this->_reward.rect.height = size.y;
	this->_reward.setSize(size.to<unsigned>());
	this->_reward.setPosition({0, -20});
	this->_reward.tint = SokuLib::Color::White;

	for (int i = 0; i < 6; i++) {
		auto &sprite = this->_panRightSpriteTxt[i];
		SokuLib::Vector2i size;

		sprite.texture.createFromText(rewardNames[i], lobbyData->getFont(14), {600, 474}, &size);
		sprite.setSize(size.to<unsigned>());
		sprite.rect.width = size.x;
		sprite.rect.height = size.y;
	}
	for (auto &ach : lobbyData->achievements)
		ach.nameSpriteFull.tint = ach.awarded ? SokuLib::Color::White : SokuLib::Color{0x80, 0x80, 0x80};
	
	this->_categories.reserve(characters.size() - 2 + (sizeof(extra) / sizeof(*extra)));
	for (auto &chr : characters) {
		if (chr.first == SokuLib::CHARACTER_RANDOM || chr.first == SokuLib::CHARACTER_NAMAZU)
			continue;
		this->_categories.emplace_back();

		auto &category = this->_categories.back();

		category.name = chr.second.codeName;
		category.sprite.texture.loadFromGame(("data/character/" + chr.second.codeName + "/face/face000.png").c_str());
		category.sprite.setMirroring(true, false);
	}
	for (unsigned i = 0; i < sizeof(extra) / sizeof(*extra); i++) {
		this->_categories.emplace_back();

		auto &category = this->_categories.back();

		category.name = extra[i].name;
		if (extra[i].fromGame)
			category.sprite.texture.loadFromGame(extra[i].sprite.c_str());
		else
			category.sprite.texture.loadFromFile((std::filesystem::path(profileFolderPath) / extra[i].sprite).string().c_str());
	}
	for (auto &ach : lobbyData->achievements) {
		if (!ach.category.empty()) {
			scores[ach.category].first += ach.awarded;
			scores[ach.category].second++;
		}
		scores[""].first += ach.awarded;
		scores[""].second++;
	}
	for (unsigned i = 0; i < this->_categories.size(); i++) {
		char buffer[10];
		auto &category = this->_categories[i];

		category.sprite.setSize({80, 32});
		category.sprite.setPosition({
			static_cast<int>(i) % 5 * 100 + 60,
			static_cast<int>(i) / 5 * 40 + 100
		});
		category.sprite.rect.width = category.sprite.texture.getSize().x;
		category.sprite.rect.height = category.sprite.texture.getSize().y;

		sprintf(buffer, "%i/%i", scores[category.name].first, scores[category.name].second);
		category.completed.texture.createFromText(buffer, lobbyData->getFont(14), {400, 100}, &size);
		category.completed.rect.width = size.x;
		category.completed.rect.height = size.y;
		category.completed.setSize(size.to<unsigned>());
		category.completed.setPosition({
			category.sprite.getPosition().x + static_cast<int>(category.sprite.getSize().x) - size.x / 2,
			category.sprite.getPosition().y + static_cast<int>(category.sprite.getSize().y) - size.y
		});
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

void AchievementsMenu::_inCategoryUpdate()
{
	if (SokuLib::inputMgrs.input.b == 1) {
		this->_achList.clear();
		playSound(0x29);
		return;
	}
#ifdef _DEBUG
	if (SokuLib::inputMgrs.input.changeCard == 1) {
		lobbyData->achievementAwardQueue.clear();
		lobbyData->achievementAwardQueue.push_back(this->_achList[this->_selected]);
	}
	if (SokuLib::inputMgrs.input.spellcard == 1) {
		auto &achievement = *this->_achList[this->_selected];

		achievement.awarded = !achievement.awarded;
		achievement.nameSpriteFull.tint = achievement.awarded ? SokuLib::Color::White : SokuLib::Color{0x80, 0x80, 0x80, 0xFF};
	}
#endif

	if (std::abs(SokuLib::inputMgrs.input.horizontalAxis) == 1 || (std::abs(SokuLib::inputMgrs.input.horizontalAxis) > 36 && std::abs(SokuLib::inputMgrs.input.horizontalAxis) % 6 == 0)) {
		int result = this->_selected + std::copysign(ACH_PER_PAGE, SokuLib::inputMgrs.input.horizontalAxis);

		if (result < 0)
			result = 0;
		else if (result >= this->_achList.size())
			result = this->_achList.size() - 1;
		if (result != this->_selected)
			playSound(0x27);
		this->_selected = result;
		this->_updateAchRightPanel();
	}
	if (std::abs(SokuLib::inputMgrs.input.verticalAxis) == 1 || (std::abs(SokuLib::inputMgrs.input.verticalAxis) > 36 && std::abs(SokuLib::inputMgrs.input.verticalAxis) % 6 == 0)) {
		playSound(0x27);
		this->_selected = static_cast<unsigned>(this->_selected + std::copysign(1, SokuLib::inputMgrs.input.verticalAxis) + this->_achList.size()) % this->_achList.size();
		this->_updateAchRightPanel();
	}
}

int AchievementsMenu::onProcess()
{
	if (SokuLib::checkKeyOneshot(DIK_ESCAPE, 0, 0, 0)) {
		playSound(0x29);
		return false;
	}
#ifndef _DEBUG
	if (lobbyData->achievementsLocked) {
		if (SokuLib::inputMgrs.input.a == 1 || SokuLib::inputMgrs.input.b == 1) {
			playSound(0x29);
			return false;
		}
		return true;
	}
#endif
	if (!this->_achList.empty())
		return this->_inCategoryUpdate(), true;
	if (SokuLib::inputMgrs.input.b == 1) {
		playSound(0x29);
		return false;
	}
	if (std::abs(SokuLib::inputMgrs.input.horizontalAxis) == 1 || (std::abs(SokuLib::inputMgrs.input.horizontalAxis) > 36 && std::abs(SokuLib::inputMgrs.input.horizontalAxis) % 6 == 0)) {
		int result = this->_categorySelected + std::copysign(1, SokuLib::inputMgrs.input.horizontalAxis);

		if (result < 0)
			this->_categorySelected = this->_categories.size() - 1;
		else
			this->_categorySelected = result % this->_categories.size();
		playSound(0x27);
	}
	if (std::abs(SokuLib::inputMgrs.input.verticalAxis) == 1 || (std::abs(SokuLib::inputMgrs.input.verticalAxis) > 36 && std::abs(SokuLib::inputMgrs.input.verticalAxis) % 6 == 0)) {
		playSound(0x27);
		if (SokuLib::inputMgrs.input.verticalAxis > 0 && this->_categorySelected + 5 > this->_categories.size())
			this->_categorySelected %= 5;
		else if (SokuLib::inputMgrs.input.verticalAxis < 0 && this->_categorySelected < 5) {
			this->_categorySelected = this->_categories.size() - (this->_categories.size() % 5) + this->_categorySelected;
			if (this->_categorySelected >= this->_categories.size())
				this->_categorySelected -= 5;
		} else
			this->_categorySelected += std::copysign(5, SokuLib::inputMgrs.input.verticalAxis);
	}
	if (SokuLib::inputMgrs.input.a == 1) {
		auto &category = this->_categories[this->_categorySelected];

		for (auto &ach : lobbyData->achievements)
			if (category.name.empty() || category.name == ach.category)
				this->_achList.push_back(&ach);
		playSound(0x28);
		this->_selected = 0;
		this->_updateAchRightPanel();
	}
	return true;
}

SokuLib::DrawUtils::Sprite *AchievementsMenu::getRewardText(const std::string &type)
{
	if (type == "accessory")
		return &this->_panRightSpriteTxt[0];
	if (type == "avatar")
		return &this->_panRightSpriteTxt[1];
	if (type == "background")
		return &this->_panRightSpriteTxt[2];
	if (type == "emote")
		return &this->_panRightSpriteTxt[3];
	if (type == "prop")
		return &this->_panRightSpriteTxt[4];
	if (type == "title")
		return &this->_panRightSpriteTxt[5];
	return &this->_panRightSpriteTxt[6];
}

void AchievementsMenu::_inCategoryRender()
{
	this->_ui.draw();
	for (unsigned i = 0; i + this->_top < this->_achList.size() && i < ACH_PER_PAGE; i++) {
		auto &ach = *this->_achList[i + this->_top];

		if (i + this->_top == this->_selected)
			displaySokuCursor(ach.nameSpriteFull.getPosition() - SokuLib::Vector2i{4, 0}, {300, 16});
		ach.nameSpriteFull.draw();
	}

	auto &ach = *this->_achList[this->_selected];

	ach.nameSpriteTitle.draw();
	for (auto &pair : this->_avatars) {
		pair.first.second++;
		if (pair.first.second >= pair.second->animationsStep) {
			pair.first.second = 0;
			pair.first.first++;
			pair.first.first %= pair.second->nbAnimations;
		}
		pair.second->sprite.rect.left = pair.second->sprite.rect.width * pair.first.first;
	}
	if (!ach.hidden || ach.awarded)
		ach.descSprite.draw();
	else
		this->_hiddenSprite.draw();
	for (auto s : this->_rewardSprites)
		s->draw();
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
	if (!this->_achList.empty())
		return this->_inCategoryRender(), 0;
	for (unsigned i = 0; i < this->_categories.size(); i++) {
		if (i == this->_categorySelected)
			displaySokuCursor(
				this->_categories[i].sprite.getPosition() + SokuLib::Vector2i{8, 0},
				this->_categories[i].sprite.getSize() - SokuLib::Vector2i{8, 0}
			);
		this->_categories[i].sprite.draw();
		this->_categories[i].completed.draw();
	}
	return 0;
}

void AchievementsMenu::_updateAchRightPanel()
{
	if (this->_selected < this->_top)
		this->_top = this->_selected;
	else if (this->_selected >= this->_top + ACH_PER_PAGE)
		this->_top = this->_selected - ACH_PER_PAGE + 1;
	for (unsigned i = 0; i + this->_top < this->_achList.size() && i < ACH_PER_PAGE; i++)
		this->_achList[i + this->_top]->nameSpriteFull.setPosition({50, static_cast<int>(110 + i * 20)});

	auto &ach = *this->_achList[this->_selected];
	auto &desc = !ach.hidden || ach.awarded ? ach.descSprite : this->_hiddenSprite;

	desc.setPosition({315, ach.nameSpriteTitle.getPosition().y + 30});
	this->_reward.setPosition({315, static_cast<int>(desc.getPosition().y + desc.getSize().y + 12)});

	int top = this->_reward.getPosition().y + this->_reward.getSize().y + 4;

	this->_avatars.clear();
	this->_rewards.clear();
	this->_rewardSprites.clear();
	this->_extraSprites.clear();
	this->_rewardSprites.push_back(&this->_reward);
	for (auto &reward : ach.rewards)
		this->_rewards[reward["type"]].push_back(reward);
	for (auto &pair : this->_rewards) {
		auto &sprite = *this->getRewardText(pair.first);
		int x = 0;
		int size = 0;

		sprite.setPosition({315, top});
		top += sprite.getSize().y;
		this->_rewardSprites.push_back(&sprite);
		if (pair.first == "avatar") {
			for (auto &reward : pair.second) {
				auto &avatar = *lobbyData->avatarsByCode[reward["id"]];

				avatar.sprite.setPosition({315 + x, top});
				avatar.sprite.setSize({
					static_cast<unsigned int>(avatar.sprite.rect.width * avatar.scale / 2),
					static_cast<unsigned int>(avatar.sprite.rect.height * avatar.scale / 2)
				});
				avatar.sprite.tint = SokuLib::Color::White;
				avatar.sprite.setMirroring(false, false);
				avatar.sprite.rect.left = 0;
				avatar.sprite.rect.top = 0;
				x += avatar.sprite.getSize().x;
				size = max(size, avatar.sprite.getSize().y);
				this->_avatars.emplace_back(std::pair<unsigned,unsigned>{0, 0}, &avatar);
				this->_rewardSprites.push_back(&avatar.sprite);
			}
		} else if (pair.first == "emote") {
			top += 2;
			for (auto &reward : pair.second) {
				auto emote = lobbyData->emotesByName[reward["name"]];
				unsigned offset = 20;
				SokuLib::Vector2i asize;

				emote->sprite.setSize({32, 32});
				emote->sprite.draw();
				this->_extraSprites.emplace_back(new SokuLib::DrawUtils::Sprite());

				auto &s = *this->_extraSprites.back();
	
				s.texture.createFromText((":" + reward["name"].get<std::string>() + ":").c_str(), lobbyData->getFont(12), {400, 100}, &asize);
				s.rect.width = asize.x;
				s.rect.height = asize.y;
				s.setSize(asize.to<unsigned>());
				if (asize.x <= 32) {
					emote->sprite.setPosition({315 + x, top});
					s.setPosition({315 + x + 32 - asize.x / 2, top + 34});
				} else {
					emote->sprite.setPosition({315 + x - 32 + asize.x - (asize.x - 32) / 2, top});
					s.setPosition({315 + x, top + 34});
				}
				s.tint = SokuLib::Color::White;
				x += max(36, asize.x + 4);
				size = max(size, 38 + asize.y);
				this->_rewardSprites.push_back(&s);
				this->_rewardSprites.push_back(&emote->sprite);
			}
		}
		top += 8;
		top += size;
	}
}
