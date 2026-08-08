#include "ErrorHandling.h"
#include "StringUtils.h"

namespace Utils {
    std::string GetLastErrorAsString() {
        DWORD errorMessageID = ::GetLastError();
        if (errorMessageID == 0) {
            return std::string();
        }

        LPWSTR messageBuffer = nullptr;
        size_t size = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, NULL);
        
        std::string message(WStringToString(std::wstring(messageBuffer, size)));
        LocalFree(messageBuffer);

        return message;
    }
}
