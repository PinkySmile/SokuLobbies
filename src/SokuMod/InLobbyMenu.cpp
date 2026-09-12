//
// Created by PinkySmile on 02/10/2022.
//

#define _USE_MATH_DEFINES
#include <dinput.h>
#include <filesystem>
#include <random>
#include <cstdlib>
#include <cctype>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cwctype>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include "InLobbyMenu.hpp"
#include "LobbyData.hpp"
#include "data.hpp"
#include "SmallHostlist.hpp"
#include "encodingConverter.hpp"
#include "createUTFTexture.hpp"
#include "integration.hpp"
#include "getPublicIp.hpp"
#include "ipv6map_extern.hpp"
#include "Blocklist.hpp"

#define CHAT_CHARACTER_LIMIT 512
#define BOX_TEXTURE_SIZE {0x2000, 30}
#define TEXTURE_MAX_SIZE 344
#define CURSOR_ENDX 637
#define CURSOR_STARTX 293
#define CURSOR_STARTY 184
#define MAX_LINE_SIZE 342
#define SCROLL_AMOUNT 20
#define CHAT_FONT_HEIGHT 14
#define ELEVEATOR_CTR_DIVIDER 90.f
#define EMOTE_PICKER_COLUMNS 8
#define EMOTE_PICKER_ROWS 4
#define EMOTE_PICKER_PAGE_SIZE (EMOTE_PICKER_COLUMNS * EMOTE_PICKER_ROWS)

static constexpr auto TEXT_BUBBLE_LIFETIME = std::chrono::milliseconds(10000);

#define DEBUG_COLOR 0x404040

struct CDesignSprite {
	void *vftable; // =008576ac
	float UNKNOWN_1[2];
	float x;
	float y;
	uint8_t active;
	uint8_t UNKNOWN_2[3];
	int32_t UNKNOWN_3;
};

auto &messageBox = *(CDesignSprite**)0x89a390;
InLobbyMenu *activeMenu = nullptr;
static WNDPROC Original_WndProc = nullptr;
static std::mutex ptrMutex;
static std::mt19937 random;

struct RecentOpponentSession {
	uint32_t playerId;
	std::string playerName;
	uint32_t machineId;
	bool matchActive;
	std::chrono::steady_clock::time_point expiresAt;
	std::string lobbyIdentity;
};

static std::optional<RecentOpponentSession> recentOpponentSession;
static std::mutex recentOpponentSessionMutex;

struct RecentPlayerSelectionsSession {
	std::deque<std::pair<uint32_t, std::string>> selections;
	std::string lobbyIdentity;
};

static std::optional<RecentPlayerSelectionsSession> recentPlayerSelectionsSession;
static std::mutex recentPlayerSelectionsSessionMutex;

static void replaceAll(std::string &value, const std::string &from, const std::string &to)
{
	if (from.empty())
		return;
	for (size_t pos = 0; (pos = value.find(from, pos)) != std::string::npos; pos += to.size())
		value.replace(pos, from.size(), to);
}

static std::string localizeKickReason(const std::string &reason)
{
	if (reason == "Timed out")
		return "连接超时。";
	if (reason == "Socket error")
		return "网络连接错误。";
	if (reason == "Server is full")
		return "服务器人数已满。";
	if (reason == "Server closed")
		return "服务器已关闭。";
	if (reason == "Incorrect password")
		return "密码错误。";
	if (reason == "Outdated server!")
		return "服务器版本过旧。";
	if (reason == "You are running an old version of SokuLobbies! Please update your mod and try again.")
		return "你的 SokuLobbies 版本过旧，请更新模组后重试。";
	constexpr char inactivePrefix[] = "Kicked for being inactive for ";
	constexpr char inactiveSuffix[] = " minutes.";
	if (
		reason.size() > sizeof(inactivePrefix) - 1 + sizeof(inactiveSuffix) - 1 &&
		reason.compare(0, sizeof(inactivePrefix) - 1, inactivePrefix) == 0 &&
		reason.compare(reason.size() - (sizeof(inactiveSuffix) - 1), sizeof(inactiveSuffix) - 1, inactiveSuffix) == 0
	)
		return "因持续 " + reason.substr(
			sizeof(inactivePrefix) - 1,
			reason.size() - (sizeof(inactivePrefix) - 1) - (sizeof(inactiveSuffix) - 1)
		) + " 分钟不活跃而被踢出。";
	constexpr char bannedPrefix[] = "You are banned from this server: ";
	if (reason.compare(0, sizeof(bannedPrefix) - 1, bannedPrefix) == 0)
		return "你已被此服务器封禁：" + reason.substr(sizeof(bannedPrefix) - 1);
	constexpr char invalidNamePrefix[] = "Invalid name (contains ";
	if (reason.compare(0, sizeof(invalidNamePrefix) - 1, invalidNamePrefix) == 0)
		return "玩家名称无效（包含 " + reason.substr(sizeof(invalidNamePrefix) - 1);
	constexpr char protocolPrefix[] = "Protocol error: ";
	if (reason.compare(0, sizeof(protocolPrefix) - 1, protocolPrefix) == 0)
		return "协议错误：" + reason.substr(sizeof(protocolPrefix) - 1);
	constexpr char internalPrefix[] = "Internal server error: ";
	if (reason.compare(0, sizeof(internalPrefix) - 1, internalPrefix) == 0)
		return "服务器内部错误：" + reason.substr(sizeof(internalPrefix) - 1);
	return reason;
}

static std::string localizeLobbyMessage(const std::string &message)
{
	// Older block-enabled servers use this compatibility response specifically
	// when a player is rejected by an opponent's blacklist.
	if (
		message == "Unable to join this arcade machine." ||
		message == "无法加入该对战机。\nUnable to join this arcade machine."
	)
		return chineseLanguage ? "你已被该玩家拉黑。" : "You have been blacklisted by this player.";
	if (!chineseLanguage)
		return message;
	constexpr char publicIpErrorPrefix[] = "Failed to get public IP";
	if (message.compare(0, sizeof(publicIpErrorPrefix) - 1, publicIpErrorPrefix) == 0)
		return "无法获取IP地址，请使用swarm的IP用于大厅";
	if (message == "Connection closed")
		return "与服务器的连接已关闭。";
	if (message == "You have been blacklisted by this player.")
		return "你已被该玩家拉黑。";
	if (message == "This player is in your blacklist.")
		return "该玩家已在你的黑名单中。";
	constexpr char joinedSuffix[] = " has joined the lobby.";
	constexpr char disconnectedSuffix[] = " has disconnected";
	constexpr char kickedMarker[] = " has been kicked: ";
	if (message.size() >= sizeof(joinedSuffix) - 1 && message.compare(message.size() - (sizeof(joinedSuffix) - 1), sizeof(joinedSuffix) - 1, joinedSuffix) == 0)
		return message.substr(0, message.size() - (sizeof(joinedSuffix) - 1)) + " 已进入大厅。";
	if (message.size() >= sizeof(disconnectedSuffix) - 1 && message.compare(message.size() - (sizeof(disconnectedSuffix) - 1), sizeof(disconnectedSuffix) - 1, disconnectedSuffix) == 0)
		return message.substr(0, message.size() - (sizeof(disconnectedSuffix) - 1)) + " 已断开连接。";
	auto kicked = message.find(kickedMarker);
	if (kicked != std::string::npos)
		return message.substr(0, kicked) + " 已被踢出：" + localizeKickReason(message.substr(kicked + sizeof(kickedMarker) - 1));
	constexpr char joinedArcadePrefix[] = "You joined arcade ";
	if (message.compare(0, sizeof(joinedArcadePrefix) - 1, joinedArcadePrefix) == 0)
		return "你已进入对战机 " + message.substr(sizeof(joinedArcadePrefix) - 1);
	constexpr char leftArcadePrefix[] = "You left arcade ";
	if (message.compare(0, sizeof(leftArcadePrefix) - 1, leftArcadePrefix) == 0)
		return "你已离开对战机 " + message.substr(sizeof(leftArcadePrefix) - 1);
	constexpr char kickedPrefix[] = "Kicked: ";
	if (message.compare(0, sizeof(kickedPrefix) - 1, kickedPrefix) == 0)
		return "已被踢出：" + localizeKickReason(message.substr(sizeof(kickedPrefix) - 1));

	std::string localized = message;
	replaceAll(localized, "Cannot join arcade ", "无法进入对战机 ");
	replaceAll(localized, " because the players here are using a different netplay version than you.", "：其中玩家使用的联机版本与你不同。");
	replaceAll(localized, " because your Soku2 version mismatches.", "：双方的 Soku2 版本不一致。");
	replaceAll(localized, "Your version: ", "你的版本：");
	replaceAll(localized, "Their version: ", "对方版本：");
	replaceAll(localized, "Unknown version string.", "未知版本。");
	replaceAll(localized, " without SWR.", "，未启用 SWR。");
	replaceAll(localized, " with SWR.", "，启用 SWR。");
	replaceAll(localized, "Not found.", "未检测到。");
	return localized;
}

static std::wstring messageBoxText(const std::string &text)
{
	try {
		return convertEncoding<char, wchar_t, UTF8Decode, UTF16Encode>(text);
	} catch (...) {
		if (text.empty())
			return {};
		int size = MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
		if (size <= 0)
			return std::wstring(text.begin(), text.end());
		std::wstring result(size, L'\0');
		MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
		return result;
	}
}

static bool isRectVisible(int x, int y, int width, int height, int margin = 0)
{
	return x + width >= -margin && y + height >= -margin && x <= 640 + margin && y <= 480 + margin;
}

static bool naturalAliasLess(const std::string &left, const std::string &right)
{
	size_t leftPos = 0;
	size_t rightPos = 0;

	while (leftPos < left.size() && rightPos < right.size()) {
		auto leftChar = static_cast<unsigned char>(left[leftPos]);
		auto rightChar = static_cast<unsigned char>(right[rightPos]);
		if (std::isdigit(leftChar) && std::isdigit(rightChar)) {
			auto leftEnd = leftPos;
			auto rightEnd = rightPos;

			while (leftEnd < left.size() && std::isdigit(static_cast<unsigned char>(left[leftEnd])))
				leftEnd++;
			while (rightEnd < right.size() && std::isdigit(static_cast<unsigned char>(right[rightEnd])))
				rightEnd++;
			auto leftNumber = leftPos;
			auto rightNumber = rightPos;
			while (leftNumber < leftEnd && left[leftNumber] == '0')
				leftNumber++;
			while (rightNumber < rightEnd && right[rightNumber] == '0')
				rightNumber++;
			auto leftDigits = leftEnd - leftNumber;
			auto rightDigits = rightEnd - rightNumber;

			if (leftDigits != rightDigits)
				return leftDigits < rightDigits;
			for (size_t i = 0; i < leftDigits; i++)
				if (left[leftNumber + i] != right[rightNumber + i])
					return left[leftNumber + i] < right[rightNumber + i];
			if (leftEnd - leftPos != rightEnd - rightPos)
				return leftEnd - leftPos < rightEnd - rightPos;
			leftPos = leftEnd;
			rightPos = rightEnd;
			continue;
		}
		auto foldedLeft = static_cast<unsigned char>(std::tolower(leftChar));
		auto foldedRight = static_cast<unsigned char>(std::tolower(rightChar));

		if (foldedLeft != foldedRight)
			return foldedLeft < foldedRight;
		leftPos++;
		rightPos++;
	}
	return left.size() < right.size();
}

static LRESULT __stdcall Hooked_WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (
		uMsg == WM_IME_NOTIFY &&
		wParam == IMN_PRIVATE && (
			lParam == 1 || lParam == 2 ||
			lParam == 16|| lParam == 17||
			lParam == 26|| lParam == 27|| lParam == 28
		)
	)
		return 0;

	unsigned eventList[] = {
		WM_IME_STARTCOMPOSITION,
		WM_IME_ENDCOMPOSITION,
		WM_INPUTLANGCHANGE,
		WM_IME_COMPOSITION,
		WM_IME_NOTIFY,
		WM_KEYDOWN,
		WM_KEYUP,
		WM_KILLFOCUS
	};
	const size_t size = sizeof(eventList) / sizeof(*eventList);

	if (std::find(eventList, eventList + size, uMsg) == eventList + size)
		return CallWindowProc(Original_WndProc, hWnd, uMsg, wParam, lParam);
	if (uMsg == WM_IME_NOTIFY) {
		if (wParam == IMN_SETCONVERSIONMODE) {
			ptrMutex.lock();
			if (!activeMenu) {
				ptrMutex.unlock();
				return CallWindowProc(Original_WndProc, hWnd, uMsg, wParam, lParam);
			}
			if (activeMenu->immCtx)
				ImmReleaseContext(hWnd, activeMenu->immCtx);
			activeMenu->immCtx = nullptr;
			activeMenu->immComposition.clear();
			activeMenu->compositionCursor = 0;
			ptrMutex.unlock();
		} else if (wineVersion && (wParam == 0xf || wParam == 0x10))
			//#define IMN_WINE_SET_OPEN_STATUS  0x000f
			//#define IMN_WINE_SET_COMP_STRING  0x0010
			// On wine>=8.9 IMN_WINE_SET_COMP_STRING should be processed by DefWindowProc,
			// or all other WM_IME_* messages will not be got.
			// But th123 doesn't call DefWindowProc when processing WM_IME_NOTIFY.
			return DefWindowProc(hWnd, uMsg, wParam, lParam);

		return CallWindowProc(Original_WndProc, hWnd, uMsg, wParam, lParam);
	}
	ptrMutex.lock();
	if (!activeMenu) {
		ptrMutex.unlock();
		return CallWindowProc(Original_WndProc, hWnd, uMsg, wParam, lParam);
	}
	if (uMsg == WM_KILLFOCUS) {
		activeMenu->onInputFocusLost();
	} else if (uMsg == WM_INPUTLANGCHANGE) {
		activeMenu->immComposition.clear();
	} else if (uMsg == WM_IME_STARTCOMPOSITION) {
		activeMenu->immCtx = ImmGetContext(SokuLib::window);
		// This disables the windows builtin IME window.
		// For now we keep it because part of its features are not properly supported.
		//ptrMutex.unlock();
		//return 0;
	} else if (uMsg == WM_IME_ENDCOMPOSITION) {
		MSG compositionMsg;

		if (
			::PeekMessage(&compositionMsg, hWnd, WM_IME_STARTCOMPOSITION, WM_IME_COMPOSITION, PM_NOREMOVE) &&
			compositionMsg.message == WM_IME_COMPOSITION &&
			(compositionMsg.lParam & GCS_RESULTSTR)
		) {
			ptrMutex.unlock();
			return CallWindowProc(Original_WndProc, hWnd, uMsg, wParam, lParam);
		}
		if (activeMenu->immCtx)
			ImmReleaseContext(hWnd, activeMenu->immCtx);
		activeMenu->immCtx = nullptr;
		activeMenu->immComposition.clear();
		activeMenu->compositionCursor = 0;
		// This disables the windows builtin IME window.
		// For now we keep it because part of its features are not properly supported.
		//ptrMutex.unlock();
		//return 0;
	} else if (uMsg == WM_IME_COMPOSITION) {
		// Wine (>=8.9, <=8.14) doesn't send WM_IME_STARTCOMPOSITION and WM_IME_ENDCOMPOSITION,
		// so here we call ImmGetContext as a workaround if necessary.
		// Moreover, ImmReleaseContext on Wine does nothing but returns true.
		// As a result, the possible absence of WM_IME_ENDCOMPOSITION on Wine doesn't matter.
		if (wineVersion && activeMenu->immCtx == nullptr)
			activeMenu->wineWorkaroundNeeded = true;
		if (activeMenu->wineWorkaroundNeeded)
			activeMenu->immCtx = ImmGetContext(hWnd);
		if (!activeMenu->immCtx)
			activeMenu->immCtx = ImmGetContext(hWnd);
		if (!activeMenu->immCtx) {
			ptrMutex.unlock();
			return CallWindowProc(Original_WndProc, hWnd, uMsg, wParam, lParam);
		}

		activeMenu->onKeyReleased();
		if (lParam & GCS_RESULTSTR) {
			LONG required = ImmGetCompositionStringW(activeMenu->immCtx, GCS_RESULTSTR, nullptr, 0);

			if (required > 0 && required % sizeof(wchar_t) == 0) {
				std::vector<wchar_t> result(required / sizeof(wchar_t));
				LONG copied = ImmGetCompositionStringW(activeMenu->immCtx, GCS_RESULTSTR, result.data(), required);

				if (copied > 0 && copied <= required && copied % sizeof(wchar_t) == 0)
					activeMenu->addString(result.data(), copied / sizeof(wchar_t));
			}
			activeMenu->immComposition.clear();
			activeMenu->textChanged |= 3;
		}
		//required = ImmGetCompositionStringW(activeMenu->immCtx, GCS_COMPATTR, nullptr, 0);
		//printf("GCS_COMPATTR: Required %li\n", required);
		if (lParam & GCS_COMPSTR) {
			LONG required = ImmGetCompositionStringW(activeMenu->immCtx, GCS_COMPSTR, nullptr, 0);

			activeMenu->immComposition.clear();
			if (required > 0 && required % sizeof(wchar_t) == 0) {
				activeMenu->immComposition.resize(required / sizeof(wchar_t));
				LONG copied = ImmGetCompositionStringW(
					activeMenu->immCtx,
					GCS_COMPSTR,
					activeMenu->immComposition.data(),
					required
				);
				if (copied < 0 || copied > required || copied % sizeof(wchar_t) != 0)
					activeMenu->immComposition.clear();
				else
					activeMenu->immComposition.resize(copied / sizeof(wchar_t));
			}
			activeMenu->textChanged |= 2;
		}
		if (lParam & GCS_CURSORPOS) {
			LONG cursor = ImmGetCompositionStringW(
				activeMenu->immCtx,
				GCS_CURSORPOS,
				nullptr,
				0
			);
			activeMenu->compositionCursor = cursor < 0 ? 0 : cursor;
			activeMenu->textChanged |= 4;
		}
	} else if (uMsg == WM_KEYDOWN) {
		activeMenu->onWindowKeyEvent(
			static_cast<unsigned>(wParam),
			true,
			(lParam & (1u << 30)) != 0
		);
		BYTE keyboardState[256];
		GetKeyboardState(keyboardState);
		activeMenu->keyBufferUsed = ToUnicode(
			static_cast<UINT>(wParam),
			static_cast<UINT>((lParam >> 16) & 0xFF),
			keyboardState,
			activeMenu->keyBuffer,
			2,
			0
		);

		if (activeMenu->keyBufferUsed > 0) {
			auto decoded = UTF16Decode(std::wstring(activeMenu->keyBuffer, activeMenu->keyBuffer + activeMenu->keyBufferUsed));

			if (!decoded.empty())
				activeMenu->onKeyPressed(decoded[0]);
		}
		if (
			wParam < sizeof(activeMenu->keysPressed) / sizeof(*activeMenu->keysPressed) &&
			wParam != VK_ESCAPE && wParam != VK_F1 && wParam != VK_F2
		) {
			std::lock_guard<std::mutex> lock_(activeMenu->keyTimersMutex);
			activeMenu->keysPressed[wParam] = true;
		}
	} else if (uMsg == WM_KEYUP) {
		activeMenu->onWindowKeyEvent(static_cast<unsigned>(wParam), false, false);
		activeMenu->onKeyReleased();
	}

	ptrMutex.unlock();
	return CallWindowProc(Original_WndProc, hWnd, uMsg, wParam, lParam);
}

bool isNumber(const std::string &str)
{
	for (auto c : str)
		if (!std::isdigit(c))
			return false;
	return true;
}

std::vector<std::string> split(const std::string &str, char delim)
{
	auto i = 0;
	std::vector<std::string> list;
	auto pos = str.find(delim);

	while (pos != std::string::npos) {
		list.push_back(str.substr(i, pos - i));
		i = ++pos;
		pos = str.find(delim, pos);
	}
	list.push_back(str.substr(i, str.length()));
	return list;
}

bool checkIp(const std::string &ip)
{
	std::vector<std::string> list = split(ip, '.');

	if (list.size() != 4)
		return false;
	for (const auto &str : list)
		if (!isNumber(str) || stoi(str) > 255 || stoi(str) < 0)
			return false;
	return true;
}

void InLobbyMenu::_openMessageBox(int sound, const std::string &text, const std::string &title, UINT type)
{
	auto wideText = messageBoxText(text);
	auto wideTitle = messageBoxText(title);
	if (SokuLib::sceneId == SokuLib::SCENE_BATTLECL || SokuLib::sceneId == SokuLib::SCENE_BATTLESV) {
		std::lock_guard<std::mutex> messageBoxMutexGuard(this->_messageBoxQueueMutex);
		this->_messageBoxQueue.push(MessageBoxArgs{sound, std::move(wideText), std::move(wideTitle), type});
	}
	else {
		playSound(sound);
		MessageBoxW(SokuLib::window, wideText.c_str(), wideTitle.c_str(), type);
	}
}

InLobbyMenu::InLobbyMenu(LobbyMenu *menu, SokuLib::MenuConnect *parent, std::shared_ptr<Connection> &connection) :
	_connection(connection),
	_parent(parent),
	_menu(menu)
{
	SokuLib::FontDescription desc;
	bool hasEnglishPatch = (*(int *)0x411c64 == 1);

	desc.r1 = 255;
	desc.r2 = 255;
	desc.g1 = 255;
	desc.g2 = 255;
	desc.b1 = 255;
	desc.b2 = 255;
	desc.height = CHAT_FONT_HEIGHT + hasEnglishPatch * 2;
	desc.weight = FW_NORMAL;
	desc.italic = 0;
	desc.shadow = 1;
	desc.bufferSize = 1000000;
	desc.charSpaceX = 0;
	desc.charSpaceY = hasEnglishPatch * -2;
	desc.offsetX = 0;
	desc.offsetY = 0;
	desc.useOffset = 0;
	strcpy(desc.faceName, "Tahoma");
	desc.weight = FW_REGULAR;


	this->_chatFont.create();
	this->_chatFont.setIndirect(desc);
	desc.height = 16 + hasEnglishPatch * 2;
	desc.weight = FW_MEDIUM;
	desc.shadow = 0;
	this->_textBubbleFont.create();
	this->_textBubbleFont.setIndirect(desc);
	this->_initEmotePickerOrder();
	this->_initQuickMessageSprites();
	this->_initChatPopupModeSprites();

	for (int i = 0; i < 3; i++) {
		const char *paths[3] = {
			"assets/lobby/waiting.png",
			"assets/lobby/fighting.png",
			"assets/lobby/watching.png",
		};

		this->_battleStatus[i].texture.loadFromFile((std::filesystem::path(profileFolderPath) / paths[i]).string().c_str());
		this->_battleStatus[i].setSize({
			this->_battleStatus[i].texture.getSize().x,
			this->_battleStatus[i].texture.getSize().y
		});
		this->_battleStatus[i].rect.width = this->_battleStatus[i].texture.getSize().x;
		this->_battleStatus[i].rect.height = this->_battleStatus[i].texture.getSize().y;
		this->_battleStatus[i].setPosition({174, 218});
	}

	this->_chatSeat.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/lobby/chat_seat.png").string().c_str());
	this->_chatSeat.setSize({
		this->_chatSeat.texture.getSize().x,
		this->_chatSeat.texture.getSize().y
	});
	this->_chatSeat.rect.width = this->_chatSeat.texture.getSize().x;
	this->_chatSeat.rect.height = this->_chatSeat.texture.getSize().y;
	this->_chatSeat.setPosition({290, 0});
	this->_chatSeat.tint = SokuLib::Color{0xFF, 0xFF, 0xFF, 0};

	this->_loadingText.texture.createFromText("Joining Lobby...", lobbyData->getFont(16), {300, 74});
	this->_loadingText.setSize({
		this->_loadingText.texture.getSize().x,
		this->_loadingText.texture.getSize().y
	});
	this->_loadingText.rect.width = this->_loadingText.texture.getSize().x;
	this->_loadingText.rect.height = this->_loadingText.texture.getSize().y;
	this->_loadingText.setPosition({174, 218});

	this->_messageBox.texture.loadFromGame("data/menu/21_Base.cv2");
	this->_messageBox.setSize({
		this->_messageBox.texture.getSize().x,
		this->_messageBox.texture.getSize().y
	});
	this->_messageBox.rect.width = this->_messageBox.texture.getSize().x;
	this->_messageBox.rect.height = this->_messageBox.texture.getSize().y;
	this->_messageBox.setPosition({155, 203});

	this->_loadingGear.texture.loadFromGame("data/scene/logo/gear.bmp");
	this->_loadingGear.setSize({
		this->_loadingGear.texture.getSize().x,
		this->_loadingGear.texture.getSize().y
	});
	this->_loadingGear.rect.width = this->_loadingGear.texture.getSize().x;
	this->_loadingGear.rect.height = this->_loadingGear.texture.getSize().y;

	this->_textCursor.setSize({1, 14});
	this->_textCursor.setPosition({CURSOR_STARTX, CURSOR_STARTY});
	this->_textCursor.setFillColor(SokuLib::Color::White);
	this->_textCursor.setBorderColor(SokuLib::Color::Transparent);

	this->_textSprite[0].rect.width = CURSOR_ENDX - CURSOR_STARTX;
	this->_textSprite[0].rect.height = 18;
	this->_textSprite[0].setSize(SokuLib::Vector2i{
		this->_textSprite[0].rect.width,
		this->_textSprite[0].rect.height
	}.to<unsigned>());
	this->_textSprite[0].setPosition({CURSOR_STARTX - (*(int *)0x411c64 == 1) * 2, CURSOR_STARTY});
	for (int i = ' '; i < 0x100; i++)
		this->_getTextSize(i);
	std::lock_guard<std::mutex> functionMutexGuard(this->_connection->functionMutex);
	this->onConnectRequest = this->_connection->onConnectRequest;
	this->onError = this->_connection->onError;
	this->onImpMsg = this->_connection->onImpMsg;
	this->onMsg = this->_connection->onMsg;
	this->onHostRequest = this->_connection->onHostRequest;
	this->onConnect = this->_connection->onConnect;
	this->onPlayerJoin = this->_connection->onPlayerJoin;
	this->onPlayerLeave = this->_connection->onPlayerLeave;
	this->_connection->onDisconnect = [this]{
		this->_disconnected = true;
	};
	this->_connection->onPlayerJoin = [this](const Player &r){
		{
			std::lock_guard<std::mutex> lock(this->_recentOpponentMutex);
			if (this->_recentOpponent && !this->_recentOpponent->matchActive && this->_recentOpponent->playerName == r.name)
				this->_recentOpponent->playerId = r.id;
		}
		this->_queuePlayerName(r.id, r.name);
	};
	this->_connection->onPlayerLeave = [this](const Player &r){
		{
			std::lock_guard<std::mutex> lock(this->_recentOpponentMutex);
			if (this->_recentOpponent && this->_recentOpponent->playerId == r.id) {
				this->_recentOpponent->matchActive = false;
				this->_recentOpponent->expiresAt = std::chrono::steady_clock::now() + std::chrono::minutes(5);
			}
		}
		this->_saveRecentOpponent();
		this->_queuePlayerRemoval(r.id);
	};
	this->_connection->onConnect = [this](const Lobbies::PacketOlleh &r){
		auto &bg = lobbyData->backgrounds[r.bg];
		int id = 0;

		this->_machines.reserve(bg.arcades.size());
		for (auto &arcade : bg.arcades) {
			if (arcade.old)
				this->_machines.emplace_back(
					UINT32_MAX,
					arcade.pos,
					&lobbyData->arcades.hostlist,
					lobbyData->arcades.skins[0]
				);
			else
				this->_machines.emplace_back(
					id,
					arcade.pos,
					&lobbyData->arcades.intro,
					lobbyData->arcades.skins[1]
				);
			id++;
		}
		id = 0;
		this->_elevators.reserve(bg.elevators.size());
		for (auto &elevator : bg.elevators)
			this->_elevators.emplace_back(
				id++,
				elevator.pos,
				elevator,
				lobbyData->elevators[elevator.skin < lobbyData->elevators.size() ? elevator.skin : 0]
			);
		this->_background = r.bg;
		this->_connection->getMe()->pos.x = bg.startX;
		this->_connection->getMe()->pos.y = bg.platforms[bg.startPlatform].pos.y;

		this->_roomName = std::string(r.name, strnlen(r.name, sizeof(r.name)));
		this->_lobbyIdentity = std::string(servHost) + ":" + std::to_string(servPort) + "/" + this->_roomName;
		this->_restoreRecentOpponent();
		this->_restoreRecentPlayerSelections();
		this->_queuePlayerName(r.id, std::string(r.realName, strnlen(r.realName, sizeof(r.realName))));
		this->_probeBlocklistServer();
		this->_music = "data/bgm/" + std::string(r.music, strnlen(r.music, sizeof(r.music))) + ".ogg";
		SokuLib::playBGM(this->_music.c_str());
		if (!hasIpv6Map())
			this->_addMessageToList(0xFFFF00, 0, chineseLanguage ? "未加载 IPv6MapSokuMod，因此无法使用 IPv6。" : "IPv6MapSokuMod isn't loaded, so IPv6 will not be supported.");
		else if (isIpv6Available()) {
			this->_addMessageToList(0xFFFF00, 0, chineseLanguage ? "支持 IPv6 连接。" : "IPv6 connectivity is supported.");
#ifdef _DEBUG
			this->_addMessageToList(DEBUG_COLOR, 0, "Your IPv6 Address is: " + getMyIpv6());
#endif
		} else
			this->_addMessageToList(0xFFFF00, 0, chineseLanguage ? "当前环境不支持 IPv6。" : "IPv6 not supported.");
	};
	this->_connection->onError = [this](const std::string &msg){
		this->_wasConnected = true;
		this->_openMessageBox(38, localizeLobbyMessage(msg), chineseLanguage ? "内部错误" : "Internal Error", MB_ICONERROR);
	};
	this->_connection->onImpMsg = [this](const std::string &msg){
		this->_openMessageBox(23, localizeLobbyMessage(msg), chineseLanguage ? "服务器通知" : "Notification from server", MB_ICONINFORMATION);
	};
	this->_connection->onMsg = [this](int32_t channel, int32_t player, const std::string &msg){
		if (channel == Lobbies::BLOCKLIST_CONTROL_CHANNEL && msg == "BLOCKCAP1") {
			// Synchronization is sent optimistically for compatibility with older
			// block-enabled servers which do not return BLOCKCAP1.
			this->_serverBlocklistSupported = true;
			return;
		}
		if (channel == Lobbies::BLOCKLIST_CONTROL_CHANNEL && msg.compare(0, 9, "BLOCKIP1\t") == 0) {
			auto separator = msg.find('\t', 9);
			if (separator != std::string::npos) {
				auto encodedName = msg.substr(9, separator - 9);
				auto ip = msg.substr(separator + 1);
				std::string name;
				if (encodedName.size() % 2 == 0) {
					auto digit = [](char c) -> int {
						if (c >= '0' && c <= '9') return c - '0';
						if (c >= 'a' && c <= 'f') return c - 'a' + 10;
						if (c >= 'A' && c <= 'F') return c - 'A' + 10;
						return -1;
					};
					for (size_t i = 0; i < encodedName.size(); i += 2) {
						auto high = digit(encodedName[i]);
						auto low = digit(encodedName[i + 1]);
						if (high < 0 || low < 0) {
							name.clear();
							break;
						}
						name.push_back(static_cast<char>((high << 4) | low));
					}
				}
				if (!name.empty() && Blocklist::contains(name) && Blocklist::addIp(name, ip)) {
					this->_syncBlocklistToServer();
				}
			}
			return;
		}
		if (
			player == 0 &&
			msg.find("__block") != std::string::npos &&
			(msg.find("Unknown command") != std::string::npos || msg.find("未知指令") != std::string::npos)
		)
			return;
		bool privateMessage = channel == -1;
		this->_logChatToFile(player, msg);
		this->_showEmoteBubble(player, msg);
		this->_showTextBubble(player, msg, privateMessage);
		std::optional<unsigned> colorOverride;
		bool opponentDisconnected = false;
		bool opponentJoined = false;
		bool opponentMessage = false;
		if (privateMessage)
			colorOverride = opponentChatColor;
		{
			std::lock_guard<std::mutex> lock(this->_recentOpponentMutex);
			if (this->_recentOpponent) {
				auto now = std::chrono::steady_clock::now();
				bool highlighted = this->_recentOpponent->matchActive || now < this->_recentOpponent->expiresAt;
				if (highlighted && player == this->_recentOpponent->playerId) {
					colorOverride = opponentChatColor;
					opponentMessage = true;
				}
				if (player == 0) {
					auto joinMessage = this->_recentOpponent->playerName + " has joined the lobby.";
					auto disconnectPrefix = this->_recentOpponent->playerName + " has disconnected";
					auto kickedPrefix = this->_recentOpponent->playerName + " has been kicked:";
					opponentJoined = msg == joinMessage;
					opponentDisconnected = msg.compare(0, disconnectPrefix.size(), disconnectPrefix) == 0 ||
						msg.compare(0, kickedPrefix.size(), kickedPrefix) == 0;
					if (highlighted && (opponentJoined || opponentDisconnected))
						colorOverride = opponentChatColor;
				}
			}
		}
		bool autoPopup = chatPopupMode == CHAT_POPUP_ALL || (chatPopupMode == CHAT_POPUP_OPPONENTS && opponentMessage);
		const bool inBattle =
			SokuLib::sceneId == SokuLib::SCENE_BATTLECL ||
			SokuLib::sceneId == SokuLib::SCENE_BATTLESV ||
			SokuLib::newSceneId == SokuLib::SCENE_BATTLECL ||
			SokuLib::newSceneId == SokuLib::SCENE_BATTLESV;
		if (inBattle && opponentMessage && chatPopupMode != CHAT_POPUP_NEVER)
			this->_battleOpponentChatPopup.store(true, std::memory_order_relaxed);
		auto endsWith = [&msg](const char *suffix) {
			auto length = strlen(suffix);

			return msg.size() >= length && msg.compare(msg.size() - length, length, suffix) == 0;
		};
		bool playerPresenceMessage = player == 0 && (
			endsWith(" has joined the lobby.") ||
			endsWith(" has disconnected") ||
			msg.find(" has been kicked:") != std::string::npos
		);
		if (chatPopupMode != CHAT_POPUP_NEVER && !playerPresenceMessage)
			playSound(49);
		this->_addMessageToList(channel, player, localizeLobbyMessage(msg), colorOverride, autoPopup);
		if (opponentDisconnected) {
			std::lock_guard<std::mutex> lock(this->_recentOpponentMutex);
			if (this->_recentOpponent) {
				this->_recentOpponent->matchActive = false;
				this->_recentOpponent->expiresAt = std::chrono::steady_clock::now() + std::chrono::minutes(5);
			}
		}
		this->_saveRecentOpponent();
	};
	this->_connection->onConnectRequest = [this](const std::string &ip, unsigned short port, bool spectate){
		printf("onConnectRequest %s %u %s\n", ip.c_str(), port, spectate ? "true" : "false");
		playSound(57);
		if (!checkIp(ip)) {
			Lobbies::PacketArcadeLeave leave{0};

			this->_connection->send(&leave, sizeof(leave));
			this->_addMessageToList(
				0xFF0000,
				0,
				chineseLanguage ? "连接失败：对方配置的 IP 地址无效（" + ip + "）。" : "Failed to connect: Your opponent's custom IP is invalid (" + ip + ")"
			);
			return;
		}
		this->_addMessageToList(
			0x00FF00,
			0,
			strncmp(ip.c_str(), "127.127.", 8) ?
				(chineseLanguage ? "通过 IPv4 连接。" : "Connect via IPv4.") :
				(chineseLanguage ? "通过 IPv6 连接。" : "Connect via IPv6.")
		);
#ifdef _DEBUG
		this->_addMessageToList(DEBUG_COLOR, 0, "Connecting to " + ip + ":" + std::to_string(port) + (spectate ? " as spectator" : " as a player"));
#endif
		if (spectate)
			this->_connection->getMe()->battleStatus = Lobbies::BATTLE_STATUS_SPECTATING;
		else
			this->_connection->getMe()->battleStatus = Lobbies::BATTLE_STATUS_PLAYING;

		Lobbies::PacketBattleStatusUpdate packet{0, this->_connection->getMe()->battleStatus};

		this->_connection->send(&packet, sizeof(packet));
		this->_parent->joinHost(ip.c_str(), port, spectate);
	};
	this->_connection->onHostRequest = [this]{
		if (SokuLib::sceneId != SokuLib::SCENE_TITLE)
			return hostPort;

		Lobbies::PacketBattleStatusUpdate packet{0, Lobbies::BATTLE_STATUS_PLAYING};

		this->_connection->send(&packet, sizeof(packet));
		playSound(57);
		this->_connection->getMe()->battleStatus = Lobbies::BATTLE_STATUS_PLAYING;
		if (this->_parent->choice == 0)
			this->_startHosting();
		return hostPort;
	};
	this->_connection->onArcadeEngage = [this](const Player &p, uint32_t id){
		if (id >= this->_machines.size() - 1)
			return;

		auto &machine = this->_machines[id];

		machine.mutex.lock();
		machine.playerCount++;
		if (machine.playerCount == 1) {
			machine.animation = 0;
			machine.animationCtr = 0;
			machine.animIdle = false;
			machine.currentAnim = &lobbyData->arcades.select;
			if (&p == this->_connection->getMe()) {
				printf("Host pref %x\n", p.settings.hostPref);
				if (p.settings.hostPref & Lobbies::HOSTPREF_ACCEPT_HOSTLIST)
					this->_startHosting();
			}
		} else if (machine.playerCount == 2) {
			machine.animation = 0;
			machine.animationCtr = 0;
			machine.animIdle = false;
			machine.currentAnim = &lobbyData->arcades.game[random() % lobbyData->arcades.game.size()];
		}
		machine.mutex.unlock();
	};
	this->_connection->onArcadeLeave = [this](const Player &p, uint32_t id){
		if (p.id == this->_connection->getMe()->id) {
			Lobbies::PacketBattleStatusUpdate packet{0, Lobbies::BATTLE_STATUS_IDLE};

			this->_currentMachine = nullptr;
			this->_connection->getMe()->battleStatus = Lobbies::BATTLE_STATUS_IDLE;
			this->_connection->send(&packet, sizeof(packet));
		}
		if (id >= this->_machines.size() - 1)
			return;

		auto &machine = this->_machines[id];

		machine.mutex.lock();
		machine.playerCount--;
		if (machine.playerCount == 1) {
			machine.animation = 0;
			machine.animationCtr = 0;
			machine.animIdle = false;
			machine.currentAnim = &lobbyData->arcades.select;
		} else if (machine.playerCount == 0) {
			machine.animIdle = true;
			machine.animationCtr = 0;
			machine.currentAnim = &lobbyData->arcades.intro;
			machine.animation = machine.currentAnim->frameCount - 1;
		}
		machine.mutex.unlock();
	};
	this->_connectThread = std::thread{[this](){
		for (int i = 0; i < lobbyJoinTries; i++) {
			if (this->_disconnected)
				return;
			if (!this->_connection->isConnected())
				return;
			if (this->_connection->isInit())
				return;
			this->_connection->connect();
			std::this_thread::sleep_for(std::chrono::seconds(lobbyJoinInterval));
		}
		playSound(38);
		this->_wasConnected = true;
		MessageBoxW(SokuLib::window, L"Failed to join lobby: Connection timed out.", L"Timed out", MB_ICONERROR);
	}};
	ptrMutex.lock();
	activeMenu = this;
	ptrMutex.unlock();
	this->_messageBoxThread = std::thread{[this](){
		for (;; std::this_thread::sleep_for(std::chrono::milliseconds(100))) {
			// * (char *)0x0089FFDC: is Soku still running
			if (!*(char *)0x0089FFDC)
				return;
			this->_connection->meMutex.lock();
			if (SokuLib::sceneId != SokuLib::SCENE_TITLE && this->_connection->getMe()->battleStatus == Lobbies::BATTLE_STATUS_WAITING) {
				Lobbies::PacketBattleStatusUpdate packet{0, Lobbies::BATTLE_STATUS_PLAYING};

				this->_connection->send(&packet, sizeof(packet));
				this->_connection->getMe()->battleStatus = Lobbies::BATTLE_STATUS_PLAYING;
				this->_connection->sendGameInfo();
			}
			this->_connection->meMutex.unlock();
			if (SokuLib::sceneId == SokuLib::SCENE_BATTLECL || SokuLib::sceneId == SokuLib::SCENE_BATTLESV)
				continue;

			std::lock_guard<std::mutex> messageBoxMutexGuard(this->_messageBoxQueueMutex);

			if (this->_messageBoxQueue.empty()) {
				if (activeMenu)
					continue;
				else
					break;
			}
			const MessageBoxArgs &args = this->_messageBoxQueue.front();
			playSound(args.sound);
			MessageBoxW(SokuLib::window, args.text.c_str(), args.title.c_str(), args.type);
			this->_messageBoxQueue.pop();
		}
	}};
	if (!Original_WndProc)
		Original_WndProc = (WNDPROC) SetWindowLongPtr(SokuLib::window, GWL_WNDPROC, (LONG_PTR) Hooked_WndProc);
}

InLobbyMenu::~InLobbyMenu()
{
	if (this->_hostlist)
		this->_hostlist->requestStop();
	for (auto &hostlist : this->_retiredHostlists)
		hostlist->requestStop();
	this->_saveRecentOpponent(true);
	this->_saveRecentPlayerSelections();
	ptrMutex.lock();
	activeMenu = nullptr;
	ptrMutex.unlock();
	if (this->immCtx)
		ImmReleaseContext(SokuLib::window, this->immCtx);
	this->_unhook();
	if (!this->_disconnected) {
		this->_connection->onDisconnect = nullptr;
		this->_connection->disconnect();
	}
	this->_menu->setActive();
	if (!this->_music.empty())
		SokuLib::playBGM("data/bgm/op2.ogg");
	if (this->_hostThread.joinable())
		this->_hostThread.join();
	if (this->_connectThread.joinable())
		this->_connectThread.join();
	if (this->_messageBoxThread.joinable())
		this->_messageBoxThread.join();
}

void InLobbyMenu::_()
{
	*(int *)0x882a94 = 0x16;
	if (this->_disconnected || !this->_connection->isInit() || !this->_connection->isConnected())
		return;

	Lobbies::PacketArcadeLeave leave{0};
	Lobbies::PacketBattleStatusUpdate packet{0, Lobbies::BATTLE_STATUS_IDLE};

	this->_connection->send(&packet, sizeof(packet));
	this->_connection->send(&leave, sizeof(leave));
	if (!this->_hostlist) {
		this->_currentMachine = nullptr;
		SokuLib::playBGM(this->_music.c_str());
	} else
		SokuLib::playBGM("data/bgm/op2.ogg");
	*(*(char **)0x89a390 + 20) = false;
	this->_parent->choice = 0;
	this->_parent->subchoice = 0;
	this->_connection->meMutex.lock();
	this->_connection->getMe()->battleStatus = Lobbies::BATTLE_STATUS_IDLE;
	this->_connection->meMutex.unlock();
	messageBox->active = false;
}

int InLobbyMenu::onProcess()
{
	this->_retiredHostlists.erase(std::remove_if(
		this->_retiredHostlists.begin(),
		this->_retiredHostlists.end(),
		[](const std::unique_ptr<SmallHostlist> &hostlist) {
			return hostlist->isStopped();
		}
	), this->_retiredHostlists.end());
	if (this->_disconnected)
		return false;
	try {
		this->_menu->execUiCallbacks();
		std::unique_lock<std::mutex> meMutexLock(this->_connection->meMutex);
		if (!this->_connection->isInit()) {
			if (!this->_wasConnected) {
				this->_loadingGear.setRotation(this->_loadingGear.getRotation() + 0.1);
				return this->_connection->isConnected();
			}
			else
				return false;
		}

		auto inputs = SokuLib::inputMgrs.input;
		auto me = this->_connection->getMe();

		this->routePendingHotkeys();
		if (this->_translateAnimation) {
			if (this->_translateAnimation)
				this->_translate += this->_translateStep;
			this->_translateAnimation--;
		}
		memset(&SokuLib::inputMgrs.input, 0, sizeof(SokuLib::inputMgrs.input));
		// We call MenuConnect::onProcess directly because we don't want to trigger any hook.
		// After all, we are not technically inside the connect menu.
		reinterpret_cast<void (__thiscall *)(SokuLib::MenuConnect *)>(0x449160)(this->_parent);
		SokuLib::inputMgrs.input = inputs;
		if (this->_hostlist && (this->_hostlistExitRequested || !this->_hostlist->update())) {
			this->_hostlistExitRequested = false;
			SokuLib::playBGM(this->_music.c_str());
			this->_hostlist->requestStop();
			this->_retiredHostlists.emplace_back(std::move(this->_hostlist));
			this->_currentMachine = nullptr;
			this->_nextArcadeActionAt = std::chrono::steady_clock::now() + std::chrono::seconds(1);
			playSound(0x29);
			return true;
		}
		this->updateChat(false);
		if (this->_parent->choice > 0) {
			if (this->_parent->subchoice == 5) { //Already Playing
				this->_parent->notSpectateFlag = !this->_parent->notSpectateFlag;
				this->_parent->join();
			} else if (this->_parent->subchoice == 10) { //Connect Failed
				Lobbies::PacketArcadeLeave leave{0};

				this->_connection->send(&leave, sizeof(leave));
				if (!this->_hostlist)
					this->_currentMachine = nullptr;

				Lobbies::PacketBattleStatusUpdate packet{0, Lobbies::BATTLE_STATUS_IDLE};

				this->_connection->send(&packet, sizeof(packet));
				me->battleStatus = Lobbies::BATTLE_STATUS_IDLE;
				*(*(char **)0x89a390 + 20) = false;
				this->_addMessageToList(
					0xFF0000,
					0,
					chineseLanguage ?
						(this->_parent->subchoice == 5 ? "连接对手失败：对方已经开始游戏。" : "连接对手失败：网络连接失败。") :
						("Failed connecting to opponent: " + std::string(this->_parent->subchoice == 5 ? "They are already playing" : "Connection failed"))
				);
				this->_parent->choice = 0;
				this->_parent->subchoice = 0;
				messageBox->active = false;
			}
		}
		if (this->_lobbyExitRequested) {
			this->_lobbyExitRequested = false;
			playSound(0x29);
			if (!this->_editingText) {
				meMutexLock.unlock();
				this->_unhook();
				this->_connection->disconnect();
				if (this->_parent->choice == SokuLib::MenuConnect::CHOICE_HOST) {
					memset(&SokuLib::inputMgrs.input, 0, sizeof(SokuLib::inputMgrs.input));
					SokuLib::inputMgrs.input.b = 1;
					reinterpret_cast<void (__thiscall *)(SokuLib::MenuConnect *)>(0x449160)(this->_parent);
					SokuLib::inputMgrs.input = inputs;
				}
				this->_parent->choice = 0;
				this->_parent->subchoice = 0;
				return false;
			}
		}

		auto &bg = lobbyData->backgrounds[this->_background];

		if (SokuLib::inputMgrs.input.changeCard) {
			int amount = 16;

			if (me->dir & 0b1111) {
				me->dir &= 0b10000;

				Lobbies::PacketMove m{0, me->dir};

				this->_connection->send(&m, sizeof(m));
			}
			if (SokuLib::inputMgrs.input.d)
				amount = 32;
			if (SokuLib::inputMgrs.input.horizontalAxis < 0)
				this->_camera.x -= amount;
			else if (SokuLib::inputMgrs.input.horizontalAxis > 0)
				this->_camera.x += amount;
			if (SokuLib::inputMgrs.input.verticalAxis < 0)
				this->_camera.y -= amount;
			else if (SokuLib::inputMgrs.input.verticalAxis > 0)
				this->_camera.y += amount;

			if (this->_camera.x < 320)
				this->_camera.x = 320;
			else if (this->_camera.x > bg.size.x - 320)
				this->_camera.x = bg.size.x - 320;
			if (this->_camera.y < 340)
				this->_camera.y = 340;
			else if (this->_camera.y > bg.size.y - 140)
				this->_camera.y = bg.size.y - 140;
		} else if (this->_currentElevator) {
			bool elevatorChanged = false;

			if (
				this->_currentElevator->pos.x != me->pos.x ||
				(this->_elevatorCtr != (this->_elevatorOut ? 0 : 30) && this->_currentElevator->state == 2)
			) {
				int diff = me->pos.x - this->_currentElevator->pos.x;

				if (std::abs(diff) <= PLAYER_H_SPEED)
					me->pos.x = this->_currentElevator->pos.x;
				else
					me->pos.x -= std::copysign(PLAYER_H_SPEED, diff);
				if ((me->dir & 0b1111) != 0b0011) {
					me->dir &= 0b10000;
					me->dir |= 0b00011;

					Lobbies::PacketMove m{0, me->dir};

					this->_connection->send(&m, sizeof(m));
				}
				if (diff == 0);
				else if (diff > 0)
					me->dir |= 0b10000;
				else
					me->dir &= 0b01111;
			} else if (me->dir & 0b1111) {
				me->dir &= 0b10000;

				Lobbies::PacketMove m{0, me->dir};

				this->_connection->send(&m, sizeof(m));
			}
			switch (this->_currentElevator->state) {
			case 0:
				if (SokuLib::inputMgrs.input.b == 1 || SokuLib::inputMgrs.input.a == 1) {
					this->_elevatorOut = true;
					this->_currentElevator->state = 1;
					break;
				}
				if (SokuLib::inputMgrs.input.horizontalAxis == 1) {
					if (this->_currentElevator->links.rightLink) {
						this->_currentPlatform = this->_currentElevator->links.rightLink->platform;
						this->_currentElevator = &this->_elevators[this->_currentElevator->links.rightLink->elevator];
						playSound(0x27);
						elevatorChanged = true;
					}
				} else if (SokuLib::inputMgrs.input.horizontalAxis == -1) {
					if (this->_currentElevator->links.leftLink) {
						this->_currentPlatform = this->_currentElevator->links.leftLink->platform;
						this->_currentElevator = &this->_elevators[this->_currentElevator->links.leftLink->elevator];
						playSound(0x27);
						elevatorChanged = true;
					}
				} else if (SokuLib::inputMgrs.input.verticalAxis == -1) {
					if (this->_currentElevator->links.upLink) {
						this->_currentPlatform = this->_currentElevator->links.upLink->platform;
						this->_currentElevator = &this->_elevators[this->_currentElevator->links.upLink->elevator];
						playSound(0x27);
						elevatorChanged = true;
					}
				} else if (SokuLib::inputMgrs.input.verticalAxis == 1) {
					if (this->_currentElevator->links.downLink) {
						this->_currentPlatform = this->_currentElevator->links.downLink->platform;
						this->_currentElevator = &this->_elevators[this->_currentElevator->links.downLink->elevator];
						playSound(0x27);
						elevatorChanged = true;
					}
				}
				if (elevatorChanged) {
					SokuLib::Vector2i afterTranslate;

					if (this->_currentElevator->pos.x < 320)
						afterTranslate.x = 0;
					else if (this->_currentElevator->pos.x > bg.size.x - 320)
						afterTranslate.x = 640 - bg.size.x;
					else
						afterTranslate.x = 320 - this->_currentElevator->pos.x;
					afterTranslate.y = 340 - this->_currentElevator->pos.y;
					this->_translateAnimation = 15;
					this->_translateTarget = afterTranslate;
					this->_translateStep = {
						(int)(this->_translateTarget.x - this->_translate.x) / (int)this->_translateAnimation,
						(int)(this->_translateTarget.y - this->_translate.y) / (int)this->_translateAnimation
					};
				}
				me->pos.x = this->_currentElevator->pos.x;
				me->pos.y = this->_currentElevator->pos.y;
				break;
			case 2:
				if (!this->_elevatorOut) {
					if (std::find(this->_insideElevator.begin(), this->_insideElevator.end(), me->id) == this->_insideElevator.end())
						this->_insideElevator.push_back(me->id);
					if (this->_elevatorCtr < 30) {
						this->_elevatorCtr++;
						this->_zoom = 1 - (this->_elevatorCtr / 60.f);
						break;
					}
					this->_currentElevator->state = 3;
				} else {
					if (this->_elevatorCtr > 0) {
						this->_elevatorCtr--;
						this->_zoom = 1 - (this->_elevatorCtr / 60.f);
						break;
					}
					this->_insideElevator.erase(std::find(this->_insideElevator.begin(), this->_insideElevator.end(), me->id));
					this->_currentElevator->state = 3;
					this->_currentElevator = nullptr;
					this->_elevatorOut = false;
				}
				break;
			default:
				break;
			}
		} else if (!this->_currentMachine && !this->_editingText) {
			if (
				SokuLib::inputMgrs.input.a == 1 &&
				std::chrono::steady_clock::now() >= this->_nextArcadeActionAt
			) {
				for (auto &machine : this->_machines) {
					if (me->pos.x < machine.pos.x - machine.skin.sprite.getSize().x / 2)
						continue;
					if (me->pos.y < machine.pos.y)
						continue;
					if (me->pos.x > machine.pos.x + machine.skin.sprite.getSize().x / 2)
						continue;
					if (me->pos.y > machine.pos.y + machine.skin.sprite.getSize().y)
						continue;
					if (machine.id != UINT32_MAX) {
						auto blockedPlayer = this->_getBlacklistedBattlePlayer(machine.id);

						if (blockedPlayer) {
							this->_addMessageToList(
								0xFF0000,
								0,
								chineseLanguage
									? "无法加入：" + *blockedPlayer + " 在你的黑名单中。"
									: "Cannot join: " + *blockedPlayer + " is in your blacklist."
							);
							goto touched;
						}
					}
					this->_currentMachine = &machine;
					playSound(0x28);
					if (machine.id == UINT32_MAX) {
						this->_hostlist.reset(new SmallHostlist(0.6, {128, 48}, this->_parent));
						SokuLib::playBGM("data/bgm/op2.ogg");
						for (auto &achievement : lobbyData->achievementByRequ["old_arcade"])
							if (!achievement->awarded) {
								achievement->awarded = true;
								lobbyData->achievementAwardQueue.push_back(achievement);
							}
						goto touched;
					}

					Lobbies::PacketGameRequest packet{machine.id};
					Lobbies::PacketBattleStatusUpdate p{0, Lobbies::BATTLE_STATUS_WAITING};

					this->_nextArcadeActionAt = std::chrono::steady_clock::now() + std::chrono::seconds(1);
					me->battleStatus = Lobbies::BATTLE_STATUS_WAITING;
					this->_connection->send(&packet, sizeof(packet));
					this->_connection->send(&p, sizeof(p));
					goto touched;
				}
			touched:
				for (auto &elevator : this->_elevators) {
					if (me->pos.x < elevator.pos.x - elevator.skin.cage.width / 2)
						continue;
					if (me->pos.y < elevator.pos.y)
						continue;
					if (me->pos.x > elevator.pos.x + elevator.skin.cage.width / 2)
						continue;
					if (me->pos.y > elevator.pos.y + elevator.skin.cage.height)
						continue;
					this->_currentElevator = &elevator;
					this->_currentElevator->state = 1;
					playSound(0x28);
					break;
				}
			}

			auto newDir = me->dir;

			if (SokuLib::inputMgrs.input.horizontalAxis) {
				if (SokuLib::inputMgrs.input.horizontalAxis < 0 && me->pos.x < PLAYER_H_SPEED) {
					playSound(0x29);
					meMutexLock.unlock();
					this->_connection->disconnect();
					return false;
				}
				newDir &= 0b01100;
				newDir |= 0b00001 << (SokuLib::inputMgrs.input.horizontalAxis < 0 ? 1 : 0);
				if (SokuLib::inputMgrs.input.horizontalAxis < 0)
					newDir |= 0b10000;

				auto &platform = bg.platforms[this->_currentPlatform];

				if (me->pos.x <= platform.pos.x) {
					newDir &= 0b11101;
					me->pos.x = platform.pos.x;
				}
				if (me->pos.x >= platform.pos.x + platform.width) {
					newDir &= 0b11110;
					me->pos.x = platform.pos.x + platform.width;
				}
			} else
				newDir &= 0b11100;
			if (SokuLib::inputMgrs.input.d == 0)
				newDir &= ~0b100000;
			else
				newDir |= 0b100000;
			me->pos.y = bg.platforms[this->_currentPlatform].pos.y;
			if (newDir != me->dir) {
				me->dir = newDir;
				Lobbies::PacketMove l{0, me->dir};
				this->_connection->send(&l, sizeof(l));
			}
		} else {
			if (me->dir & 0b1111) {
				me->dir &= 0b10000;

				Lobbies::PacketMove m{0, me->dir};

				this->_connection->send(&m, sizeof(m));
			}
			if (
				SokuLib::inputMgrs.input.b == 1 &&
				!this->_editingText &&
				!this->_hostlist &&
				std::chrono::steady_clock::now() >= this->_nextArcadeActionAt
			) {
				Lobbies::PacketArcadeLeave l{0};
				Lobbies::PacketBattleStatusUpdate p{0, Lobbies::BATTLE_STATUS_IDLE};

				this->_nextArcadeActionAt = std::chrono::steady_clock::now() + std::chrono::seconds(1);
				this->_connection->send(&p, sizeof(p));
				this->_connection->send(&l, sizeof(l));
				me->battleStatus = Lobbies::BATTLE_STATUS_IDLE;
				this->_currentMachine = nullptr;
				if (this->_parent->choice == SokuLib::MenuConnect::CHOICE_HOST) {
					memset(&SokuLib::inputMgrs.input, 0, sizeof(SokuLib::inputMgrs.input));
					SokuLib::inputMgrs.input.b = 1;
					reinterpret_cast<void (__thiscall *)(SokuLib::MenuConnect *)>(0x449160)(this->_parent);
					SokuLib::inputMgrs.input = inputs;
				} else
					playSound(0x29);
				this->_parent->choice = 0;
				this->_parent->subchoice = 0;
				messageBox->active = false;
			}
		}
		for (auto &machine : this->_machines) {
			machine.mutex.lock();
			if (machine.animIdle)
				goto checkSkinAnim;
			machine.animationCtr++;
			if (machine.animationCtr < 60 / machine.currentAnim->frameRate)
				goto checkSkinAnim;
			machine.animationCtr = 0;
			machine.animation++;
			if (machine.animation < machine.currentAnim->frameCount)
				goto checkSkinAnim;
			if (machine.currentAnim->loop)
				machine.animation = 0;
			else {
				machine.animIdle = true;
				machine.animation--;
			}
		checkSkinAnim:
			machine.skinAnimationCtr++;
			if (machine.skinAnimationCtr < 60 / machine.skin.frameRate)
				goto done;
			machine.skinAnimationCtr = 0;
			machine.skinAnimation++;
			if (machine.skinAnimation < machine.skin.frameCount)
				goto done;
			machine.skinAnimation = 0;
		done:
			machine.mutex.unlock();
		}
		for (auto &elevator : this->_elevators) {
			elevator.skinAnimationCtr += elevator.skin.frameRate;
			if (elevator.skinAnimationCtr < 60)
				goto checkAnim;
			while (elevator.skinAnimationCtr >= 60) {
				elevator.skinAnimationCtr -= 60;
				elevator.skinAnimation++;
				if (elevator.skinAnimation < elevator.skin.frameCount)
					continue;
				elevator.skinAnimation = 0;
			}
		checkAnim:
			if (elevator.state == 1) {
				elevator.animation++;
				if (elevator.animation >= 30) {
					elevator.animation = 30;
					elevator.state = 2;
				}
			}
			if (elevator.state == 3) {
				elevator.animation--;
				if (elevator.animation <= 0) {
					elevator.animation = 0;
					elevator.state = 0;
				}
			}
		}

		this->_connection->updatePlayers(lobbyData->avatars);
		if (SokuLib::inputMgrs.input.changeCard == 0)
			this->_camera = me->pos;
		if (this->_connection->isInit() && !this->_translateAnimation) {
			if (this->_camera.x < 320)
				this->_translate.x = 0;
			else if (this->_camera.x > bg.size.x - 320)
				this->_translate.x = 640 - bg.size.x;
			else
				this->_translate.x = 320 - this->_camera.x;
			this->_translate.y = 340 - this->_camera.y;
		}
		this->_playersCopy = this->_connection->getPlayers();
		this->_playersById.clear();
		this->_playersById.reserve(this->_playersCopy.size());
		for (const auto &player : this->_playersCopy)
			this->_playersById.emplace(player.id, &player);
		this->_playersInsideElevator.clear();
		this->_playersInsideElevator.reserve(this->_insideElevator.size());
		this->_playersInsideElevator.insert(this->_insideElevator.begin(), this->_insideElevator.end());
		this->_updateRecentOpponent();
		return true;
	} catch (std::exception &e) {
		MessageBoxA(
			SokuLib::window,
			(
				"Error updating in game lobby. You have been kicked from the lobby.\n"
				"Please, report this error.\n"
				"\n"
				"Error:\n" +
				std::string(e.what())
			).c_str(),
			"SokuLobby error",
			MB_ICONERROR
		);
		return false;
	}
}

int InLobbyMenu::onRender()
{
	if (this->_disconnected)
		return 0;
	try {
		if (!this->_connection->isInit() && !this->_wasConnected) {
			this->_messageBox.draw();
			this->_loadingText.draw();
			this->_loadingGear.setRotation(-this->_loadingGear.getRotation());
			this->_loadingGear.setPosition({412, 227});
			this->_loadingGear.draw();
			this->_loadingGear.setRotation(-this->_loadingGear.getRotation());
			this->_loadingGear.setPosition({412 + 23, 227 - 18});
			this->_loadingGear.draw();
			return 0;
		}
		this->_processPendingTextureWork();

		auto &bg = lobbyData->backgrounds[this->_background];

		SokuLib::DrawUtils::RectangleShape rect2;
#ifdef _DEBUG
		SokuLib::DrawUtils::RectangleShape rect;

		rect.setBorderColor(SokuLib::Color::White);
		rect.setFillColor(SokuLib::Color{0xFF, 0xFF, 0xFF, 0xA0});
#endif
		rect2.setBorderColor(SokuLib::Color::Black);
		rect2.setFillColor(SokuLib::Color{0x00, 0x00, 0x00, 0xA0});

		auto oldTranslate = this->_translate;

		this->_translate.x -= 320;
		this->_translate.x *= this->_zoom;
		this->_translate.x += 320;
		if (this->_translate.x > 0)
			this->_translate.x = 0;
		if (this->_translate.x < 640 - bg.size.x * this->_zoom)
			this->_translate.x = 640 - bg.size.x * this->_zoom;

		this->_translate.y -= 340;
		this->_translate.y *= this->_zoom;
		this->_translate.y += 340;
		if (this->_translate.y > 0)
			this->_translate.y = 0;
		if (this->_translate.y < 480 - bg.size.y * this->_zoom)
			this->_translate.y = 480 - bg.size.y * this->_zoom;
		for (auto &layer : bg.layers) {
			if (layer.type == LobbyData::LAYERTYPE_IMAGE) {
				SokuLib::Vector2i tsize = {
					static_cast<int>(layer.image->getSize().x * this->_zoom - 640),
					static_cast<int>(layer.image->getSize().y * this->_zoom - 480)
				};
				SokuLib::Vector2i bsize = {
					static_cast<int>(bg.size.x * this->_zoom - 640),
					static_cast<int>(bg.size.y * this->_zoom - 480)
				};
				auto translate = this->_translate;

				translate.x = translate.x * tsize.x / bsize.x;
				translate.y = translate.y * tsize.y / bsize.y;

				auto s = layer.image->getSize();

				layer.image->setSize((s * this->_zoom).to<unsigned>());
				layer.image->setPosition(translate);
				layer.image->draw();
				layer.image->setSize(s);
				continue;
			}
			if (layer.type == LobbyData::LAYERTYPE_CLOCK && bg.clock) {
				auto t = std::chrono::system_clock::now();
				std::time_t timestamp = std::chrono::system_clock::to_time_t(t);
				auto timeInfo = std::localtime(&timestamp);

				if (timeInfo) {
					if (bg.clock->hour) {
						auto s = bg.clock->hour->getSize();

						bg.clock->hour->setSize((s * this->_zoom).to<unsigned>());
						bg.clock->hour->setPosition((bg.clock->center * this->_zoom - bg.clock->hour->getSize() / 2 + this->_translate).to<int>());
						bg.clock->hour->setRotation((((timeInfo->tm_sec / 60.f + timeInfo->tm_min) / 60.f) + timeInfo->tm_hour) * M_PI * 2 / 12);
						bg.clock->hour->draw();
						bg.clock->hour->setSize(s);
					}
					if (bg.clock->minute) {
						auto s = bg.clock->minute->getSize();

						bg.clock->minute->setSize((s * this->_zoom).to<unsigned>());
						bg.clock->minute->setPosition((bg.clock->center * this->_zoom - bg.clock->minute->getSize() / 2 + this->_translate).to<int>());
						bg.clock->minute->setRotation((timeInfo->tm_sec / 60.f + timeInfo->tm_min) * M_PI * 2 / 60);
						bg.clock->minute->draw();
						bg.clock->minute->setSize(s);
					}
					if (bg.clock->second) {
						auto s = bg.clock->second->getSize();

						bg.clock->second->setSize((s * this->_zoom).to<unsigned>());
						bg.clock->second->setPosition((bg.clock->center * this->_zoom - bg.clock->second->getSize() / 2 + this->_translate).to<int>());
						bg.clock->second->setRotation(timeInfo->tm_sec * M_PI * 2 / 60);
						bg.clock->second->draw();
						bg.clock->second->setSize(s);
					}
				} else
					puts("Error");
				continue;
			}
			for (auto &machine : this->_machines) {
				SokuLib::Vector2i pos{
					static_cast<int>(this->_translate.x + (machine.pos.x - machine.skin.sprite.getSize().x / 2) * this->_zoom),
					static_cast<int>(this->_translate.y + (machine.pos.y - machine.skin.sprite.getSize().y) * this->_zoom)
				};

				machine.mutex.lock();
				machine.skin.sprite.setPosition(pos);
				machine.skin.sprite.rect.left = machine.skinAnimation * machine.skin.sprite.rect.width;

				auto s = machine.skin.sprite.getSize();

				machine.skin.sprite.setSize((s * this->_zoom).to<unsigned>());
				if (isRectVisible(pos.x, pos.y, machine.skin.sprite.getSize().x, machine.skin.sprite.getSize().y, 16))
					machine.skin.sprite.draw();
				machine.skin.sprite.setSize(s);

				pos += machine.skin.animationOffsets * this->_zoom;
				machine.currentAnim->sprite.setPosition(pos);
				if (machine.currentAnim->tilePerLine) {
					machine.currentAnim->sprite.rect.left = machine.animation % machine.currentAnim->tilePerLine * machine.currentAnim->size.x;
					machine.currentAnim->sprite.rect.top = machine.animation / machine.currentAnim->tilePerLine * machine.currentAnim->size.y;
				}

				auto s2 = machine.currentAnim->sprite.getSize();

				machine.currentAnim->sprite.setSize((s2 * this->_zoom).to<unsigned>());
				if (isRectVisible(pos.x, pos.y, machine.currentAnim->sprite.getSize().x, machine.currentAnim->sprite.getSize().y, 16))
					machine.currentAnim->sprite.draw();
				machine.currentAnim->sprite.setSize(s2);

				machine.mutex.unlock();
			}
			for (auto &elevator : this->_elevators) {
				if (elevator.links.hidden)
					continue;

				SokuLib::Vector2i pos{
					static_cast<int>(this->_translate.x + (elevator.pos.x - elevator.skin.cage.width / 2) * this->_zoom),
					static_cast<int>(this->_translate.y + (elevator.pos.y - elevator.skin.cage.height) * this->_zoom )
				};
				if (!isRectVisible(
					pos.x,
					pos.y,
					static_cast<int>(elevator.skin.cage.width * this->_zoom),
					static_cast<int>(elevator.skin.cage.height * this->_zoom),
					96
				))
					continue;

				elevator.skin.sprite.rect = elevator.skin.cage;
				elevator.skin.sprite.setPosition(pos);
				elevator.skin.sprite.setSize({
					static_cast<unsigned>(elevator.skin.cage.width * this->_zoom),
					static_cast<unsigned>(elevator.skin.cage.height * this->_zoom)
				});
				elevator.skin.sprite.rect.left += elevator.skinAnimation * elevator.skin.sprite.rect.width;
				elevator.skin.sprite.draw();

				if (elevator.links.noIndicator)
					continue;

				auto posBase = pos;

				posBase.x += (elevator.skin.cage.width / 2) * this->_zoom;
				posBase.y -= (elevator.skin.indicator.height / 2 + 8) * this->_zoom;
				pos = posBase;
				pos.x -= (elevator.skin.indicator.width / 2) * this->_zoom;
				pos.y -= (elevator.skin.indicator.height / 2) * this->_zoom;
				elevator.skin.sprite.rect = elevator.skin.indicator;
				elevator.skin.sprite.setPosition(pos);
				elevator.skin.sprite.setSize({
					static_cast<unsigned>(elevator.skin.sprite.rect.width * this->_zoom),
					static_cast<unsigned>(elevator.skin.sprite.rect.height * this->_zoom)
				});
				elevator.skin.sprite.draw();

				pos = posBase;
				pos.y -= (elevator.skin.arrow.height / 2) * this->_zoom;
				if (elevator.links.upLink && elevator.links.downLink) {
					pos.x -= (elevator.skin.arrow.width + 2) * this->_zoom;
					elevator.skin.sprite.rect = elevator.skin.arrow;
					elevator.skin.sprite.setPosition(pos);
					elevator.skin.sprite.setSize({
						static_cast<unsigned>(elevator.skin.sprite.rect.width * this->_zoom),
						static_cast<unsigned>(elevator.skin.sprite.rect.height * this->_zoom)
					});
					elevator.skin.sprite.draw();

					pos.x += (elevator.skin.arrow.width + 4) * this->_zoom;
					elevator.skin.sprite.rect.left += elevator.skin.arrow.width;
					elevator.skin.sprite.setPosition(pos);
					elevator.skin.sprite.setSize({
						static_cast<unsigned>(elevator.skin.sprite.rect.width * this->_zoom),
						static_cast<unsigned>(elevator.skin.sprite.rect.height * this->_zoom)
					});
					elevator.skin.sprite.draw();
				} else if (elevator.links.upLink || elevator.links.downLink) {
					pos.x -= (elevator.skin.arrow.width / 2) * this->_zoom;
					elevator.skin.sprite.rect = elevator.skin.arrow;
					if (elevator.links.upLink)
						elevator.skin.sprite.rect.left += elevator.skin.arrow.width;
					elevator.skin.sprite.setPosition(pos);
					elevator.skin.sprite.setSize({
						static_cast<unsigned>(elevator.skin.sprite.rect.width * this->_zoom),
						static_cast<unsigned>(elevator.skin.sprite.rect.height * this->_zoom)
					});
					elevator.skin.sprite.draw();
				}
			}
			for (auto &player : this->_playersCopy) {
				if (this->_playersInsideElevator.find(player.id) == this->_playersInsideElevator.end())
					continue;
				if (player.player.avatar < lobbyData->avatars.size()) {
					auto &avatar = lobbyData->avatars[player.player.avatar];
					auto elevatorScale = this->_elevatorCtr / ELEVEATOR_CTR_DIVIDER + 1;
					int visibleWidth = static_cast<int>(avatar.sprite.texture.getSize().x / avatar.nbAnimations * avatar.scale / elevatorScale * this->_zoom);
					int visibleHeight = static_cast<int>(avatar.sprite.texture.getSize().y / 2 * avatar.scale / elevatorScale * this->_zoom);
					int visibleX = static_cast<int>(this->_translate.x + player.pos.x * this->_zoom - visibleWidth / 2);
					int visibleY = static_cast<int>(this->_translate.y + player.pos.y * this->_zoom - visibleHeight);

					if (!isRectVisible(visibleX, visibleY, visibleWidth, visibleHeight, 16))
						continue;

					avatar.sprite.tint = SokuLib::Color::White;
					avatar.sprite.rect.width = avatar.sprite.texture.getSize().x / avatar.nbAnimations;
					avatar.sprite.rect.height = avatar.sprite.texture.getSize().y / 2;
					avatar.sprite.setSize({
						static_cast<unsigned int>(avatar.sprite.rect.width * avatar.scale / (this->_elevatorCtr / ELEVEATOR_CTR_DIVIDER + 1)),
						static_cast<unsigned int>(avatar.sprite.rect.height * avatar.scale / (this->_elevatorCtr / ELEVEATOR_CTR_DIVIDER + 1))
					});
					avatar.sprite.setPosition({
						static_cast<int>(player.pos.x - avatar.sprite.getSize().x / 2),
						static_cast<int>(player.pos.y - avatar.sprite.getSize().y)
					});
					avatar.sprite.rect.top = avatar.sprite.rect.height * player.animation;
					avatar.sprite.rect.left = player.currentAnimation * avatar.sprite.rect.width;
					if (this->_elevatorCtr >= 15) {
						SokuLib::Vector2i pos{
							static_cast<int>(this->_currentElevator->pos.x - this->_currentElevator->skin.cage.width / 2),
							static_cast<int>(this->_currentElevator->pos.y - this->_currentElevator->skin.cage.height)
						};
						SokuLib::Vector2i size{
							static_cast<int>((this->_currentElevator->skin.doorLeft.width + this->_currentElevator->skin.doorRight.width)),
							static_cast<int>(min(this->_currentElevator->skin.doorLeft.height, this->_currentElevator->skin.doorRight.height))
						};
						auto oldPos = avatar.sprite.getPosition();
						auto oldSize = avatar.sprite.getSize();
						auto newPos = avatar.sprite.getPosition();
						auto newSize = avatar.sprite.getSize();
						bool changed = false;

						pos += this->_currentElevator->skin.doorOffset;
						if (avatar.sprite.getPosition().x < pos.x) {
							newPos.x = pos.x;
							avatar.sprite.rect.left += (pos.x - oldPos.x) * (this->_elevatorCtr / ELEVEATOR_CTR_DIVIDER + 1) / avatar.scale;
							newSize.x -= pos.x - oldPos.x;
							changed = true;
						}
						if (avatar.sprite.getPosition().y < pos.y) {
							newPos.y = pos.y;
							avatar.sprite.rect.top += (pos.y - oldPos.y) * (this->_elevatorCtr / ELEVEATOR_CTR_DIVIDER + 1) / avatar.scale;
							newSize.y -= pos.y - oldPos.y;
							changed = true;
						}
						if (newSize.x > size.x) {
							newSize.x = size.x;
							changed = true;
						}
						if (newSize.y > size.y) {
							newSize.y = size.y;
							changed = true;
						}
						if (changed) {
							avatar.sprite.setPosition(newPos);
							avatar.sprite.setSize(newSize);
							avatar.sprite.rect.width = newSize.x * (this->_elevatorCtr / ELEVEATOR_CTR_DIVIDER + 1) / avatar.scale;
							avatar.sprite.rect.height = newSize.y * (this->_elevatorCtr / ELEVEATOR_CTR_DIVIDER + 1) / avatar.scale;
						}
					}
					avatar.sprite.setMirroring(false, false);

					avatar.sprite.setSize((avatar.sprite.getSize() * this->_zoom).to<unsigned>());
					avatar.sprite.setPosition(((avatar.sprite.getPosition() * this->_zoom) + this->_translate).to<int>());
				#ifdef _DEBUG
					extern bool debug;
					if (debug) {
						rect.setSize(avatar.sprite.getSize());
						rect.setPosition(avatar.sprite.getPosition());
						rect.draw();
					}
				#endif
					avatar.sprite.draw();
					avatar.sprite.rect.width = avatar.sprite.texture.getSize().x / avatar.nbAnimations;
					avatar.sprite.rect.height = avatar.sprite.texture.getSize().y / 2;
					avatar.sprite.setSize({
						static_cast<unsigned int>(avatar.sprite.rect.width * avatar.scale / (this->_elevatorCtr / ELEVEATOR_CTR_DIVIDER + 1)),
						static_cast<unsigned int>(avatar.sprite.rect.height * avatar.scale / (this->_elevatorCtr / ELEVEATOR_CTR_DIVIDER + 1))
					});
					avatar.sprite.setPosition({
						static_cast<int>(player.pos.x - avatar.sprite.getSize().x / 2),
						static_cast<int>(player.pos.y - avatar.sprite.getSize().y)
					});
				} else {
					int visibleX = static_cast<int>(this->_translate.x + (player.pos.x - 32) * this->_zoom);
					int visibleY = static_cast<int>(this->_translate.y + (player.pos.y + 64) * this->_zoom);
					if (!isRectVisible(visibleX, visibleY, static_cast<int>(64 * this->_zoom), static_cast<int>(64 * this->_zoom), 16))
						continue;
					rect2.setSize({static_cast<unsigned int>(64 * this->_zoom), static_cast<unsigned int>(64 * this->_zoom)});
					rect2.setPosition({
						static_cast<int>(this->_translate.x + (player.pos.x - 32) * this->_zoom),
						static_cast<int>(this->_translate.y + (player.pos.y + 64) * this->_zoom)
					});
					rect2.draw();
				}
			}
			for (auto &elevator : this->_elevators) {
				if (elevator.links.hidden)
					continue;
				if (elevator.animation >= 30)
					continue;

				SokuLib::Vector2i pos{
					static_cast<int>(this->_translate.x + (elevator.pos.x - elevator.skin.cage.width / 2) * this->_zoom),
					static_cast<int>(this->_translate.y + (elevator.pos.y - elevator.skin.cage.height) * this->_zoom)
				};
				if (!isRectVisible(
					pos.x,
					pos.y,
					static_cast<int>(elevator.skin.cage.width * this->_zoom),
					static_cast<int>(elevator.skin.cage.height * this->_zoom),
					32
				))
					continue;

				if (elevator.skin.anim == LobbyData::DOOR_OPEN_SLIDE) {
					pos += elevator.skin.doorOffset * this->_zoom;
					elevator.skin.sprite.rect = elevator.skin.doorLeft;
					elevator.skin.sprite.rect.width = elevator.skin.sprite.rect.width * (30 - elevator.animation) / 30;
					elevator.skin.sprite.rect.left += elevator.skin.doorRight.width - elevator.skin.sprite.rect.width;
					elevator.skin.sprite.setPosition(pos);
					elevator.skin.sprite.setSize({
						static_cast<unsigned>(elevator.skin.sprite.rect.width * this->_zoom),
						static_cast<unsigned>(elevator.skin.sprite.rect.height * this->_zoom)
					});
					elevator.skin.sprite.draw();

					pos.x += elevator.skin.doorLeft.width * this->_zoom;
					elevator.skin.sprite.rect = elevator.skin.doorRight;
					elevator.skin.sprite.rect.width = elevator.skin.sprite.rect.width * (30 - elevator.animation) / 30;
					pos.x += (elevator.skin.doorRight.width - elevator.skin.sprite.rect.width) * this->_zoom;
					elevator.skin.sprite.setPosition(pos);
					elevator.skin.sprite.setSize({
						static_cast<unsigned>(std::ceil(elevator.skin.sprite.rect.width * this->_zoom)),
						static_cast<unsigned>(elevator.skin.sprite.rect.height * this->_zoom)
					});
					elevator.skin.sprite.draw();
				} else if (elevator.skin.anim == LobbyData::DOOR_OPEN_ROTATE) {
					float angle = M_PI_2 * elevator.animation / 30;

					pos += elevator.skin.doorOffset * this->_zoom;
					elevator.skin.sprite.rect = elevator.skin.doorLeft;
					elevator.skin.sprite.setPosition(pos);
					elevator.skin.sprite.setSize({
						static_cast<unsigned>(elevator.skin.sprite.rect.width * cos(angle) * this->_zoom),
						static_cast<unsigned>(elevator.skin.sprite.rect.height * this->_zoom)
					});
					elevator.skin.sprite.draw();

					pos.x += (elevator.skin.doorLeft.width + elevator.skin.doorRight.width - elevator.skin.doorLeft.width * cos(angle)) * this->_zoom;
					elevator.skin.sprite.rect = elevator.skin.doorRight;
					elevator.skin.sprite.setPosition(pos);
					elevator.skin.sprite.setSize({
						static_cast<unsigned>(std::ceil(elevator.skin.sprite.rect.width * cos(angle) * this->_zoom)),
						static_cast<unsigned>(elevator.skin.sprite.rect.height * this->_zoom)
					});
					elevator.skin.sprite.draw();
				}
			}

			for (auto &player : this->_playersCopy) {
				if (this->_playersInsideElevator.find(player.id) != this->_playersInsideElevator.end())
					continue;
				if (player.player.avatar < lobbyData->avatars.size()) {
					auto &avatar = lobbyData->avatars[player.player.avatar];
					int visibleWidth = static_cast<int>(avatar.sprite.texture.getSize().x / avatar.nbAnimations * avatar.scale * this->_zoom);
					int visibleHeight = static_cast<int>(avatar.sprite.texture.getSize().y / 2 * avatar.scale * this->_zoom);
					int visibleX = static_cast<int>(this->_translate.x + player.pos.x * this->_zoom - visibleWidth / 2);
					int visibleY = static_cast<int>(this->_translate.y + player.pos.y * this->_zoom - visibleHeight);

					if (!isRectVisible(visibleX, visibleY, visibleWidth, visibleHeight, 16))
						continue;

					avatar.sprite.tint = SokuLib::Color::White;
					avatar.sprite.rect.width = avatar.sprite.texture.getSize().x / avatar.nbAnimations;
					avatar.sprite.rect.height = avatar.sprite.texture.getSize().y / 2;
					avatar.sprite.setSize({
						static_cast<unsigned int>(avatar.sprite.rect.width * avatar.scale * this->_zoom),
						static_cast<unsigned int>(avatar.sprite.rect.height * avatar.scale * this->_zoom)
					});
					avatar.sprite.setPosition({
						static_cast<int>(this->_translate.x + player.pos.x * this->_zoom - avatar.sprite.getSize().x / 2),
						static_cast<int>(this->_translate.y + player.pos.y * this->_zoom - avatar.sprite.getSize().y)
					});
					avatar.sprite.rect.top = avatar.sprite.rect.height * player.animation;
					avatar.sprite.rect.left = player.currentAnimation * avatar.sprite.rect.width;
					avatar.sprite.setMirroring((player.dir & 0b10000) == 0, false);
				#ifdef _DEBUG
					extern bool debug;
					if (debug) {
						rect.setSize(avatar.sprite.getSize());
						rect.setPosition(avatar.sprite.getPosition());
						rect.draw();
					}
				#endif
					avatar.sprite.draw();
					if (player.battleStatus) {
						auto &status = this->_battleStatus[player.battleStatus - 1];

						status.setSize((status.texture.getSize() * this->_zoom).to<unsigned>());
						status.setPosition({
							static_cast<int>(this->_translate.x + player.pos.x * this->_zoom - status.getSize().x / 2),
							static_cast<int>(this->_translate.y + player.pos.y * this->_zoom - avatar.sprite.getSize().y - status.getSize().y)
						});
						status.draw();
					}
				} else {
					int visibleX = static_cast<int>(this->_translate.x + (player.pos.x - 32) * this->_zoom);
					int visibleY = static_cast<int>(this->_translate.y + (player.pos.y - 64) * this->_zoom);
					if (!isRectVisible(visibleX, visibleY, static_cast<int>(64 * this->_zoom), static_cast<int>(64 * this->_zoom), 16))
						continue;
					rect2.setSize({static_cast<unsigned int>(64 * this->_zoom), static_cast<unsigned int>(64 * this->_zoom)});
					rect2.setPosition({
						static_cast<int>(this->_translate.x + (player.pos.x - 64 / 2) * this->_zoom),
						static_cast<int>(this->_translate.y + (player.pos.y - 64) * this->_zoom)
					});
					rect2.draw();
				}
			}
		}

		struct NamePlacement {
			float minX;
			float maxX;
			float y;
		};
		constexpr int firstNameRow = -2;
		constexpr int lastNameRow = 25;
		static thread_local std::array<std::vector<NamePlacement>, lastNameRow - firstNameRow + 1> nameRows;

		for (auto &row : nameRows)
			row.clear();

		for (auto &player : this->_playersCopy) {
			auto playerData = this->_extraPlayerData.find(player.id);
			if (playerData == this->_extraPlayerData.end())
				continue;
			auto &name = playerData->second.name;
			auto minPos = this->_translate.x + player.pos.x * this->_zoom - name.getSize().x / 2.f;
			auto maxPos = this->_translate.x + player.pos.x * this->_zoom + name.getSize().x / 2.f;
			auto posY = this->_translate.y + (player.pos.y - 120) * this->_zoom;
			if (!isRectVisible(
				static_cast<int>(minPos),
				static_cast<int>(posY),
				static_cast<int>(name.getSize().x),
				static_cast<int>(name.getSize().y),
				16
			))
				continue;
			bool conflict = false;
			bool visible = true;

			do {
				conflict = false;
				auto row = static_cast<int>(std::floor(posY / 20.f));
				for (
					int nearbyRow = max(firstNameRow, row - 1);
					nearbyRow <= min(lastNameRow, row + 1) && !conflict;
					nearbyRow++
				) {
					for (const auto &old : nameRows[nearbyRow - firstNameRow]) {
						if (old.maxX < minPos || old.minX > maxPos || old.y <= posY - 20 || old.y >= posY + 20)
							continue;
						conflict = true;
						posY -= 20;
						break;
					}
				}
				if (posY + name.getSize().y < 0)
					visible = false;
			} while (conflict && visible);
			if (!visible)
				continue;

			name.setPosition({
				static_cast<int>(minPos),
				static_cast<int>(posY)
			});
			auto row = std::clamp(static_cast<int>(std::floor(posY / 20.f)), firstNameRow, lastNameRow);
			nameRows[row - firstNameRow].push_back({minPos, maxPos, posY});
			name.draw();
		}
		this->_renderTextBubbles(this->_playersById);
		this->_renderEmoteBubbles(this->_playersById);
		this->_translate = oldTranslate;
		if (this->_currentMachine)
			this->_renderMachineOverlay();
		if (!this->_hostlist)
			this->renderChat();
	} catch (std::exception &e) {
		MessageBoxA(
			SokuLib::window,
			(
				"Error rendering in game lobby. You have been kicked from the lobby.\n"
				"Please, report this error.\n"
				"\n"
				"Error:\n" +
				std::string(e.what())
			).c_str(),
			"SokuLobby error",
			MB_ICONERROR
		);
	}
	return 0;
}

void InLobbyMenu::_unhook()
{
	std::lock_guard<std::mutex> functionMutexGuard(this->_connection->functionMutex);
	this->_connection->onConnectRequest = this->onConnectRequest;
	this->_connection->onError = this->onError;
	this->_connection->onImpMsg = this->onImpMsg;
	this->_connection->onMsg = this->onMsg;
	this->_connection->onHostRequest = this->onHostRequest;
	this->_connection->onConnect = this->onConnect;
	this->_connection->onPlayerJoin = this->onPlayerJoin;
	this->_connection->onPlayerLeave = this->onPlayerLeave;
	this->_connection->onArcadeEngage = this->onArcadeEngage;
	this->_connection->onArcadeLeave = this->onArcadeLeave;
}

void InLobbyMenu::_addMessageToList(unsigned int channel, unsigned player, const std::string &msg, std::optional<unsigned> colorOverride, bool autoPopup)
{
	// Error messages must remain visible regardless of the F3 popup mode.
	if (channel == 0xFF0000) {
		this->_errorChatPopup.store(true, std::memory_order_relaxed);
		this->_chatTimer = 900;
	} else if (autoPopup)
		this->_chatTimer = 900;
	std::lock_guard<std::mutex> lock(this->_chatMessagesMutex);
	this->_chatMessages.emplace_front();
	this->_chatMessages.front().preserveScrollAnchor = this->_chatScrolledAwayFromBottom.load(std::memory_order_relaxed);
	this->_chatMessages.front().lazy_message.emplace(channel, player, msg, colorOverride);
}

void InLobbyMenu::_queuePlayerName(unsigned player, const std::string &name)
{
	std::lock_guard<std::mutex> lock(this->_pendingTextureWorkMutex);
	if (this->_pendingPlayerNames.find(player) == this->_pendingPlayerNames.end())
		this->_pendingPlayerNameOrder.push(player);
	this->_pendingPlayerNames[player] = name;
}

void InLobbyMenu::_queuePlayerRemoval(unsigned player)
{
	std::lock_guard<std::mutex> lock(this->_pendingTextureWorkMutex);
	this->_pendingPlayerRemovals.insert(player);
	this->_pendingPlayerNames.erase(player);
	this->_pendingTextBubbles.erase(player);
}

void InLobbyMenu::_buildPlayerName(unsigned player, const std::string &name)
{
	constexpr int horizontalPadding = 2;
	constexpr SokuLib::Vector2i textureSize{200, 20};
	SokuLib::Vector2i size;
	int texId = 0;
	std::wstring decoded;

	try {
		decoded = convertEncoding<char, wchar_t, UTF8Decode, UTF16Encode>(name);
	} catch (...) {
		return;
	}
	auto &font = lobbyData->getFont(16);
	auto originalOffsetX = font.description.offsetX;

	font.description.offsetX = originalOffsetX + horizontalPadding;
	auto created = createTextTexture(texId, decoded.c_str(), font, textureSize, &size);
	font.description.offsetX = originalOffsetX;
	if (!created) {
		puts("Error creating player name texture");
		return;
	}
	size.x = (std::min)(textureSize.x, size.x + horizontalPadding * 2);
	auto &sprite = this->_extraPlayerData[player].name;
	sprite.texture.setHandle(texId, textureSize.to<unsigned>());
	sprite.setSize(size.to<unsigned>());
	sprite.rect.width = size.x;
	sprite.rect.height = size.y;
}

void InLobbyMenu::_processPendingTextureWork()
{
	constexpr unsigned playerNamesPerFrame = 2;
	constexpr unsigned textBubblesPerFrame = 2;
	std::set<uint32_t> removals;
	std::vector<std::pair<uint32_t, std::string>> playerNames;
	struct TextBubbleWork {
		uint32_t player;
		std::string message;
		bool privateMessage;
		std::chrono::steady_clock::time_point receivedAt;
	};
	std::vector<TextBubbleWork> textBubbles;
	{
		std::lock_guard<std::mutex> lock(this->_pendingTextureWorkMutex);
		auto now = std::chrono::steady_clock::now();

		removals.swap(this->_pendingPlayerRemovals);
		for (unsigned i = 0; i < playerNamesPerFrame && !this->_pendingPlayerNameOrder.empty();) {
			auto player = this->_pendingPlayerNameOrder.front();
			this->_pendingPlayerNameOrder.pop();
			auto work = this->_pendingPlayerNames.find(player);
			if (work == this->_pendingPlayerNames.end())
				continue;
			playerNames.emplace_back(work->first, std::move(work->second));
			this->_pendingPlayerNames.erase(work);
			i++;
		}
		for (unsigned i = 0; i < textBubblesPerFrame && !this->_pendingTextBubbleOrder.empty();) {
			auto player = this->_pendingTextBubbleOrder.front();
			this->_pendingTextBubbleOrder.pop();
			auto work = this->_pendingTextBubbles.find(player);
			if (work == this->_pendingTextBubbles.end())
				continue;
			if (now - work->second.receivedAt < TEXT_BUBBLE_LIFETIME)
				textBubbles.push_back({work->first, std::move(work->second.message), work->second.privateMessage, work->second.receivedAt});
			this->_pendingTextBubbles.erase(work);
			i++;
		}
	}
	if (!removals.empty()) {
		{
			std::lock_guard<std::mutex> lock(this->_playerEmoteBubblesMutex);
			for (auto player : removals)
				this->_playerEmoteBubbles.erase(player);
		}
		for (auto player : removals) {
			this->_playerTextBubbles.erase(player);
			this->_extraPlayerData.erase(player);
		}
	}
	for (const auto &[player, name] : playerNames)
		this->_buildPlayerName(player, name);
	for (const auto &work : textBubbles)
		this->_buildTextBubble(work.player, work.message, work.privateMessage, work.receivedAt);
}

void InLobbyMenu::_showEmoteBubble(unsigned player, const std::string &msg)
{
	if (player == 0)
		return;
	auto marker = msg.find('\x01');
	if (
		marker == std::string::npos || marker + 3 != msg.size() || marker < 3 ||
		msg.compare(marker - 3, 3, "]: ") != 0
	)
		return;
	auto low = static_cast<unsigned char>(msg[marker + 1]);
	auto high = static_cast<unsigned char>(msg[marker + 2]);
	if (!(low & 0x80) || !(high & 0x80))
		return;
	unsigned emoteId = (low & 0x7F) | ((high & 0x7F) << 7);
	if (emoteId == 0 || emoteId >= lobbyData->emotes.size())
		return;

	std::lock_guard<std::mutex> lock(this->_playerEmoteBubblesMutex);
	this->_playerEmoteBubbles[player] = {emoteId, std::chrono::steady_clock::now()};
}

void InLobbyMenu::_renderEmoteBubbles(const std::unordered_map<uint32_t, const Player *> &playersById)
{
	constexpr auto lifetime = std::chrono::milliseconds(10000);
	constexpr auto fadeIn = std::chrono::milliseconds(150);
	constexpr auto fadeOut = std::chrono::milliseconds(400);
	auto now = std::chrono::steady_clock::now();
	std::map<uint32_t, PlayerEmoteBubble> bubbles;
	{
		std::lock_guard<std::mutex> lock(this->_playerEmoteBubblesMutex);
		for (auto it = this->_playerEmoteBubbles.begin(); it != this->_playerEmoteBubbles.end();) {
			if (now - it->second.startedAt >= lifetime)
				it = this->_playerEmoteBubbles.erase(it);
			else {
				bubbles.emplace(*it);
				++it;
			}
		}
	}

	for (const auto &[playerId, bubble] : bubbles) {
		auto playerEntry = playersById.find(playerId);
		if (playerEntry == playersById.end() || bubble.emoteId >= lobbyData->emotes.size())
			continue;
		auto player = playerEntry->second;

		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - bubble.startedAt);
		float opacity = 1.f;
		if (elapsed < fadeIn)
			opacity = static_cast<float>(elapsed.count()) / fadeIn.count();
		else if (elapsed > lifetime - fadeOut)
			opacity = static_cast<float>((lifetime - elapsed).count()) / fadeOut.count();
		unsigned char alpha = static_cast<unsigned char>(std::clamp(opacity, 0.f, 1.f) * 255);
		int avatarHeight = 64;
		if (player->player.avatar < lobbyData->avatars.size()) {
			auto &avatar = lobbyData->avatars[player->player.avatar];
			avatarHeight = static_cast<int>(avatar.sprite.texture.getSize().y / 2 * avatar.scale);
		}
		int x = static_cast<int>(this->_translate.x + player->pos.x * this->_zoom - 23);
		int floatOffset = static_cast<int>(std::round(std::sin(elapsed.count() * 2 * M_PI / 1600.f) * 3));
		int y = static_cast<int>(this->_translate.y + (player->pos.y - avatarHeight) * this->_zoom - 54 - floatOffset);
		x = std::clamp(x, 2, 592);
		y = std::clamp(y, 2, 426);

		SokuLib::DrawUtils::RectangleShape frame;
		SokuLib::DrawUtils::RectangleShape tail;
		frame.setPosition({x, y});
		frame.setSize({46, 46});
		frame.setBorderColor(SokuLib::Color{0x78, 0x80, 0x90, alpha});
		frame.setFillColor(SokuLib::Color{0xF0, 0xF2, 0xF5, static_cast<unsigned char>(alpha * 0.9f)});
		frame.draw();
		tail.setPosition({x + 20, y + 45});
		tail.setSize({6, 7});
		tail.setBorderColor(SokuLib::Color{0x78, 0x80, 0x90, alpha});
		tail.setFillColor(SokuLib::Color{0xF0, 0xF2, 0xF5, static_cast<unsigned char>(alpha * 0.9f)});
		tail.draw();

		auto &emote = lobbyData->emotes[bubble.emoteId];
		emote.sprite.rect.left = 0;
		emote.sprite.rect.top = 0;
		emote.sprite.rect.width = EMOTE_SIZE;
		emote.sprite.rect.height = EMOTE_SIZE;
		emote.sprite.setSize({EMOTE_SIZE, EMOTE_SIZE});
		emote.sprite.setPosition({x + 7, y + 7});
		emote.sprite.tint = SokuLib::Color{0xFF, 0xFF, 0xFF, alpha};
		emote.sprite.draw();
	}
}

void InLobbyMenu::_showTextBubble(unsigned player, const std::string &msg, bool privateMessage)
{
	if (!showTextBubbles || player == 0 || SokuLib::sceneId != SokuLib::SCENE_TITLE)
		return;
	std::lock_guard<std::mutex> lock(this->_pendingTextureWorkMutex);
	if (this->_pendingTextBubbles.find(player) == this->_pendingTextBubbles.end())
		this->_pendingTextBubbleOrder.push(player);
	this->_pendingTextBubbles[player] = {msg, privateMessage, std::chrono::steady_clock::now()};
}

void InLobbyMenu::_buildTextBubble(unsigned player, const std::string &msg, bool privateMessage, std::chrono::steady_clock::time_point receivedAt)
{
	auto bodyStart = msg.find("]: ");
	if (bodyStart == std::string::npos)
		return;
	bodyStart += 3;
	std::string body;
	for (size_t i = bodyStart; i < msg.size();) {
		if (static_cast<unsigned char>(msg[i]) == 0x01 && i + 2 < msg.size()) {
			i += 3;
			continue;
		}
		body += msg[i++];
	}
	if (body.empty())
		return;

	std::wstring text;
	try {
		text = convertEncoding<char, wchar_t, UTF8Decode, UTF16Encode>(body);
	} catch (...) {
		return;
	}
	for (auto &c : text)
		if (c == L'\r' || c == L'\n' || c == L'\t')
			c = L' ';
	text.erase(text.begin(), std::find_if(text.begin(), text.end(), [](wchar_t c) { return !std::iswspace(c); }));
	text.erase(std::find_if(text.rbegin(), text.rend(), [](wchar_t c) { return !std::iswspace(c); }).base(), text.end());
	if (text.empty())
		return;

	constexpr int maxTextWidth = 260;
	auto &bubbleFont = this->_textBubbleFont;
	auto avoidSplitSurrogate = [](const std::wstring &value, size_t fit) {
		if (fit && fit < value.size() && value[fit - 1] >= 0xD800 && value[fit - 1] <= 0xDBFF)
			fit--;
		return fit;
	};
	std::wstring lines[2];
	auto fit = avoidSplitSurrogate(text, getTextFit(text.c_str(), bubbleFont, maxTextWidth, true));
	if (fit >= text.size())
		lines[0] = text;
	else {
		if (!fit)
			fit = text.size() >= 2 && text[0] >= 0xD800 && text[0] <= 0xDBFF && text[1] >= 0xDC00 && text[1] <= 0xDFFF ? 2 : 1;
		auto breakAt = text.find_last_of(L" \t", fit - 1);
		if (breakAt != std::wstring::npos && breakAt != 0)
			fit = breakAt;
		lines[0] = text.substr(0, fit);
		lines[1] = text.substr(fit);
		lines[1].erase(lines[1].begin(), std::find_if(lines[1].begin(), lines[1].end(), [](wchar_t c) { return !std::iswspace(c); }));
		auto secondFit = avoidSplitSurrogate(lines[1], getTextFit(lines[1].c_str(), bubbleFont, maxTextWidth, true));
		if (secondFit < lines[1].size()) {
			auto ellipsisWidth = getTextSize(L"...", bubbleFont, {maxTextWidth, 28}, true).x;
			secondFit = avoidSplitSurrogate(lines[1], getTextFit(lines[1].c_str(), bubbleFont, maxTextWidth - ellipsisWidth, true));
			lines[1].resize(secondFit);
			lines[1] += L"...";
		}
	}

	unsigned lineCount = lines[1].empty() ? 1 : 2;
	SokuLib::Vector2i textSizes[2]{};
	int textureIds[2]{};
	for (unsigned i = 0; i < lineCount; i++) {
		if (!createTextTexture(textureIds[i], lines[i].c_str(), bubbleFont, {maxTextWidth, 28}, &textSizes[i], true)) {
			for (unsigned j = 0; j < i; j++)
				SokuLib::textureMgr.remove(textureIds[j]);
			return;
		}
	}
	auto &bubble = this->_playerTextBubbles[player];
	for (unsigned i = lineCount; i < bubble.lineCount; i++)
		bubble.text[i].texture.destroy();
	for (unsigned i = 0; i < lineCount; i++) {
		bubble.text[i].texture.setHandle(textureIds[i], {maxTextWidth, 28});
		bubble.text[i].rect.left = 0;
		bubble.text[i].rect.top = 0;
		bubble.text[i].rect.width = textSizes[i].x;
		bubble.text[i].rect.height = textSizes[i].y;
		bubble.text[i].setSize(textSizes[i].to<unsigned>());
		bubble.textSize[i] = textSizes[i];
	}
	bubble.lineCount = lineCount;
	bubble.privateMessage = privateMessage;
	bubble.startedAt = receivedAt;
}

void InLobbyMenu::_clearTextBubbles()
{
	{
		std::lock_guard<std::mutex> lock(this->_pendingTextureWorkMutex);
		this->_pendingTextBubbles.clear();
		std::queue<uint32_t> empty;
		this->_pendingTextBubbleOrder.swap(empty);
	}
	this->_playerTextBubbles.clear();
}

void InLobbyMenu::_renderTextBubbles(const std::unordered_map<uint32_t, const Player *> &playersById)
{
	constexpr auto fadeIn = std::chrono::milliseconds(150);
	constexpr auto fadeOut = std::chrono::milliseconds(400);
	auto now = std::chrono::steady_clock::now();
	for (auto it = this->_playerTextBubbles.begin(); it != this->_playerTextBubbles.end();) {
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.startedAt);
		if (elapsed >= TEXT_BUBBLE_LIFETIME) {
			it = this->_playerTextBubbles.erase(it);
			continue;
		}
		auto playerEntry = playersById.find(it->first);
		if (playerEntry == playersById.end()) {
			++it;
			continue;
		}
		auto player = playerEntry->second;

		float opacity = 1.f;
		if (elapsed < fadeIn)
			opacity = static_cast<float>(elapsed.count()) / fadeIn.count();
		else if (elapsed > TEXT_BUBBLE_LIFETIME - fadeOut)
			opacity = static_cast<float>((TEXT_BUBBLE_LIFETIME - elapsed).count()) / fadeOut.count();
		unsigned char alpha = static_cast<unsigned char>(std::clamp(opacity, 0.f, 1.f) * 255);
		int avatarHeight = 64;
		if (player->player.avatar < lobbyData->avatars.size()) {
			auto &avatar = lobbyData->avatars[player->player.avatar];
			avatarHeight = static_cast<int>(avatar.sprite.texture.getSize().y / 2 * avatar.scale);
		}
		int textWidth = 0;
		for (unsigned i = 0; i < it->second.lineCount; i++)
			textWidth = max(textWidth, it->second.textSize[i].x);
		int bubbleWidth = std::clamp(textWidth + 18, 60, 278);
		int bubbleHeight = it->second.lineCount == 2 ? 58 : 38;
		int x = static_cast<int>(this->_translate.x + player->pos.x * this->_zoom - bubbleWidth / 2);
		int y = static_cast<int>(this->_translate.y + (player->pos.y - avatarHeight) * this->_zoom - bubbleHeight - 8);
		x = std::clamp(x, 2, 638 - bubbleWidth);
		y = std::clamp(y, 2, 471 - bubbleHeight);

		SokuLib::DrawUtils::RectangleShape frame;
		SokuLib::DrawUtils::RectangleShape tail;
		SokuLib::Color borderColor{0x78, 0x80, 0x90, alpha};
		SokuLib::Color fillColor{0xF0, 0xF2, 0xF5, static_cast<unsigned char>(alpha * 0.9f)};
		if (it->second.privateMessage) {
			borderColor = SokuLib::Color{
				static_cast<unsigned char>((opponentChatColor >> 16) & 0xFF),
				static_cast<unsigned char>((opponentChatColor >> 8) & 0xFF),
				static_cast<unsigned char>(opponentChatColor & 0xFF), alpha};
			fillColor = SokuLib::Color{
				static_cast<unsigned char>((borderColor.r + 0xFF * 3) / 4),
				static_cast<unsigned char>((borderColor.g + 0xFF * 3) / 4),
				static_cast<unsigned char>((borderColor.b + 0xFF * 3) / 4),
				static_cast<unsigned char>(alpha * 0.94f)};
		}
		frame.setPosition({x, y});
		frame.setSize({static_cast<unsigned>(bubbleWidth), static_cast<unsigned>(bubbleHeight)});
		frame.setBorderColor(borderColor);
		frame.setFillColor(fillColor);
		frame.draw();
		tail.setPosition({x + bubbleWidth / 2 - 3, y + bubbleHeight - 1});
		tail.setSize({6, 7});
		tail.setBorderColor(borderColor);
		tail.setFillColor(fillColor);
		tail.draw();

		for (unsigned i = 0; i < it->second.lineCount; i++) {
			it->second.text[i].tint = SokuLib::Color{0x20, 0x24, 0x2C, alpha};
			it->second.text[i].setPosition({x + 9, y + 6 + static_cast<int>(i) * 24});
			it->second.text[i].draw();
		}
		++it;
	}
}

void InLobbyMenu::_restoreRecentOpponent()
{
	auto now = std::chrono::steady_clock::now();
	std::optional<RecentOpponentSession> session;
	{
		std::lock_guard<std::mutex> sessionLock(recentOpponentSessionMutex);
		if (!recentOpponentSession || recentOpponentSession->lobbyIdentity != this->_lobbyIdentity)
			return;
		if (!recentOpponentSession->matchActive && now >= recentOpponentSession->expiresAt) {
			recentOpponentSession.reset();
			return;
		}
		session = recentOpponentSession;
	}
	std::lock_guard<std::mutex> lock(this->_recentOpponentMutex);
	this->_recentOpponent = RecentOpponent{
		session->playerId,
		session->playerName,
		session->machineId,
		session->matchActive,
		session->expiresAt
	};
}

void InLobbyMenu::_saveRecentOpponent(bool leavingLobby)
{
	if (this->_lobbyIdentity.empty())
		return;
	auto now = std::chrono::steady_clock::now();
	std::optional<RecentOpponent> opponent;
	{
		std::lock_guard<std::mutex> lock(this->_recentOpponentMutex);
		if (!this->_recentOpponent)
			return;
		if (leavingLobby && this->_recentOpponent->matchActive) {
			this->_recentOpponent->matchActive = false;
			this->_recentOpponent->expiresAt = now + std::chrono::minutes(5);
		}
		if (!this->_recentOpponent->matchActive && now >= this->_recentOpponent->expiresAt)
			return;
		opponent = this->_recentOpponent;
	}
	std::lock_guard<std::mutex> sessionLock(recentOpponentSessionMutex);
	recentOpponentSession = RecentOpponentSession{
		opponent->playerId,
		opponent->playerName,
		opponent->machineId,
		opponent->matchActive,
		opponent->expiresAt,
		this->_lobbyIdentity
	};
}

void InLobbyMenu::_restoreRecentPlayerSelections()
{
	std::lock_guard<std::mutex> sessionLock(recentPlayerSelectionsSessionMutex);
	if (!recentPlayerSelectionsSession || recentPlayerSelectionsSession->lobbyIdentity != this->_lobbyIdentity)
		return;
	this->_recentPlayerSelections.clear();
	for (const auto &[playerId, playerName] : recentPlayerSelectionsSession->selections) {
		if (playerName.empty())
			continue;
		this->_recentPlayerSelections.push_back({playerId, playerName});
		if (this->_recentPlayerSelections.size() == 3)
			break;
	}
}

void InLobbyMenu::_saveRecentPlayerSelections()
{
	if (this->_lobbyIdentity.empty())
		return;
	RecentPlayerSelectionsSession session;
	session.lobbyIdentity = this->_lobbyIdentity;
	for (const auto &entry : this->_recentPlayerSelections) {
		if (entry.playerName.empty())
			continue;
		session.selections.emplace_back(entry.playerId, entry.playerName);
		if (session.selections.size() == 3)
			break;
	}
	std::lock_guard<std::mutex> sessionLock(recentPlayerSelectionsSessionMutex);
	recentPlayerSelectionsSession = std::move(session);
}

void InLobbyMenu::_updateRecentOpponent()
{
	auto me = this->_connection->getMe();
	if (!me)
		return;
	const auto &players = this->_playersCopy;
	auto now = std::chrono::steady_clock::now();
	auto machineId = this->_currentMachine ? this->_currentMachine->id : me->machineId;
	std::unique_lock<std::mutex> lock(this->_recentOpponentMutex);

	if (this->_recentOpponent) {
		if (!this->_recentOpponent->matchActive) {
			if (me->battleStatus == Lobbies::BATTLE_STATUS_PLAYING) {
				auto opponent = std::find_if(players.begin(), players.end(), [me, machineId](const Player &p){
					return p.id != me->id && p.machineId == machineId && p.battleStatus == Lobbies::BATTLE_STATUS_PLAYING;
				});
				if (opponent != players.end()) {
					this->_recentOpponent = RecentOpponent{opponent->id, opponent->name, opponent->machineId};
					lock.unlock();
					this->_saveRecentOpponent();
					return;
				}
			}
			if (now >= this->_recentOpponent->expiresAt) {
				this->_recentOpponent.reset();
				lock.unlock();
				std::lock_guard<std::mutex> sessionLock(recentOpponentSessionMutex);
				if (recentOpponentSession && recentOpponentSession->lobbyIdentity == this->_lobbyIdentity)
					recentOpponentSession.reset();
			}
			return;
		}

		auto opponent = std::find_if(players.begin(), players.end(), [this](const Player &p){
			return p.id == this->_recentOpponent->playerId;
		});
		if (
			me->battleStatus != Lobbies::BATTLE_STATUS_PLAYING ||
			opponent == players.end() ||
			opponent->battleStatus != Lobbies::BATTLE_STATUS_PLAYING ||
			machineId != this->_recentOpponent->machineId ||
			opponent->machineId != this->_recentOpponent->machineId
		) {
			this->_recentOpponent->matchActive = false;
			this->_recentOpponent->expiresAt = now + std::chrono::minutes(5);
		}
		lock.unlock();
		this->_saveRecentOpponent();
		return;
	}

	if (me->battleStatus != Lobbies::BATTLE_STATUS_PLAYING)
		return;
	for (const auto &player : players) {
		if (
			player.id != me->id &&
			player.machineId == machineId &&
			player.battleStatus == Lobbies::BATTLE_STATUS_PLAYING
		) {
			this->_recentOpponent = RecentOpponent{player.id, player.name, player.machineId};
			lock.unlock();
			this->_saveRecentOpponent();
			return;
		}
	}
}

void InLobbyMenu::_logChatToFile(unsigned player, const std::string &msg)
{
	static std::mutex logMutex;
	if (player == 0)
		return; // Skip system/notification lines; keep only user chat
	try {
		std::lock_guard<std::mutex> guard(logMutex);
		auto dir = std::filesystem::path(profileFolderPath) / "chatlog";
		std::filesystem::create_directories(dir);
		auto now = std::chrono::system_clock::now();
		auto tt = std::chrono::system_clock::to_time_t(now);
		std::tm local{};
		localtime_s(&local, &tt);
		std::ostringstream filename;
		filename << std::put_time(&local, "%Y-%m-%d") << ".txt";
		std::string sanitized = msg;
		for (auto &ch : sanitized)
			if (ch == '\n' || ch == '\r')
				ch = ' ';
		std::ofstream out(dir / filename.str(), std::ios::app);
		if (!out)
			return;
		out << std::put_time(&local, "%H:%M:%S") << " " << sanitized << "\n";
	} catch (...) {
	}
}

void InLobbyMenu::onKeyPressed(unsigned chr)
{
	if (chr == 0x7F || chr < 32 || !this->_editingText || this->_battleChatAwaitRelease)
		return;
	this->_textMutex.lock();
	if (this->_hasSelection())
		this->_deleteSelection();
	if (this->_lastPressed && this->_textTimer == 0) {
		std::basic_string<unsigned> s{&this->_lastPressed, &this->_lastPressed + 1};
		auto result = UTF16Encode(s);

		if (result.size() + this->_buffer.size() <= CHAT_CHARACTER_LIMIT) {
			this->_buffer.insert(this->_buffer.begin() + this->_textCursorPosIndex, result.begin(), result.end());
			this->_updateTextCursor(this->_textCursorPosIndex + result.size());
			this->textChanged |= 1;
			playSound(0x27);
		} else
			playSound(0x29);
	}
	this->_lastPressed = chr;
	this->_textTimer = 0;
	this->_textMutex.unlock();
}

void InLobbyMenu::onKeyReleased()
{
	this->_textMutex.lock();
	if (this->_hasSelection())
		this->_deleteSelection();
	if (this->_lastPressed && this->_textTimer == 0) {
		std::basic_string<unsigned> s{&this->_lastPressed, &this->_lastPressed + 1};
		auto result = UTF16Encode(s);

		if (result.size() + this->_buffer.size() <= CHAT_CHARACTER_LIMIT) {
			this->_buffer.insert(this->_buffer.begin() + this->_textCursorPosIndex, result.begin(), result.end());
			this->_updateTextCursor(this->_textCursorPosIndex + result.size());
			playSound(0x27);
			this->textChanged |= 1;
		} else
			playSound(0x29);
	}
	this->_lastPressed = 0;
	this->_textTimer = 0;
	this->_textMutex.unlock();
}

void InLobbyMenu::_inputBoxUpdate(bool blockChatInput)
{
	if (GetForegroundWindow() != SokuLib::window) {
		this->_battleChatHolding = false;
		this->_battleChatHintUntil = {};
		return;
	}
	std::lock_guard<std::mutex> lock_(this->keyTimersMutex);
	this->_processHotkeyEvents();
	bool openBattleChat = false;
	if (blockChatInput && !this->_battleChatActive && this->_editingText) {
		this->_editingText = false;
		this->_clearSelection();
		this->_chatOffset = 0;
		this->_chatScrolledAwayFromBottom.store(false, std::memory_order_relaxed);
		this->_privateMessageCompletions.clear();
		this->_privateMessageCompletionTimer = 0;
		this->immComposition.clear();
		this->compositionCursor = 0;
		if (this->immCtx)
			ImmNotifyIME(this->immCtx, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
	}
	this->_battleChatActive = blockChatInput;
	{
		auto now = std::chrono::steady_clock::now();
		bool down = (GetAsyncKeyState(chatKey) & 0x8000) != 0;
		if (this->_battleChatAwaitRelease) {
			// Do not turn the opening hold (including key repeat) into chat input.
			this->_battleChatAwaitRelease = down;
			memset(this->keysPressed, 0, sizeof(this->keysPressed));
			this->_lastPressed = 0;
			return;
		}
		if (!blockChatInput || this->_editingText || this->_emotePickerOpen || this->_quickMessageMenuOpen) {
			this->_battleChatHolding = false;
			this->_battleChatHintUntil = {};
		} else {
			if (down || this->keysPressed[chatKey])
				this->_battleChatHintUntil = now + std::chrono::seconds(2);
			if (down && !this->_battleChatHolding)
				this->_battleChatHoldStarted = now;
			this->_battleChatHolding = down;
			if (down && now - this->_battleChatHoldStarted >= std::chrono::milliseconds(1500)) {
				openBattleChat = true;
				this->_battleChatAwaitRelease = true;
				this->_battleChatHolding = false;
				this->_battleChatHintUntil = {};
			}
		}
	}
	if (this->_emotePickerOpen) {
		this->_updateEmotePicker();
		goto ret_reset_keysPressed;
	}
	if (this->_quickMessageMenuOpen) {
		this->_updateQuickMessageMenu();
		goto ret_reset_keysPressed;
	}
	if (this->keysPressed[VK_F3]) {
		chatPopupMode = static_cast<ChatPopupMode>((chatPopupMode + 1) % 3);
		this->_chatPopupModeTimer = 180;
		if (chatPopupMode == CHAT_POPUP_NEVER) {
			this->_editingText = false;
			this->_clearSelection();
			this->_chatOffset = 0;
			this->_chatScrolledAwayFromBottom.store(false, std::memory_order_relaxed);
			this->_chatTimer = 0;
			this->_chatSeat.tint.a = 0;
		}
		playSound(0x27);
		goto ret_reset_keysPressed;
	}
	bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
	if (this->keysPressed[VK_PRIOR]) {
		playSound(0x27);
		this->_chatOffset += SCROLL_AMOUNT;
		this->_chatScrolledAwayFromBottom.store(true, std::memory_order_relaxed);
		this->_chatTimer = max(this->_chatTimer, 180);
	}
	if (this->keysPressed[VK_NEXT]) {
		if (this->_chatOffset < SCROLL_AMOUNT)
			this->_chatOffset = 0;
		else
			this->_chatOffset -= SCROLL_AMOUNT;
		this->_chatScrolledAwayFromBottom.store(this->_chatOffset != 0, std::memory_order_relaxed);
		this->_chatTimer = max(this->_chatTimer, 180);
		playSound(0x27);
	}
	if (!this->_editingText) {
		if (openBattleChat || (!blockChatInput && this->keysPressed[chatKey])) {
			this->_editingText = true;
			this->_initInputBox();
			playSound(0x28);
		}
		goto ret_reset_keysPressed;
	}
	if (ctrlDown && this->keysPressed['C']) {
		this->_copySelectionToClipboard();
		playSound(0x27);
		goto ret_reset_keysPressed;
	}
	if (ctrlDown && this->keysPressed['V']) {
		this->_pasteFromClipboard();
		goto ret_reset_keysPressed;
	}
	if (this->keysPressed[VK_TAB] && this->immComposition.empty()) {
		this->_completePrivateMessageRecipient();
		goto ret_reset_keysPressed;
	}
	if (!this->_privateMessageCompletions.empty() && this->_privateMessageCompletionTimer) {
		if (this->keysPressed[VK_UP] || this->keysPressed[VK_DOWN]) {
			std::lock_guard<std::mutex> textLock(this->_textMutex);
			this->_commandTabCycleArmed = false;
			if (this->keysPressed[VK_UP])
				this->_privateMessageCompletionIndex = (this->_privateMessageCompletionIndex + this->_privateMessageCompletions.size() - 1) % this->_privateMessageCompletions.size();
			else
				this->_privateMessageCompletionIndex = (this->_privateMessageCompletionIndex + 1) % this->_privateMessageCompletions.size();
			constexpr unsigned visibleCompletions = 10;
			if (this->_privateMessageCompletionIndex < this->_privateMessageCompletionScroll)
				this->_privateMessageCompletionScroll = this->_privateMessageCompletionIndex;
			else if (this->_privateMessageCompletionIndex >= this->_privateMessageCompletionScroll + visibleCompletions)
				this->_privateMessageCompletionScroll = this->_privateMessageCompletionIndex - visibleCompletions + 1;
			this->_applyPrivateMessageCompletion();
			playSound(0x27);
			goto ret_reset_keysPressed;
		}
		if (this->keysPressed[VK_RETURN]) {
			{
				std::lock_guard<std::mutex> textLock(this->_textMutex);
				this->_commandTabCycleArmed = false;
				this->_rememberCurrentPlayerCompletion();
				this->_applyPrivateMessageCompletion();
			}
			this->_privateMessageCompletions.clear();
			this->_privateMessageCompletionTimer = 0;
			playSound(0x28);
			goto ret_reset_keysPressed;
		}
	}
	if (this->keysPressed[VK_UP]) {
		playSound(0x27);
		this->_chatOffset += SCROLL_AMOUNT;
		this->_chatScrolledAwayFromBottom.store(true, std::memory_order_relaxed);
		this->_chatTimer = max(this->_chatTimer, 180);
		goto ret_reset_keysPressed;
	}
	if (this->keysPressed[VK_DOWN]) {
		if (this->_chatOffset < SCROLL_AMOUNT)
			this->_chatOffset = 0;
		else
			this->_chatOffset -= SCROLL_AMOUNT;
		this->_chatScrolledAwayFromBottom.store(this->_chatOffset != 0, std::memory_order_relaxed);
		this->_chatTimer = max(this->_chatTimer, 180);
		playSound(0x27);
		goto ret_reset_keysPressed;
	}
	if (this->keysPressed[VK_RETURN]) {
		if (this->immComposition.empty()) {
			if (this->_buffer.size() != 1) {
				this->_sendMessage(std::wstring{this->_buffer.begin(), this->_buffer.end() - 1});
				playSound(0x28);
			} else
				playSound(0x29);
			this->_editingText = false;
			this->_clearSelection();
			this->_chatOffset = 0;
			this->_chatScrolledAwayFromBottom.store(false, std::memory_order_relaxed);
			if (blockChatInput) {
				// The configured chat key is Enter by default. Consume the same
				// physical press after sending so it cannot start another hold.
				this->_battleChatAwaitRelease = true;
				this->_battleChatHolding = false;
				this->_battleChatHintUntil = {};
			}
		}
		goto ret_reset_keysPressed;
	}
	this->_textMutex.lock();
	if (this->immComposition.empty()) {
		if (this->keysPressed[VK_HOME]) {
			playSound(0x27);
			this->_updateTextCursor(0);
			this->_clearSelection();
		}
		if (this->keysPressed[VK_END]) {
			playSound(0x27);
			this->_updateTextCursor(this->_buffer.size() - 1);
			this->_clearSelection();
		}
		if (this->keysPressed[VK_BACK]) {
			if (this->_deleteSelection()) {
				playSound(0x27);
			} else if (this->_textCursorPosIndex != 0) {
				size_t eraseStart = this->_textCursorPosIndex - 1;
				size_t eraseCount = 1;

				if (eraseStart && IS_LOW_SURROGATE(this->_buffer[eraseStart]) && IS_HIGH_SURROGATE(this->_buffer[eraseStart - 1])) {
					eraseStart--;
					eraseCount++;
				}
				this->_buffer.erase(eraseStart, eraseCount);
				this->_updateTextCursor(eraseStart);
				this->textChanged |= 1;
				playSound(0x27);
			}
		}
		if (this->keysPressed[VK_DELETE]) {
			if (this->_deleteSelection()) {
				playSound(0x27);
			} else if (this->_textCursorPosIndex < this->_buffer.size() - 1) {
				size_t eraseCount = 1;

				if (
					IS_HIGH_SURROGATE(this->_buffer[this->_textCursorPosIndex]) &&
					this->_textCursorPosIndex + 1 < this->_buffer.size() - 1 &&
					IS_LOW_SURROGATE(this->_buffer[this->_textCursorPosIndex + 1])
				)
					eraseCount++;
				this->_buffer.erase(this->_textCursorPosIndex, eraseCount);
				playSound(0x27);
				this->textChanged |= 1;
			}
		}
		if (this->keysPressed[VK_LEFT]) {
			if (this->_hasSelection()) {
				this->_updateTextCursor((std::min)(this->_selectionStart, this->_selectionEnd));
				this->_clearSelection();
				playSound(0x27);
			} else if (this->_textCursorPosIndex != 0) {
				size_t newPosition = this->_textCursorPosIndex - 1;

				if (newPosition && IS_LOW_SURROGATE(this->_buffer[newPosition]) && IS_HIGH_SURROGATE(this->_buffer[newPosition - 1]))
					newPosition--;
				this->_updateTextCursor(newPosition);
				playSound(0x27);
			}
		}
		if (this->keysPressed[VK_RIGHT]) {
			if (this->_hasSelection()) {
				this->_updateTextCursor((std::max)(this->_selectionStart, this->_selectionEnd));
				this->_clearSelection();
				playSound(0x27);
			} else if (this->_textCursorPosIndex != this->_buffer.size() - 1) {
				size_t newPosition = this->_textCursorPosIndex + 1;

				if (
					IS_HIGH_SURROGATE(this->_buffer[this->_textCursorPosIndex]) &&
					newPosition < this->_buffer.size() - 1 &&
					IS_LOW_SURROGATE(this->_buffer[newPosition])
				)
					newPosition++;
				this->_updateTextCursor(newPosition);
				playSound(0x27);
			}
		}
		if (this->_lastPressed) {
			this->_textTimer++;
			if (this->_textTimer == 1 || (this->_textTimer > 36 && this->_textTimer % 6 == 0)) {
				std::basic_string<unsigned> s{&this->_lastPressed, &this->_lastPressed + 1};
				auto result = UTF16Encode(s);

				if (this->_hasSelection())
					this->_deleteSelection();
				if (result.size() + this->_buffer.size() <= CHAT_CHARACTER_LIMIT) {
					this->_buffer.insert(this->_buffer.begin() + this->_textCursorPosIndex, result.begin(), result.end());
					this->_updateTextCursor(this->_textCursorPosIndex + result.size());
					playSound(0x27);
					this->textChanged |= 1;
				} else
					playSound(0x29);
			}
		}
	}
	if (this->textChanged) {
		this->_refreshPrivateMessageCompletions();
		this->_updateCompositionSprite();
	}
	this->_textMutex.unlock();
	ret_reset_keysPressed:
	memset(this->keysPressed, 0, sizeof(this->keysPressed));
}

void InLobbyMenu::_processHotkeyEvents()
{
	while (!this->_hotkeyEvents.empty()) {
		auto event = this->_hotkeyEvents.front();
		this->_hotkeyEvents.pop_front();
		if (!event.pressed)
			continue;

		if (event.key == VK_F1) {
			if (this->_editingText)
				continue;
			if (this->_quickMessageMenuOpen) {
				this->_quickMessageMenuOpen = false;
				this->_emotePickerOpen = true;
				this->_normalizeEmotePickerSelection();
				playSound(0x27);
			} else if (this->_emotePickerOpen) {
				this->_emotePickerOpen = false;
				playSound(0x29);
			} else {
				this->_emotePickerOpen = true;
				this->_normalizeEmotePickerSelection();
				playSound(0x28);
			}
			continue;
		}
		if (event.key == VK_F2) {
			if (this->_editingText)
				continue;
			if (this->_emotePickerOpen) {
				this->_emotePickerOpen = false;
				this->_quickMessageMenuOpen = true;
				playSound(0x27);
			} else if (this->_quickMessageMenuOpen) {
				this->_quickMessageMenuOpen = false;
				playSound(0x29);
			} else {
				this->_quickMessageMenuOpen = true;
				playSound(0x28);
			}
			continue;
		}
		if (event.key != VK_ESCAPE)
			continue;
		if (event.mapped) {
			// Preserve the original controller/keymap behavior: B/X is only an
			// Escape substitute in the emote picker. The quick-message menu and
			// chat input merely block game input, just as before this refactor.
			if (this->_emotePickerOpen) {
				this->_emotePickerOpen = false;
				playSound(0x29);
			}
			this->_consumeEscape(event);
			continue;
		}

		if (this->_emotePickerOpen) {
			this->_emotePickerOpen = false;
			this->_consumeEscape(event);
			playSound(0x29);
		} else if (this->_quickMessageMenuOpen) {
			this->_quickMessageMenuOpen = false;
			this->_consumeEscape(event);
			playSound(0x29);
		} else if (this->_editingText) {
			this->_editingText = false;
			this->_clearSelection();
			this->_consumeEscape(event);
			playSound(0x29);
		} else if (event.escapeOwner == EscapeOwner::MOD_UI) {
			if (this->_hostlist)
				this->_hostlistExitRequested = true;
			this->_consumeEscape(event);
		} else if (event.escapeOwner == EscapeOwner::LOBBY) {
			this->_consumeEscape(event);
			if (SokuLib::sceneId == SokuLib::SCENE_TITLE && SokuLib::newSceneId == SokuLib::SCENE_TITLE)
				this->_lobbyExitRequested = true;
		}
	}
}

void InLobbyMenu::_initEmotePickerOrder()
{
	this->_emotePickerOrder.clear();
	if (lobbyData->emotes.size() <= 1)
		return;
	this->_emotePickerOrder.reserve(lobbyData->emotes.size() - 1);
	for (unsigned i = 1; i < lobbyData->emotes.size(); i++)
		this->_emotePickerOrder.push_back(i);
	std::stable_sort(this->_emotePickerOrder.begin(), this->_emotePickerOrder.end(), [](unsigned leftId, unsigned rightId) {
		const auto &left = lobbyData->emotes[leftId].alias;
		const auto &right = lobbyData->emotes[rightId].alias;

		if (left.empty() != right.empty())
			return !left.empty();
		if (left.empty())
			return leftId < rightId;
		if (naturalAliasLess(left.front(), right.front()))
			return true;
		if (naturalAliasLess(right.front(), left.front()))
			return false;
		return leftId < rightId;
	});
}

void InLobbyMenu::_normalizeEmotePickerSelection()
{
	if (this->_emotePickerOrder.empty()) {
		this->_emotePickerSelection = 0;
		return;
	}
	if (this->_emotePickerSelection >= this->_emotePickerOrder.size())
		this->_emotePickerSelection = 0;
}

void InLobbyMenu::_updateEmotePicker()
{
	this->_normalizeEmotePickerSelection();
	if (this->_emotePickerOrder.empty())
		return;

	const unsigned first = 0;
	const unsigned last = static_cast<unsigned>(this->_emotePickerOrder.size() - 1);
	const unsigned pageStart = first + ((this->_emotePickerSelection - first) / EMOTE_PICKER_PAGE_SIZE) * EMOTE_PICKER_PAGE_SIZE;
	const unsigned pageEnd = min(last, pageStart + EMOTE_PICKER_PAGE_SIZE - 1);
	unsigned next = this->_emotePickerSelection;
	const bool keyboardNavigationDown =
		(GetAsyncKeyState(VK_LEFT) & 0x8000) ||
		(GetAsyncKeyState(VK_RIGHT) & 0x8000) ||
		(GetAsyncKeyState(VK_UP) & 0x8000) ||
		(GetAsyncKeyState(VK_DOWN) & 0x8000) ||
		(GetAsyncKeyState(VK_PRIOR) & 0x8000) ||
		(GetAsyncKeyState(VK_NEXT) & 0x8000);
	const bool acceptNavigation = !this->_emotePickerNavigationHeld;

	this->_emotePickerNavigationHeld = keyboardNavigationDown;
	if (acceptNavigation && this->keysPressed[VK_LEFT])
		next = next == pageStart ? pageEnd : next - 1;
	else if (acceptNavigation && this->keysPressed[VK_RIGHT])
		next = next == pageEnd ? pageStart : next + 1;
	else if (acceptNavigation && this->keysPressed[VK_UP]) {
		if (next >= first + EMOTE_PICKER_COLUMNS)
			next -= EMOTE_PICKER_COLUMNS;
		else
			next = min(last, first + ((last - first) / EMOTE_PICKER_PAGE_SIZE) * EMOTE_PICKER_PAGE_SIZE + (next - first));
	} else if (acceptNavigation && this->keysPressed[VK_DOWN]) {
		if (next + EMOTE_PICKER_COLUMNS <= last)
			next += EMOTE_PICKER_COLUMNS;
		else
			next = first + (next - first) % EMOTE_PICKER_COLUMNS;
	} else if (acceptNavigation && this->keysPressed[VK_PRIOR])
		next = pageStart == first ? first + ((last - first) / EMOTE_PICKER_PAGE_SIZE) * EMOTE_PICKER_PAGE_SIZE : pageStart - EMOTE_PICKER_PAGE_SIZE;
	else if (acceptNavigation && this->keysPressed[VK_NEXT])
		next = pageEnd == last ? first : pageStart + EMOTE_PICKER_PAGE_SIZE;

	if (next != this->_emotePickerSelection) {
		this->_emotePickerSelection = min(next, last);
		playSound(0x27);
	}
	if (this->keysPressed[VK_RETURN]) {
		auto &emote = lobbyData->emotes[this->_emotePickerOrder[this->_emotePickerSelection]];

		if (lobbyData->isLocked(emote))
			playSound(0x29);
		else {
			this->_sendEmote(emote);
			this->_emotePickerOpen = false;
			playSound(0x28);
		}
	}
}

void InLobbyMenu::_updateQuickMessageMenu()
{
	for (unsigned i = 0; i < 9; i++) {
		if (!this->keysPressed['1' + i] && !this->keysPressed[VK_NUMPAD1 + i])
			continue;
		if (quickMessages[i].empty())
			playSound(0x29);
		else {
			this->_sendMessage(quickMessages[i]);
			this->_quickMessageMenuOpen = false;
			playSound(0x28);
		}
		return;
	}
}

void InLobbyMenu::_initQuickMessageSprites()
{
	for (unsigned i = 0; i < 9; i++) {
		std::wstring label = std::to_wstring(i + 1) + L". " + (quickMessages[i].empty() ? L"(not configured)" : quickMessages[i]);
		int textureId = 0;

		if (!createTextTexture(textureId, label.c_str(), this->_chatFont, {360, 24}, &this->_quickMessageTextSizes[i]))
			continue;
		this->_quickMessageSprites[i].texture.setHandle(textureId, {360, 24});
		this->_quickMessageSprites[i].rect.width = this->_quickMessageTextSizes[i].x;
		this->_quickMessageSprites[i].rect.height = this->_quickMessageTextSizes[i].y;
		this->_quickMessageSprites[i].setSize({
			static_cast<unsigned>(this->_quickMessageTextSizes[i].x),
			static_cast<unsigned>(this->_quickMessageTextSizes[i].y)
		});
		if (quickMessages[i].empty())
			this->_quickMessageSprites[i].tint = SokuLib::Color{0x80, 0x80, 0x80, 0xFF};
	}
}

void InLobbyMenu::_initChatPopupModeSprites()
{
	const wchar_t *labels[3] = {
		chineseLanguage ? L"\u804A\u5929\u5F39\u51FA\uFF1A\u6240\u6709\u4EBA" : L"Chat popup: All players",
		chineseLanguage ? L"\u804A\u5929\u5F39\u51FA\uFF1A\u4EC5\u5BF9\u6218\u73A9\u5BB6" : L"Chat popup: Battle players only",
		chineseLanguage ? L"\u804A\u5929\u5F39\u51FA\uFF1A\u4ECE\u4E0D" : L"Chat popup: Never"
	};
	for (unsigned i = 0; i < 3; i++) {
		int textureId = 0;
		SokuLib::Vector2i size;
		if (!createTextTexture(textureId, labels[i], this->_textBubbleFont, {300, 28}, &size, true))
			continue;
		this->_chatPopupModeSprites[i].texture.setHandle(textureId, {300, 28});
		this->_chatPopupModeSprites[i].rect = {0, 0, size.x, size.y};
		this->_chatPopupModeSprites[i].setSize(size.to<unsigned>());
	}
}

void InLobbyMenu::_initInputBox()
{
	int ret;

	playSound(0x28);
	memset(this->keysPressed, 0, sizeof(this->keysPressed));
	this->_lastPressed = 0;
	this->_textTimer = 0;
	this->_buffer.clear();
	this->_buffer.push_back(0);
	this->_clearSelection();
	this->_privateMessageCompletions.clear();
	this->_privateMessageCompletionTimer = 0;

	this->textChanged = 3;
	this->_updateCompositionSprite();

	this->_textCursor.setPosition({CURSOR_STARTX, CURSOR_STARTY});
	this->_textCursorPosSize = 0;
	this->_textCursorPosIndex = 0;
	this->_textSprite[0].rect.left = 0;
	this->_editingText = true;

	CANDIDATEFORM candidate;
	RECT rect;

	GetWindowRect(SokuLib::window, &rect);

	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;
	float xRatio = width / 640.f;
	float yRatio = height / 480.f;
	POINT result = {
		static_cast<LONG>(this->_textCursor.getPosition().x * xRatio),
		static_cast<LONG>(this->_textCursor.getPosition().y * yRatio + this->_textCursor.getSize().y)
	};

	candidate.dwIndex = 0;
	candidate.dwStyle = CFS_CANDIDATEPOS;
	candidate.ptCurrentPos = result;
	if (this->immCtx)
		ImmSetCandidateWindow(this->immCtx, &candidate);
}

void InLobbyMenu::_clearSelection()
{
	this->_selectionStart = -1;
	this->_selectionEnd = -1;
}

bool InLobbyMenu::_hasSelection() const
{
	return this->_selectionStart >= 0 && this->_selectionEnd >= 0 && this->_selectionStart != this->_selectionEnd;
}

bool InLobbyMenu::_deleteSelection()
{
	if (!this->_hasSelection())
		return false;

	int realLen = this->_buffer.size() > 0 ? this->_buffer.size() - 1 : 0;
	int start = (std::min)(this->_selectionStart, this->_selectionEnd);
	int end = (std::max)(this->_selectionStart, this->_selectionEnd);
	if (start < 0)
		start = 0;
	if (end < 0)
		end = 0;
	if (start > realLen)
		start = realLen;
	if (end > realLen)
		end = realLen;

	if (start == end) {
		this->_clearSelection();
		return false;
	}

	this->_buffer.erase(this->_buffer.begin() + start, this->_buffer.begin() + end);
	if (this->_buffer.empty() || this->_buffer.back() != 0)
		this->_buffer.push_back(0);
	this->_updateTextCursor(start);
	this->_clearSelection();
	this->textChanged |= 1;
	return true;
}

std::wstring InLobbyMenu::_getSelectedText() const
{
	if (!this->_hasSelection())
		return {};

	int realLen = this->_buffer.size() > 0 ? this->_buffer.size() - 1 : 0;
	int start = (std::min)(this->_selectionStart, this->_selectionEnd);
	int end = (std::max)(this->_selectionStart, this->_selectionEnd);
	if (start < 0)
		start = 0;
	if (end < 0)
		end = 0;
	if (start > realLen)
		start = realLen;
	if (end > realLen)
		end = realLen;

	return std::wstring{this->_buffer.begin() + start, this->_buffer.begin() + end};
}

void InLobbyMenu::_copySelectionToClipboard()
{
	std::lock_guard<std::mutex> textLock(this->_textMutex);
	if (this->_buffer.empty())
		return;
	std::wstring text = this->_hasSelection() ? this->_getSelectedText() : std::wstring{this->_buffer.begin(), this->_buffer.end() - 1};

	if (text.empty())
		return;
	if (!OpenClipboard(SokuLib::window))
		return;
	EmptyClipboard();
	size_t bytes = (text.size() + 1) * sizeof(wchar_t);
	HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);

	if (!mem) {
		CloseClipboard();
		return;
	}
	wchar_t *data = static_cast<wchar_t *>(GlobalLock(mem));
	if (!data) {
		GlobalFree(mem);
		CloseClipboard();
		return;
	}
	memcpy(data, text.c_str(), bytes);
	GlobalUnlock(mem);
	SetClipboardData(CF_UNICODETEXT, mem);
	CloseClipboard();
}

void InLobbyMenu::_pasteFromClipboard()
{
	std::lock_guard<std::mutex> textLock(this->_textMutex);
	if (!OpenClipboard(SokuLib::window)) {
		playSound(0x29);
		return;
	}
	HANDLE handle = GetClipboardData(CF_UNICODETEXT);
	if (!handle) {
		CloseClipboard();
		playSound(0x29);
		return;
	}
	wchar_t *data = static_cast<wchar_t *>(GlobalLock(handle));
	if (!data) {
		CloseClipboard();
		playSound(0x29);
		return;
	}
	std::wstring clip = data;
	GlobalUnlock(handle);
	CloseClipboard();

	clip.erase(std::remove(clip.begin(), clip.end(), L'\r'), clip.end());
	std::replace(clip.begin(), clip.end(), L'\n', L' ');
	int realLen = this->_buffer.size() > 0 ? this->_buffer.size() - 1 : 0;
	int selectionLen = this->_hasSelection() ? std::abs(this->_selectionEnd - this->_selectionStart) : 0;
	int spaceLeft = CHAT_CHARACTER_LIMIT - 1 - (realLen - selectionLen);

	if (spaceLeft <= 0 || clip.empty()) {
		playSound(0x29);
		return;
	}
	if (clip.size() > static_cast<size_t>(spaceLeft))
		clip.resize(spaceLeft);
	this->_deleteSelection();
	this->_buffer.insert(this->_buffer.begin() + this->_textCursorPosIndex, clip.begin(), clip.end());
	this->_privateMessageCompletions.clear();
	this->_privateMessageCompletionTimer = 0;
	this->_updateTextCursor(this->_textCursorPosIndex + clip.size());
	this->textChanged |= 1;
	playSound(0x27);
}

void InLobbyMenu::_updateTextCursor(int pos)
{
	if (pos >= CHAT_CHARACTER_LIMIT)
		pos = CHAT_CHARACTER_LIMIT - 1;

	int computedSize = getTextSize(this->_buffer.substr(0, pos).c_str(), this->_chatFont, BOX_TEXTURE_SIZE).x;
	int newX = this->_textCursor.getPosition().x + computedSize - this->_textCursorPosSize;

	if (newX > CURSOR_ENDX) {
		//TODO
		this->_textSprite[0].rect.left += newX - CURSOR_ENDX;
		this->_textCursor.setPosition({CURSOR_ENDX, CURSOR_STARTY});
	} else if (newX < CURSOR_STARTX) {
		//TODO
		this->_textSprite[0].rect.left += newX - CURSOR_STARTX;
		this->_textCursor.setPosition({CURSOR_STARTX, CURSOR_STARTY});
	} else
		this->_textCursor.setPosition({newX, CURSOR_STARTY});
	this->_textCursorPosIndex = pos;
	this->_textCursorPosSize = computedSize;

	CANDIDATEFORM candidate;
	RECT rect;

	GetWindowRect(SokuLib::window, &rect);

	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;
	float xRatio = width / 640.f;
	float yRatio = height / 480.f;
	POINT result = {
		static_cast<LONG>(this->_textCursor.getPosition().x * xRatio),
		static_cast<LONG>(this->_textCursor.getPosition().y * yRatio + this->_textCursor.getSize().y)
	};

	candidate.dwIndex = 0;
	candidate.dwStyle = CFS_CANDIDATEPOS;
	candidate.ptCurrentPos = result;
	if (this->immCtx)
		ImmSetCandidateWindow(this->immCtx, &candidate);
}

bool InLobbyMenu::_handleLocalTeleport(const std::wstring &msg)
{
	if (msg.size() < 3 || _wcsnicmp(msg.c_str(), L"/tp", 3) != 0 || (msg.size() != 3 && !iswspace(msg[3])))
		return false;

	auto showError = [this](const std::string &chinese, const std::string &english) {
		this->_addMessageToList(0xFF0000, 0, chinese + "\n" + english);
	};
	if (this->_currentMachine || SokuLib::sceneId != SokuLib::SCENE_TITLE) {
		showError("请先离开对战机或观战机再使用 /tp。", "Leave the battle or spectator machine before using /tp.");
		return true;
	}
	if (this->_currentElevator) {
		showError("在电梯内无法使用 /tp。", "You cannot use /tp while inside an elevator.");
		return true;
	}
	std::wstring argument = msg.substr(3);
	while (!argument.empty() && iswspace(argument.front()))
		argument.erase(argument.begin());
	while (!argument.empty() && iswspace(argument.back()))
		argument.pop_back();
	if (argument.empty()) {
		showError("用法：/tp <玩家>", "Usage: /tp <player>");
		return true;
	}

	const Player *target = nullptr;
	if (argument.front() == L'@') {
		try {
			auto name = convertEncoding<wchar_t, char, UTF16Decode, UTF8Encode>(argument.substr(1));
			for (const auto &[id, player] : this->_playersById)
				if (player && player->name == name) {
					target = player;
					break;
				}
		} catch (...) {
			showError("玩家名称无效。", "Invalid player name.");
			return true;
		}
	} else {
		try {
			size_t parsed = 0;
			auto id = std::stoul(argument, &parsed);
			if (parsed != argument.size() || id > UINT32_MAX)
				throw std::invalid_argument("invalid player id");
			auto found = this->_playersById.find(static_cast<uint32_t>(id));
			if (found != this->_playersById.end())
				target = found->second;
		} catch (...) {
			showError("玩家必须使用数字 ID 或准确的 @名称。", "Player must be an id or an exact @name.");
			return true;
		}
	}
	if (!target) {
		showError("找不到该玩家。", "Cannot find that player.");
		return true;
	}
	auto me = this->_connection->getMe();
	if (!me) {
		showError("你的大厅角色尚未准备好。", "Your lobby player is not ready.");
		return true;
	}
	if (target->id == me->id) {
		showError("你已经在自己的位置。", "You are already at your own position.");
		return true;
	}

	auto now = std::chrono::steady_clock::now();
	if (now < this->_nextTeleportAt) {
		auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(this->_nextTeleportAt - now);
		auto seconds = (remaining.count() + 999) / 1000;
		showError(
			"还需等待 " + std::to_string(seconds) + " 秒才能再次使用 /tp。",
			"You can use /tp again in " + std::to_string(seconds) + " seconds."
		);
		return true;
	}

	auto &background = lobbyData->backgrounds[this->_background];
	unsigned targetPlatform = static_cast<unsigned>(background.platforms.size());
	for (unsigned i = 0; i < background.platforms.size(); i++) {
		const auto &platform = background.platforms[i];
		if (
			target->pos.y == static_cast<uint32_t>(platform.pos.y) &&
			target->pos.x >= static_cast<uint32_t>(platform.pos.x) &&
			target->pos.x <= static_cast<uint32_t>(platform.pos.x + platform.width)
		) {
			targetPlatform = i;
			break;
		}
	}
	if (targetPlatform == background.platforms.size()) {
		showError("该玩家不在可到达的平台上。", "That player is not on a reachable platform.");
		return true;
	}

	this->_currentPlatform = targetPlatform;
	me->pos = target->pos;
	me->dir &= 0b10000;
	Lobbies::PacketMove move{0, me->dir};
	Lobbies::PacketPosition position{0, me->pos.x, me->pos.y, me->dir, me->battleStatus};
	this->_connection->send(&move, sizeof(move));
	this->_connection->send(&position, sizeof(position));
	this->_nextTeleportAt = now + std::chrono::minutes(1);
	this->_addMessageToList(0x00FFFF, 0, "Teleported to " + target->name + ".");
	return true;
}

bool InLobbyMenu::_handleLocalHelp(const std::wstring &msg)
{
	std::wstring command = msg;
	while (!command.empty() && iswspace(command.front()))
		command.erase(command.begin());
	while (!command.empty() && iswspace(command.back()))
		command.pop_back();
	if (chineseLanguage) {
		if (command.size() < 5 || _wcsnicmp(command.c_str(), L"/help", 5) != 0)
			return false;
		if (command.size() > 5 && !iswspace(command[5]))
			return false;

		auto addChineseHelp = [this](const wchar_t *text, unsigned color = 0xFFFF00) {
			this->_addMessageToList(
				color,
				0,
				convertEncoding<wchar_t, char, UTF16Decode, UTF8Encode>(text)
			);
		};
		if (command.size() == 5) {
			addChineseHelp(
				L"可用指令：\n"
				L"/help [指令]\n"
				L"/join <玩家>\n"
				L"/list\n"
				L"/msg <玩家> <消息>\n"
				L"/report [玩家] <原因>\n"
				L"/tp <玩家>\n"
				L"/block <玩家>\n"
				L"/unblock <玩家名>\n"
				L"/blocklist\n"
				L"玩家联想：输入玩家后使用 Tab 或上下键选择。"
			);
			return true;
		}

		auto argument = command.substr(6);
		while (!argument.empty() && iswspace(argument.front()))
			argument.erase(argument.begin());
		while (!argument.empty() && iswspace(argument.back()))
			argument.pop_back();
		if (!argument.empty() && argument.front() == L'/')
			argument.erase(argument.begin());

		const wchar_t *helpText = nullptr;
		if (_wcsicmp(argument.c_str(), L"help") == 0)
			helpText = L"/help [指令]：显示全部指令，或查看指定指令的详细帮助。\n示例：\n/help\n/help report";
		else if (_wcsicmp(argument.c_str(), L"join") == 0)
			helpText = L"/join <玩家>：加入该玩家所在的对战机。支持数字 ID 或准确的 @玩家名。\n示例：\n/join 1\n/join @PinkySmile";
		else if (_wcsicmp(argument.c_str(), L"list") == 0)
			helpText = L"/list：显示当前大厅内所有玩家的 ID 和名称。";
		else if (_wcsicmp(argument.c_str(), L"msg") == 0)
			helpText = L"/msg <玩家> <消息>：向玩家发送私聊消息。支持数字 ID 或准确的 @玩家名。\n示例：\n/msg 1 你好\n/msg @PinkySmile 你好";
		else if (_wcsicmp(argument.c_str(), L"report") == 0)
			helpText =
				L"/report [玩家] <原因>\n"
				L"举报内容只有管理员可见。\n"
				L"如举报在线玩家，可指定数字 ID 或 @玩家名。\n"
				L"如举报玩家不在大厅内，可省略玩家直接填写原因。\n"
				L"举报后请将证据发到群178884533，或私聊群管理员。\n"
				L"示例：\n"
				L"/report @PinkySmile 多次骚扰\n"
				L"/report 被举报人已经离开大厅";
		else if (_wcsicmp(argument.c_str(), L"tp") == 0)
			helpText = L"/tp <玩家>：传送到指定玩家的位置。支持数字 ID 或准确的 @玩家名；冷却时间为60秒，在对战机、观战机或电梯内无法使用。\n示例：\n/tp 1\n/tp @PinkySmile";
		else if (_wcsicmp(argument.c_str(), L"block") == 0)
			helpText = L"/block <玩家>：阻止该玩家以对战者身份连接你。支持数字 ID 或准确的 @玩家名。\n示例：\n/block 1\n/block @PinkySmile";
		else if (_wcsicmp(argument.c_str(), L"blocklist") == 0)
			helpText = L"/blocklist：查看当前黑名单。名单保存在模组目录 blocklist.dat。";
		else if (_wcsicmp(argument.c_str(), L"unblock") == 0)
			helpText = L"/unblock <玩家名>：从屏蔽名单移除玩家。联想内容来自现有屏蔽名单。\n示例：\n/unblock PinkySmile";

		if (helpText)
			addChineseHelp(helpText);
		else {
			std::wstring error = L"未知指令：" + argument + L"。使用 /help 查看可用指令。";
			addChineseHelp(error.c_str(), 0xFF0000);
		}
		return true;
	}
	if (_wcsicmp(command.c_str(), L"/help") == 0) {
		this->_addMessageToList(
			0xFFFF00,
			0,
			"Client commands:\n/tp <player>\n/block <player>\n/unblock <name>\n/blocklist\n/report [player] <reason>: The player is optional; omit it if they have left the lobby. Then send supporting evidence in QQ group 178884533 or privately message an administrator from the group.\nPlayer completion: use Tab or Up/Down after /msg, /report, /join, /tp, /block, or /unblock."
		);
		return false;
	}
	if (command.size() < 6 || _wcsnicmp(command.c_str(), L"/help", 5) != 0 || !iswspace(command[5]))
		return false;

	auto argument = command.substr(6);
	while (!argument.empty() && iswspace(argument.front()))
		argument.erase(argument.begin());
	while (!argument.empty() && iswspace(argument.back()))
		argument.pop_back();
	if (!argument.empty() && argument.front() == L'/')
		argument.erase(argument.begin());
	if (_wcsicmp(argument.c_str(), L"tp") != 0 && _wcsicmp(argument.c_str(), L"block") != 0 && _wcsicmp(argument.c_str(), L"blocklist") != 0 && _wcsicmp(argument.c_str(), L"unblock") != 0)
		return false;

	this->_addMessageToList(0xFFFF00, 0,
		_wcsicmp(argument.c_str(), L"tp") == 0
		? "/tp <player>: Teleport to a player by id or exact @name. Has a 60-second cooldown and cannot be used at a battle machine, spectator machine, or inside an elevator.\nExample:\n/tp 1\n/tp @PinkySmile"
		: _wcsicmp(argument.c_str(), L"block") == 0
		? "/block <player>: Prevent a player from connecting to you as an opponent. Use an id or exact @name."
		: _wcsicmp(argument.c_str(), L"blocklist") == 0
		? "/blocklist: Show the current blacklist. The list is stored in blocklist.dat."
		: "/unblock <name>: Remove a player from the block list. Completions come from the current block list.\nExample:\n/unblock PinkySmile"
	);
	return true;
}

bool InLobbyMenu::_handleLocalBlock(const std::wstring &msg)
{
	bool removing = msg.size() >= 8 && _wcsnicmp(msg.c_str(), L"/unblock", 8) == 0 && (msg.size() == 8 || iswspace(msg[8]));
	bool adding = msg.size() >= 6 && _wcsnicmp(msg.c_str(), L"/block", 6) == 0 && (msg.size() == 6 || iswspace(msg[6]));
	bool listing = msg.size() >= 10 && _wcsnicmp(msg.c_str(), L"/blocklist", 10) == 0 && (msg.size() == 10 || iswspace(msg[10]));
	if (!adding && !removing && !listing)
		return false;
	auto show = [this](unsigned color, const std::string &chinese, const std::string &english) {
		this->_addMessageToList(color, 0, chineseLanguage ? chinese : english);
	};
	std::wstring argument = msg.substr(listing ? 10 : removing ? 8 : 6);
	while (!argument.empty() && iswspace(argument.front()))
		argument.erase(argument.begin());
	while (!argument.empty() && iswspace(argument.back()))
		argument.pop_back();
	if (listing) {
		if (!argument.empty()) {
			show(0xFF0000, "用法：/blocklist", "Usage: /blocklist");
			return true;
		}
		auto names = Blocklist::list();
		if (names.empty())
			show(0xFFFF00, "屏蔽名单为空。", "The block list is empty.");
		else {
			std::string result = chineseLanguage ? "已屏蔽的玩家：" : "Blocked players:";
			for (const auto &name : names)
				result += "\n" + name;
			this->_addMessageToList(0xFFFF00, 0, result);
		}
		return true;
	}
	if (argument.empty()) {
		show(0xFF0000, removing ? "用法：/unblock <玩家名>" : "用法：/block <玩家>", removing ? "Usage: /unblock <name>" : "Usage: /block <player>");
		return true;
	}
	std::string name;
	try {
		if (removing) {
			std::wstring unescaped;
			unescaped.reserve(argument.size());
			for (size_t i = 0; i < argument.size(); i++) {
				if (argument[i] == L'\\' && i + 1 < argument.size())
					i++;
				unescaped += argument[i];
			}
			argument = std::move(unescaped);
		}
		if (!removing && argument.front() != L'@') {
			size_t parsed = 0;
			auto id = std::stoul(argument, &parsed);
			if (parsed != argument.size() || id > UINT32_MAX)
				throw std::invalid_argument("invalid player id");
			auto found = this->_playersById.find(static_cast<uint32_t>(id));
			if (found == this->_playersById.end() || !found->second) {
				std::lock_guard<std::mutex> lock(this->_recentOpponentMutex);
				if (this->_recentOpponent && this->_recentOpponent->playerId == id)
					name = this->_recentOpponent->playerName;
				else {
					show(0xFF0000, "找不到该玩家。", "Cannot find that player.");
					return true;
				}
			} else {
				name = found->second->name;
			}
		} else {
			if (!argument.empty() && argument.front() == L'@')
				argument.erase(argument.begin());
			name = convertEncoding<wchar_t, char, UTF16Decode, UTF8Encode>(argument);
		}
	} catch (...) {
		show(0xFF0000, "玩家必须使用数字 ID 或准确的 @名称。", "Player must be an id or an exact @name.");
		return true;
	}
	if (removing) {
		if (Blocklist::remove(name)) {
			show(0x00FFFF, "已从屏蔽名单移除：" + name, "Removed from the block list: " + name);
			this->_syncBlocklistToServer();
		} else
			show(0xFF0000, "屏蔽名单中没有：" + name, "Not found in the block list: " + name);
	} else {
		auto added = Blocklist::add(name);
		this->_syncBlocklistToServer();
		bool recentOpponent = false;
		{
			std::lock_guard<std::mutex> lock(this->_recentOpponentMutex);
			recentOpponent = this->_recentOpponent && this->_recentOpponent->playerName == name;
		}
		if (recentOpponent)
			this->_requestRecentOpponentIp();
		show(
			added ? 0x00FFFF : 0xFFFF00,
			(added ? "已屏蔽：" : "该玩家已在屏蔽名单中：") + name,
			(added ? "Blocked: " : "Already blocked: ") + name
		);
	}
	return true;
}

void InLobbyMenu::_syncBlocklistToServer()
{
	if (!this->_serverBlocklistSupported)
		return;
	auto sendCommand = [this](const std::string &command) {
		Lobbies::PacketMessage packet{0, 0, command};
		this->_connection->send(&packet, sizeof(packet));
	};
	auto encodeHex = [](const std::string &value) {
		static constexpr char digits[] = "0123456789abcdef";
		std::string result;
		result.reserve(value.size() * 2);
		for (auto c : value) {
			auto byte = static_cast<unsigned char>(c);
			result.push_back(digits[byte >> 4]);
			result.push_back(digits[byte & 0xF]);
		}
		return result;
	};
	sendCommand("/__blockreset");
	for (const auto &entry : Blocklist::entries()) {
		sendCommand("/__blockname " + encodeHex(entry.name));
		for (const auto &ip : entry.ips)
			sendCommand("/__blockipentry " + ip);
	}
	sendCommand("/__blockready");
}

void InLobbyMenu::_probeBlocklistServer()
{
	// Deployed block-enabled servers may implement synchronization and IP lookup
	// without implementing the later capability response. Optimistically use the
	// private commands; errors from genuinely old servers are hidden above.
	this->_serverBlocklistSupported = true;
	Lobbies::PacketMessage packet{0, 0, "/__blockcap"};
	this->_connection->send(&packet, sizeof(packet));
	this->_syncBlocklistToServer();
}

void InLobbyMenu::_requestRecentOpponentIp()
{
	if (!this->_serverBlocklistSupported)
		return;
	Lobbies::PacketMessage packet{0, 0, "/__blockopponentip"};
	this->_connection->send(&packet, sizeof(packet));
}

std::optional<std::string> InLobbyMenu::_getBlacklistedBattlePlayer(unsigned machineId) const
{
	unsigned battlePlayers = 0;
	const Player *blockedPlayer = nullptr;
	for (const auto &player : this->_playersCopy) {
		if (
			player.machineId != machineId ||
			(player.battleStatus != Lobbies::BATTLE_STATUS_WAITING &&
			 player.battleStatus != Lobbies::BATTLE_STATUS_PLAYING)
		)
			continue;
		battlePlayers++;
		if (Blocklist::contains(player.name))
			blockedPlayer = &player;
	}

	// Two occupied battle seats mean that pressing Z is a spectate request.
	if (battlePlayers >= 2 || !blockedPlayer)
		return std::nullopt;
	return blockedPlayer->name;
}

void InLobbyMenu::_sendMessage(const std::wstring &msg)
{
	if (this->_handleLocalHelp(msg))
		return;
	if (this->_handleLocalBlock(msg))
		return;
	if (this->_handleLocalTeleport(msg))
		return;
	std::string encoded;
	std::wstring token;
	std::wstring currentEmote;
	bool colon = false;
	bool skip = false;

	encoded.reserve(msg.size());
	token.reserve(msg.size());
	for (auto c : msg) {
		if (!skip && c == ':') {
			colon = !colon;
			if (colon)
				continue;

			auto it = lobbyData->emotesByName.find(convertEncoding<wchar_t, char, UTF16Decode, UTF8Encode>(currentEmote));

			if (it == lobbyData->emotesByName.end()) {
				token += L':';
				token += currentEmote;
				token += L':';
			} else if (lobbyData->isLocked(*it->second)) {
				this->_addMessageToList(0xAFAFAF, 0, "You can't use :" + convertEncoding<wchar_t, char, UTF16Decode, UTF8Encode>(currentEmote) + ": because you didn't unlock it.");
				token += L':';
				token += currentEmote;
				token += L':';
			} else {
				auto nb = it->second->id;

				encoded += convertEncoding<wchar_t, char, UTF16Decode, UTF8Encode>(token);
				encoded += '\x01';
				for (int i = 0; i < 2; i++) {
					encoded += static_cast<char>((nb & 0x7F) | 0x80);
					nb >>= 7;
				}
				token.clear();
			}
			currentEmote.clear();
		} else
			(colon ? currentEmote : token) += c;
	}
	encoded += convertEncoding<wchar_t, char, UTF16Decode, UTF8Encode>(token);
	if (colon) {
		encoded += ':';
		encoded += convertEncoding<wchar_t, char, UTF16Decode, UTF8Encode>(currentEmote);
	}

	size_t pos = encoded.find("bgs");

	if (
		pos != std::string::npos &&
		(pos == 0 || !isalpha(encoded[pos-1])) &&
		(pos + 3 == encoded.size() - 1 || !isalpha(encoded[pos + 3]))
	) {
		encoded.erase(encoded.begin() + pos, encoded.begin() + pos + 3);
		encoded.insert(pos, "GGs, thanks for the games. It was very nice playing with you, let's play again later");
	}

	Lobbies::PacketMessage msgPacket{0, 0, encoded};

	this->_connection->send(&msgPacket, sizeof(msgPacket));
}

void InLobbyMenu::_sendEmote(const LobbyData::Emote &emote)
{
	std::string encoded(1, '\x01');
	auto id = emote.id;

	for (int i = 0; i < 2; i++) {
		encoded += static_cast<char>((id & 0x7F) | 0x80);
		id >>= 7;
	}
	Lobbies::PacketMessage msgPacket{0, 0, encoded};

	this->_connection->send(&msgPacket, sizeof(msgPacket));
}

void InLobbyMenu::updateChat(bool inGame)
{
	this->_connection->setSpectatingScene(
		SokuLib::sceneId == SokuLib::SCENE_LOADINGWATCH ||
		SokuLib::sceneId == SokuLib::SCENE_BATTLEWATCH ||
		SokuLib::newSceneId == SokuLib::SCENE_LOADINGWATCH ||
		SokuLib::newSceneId == SokuLib::SCENE_BATTLEWATCH
	);
	if (this->_privateMessageCompletionTimer && !--this->_privateMessageCompletionTimer) {
		this->_privateMessageCompletions.clear();
		this->_commandTabCycleArmed = false;
	}
	if (this->_disconnected)
		return;
	const bool outsideLobby = SokuLib::sceneId != SokuLib::SCENE_TITLE;
	if (outsideLobby != this->_textBubblesOutsideLobby) {
		this->_clearTextBubbles();
		this->_textBubblesOutsideLobby = outsideLobby;
	}
	const bool blockChatInput =
		SokuLib::sceneId == SokuLib::SCENE_BATTLECL ||
		SokuLib::sceneId == SokuLib::SCENE_BATTLESV ||
		SokuLib::newSceneId == SokuLib::SCENE_BATTLECL ||
		SokuLib::newSceneId == SokuLib::SCENE_BATTLESV;
	this->_inputBoxUpdate(blockChatInput);
	if (this->_chatPopupModeTimer)
		this->_chatPopupModeTimer--;
	if (this->_editingText)
		this->_chatTimer = 300;
	else if (
		inGame &&
		!this->_battleOpponentChatPopup.load(std::memory_order_relaxed) &&
		!this->_errorChatPopup.load(std::memory_order_relaxed)
	)
		this->_chatTimer = this->_chatSeat.tint.a != 0;
	if (this->_chatTimer) {
		this->_chatTimer--;

		unsigned char alpha = this->_chatTimer > 120 ? 255 : (this->_chatTimer * 255 / 120);

		this->_chatSeat.tint.a = alpha;
	} else {
		this->_battleOpponentChatPopup.store(false, std::memory_order_relaxed);
		this->_errorChatPopup.store(false, std::memory_order_relaxed);
	}
}

void InLobbyMenu::_renderBattleChatHint()
{
	auto now = std::chrono::steady_clock::now();
	if (!this->_battleChatActive || this->_editingText || now >= this->_battleChatHintUntil)
		return;
	auto elapsed = this->_battleChatHolding
		? std::chrono::duration_cast<std::chrono::milliseconds>(now - this->_battleChatHoldStarted).count() : 0;
	int remaining = (std::max)(1, 2 - static_cast<int>(elapsed / 1000));
	std::wstring text = chineseLanguage
		? L"\u957F\u6309\u804A\u5929\u952E 2 \u79D2\u6253\u5F00\u5BF9\u8BDD\u6846"
		: L"Hold the chat key for 2 seconds to open chat";
	text += chineseLanguage ? L"  -  \u5269\u4F59 " : L"  -  ";
	text += std::to_wstring(remaining);
	text += chineseLanguage ? L" \u79D2" : L"s remaining";
	if (text != this->_battleChatHintText) {
		int textureId = 0;
		SokuLib::Vector2i size;
		if (createTextTexture(textureId, text.c_str(), this->_textBubbleFont, {480, 28}, &size, true)) {
			this->_battleChatHintSprite.texture.setHandle(textureId, {480, 28});
			this->_battleChatHintSprite.rect = {0, 0, size.x, size.y};
			this->_battleChatHintSprite.setSize(size.to<unsigned>());
			this->_battleChatHintSprite.setPosition({320 - size.x / 2, 374});
			this->_battleChatHintText = text;
		}
	}
	SokuLib::DrawUtils::RectangleShape panel;
	panel.setPosition({72, 366});
	panel.setSize({496, 48});
	panel.setBorderColor(SokuLib::Color{0x78, 0x80, 0x90, 0xD0});
	panel.setFillColor(SokuLib::Color{0x12, 0x16, 0x20, 0xD0});
	panel.draw();
	this->_battleChatHintSprite.draw();
	// Avoid RectangleShape's asymmetric corner offsets and outline on this thin bar.
	auto drawBar = [](unsigned width, D3DCOLOR color) {
		if (!width)
			return;
		const float left = 83.5f;
		const float top = 403.5f;
		const float right = left + width;
		SokuLib::DrawUtils::Vertex vertices[] = {
			{left, top, 0, 1, color, 0, 0},
			{right, top, 0, 1, color, 0, 0},
			{right, top + 4, 0, 1, color, 0, 0},
			{left, top + 4, 0, 1, color, 0, 0}
		};
		SokuLib::textureMgr.setTexture(0, 0);
		SokuLib::pd3dDev->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, vertices, sizeof(*vertices));
	};
	drawBar(472, 0xFF343E50);
	drawBar(static_cast<unsigned>((std::min)(1500LL, elapsed) * 472 / 1500), 0xFF7FA6D9);
}

void InLobbyMenu::renderChat()
{
	if (this->_disconnected)
		return;
	if (this->_chatSeat.tint.a) {
		this->_chatSeat.draw();
		std::list<Message> chatMessagesToRemove;

		std::lock_guard<std::mutex> lock(this->_chatMessagesMutex);
	
		auto remaining = this->_chatOffset;
		SokuLib::Vector2i pos{292, 180};

		for (auto it = this->_chatMessages.begin(); it != this->_chatMessages.end(); it++) {
			Message &msg = *it;

			if (pos.y <= 3) {
				// msg.farUp = true;
				break;
			}

			// create sprites if they haven't been created
			if (msg.lazy_message.has_value()) {
				auto m = it;
				const bool preserveScrollAnchor = msg.preserveScrollAnchor && this->_chatOffset != 0;
				unsigned generatedHeight = 0;
				std::string line;
				std::string word;
				std::string token;
				unsigned startPos = 0;
				unsigned pos = 0;
				unsigned wordPos = 0;
				unsigned skip = 0;
				unsigned short emoteId;
				unsigned char emoteCtr = 0;
				auto pushText = [&]{
					if (line.empty())
						return;

					m->text.emplace_back();

					auto &txt = m->text.back();
					int texId = 0;

					if (msg.lazy_message->colorOverride)
						txt.sprite.tint = *msg.lazy_message->colorOverride;
					else if (msg.lazy_message->player == 0)
						txt.sprite.tint = msg.lazy_message->channel;
					//txt.sprite.texture.createFromText(line.c_str(), this->_chatFont, {350, 300}, &txt.realSize);
					if (!createTextTexture(texId, convertEncoding<char, wchar_t, UTF8Decode, UTF16Encode>(line).c_str(), this->_chatFont, {350, 300}, &txt.realSize))
						puts("Error creating text texture");
					txt.sprite.texture.setHandle(texId, {350, 300});
					txt.sprite.rect.width = txt.realSize.x;
					txt.sprite.rect.height = txt.realSize.y;
					txt.pos.x = startPos;
					if (!m->emotes.empty())
						txt.pos.y = (txt.realSize.y - EMOTE_SIZE) / 2;
					startPos = pos;
					line.clear();
				};
				auto getLineHeight = [](const Message &message) {
					int height = message.emotes.empty() ? 0 : EMOTE_SIZE;
					for (const auto &text : message.text)
						height = max(height, text.realSize.y);
					return static_cast<unsigned>(height);
				};
				auto nextLine = [&]{
					pushText();
					if (preserveScrollAnchor)
						generatedHeight += getLineHeight(*m);
					m = this->_chatMessages.emplace(m);
					pos = 0;
				};
				size_t lastTokenSize = 0;

				line.reserve(msg.lazy_message->msg.size());
				word.reserve(msg.lazy_message->msg.size());
				for (unsigned char c : msg.lazy_message->msg) {
					if (emoteCtr) {
						emoteId |= (c & 0x7F) << ((2 - emoteCtr) * 7);
						emoteCtr--;
						if (!emoteCtr) {
							if (pos + EMOTE_SIZE > MAX_LINE_SIZE) {
								nextLine();
								startPos = 0;
							}
							m->emotes.emplace_back();
							if (emoteId >= lobbyData->emotes.size())
								printf("Received invalid emote! %u < %u\n", emoteId, lobbyData->emotes.size());
							m->emotes.back().id = emoteId;
							m->emotes.back().pos.x = startPos;
							pos += EMOTE_SIZE;
							startPos = pos;
							for (auto &g : m->text)
								g.pos.y = (g.realSize.y - EMOTE_SIZE) / 2;
						}
					} else if (skip) {
						if ((c & 0b11000000) == 0x80) {
							skip--;
							token += c;
						} else
							skip = 0;
						word += c;
						if (skip != 0)
							continue;
						lastTokenSize = token.size();
						wordPos += this->_getTextSize(UTF8Decode(token)[0]);
					} else if (c == 1) {
						line += word;
						pos += wordPos;
						wordPos = 0;
						word.clear();
						pushText();
						emoteId = 0;
						emoteCtr = 2;
						continue;
					} else if (c == '\n') {
						line += word;
						pos += wordPos;
						word.clear();
						wordPos = 0;
						nextLine();
						startPos = 0;
						continue;
					} else if (c >= 0x80) {
						skip = c >= 0xC0;
						skip += c >= 0xE0;
						skip += c >= 0xF0;
						token.clear();
						token += c;
						word += c;
						continue;
					} else if (isspace(c)) {
						if (word.empty()) {
							if (pos == 0)
								continue;
						} else {
							line += word;
							pos += wordPos;
							word.clear();
							wordPos = 0;
						}
						line += ' ';
						lastTokenSize = 1;
						pos += this->_getTextSize(' ');
					} else {
						lastTokenSize = 1;
						word += c;
						wordPos += this->_getTextSize(c);
					}
					if (pos + wordPos > MAX_LINE_SIZE) {
						if (pos == 0) {
							line = word.substr(0, word.size() - lastTokenSize);
							word.erase(word.begin(), word.end() - lastTokenSize);
							wordPos = this->_getTextSize(UTF8Decode(word)[0]);
						}
						nextLine();
						startPos = 0;
					}
				}
				line += word;
				pushText();
				if (preserveScrollAnchor) {
					generatedHeight += getLineHeight(*m);
					this->_chatOffset += generatedHeight;
					remaining += generatedHeight;
				}
				msg.preserveScrollAnchor = false;
				msg.lazy_message.reset();
			}

			int maxSize = msg.emotes.empty() ? 0 : EMOTE_SIZE;

			// msg.farUp = false;
			bool farDown = true;
			for (auto &text : msg.text) {
				if (remaining <= text.realSize.y - text.pos.y) {
					auto p = pos;

					if (!remaining)
						p += text.pos;
					else
						p.y += min(0, text.pos.y + static_cast<int>(remaining));
					this->_updateMessageSprite(p, remaining < -text.pos.y ? 0 : remaining + text.pos.y, text.realSize, text.sprite, this->_chatSeat.tint.a);
					farDown = false;
				} else
					text.sprite.tint.a = 0;
				maxSize = max(maxSize, text.realSize.y);
			}
			for (auto &emote : msg.emotes) {
				emote.offset = pos;
				emote.cutRemain = remaining;
			}
			if (remaining <= EMOTE_SIZE && !msg.emotes.empty())
				farDown = false;
			if (remaining > maxSize)
				remaining -= maxSize;
			else {
				pos.y -= maxSize - remaining;
				remaining = 0;
			}

			if (farDown)
				continue;
			for (auto &text : msg.text) {
				//SokuLib::SpriteEx s;
				//auto handle = text.sprite.texture.releaseHandle();

				//text.sprite.texture.setHandle(handle, text.sprite.texture.getSize());
				//s.setTexture(
				//	handle,
				//	text.sprite.rect.left,
				//	text.sprite.rect.top,
				//	text.sprite.rect.width,
				//	text.sprite.rect.height,
				//	0, 0
				//);
				//s.loadTransform();
				//s.translate(text.sprite.getPosition().x, text.sprite.getPosition().y, 0);
				//s.saveTransform();
				//for (auto &c : s.vertices)
				//	c.color = text.sprite.tint;
				//reinterpret_cast<void(__fastcall*)(int, int, int)>(0x404b80)(0x896b4c, 0, 2);
				//s.render();
				//reinterpret_cast<void(__fastcall*)(int, int, int)>(0x404b80)(0x896b4c, 0, 1);
				text.sprite.draw();
			}
			for (auto &emote : msg.emotes) {
				auto &emoteObj = lobbyData->emotes[emote.id < lobbyData->emotes.size() ? emote.id : 0];
				auto pos = emote.pos + emote.offset;

				emoteObj.sprite.tint.a = this->_chatSeat.tint.a;
				emoteObj.sprite.rect.top = 0;
				emoteObj.sprite.rect.height = EMOTE_SIZE - emote.cutRemain;
				pos.y -= emoteObj.sprite.rect.height;
				if (pos.y < 3) {
					emoteObj.sprite.rect.height -= 3 - pos.y;
					emoteObj.sprite.rect.top = 3 - pos.y;
					pos.y = 3;
				}
				emoteObj.sprite.setSize({EMOTE_SIZE, static_cast<unsigned int>(emoteObj.sprite.rect.height)});
				emoteObj.sprite.setPosition(pos);
				emoteObj.sprite.draw();
			}
		}

		if (this->_chatMessages.size() > maxChatMessages) {
			// Move those very old message into tmpChatMessages, so that they will be destructed with tmpChatMessages,
			// after the mutex is unlocked.
			// As a result, some D3D9 operations which might block will run outside the lock of _chatMessages.
			auto index = this->_chatMessages.end();
			size_t toRemoveCount = this->_chatMessages.size() - maxChatMessages;
			for (size_t i = 0; i < toRemoveCount; i++)
				index--;
			chatMessagesToRemove.splice(chatMessagesToRemove.end(), this->_chatMessages, index, this->_chatMessages.end());
		}
	}
	if (this->_editingText) {
		this->_renderPrivateMessageCompletions();
		for (auto &sprite : this->_textSprite) {
			//SokuLib::SpriteEx s;
			//auto handle = sprite.texture.releaseHandle();
	
			//sprite.texture.setHandle(handle, sprite.texture.getSize());
			//s.setTexture(
			//	handle,
			//	sprite.rect.left,
			//	sprite.rect.top,
			//	sprite.rect.width,
			//	sprite.rect.height,
			//	0, 0
			//);
			//s.loadTransform();
			//s.translate(sprite.getPosition().x, sprite.getPosition().y, 0);
			//s.saveTransform();
			//for (auto &c : s.vertices)
			//	c.color = sprite.tint;
			//reinterpret_cast<void(__fastcall*)(int, int, int)>(0x404b80)(0x896b4c, 0, 2);
			//s.render();
			//reinterpret_cast<void(__fastcall*)(int, int, int)>(0x404b80)(0x896b4c, 0, 1);
			sprite.draw();
		}
		this->_textCursor.draw();
	}
	if (this->_emotePickerOpen)
		this->_renderEmotePicker();
	if (this->_quickMessageMenuOpen)
		this->_renderQuickMessageMenu();
	if (this->_chatPopupModeTimer)
		this->_renderChatPopupMode();
	this->_renderBattleChatHint();
}

void InLobbyMenu::_renderEmotePicker()
{
	if (this->_emotePickerOrder.empty())
		return;

	constexpr int cellSize = 38;
	constexpr int panelY = 136;
	constexpr int padding = 8;
	constexpr int panelWidth = padding * 2 + EMOTE_PICKER_COLUMNS * cellSize;
	constexpr int panelHeight = padding * 2 + EMOTE_PICKER_ROWS * cellSize + 24;
	constexpr int panelX = (640 - panelWidth) / 2;
	SokuLib::DrawUtils::RectangleShape panel;
	SokuLib::DrawUtils::RectangleShape selection;

	panel.setPosition({panelX, panelY});
	panel.setSize({panelWidth, panelHeight});
	panel.setBorderColor(SokuLib::Color{0x80, 0x88, 0x98, 0xFF});
	panel.setFillColor(SokuLib::Color{0x12, 0x16, 0x20, 0xE8});
	panel.draw();

	const unsigned first = 0;
	const unsigned last = static_cast<unsigned>(this->_emotePickerOrder.size() - 1);
	const unsigned pageStart = first + ((this->_emotePickerSelection - first) / EMOTE_PICKER_PAGE_SIZE) * EMOTE_PICKER_PAGE_SIZE;
	const unsigned pageEnd = min(last, pageStart + EMOTE_PICKER_PAGE_SIZE - 1);

	for (unsigned i = pageStart; i <= pageEnd; i++) {
		auto &emote = lobbyData->emotes[this->_emotePickerOrder[i]];
		unsigned cell = i - pageStart;
		SokuLib::Vector2i pos{
			panelX + padding + static_cast<int>(cell % EMOTE_PICKER_COLUMNS) * cellSize + 3,
			panelY + padding + static_cast<int>(cell / EMOTE_PICKER_COLUMNS) * cellSize + 3
		};

		emote.sprite.rect.left = 0;
		emote.sprite.rect.top = 0;
		emote.sprite.rect.width = EMOTE_SIZE;
		emote.sprite.rect.height = EMOTE_SIZE;
		emote.sprite.setSize({EMOTE_SIZE, EMOTE_SIZE});
		emote.sprite.setPosition(pos);
		emote.sprite.tint = lobbyData->isLocked(emote)
			? SokuLib::Color{0x70, 0x70, 0x70, 0xA0}
			: SokuLib::Color::White;
		emote.sprite.draw();
	}

	unsigned selectedCell = this->_emotePickerSelection - pageStart;
	selection.setPosition({
		panelX + padding + static_cast<int>(selectedCell % EMOTE_PICKER_COLUMNS) * cellSize,
		panelY + padding + static_cast<int>(selectedCell / EMOTE_PICKER_COLUMNS) * cellSize
	});
	selection.setSize({cellSize, cellSize});
	auto &selected = lobbyData->emotes[this->_emotePickerOrder[this->_emotePickerSelection]];
	selection.setBorderColor(lobbyData->isLocked(selected)
		? SokuLib::Color{0xB0, 0x58, 0x58, 0xFF}
		: SokuLib::Color{0x78, 0xB8, 0xD0, 0xFF});
	selection.setFillColor(SokuLib::Color{0, 0, 0, 0});
	selection.draw();

	std::ostringstream label;
	label << (selected.alias.empty() ? "(no alias)" : ":" + selected.alias.front() + ":")
		<< (lobbyData->isLocked(selected) ? "  [locked]" : "")
		<< "    " << ((pageStart - first) / EMOTE_PICKER_PAGE_SIZE + 1)
		<< "/" << ((last - first) / EMOTE_PICKER_PAGE_SIZE + 1);
	SokuLib::DrawUtils::Sprite text;

	text.texture.createFromText(label.str().c_str(), this->_chatFont, {panelWidth - padding * 2, 20});
	text.rect.width = text.texture.getSize().x;
	text.rect.height = text.texture.getSize().y;
	text.setSize(text.texture.getSize());
	text.setPosition({panelX + padding, panelY + padding + EMOTE_PICKER_ROWS * cellSize + 2});
	text.draw();
}

void InLobbyMenu::_renderQuickMessageMenu()
{
	constexpr int panelX = 132;
	constexpr int panelY = 82;
	constexpr int panelWidth = 376;
	constexpr int panelHeight = 303;
	constexpr int padding = 12;
	SokuLib::DrawUtils::RectangleShape panel;

	panel.setPosition({panelX, panelY});
	panel.setSize({panelWidth, panelHeight});
	panel.setBorderColor(SokuLib::Color{0x80, 0x88, 0x98, 0xFF});
	panel.setFillColor(SokuLib::Color{0x12, 0x16, 0x20, 0xE8});
	panel.draw();
	for (unsigned i = 0; i < 9; i++) {
		this->_quickMessageSprites[i].setPosition({
			panelX + padding,
			panelY + padding + static_cast<int>(i) * 31
		});
		this->_quickMessageSprites[i].draw();
	}
}

void InLobbyMenu::_renderChatPopupMode()
{
	auto &text = this->_chatPopupModeSprites[chatPopupMode];
	unsigned char alpha = this->_chatPopupModeTimer > 30 ? 255 : static_cast<unsigned char>(this->_chatPopupModeTimer * 255 / 30);
	constexpr int width = 316;
	constexpr int height = 42;
	constexpr int x = (640 - width) / 2;
	constexpr int y = 28;
	SokuLib::DrawUtils::RectangleShape panel;

	panel.setPosition({x, y});
	panel.setSize({width, height});
	panel.setBorderColor(SokuLib::Color{0x78, 0x80, 0x90, alpha});
	panel.setFillColor(SokuLib::Color{0x12, 0x16, 0x20, static_cast<unsigned char>(alpha * 0.9f)});
	panel.draw();
	text.tint = SokuLib::Color{0xFF, 0xFF, 0xFF, alpha};
	text.setPosition({x + 8, y + 7});
	text.draw();
}

bool InLobbyMenu::isInputing()
{
	return this->_editingText || this->_emotePickerOpen || this->_quickMessageMenuOpen;
}

bool InLobbyMenu::isEmotePickerOpen() const
{
	return this->_emotePickerOpen;
}

void InLobbyMenu::routePendingHotkeys()
{
	if (GetForegroundWindow() != SokuLib::window)
		return;
	std::lock_guard<std::mutex> lock(this->keyTimersMutex);
	this->_processHotkeyEvents();
}

void InLobbyMenu::_setEscapeSource(bool &source, uint64_t &generation, bool down, bool mapped)
{
	if (source == down)
		return;
	source = down;
	if (!down)
		return;
	generation = ++this->_nextEscapeGeneration;
	auto owner = mapped ? EscapeOwner::MOD_UI : this->_classifyEscapeOwner();
	if (!mapped) {
		this->_keyboardEscapeOwner = owner;
		this->_keyboardEscapeScene = SokuLib::sceneId;
	}
	this->_hotkeyEvents.push_back({VK_ESCAPE, true, mapped, generation, owner, SokuLib::sceneId});
}

void InLobbyMenu::_completePrivateMessageRecipient()
{
	std::lock_guard<std::mutex> textLock(this->_textMutex);
	size_t targetStart;
	size_t targetEnd;
	bool appendSpace;
	if (!this->_getPlayerCompletionTarget(targetStart, targetEnd, appendSpace)) {
		this->_commandTabCycleArmed = false;
		this->_privateMessageCompletions.clear();
		this->_privateMessageCompletionTimer = 0;
		playSound(0x29);
		return;
	}
	if (this->_privateMessageCompletions.empty())
		this->_refreshPrivateMessageCompletions();
	if (this->_privateMessageCompletions.empty()) {
		this->_commandTabCycleArmed = false;
		this->_privateMessageCompletionTimer = 0;
		playSound(0x29);
		return;
	}
	const bool commandCompletion = this->_privateMessageCompletions[this->_privateMessageCompletionIndex].command;
	if (commandCompletion && this->_commandTabCycleArmed) {
		this->_privateMessageCompletionIndex = (this->_privateMessageCompletionIndex + 1) % this->_privateMessageCompletions.size();
		constexpr unsigned visibleCompletions = 10;
		if (this->_privateMessageCompletionIndex < this->_privateMessageCompletionScroll)
			this->_privateMessageCompletionScroll = this->_privateMessageCompletionIndex;
		else if (this->_privateMessageCompletionIndex >= this->_privateMessageCompletionScroll + visibleCompletions)
			this->_privateMessageCompletionScroll = this->_privateMessageCompletionIndex - visibleCompletions + 1;
	}
	this->_rememberCurrentPlayerCompletion();
	this->_applyPrivateMessageCompletion();
	this->_commandTabCycleArmed = commandCompletion;
	if (!commandCompletion) {
		this->_privateMessageCompletions.clear();
		this->_privateMessageCompletionTimer = 0;
	}
	playSound(0x27);
}

bool InLobbyMenu::_getPlayerCompletionTarget(size_t &targetStart, size_t &targetEnd, bool &appendSpace) const
{
	if (this->_buffer.empty())
		return false;
	std::wstring text(this->_buffer.begin(), this->_buffer.end() - 1);
	struct CommandPrefix {
		const wchar_t *text;
		bool appendSpace;
	};
	static constexpr CommandPrefix prefixes[] = {
		{L"/msg ", true},
		{L"/report ", true},
		{L"/join ", false},
		{L"/tp ", false},
		{L"/block ", false},
		{L"/unblock ", false},
	};
	for (const auto &prefix : prefixes) {
		targetStart = wcslen(prefix.text);
		if (text.compare(0, targetStart, prefix.text) != 0)
			continue;
		bool escaped = false;
		for (targetEnd = targetStart; targetEnd < text.size(); targetEnd++) {
			if (escaped) {
				escaped = false;
				continue;
			}
			if (text[targetEnd] == L'\\') {
				escaped = true;
				continue;
			}
			if (iswspace(text[targetEnd]))
				break;
		}
		// A report containing more text without an explicit @name is the
		// reason-only form, not a player completion target.
		if (
			wcscmp(prefix.text, L"/report ") == 0 &&
			targetEnd < text.size() &&
			(targetStart >= text.size() || text[targetStart] != L'@')
		)
			return false;
		appendSpace = prefix.appendSpace;
		return this->_textCursorPosIndex >= static_cast<int>(targetStart);
	}
	if (!text.empty() && text.front() == L'/' && text.find_first_of(L" \t\r\n", 1) == std::wstring::npos) {
		targetStart = 1;
		targetEnd = text.size();
		appendSpace = false;
		return this->_textCursorPosIndex >= 1 && this->_textCursorPosIndex <= static_cast<int>(targetEnd);
	}
	return false;
}

void InLobbyMenu::_refreshPrivateMessageCompletions()
{
	this->_commandTabCycleArmed = false;
	size_t targetStart;
	size_t targetEnd;
	bool appendSpace;
	if (!this->_getPlayerCompletionTarget(targetStart, targetEnd, appendSpace) || this->_textCursorPosIndex > static_cast<int>(targetEnd)) {
		this->_privateMessageCompletions.clear();
		this->_privateMessageCompletionTimer = 0;
		return;
	}
	std::wstring text(this->_buffer.begin(), this->_buffer.end() - 1);
	const bool commandCompletion = targetStart == 1 && !text.empty() && text.front() == L'/';
	const bool unblockCommand = text.compare(0, wcslen(L"/unblock "), L"/unblock ") == 0;
	std::optional<RecentOpponent> recentOpponent;
	{
		std::lock_guard<std::mutex> lock(this->_recentOpponentMutex);
		recentOpponent = this->_recentOpponent;
	}
	const size_t recentSelectionLimit = recentOpponent ? 2 : 3;
	std::wstring query = text.substr(targetStart, targetEnd - targetStart);
	if (!query.empty() && query.front() == L'@')
		query.erase(query.begin());
	std::transform(query.begin(), query.end(), query.begin(), towlower);
	auto fuzzyMatch = [](const std::wstring &value, const std::wstring &needle) {
		auto next = needle.begin();
		for (wchar_t chr : value)
			if (next != needle.end() && chr == *next)
				next++;
		return next == needle.end();
	};
	this->_privateMessageCompletions.clear();
	if (commandCompletion) {
		static constexpr const wchar_t *commands[]{
			L"help", L"join", L"list", L"msg", L"report", L"tp", L"block", L"blocklist", L"unblock"
		};
		for (const auto *command : commands) {
			std::wstring name = command;
			auto lowered = name;
			std::transform(lowered.begin(), lowered.end(), lowered.begin(), towlower);
			if (!query.empty() && lowered.compare(0, query.size(), query) != 0)
				continue;
			this->_privateMessageCompletions.push_back({0, std::move(name), false, true, -1, {}});
		}
	} else if (unblockCommand) {
		for (const auto &blockedName : Blocklist::list()) {
			std::wstring name;
			try {
				name = convertEncoding<char, wchar_t, UTF8Decode, UTF16Encode>(blockedName);
			} catch (...) {
				continue;
			}
			std::wstring lowered = name;
			std::transform(lowered.begin(), lowered.end(), lowered.begin(), towlower);
			if (!query.empty() && !fuzzyMatch(lowered, query))
				continue;
			int recentSelectionRank = -1;
			for (size_t i = 0; i < this->_recentPlayerSelections.size() && i < recentSelectionLimit; i++)
				if (this->_recentPlayerSelections[i].playerName == blockedName) {
					recentSelectionRank = static_cast<int>(i);
					break;
				}
			this->_privateMessageCompletions.push_back({0, std::move(name), false, false, recentSelectionRank, {}});
		}
	} else {
		auto me = this->_connection->getMe();
		if (recentOpponent && recentOpponent->playerId && !recentOpponent->playerName.empty()) {
			try {
				auto name = convertEncoding<char, wchar_t, UTF8Decode, UTF16Encode>(recentOpponent->playerName);
				auto lowered = name;
				std::transform(lowered.begin(), lowered.end(), lowered.begin(), towlower);
				auto idText = std::to_wstring(recentOpponent->playerId);
				if (query.empty() || fuzzyMatch(lowered, query) || idText.find(query) == 0) {
					int recentSelectionRank = -1;
					for (size_t i = 0; i < this->_recentPlayerSelections.size() && i < recentSelectionLimit; i++)
						if (
							this->_recentPlayerSelections[i].playerId == recentOpponent->playerId ||
							this->_recentPlayerSelections[i].playerName == recentOpponent->playerName
						) {
							recentSelectionRank = static_cast<int>(i);
							break;
						}
					this->_privateMessageCompletions.push_back({
						recentOpponent->playerId,
						std::move(name),
						true,
						false,
						recentSelectionRank,
						{}
					});
				}
			} catch (...) {
			}
		}
		for (const auto &[id, player] : this->_playersById) {
			if (!id || !player || player->name.empty() || (me && id == me->id) || (recentOpponent && id == recentOpponent->playerId))
				continue;
			std::wstring name;
			try {
				name = convertEncoding<char, wchar_t, UTF8Decode, UTF16Encode>(player->name);
			} catch (...) {
				continue;
			}
			std::wstring lowered = name;
			std::transform(lowered.begin(), lowered.end(), lowered.begin(), towlower);
			auto idText = std::to_wstring(id);
			if (!query.empty() && !fuzzyMatch(lowered, query) && idText.find(query) != 0)
				continue;
			int recentSelectionRank = -1;
			for (size_t i = 0; i < this->_recentPlayerSelections.size() && i < recentSelectionLimit; i++)
				if (
					this->_recentPlayerSelections[i].playerId == id ||
					this->_recentPlayerSelections[i].playerName == player->name
				) {
					recentSelectionRank = static_cast<int>(i);
					break;
				}
			this->_privateMessageCompletions.push_back({id, std::move(name), false, false, recentSelectionRank, {}});
		}
	}
	std::sort(this->_privateMessageCompletions.begin(), this->_privateMessageCompletions.end(), [&query](const auto &left, const auto &right) {
		if (left.recentSelectionRank != right.recentSelectionRank) {
			if (left.recentSelectionRank < 0)
				return false;
			if (right.recentSelectionRank < 0)
				return true;
			return left.recentSelectionRank < right.recentSelectionRank;
		}
		if (left.recentOpponent != right.recentOpponent)
			return left.recentOpponent;
		auto rank = [&query](const std::wstring &name) {
			std::wstring lowered = name;
			std::transform(lowered.begin(), lowered.end(), lowered.begin(), towlower);
			if (lowered.find(query) == 0)
				return 0;
			if (lowered.find(query) != std::wstring::npos)
				return 1;
			return 2;
		};
		auto leftRank = rank(left.playerName);
		auto rightRank = rank(right.playerName);
		return leftRank != rightRank ? leftRank < rightRank : left.playerName < right.playerName;
	});
	this->_privateMessageCompletionIndex = 0;
	this->_privateMessageCompletionScroll = 0;
	for (auto &entry : this->_privateMessageCompletions) {
		auto label = commandCompletion
			? L"/" + entry.playerName
			: unblockCommand ? entry.playerName : entry.playerName + L"  (#" + std::to_wstring(entry.playerId) + L")";
		if (entry.recentSelectionRank >= 0)
			label += chineseLanguage ? L"  [最近选择]" : L"  [Recent]";
		else if (entry.recentOpponent)
			label += chineseLanguage ? L"  [最近对手]" : L"  [Recent opponent]";
		int textureId = 0;
		SokuLib::Vector2i size;
		if (createTextTexture(textureId, label.c_str(), this->_textBubbleFont, {300, 22}, &size, true)) {
			entry.label.texture.setHandle(textureId, {300, 22});
			entry.label.rect = {0, 0, size.x, size.y};
			entry.label.setSize(size.to<unsigned>());
		}
	}
	this->_privateMessageCompletionTimer = this->_privateMessageCompletions.empty() ? 0 : 240;
}

void InLobbyMenu::_rememberCurrentPlayerCompletion()
{
	if (this->_privateMessageCompletions.empty())
		return;
	const auto &completion = this->_privateMessageCompletions[this->_privateMessageCompletionIndex];
	if (completion.command)
		return;
	std::string name;
	try {
		name = convertEncoding<wchar_t, char, UTF16Decode, UTF8Encode>(completion.playerName);
	} catch (...) {
		return;
	}
	this->_recentPlayerSelections.erase(
		std::remove_if(
			this->_recentPlayerSelections.begin(),
			this->_recentPlayerSelections.end(),
			[&](const RecentPlayerSelection &entry) {
				return entry.playerName == name || (completion.playerId && entry.playerId == completion.playerId);
			}
		),
		this->_recentPlayerSelections.end()
	);
	this->_recentPlayerSelections.push_front({completion.playerId, std::move(name)});
	bool hasRecentOpponent = false;
	{
		std::lock_guard<std::mutex> lock(this->_recentOpponentMutex);
		hasRecentOpponent = this->_recentOpponent.has_value();
	}
	const size_t limit = hasRecentOpponent ? 2 : 3;
	while (this->_recentPlayerSelections.size() > limit)
		this->_recentPlayerSelections.pop_back();
	this->_saveRecentPlayerSelections();
}

void InLobbyMenu::_applyPrivateMessageCompletion()
{
	if (this->_privateMessageCompletions.empty())
		return;
	size_t targetStart;
	size_t targetEnd;
	bool appendSpace;
	if (!this->_getPlayerCompletionTarget(targetStart, targetEnd, appendSpace))
		return;
	const auto &completion = this->_privateMessageCompletions[this->_privateMessageCompletionIndex];
	std::wstring replacement;
	std::wstring currentText(this->_buffer.begin(), this->_buffer.end() - 1);
	if (completion.command) {
		replacement = completion.playerName;
	}
	else if (currentText.compare(0, wcslen(L"/unblock "), L"/unblock ") == 0) {
		for (wchar_t chr : completion.playerName) {
			if (chr == L'\\' || iswspace(chr))
				replacement += L'\\';
			replacement += chr;
		}
	}
	else if (currentText.compare(0, wcslen(L"/report "), L"/report ") == 0) {
		replacement = L"@";
		for (wchar_t chr : completion.playerName) {
			if (chr == L'\\' || iswspace(chr))
				replacement += L'\\';
			replacement += chr;
		}
	} else
		replacement = std::to_wstring(completion.playerId);
	this->_buffer.erase(this->_buffer.begin() + targetStart, this->_buffer.begin() + targetEnd);
	this->_buffer.insert(this->_buffer.begin() + targetStart, replacement.begin(), replacement.end());
	auto newEnd = targetStart + replacement.size();
	if (appendSpace && (newEnd >= this->_buffer.size() - 1 || this->_buffer[newEnd] != L' '))
		this->_buffer.insert(this->_buffer.begin() + newEnd, L' ');
	this->_updateTextCursor(static_cast<int>(newEnd + appendSpace));
	this->_clearSelection();
	this->textChanged |= 1;
	this->_updateCompositionSprite();
	this->_privateMessageCompletionTimer = 240;
}

void InLobbyMenu::_renderPrivateMessageCompletions()
{
	if (!this->_privateMessageCompletionTimer || this->_privateMessageCompletions.empty())
		return;
	constexpr unsigned maxVisible = 10;
	constexpr int panelY = 202;
	auto first = min(this->_privateMessageCompletionScroll, static_cast<unsigned>(this->_privateMessageCompletions.size() - 1));
	auto count = min(maxVisible, static_cast<unsigned>(this->_privateMessageCompletions.size() - first));
	int height = 10 + static_cast<int>(count) * 24;
	SokuLib::DrawUtils::RectangleShape panel;
	panel.setPosition({292, panelY});
	panel.setSize({316, static_cast<unsigned>(height)});
	panel.setBorderColor(SokuLib::Color{0x7F, 0xA6, 0xD9, 0xFF});
	panel.setFillColor(SokuLib::Color{0x10, 0x18, 0x28, 0xEE});
	panel.draw();
	for (unsigned i = 0; i < count; i++) {
		auto index = first + i;
		auto &entry = this->_privateMessageCompletions[index];
		if (index == this->_privateMessageCompletionIndex) {
			SokuLib::DrawUtils::RectangleShape selected;
			selected.setPosition({296, panelY + 5 + static_cast<int>(i) * 24});
			selected.setSize({308, 22});
			selected.setFillColor(SokuLib::Color{0x45, 0x62, 0x88, 0xD8});
			selected.draw();
		}
		entry.label.setPosition({302, panelY + 6 + static_cast<int>(i) * 24});
		entry.label.draw();
	}
}

InLobbyMenu::EscapeOwner InLobbyMenu::_classifyEscapeOwner() const
{
	if (
		this->_editingText ||
		this->_emotePickerOpen ||
		this->_quickMessageMenuOpen ||
		(this->_hostlist && SokuLib::sceneId == SokuLib::SCENE_TITLE && SokuLib::newSceneId == SokuLib::SCENE_TITLE)
	)
		return EscapeOwner::MOD_UI;
	if (std::any_of(this->_hotkeyEvents.begin(), this->_hotkeyEvents.end(), [](const HotkeyEvent &event) {
		return event.pressed && (event.key == VK_F1 || event.key == VK_F2);
	}))
		return EscapeOwner::MOD_UI;
	if (SokuLib::sceneId == SokuLib::SCENE_TITLE && SokuLib::newSceneId == SokuLib::SCENE_TITLE)
		return EscapeOwner::LOBBY;
	return EscapeOwner::NATIVE_GAME;
}

void InLobbyMenu::_consumeEscape(const HotkeyEvent &event)
{
	if (!event.mapped)
		this->_consumedKeyboardEscapeGeneration = event.escapeGeneration;
}

void InLobbyMenu::onWindowKeyEvent(unsigned key, bool pressed, bool repeated)
{
	if (key != VK_ESCAPE && key != VK_F1 && key != VK_F2)
		return;
	std::lock_guard<std::mutex> lock(this->keyTimersMutex);
	if (key == VK_ESCAPE) {
		this->_setEscapeSource(
			this->_keyboardEscapeDown,
			this->_keyboardEscapeGeneration,
			pressed,
			false
		);
		return;
	}
	if (pressed && !repeated)
		this->_hotkeyEvents.push_back({key, true, false, 0, EscapeOwner::NATIVE_GAME, SokuLib::sceneId});
}

void InLobbyMenu::onInputFocusLost()
{
	std::lock_guard<std::mutex> lock(this->keyTimersMutex);
	this->_battleChatHolding = false;
	this->_battleChatHintUntil = {};
	this->_keyboardEscapeDown = false;
	this->_mappedEscapeDown = false;
	this->_hotkeyEvents.clear();
	memset(this->keysPressed, 0, sizeof(this->keysPressed));
}

void InLobbyMenu::setMappedEscapeDown(bool down)
{
	std::lock_guard<std::mutex> lock(this->keyTimersMutex);
	if (this->_mappedEscapeDown == down)
		return;
	this->_mappedEscapeDown = down;
	if (!down)
		return;

	// Outside the emote picker B/X belongs to the game. In particular,
	// it must keep its original meaning at an arcade machine and in native scenes.
	if (!this->_emotePickerOpen) {
		this->_mappedEscapeGeneration = 0;
		return;
	}
	this->_mappedEscapeGeneration = ++this->_nextEscapeGeneration;
	this->_hotkeyEvents.push_back({VK_ESCAPE, true, true, this->_mappedEscapeGeneration, EscapeOwner::MOD_UI, SokuLib::sceneId});
}

bool InLobbyMenu::filterNativeEscape(bool pressed)
{
	if (!pressed)
		return false;
	std::lock_guard<std::mutex> lock(this->keyTimersMutex);
	if (
		this->_keyboardEscapeGeneration && (
			this->_keyboardEscapeOwner != EscapeOwner::NATIVE_GAME ||
			SokuLib::sceneId != this->_keyboardEscapeScene ||
			SokuLib::newSceneId != this->_keyboardEscapeScene
		)
	) {
		if (this->_keyboardEscapeGeneration)
			this->_consumedKeyboardEscapeGeneration = this->_keyboardEscapeGeneration;
		return false;
	}
	if (!this->_keyboardEscapeGeneration && this->_classifyEscapeOwner() != EscapeOwner::NATIVE_GAME)
		return false;
	if (
		this->_keyboardEscapeGeneration && (
			this->_consumedKeyboardEscapeGeneration == this->_keyboardEscapeGeneration ||
			this->_nativeKeyboardEscapeGeneration == this->_keyboardEscapeGeneration
		)
	)
		return false;
	this->_nativeKeyboardEscapeGeneration = this->_keyboardEscapeGeneration;
	return true;
}

void InLobbyMenu::_updateMessageSprite(SokuLib::Vector2i pos, unsigned int remaining, SokuLib::Vector2i realSize, SokuLib::DrawUtils::Sprite &sprite, unsigned char alpha)
{
	sprite.tint.a = alpha;
	sprite.rect.top = 0;
	sprite.rect.width = realSize.x;
	sprite.rect.height = realSize.y - remaining;
	pos.y -= sprite.rect.height;
	if (pos.y < 3) {
		sprite.rect.height -= 3 - pos.y;
		sprite.rect.top = 3 - pos.y;
		pos.y = 3;
	}
	sprite.setSize({
		static_cast<unsigned int>(sprite.rect.width),
		static_cast<unsigned int>(sprite.rect.height)
	});
	sprite.setPosition(pos);
}

void InLobbyMenu::_renderMachineOverlay()
{
	if (this->_currentMachine->id != UINT32_MAX)
		return;
	this->_hostlist->render();
	this->_currentMachine->skin.overlay.draw();
}



constexpr uint8_t VN[16] = {
	0x46, 0xC9, 0x67, 0xC8,
	0xAC, 0xF2, 0x44, 0x4D,
	0xB8, 0xB1, 0xEC, 0xEE,
	0xD4, 0xD5, 0x40, 0x4A
};
constexpr uint8_t SR_SWR[16] = {
	0x64, 0x73, 0x65, 0xD9,
	0xFF, 0xC4, 0x6E, 0x48,
	0x8D, 0x7C, 0xA1, 0x92,
	0x31, 0x34, 0x72, 0x95
};
constexpr uint8_t VN_SWR[16] = {
	0x6E, 0x73, 0x65, 0xD9,
	0xFF, 0xC4, 0x6E, 0x48,
	0x8D, 0x7C, 0xA1, 0x92,
	0x31, 0x34, 0x72, 0x95
};
constexpr uint8_t GR_SWR[16] = {
	0x69, 0x73, 0x65, 0xD9,
	0xFF, 0xC4, 0x6E, 0x48,
	0x8D, 0x7C, 0xA1, 0x92,
	0x31, 0x34, 0x72, 0x95
};
constexpr uint8_t GRCN_SWR[16] = {
	0x6A, 0x73, 0x65, 0xD9,
	0xFF, 0xC4, 0x6E, 0x48,
	0x8D, 0x7C, 0xA1, 0x92,
	0x31, 0x34, 0x72, 0x95
};
constexpr uint8_t GR6_SWR[16] = {
	0x6B, 0x73, 0x65, 0xD9,
	0xFF, 0xC4, 0x6E, 0x48,
	0x8D, 0x7C, 0xA1, 0x92,
	0x31, 0x34, 0x72, 0x95
};
constexpr uint8_t GRCN6_SWR[16] = {
	0x6C, 0x73, 0x65, 0xD9,
	0xFF, 0xC4, 0x6E, 0x48,
	0x8D, 0x7C, 0xA1, 0x92,
	0x31, 0x34, 0x72, 0x95
};
const uint8_t *versions[] = {
	VN,
	SR_SWR,
	VN_SWR,
	GR_SWR,
	GRCN_SWR,
	GR6_SWR,
	GRCN6_SWR,
};
const char * const versionNames[] = { "-SWR", "+SR", "Vanilla", "+GR", "+GR-62FPS", "+GR0.6", "+GR0.6-62FPS" };

void InLobbyMenu::_startHosting()
{
	auto ranked = this->_connection->getMe()->settings.hostPref & Lobbies::HOSTPREF_PREFER_RANKED;

	this->_parent->setupHost(hostPort, true);
	if (this->_hostThread.joinable())
		this->_hostThread.join();
	this->_hostThread = std::thread{[this, ranked]{
		std::string converted;
		const char * ip;
		try {
			ip = getMyIp();
		} catch (std::exception &e) {
			this->_addMessageToList(0xFF0000, 0, localizeLobbyMessage(std::string("Failed to get public IP: ") + e.what()));
			return;
		}

		unsigned short port = hostPort;
		auto dup = strdup(ip);
		char *pos = strchr(dup, ':');
		std::string name;

		for (int i = 0; i < sizeof(versionNames) / sizeof(*versionNames); i++) {
			if (memcmp(versions[i], (unsigned char *)0x858B80, 16) == 0) {
				name = versionNames[i];
				break;
			}
		}
		if (name.empty())
			name = "+???";

		if (pos) {
			try {
				port = std::stoul(pos + 1);
			} catch (std::exception &e) {
				puts(e.what());
			}
			*pos = 0;
		}
		printf("Putting hostlist %s:%u\n", dup, port);
		th123intl::ConvertCodePage(th123intl::GetTextCodePage(), SokuLib::profile1.name.operator std::string(), CP_UTF8, converted);
		nlohmann::json data = {
			{"profile_name", converted},
			{"message", "[" + name + "] SokuLobbies " + std::string(modVersion) + ": Waiting in " + this->_roomName + " | " + (ranked ? "ranked" : "casual")},
			{"host", dup},
			{"port", port}
		};

		free(dup);
		try {
			lobbyData->httpRequest("https://konni.delthas.fr/games", "PUT", data.dump());
			this->_addMessageToList(0x00FF00, 0, "Broadcast to hostlist successful");
		} catch (std::exception &e) {
			std::string error = e.what();
			if (!chineseLanguage)
				this->_addMessageToList(0xFF0000, 0, "Hostlist error: " + error);
			else if (error.find("HTTP 400") != std::string::npos && error.find("couldn't connect to host") != std::string::npos)
				this->_addMessageToList(0xFF0000, 0, "\u521B\u5EFA\u8FDE\u63A5\u5931\u8D25\uFF1A\u8BF7\u786E\u8BA4\u4F60\u5DF2\u7ECF\u4F7F\u7528 swarm \u5EFA\u7ACB\u4E3B\u673A\uFF0C\u6709\u7591\u95EE\u53EF\u67E5\u770B wiki.514.live\u7F51\u7AD9\u8054\u673A\u6559\u7A0B\u3002");
			else
				this->_addMessageToList(0xFF0000, 0, "\u521B\u5EFA\u8FDE\u63A5\u5931\u8D25\uFF1A" + error);
		}
	}};
}

void InLobbyMenu::addString(wchar_t *str, size_t size)
{
	this->_textMutex.lock();
	const size_t contentSize = this->_buffer.empty() ? 0 : this->_buffer.size() - 1;
	const size_t available = contentSize < CHAT_CHARACTER_LIMIT - 1 ? CHAT_CHARACTER_LIMIT - 1 - contentSize : 0;

	size = (std::min)(size, available);
	if (size && IS_HIGH_SURROGATE(str[size - 1]))
		size--;
	if (!size) {
		playSound(0x29);
		this->_textMutex.unlock();
		return;
	}
	this->_buffer.insert(this->_buffer.begin() + this->_textCursorPosIndex, str, str + size);
	this->_updateTextCursor(this->_textCursorPosIndex + size);
	this->textChanged |= 1;
	playSound(0x27);
	this->_textMutex.unlock();
}

void InLobbyMenu::_updateCompositionSprite()
{
	if (this->textChanged & 1) {
		int ret = 0;
		SokuLib::Vector2i textureSize = BOX_TEXTURE_SIZE;
		D3DCAPS9 caps{};
		const int visibleWidth = this->_textSprite[0].rect.width;
		const int textWidth = getTextSize(this->_buffer.data(), this->_chatFont, BOX_TEXTURE_SIZE).x;

		if (SokuLib::pd3dDev && SUCCEEDED(SokuLib::pd3dDev->GetDeviceCaps(&caps)))
			textureSize.x = (std::min)(textureSize.x, static_cast<int>(caps.MaxTextureWidth));
		textureSize.x = (std::min)(textureSize.x, (std::max)(visibleWidth, textWidth + 8));
		if (createTextTexture(ret, this->_buffer.data(), this->_chatFont, textureSize, nullptr))
			this->_textSprite[0].texture.setHandle(ret, textureSize.to<unsigned>());
		else
			puts("Error creating text texture");
	}
	this->textChanged = false;
}

int InLobbyMenu::_getTextSize(unsigned int i)
{
	auto it = this->_textSize.find(i);

	if (it != this->_textSize.end())
		return it->second;

	int size = getTextSize(UTF16Encode(std::basic_string<unsigned>(&i, &i + 1)).c_str(), this->_chatFont, {32, 20}).x;

	this->_textSize[i] = size;
	return size;
}

InLobbyMenu::ArcadeMachine::ArcadeMachine(unsigned id, SokuLib::Vector2i pos, LobbyData::ArcadeAnimation *currentAnim, LobbyData::ArcadeSkin &skin):
	id(id),
	pos(pos),
	currentAnim(currentAnim),
	skin(skin)
{
}

InLobbyMenu::ArcadeMachine::ArcadeMachine(const InLobbyMenu::ArcadeMachine &):
	skin(*(LobbyData::ArcadeSkin*)nullptr)
{
	puts("ArcadeMachine(const InLobbyMenu::ArcadeMachine &)");
	assert(false);
}

InLobbyMenu::ElevatorMachine::ElevatorMachine(unsigned id, SokuLib::Vector2i pos, LobbyData::ElevatorPlacement &links, LobbyData::ElevatorSkin &skin):
	id(id),
	pos(pos),
	skin(skin),
	links(links)
{
}

InLobbyMenu::ElevatorMachine::ElevatorMachine(const InLobbyMenu::ElevatorMachine &):
	skin(*(LobbyData::ElevatorSkin*)nullptr),
	links(*(LobbyData::ElevatorPlacement *)nullptr)
{
	puts("ElevatorMachine(const InLobbyMenu::ElevatorMachine &)");
	assert(false);
}
