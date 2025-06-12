#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6AutoconfigTest");

int main(int argc, char *argv[]) {
    LogComponentEnable("Ipv6AutoconfigTest", LOG_LEVEL_INFO);

    Ipv6Address prefix1 = Ipv6Address("2001:1::");
    Ipv6Address prefix2 = Ipv6Address("2002:1:1::");

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

    for (uint32_t i = 0; i < 10; ++i) {
        Ipv6Address address1 = Ipv6Address::MakeAutoconfiguredAddress(macAddresses[i], prefix1);
        Ipv6Address address2 = Ipv6Address::MakeAutoconfiguredAddress(macAddresses[i], prefix2);

        NS_LOG_INFO("MAC: " << macAddresses[i] <<
                    ", Prefix1: " << prefix1 << " -> Address: " << address1 <<
                    ", Prefix2: " << prefix2 << " -> Address: " << address2);
    }

    return 0;
}