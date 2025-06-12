#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6AutoconfigurationTest");

int main(int argc, char *argv[]) {
    LogComponentEnable("Ipv6AutoconfigurationTest", LOG_LEVEL_INFO);

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

    Ipv6Address prefixes[2] = {
        Ipv6Address("2001:1::"),
        Ipv6Address("2002:1:1::")
    };

    for (uint32_t prefixIdx = 0; prefixIdx < 2; ++prefixIdx) {
        Ipv6Address prefix = prefixes[prefixIdx];
        NS_LOG_INFO("Using Prefix: " << prefix);
        for (uint32_t macIdx = 0; macIdx < 10; ++macIdx) {
            Mac48Address mac = macAddresses[macIdx];
            Ipv6Address ipv6Addr = Ipv6Address::MakeAutoconfiguredAddress(mac.As(), prefix);
            NS_LOG_INFO("MAC: " << mac << " -> IPv6: " << ipv6Addr);
        }
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}