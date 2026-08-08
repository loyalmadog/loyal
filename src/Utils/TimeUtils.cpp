#include "TimeUtils.h"
#include <iomanip>
#include <sstream>

namespace Utils {
    std::string SystemTimeToStdString(const SYSTEMTIME& st) {
        std::stringstream ss;
        ss << std::setfill('0') 
           << std::setw(4) << st.wYear << "-"
           << std::setw(2) << st.wMonth << "-"
           << std::setw(2) << st.wDay << " "
           << std::setw(2) << st.wHour << ":"
           << std::setw(2) << st.wMinute << ":"
           << std::setw(2) << st.wSecond;
        return ss.str();
    }

    std::string FileTimeToStdString(const FILETIME& ft) {
        if (ft.dwHighDateTime == 0 && ft.dwLowDateTime == 0) {
            return "N/A";
        }

        SYSTEMTIME stUTC, stLocal;
        FileTimeToSystemTime(&ft, &stUTC);
        SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
        
        return SystemTimeToStdString(stLocal);
    }
}
