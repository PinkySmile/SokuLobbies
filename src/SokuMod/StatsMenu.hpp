//
// Created by PinkySmile on 09/11/2022.
//

#ifndef SOKULOBBIES_STATSMENU_HPP
#define SOKULOBBIES_STATSMENU_HPP


#include <memory>
#include <SokuLib.hpp>

class StatsMenu : public SokuLib::IMenu {
private:
	struct Category {
		SokuLib::DrawUtils::Sprite title;
		SokuLib::DrawUtils::Sprite sprite;
	};
	struct ChrEntry {
		SokuLib::DrawUtils::Sprite portraitTitle;
		SokuLib::DrawUtils::Sprite portrait;
		SokuLib::DrawUtils::Sprite title;
		SokuLib::DrawUtils::Sprite name;
		SokuLib::DrawUtils::Sprite wins;
		SokuLib::DrawUtils::Sprite losses;
		SokuLib::DrawUtils::Sprite total;
		SokuLib::DrawUtils::Sprite winratio;
	};

	SokuLib::DrawUtils::Sprite title;
	std::vector<Category> _categories;
	std::vector<std::shared_ptr<ChrEntry>> _globalStats;
	std::vector<std::shared_ptr<ChrEntry>> _againstStats;
	std::vector<std::vector<std::shared_ptr<ChrEntry>>> _matchupStats;
	std::vector<std::vector<std::shared_ptr<ChrEntry>>> _cardsStats;
	unsigned _start = 0;
	unsigned _categorySelected = 0;
	unsigned _currentMenu = 0;
	unsigned _nbMenus = 0;
	bool _isSelected = false;

	std::vector<std::shared_ptr<ChrEntry>> *_getCurrentList(unsigned *maxLine, unsigned *lineSize);
	void _createGlobalStats();
	void _createAgainstStats();
	void _createMUStats(std::vector<std::shared_ptr<ChrEntry>> &list, const std::pair<unsigned, struct Character> &chr);
	void _createCardsStats(std::vector<std::shared_ptr<ChrEntry>> &list, const std::pair<unsigned, struct Character> &chr);
	void _updateNormalStats(const std::vector<std::shared_ptr<ChrEntry>> &list, unsigned maxLine, unsigned lineSize);
	void _renderNormalStats(const std::vector<std::shared_ptr<ChrEntry>> &list, unsigned maxLine);

public:
	StatsMenu();
	void _() override;
	int onProcess() override;
	int onRender() override;
};


#endif //SOKULOBBIES_STATSMENU_HPP
