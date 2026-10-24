#pragma once

#include <Windows.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace WindowsUtils
{
    // Engine strings are UTF-8. Wide strings are used only at Windows API boundaries.
    inline std::wstring ToWstring(std::string_view Text)
    {
        if (Text.empty()) return {};
        if (Text.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
            throw std::length_error("UTF-8 string is too long");
        const int Length = static_cast<int>(Text.size());
        const int Count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Text.data(), Length, nullptr, 0);
        if (!Count) throw std::system_error(GetLastError(), std::system_category(), "MultiByteToWideChar");
        std::wstring Result(Count, L'\0');
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Text.data(), Length, Result.data(), Count))
            throw std::system_error(GetLastError(), std::system_category(), "MultiByteToWideChar");
        return Result;
    }

    inline std::string ToString(std::wstring_view Text)
    {
        if (Text.empty()) return {};
        if (Text.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
            throw std::length_error("UTF-16 string is too long");
        const int Length = static_cast<int>(Text.size());
        const int Count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, Text.data(), Length, nullptr, 0, nullptr, nullptr);
        if (!Count) throw std::system_error(GetLastError(), std::system_category(), "WideCharToMultiByte");
        std::string Result(Count, '\0');
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, Text.data(), Length, Result.data(), Count, nullptr, nullptr))
            throw std::system_error(GetLastError(), std::system_category(), "WideCharToMultiByte");
        return Result;
    }
}
