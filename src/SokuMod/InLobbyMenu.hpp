//
// Created by PinkySmile on 02/10/2022.
//

#ifndef SOKULOBBIES_INLOBBYMENU_HPP
#define SOKULOBBIES_INLOBBYMENU_HPP


#include <mutex>
#include <thread>
#include <Socket.hpp>
#include <SokuLib.hpp>
#include "Connection.hpp"
#include "LobbyData.hpp"
#include "LobbyMenu.hpp"

class InLobbyMenu : public SokuLib::IMenu {
private:
	struct PlayerData {
		SokuLib::DrawUtils::Sprite name;
	};
	struct MessageText {
		SokuLib::DrawUtils::Sprite sprite;
		SokuLib::Vector2i realSize;
		SokuLib::Vector2i pos;
	};
	struct MessageEmote {
		unsigned id;
		unsigned cutRemain;
		SokuLib::Vector2i pos;
		SokuLib::Vector2i offset;
	};
	struct Message {
		std::vector<MessageEmote> emotes;
		std::list<MessageText> text;
		bool farDown = false;
		bool farUp = false;
	};
	struct ArcadeMachine {
		unsigned id;
		SokuLib::Vector2i pos;
		LobbyData::ArcadeAnimation *currentAnim;
		LobbyData::ArcadeSkin &skin;
		std::mutex mutex;
		unsigned skinAnimationCtr = 0;
		unsigned skinAnimation = 0;
		unsigned animationCtr = 0;
		unsigned animation = 0;
		unsigned playerCount = 0;
		bool animIdle = false;

		ArcadeMachine(unsigned id, SokuLib::Vector2i pos, LobbyData::ArcadeAnimation *currentAnim, LobbyData::ArcadeSkin &skin);
		ArcadeMachine(const ArcadeMachine &);
	};

	std::function<void (const std::string &ip, unsigned short port, bool spectate)> onConnectRequest;
	std::function<void (const std::string &msg)> onError;
	std::function<void (const std::string &msg)> onImpMsg;
	std::function<void (int32_t channel, int32_t player, const std::string &msg)> onMsg;
	std::function<void (const Player &)> onPlayerJoin;
	std::function<unsigned short ()> onHostRequest;
	std::function<void (const Lobbies::PacketOlleh &)> onConnect;
	std::function<void (const Player &, uint32_t id)> onArcadeEngage;
	std::function<void (const Player &, uint32_t id)> onArcadeLeave;
	SokuLib::Vector2i _translate{0, 0};
	LobbyMenu *_menu;
	Connection &connection;
	SokuLib::MenuConnect *parent;
	bool _wasConnected = false;
	unsigned _chatTimer = 0;
	unsigned _chatOffset = 0;
	uint8_t _background = 0;
	std::string _music;
	SokuLib::DrawUtils::Sprite _chatSeat;
	SokuLib::DrawUtils::Sprite _loadingText;
	SokuLib::DrawUtils::Sprite _messageBox;
	SokuLib::DrawUtils::Sprite _loadingGear;
	SokuLib::DrawUtils::Sprite _inBattle;
	std::list<Message> _chatMessages;
	std::map<uint32_t, PlayerData> _extraPlayerData;
	std::vector<char> _buffer;
	std::vector<ArcadeMachine> _machines;

	// Chat input box
	SokuLib::DrawUtils::RectangleShape _textCursor;
	SokuLib::DrawUtils::Sprite _textSprite;
	std::mutex _textMutex;
	unsigned _timers[256];
	unsigned _textTimer = 0;
	char _lastPressed = 0;
	int _textCursorPos = 0;
	bool _textChanged = false;
	bool _editingText = false;
	bool _returnPressed = false;

	void _updateMessageSprite(SokuLib::Vector2i pos, unsigned int remaining, SokuLib::Vector2i realSize, SokuLib::DrawUtils::Sprite &sprite, unsigned char alpha);
	void _addMessageToList(unsigned channel, unsigned player, const std::string &msg);
	void _inputBoxUpdate();
	void _initInputBox();
	void _updateTextCursor(int pos);
	void _sendMessage(const std::string &msg);
	std::string _sanitizeInput();
	void _unhook();

public:
	InLobbyMenu(LobbyMenu *menu, SokuLib::MenuConnect *parent, Connection &connection);
	~InLobbyMenu();
	void _() override;
	int onProcess() override;
	int onRender() override;

	void onKeyPressed(int chr);
	void onKeyReleased();

	void updateChat();
	void renderChat();
	bool isInputing();
};

extern InLobbyMenu *activeMenu;


#endif //SOKULOBBIES_INLOBBYMENU_HPP
