//
// Created by PinkySmile on 09/11/2022.
//

#ifndef SOKULOBBIES_STATSMENU_HPP
#define SOKULOBBIES_STATSMENU_HPP


#include <memory>
#include <SokuLib.hpp>

class StatsMenu : public SokuLib::IMenu {
private:
	struct ChrEntry {
		SokuLib::DrawUtils::Sprite portrait;
		SokuLib::DrawUtils::Sprite name;
		SokuLib::DrawUtils::Sprite wins;
		SokuLib::DrawUtils::Sprite losses;
		SokuLib::DrawUtils::Sprite total;
		SokuLib::DrawUtils::Sprite winratio;
	};

	SokuLib::DrawUtils::Sprite title;
	std::vector<std::shared_ptr<ChrEntry>> _stats;
	SokuLib::SWRFont _defaultFont12;
	unsigned _start = 0;

public:
	StatsMenu();
	void _() override;
	int onProcess() override;
	int onRender() override;
};


#endif //SOKULOBBIES_STATSMENU_HPP
