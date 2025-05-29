#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6AutoconfigExample");

int main(int argc, char *argv[])
{
    LogComponentEnable("Ipv6AutoconfigExample", LOG_LEVEL_INFO);

    // Initialize 10 specific MAC addresses
    std::array<Mac48Address, 10> macAddrs = {
        Mac48Address("00:00:00:00:00:01"),
        Mac48Address("00:00:00:00:00:02"),
        Mac48Address("00:11:22:33:44:55"),
        Mac48Address("0a:0b:0c:0d:0e:0f"),
        Mac48Address("12:34:56:78:9a:bc"),
        Mac48Address("ab:cd:ef:01:23:45"),
        Mac48Address("fe:dc:ba:98:76:54"),
        Mac48Address("01:23:45:67:89:ab"),
        Mac48Address("aa:bb:cc:dd:ee:ff"),
        Mac48Address("de:ad:be:ef:00:01")
    };

    std::vector<Ipv6Address> prefixes = {
        Ipv6Address("2001:1::"),
        Ipv6Address("2002:1:1::")
    };

    std::vector<uint8_t> prefixLengths = {64, 64};

    NS_LOG_INFO("IPv6 Address Autoconfiguration from MAC addresses:");

    for (size_t p = 0; p < prefixes.size(); ++p) {
        NS_LOG_INFO("Prefix: " << prefixes[p] << "/" << (uint32_t)prefixLengths[p]);
        for (size_t i = 0; i < macAddrs.size(); ++i) {
            Ipv6Address addr = Ipv6AddressHelper::MakeAutoconfiguredAddress(prefixes[p], prefixLengths[p], macAddrs[i]);
            NS_LOG_INFO("  MAC: " << macAddrs[i] << " -> IPv6: " << addr);
        }
    }

    Simulator::Stop(Seconds(0.1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}