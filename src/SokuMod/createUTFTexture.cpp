//
// Created by PinkySmile on 29/01/2023.
//

#include "createUTFTexture.hpp"

#include <map>
#include <string>
#include <tuple>
#include <utility>

bool textureUnderline = false;

static HFONT createFont(const SokuLib::SWRFont &font, bool sharp)
{
	return CreateFontA(
		font.description.height,
		0,
		0,
		0,
		font.description.weight,
		font.description.italic,
		textureUnderline,
		0,
		*(int*)0x411c64,
		OUT_TT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		sharp ? NONANTIALIASED_QUALITY : PROOF_QUALITY,
		FIXED_PITCH | FF_MODERN,
		font.description.faceName
	);
}

static void setFont(HDC context, SokuLib::SWRFont &font, bool sharp)
{
	font.font = createFont(font, sharp);
	font.hdc = context;
	font.gdiobj = SelectObject(context, font.font);
}

static void unsetFont(HDC context, SokuLib::SWRFont &font)
{
	if (font.gdiobj)
		SelectObject(context, font.gdiobj);
	if (font.font)
		DeleteObject(font.font);
	font.gdiobj = nullptr;
	font.font = nullptr;
	font.hdc = nullptr;
}

struct MeasurementFontKey {
	LONG height;
	LONG weight;
	BYTE italic;
	bool underline;
	bool sharp;
	int charset;
	std::string faceName;

	bool operator<(const MeasurementFontKey &other) const
	{
		return std::tie(height, weight, italic, underline, sharp, charset, faceName) <
			std::tie(other.height, other.weight, other.italic, other.underline, other.sharp, other.charset, other.faceName);
	}
};

class TextMeasurementContext {
private:
	HDC _context = CreateCompatibleDC(nullptr);
	std::map<MeasurementFontKey, HFONT> _fonts;

public:
	~TextMeasurementContext()
	{
		for (const auto &[key, font] : this->_fonts)
			DeleteObject(font);
		if (this->_context)
			DeleteDC(this->_context);
	}

	bool measure(const wchar_t *text, const SokuLib::SWRFont &font, bool sharp, int maxWidth, int *fit, SIZE &size)
	{
		if (!this->_context)
			return false;
		MeasurementFontKey key{
			font.description.height,
			font.description.weight,
			font.description.italic,
			textureUnderline,
			sharp,
			*(int *)0x411c64,
			std::string(font.description.faceName, strnlen(font.description.faceName, sizeof(font.description.faceName)))
		};
		auto found = this->_fonts.find(key);
		if (found == this->_fonts.end()) {
			auto created = createFont(font, sharp);
			if (!created)
				return false;
			found = this->_fonts.emplace(std::move(key), created).first;
		}
		auto previous = SelectObject(this->_context, found->second);
		auto length = static_cast<int>(wcslen(text));
		BOOL result;

		if (fit)
			result = GetTextExtentExPointW(this->_context, text, length, maxWidth, fit, nullptr, &size);
		else
			result = GetTextExtentPoint32W(this->_context, text, length, &size);
		SelectObject(this->_context, previous);
		return result != FALSE;
	}
};

static thread_local TextMeasurementContext measurementContext;

inline unsigned int _div255(unsigned int v) { return (v + 1 + (v >> 8)) >> 8; }

static void __fastcall repl_alphaBlend(unsigned int color, unsigned int alpha, unsigned int* out) {
	unsigned short a0 = ((alpha*0xffu) >> 4) & 0xff;
	unsigned short a1 = *out >> 24;
	unsigned short a2 = a0 + _div255(a1*(255-a0));

	unsigned int result = (unsigned int)a2 << 24;
	if (a2 != 0) for (int i = 0; i < 3; ++i) {
		unsigned short c0 = (color >> i*8) & 0xff;
		unsigned short c1 = (*out >> i*8) & 0xff;
		unsigned short c2 = (c0*a0 + _div255(c1*a1)*(255-a0)) / a2;
		result |= ((unsigned int)c2 & 0xff) << i*8;
	}
	*out = result;
}

static void __fastcall repl_textShadow(int height, int width, int inputLineSize, int outputLineSize, unsigned int* input, unsigned int* output) {
	width -= 1;
	height -= 1;

	for (int j = 1; j < height; ++j) {
		for (int i = 1; i < width; ++i) {
			unsigned int c0 = input[j * inputLineSize + i];

			if (c0 >> 24) {
				unsigned alpha = c0 >> 27;
				unsigned int c1 = 0xff000000;

				if (alpha > 16)
					alpha = 16;
				repl_alphaBlend(c0, alpha, &c1);
				repl_alphaBlend(c1, 16, &output[j * outputLineSize + i]);
			} else {
				unsigned char current = input[(j - 1) * inputLineSize + i] >> 24;
				unsigned char next = input[(j + 1) * inputLineSize + i] >> 24;

				if (current < next)
					current = next;
				next = input[j * inputLineSize + (i - 1)] >> 24;
				if (current < next)
					current = next;
				next = input[j * inputLineSize + (i + 1)] >> 24;
				if (current < next)
					current = next;
				repl_alphaBlend(0, current >> 4, &output[j * outputLineSize + i]);
			}
		}
	}
}

static bool isD3D9ExDevice()
{
	IDirect3DDevice9Ex *deviceEx = nullptr;
	auto result = SokuLib::pd3dDev->QueryInterface(__uuidof(IDirect3DDevice9Ex), reinterpret_cast<void **>(&deviceEx));

	if (SUCCEEDED(result))
		deviceEx->Release();
	return SUCCEEDED(result);
}

bool createTextTexture(int &retId, const wchar_t* text, SokuLib::SWRFont& font, SokuLib::Vector2i texsize, SokuLib::Vector2i *size, bool sharp)
{
	auto strSize = wcslen(text);
	LPDIRECT3DTEXTURE9 *texPtr = SokuLib::textureMgr.allocate(&retId);
	LPDIRECT3DTEXTURE9 texPtr2 = nullptr;
	LPDIRECT3DTEXTURE9 uploadTexture = nullptr;
	LPDIRECT3DTEXTURE9 outputTexture = nullptr;
	LPDIRECT3DSURFACE9 surface;
	D3DLOCKED_RECT r1;
	D3DLOCKED_RECT r2;
	HRESULT ret;
	HDC context;
	SIZE actualSize;
	bool d3d9Ex = isD3D9ExDevice();
	auto cpuPool = d3d9Ex ? D3DPOOL_SYSTEMMEM : D3DPOOL_MANAGED;

	*texPtr = nullptr;
	EnterCriticalSection((LPCRITICAL_SECTION)0x8a0e14);
	ret = D3DXCreateTexture(SokuLib::pd3dDev, texsize.x, texsize.y, 1, 0, D3DFMT_X8R8G8B8, cpuPool, &texPtr2);
	LeaveCriticalSection((LPCRITICAL_SECTION)0x8a0e14);
	if (D3D_OK != ret) {
		puts("Error in D3DXCreateTexture XRGB");
		SokuLib::textureMgr.deallocate(retId);
		return false;
	}

	EnterCriticalSection((LPCRITICAL_SECTION)0x8a0e14);
	if (d3d9Ex) {
		ret = D3DXCreateTexture(SokuLib::pd3dDev, texsize.x, texsize.y, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &uploadTexture);
		if (SUCCEEDED(ret))
			ret = D3DXCreateTexture(SokuLib::pd3dDev, texsize.x, texsize.y, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, texPtr);
		outputTexture = uploadTexture;
	} else {
		ret = D3DXCreateTexture(SokuLib::pd3dDev, texsize.x, texsize.y, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, texPtr);
		outputTexture = *texPtr;
	}
	LeaveCriticalSection((LPCRITICAL_SECTION)0x8a0e14);
	if (D3D_OK != ret) {
		puts("Error in D3DXCreateTexture ARGB");
		if (uploadTexture)
			uploadTexture->Release();
		if (*texPtr)
			(*texPtr)->Release();
		*texPtr = nullptr;
		texPtr2->Release();
		SokuLib::textureMgr.deallocate(retId);
		return false;
	}

	if (D3D_OK != texPtr2->GetSurfaceLevel(0, &surface)) {
		puts("Error in GetSurfaceLevel");
		texPtr2->Release();
		if (uploadTexture)
			uploadTexture->Release();
		SokuLib::textureMgr.remove(retId);
		return false;
	}

	if (D3D_OK != surface->GetDC(&context)) {
		puts("Error in GetDC");
		surface->Release();
		texPtr2->Release();
		if (uploadTexture)
			uploadTexture->Release();
		SokuLib::textureMgr.remove(retId);
		return false;
	}

	PatBlt(context, 0, 0, texsize.x, texsize.y, BLACKNESS);
	font.maxWidth = texsize.x;
	font.maxHeight = texsize.y;
	setFont(context, font, sharp);
	SetBkColor(context, 0x000000);
	SetTextColor(context, 0xffffff);
	if (!TextOutW(context, font.description.offsetX, font.description.offsetY, text, strSize)) {
		puts("Error in TextOutW");
		unsetFont(context, font);
		surface->ReleaseDC(context);
		surface->Release();
		texPtr2->Release();
		if (uploadTexture)
			uploadTexture->Release();
		SokuLib::textureMgr.remove(retId);
		return false;
	}

	if (size) {
		if (!GetTextExtentPoint32W(context, text, strSize, &actualSize)) {
			puts("Error in GetTextExtentPoint32W");
			unsetFont(context, font);
			surface->ReleaseDC(context);
			surface->Release();
			texPtr2->Release();
			if (uploadTexture)
				uploadTexture->Release();
			SokuLib::textureMgr.remove(retId);
			return false;
		}
		size->x = actualSize.cx;
		size->y = actualSize.cy;
	}

	unsetFont(context, font);
	surface->ReleaseDC(context);
	surface->Release();

	if (D3D_OK != outputTexture->LockRect(0, &r1, nullptr, 0)) {
		puts("Error in LockRect 1");
		texPtr2->Release();
		if (uploadTexture)
			uploadTexture->Release();
		SokuLib::textureMgr.remove(retId);
		return false;
	}
	if (D3D_OK != texPtr2->LockRect(0, &r2, nullptr, 0)) {
		puts("Error in LockRect 2");
		outputTexture->UnlockRect(0);
		texPtr2->Release();
		if (uploadTexture)
			uploadTexture->Release();
		SokuLib::textureMgr.remove(retId);
		return false;
	}

	auto ptr1 = reinterpret_cast<SokuLib::DrawUtils::DxSokuColor *>(r1.pBits);
	auto ptr2 = reinterpret_cast<SokuLib::DrawUtils::DxSokuColor *>(r2.pBits);
	auto outputLineSize = r1.Pitch / sizeof(*ptr1);
	auto inputLineSize = r2.Pitch / sizeof(*ptr2);

	for (int y = 0; y < texsize.y; y++) {
		memset(ptr1 + y * outputLineSize, 0, texsize.x * sizeof(*ptr1));
		for (int x = 0; x < texsize.x; x++) {
			auto color = ptr2[y * inputLineSize + x];

			if (color) {
				auto mean = (color.r + color.g + color.b) / 3;

				(font.description.shadow ? ptr2 + y * inputLineSize : ptr1 + y * outputLineSize)[x] = SokuLib::Color{0xFF, 0xFF, 0xFF, static_cast<unsigned char>(mean)};
			}
		}
	}
	if (font.description.shadow)
		repl_textShadow(texsize.y, texsize.x, inputLineSize, outputLineSize, reinterpret_cast<unsigned *>(r2.pBits), reinterpret_cast<unsigned *>(r1.pBits));
	texPtr2->UnlockRect(0);
	outputTexture->UnlockRect(0);
	texPtr2->Release();
	if (d3d9Ex) {
		EnterCriticalSection((LPCRITICAL_SECTION)0x8a0e14);
		ret = SokuLib::pd3dDev->UpdateTexture(uploadTexture, *texPtr);
		LeaveCriticalSection((LPCRITICAL_SECTION)0x8a0e14);
		uploadTexture->Release();
		if (FAILED(ret)) {
			puts("Error in UpdateTexture");
			SokuLib::textureMgr.remove(retId);
			return false;
		}
	}
	return true;
}

SokuLib::Vector2i getTextSize(const wchar_t *text, SokuLib::SWRFont &font, SokuLib::Vector2i texsize, bool sharp)
{
	(void)texsize;
	SIZE actualSize{};
	if (!measurementContext.measure(text, font, sharp, 0, nullptr, actualSize))
		return {0, 0};
	return {
		actualSize.cx,
		actualSize.cy
	};
}

size_t getTextFit(const wchar_t *text, SokuLib::SWRFont &font, int maxWidth, bool sharp)
{
	if (maxWidth < 0)
		return 0;
	SIZE actualSize{};
	int fit = 0;
	if (!measurementContext.measure(text, font, sharp, maxWidth, &fit, actualSize))
		return 0;
	return static_cast<size_t>(fit);
}
