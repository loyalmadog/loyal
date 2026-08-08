#pragma once
#include <string>
#include <windows.h>

namespace Utils {
    std::string FileTimeToStdString(const FILETIME& ft);
    std::string SystemTimeToStdString(const SYSTEMTIME& st);
}
