#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <optional>

namespace Registry {
    class RegistryKey {
    public:
        RegistryKey() = default;
        ~RegistryKey();

        
        RegistryKey(const RegistryKey&) = delete;
        RegistryKey& operator=(const RegistryKey&) = delete;

        
        RegistryKey(RegistryKey&& other) noexcept;
        RegistryKey& operator=(RegistryKey&& other) noexcept;

        bool Open(HKEY hKeyRoot, const std::wstring& subKey, REGSAM samDesired = KEY_READ);
        void Close();
        bool IsValid() const { return m_hKey != nullptr; }

        std::optional<std::wstring> ReadString(const std::wstring& valueName);
        std::optional<DWORD> ReadDword(const std::wstring& valueName);
        std::optional<std::vector<uint8_t>> ReadBinary(const std::wstring& valueName);

        struct SubKeyInfo {
            std::wstring Name;
            FILETIME LastWriteTime;
        };
        std::vector<SubKeyInfo> EnumerateSubKeys();
        std::vector<std::wstring> EnumerateValues();

    private:
        HKEY m_hKey = nullptr;
    };
}
