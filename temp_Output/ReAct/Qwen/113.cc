#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6AutoconfigurationTest");

int main(int argc, char *argv[]) {
    LogComponentEnable("Ipv6AutoconfigurationTest", LOG_LEVEL_INFO);

    uint8_t macPrefixes[10][6] = {
        {0x00, 0x11, 0x22, 0x33, 0x44, 0x55},
        {0x00, 0x11, 0x22, 0x33, 0x44, 0x66},
        {0x00, 0x11, 0x22, 0x33, 0x44, 0x77},
        {0x00, 0x11, 0x22, 0x33, 0x44, 0x88},
        {0x00, 0x11, 0x22, 0x33, 0x44, 0x99},
        {0x00, 0x11, 0x22, 0x33, 0x44, 0xAA},
        {0x00, 0x11, 0x22, 0x33, 0x44, 0xBB},
        {0x00, 0x11, 0x22, 0x33, 0x44, 0xCC},
        {0x00, 0x11, 0x22, 0x33, 0x44, 0xDD},
        {0x00, 0x11, 0x22, 0x33, 0x44, 0xEE}
    };

    Ipv6Address prefixes[] = {
        Ipv6Address("2001:1::"),
        Ipv6Address("2002:1:1::")
    };

    for (int p = 0; p < 2; ++p) {
        NS_LOG_INFO("Using prefix: " << prefixes[p]);
        for (int i = 0; i < 10; ++i) {
            Mac48Address mac = Mac48Address::ConvertFrom(MacAddress::AllocateMac48(macPrefixes[i]));

            Ipv6Address ipv6Addr = Ipv6AddressGenerator::GetLinkLocalAddress(mac);
            Ipv6AddressGenerator::AddAllocated(ipv6Addr);

            Ipv6Address combined = prefixes[p] | ipv6Addr;
            NS_LOG_INFO("MAC: " << mac << " -> IPv6: " << combined);
        }
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}