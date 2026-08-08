#include "RegistryKey.h"

namespace Registry {

    RegistryKey::~RegistryKey() {
        Close();
    }

    RegistryKey::RegistryKey(RegistryKey&& other) noexcept : m_hKey(other.m_hKey) {
        other.m_hKey = nullptr;
    }

    RegistryKey& RegistryKey::operator=(RegistryKey&& other) noexcept {
        if (this != &other) {
            Close();
            m_hKey = other.m_hKey;
            other.m_hKey = nullptr;
        }
        return *this;
    }

    bool RegistryKey::Open(HKEY hKeyRoot, const std::wstring& subKey, REGSAM samDesired) {
        Close();
        LSTATUS status = RegOpenKeyExW(hKeyRoot, subKey.c_str(), 0, samDesired, &m_hKey);
        return status == ERROR_SUCCESS;
    }

    void RegistryKey::Close() {
        if (m_hKey) {
            RegCloseKey(m_hKey);
            m_hKey = nullptr;
        }
    }

    std::optional<std::wstring> RegistryKey::ReadString(const std::wstring& valueName) {
        if (!IsValid()) return std::nullopt;

        DWORD type = 0;
        DWORD dataSize = 0;
        LSTATUS status = RegQueryValueExW(m_hKey, valueName.c_str(), nullptr, &type, nullptr, &dataSize);
        
        if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
            return std::nullopt;
        }

        std::wstring result(dataSize / sizeof(wchar_t), L'\0');
        status = RegQueryValueExW(m_hKey, valueName.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(&result[0]), &dataSize);
        
        if (status == ERROR_SUCCESS) {
            // Remove null terminator if present in the string length
            if (!result.empty() && result.back() == L'\0') {
                result.pop_back();
            }
            return result;
        }
        return std::nullopt;
    }

    std::optional<DWORD> RegistryKey::ReadDword(const std::wstring& valueName) {
        if (!IsValid()) return std::nullopt;

        DWORD type = 0;
        DWORD data = 0;
        DWORD dataSize = sizeof(DWORD);
        LSTATUS status = RegQueryValueExW(m_hKey, valueName.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(&data), &dataSize);
        
        if (status == ERROR_SUCCESS && type == REG_DWORD) {
            return data;
        }
        return std::nullopt;
    }

    std::optional<std::vector<uint8_t>> RegistryKey::ReadBinary(const std::wstring& valueName) {
        if (!IsValid()) return std::nullopt;

        DWORD type = 0;
        DWORD dataSize = 0;
        LSTATUS status = RegQueryValueExW(m_hKey, valueName.c_str(), nullptr, &type, nullptr, &dataSize);
        
        if (status != ERROR_SUCCESS || (type != REG_BINARY && type != REG_NONE)) {
            return std::nullopt;
        }

        std::vector<uint8_t> buffer(dataSize);
        status = RegQueryValueExW(m_hKey, valueName.c_str(), nullptr, &type, buffer.data(), &dataSize);
        
        if (status == ERROR_SUCCESS) {
            return buffer;
        }
        return std::nullopt;
    }

    std::vector<RegistryKey::SubKeyInfo> RegistryKey::EnumerateSubKeys() {
        std::vector<SubKeyInfo> subkeys;
        if (!IsValid()) return subkeys;

        DWORD index = 0;
        const DWORD MAX_KEY_LENGTH = 255;
        wchar_t keyName[MAX_KEY_LENGTH];
        DWORD keyNameSize;
        FILETIME lastWriteTime;

        while (true) {
            keyNameSize = MAX_KEY_LENGTH;
            LSTATUS status = RegEnumKeyExW(m_hKey, index, keyName, &keyNameSize, nullptr, nullptr, nullptr, &lastWriteTime);
            
            if (status == ERROR_SUCCESS) {
                subkeys.push_back({ std::wstring(keyName, keyNameSize), lastWriteTime });
                index++;
            } else if (status == ERROR_NO_MORE_ITEMS) {
                break;
            } else {
                // Handle error if needed
                break;
            }
        }
        return subkeys;
    }

    std::vector<std::wstring> RegistryKey::EnumerateValues() {
        std::vector<std::wstring> values;
        if (!IsValid()) return values;

        DWORD index = 0;
        const DWORD MAX_VALUE_NAME = 16383;
        wchar_t valueName[MAX_VALUE_NAME];
        DWORD valueNameSize;

        while (true) {
            valueNameSize = MAX_VALUE_NAME;
            LSTATUS status = RegEnumValueW(m_hKey, index, valueName, &valueNameSize, nullptr, nullptr, nullptr, nullptr);
            
            if (status == ERROR_SUCCESS) {
                values.push_back(std::wstring(valueName, valueNameSize));
                index++;
            } else if (status == ERROR_NO_MORE_ITEMS) {
                break;
            } else {
                break;
            }
        }
        return values;
    }
}
