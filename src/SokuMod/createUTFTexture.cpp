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

	DeleteObject(font.font);
	DeleteObject(font.gdiobj);
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
	if (D3D_OK != texPtr2->LockRect(0, &r2, nullptr, 0)) {
		puts("Error in LockRect 2");
		texPtr2->Release();
		SokuLib::textureMgr.deallocate(retId);
		return false;
	}

	auto ptr1 = reinterpret_cast<SokuLib::DrawUtils::DxSokuColor *>(r1.pBits);
	auto ptr2 = reinterpret_cast<SokuLib::DrawUtils::DxSokuColor *>(r2.pBits);

	for (int i = 0; i < texsize.x * texsize.y; i++) {
		auto color = ptr2[i];

		if (color) {
			auto mean = (color.r + color.g + color.b) / 3;

			ptr1[i] = SokuLib::Color{0xFF, 0xFF, 0xFF, static_cast<unsigned char>(mean)};
		}
	}
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

	DeleteObject(font.font);
	DeleteObject(font.gdiobj);
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