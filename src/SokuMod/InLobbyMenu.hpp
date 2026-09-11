//
// Created by PinkySmile on 02/10/2022.
//

#ifndef SOKULOBBIES_INLOBBYMENU_HPP
#define SOKULOBBIES_INLOBBYMENU_HPP


#include <mutex>
#include <thread>
#include <queue>
#include <deque>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <atomic>
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
	struct LazyMessage {
		const unsigned int channel;
		const unsigned player;
		const std::string msg;
		const std::optional<unsigned> colorOverride;
		LazyMessage(unsigned int channel, unsigned player, const std::string &msg, std::optional<unsigned> colorOverride) :
			channel(channel),
			player(player),
			msg(msg),
			colorOverride(colorOverride) {

		}
	};
	struct Message {
		std::vector<MessageEmote> emotes;
		std::list<MessageText> text;
		std::optional<LazyMessage> lazy_message;
		bool preserveScrollAnchor = false;
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
	struct ElevatorMachine {
		unsigned id;
		SokuLib::Vector2i pos;
		LobbyData::ElevatorSkin &skin;
		LobbyData::ElevatorPlacement &links;
		std::mutex mutex;
		unsigned char skinAnimationCtr = 0;
		char skinAnimation = 0;
		char animation = 0;
		unsigned char state = 0;

		ElevatorMachine(unsigned id, SokuLib::Vector2i pos, LobbyData::ElevatorPlacement &links, LobbyData::ElevatorSkin &skin);
		ElevatorMachine(const ElevatorMachine &);
	};
	struct MessageBoxArgs {
		int sound;
		std::wstring text;
		std::wstring title;
		UINT type;
	};

	std::function<void (const std::string &ip, unsigned short port, bool spectate)> onConnectRequest;
	std::function<void (const std::string &msg)> onError;
	std::function<void (const std::string &msg)> onImpMsg;
	std::function<void (int32_t channel, int32_t player, const std::string &msg)> onMsg;
	std::function<void (const Player &)> onPlayerJoin;
	std::function<void (const Player &)> onPlayerLeave;
	std::function<unsigned short ()> onHostRequest;
	std::function<void (const Lobbies::PacketOlleh &)> onConnect;
	std::function<void (const Player &, uint32_t id)> onArcadeEngage;
	std::function<void (const Player &, uint32_t id)> onArcadeLeave;
	SokuLib::Vector2i _translate{0, 0};
	SokuLib::Vector2i _translateStep{0, 0};
	SokuLib::Vector2i _translateTarget{0, 0};
	unsigned char _translateAnimation = 0;
	std::string _roomName;
	std::string _lobbyIdentity;
	LobbyMenu *_menu;
	ArcadeMachine *_currentMachine = nullptr;
	std::chrono::steady_clock::time_point _nextArcadeActionAt{};
	std::chrono::steady_clock::time_point _nextTeleportAt{};
	ElevatorMachine *_currentElevator = nullptr;
	SokuLib::Vector2u _camera;
	float _zoom = 1;
	std::shared_ptr<Connection> _connection;
	SokuLib::MenuConnect *_parent;
	bool _wasConnected = false;
	bool _disconnected = false;
	bool _serverBlocklistSupported = false;
	std::vector<Player> _playersCopy;
	std::unordered_map<uint32_t, const Player *> _playersById;
	std::unordered_set<uint32_t> _playersInsideElevator;
	unsigned _chatTimer = 0;
	unsigned _chatOffset = 0;
	std::atomic_bool _chatScrolledAwayFromBottom{false};
	std::atomic_bool _battleOpponentChatPopup{false};
	uint8_t _background = 0;
	std::string _music;
	SokuLib::DrawUtils::Sprite _chatSeat;
	SokuLib::DrawUtils::Sprite _loadingText;
	SokuLib::DrawUtils::Sprite _messageBox;
	SokuLib::DrawUtils::Sprite _loadingGear;
	SokuLib::DrawUtils::Sprite _battleStatus[3];
	std::list<Message> _chatMessages;
	std::mutex _chatMessagesMutex;
	struct RecentOpponent {
		uint32_t playerId;
		std::string playerName;
		uint32_t machineId;
		bool matchActive = true;
		std::chrono::steady_clock::time_point expiresAt;
	};
	std::optional<RecentOpponent> _recentOpponent;
	std::mutex _recentOpponentMutex;
	struct PlayerEmoteBubble {
		unsigned emoteId;
		std::chrono::steady_clock::time_point startedAt;
	};
	std::map<uint32_t, PlayerEmoteBubble> _playerEmoteBubbles;
	std::mutex _playerEmoteBubblesMutex;
	struct PlayerTextBubble {
		SokuLib::DrawUtils::Sprite text[2];
		SokuLib::Vector2i textSize[2];
		unsigned lineCount = 0;
		bool privateMessage = false;
		std::chrono::steady_clock::time_point startedAt;
	};
	std::map<uint32_t, PlayerTextBubble> _playerTextBubbles;
	struct PendingTextBubble {
		std::string message;
		bool privateMessage;
		std::chrono::steady_clock::time_point receivedAt;
	};
	std::map<uint32_t, PlayerData> _extraPlayerData;
	std::map<uint32_t, std::string> _pendingPlayerNames;
	std::queue<uint32_t> _pendingPlayerNameOrder;
	std::map<uint32_t, PendingTextBubble> _pendingTextBubbles;
	std::queue<uint32_t> _pendingTextBubbleOrder;
	std::set<uint32_t> _pendingPlayerRemovals;
	std::mutex _pendingTextureWorkMutex;
	bool _textBubblesOutsideLobby = false;
	std::wstring _buffer;
	std::vector<ArcadeMachine> _machines;
	std::vector<ElevatorMachine> _elevators;
	std::unique_ptr<class SmallHostlist> _hostlist;
	std::vector<std::unique_ptr<class SmallHostlist>> _retiredHostlists;
	std::vector<uint32_t> _insideElevator;
	std::thread _connectThread;
	unsigned char _elevatorCtr = 0;
	bool _elevatorOut = false;
	std::queue<MessageBoxArgs> _messageBoxQueue;
	std::mutex _messageBoxQueueMutex;
	std::thread _messageBoxThread;

	// Chat input box
	SokuLib::SWRFont _chatFont;
	SokuLib::SWRFont _textBubbleFont;
	SokuLib::DrawUtils::RectangleShape _textCursor;
	SokuLib::DrawUtils::Sprite _textSprite[2];
	std::mutex _textMutex;
	std::thread _hostThread;
	unsigned _textTimer = 0;
	unsigned _lastPressed = 0;
	unsigned _currentPlatform = 0;
	int _textCursorPosIndex = 0;
	int _textCursorPosSize = 0;
	int _selectionStart = -1;
	int _selectionEnd = -1;
	bool _editingText = false;
	bool _battleChatActive = false;
	bool _battleChatHolding = false;
	bool _battleChatAwaitRelease = false;
	std::chrono::steady_clock::time_point _battleChatHoldStarted{};
	std::chrono::steady_clock::time_point _battleChatHintUntil{};
	std::wstring _battleChatHintText;
	SokuLib::DrawUtils::Sprite _battleChatHintSprite;
	bool _emotePickerOpen = false;
	bool _emotePickerNavigationHeld = false;
	unsigned _emotePickerSelection = 0;
	std::vector<unsigned> _emotePickerOrder;
	bool _quickMessageMenuOpen = false;
	enum class EscapeOwner {
		NATIVE_GAME,
		LOBBY,
		MOD_UI,
	};
	struct HotkeyEvent {
		unsigned key;
		bool pressed;
		bool mapped;
		uint64_t escapeGeneration;
		EscapeOwner escapeOwner;
		SokuLib::Scene escapeScene;
	};
	std::deque<HotkeyEvent> _hotkeyEvents;
	bool _keyboardEscapeDown = false;
	bool _mappedEscapeDown = false;
	uint64_t _nextEscapeGeneration = 0;
	uint64_t _keyboardEscapeGeneration = 0;
	uint64_t _mappedEscapeGeneration = 0;
	uint64_t _consumedKeyboardEscapeGeneration = 0;
	uint64_t _nativeKeyboardEscapeGeneration = 0;
	EscapeOwner _keyboardEscapeOwner = EscapeOwner::NATIVE_GAME;
	SokuLib::Scene _keyboardEscapeScene = SokuLib::SCENE_TITLE;
	bool _lobbyExitRequested = false;
	bool _hostlistExitRequested = false;
	SokuLib::DrawUtils::Sprite _quickMessageSprites[9];
	SokuLib::Vector2i _quickMessageTextSizes[9];
	SokuLib::DrawUtils::Sprite _chatPopupModeSprites[3];
	unsigned _chatPopupModeTimer = 0;
	struct PrivateMessageCompletion {
		uint32_t playerId;
		std::wstring playerName;
		bool recentOpponent = false;
		SokuLib::DrawUtils::Sprite label;
	};
	std::vector<PrivateMessageCompletion> _privateMessageCompletions;
	unsigned _privateMessageCompletionIndex = 0;
	unsigned _privateMessageCompletionScroll = 0;
	unsigned _privateMessageCompletionTimer = 0;
	std::map<unsigned, int> _textSize;

	void _updateMessageSprite(SokuLib::Vector2i pos, unsigned int remaining, SokuLib::Vector2i realSize, SokuLib::DrawUtils::Sprite &sprite, unsigned char alpha);
	void _addMessageToList(unsigned channel, unsigned player, const std::string &msg, std::optional<unsigned> colorOverride = std::nullopt, bool autoPopup = true);
	void _updateRecentOpponent();
	void _restoreRecentOpponent();
	void _saveRecentOpponent(bool leavingLobby = false);
	void _showEmoteBubble(unsigned player, const std::string &msg);
	void _renderEmoteBubbles(const std::unordered_map<uint32_t, const Player *> &playersById);
	void _showTextBubble(unsigned player, const std::string &msg, bool privateMessage = false);
	void _buildTextBubble(unsigned player, const std::string &msg, bool privateMessage, std::chrono::steady_clock::time_point receivedAt);
	void _clearTextBubbles();
	void _renderTextBubbles(const std::unordered_map<uint32_t, const Player *> &playersById);
	void _queuePlayerName(unsigned player, const std::string &name);
	void _queuePlayerRemoval(unsigned player);
	void _buildPlayerName(unsigned player, const std::string &name);
	void _processPendingTextureWork();
	void _logChatToFile(unsigned player, const std::string &msg);
	void _inputBoxUpdate(bool blockChatInput);
	void _processHotkeyEvents();
	EscapeOwner _classifyEscapeOwner() const;
	void _setEscapeSource(bool &source, uint64_t &generation, bool down, bool mapped);
	void _consumeEscape(const HotkeyEvent &event);
	void _updateEmotePicker();
	void _renderEmotePicker();
	void _initEmotePickerOrder();
	void _updateQuickMessageMenu();
	void _renderQuickMessageMenu();
	void _initQuickMessageSprites();
	void _initChatPopupModeSprites();
	void _renderChatPopupMode();
	void _renderBattleChatHint();
	void _sendEmote(const LobbyData::Emote &emote);
	void _normalizeEmotePickerSelection();
	void _initInputBox();
	void _clearSelection();
	bool _hasSelection() const;
	bool _deleteSelection();
	std::wstring _getSelectedText() const;
	void _copySelectionToClipboard();
	void _pasteFromClipboard();
	void _completePrivateMessageRecipient();
	bool _getPlayerCompletionTarget(size_t &targetStart, size_t &targetEnd, bool &appendSpace) const;
	void _refreshPrivateMessageCompletions();
	void _applyPrivateMessageCompletion();
	void _renderPrivateMessageCompletions();
	void _updateTextCursor(int pos);
	bool _handleLocalTeleport(const std::wstring &msg);
	bool _handleLocalBlock(const std::wstring &msg);
	bool _handleLocalHelp(const std::wstring &msg);
	void _sendMessage(const std::wstring &msg);
	void _probeBlocklistServer();
	void _syncBlocklistToServer();
	void _requestRecentOpponentIp();
	void _unhook();
	void _renderMachineOverlay();
	void _startHosting();
	void _updateCompositionSprite();
	void _openMessageBox(int sound, const std::string &text, const std::string &title, UINT type);
	int _getTextSize(unsigned i);

public:
	char textChanged = 0;
	std::mutex keyTimersMutex;
	bool keysPressed[256] = {false};
	HIMC immCtx = nullptr;
	std::wstring immComposition;
	bool hasDeadkey = false;
	int compositionCursor = 0;
	int keyBufferUsed = 0;
	wchar_t keyBuffer[2] = {0, 0};
	bool wineWorkaroundNeeded = false;

	InLobbyMenu(LobbyMenu *menu, SokuLib::MenuConnect *parent, std::shared_ptr<Connection> &connection);
	~InLobbyMenu();
	void _() override;
	int onProcess() override;
	int onRender() override;

	void onKeyPressed(unsigned chr);
	void onKeyReleased();
	void addString(wchar_t *str, size_t size);

	void updateChat(bool inGame);
	void renderChat();
	bool isInputing();
	bool isEmotePickerOpen() const;
	void routePendingHotkeys();
	void onWindowKeyEvent(unsigned key, bool pressed, bool repeated);
	void onInputFocusLost();
	void setMappedEscapeDown(bool down);
	bool filterNativeEscape(bool pressed);
};

extern InLobbyMenu *activeMenu;


#endif //SOKULOBBIES_INLOBBYMENU_HPP
