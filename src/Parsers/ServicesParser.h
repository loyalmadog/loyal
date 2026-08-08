#pragma once
#include <string>
#include <vector>

namespace Parsers {

    struct ServiceEntry {
        std::string Name;
        std::string DisplayName;
        std::string Status; // "Running", "Stopped", etc.
    };

    class ServicesParser {
    public:
        // Parse the status of the hardcoded forensic-relevant services
        static std::vector<ServiceEntry> Parse();
    };

}
