#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/mac48-address.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6AutoconfFromMacExample");

Ipv6Address
Ipv6AutoconfAddress(const std::string& prefix, const Mac48Address& mac)
{
  // Get MAC bytes
  uint8_t macBytes[6];
  mac.CopyTo(macBytes);

  // EUI-64: Split the MAC: 6 bytes: a1 a2 a3 a4 a5 a6
  // EUI-64: [a1 ^ 0x02] a2 a3 0xFF 0xFE a4 a5 a6
  uint8_t eui64[8];
  eui64[0] = macBytes[0] ^ 0x02;  // Flip the Universal/Local bit
  eui64[1] = macBytes[1];
  eui64[2] = macBytes[2];
  eui64[3] = 0xFF;
  eui64[4] = 0xFE;
  eui64[5] = macBytes[3];
  eui64[6] = macBytes[4];
  eui64[7] = macBytes[5];

  // Prefix is e.g. "2001:1::" or "2002:1:1::", interpret as Ipv6Address, use upper 64 bits
  Ipv6Address ipv6Prefix(prefix.c_str());
  uint8_t prefixBytes[16];
  ipv6Prefix.GetBytes(prefixBytes);

  // Combine: upper 8 bytes from prefix, lower 8 from eui64 for autoconf address
  uint8_t addrBytes[16];
  for (int i = 0; i < 8; ++i)
    {
      addrBytes[i] = prefixBytes[i];
    }
  for (int i = 0; i < 8; ++i)
    {
      addrBytes[8 + i] = eui64[i];
    }
  return Ipv6Address(addrBytes);
}

int
main(int argc, char* argv[])
{
  LogComponentEnable("Ipv6AutoconfFromMacExample", LOG_LEVEL_INFO);

  // Define 10 MAC addresses
  Mac48Address macArray[10] = {
      Mac48Address("00:11:22:33:44:01"),
      Mac48Address("00:11:22:33:44:02"),
      Mac48Address("00:11:22:33:44:03"),
      Mac48Address("00:11:22:33:44:04"),
      Mac48Address("00:11:22:33:44:05"),
      Mac48Address("00:11:22:33:44:06"),
      Mac48Address("00:11:22:33:44:07"),
      Mac48Address("00:11:22:33:44:08"),
      Mac48Address("00:11:22:33:44:09"),
      Mac48Address("00:11:22:33:44:0A")
  };

  std::string prefixes[] = {"2001:1::", "2002:1:1::"};

  for (const std::string& prefix : prefixes)
    {
      NS_LOG_INFO("Generating IPv6 addresses for prefix: " << prefix);
      for (uint32_t i = 0; i < 10; ++i)
        {
          Mac48Address mac = macArray[i];
          Ipv6Address ipv6 = Ipv6AutoconfAddress(prefix, mac);

          NS_LOG_INFO(" MAC: " << mac << " => IPv6: " << ipv6);
        }
    }

  Simulator::Stop(Seconds(0.1));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}