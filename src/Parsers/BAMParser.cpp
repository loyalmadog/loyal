#include "BAMParser.h"
#include "../Registry/RegistryKey.h"
#include "../Utils/TimeUtils.h"
#include "../Utils/StringUtils.h"
#include "spdlog/spdlog.h"

#include <windows.h>
#include <wintrust.h>
#include <softpub.h>
#include <tlhelp32.h>

#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")

namespace Parsers {

    
    static bool IsMicrosoftCert(PCCERT_CONTEXT pCert) {
        char subjectName[512] = {};
        CertGetNameStringA(pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, subjectName, sizeof(subjectName));
        return std::string(subjectName).find("Microsoft") != std::string::npos;
    }

    
    static bool CheckMsgSigner(HCRYPTMSG hMsg, HCERTSTORE hStore) {
        DWORD dwSignerCount = 0, dwLen = sizeof(dwSignerCount);
        if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_COUNT_PARAM, 0, &dwSignerCount, &dwLen) || dwSignerCount == 0)
            return false;
        DWORD cbSignerInfo = 0;
        CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, nullptr, &cbSignerInfo);
        if (cbSignerInfo == 0) return false;
        std::vector<BYTE> signerBuf(cbSignerInfo);
        if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, signerBuf.data(), &cbSignerInfo)) return false;
        CMSG_SIGNER_INFO* pSigner = (CMSG_SIGNER_INFO*)signerBuf.data();
        CERT_INFO certInfo = {};
        certInfo.Issuer       = pSigner->Issuer;
        certInfo.SerialNumber = pSigner->SerialNumber;
        PCCERT_CONTEXT pCert = CertFindCertificateInStore(
            hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SUBJECT_CERT, &certInfo, nullptr);
        if (!pCert) return false;
        bool result = IsMicrosoftCert(pCert);
        CertFreeCertificateContext(pCert);
        return result;
    }

    
    
    static bool IsMicrosoftSigned(const std::wstring& filePath) {
        
        WINTRUST_FILE_INFO fileInfo = {};
        fileInfo.cbStruct    = sizeof(WINTRUST_FILE_INFO);
        fileInfo.pcwszFilePath = filePath.c_str();
        GUID actionGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
        WINTRUST_DATA trustData = {};
        trustData.cbStruct            = sizeof(WINTRUST_DATA);
        trustData.dwUIChoice          = WTD_UI_NONE;
        trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
        trustData.dwUnionChoice       = WTD_CHOICE_FILE;
        trustData.pFile               = &fileInfo;
        trustData.dwStateAction       = WTD_STATEACTION_VERIFY;
        
        trustData.dwProvFlags         = WTD_CACHE_ONLY_URL_RETRIEVAL;
        LONG trustResult = WinVerifyTrust(NULL, &actionGuid, &trustData);
        trustData.dwStateAction = WTD_STATEACTION_CLOSE;
        WinVerifyTrust(NULL, &actionGuid, &trustData);

        if (trustResult != ERROR_SUCCESS) return false;

        
        HCERTSTORE hStore = nullptr;
        HCRYPTMSG  hMsg   = nullptr;
        DWORD dwEncoding = 0, dwContentType = 0, dwFormatType = 0;
        BOOL ok = CryptQueryObject(
            CERT_QUERY_OBJECT_FILE, filePath.c_str(),
            CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
            CERT_QUERY_FORMAT_FLAG_BINARY,
            0, &dwEncoding, &dwContentType, &dwFormatType,
            &hStore, &hMsg, nullptr);

        if (!ok) {
            
            
            return true;
        }

        bool result = CheckMsgSigner(hMsg, hStore);
        if (hMsg)   CryptMsgClose(hMsg);
        if (hStore) CertCloseStore(hStore, 0);
        return result;
    }

    
    static std::wstring ResolveNtPathToDosPath(const std::wstring& ntPath) {
        wchar_t driveStr[] = L"A:";
        wchar_t devicePath[MAX_PATH];
        
        for (wchar_t drive = L'A'; drive <= L'Z'; ++drive) {
            driveStr[0] = drive;
            if (QueryDosDeviceW(driveStr, devicePath, MAX_PATH)) {
                std::wstring dev(devicePath);
                
                if (ntPath.find(dev) == 0) {
                    std::wstring result = driveStr;
                    result += ntPath.substr(dev.length());
                    return result;
                }
            }
        }
        return ntPath; 
    }

    
    static FILETIME GetWindowsBootTime() {
        
        ULONGLONG uptimeMs = GetTickCount64();

        
        FILETIME nowFt;
        GetSystemTimeAsFileTime(&nowFt);

        
        ULARGE_INTEGER now;
        now.LowPart  = nowFt.dwLowDateTime;
        now.HighPart = nowFt.dwHighDateTime;

        
        now.QuadPart -= (uptimeMs * 10000ULL);

        FILETIME bootFt;
        bootFt.dwLowDateTime  = now.LowPart;
        bootFt.dwHighDateTime = now.HighPart;
        return bootFt;
    }

    std::vector<BAMEntry> BAMParser::Parse() {
        std::vector<BAMEntry> entries;
        Registry::RegistryKey bamRoot;

        if (!bamRoot.Open(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\bam\\State\\UserSettings", KEY_READ | KEY_WOW64_64KEY)) {
            spdlog::warn("Impossible d'ouvrir la clé BAM (droits Administrateur requis)");
            return entries;
        }

        
        FILETIME bootTime = GetWindowsBootTime();

        auto subkeys = bamRoot.EnumerateSubKeys();
        for (const auto& subkey : subkeys) {
            Registry::RegistryKey userKey;
            std::wstring fullPath = L"SYSTEM\\CurrentControlSet\\Services\\bam\\State\\UserSettings\\" + subkey.Name;
            if (!userKey.Open(HKEY_LOCAL_MACHINE, fullPath, KEY_READ | KEY_WOW64_64KEY)) continue;

            auto values = userKey.EnumerateValues();
            for (const auto& valName : values) {
                if (valName == L"Version" || valName == L"SequenceNumber") continue;

                auto data = userKey.ReadBinary(valName);
                if (!data || data->size() < 24) continue;

                FILETIME ft;
                memcpy(&ft, data->data(), sizeof(FILETIME));

                BAMEntry entry;
                entry.SID            = Utils::WStringToString(subkey.Name);
                
                
                std::wstring dosPath = ResolveNtPathToDosPath(valName);
                
                
                std::wstring pathLower = dosPath;
                for (auto& c : pathLower) c = towlower(c);
                if (pathLower.find(L"c:\\") != 0) {
                    continue; 
                }

                entry.ExecutablePath = Utils::WStringToString(dosPath);
                
                entry.ExecutionTime  = Utils::FileTimeToStdString(ft);

                ULARGE_INTEGER entryTime;
                entryTime.LowPart  = ft.dwLowDateTime;
                entryTime.HighPart = ft.dwHighDateTime;
                entry.Timestamp = entryTime.QuadPart;

                
                entry.IsSigned = IsMicrosoftSigned(dosPath);

                
                
                ULARGE_INTEGER boot;
                boot.LowPart       = bootTime.dwLowDateTime;
                boot.HighPart      = bootTime.dwHighDateTime;
                entry.IsRunning = (entryTime.QuadPart >= boot.QuadPart);

                entries.push_back(entry);
            }
        }

        return entries;
    }
}
