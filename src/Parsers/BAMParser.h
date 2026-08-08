#pragma once
#include <string>
#include <vector>
#include <windows.h>

namespace Parsers {
    struct BAMEntry {
        std::string  SID;
        std::string  ExecutablePath;
        std::string  ExecutionTime;
        ULONGLONG    Timestamp = 0; // Pour le tri (valeur brute du FILETIME)
        bool         IsSigned  = false;
        bool         IsRunning = false;
    };

    class BAMParser {
    public:
        static std::vector<BAMEntry> Parse();
    };
}
