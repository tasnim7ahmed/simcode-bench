#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/mac48-address.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6AutoconfigMacAddrsExample");

// Converts a MAC-48 (EUI-48) to EUI-64, per RFC 4291 and RFC 2373 Appendix A
// Invert the U/L bit (Universal/Local) in first octet, insert fffe in the middle
static void Mac48ToEui64(const Mac48Address& mac, uint8_t eui64[8])
{
  uint8_t macBytes[6];
  mac.CopyTo(macBytes);

  eui64[0] = macBytes[0] ^ 0x02; // invert U/L bit as per standards
  eui64[1] = macBytes[1];
  eui64[2] = macBytes[2];
  eui64[3] = 0xFF;
  eui64[4] = 0xFE;
  eui64[5] = macBytes[3];
  eui64[6] = macBytes[4];
  eui64[7] = macBytes[5];
}

// Generates interface identifier and returns corresponding IPv6 address for a given prefix and MAC
static Ipv6Address GenerateIpv6Autoconfig(const std::string& prefix, const Mac48Address& mac)
{
  // Parse the prefix to extract the first 8 bytes of address
  // Prefix should be in the form '2001:1::'
  Ipv6Address ipv6Prefix(prefix.c_str()); // ns-3 will zero-fill the lower bits
  uint8_t addrBytes[16];
  ipv6Prefix.GetBytes(addrBytes);

  // Convert MAC-48 to EUI-64
  uint8_t eui64[8];
  Mac48ToEui64(mac, eui64);

  // Set last 8 bytes to EUI-64
  for (int i = 0; i < 8; ++i)
    addrBytes[8 + i] = eui64[i];

  return Ipv6Address(addrBytes);
}

int main(int argc, char *argv[])
{
  // Set up logging
  LogComponentEnable("Ipv6AutoconfigMacAddrsExample", LOG_LEVEL_INFO);

  // Initialize 10 specific MAC addresses
  Mac48Address macAddrs[10] = {
    Mac48Address("00:11:22:33:44:55"),
    Mac48Address("00:11:22:33:44:56"),
    Mac48Address("00:11:22:33:44:57"),
    Mac48Address("00:11:22:33:44:58"),
    Mac48Address("00:11:22:33:44:59"),
    Mac48Address("00:11:22:33:44:5A"),
    Mac48Address("00:11:22:33:44:5B"),
    Mac48Address("00:11:22:33:44:5C"),
    Mac48Address("00:11:22:33:44:5D"),
    Mac48Address("00:11:22:33:44:5E"),
  };

  std::string prefixes[2] = { "2001:1::", "2002:1:1::" };

  for (int p = 0; p < 2; ++p)
    {
      NS_LOG_INFO("Using IPv6 prefix: " << prefixes[p]);
      std::cout << "Using IPv6 prefix: " << prefixes[p] << std::endl;

      for (unsigned int i = 0; i < 10; ++i)
        {
          Ipv6Address autoconf = GenerateIpv6Autoconfig(prefixes[p], macAddrs[i]);
          std::cout << "  MAC: " << macAddrs[i] << "  ->  IPv6 Address: " << autoconf << std::endl;
          NS_LOG_INFO("  MAC: " << macAddrs[i] << "  ->  IPv6 Address: " << autoconf);
        }
      std::cout << std::endl;
    }

  Simulator::Stop(Seconds(0.01));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}