#pragma once
#include <string>
#include <vector>

namespace Parsers {

    struct ServiceEntry {
        std::string Name;
        std::string DisplayName;
        std::string Status; 
    };

    class ServicesParser {
    public:
        
        static std::vector<ServiceEntry> Parse();
    };

}
