#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/mac48-address.h"
#include <vector>
#include <array>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("Ipv6AutoconfFromMac");

Ipv6Address
GenerateAutoconfiguredIpv6Address(std::string prefix, Mac48Address mac)
{
    // Convert prefix string to Ipv6Address
    Ipv6Address prefixAddr(prefix.c_str());
    // The prefix length is /64 for SLAAC
    uint8_t prefixLength = 64;
    // Get MAC address bytes
    uint8_t macBytes[6];
    mac.CopyTo(macBytes);

    // Make EUI-64
    uint8_t eui[8];
    // First three bytes
    eui[0] = macBytes[0] ^ 0x02; // Flip the U/L bit
    eui[1] = macBytes[1];
    eui[2] = macBytes[2];
    eui[3] = 0xff;
    eui[4] = 0xfe;
    eui[5] = macBytes[3];
    eui[6] = macBytes[4];
    eui[7] = macBytes[5];

    // Form the full IPv6 address bytes
    uint8_t ipv6[16];
    prefixAddr.GetBytes(ipv6);

    // Overwrite host identifier portion with the EUI-64
    for (int i = 0; i < 8; ++i)
    {
        ipv6[8 + i] = eui[i];
    }

    return Ipv6Address(ipv6);
}

int main(int argc, char *argv[])
{
    LogComponentEnable("Ipv6AutoconfFromMac", LOG_LEVEL_INFO);

    NS_LOG_INFO("Starting IPv6 SLAAC address generation from MACs...");

    std::array<Mac48Address, 10> macs = {
        Mac48Address("00:11:22:33:44:55"),
        Mac48Address("66:77:88:99:AA:BB"),
        Mac48Address("12:34:56:78:9A:BC"),
        Mac48Address("DE:AD:BE:EF:00:01"),
        Mac48Address("FE:DC:BA:98:76:54"),
        Mac48Address("01:23:45:67:89:AB"),
        Mac48Address("10:20:30:40:50:60"),
        Mac48Address("A1:B2:C3:D4:E5:F6"),
        Mac48Address("55:44:33:22:11:00"),
        Mac48Address("CA:FE:BA:BE:FA:CE")
    };

    std::vector<std::string> prefixes = {"2001:1::", "2002:1:1::"};

    for (const auto& prefix : prefixes)
    {
        NS_LOG_INFO("Prefix: " << prefix << "/64");
        for (size_t i=0; i < macs.size(); ++i)
        {
            Ipv6Address addr = GenerateAutoconfiguredIpv6Address(prefix, macs[i]);
            NS_LOG_INFO("MAC " << macs[i] << " => IPv6: " << addr);
        }
    }

    NS_LOG_INFO("Done.");
    return 0;
}