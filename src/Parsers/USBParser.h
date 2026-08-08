#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace Parsers {

struct USBEntry {
    std::string FriendlyName;
    std::string Manufacturer;
    std::string DeviceDesc;       // vendor class name
    std::string SerialNumber;
    std::string DeviceClass;      // VID_xxxx&PID_xxxx key name
    std::string VID;
    std::string PID;
    std::string Capabilities;
    std::string FirstConnected;
    std::string LastConnected;
    std::string LastRemoved;
    bool        IsConnected    = false;
    uint64_t    FirstTimestamp = 0;
    uint64_t    LastTimestamp  = 0;
    uint64_t    RemoveTimestamp = 0;
};

class USBParser {
public:
    static std::vector<USBEntry> Parse();
};

} // namespace Parsers
