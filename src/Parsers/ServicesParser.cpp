#include "ServicesParser.h"
#include "../Utils/StringUtils.h"
#include "spdlog/spdlog.h"
#include <windows.h>

namespace Parsers {

    std::vector<ServiceEntry> ServicesParser::Parse() {
        std::vector<ServiceEntry> entries;

        
        std::vector<std::string> targetServices = {
            "DPS", "EventLog", "SysMain", "AppInfo", "PcaSvc", 
            "DusmSvc", "BAM", "DiagTrack", "Schedule", "SearchIndexer"
        };

        SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
        if (!hSCManager) {
            spdlog::error("Impossible d'ouvrir le Service Control Manager");
            return entries;
        }

        for (const auto& svcName : targetServices) {
            ServiceEntry entry;
            entry.Name = svcName;
            entry.DisplayName = svcName; 
            entry.Status = "Unknown";

            std::wstring wSvcName = Utils::StringToWString(svcName);
            SC_HANDLE hService = OpenServiceW(hSCManager, wSvcName.c_str(), SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
            
            if (hService) {
                
                SERVICE_STATUS_PROCESS ssStatus;
                DWORD bytesNeeded;
                if (QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssStatus, sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
                    switch (ssStatus.dwCurrentState) {
                        case SERVICE_STOPPED:       entry.Status = "Stopped"; break;
                        case SERVICE_START_PENDING: entry.Status = "Starting"; break;
                        case SERVICE_STOP_PENDING:  entry.Status = "Stopping"; break;
                        case SERVICE_RUNNING:       entry.Status = "Running"; break;
                        case SERVICE_CONTINUE_PENDING: entry.Status = "Continuing"; break;
                        case SERVICE_PAUSE_PENDING: entry.Status = "Pausing"; break;
                        case SERVICE_PAUSED:        entry.Status = "Paused"; break;
                        default:                    entry.Status = "Unknown"; break;
                    }
                }

                
                DWORD cbBufSize = 0;
                QueryServiceConfigW(hService, NULL, 0, &cbBufSize); 
                if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                    LPQUERY_SERVICE_CONFIGW pSvcConfig = (LPQUERY_SERVICE_CONFIGW)LocalAlloc(LMEM_FIXED, cbBufSize);
                    if (pSvcConfig) {
                        if (QueryServiceConfigW(hService, pSvcConfig, cbBufSize, &cbBufSize)) {
                            entry.DisplayName = Utils::WStringToString(pSvcConfig->lpDisplayName);
                        }
                        LocalFree(pSvcConfig);
                    }
                }

                CloseServiceHandle(hService);
            } else {
                entry.Status = "Not Found";
            }

            entries.push_back(entry);
        }

        CloseServiceHandle(hSCManager);
        return entries;
    }

}
