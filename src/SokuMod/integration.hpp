#include <windows.h>
#include <locale>

#ifndef TH123INTL_HPP
#define TH123INTL_HPP

namespace th123intl {
    inline unsigned int GetFileSystemCodePage() { return GetACP(); }
    inline unsigned int GetTextCodePage() {
        auto handle = GetModuleHandleA("th123intl.dll");
        FARPROC proc = 0;
        if (handle) proc = GetProcAddress(handle, "GetTextCodePage");
        if (!handle || !proc) return 932;
        return proc();
    }

    template <_locale_t& curLocale>
    _locale_t GetLocale() {
        if (curLocale) return curLocale;
        auto handle = GetModuleHandleA("th123intl.dll");
        FARPROC proc = 0;
        if (handle) proc = GetProcAddress(handle, "GetLocale");
        if (!handle || !proc) {
            if (!curLocale) curLocale = _create_locale(LC_ALL, "ja_JP");
            return curLocale;
        }
        return curLocale = (_locale_t)proc();
    }

    template<int inFlags = MB_USEGLYPHCHARS, int outFlags = WC_COMPOSITECHECK|WC_DEFAULTCHAR>
    inline void ConvertCodePage(unsigned int fromCP, const std::string_view& from, unsigned int toCP, std::string& to) {
        if (fromCP == toCP) { to = from; return; }
        else {
            DWORD dwFlags = fromCP == CP_UTF8 ? inFlags & WC_ERR_INVALID_CHARS : inFlags;
            std::wstring buffer;
            size_t size = MultiByteToWideChar(fromCP, dwFlags, from.data(), from.size(), NULL, NULL);
            buffer.resize(size);
            if (!MultiByteToWideChar(fromCP, dwFlags, from.data(), from.size(), buffer.data(), buffer.size()))
                { to.resize(0); return; }

            dwFlags = toCP == CP_UTF8 ? outFlags & WC_ERR_INVALID_CHARS : outFlags;
            size = WideCharToMultiByte(toCP, dwFlags, buffer.data(), buffer.size(), NULL, NULL, NULL, NULL);
            to.resize(size);
            if (!WideCharToMultiByte(toCP, dwFlags, buffer.data(), buffer.size(), to.data(), to.size(), NULL, NULL))
                { to.resize(0); return; }
        }
    }

    template<int flags = WC_COMPOSITECHECK|WC_DEFAULTCHAR>
    inline void ConvertCodePage(const std::wstring_view& from, unsigned int toCP, std::string& to) {
        const DWORD dwFlags = toCP == CP_UTF8 ? flags & WC_ERR_INVALID_CHARS : flags;
        size_t size = WideCharToMultiByte(toCP, dwFlags, from.data(), from.size(), NULL, NULL, NULL, NULL);
        to.resize(size);
        if (!WideCharToMultiByte(toCP, dwFlags, from.data(), from.size(), to.data(), to.size(), NULL, NULL))
            { to.resize(0); return; }
    }

    template<int flags = MB_USEGLYPHCHARS>
    inline void ConvertCodePage(unsigned int fromCP, const std::string_view& from, std::wstring& to) {
        const DWORD dwFlags = fromCP == CP_UTF8 ? flags & WC_ERR_INVALID_CHARS : flags;
        size_t size = MultiByteToWideChar(fromCP, dwFlags, from.data(), from.size(), NULL, NULL);
        to.resize(size);
        if (!MultiByteToWideChar(fromCP, dwFlags, from.data(), from.size(), to.data(), to.size()))
            { to.resize(0); return; }
    }
}

#endif
