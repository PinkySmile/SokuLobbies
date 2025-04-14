//
// Created by PinkySmile on 29/01/2023.
//

#include "createUTFTexture.hpp"

bool textureUnderline = false;

static void setFont(HDC context, SokuLib::SWRFont &font)
{
	font.font = CreateFontA(
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
		PROOF_QUALITY,
		FIXED_PITCH | FF_MODERN,
		font.description.faceName
	);
	font.hdc = context;
	font.gdiobj = SelectObject(context, font.font);
}

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

static void __fastcall repl_textShadow(int height, int width, int lineSizeInput, int lineSizeOutput, const unsigned int* input, unsigned int* output) {
	width -= 1;
	height -= 1;

	for (int j = 1; j < height; ++j) {
		for (int i = 1; i < width; ++i) {
			unsigned int c0 = input[j * lineSizeInput + i];

			if (c0 >> 24) {
				unsigned alpha = c0 >> 27;
				unsigned int c1 = 0xff000000;

				if (alpha > 16)
					alpha = 16;
				repl_alphaBlend(c0, alpha, &c1);
				repl_alphaBlend(c1, 16, &output[j * lineSizeOutput + i]);
			} else {
				unsigned char current = input[(j - 1) * lineSizeInput + i] >> 24;
				unsigned char next = input[(j + 1) * lineSizeInput + i] >> 24;

				if (current < next)
					current = next;
				next = input[j * lineSizeInput + (i - 1)] >> 24;
				if (current < next)
					current = next;
				next = input[j * lineSizeInput + (i + 1)] >> 24;
				if (current < next)
					current = next;
				repl_alphaBlend(0, current >> 4, &output[j * lineSizeOutput + i]);
			}
		}
	}
}

bool createTextTexture(int &retId, const wchar_t* text, SokuLib::SWRFont& font, SokuLib::Vector2i texsize, SokuLib::Vector2i *size)
{
	printf("Creating texture for wtext %S\n", text);
	auto strSize = wcslen(text);
	LPDIRECT3DTEXTURE9 *texPtr = SokuLib::textureMgr.allocate(&retId);
	LPDIRECT3DTEXTURE9 texPtr2;
	LPDIRECT3DSURFACE9 surface;
	D3DLOCKED_RECT r1;
	D3DLOCKED_RECT r2;
	HRESULT ret;
	HDC context;
	SIZE actualSize;

	*texPtr = nullptr;
	EnterCriticalSection((LPCRITICAL_SECTION)0x8a0e14);
	ret = D3DXCreateTexture(SokuLib::pd3dDev, texsize.x, texsize.y, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &texPtr2);
	LeaveCriticalSection((LPCRITICAL_SECTION)0x8a0e14);
	if (D3D_OK != ret) {
		puts("Error in D3DXCreateTexture XRGB");
		SokuLib::textureMgr.deallocate(retId);
		return false;
	}

	EnterCriticalSection((LPCRITICAL_SECTION)0x8a0e14);
	ret = D3DXCreateTexture(SokuLib::pd3dDev, texsize.x, texsize.y, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, texPtr);
	LeaveCriticalSection((LPCRITICAL_SECTION)0x8a0e14);
	if (D3D_OK != ret) {
		puts("Error in D3DXCreateTexture ARGB");
		texPtr2->Release();
		SokuLib::textureMgr.deallocate(retId);
		return false;
	}

	if (D3D_OK != texPtr2->LockRect(0, &r2, nullptr, 0)) {
		puts("Error in LockRect 2");
		texPtr2->Release();
		SokuLib::textureMgr.deallocate(retId);
		return false;
	}
	for (int y = 0; y < texsize.y ;y++)
		memset((char *)r2.pBits + r2.Pitch * y, 0, texsize.x * 4);
	assert(SUCCEEDED(texPtr2->UnlockRect(0)));

	if (D3D_OK != texPtr2->GetSurfaceLevel(0, &surface)) {
		puts("Error in GetSurfaceLevel");
		texPtr2->Release();
		SokuLib::textureMgr.deallocate(retId);
		return false;
	}

	if (D3D_OK != surface->GetDC(&context)) {
		puts("Error in GetDC");
		texPtr2->Release();
		SokuLib::textureMgr.deallocate(retId);
		return false;
	}

	font.maxWidth = texsize.x;
	font.maxHeight = texsize.y;
	setFont(context, font);
	SetBkColor(context, 0x000000);
	SetTextColor(context, 0xffffff);
	if (!TextOutW(context, font.description.offsetX, font.description.offsetY, text, strSize)) {
		puts("Error in TextOutW");
		texPtr2->Release();
		SokuLib::textureMgr.deallocate(retId);
		return false;
	}

	if (size) {
		if (!GetTextExtentPoint32W(context, text, strSize, &actualSize)) {
			puts("Error in GetTextExtentPoint32W");
			texPtr2->Release();
			SokuLib::textureMgr.deallocate(retId);
			return false;
		}
		size->x = actualSize.cx;
		size->y = actualSize.cy;
	}

	DeleteObject(SelectObject(context, font.gdiobj));
	surface->ReleaseDC(context);
	surface->Release();
	font.gdiobj = nullptr;
	font.font = nullptr;
	font.hdc = nullptr;

	if (D3D_OK != (*texPtr)->LockRect(0, &r1, nullptr, 0)) {
		puts("Error in LockRect 1");
		texPtr2->Release();
		SokuLib::textureMgr.deallocate(retId);
		return false;
	}
	for (int y = 0; y < texsize.y; y++)
		memset((char *)r1.pBits + r1.Pitch * y, 0, texsize.x * 4);

	if (D3D_OK != texPtr2->LockRect(0, &r2, nullptr, 0)) {
		puts("Error in LockRect 2");
		(*texPtr)->UnlockRect(0);
		texPtr2->Release();
		SokuLib::textureMgr.deallocate(retId);
		return false;
	}

	auto ptr1 = reinterpret_cast<SokuLib::DrawUtils::DxSokuColor *>(r1.pBits);
	auto ptr2 = reinterpret_cast<SokuLib::DrawUtils::DxSokuColor *>(r2.pBits);

	// Should we even assume it aligned to 4? Soku does assume it, but I don't found any document that confirms it.
	assert(r1.Pitch % 4 == 0 && r2.Pitch % 4 ==0);
	auto lineSize1 = r1.Pitch / 4;
	auto lineSize2 = r2.Pitch / 4;
	for (int y = 0; y < texsize.y; y++) {
		for (int x = 0; x < texsize.x; x++) {
			auto color = ptr2[x + y * lineSize2];

			if (color) {
				auto mean = (color.r + color.g + color.b) / 3;
				auto src = SokuLib::Color{0xFF, 0xFF, 0xFF, static_cast<unsigned char>(mean)};
				if (font.description.shadow)
					ptr2[x + y * lineSize2] = src;
				else
				    ptr1[x + y * lineSize1] = src;
			}
		}
	}
	if (font.description.shadow)
		repl_textShadow(texsize.y, texsize.x, lineSize2, lineSize1, reinterpret_cast<unsigned *>(r2.pBits), reinterpret_cast<unsigned *>(r1.pBits));
	texPtr2->UnlockRect(0);
	(*texPtr)->UnlockRect(0);
	texPtr2->Release();
	return true;
}

SokuLib::Vector2i getTextSize(const wchar_t *text, SokuLib::SWRFont &font, SokuLib::Vector2i texsize)
{
	auto strSize = wcslen(text);
	LPDIRECT3DTEXTURE9 texPtr2;
	LPDIRECT3DSURFACE9 surface;
	HRESULT ret;
	HDC context;
	SIZE actualSize;

	EnterCriticalSection((LPCRITICAL_SECTION)0x8a0e14);
	ret = D3DXCreateTexture(SokuLib::pd3dDev, texsize.x, texsize.y, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &texPtr2);
	LeaveCriticalSection((LPCRITICAL_SECTION)0x8a0e14);
	if (D3D_OK != ret) {
		puts("Error in D3DXCreateTexture XRGB");
		return {0, 0};
	}

	if (D3D_OK != texPtr2->GetSurfaceLevel(0, &surface)) {
		puts("Error in GetSurfaceLevel");
		texPtr2->Release();
		return {0, 0};
	}

	if (D3D_OK != surface->GetDC(&context)) {
		puts("Error in GetDC");
		texPtr2->Release();
		return {0, 0};
	}

	font.maxWidth = texsize.x;
	font.maxHeight = texsize.y;
	setFont(context, font);
	if (!GetTextExtentPoint32W(context, text, strSize, &actualSize)) {
		puts("Error in GetTextExtentPoint32W");
		texPtr2->Release();
		return {0, 0};
	}

	DeleteObject(SelectObject(context, font.gdiobj));
	surface->ReleaseDC(context);
	surface->Release();
	font.gdiobj = nullptr;
	font.font = nullptr;
	font.hdc = nullptr;
	texPtr2->Release();
	return {
		actualSize.cx,
		actualSize.cy
	};
}