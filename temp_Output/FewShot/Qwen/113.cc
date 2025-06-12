#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6AutoconfigTest");

int main(int argc, char *argv[]) {
    LogComponentEnable("Ipv6AutoconfigTest", LOG_LEVEL_INFO);

    // Array of 10 MAC addresses
    Mac48Address macAddresses[10] = {
        Mac48Address("00:00:00:00:00:01"),
        Mac48Address("00:00:00:00:00:02"),
        Mac48Address("00:00:00:00:00:03"),
        Mac48Address("00:00:00:00:00:04"),
        Mac48Address("00:00:00:00:00:05"),
        Mac48Address("00:00:00:00:00:06"),
        Mac48Address("00:00:00:00:00:07"),
        Mac48Address("00:00:00:00:00:08"),
        Mac48Address("00:00:00:00:00:09"),
        Mac48Address("00:00:00:00:00:0a")
    };

    // IPv6 prefixes
    Ipv6Address prefixes[2] = {
        Ipv6Address("2001:1::"),
        Ipv6Address("2002:1:1::")
    };

    // Generate and log IPv6 addresses
    for (int i = 0; i < 2; ++i) {
        Ipv6Address prefix = prefixes[i];
        uint8_t prefixLength = 64;

        NS_LOG_INFO("Prefix: " << prefix);

        for (int j = 0; j < 10; ++j) {
            Mac48Address mac = macAddresses[j];
            Ipv6Address ipv6Addr = Ipv6Address::MakeAutoconfiguredAddress(mac.AsToRs(), prefix, prefixLength);
            NS_LOG_INFO("  MAC: " << mac << " -> IPv6: " << ipv6Addr);
        }
    }

    return 0;
}