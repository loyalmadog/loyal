#include "PrefetchParser.h"
#include <windows.h>
#include <wintrust.h>
#include <softpub.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdint>
#pragma comment(lib, "wintrust.lib")

// ============================================================
// Helpers pour charger RtlDecompressBufferEx depuis ntdll.dll
// (Les .pf Windows 10/11 sont compressés en XPRESS_HUFF)
// ============================================================
typedef NTSTATUS (WINAPI* RtlGetCompressionWorkSpaceSize_t)(USHORT, PULONG, PULONG);
typedef NTSTATUS (WINAPI* RtlDecompressBufferEx_t)(USHORT, PUCHAR, ULONG, PUCHAR, ULONG, PULONG, PVOID);

static const USHORT COMPRESS_ALGORITHM_XPRESS_HUFF = 0x0004 | 0x8000; // XPRESS with Huffman
static const USHORT COMPRESS_ALGORITHM_XPRESS      = 0x0004;

namespace Parsers {

// ============================================================
// Convertit un FILETIME en string lisible et retourne le timestamp brut
// ============================================================
static std::string FiletimeToString(uint64_t ft, uint64_t& outTimestamp) {
    outTimestamp = ft;
    if (ft == 0) return "Never";

    FILETIME filetime;
    filetime.dwLowDateTime  = (DWORD)(ft & 0xFFFFFFFF);
    filetime.dwHighDateTime = (DWORD)(ft >> 32);

    FILETIME localFt;
    FileTimeToLocalFileTime(&filetime, &localFt);

    SYSTEMTIME st;
    FileTimeToSystemTime(&localFt, &st);

    char buf[64];
    sprintf_s(buf, "%04d-%02d-%02d %02d:%02d:%02d",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);
    return std::string(buf);
}

// Surcharge sans outTimestamp
static std::string FiletimeToString(uint64_t ft) {
    uint64_t dummy;
    return FiletimeToString(ft, dummy);
}

// ============================================================
// Formate une taille en octets -> "xx.xx KB"
// ============================================================
static std::string FormatSize(uint64_t bytes) {
    char buf[64];
    if (bytes < 1024) sprintf_s(buf, "%llu B", bytes);
    else if (bytes < 1024*1024) sprintf_s(buf, "%.2f KB", bytes / 1024.0);
    else sprintf_s(buf, "%.2f MB", bytes / (1024.0*1024.0));
    return std::string(buf);
}

// ============================================================
// Récupère taille + dates création/modification d'un fichier
// ============================================================
static void GetFileMetadata(const std::string& path, std::string& outSize, std::string& outCreated, std::string& outModified) {
    outSize = outCreated = outModified = "N/A";
    if (path.empty()) return;

    WIN32_FILE_ATTRIBUTE_DATA info = {};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &info)) return;

    uint64_t sz = ((uint64_t)info.nFileSizeHigh << 32) | info.nFileSizeLow;
    outSize = FormatSize(sz);

    uint64_t created  = ((uint64_t)info.ftCreationTime.dwHighDateTime << 32)   | info.ftCreationTime.dwLowDateTime;
    uint64_t modified = ((uint64_t)info.ftLastWriteTime.dwHighDateTime << 32)   | info.ftLastWriteTime.dwLowDateTime;

    outCreated  = FiletimeToString(created);
    outModified = FiletimeToString(modified);
}

// ============================================================
// Tente de décompresser un fichier .pf Windows 10/11
// Retourne le buffer décompressé ou vide en cas d'échec
// ============================================================
static std::vector<uint8_t> TryDecompress(const std::vector<uint8_t>& raw) {
    if (raw.size() < 8) return {};

    // Signature MAM = fichier compressé Win10
    uint32_t sig = *(uint32_t*)raw.data();
    if (sig != 0x044D414D) {
        // Pas compressé, retourner tel quel
        return raw;
    }

    uint32_t uncompressedSize = *(uint32_t*)(raw.data() + 4);
    if (uncompressedSize == 0 || uncompressedSize > 100 * 1024 * 1024) return {};

    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) return {};

    auto pGetWS   = (RtlGetCompressionWorkSpaceSize_t)GetProcAddress(hNtdll, "RtlGetCompressionWorkSpaceSize");
    auto pDecompr = (RtlDecompressBufferEx_t)         GetProcAddress(hNtdll, "RtlDecompressBufferEx");
    if (!pGetWS || !pDecompr) return {};

    ULONG wsSize = 0, fragSize = 0;
    // Essaie XPRESS Huffman d'abord (Win10), puis XPRESS
    USHORT algo = COMPRESS_ALGORITHM_XPRESS_HUFF;
    NTSTATUS status = pGetWS(algo, &wsSize, &fragSize);
    if (status != 0) {
        algo = COMPRESS_ALGORITHM_XPRESS;
        status = pGetWS(algo, &wsSize, &fragSize);
        if (status != 0) return {};
    }

    std::vector<uint8_t> workspace(wsSize);
    std::vector<uint8_t> output(uncompressedSize);

    ULONG finalSize = 0;
    const uint8_t* compressed     = raw.data() + 8;
    ULONG          compressedSize = (ULONG)(raw.size() - 8);

    status = pDecompr(algo,
        output.data(), uncompressedSize,
        (PUCHAR)compressed, compressedSize,
        &finalSize, workspace.data());

    if (status != 0) return {};
    output.resize(finalSize);
    return output;
}

// ============================================================
// Parse un fichier .pf décompressé et rempli une PrefetchEntry
// ============================================================
static bool ParsePFBuffer(const std::vector<uint8_t>& buf, const std::string& pfPath, PrefetchEntry& out) {
    if (buf.size() < 0xD0 + 4) return false;

    uint32_t version   = *(uint32_t*)(buf.data() + 0x00);
    uint32_t signature = *(uint32_t*)(buf.data() + 0x04);

    // Signature SCCA = 0x41434353
    if (signature != 0x41434353) return false;
    // Versions supportées: 17 (XP), 23 (Win7), 26 (Win8), 30 (Win10), 31 (Win11)
    if (version != 17 && version != 23 && version != 26 && version != 30 && version != 31) return false;

    // --- Nom de l'exécutable (offset 0x10, 60 octets, UTF-16LE) ---
    const wchar_t* rawName = (const wchar_t*)(buf.data() + 0x10);
    // 60 octets = 30 wchar_t
    std::wstring wname(rawName, 30);
    // Tronquer au premier \0
    size_t nullPos = wname.find(L'\0');
    if (nullPos != std::wstring::npos) wname.resize(nullPos);

    // Conversion en UTF-8
    char mbName[256] = {};
    WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, mbName, sizeof(mbName), nullptr, nullptr);
    out.ExecutableName = mbName;
    out.PrefetchFile   = pfPath;

    // --- Historique d'exécution (jusqu'à 8 timestamps pour Win8/10/11) ---
    if (version == 17) {
        uint64_t ts = *(uint64_t*)(buf.data() + 0x78);
        out.LastRunTime = FiletimeToString(ts, out.LastRunTimestamp);
        if (ts) out.RunHistory.push_back(out.LastRunTime);
    } else if (version == 23) {
        uint64_t ts = *(uint64_t*)(buf.data() + 0x80);
        out.LastRunTime = FiletimeToString(ts, out.LastRunTimestamp);
        if (ts) out.RunHistory.push_back(out.LastRunTime);
    } else {
        // Win8 (26), Win10 (30), Win11 (31) : jusqu'à 8 timestamps
        size_t tsOffset = 0x80;
        for (int i = 0; i < 8; ++i) {
            if (tsOffset + 8 > buf.size()) break;
            uint64_t ts = *(uint64_t*)(buf.data() + tsOffset + i * 8);
            if (ts == 0) continue;
            std::string timeStr;
            if (i == 0) {
                timeStr = FiletimeToString(ts, out.LastRunTimestamp);
                out.LastRunTime = timeStr;
            } else {
                uint64_t dummy;
                timeStr = FiletimeToString(ts, dummy);
            }
            out.RunHistory.push_back(timeStr);
        }
    }

    return true;
}

// ============================================================
// Recherche un exe par son nom dans les chemins système communs
// Retourne le chemin complet ou "" si introuvable
// ============================================================
static std::string ResolveExecutable(const std::string& exeName) {
    // 1. SearchPath cherche dans PATH + dossiers système
    wchar_t wideName[MAX_PATH] = {};
    MultiByteToWideChar(CP_UTF8, 0, exeName.c_str(), -1, wideName, MAX_PATH);

    wchar_t found[MAX_PATH] = {};
    if (SearchPathW(nullptr, wideName, nullptr, MAX_PATH, found, nullptr) > 0) {
        char mb[MAX_PATH] = {};
        WideCharToMultiByte(CP_UTF8, 0, found, -1, mb, MAX_PATH, nullptr, nullptr);
        return std::string(mb);
    }

    // 2. Recherche manuelle dans les chemins courants
    const char* extraDirs[] = {
        "C:\\Program Files\\",
        "C:\\Program Files (x86)\\",
        "C:\\Windows\\",
        "C:\\Windows\\System32\\",
        "C:\\Windows\\SysWOW64\\",
    };
    for (auto& dir : extraDirs) {
        std::string candidate = std::string(dir) + exeName;
        if (GetFileAttributesA(candidate.c_str()) != INVALID_FILE_ATTRIBUTES)
            return candidate;
    }
    return "";
}

// ============================================================
// Vérifie si un exe est signé numériquement via WinTrust
// ============================================================
static bool IsExecutableSigned(const std::string& path) {
    if (path.empty()) return false;

    wchar_t widePath[MAX_PATH] = {};
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, widePath, MAX_PATH);

    WINTRUST_FILE_INFO fileInfo = {};
    fileInfo.cbStruct       = sizeof(fileInfo);
    fileInfo.pcwszFilePath  = widePath;

    GUID actionGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA wd = {};
    wd.cbStruct            = sizeof(wd);
    wd.dwUIChoice          = WTD_UI_NONE;
    wd.fdwRevocationChecks = WTD_REVOKE_NONE;
    wd.dwUnionChoice       = WTD_CHOICE_FILE;
    wd.pFile               = &fileInfo;
    wd.dwStateAction       = WTD_STATEACTION_VERIFY;

    LONG result = WinVerifyTrust(nullptr, &actionGuid, &wd);

    wd.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &actionGuid, &wd);

    return (result == ERROR_SUCCESS);
}

// ============================================================
// Point d'entrée principal
// ============================================================
std::vector<PrefetchEntry> PrefetchParser::Parse() {
    std::vector<PrefetchEntry> results;

    const char* pfDir = "C:\\Windows\\Prefetch\\";
    std::string pattern = std::string(pfDir) + "*.pf";

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return results;

    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

        std::string fullPath = std::string(pfDir) + ffd.cFileName;

        // Lecture brute du fichier
        HANDLE hFile = CreateFileA(fullPath.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) continue;

        DWORD fileSize = GetFileSize(hFile, nullptr);
        if (fileSize == 0 || fileSize > 10 * 1024 * 1024) {
            CloseHandle(hFile);
            continue;
        }

        std::vector<uint8_t> rawBuf(fileSize);
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(hFile, rawBuf.data(), fileSize, &bytesRead, nullptr);
        CloseHandle(hFile);
        if (!ok || bytesRead == 0) continue;

        // Décompression si nécessaire
        auto buf = TryDecompress(rawBuf);
        if (buf.empty()) continue;

        // Parse
        PrefetchEntry entry;
        if (ParsePFBuffer(buf, fullPath, entry)) {
            // Résolution du chemin et vérification signature
            entry.ResolvedPath = ResolveExecutable(entry.ExecutableName);
            entry.ExistsOnDisk = !entry.ResolvedPath.empty();
            entry.IsSigned     = entry.ExistsOnDisk ? IsExecutableSigned(entry.ResolvedPath) : false;

            // Métadonnées du .pf
            GetFileMetadata(fullPath, entry.PfSize, entry.PfCreated, entry.PfModified);

            // Métadonnées de l'exe
            if (entry.ExistsOnDisk)
                GetFileMetadata(entry.ResolvedPath, entry.ExeSize, entry.ExeCreated, entry.ExeModified);

            results.push_back(std::move(entry));
        }

    } while (FindNextFileA(hFind, &ffd));

    FindClose(hFind);
    return results;
}

} // namespace Parsers
