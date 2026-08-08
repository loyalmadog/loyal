#pragma once
#include <string>
#include <windows.h>

namespace Utils {
    std::string WStringToString(const std::wstring& wstr);
    std::wstring StringToWString(const std::string& str);
}
