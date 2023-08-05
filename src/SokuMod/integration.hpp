#include <windows.h>
#include <locale>

namespace th123intl {
    inline unsigned int GetFileSystemCodePage() { return GetACP(); }
    inline unsigned int GetTextCodePage() {
        auto handle = GetModuleHandleA("th123intl.dll");
        FARPROC proc = 0;
        if (handle) proc = GetProcAddress(handle, "GetTextCodePage");
        if (!handle || !proc) return 932;
        return proc();
    }

    template <_locale_t* curLocale>
    _locale_t GetLocale() {
        if (*curLocale) return *curLocale;
        auto handle = GetModuleHandleA("th123intl.dll");
        FARPROC proc = 0;
        if (handle) proc = GetProcAddress(handle, "GetLocale");
        if (!handle || !proc) {
            if (!*curLocale) *curLocale = _create_locale(LC_ALL, "ja_JP");
            return *curLocale;
        }
        return *curLocale = (_locale_t)proc();
    }

    template<int inFlags = MB_USEGLYPHCHARS, int outFlags = WC_COMPOSITECHECK|WC_DEFAULTCHAR>
    inline void ConvertCodePage(unsigned int fromCP, const std::string_view& from, unsigned int toCP, std::string& to) {
        if (fromCP == toCP) { to = from; return; }
        else {
            std::wstring buffer;
            size_t size = MultiByteToWideChar(fromCP, inFlags, from.data(), from.size(), NULL, NULL);
            buffer.resize(size);
            if (!MultiByteToWideChar(fromCP, inFlags, from.data(), from.size(), buffer.data(), buffer.size()))
                { to.resize(0); return; }

            size = WideCharToMultiByte(toCP, outFlags, buffer.data(), -1, NULL, NULL, NULL, NULL);
            to.resize(size);
            if (!WideCharToMultiByte(toCP, outFlags, buffer.data(), -1, to.data(), to.size(), NULL, NULL))
                { to.resize(0); return; }
            to.resize(size-1);
        }
    }

    template<int flags = WC_COMPOSITECHECK|WC_DEFAULTCHAR>
    inline void ConvertCodePage(const std::wstring_view& from, unsigned int toCP, std::string& to) {
        size_t size = WideCharToMultiByte(toCP, flags, from.data(), from.size(), NULL, NULL, NULL, NULL);
        to.resize(size);
        if (!WideCharToMultiByte(toCP, flags, from.data(), from.size(), to.data(), to.size(), NULL, NULL))
            { to.resize(0); return; }
        to.resize(size-1);
    }

    template<int flags = MB_USEGLYPHCHARS>
    inline void ConvertCodePage(unsigned int fromCP, const std::string_view& from, std::wstring& to) {
        size_t size = MultiByteToWideChar(fromCP, flags, from.data(), from.size(), NULL, NULL);
        to.resize(size);
        if (!MultiByteToWideChar(fromCP, flags, from.data(), from.size(), to.data(), to.size()))
            { to.resize(0); return; }
        to.resize(size-1);
    }
}