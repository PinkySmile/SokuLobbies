//
// Created by PinkySmile on 29/01/2023.
//

#include "createUTFTexture.hpp"

static void setFont(HDC context, SokuLib::SWRFont &font)
{
	font.font = CreateFontA(font.description.height, 0, 0, 0, font.description.weight, font.description.italic, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FIXED_PITCH|FF_MODERN, font.description.faceName);
	font.hdc = context;
	font.gdiobj = SelectObject(context, font.font);
}

bool createTextTexture(unsigned int& retId, const wchar_t* text, SokuLib::SWRFont& font, SokuLib::Vector2i texsize, SokuLib::Vector2i *size)
{
	printf("Creating texture for wtext %S\n", text);
	auto handleMgr = (SokuLib::HandleManager<LPDIRECT3DTEXTURE9> *)&SokuLib::textureMgr;
	auto strSize = wcslen(text);
	LPDIRECT3DTEXTURE9 *texPtr = handleMgr->Allocate(retId);
	LPDIRECT3DSURFACE9 surface;
	HRESULT ret;
	HDC context;
	SIZE actualSize;

	*texPtr = nullptr;
	EnterCriticalSection((LPCRITICAL_SECTION)0x8a0e14);
	ret = D3DXCreateTexture(SokuLib::pd3dDev, texsize.x, texsize.y, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, texPtr);
	LeaveCriticalSection((LPCRITICAL_SECTION)0x8a0e14);
	if (D3D_OK != ret) {
		puts("Error in D3DXCreateTexture");
		handleMgr->Deallocate(retId);
		return false;
	}

	if (D3D_OK != (*texPtr)->GetSurfaceLevel(0, &surface)) {
		puts("Error in GetSurfaceLevel");
		handleMgr->Deallocate(retId);
		return false;
	}

	if (D3D_OK != surface->GetDC(&context)) {
		puts("Error in GetDC");
		handleMgr->Deallocate(retId);
		return false;
	}

	font.maxWidth = texsize.x;
	font.maxHeight = texsize.y;
	setFont(context, font);
	if (!TextOutW(context, font.description.offsetX, font.description.offsetY, text, strSize)) {
		puts("Error in TextOutW");
		handleMgr->Deallocate(retId);
		return false;
	}

	if (size) {
		if (!GetTextExtentPoint32W(context, text, strSize, &actualSize)) {
			puts("Error in GetTextExtentPoint32W");
			handleMgr->Deallocate(retId);
			return false;
		}
		size->x = actualSize.cx;
		size->y = actualSize.cy;
	}

	DeleteObject(font.gdiobj);
	surface->ReleaseDC(context);
	font.gdiobj = nullptr;
	font.font = nullptr;
	font.hdc = nullptr;
	return true;
}