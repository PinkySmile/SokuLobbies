#ifndef SOKULOBBIES_OPTIONSMENU_HPP
#define SOKULOBBIES_OPTIONSMENU_HPP

#include <functional>
#include <string>
#include <vector>
#include <SokuLib.hpp>

class LobbyMenu;

class OptionsMenu : public SokuLib::IMenu {
private:
	struct Choice {
		std::string label;
		unsigned value;
	};
	struct Option {
		std::string name;
		std::vector<Choice> choices;
		std::function<unsigned ()> get;
		std::function<bool (unsigned)> apply;
		std::function<std::string (unsigned)> formatCustom;
		std::function<void ()> confirm;
		unsigned index = 0;
		SokuLib::DrawUtils::Sprite labelSprite;
		SokuLib::DrawUtils::Sprite valueSprite;
	};

	LobbyMenu *_parent;
	unsigned _cursor = 0;
	unsigned _statusTimer = 0;
	std::vector<Option> _options;
	SokuLib::DrawUtils::RectangleShape _background;
	SokuLib::DrawUtils::Sprite _title;
	SokuLib::DrawUtils::Sprite _hint;
	SokuLib::DrawUtils::Sprite _status;
	bool _editingMessages = false;
	bool _capturingChatKey = false;
	bool _chatKeyCaptureArmed = false;
	unsigned _messageCursor = 0;
	SokuLib::DrawUtils::Sprite _messagesTitle;
	SokuLib::DrawUtils::Sprite _messagesHint;
	std::vector<SokuLib::DrawUtils::Sprite> _messageLabels;
	std::vector<SokuLib::DrawUtils::Sprite> _messageValues;

	void _addOption(Option option);
	void _refreshValue(Option &option);
	void _applyValue(Option &option, int delta);
	void _showSaveError();
	void _showStatus(const wchar_t *message, const SokuLib::Color &color);
	void _initMessageEditor();
	void _refreshMessageValue(unsigned index);
	void _openMessageEditor(unsigned index);
	void _updateMessageEditor();
	void _renderMessageEditor();
	void _updateChatKeyCapture();
	void _refreshLanguage();

public:
	explicit OptionsMenu(LobbyMenu *parent);
	~OptionsMenu() override;
	void _() override;
	int onProcess() override;
	int onRender() override;
};

#endif
