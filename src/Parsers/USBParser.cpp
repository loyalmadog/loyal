#include "USBParser.h"
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <cstdio>
#include <string>
#include <vector>
#include <set>
#include <algorithm>

// Manual DEVPKEY definitions for timestamps
// GUID: {83da6326-97a6-4088-9453-a1923f573b29}
static const DEVPROPKEY MY_DEVPKEY_InstallDate    = {{ 0x83da6326, 0x97a6, 0x4088, {0x94,0x53,0xa1,0x92,0x3f,0x57,0x3b,0x29} }, 100 };
static const DEVPROPKEY MY_DEVPKEY_FirstInstall   = {{ 0x83da6326, 0x97a6, 0x4088, {0x94,0x53,0xa1,0x92,0x3f,0x57,0x3b,0x29} }, 101 };
static const DEVPROPKEY MY_DEVPKEY_LastArrival    = {{ 0x83da6326, 0x97a6, 0x4088, {0x94,0x53,0xa1,0x92,0x3f,0x57,0x3b,0x29} }, 102 };
static const DEVPROPKEY MY_DEVPKEY_LastRemoval    = {{ 0x83da6326, 0x97a6, 0x4088, {0x94,0x53,0xa1,0x92,0x3f,0x57,0x3b,0x29} }, 103 };

#define DEVPROP_TYPE_FILETIME 0x00000040

namespace Parsers {

// -------------------------------------------------------
// Helpers
// -------------------------------------------------------

static std::string FileTimeToStr(const FILETIME& ft)
{
    if (ft.dwLowDateTime == 0 && ft.dwHighDateTime == 0)
        return "";
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
    char buf[64];
    sprintf_s(buf, "%04d-%02d-%02d %02d:%02d:%02d",
        stLocal.wYear, stLocal.wMonth, stLocal.wDay,
        stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
    return buf;
}

static uint64_t FTtoU64(const FILETIME& ft)
{
    ULARGE_INTEGER u;
    u.LowPart  = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return u.QuadPart;
}

static std::string CapsToString(DWORD caps)
{
    if (caps == 0) return "None";
    std::string result;
    auto add = [&](const char* s) {
        if (!result.empty()) result += ", ";
        result += s;
    };
    if (caps & 0x00000002) add("EjectSupported");
    if (caps & 0x00000004) add("Removable");
    if (caps & 0x00000010) add("UniqueID");
    if (caps & 0x00000020) add("SilentInstall");
    if (caps & 0x00000080) add("SurpriseRemovalOK");
    return result;
}

static void ExtractVIDPID(const std::string& str, std::string& vid, std::string& pid)
{
    vid.clear(); pid.clear();
    size_t vi = str.find("VID_");
    if (vi == std::string::npos) vi = str.find("vid_");
    if (vi != std::string::npos && vi + 8 <= str.size())
        vid = str.substr(vi + 4, 4);
    size_t pi = str.find("PID_");
    if (pi == std::string::npos) pi = str.find("pid_");
    if (pi != std::string::npos && pi + 8 <= str.size())
        pid = str.substr(pi + 4, 4);
    // Uppercase
    for (char& c : vid) c = (char)toupper((unsigned char)c);
    for (char& c : pid) c = (char)toupper((unsigned char)c);
}

// Read a FILETIME device property via SetupAPI
static FILETIME GetDevPropFT(HDEVINFO hDev, SP_DEVINFO_DATA& dd, const DEVPROPKEY& key)
{
    FILETIME ft = {};
    DEVPROPTYPE dpType = 0;
    DWORD required = 0;
    // First call to get required size
    if (!SetupDiGetDevicePropertyW(hDev, &dd, &key, &dpType, (PBYTE)&ft, sizeof(ft), &required, 0))
        return ft;
    // dpType should be DEVPROP_TYPE_FILETIME
    return ft;
}

// -------------------------------------------------------
// Main parse - uses SetupAPI to enumerate ALL USB devices
// (present AND non-present) so we capture full history
// -------------------------------------------------------

std::vector<USBEntry> USBParser::Parse()
{
    std::vector<USBEntry> entries;

    // --- Step 1: Build set of CURRENTLY PRESENT device instance IDs ---
    std::set<std::string> presentSet;
    {
        HDEVINFO hPresent = SetupDiGetClassDevs(NULL, "USB", NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
        if (hPresent != INVALID_HANDLE_VALUE)
        {
            SP_DEVINFO_DATA dd = {}; dd.cbSize = sizeof(dd);
            for (DWORD i = 0; SetupDiEnumDeviceInfo(hPresent, i, &dd); i++)
            {
                char instId[512] = {};
                if (SetupDiGetDeviceInstanceIdA(hPresent, &dd, instId, sizeof(instId) - 1, NULL))
                {
                    for (char* p = instId; *p; p++) *p = (char)toupper((unsigned char)*p);
                    presentSet.insert(instId);
                }
            }
            SetupDiDestroyDeviceInfoList(hPresent);
        }
    }

    // --- Step 2: Enumerate ALL USB devices (includes disconnected / historical) ---
    HDEVINFO hDevInfo = SetupDiGetClassDevs(NULL, "USB", NULL, DIGCF_ALLCLASSES);
    if (hDevInfo == INVALID_HANDLE_VALUE)
        return entries;

    SP_DEVINFO_DATA devData = {};
    devData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devData); i++)
    {
        // Get full instance ID (e.g. "USB\VID_xxxx&PID_xxxx\serial")
        char instanceId[512] = {};
        if (!SetupDiGetDeviceInstanceIdA(hDevInfo, &devData, instanceId, sizeof(instanceId) - 1, NULL))
            continue;

        USBEntry entry;
        std::string instIdStr = instanceId;

        // Build normalized uppercase version for set lookup
        std::string instUpper = instIdStr;
        for (char& c : instUpper) c = (char)toupper((unsigned char)c);

        // Extract VID / PID
        ExtractVIDPID(instIdStr, entry.VID, entry.PID);

        // DeviceClass = the VID_xxxx&PID_xxxx segment
        {
            size_t s1 = instIdStr.find('\\');
            size_t s2 = (s1 != std::string::npos) ? instIdStr.find('\\', s1 + 1) : std::string::npos;
            if (s1 != std::string::npos && s2 != std::string::npos)
                entry.DeviceClass = instIdStr.substr(s1 + 1, s2 - s1 - 1);
            // Serial = last segment
            if (s2 != std::string::npos)
                entry.SerialNumber = instIdStr.substr(s2 + 1);
        }

        // --- String properties via SetupDi ---
        DWORD regType = 0, reqSz = 0;
        char strBuf[512] = {};

        reqSz = sizeof(strBuf) - 1;
        if (SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devData, SPDRP_FRIENDLYNAME, &regType, (PBYTE)strBuf, sizeof(strBuf) - 1, &reqSz))
            entry.FriendlyName = strBuf;

        memset(strBuf, 0, sizeof(strBuf)); reqSz = sizeof(strBuf) - 1;
        if (SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devData, SPDRP_DEVICEDESC, &regType, (PBYTE)strBuf, sizeof(strBuf) - 1, &reqSz))
            entry.DeviceDesc = strBuf;

        memset(strBuf, 0, sizeof(strBuf)); reqSz = sizeof(strBuf) - 1;
        if (SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devData, SPDRP_MFG, &regType, (PBYTE)strBuf, sizeof(strBuf) - 1, &reqSz))
            entry.Manufacturer = strBuf;

        // Strip "@oem.inf,...;Real Name" format
        auto stripInf = [](std::string& s) {
            size_t semi = s.rfind(';');
            if (semi != std::string::npos) s = s.substr(semi + 1);
            if (!s.empty() && s.front() == '(' && s.back() == ')') s.clear();
        };
        stripInf(entry.DeviceDesc);
        stripInf(entry.Manufacturer);

        if (entry.FriendlyName.empty()) entry.FriendlyName = entry.DeviceDesc;

        // Capabilities
        DWORD caps = 0; reqSz = sizeof(caps);
        if (SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devData, SPDRP_CAPABILITIES, &regType, (PBYTE)&caps, sizeof(caps), &reqSz))
            entry.Capabilities = CapsToString(caps);

        // IsConnected via SetupAPI present set
        entry.IsConnected = (presentSet.count(instUpper) > 0);

        // --- Timestamps via SetupDiGetDevicePropertyW ---
        {
            FILETIME ft;

            // First connect (install date)
            ft = GetDevPropFT(hDevInfo, devData, MY_DEVPKEY_InstallDate);
            if (ft.dwLowDateTime || ft.dwHighDateTime) {
                entry.FirstConnected = FileTimeToStr(ft);
                entry.FirstTimestamp = FTtoU64(ft);
            }
            // If no install date, try first-install
            if (entry.FirstConnected.empty()) {
                ft = GetDevPropFT(hDevInfo, devData, MY_DEVPKEY_FirstInstall);
                if (ft.dwLowDateTime || ft.dwHighDateTime) {
                    entry.FirstConnected = FileTimeToStr(ft);
                    entry.FirstTimestamp = FTtoU64(ft);
                }
            }

            // Last arrival (last connect)
            ft = GetDevPropFT(hDevInfo, devData, MY_DEVPKEY_LastArrival);
            if (ft.dwLowDateTime || ft.dwHighDateTime) {
                entry.LastConnected = FileTimeToStr(ft);
                entry.LastTimestamp = FTtoU64(ft);
            }

            // Last removal
            ft = GetDevPropFT(hDevInfo, devData, MY_DEVPKEY_LastRemoval);
            if (ft.dwLowDateTime || ft.dwHighDateTime) {
                entry.LastRemoved     = FileTimeToStr(ft);
                entry.RemoveTimestamp = FTtoU64(ft);
            }
        }

        entries.push_back(std::move(entry));
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

    // Sort: connected first, then by last connect time descending
    std::sort(entries.begin(), entries.end(), [](const USBEntry& a, const USBEntry& b) {
        if (a.IsConnected != b.IsConnected) return (int)a.IsConnected > (int)b.IsConnected;
        return a.LastTimestamp > b.LastTimestamp;
    });

    return entries;
}

} // namespace Parsers
