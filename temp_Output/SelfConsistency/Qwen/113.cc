#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6AutoconfigurationTest");

int main(int argc, char *argv[]) {
    NS_LOG_INFO("Starting IPv6 autoconfiguration test.");

    // Enable logging
    LogComponentEnable("Ipv6AutoconfigurationTest", LOG_LEVEL_INFO);

    // Define MAC addresses
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

    // Define prefixes
    Ipv6Prefix prefix1("2001:1::", 64);
    Ipv6Prefix prefix2("2002:1:1::", 64);

    NS_LOG_INFO("Using prefix: " << prefix1);
    for (uint32_t i = 0; i < 10; ++i) {
        Ipv6Address address = Ipv6Address::MakeAutoconfiguredAddress(macAddresses[i], prefix1);
        NS_LOG_INFO("MAC: " << macAddresses[i] << " -> IPv6: " << address);
    }

    NS_LOG_INFO("Using prefix: " << prefix2);
    for (uint32_t i = 0; i < 10; ++i) {
        Ipv6Address address = Ipv6Address::MakeAutoconfiguredAddress(macAddresses[i], prefix2);
        NS_LOG_INFO("MAC: " << macAddresses[i] << " -> IPv6: " << address);
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}