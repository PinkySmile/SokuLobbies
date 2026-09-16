//
// Created by PinkySmile on 29/01/2023.
//

#ifndef SOKULOBBIES_CREATEUTFTEXTURE_HPP
#define SOKULOBBIES_CREATEUTFTEXTURE_HPP

#include <cstddef>
#include <SokuLib.hpp>

bool createTextTexture(int &retId, const wchar_t* text, SokuLib::SWRFont& font, SokuLib::Vector2i texsize, SokuLib::Vector2i *size, bool sharp = false);
SokuLib::Vector2i getTextSize(const wchar_t *text, SokuLib::SWRFont &font, SokuLib::Vector2i texsize, bool sharp = false);
size_t getTextFit(const wchar_t *text, SokuLib::SWRFont &font, int maxWidth, bool sharp = false);

extern bool textureUnderline;

#endif //SOKULOBBIES_CREATEUTFTEXTURE_HPP
