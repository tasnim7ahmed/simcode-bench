#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/mac48-address.h"
#include <iostream>
#include <array>

using namespace ns3;

int main(int argc, char *argv[]) {
    LogComponentEnable("Ipv6AutoconfigurationExample", LOG_LEVEL_INFO);

    std::array<Mac48Address, 10> macAddresses = {
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

    Ipv6Prefix prefix1 = Ipv6Prefix("2001:1::/64");
    Ipv6Prefix prefix2 = Ipv6Prefix("2002:1:1::/64");

    std::cout << "Prefix: " << prefix1 << std::endl;
    for (const auto& mac : macAddresses) {
        Ipv6Address ipv6Address = Ipv6Address::MakeAutoconfiguredAddress(prefix1, mac);
        std::cout << "MAC: " << mac << ", IPv6: " << ipv6Address << std::endl;
    }

    std::cout << "Prefix: " << prefix2 << std::endl;
    for (const auto& mac : macAddresses) {
        Ipv6Address ipv6Address = Ipv6Address::MakeAutoconfiguredAddress(prefix2, mac);
        std::cout << "MAC: " << mac << ", IPv6: " << ipv6Address << std::endl;
    }

    return 0;
}