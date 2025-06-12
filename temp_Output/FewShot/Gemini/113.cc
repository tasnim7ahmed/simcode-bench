#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

#include <iostream>
#include <iomanip>

using namespace ns3;

int main(int argc, char *argv[]) {
    LogComponentEnable("IPv6AddressAutoconfiguration", LOG_LEVEL_INFO);

    Mac48Address macAddresses[10];
    macAddresses[0] = Mac48Address("00:00:00:00:00:01");
    macAddresses[1] = Mac48Address("00:00:00:00:00:02");
    macAddresses[2] = Mac48Address("00:00:00:00:00:03");
    macAddresses[3] = Mac48Address("00:00:00:00:00:04");
    macAddresses[4] = Mac48Address("00:00:00:00:00:05");
    macAddresses[5] = Mac48Address("00:00:00:00:00:06");
    macAddresses[6] = Mac48Address("00:00:00:00:00:07");
    macAddresses[7] = Mac48Address("00:00:00:00:00:08");
    macAddresses[8] = Mac48Address("00:00:00:00:00:09");
    macAddresses[9] = Mac48Address("00:00:00:00:00:0a");

    Ipv6Prefix prefix1 = Ipv6Prefix("2001:1::");
    Ipv6Prefix prefix2 = Ipv6Prefix("2002:1:1::");

    std::cout << "Prefix: " << prefix1 << std::endl;
    for (int i = 0; i < 10; ++i) {
        Ipv6Address address = Ipv6Address::GetAutoconfiguredAddress(prefix1, macAddresses[i]);
        std::cout << "  MAC: " << macAddresses[i] << ", IPv6: " << address << std::endl;
    }

    std::cout << "Prefix: " << prefix2 << std::endl;
    for (int i = 0; i < 10; ++i) {
        Ipv6Address address = Ipv6Address::GetAutoconfiguredAddress(prefix2, macAddresses[i]);
        std::cout << "  MAC: " << macAddresses[i] << ", IPv6: " << address << std::endl;
    }

    return 0;
}