#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace Parsers {

    struct PrefetchEntry {
        std::string ExecutableName;   
        std::string LastRunTime;      
        uint64_t    LastRunTimestamp; 
        std::string PrefetchFile;     
        std::string ResolvedPath;     
        bool        ExistsOnDisk;     
        bool        IsSigned;         

        
        std::vector<std::string> RunHistory;

        
        std::string PfSize;
        std::string PfCreated;
        std::string PfModified;

        
        std::string ExeSize;
        std::string ExeCreated;
        std::string ExeModified;
    };

    class PrefetchParser {
    public:
        static std::vector<PrefetchEntry> Parse();
    };

} 
