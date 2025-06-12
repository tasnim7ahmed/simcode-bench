#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6AutoconfigurationTest");

int main(int argc, char *argv[]) {
    LogComponentEnable("Ipv6AutoconfigurationTest", LOG_LEVEL_INFO);

    std::vector<std::string> macAddresses = {
        "00:00:00:00:00:01",
        "00:00:00:00:00:02",
        "00:00:00:00:00:03",
        "00:00:00:00:00:04",
        "00:00:00:00:00:05",
        "00:00:00:00:00:06",
        "00:00:00:00:00:07",
        "00:00:00:00:00:08",
        "00:00:00:00:00:09",
        "00:00:00:00:00:0a"
    };

    std::vector<Ipv6Address> prefixes;
    prefixes.emplace_back("2001:1::");
    prefixes.emplace_back("2002:1:1::");

    for (auto &prefix : prefixes) {
        NS_LOG_INFO("Using prefix: " << prefix);
        for (auto &macStr : macAddresses) {
            Mac48Address mac = Mac48Address::ConvertFrom(MacAddress::AllocateMacFromString(macStr.c_str()));
            Ipv6Address ipv6Addr = Ipv6Address::MakeAutoconfiguredAddress(mac, prefix);
            NS_LOG_INFO("MAC: " << macStr << " -> IPv6: " << ipv6Addr);
        }
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}