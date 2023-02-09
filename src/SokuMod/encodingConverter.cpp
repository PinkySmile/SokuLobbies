//
// Created by PinkySmile on 29/01/2023.
//

#include "encodingConverter.hpp"
#include "shiftJISConvTable.inl"
#include "shiftJISReverseConvTable.inl"

std::basic_string<unsigned> shiftJISDecode(const std::string &str)
{
	std::basic_string<unsigned> output;
	auto bytes = reinterpret_cast<const unsigned char *>(str.c_str());

	output.reserve(str.size());
	for (size_t i = 0; i < str.size(); i++) {
		unsigned char arraySection = (bytes[i] >> 4U);
		unsigned short arrayOffset;

		if (arraySection == 0x8 || arraySection == 0x9 || arraySection == 0xE) {
			arrayOffset = bytes[i] << 8U;
			i++;
			if (i < str.size())
				arrayOffset |= bytes[i];
			else {
				output.push_back(0xFFFD);
				break;
			}
		} else
			arrayOffset = bytes[i];

		uint16_t unicodeValue = 0xFFFD;
		auto it = shiftJISConvTable.find(arrayOffset);

		if (it != shiftJISConvTable.end())
			unicodeValue = it->second;
		output.push_back(unicodeValue);
	}
	output.shrink_to_fit();
	return output;
}

std::basic_string<unsigned> UTF8Decode(const std::string &str)
{
	std::basic_string<unsigned> output;
	auto bytes = reinterpret_cast<const unsigned char *>(str.c_str());

	output.reserve(str.size());
	for (size_t i = 0; i < str.size(); i++) {
		if (bytes[i] < 0x80)
			output.push_back(bytes[i]);
		else if (bytes[i] < 0xC0)
			output.push_back(0xFFFD);
		else if (bytes[i] < 0xE0) {
			if (i + 1 >= str.size()) {
				output.push_back(0xFFFC);
				output.shrink_to_fit();
				return output;
			}
			if ((bytes[i + 1] & 0b11000000U) != 0x80U) {
				output.push_back(0xFFFD);
				continue;
			}

			auto pt = ((bytes[i] & 0b11111) << 6) | (bytes[i + 1] & 0b111111);

			if (pt < 0x80)
				// Overlong encoding
				output.push_back(0xFFFD);
			else
				output.push_back(pt);
			i += 1;
		} else if (bytes[i] < 0xF0) {
			if (i + 2 >= str.size()) {
				output.push_back(0xFFFC);
				output.shrink_to_fit();
				return output;
			}
			if ((bytes[i + 1] & 0b11000000U) != 0x80U || (bytes[i + 2] & 0b11000000U) != 0x80U) {
				output.push_back(0xFFFD);
				i += (bytes[i + 1] & 0b11000000U) == 0x80U;
				continue;
			}

			auto pt = ((bytes[i] & 0b1111) << 12) | ((bytes[i + 1] & 0b111111) << 6) | (bytes[i + 2] & 0b111111);

			if (pt < 0x800)
				// Overlong encoding
				output.push_back(0xFFFD);
			else
				output.push_back(pt);
			i += 2;
		} else {
			if (i + 3 >= str.size()) {
				output.push_back(0xFFFC);
				output.shrink_to_fit();
				return output;
			}
			if ((bytes[i + 1] & 0b11000000U) != 0x80U || (bytes[i + 2] & 0b11000000U) != 0x80U || (bytes[i + 3] & 0b11000000U) != 0x80U) {
				output.push_back(0xFFFD);
				if ((bytes[i + 1] & 0b11000000U) == 0x80U)
					i += 1 + ((bytes[i + 2] & 0b11000000U) == 0x80U);
				continue;
			}

			auto pt = ((bytes[i] & 0b111) << 16) |
				((bytes[i + 1] & 0b111111) << 12) |
				((bytes[i + 2] & 0b111111) << 6) |
				(bytes[i + 3] & 0b111111);

			if (pt < 0x10000)
				// Overlong encoding
				output.push_back(0xFFFD);
			else if (pt > 0x10FFFF)
				// Invalid code points
				output.push_back(0xFFFD);
			else
				output.push_back(pt);
			i += 3;
		}
	}
	output.shrink_to_fit();
	return output;
}

std::basic_string<unsigned> UTF16Decode(const std::wstring &str)
{
	std::basic_string<unsigned> output;
	auto bytes = reinterpret_cast<const unsigned short *>(str.c_str());

	output.reserve(str.size());
	for (size_t i = 0; i < str.size(); i++) {
		if (bytes[i] > 0xD800 && bytes[i] <= 0xDBFF) {
			// End of string is next
			if (i + 1 >= str.size()) {
				output.push_back(0xFFFD);
				continue;
			}
			// Next is not a valid continuation point
			if (bytes[i + 1] < 0xDC00 || bytes[i + 1] > 0xDFFF) {
				output.push_back(0xFFFD);
				continue;
			}

			auto pt = ((bytes[i] & 0x3FF) << 10) | (bytes[i + 1] & 0x3FF);

			if (pt < 0x10000)
				// Overlong encoding
				output.push_back(0xFFFD);
			else
				output.push_back(pt);
			i++;
		} else if (bytes[i] > 0xDC00 && bytes[i] <= 0xDFFF)
			// Unexpected continuation bytes
			output.push_back(0xFFFD);
		else
			output.push_back(bytes[i]);
	}
	output.shrink_to_fit();
	return output;
}

std::string shiftJISEncode(const std::basic_string<unsigned> &str)
{
	std::string output;

	output.reserve(str.size() * 2);
	for (auto unicodeValue : str) {
		// We filter out invalid code points
		if (unicodeValue > 0x10FFFF)
			unicodeValue = 0xFFFD;
		if (unicodeValue >= 0xD800 && unicodeValue <= 0xDFFF)
			unicodeValue = 0xFFFD;

		auto it = shiftJISReverseConvTable.find(unicodeValue);
		uint16_t value = 0xFFFD;

		if (it != shiftJISReverseConvTable.end())
			value = it->second;
		if (value > 0xFF)
			output.push_back(value >> 8U);
		output.push_back(value);
	}
	output.shrink_to_fit();
	return output;
}

std::string UTF8Encode(const std::basic_string<unsigned> &str)
{
	std::string output;

	output.reserve(str.size() * 4);
	for (auto unicodeValue : str) {
		// We filter out invalid code points
		if (unicodeValue > 0x10FFFF)
			unicodeValue = 0xFFFD;
		if (unicodeValue >= 0xD800 && unicodeValue <= 0xDFFF)
			unicodeValue = 0xFFFD;

		if (unicodeValue < 0x80)
			output.push_back(unicodeValue);
		else if (unicodeValue < 0x800) {
			output.push_back(0xC0U | ((unicodeValue >> 6U) & 0b00011111U));
			output.push_back(0x80U | ((unicodeValue >> 0U) & 0b00111111U));
		} else if (unicodeValue < 0x10000) {
			output.push_back(0xE0U | ((unicodeValue >> 12U)& 0b00001111U));
			output.push_back(0x80U | ((unicodeValue >> 6U) & 0b00111111U));
			output.push_back(0x80U | ((unicodeValue >> 0U) & 0b00111111U));
		} else {
			output.push_back(0xF0U | ((unicodeValue >> 18U) & 0b00000111U));
			output.push_back(0x80U | ((unicodeValue >> 12U) & 0b00111111U));
			output.push_back(0x80U | ((unicodeValue >> 6U)  & 0b00111111U));
			output.push_back(0x80U | ((unicodeValue >> 0U)  & 0b00111111U));
		}
	}
	output.shrink_to_fit();
	return output;
}

std::wstring UTF16Encode(const std::basic_string<unsigned> &str)
{
	std::wstring output;

	output.reserve(str.size() * 2);
	for (auto unicodeValue : str) {
		// We filter out invalid code points
		if (unicodeValue > 0x10FFFF)
			unicodeValue = 0xFFFD;
		if (unicodeValue >= 0xD800 && unicodeValue <= 0xDFFF)
			unicodeValue = 0xFFFD;

		if (unicodeValue <= 0xFFFF)
			output.push_back(unicodeValue);
		else {
			output.push_back(0xC0U | ((unicodeValue >> 6U) & 0b00011111U));
			output.push_back(0x80U | ((unicodeValue >> 0U) & 0b00111111U));
		}
	}
	output.shrink_to_fit();
	return output;
}
