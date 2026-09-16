//
// Created by Gegel85 on 05/04/2022.
//

#include <SokuLib.hpp>
#include <imm.h>
#include <mutex>
#include <process.h>
#include "InputBox.hpp"
#include "createUTFTexture.hpp"

#define CURSOR_ENDX 465
#define CURSOR_STARTX 174
#define CURSOR_STARTY 233
#define CURSOR_STEP 6

void playSound(int se);

bool inputBoxShown = false;
static bool hasEnglishPatch;
static bool changed = false;
static char lastPressed = 0;
static unsigned t = 0;
static WNDPROC Original_WndProc = nullptr;
static int cursorPos = 0;
static bool started = false;
static bool loaded = false;
static bool escPressed = false;
static bool returnPressed = false;
static bool changes = false;
static SokuLib::DrawUtils::RectangleShape whiteBox;
static SokuLib::DrawUtils::RectangleShape cursor;
static SokuLib::DrawUtils::Sprite titleSprite;
static SokuLib::DrawUtils::Sprite textSprite;
static SokuLib::DrawUtils::Sprite boxSprite;
static std::function<void (const std::string &value)> onAcceptFct;
static std::function<void (const std::wstring &value)> onWideAcceptFct;
static std::vector<char> buffer;
static bool wideMode = false;
static bool wideDirty = false;
static bool suppressWideEscape = false;
static bool suppressWideReturn = false;
static size_t wideCursorPos = 0;
static size_t wideMaxLength = 0;
static wchar_t pendingHighSurrogate = 0;
static std::wstring wideBuffer;
static wchar_t wideShownChr;
static std::wstring wideComposition;
static BYTE current[256];
static unsigned timers[256];
static std::mutex mutex;
static char shownChr;
static SokuLib::SWRFont defaultFont12;

static size_t previousCodePoint(const std::wstring &text, size_t pos)
{
	if (!pos)
		return 0;
	pos--;
	if (pos && text[pos] >= 0xDC00 && text[pos] <= 0xDFFF && text[pos - 1] >= 0xD800 && text[pos - 1] <= 0xDBFF)
		pos--;
	return pos;
}

static size_t nextCodePoint(const std::wstring &text, size_t pos)
{
	if (pos >= text.size())
		return text.size();
	if (pos + 1 < text.size() && text[pos] >= 0xD800 && text[pos] <= 0xDBFF && text[pos + 1] >= 0xDC00 && text[pos + 1] <= 0xDFFF)
		return pos + 2;
	return pos + 1;
}

static void insertWideText(const std::wstring &text)
{
	if (text.empty() || wideBuffer.size() >= wideMaxLength)
		return;
	auto insert = text.substr(0, wideMaxLength - wideBuffer.size());
	if (!insert.empty() && insert.back() >= 0xD800 && insert.back() <= 0xDBFF)
		insert.pop_back();
	wideBuffer.insert(wideCursorPos, insert);
	wideCursorPos += insert.size();
	wideDirty = true;
}

static void refreshWideText()
{
	if (!wideDirty)
		return;
	std::wstring combined = wideBuffer.substr(0, wideCursorPos) + wideComposition + wideBuffer.substr(wideCursorPos);
	if (wideShownChr)
		combined.assign(combined.size(), wideShownChr);
	size_t compositionEnd = wideCursorPos + wideComposition.size();
	size_t start = 0;
	while (start < wideCursorPos && getTextSize(combined.substr(start, compositionEnd - start).c_str(), defaultFont12, {2048, 20}, true).x > 284)
		start = nextCodePoint(combined, start);
	std::wstring visible = combined.substr(start);
	auto fit = getTextFit(visible.c_str(), defaultFont12, 288, true);
	if (fit < visible.size())
		visible.resize(fit);
	SokuLib::Vector2i size;
	int textureId = 0;
	if (visible.empty()) {
		textSprite.texture.destroy();
		textSprite.rect = {0, 0, 292, 18};
		textSprite.setSize({291, 18});
	} else if (createTextTexture(textureId, visible.c_str(), defaultFont12, {292, 20}, &size, true)) {
		textSprite.texture.setHandle(textureId, {292, 20});
		textSprite.rect = {0, 0, 292, 18};
		textSprite.setSize({291, 18});
	}
	auto before = combined.substr(start, compositionEnd - start);
	auto cursorX = getTextSize(before.c_str(), defaultFont12, {2048, 20}, true).x;
	cursor.setPosition({CURSOR_STARTX + (std::min)(cursorX, 289), CURSOR_STARTY});
	HIMC context = ImmGetContext(SokuLib::window);
	if (context) {
		COMPOSITIONFORM composition{};
		composition.dwStyle = CFS_POINT;
		composition.ptCurrentPos = {cursor.getPosition().x, cursor.getPosition().y + 16};
		ImmSetCompositionWindow(context, &composition);
		CANDIDATEFORM candidate{};
		candidate.dwIndex = 0;
		candidate.dwStyle = CFS_CANDIDATEPOS;
		candidate.ptCurrentPos = composition.ptCurrentPos;
		ImmSetCandidateWindow(context, &candidate);
		ImmReleaseContext(SokuLib::window, context);
	}
	wideDirty = false;
}

static void resetInputState()
{
	inputBoxShown = false;
	wideMode = false;
	wideBuffer.clear();
	wideComposition.clear();
	pendingHighSurrogate = 0;
	suppressWideEscape = false;
	suppressWideReturn = false;
	returnPressed = false;
	escPressed = false;
	onAcceptFct = {};
	onWideAcceptFct = {};
}

static void updateCursor(int newVal)
{
	int diff = newVal - cursorPos;
	int newX = cursor.getPosition().x + diff * CURSOR_STEP;

	if (newX > CURSOR_ENDX) {
		textSprite.rect.left += newX - CURSOR_ENDX;
		cursor.setPosition({CURSOR_ENDX, CURSOR_STARTY});
	} else if (newX < CURSOR_STARTX) {
		textSprite.rect.left += newX - CURSOR_STARTX;
		cursor.setPosition({CURSOR_STARTX, CURSOR_STARTY});
	} else
		cursor.setPosition({newX, CURSOR_STARTY});
	cursorPos = newVal;
}

void inputBoxRender()
{
	if (!inputBoxShown)
		return;
	if (wideMode)
		refreshWideText();

	boxSprite.draw();
	whiteBox.draw();
	titleSprite.draw();
	textSprite.draw();
	cursor.draw();
}

static std::string sanitizeInput()
{
	if (shownChr) {
		std::string result;

		result.resize(buffer.size() - 1, shownChr);
		return result;
	}

	std::string result{buffer.begin(), buffer.end() - 1};

	for (size_t pos = result.find('<'); pos != std::string::npos; pos = result.find('<'))
		result[pos] = '{';
	for (size_t pos = result.find('>'); pos != std::string::npos; pos = result.find('>'))
		result[pos] = '}';
	return result;
}

void inputBoxUpdate()
{
	if (!inputBoxShown)
		return;

	// Do not handle keyboard input while the game window is unfocused to avoid
	// validating the dialog when Enter is pressed in another window.
	if (GetForegroundWindow() != SokuLib::window) {
		memset(current, 0, sizeof(current));
		memset(timers, 0, sizeof(timers));
		lastPressed = 0;
		t = 0;
		escPressed = false;
		returnPressed = false;
		return;
	}

	for (size_t i = 0; i < sizeof(current); i++) {
		int j = GetAsyncKeyState(i);

		current[i] = j >> 8 | j & 1;
		if (current[i] & 0x80)
			timers[i]++;
		else
			timers[i] = 0;
	}
	if (escPressed) {
		inputBoxShown = (current[VK_ESCAPE] & 0x80) != 0;
		if (!inputBoxShown)
			resetInputState();
		return;
	}
	if (returnPressed) {
		if ((current[VK_RETURN] & 0x80) == 0) {
			inputBoxShown = false;
			auto wasWide = wideMode;
			auto narrowCallback = onAcceptFct;
			auto wideCallback = onWideAcceptFct;
			auto narrowValue = buffer.empty() ? std::string{} : std::string(buffer.data());
			auto wideValue = wideBuffer;
			resetInputState();
			try {
				if (wasWide && wideCallback)
					wideCallback(wideValue);
				else if (!wasWide && narrowCallback)
					narrowCallback(narrowValue);
			} catch (...) {}
		}
		return;
	}
	if (suppressWideEscape) {
		if (!(current[VK_ESCAPE] & 0x80))
			suppressWideEscape = false;
		return;
	}
	if (suppressWideReturn) {
		if (!(current[VK_RETURN] & 0x80))
			suppressWideReturn = false;
		return;
	}
	if (timers[VK_ESCAPE] == 1) {
		playSound(0x29);
		escPressed = true;
		return;
	}
	mutex.lock();
	if (wideMode) {
		if (timers[VK_HOME] == 1) {
			wideCursorPos = 0;
			wideDirty = true;
			playSound(0x27);
		}
		if (timers[VK_END] == 1) {
			wideCursorPos = wideBuffer.size();
			wideDirty = true;
			playSound(0x27);
		}
		if (timers[VK_RETURN] == 1) {
			returnPressed = true;
			mutex.unlock();
			return;
		}
		if (timers[VK_BACK] == 1 || (timers[VK_BACK] > 36 && timers[VK_BACK] % 6 == 0)) {
			auto previous = previousCodePoint(wideBuffer, wideCursorPos);
			if (previous != wideCursorPos) {
				wideBuffer.erase(previous, wideCursorPos - previous);
				wideCursorPos = previous;
				wideDirty = true;
				playSound(0x27);
			}
		}
		if (timers[VK_DELETE] == 1 || (timers[VK_DELETE] > 36 && timers[VK_DELETE] % 6 == 0)) {
			auto next = nextCodePoint(wideBuffer, wideCursorPos);
			if (next != wideCursorPos) {
				wideBuffer.erase(wideCursorPos, next - wideCursorPos);
				wideDirty = true;
				playSound(0x27);
			}
		}
		if (timers[VK_LEFT] == 1 || (timers[VK_LEFT] > 36 && timers[VK_LEFT] % 3 == 0)) {
			auto previous = previousCodePoint(wideBuffer, wideCursorPos);
			if (previous != wideCursorPos) {
				wideCursorPos = previous;
				wideDirty = true;
				playSound(0x27);
			}
		}
		if (timers[VK_RIGHT] == 1 || (timers[VK_RIGHT] > 36 && timers[VK_RIGHT] % 3 == 0)) {
			auto next = nextCodePoint(wideBuffer, wideCursorPos);
			if (next != wideCursorPos) {
				wideCursorPos = next;
				wideDirty = true;
				playSound(0x27);
			}
		}
		mutex.unlock();
		return;
	}
	if (timers[VK_HOME] == 1) {
		playSound(0x27);
		updateCursor(0);
	}
	if (timers[VK_END] == 1) {
		playSound(0x27);
		updateCursor(buffer.size() - 1);
	}
	if (timers[VK_RETURN] == 1) {
		returnPressed = true;
		mutex.unlock();
		return;
	}
	if (timers[VK_BACK] == 1 || (timers[VK_BACK] > 36 && timers[VK_BACK] % 6 == 0)) {
		if (cursorPos != 0) {
			buffer.erase(buffer.begin() + cursorPos - 1);
			updateCursor(cursorPos - 1);
			changes = true;
			playSound(0x27);
		}
	}
	if (timers[VK_DELETE] == 1 || (timers[VK_DELETE] > 36 && timers[VK_DELETE] % 6 == 0)) {
		if (cursorPos < buffer.size() - 1) {
			buffer.erase(buffer.begin() + cursorPos);
			playSound(0x27);
			changes = true;
			textSprite.texture.createFromText(sanitizeInput().c_str(), defaultFont12, {8 * buffer.size(), 1800});
		}
	}
	if (timers[VK_LEFT] == 1 || (timers[VK_LEFT] > 36 && timers[VK_LEFT] % 3 == 0)) {
		if (cursorPos != 0) {
			updateCursor(cursorPos - 1);
			playSound(0x27);
		}
	}
	if (timers[VK_RIGHT] == 1 || (timers[VK_RIGHT] > 36 && timers[VK_RIGHT] % 3 == 0)) {
		if (cursorPos != buffer.size() - 1) {
			updateCursor(cursorPos + 1);
			playSound(0x27);
		}
	}
	if (lastPressed) {
		t++;
		if (t == 1 || (t > 36 && t % 6 == 0)) {
			buffer.insert(buffer.begin() + cursorPos, lastPressed);
			updateCursor(cursorPos + 1);
			playSound(0x27);
			changes = true;
		}
	}
	if (
		(SokuLib::mainMode == SokuLib::BATTLE_MODE_VSSERVER || SokuLib::mainMode == SokuLib::BATTLE_MODE_VSCLIENT) &&
		(
			SokuLib::sceneId == SokuLib::SCENE_BATTLE ||
			SokuLib::sceneId == SokuLib::SCENE_BATTLECL ||
			SokuLib::sceneId == SokuLib::SCENE_BATTLESV ||
			SokuLib::newSceneId == SokuLib::SCENE_BATTLE ||
			SokuLib::newSceneId == SokuLib::SCENE_BATTLECL ||
			SokuLib::newSceneId == SokuLib::SCENE_BATTLESV
		)
	) {
		mutex.unlock();
		return;
	}
	if (changes)
		textSprite.texture.createFromText(sanitizeInput().c_str(), defaultFont12, {max(292, 8 * buffer.size()), 1800});
	changes = false;
	mutex.unlock();
}

LRESULT __stdcall Hooked_WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (inputBoxShown && wideMode) {
		if (uMsg == WM_KEYDOWN &&
			((wParam == 'V' && (GetKeyState(VK_CONTROL) & 0x8000)) || (wParam == VK_INSERT && (GetKeyState(VK_SHIFT) & 0x8000)))) {
			if (OpenClipboard(hWnd)) {
				HANDLE handle = GetClipboardData(CF_UNICODETEXT);
				if (handle) {
					const auto *clipboard = static_cast<const wchar_t *>(GlobalLock(handle));
					if (clipboard) {
						std::wstring pasted(clipboard);
						for (auto &chr : pasted)
							if (chr == L'\r' || chr == L'\n')
								chr = L' ';
						std::lock_guard<std::mutex> lock(mutex);
						insertWideText(pasted);
						GlobalUnlock(handle);
					}
				}
				CloseClipboard();
			}
			return 0;
		}
		if (uMsg == WM_KEYDOWN && !wideComposition.empty()) {
			if (wParam == VK_ESCAPE)
				suppressWideEscape = true;
			else if (wParam == VK_RETURN)
				suppressWideReturn = true;
		}
		if (uMsg == WM_CHAR && wParam >= 0x20 && wParam != 0x7F) {
			std::lock_guard<std::mutex> lock(mutex);
			auto chr = static_cast<wchar_t>(wParam);
			if (chr >= 0xD800 && chr <= 0xDBFF)
				pendingHighSurrogate = chr;
			else if (chr >= 0xDC00 && chr <= 0xDFFF && pendingHighSurrogate) {
				std::wstring pair{pendingHighSurrogate, chr};
				pendingHighSurrogate = 0;
				insertWideText(pair);
			} else {
				pendingHighSurrogate = 0;
				insertWideText(std::wstring(1, chr));
			}
			return 0;
		}
		if (uMsg == WM_IME_COMPOSITION) {
			HIMC context = ImmGetContext(hWnd);
			if (context) {
				std::lock_guard<std::mutex> lock(mutex);
				if (lParam & GCS_RESULTSTR) {
					LONG bytes = ImmGetCompositionStringW(context, GCS_RESULTSTR, nullptr, 0);
					if (bytes > 0) {
						std::wstring result(static_cast<size_t>(bytes) / sizeof(wchar_t), L'\0');
						if (ImmGetCompositionStringW(context, GCS_RESULTSTR, result.data(), bytes) == bytes)
							insertWideText(result);
					}
					wideComposition.clear();
				}
				if (lParam & GCS_COMPSTR) {
					LONG bytes = ImmGetCompositionStringW(context, GCS_COMPSTR, nullptr, 0);
					wideComposition.clear();
					if (bytes > 0) {
						wideComposition.resize(static_cast<size_t>(bytes) / sizeof(wchar_t));
						ImmGetCompositionStringW(context, GCS_COMPSTR, wideComposition.data(), bytes);
					}
				}
				wideDirty = true;
				ImmReleaseContext(hWnd, context);
			}
			return 0;
		}
		if (uMsg == WM_IME_ENDCOMPOSITION) {
			std::lock_guard<std::mutex> lock(mutex);
			wideComposition.clear();
			wideDirty = true;
		}
	} else if (uMsg == WM_KEYDOWN && inputBoxShown) {
		BYTE keyboardState[256];

		GetKeyboardState(keyboardState);
		if(!(MapVirtualKey(wParam, MAPVK_VK_TO_CHAR) >> (sizeof(UINT) * 8 - 1) & 1)) {
			unsigned short chr = 0;
			int nb = ToAscii((UINT)wParam, lParam, keyboardState, &chr, 0);

			if (nb == 1 && chr < 0x7F && chr >= 32) {
				mutex.lock();
				if (lastPressed && t == 0) {
					buffer.insert(buffer.begin() + cursorPos, lastPressed);
					updateCursor(cursorPos + 1);
					changes = true;
					playSound(0x27);
				}
				lastPressed = chr;
				t = 0;
				mutex.unlock();
			}
		}
	} else if (uMsg == WM_KEYUP && inputBoxShown) {
		mutex.lock();
		if (lastPressed && t == 0) {
			buffer.insert(buffer.begin() + cursorPos, lastPressed);
			updateCursor(cursorPos + 1);
			playSound(0x27);
		}
		lastPressed = 0;
		t = 0;
		mutex.unlock();
	}
	return CallWindowProc(Original_WndProc, hWnd, uMsg, wParam, lParam);
}

void inputBoxLoadAssets()
{
	if (loaded)
		return;
	if (!Original_WndProc)
		Original_WndProc = (WNDPROC)SetWindowLongPtr(SokuLib::window, GWL_WNDPROC, (LONG_PTR)Hooked_WndProc);
	loaded = true;
	hasEnglishPatch = (*(int *)0x411c64 == 1);
	boxSprite.texture.loadFromGame("data/menu/21_Base.bmp");
	boxSprite.rect.width = boxSprite.texture.getSize().x;
	boxSprite.rect.height = boxSprite.texture.getSize().y;
	boxSprite.setSize(boxSprite.texture.getSize());
	boxSprite.setPosition({160, 192});

	whiteBox.setSize({300, 18});
	whiteBox.setPosition({170, 231});
	whiteBox.setFillColor(SokuLib::Color::White);
	whiteBox.setBorderColor(SokuLib::Color::Transparent);

	cursor.setSize({1, 14});
	cursor.setPosition({CURSOR_STARTX, CURSOR_STARTY});
	cursor.setFillColor(SokuLib::Color::Black);
	cursor.setBorderColor(SokuLib::Color::Transparent);

	textSprite.fillColors[SokuLib::DrawUtils::GradiantRect::RECT_BOTTOM_LEFT_CORNER] = SokuLib::DrawUtils::DxSokuColor{0x80, 0x80, 0xFF};
	textSprite.fillColors[SokuLib::DrawUtils::GradiantRect::RECT_BOTTOM_RIGHT_CORNER]= SokuLib::DrawUtils::DxSokuColor{0x80, 0x80, 0xFF};
	textSprite.rect.width = 292;
	textSprite.rect.height = 18;
	textSprite.setSize({291, 18});
	textSprite.setPosition({174 - hasEnglishPatch * 2, CURSOR_STARTY});

	titleSprite.rect.width = 292;
	titleSprite.rect.height = 32;
	titleSprite.setSize({292, 32});
	titleSprite.setPosition({174, 202});

	SokuLib::FontDescription desc;

	desc.r1 = 255;
	desc.r2 = 255;
	desc.g1 = 255;
	desc.g2 = 255;
	desc.b1 = 255;
	desc.b2 = 255;
	desc.height = 12 + hasEnglishPatch * 2;
	desc.weight = FW_NORMAL;
	desc.italic = 0;
	desc.shadow = 1;
	desc.bufferSize = 1000000;
	desc.charSpaceX = 0;
	desc.charSpaceY = hasEnglishPatch * -2;
	desc.offsetX = 0;
	desc.offsetY = 0;
	desc.useOffset = 0;
	strcpy(desc.faceName, "MonoSpatialModSWR");
	desc.weight = FW_REGULAR;
	defaultFont12.create();
	defaultFont12.setIndirect(desc);
}

void inputBoxUnloadAssets()
{
	if (!loaded)
		return;
	loaded = false;
	closeInputDialog();
	titleSprite.texture.destroy();
	textSprite.texture.destroy();
	boxSprite.texture.destroy();
	defaultFont12.destruct();
}

void openInputDialog(const char *title, const char *defaultValue, char shownChar)
{
	playSound(0x28);

	shownChr = shownChar;
	wideShownChr = 0;
	wideMode = false;
	onWideAcceptFct = {};
	memset(current, 0, sizeof(current));
	memset(timers, 0, sizeof(timers));
	lastPressed = 0;
	t = 0;
	buffer.clear();
	if (defaultValue) {
		buffer.reserve(strlen(defaultValue));
		buffer.insert(buffer.begin(), defaultValue, defaultValue + strlen(defaultValue));
	}
	buffer.push_back(0);

	titleSprite.texture.createFromText(title, defaultFont12, {292, 32});
	textSprite.texture.createFromText(sanitizeInput().c_str(), defaultFont12, {max(292, 8 * buffer.size()), 20});
	textSprite.rect.left = 0;

	cursorPos = 0;
	cursor.setPosition({CURSOR_STARTX, CURSOR_STARTY});
	updateCursor(buffer.size() - 1);

	inputBoxShown = true;
	returnPressed = false;
	escPressed = false;
}

void setInputBoxCallbacks(const std::function<void (const std::string &value)> &onAccept)
{
	onAcceptFct = onAccept;
}

void closeInputDialog()
{
	if (wideMode && SokuLib::window) {
		HIMC context = ImmGetContext(SokuLib::window);
		if (context) {
			ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
			ImmReleaseContext(SokuLib::window, context);
		}
	}
	resetInputState();
}

void openWideInputDialog(const wchar_t *title, const std::wstring &defaultValue, size_t maxLength, wchar_t shownChar)
{
	playSound(0x28);
	memset(current, 0, sizeof(current));
	memset(timers, 0, sizeof(timers));
	lastPressed = 0;
	t = 0;
	wideMode = true;
	wideShownChr = shownChar;
	onAcceptFct = {};
	wideMaxLength = maxLength;
	wideBuffer = defaultValue.substr(0, maxLength);
	if (!wideBuffer.empty() && wideBuffer.back() >= 0xD800 && wideBuffer.back() <= 0xDBFF)
		wideBuffer.pop_back();
	wideCursorPos = wideBuffer.size();
	wideComposition.clear();
	wideDirty = true;
	pendingHighSurrogate = 0;
	suppressWideEscape = false;
	suppressWideReturn = false;

	SokuLib::Vector2i size;
	int textureId = 0;
	if (createTextTexture(textureId, title ? title : L"", defaultFont12, {292, 32}, &size, true)) {
		titleSprite.texture.setHandle(textureId, {292, 32});
		titleSprite.rect = {0, 0, 292, 32};
		titleSprite.setSize({292, 32});
	}
	textSprite.rect.left = 0;
	cursor.setPosition({CURSOR_STARTX, CURSOR_STARTY});
	inputBoxShown = true;
	returnPressed = false;
	escPressed = false;
}

void setWideInputBoxCallbacks(const std::function<void (const std::wstring &value)> &onAccept)
{
	onWideAcceptFct = onAccept;
}
