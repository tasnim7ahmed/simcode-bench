#include "ns3/core-module.h"
#include "ns3/network-module.h"

#include <vector> // Required for std::vector

NS_LOG_COMPONENT_DEFINE("Ipv6AutoconfigTest");

int main(int argc, char *argv[])
{
  LogComponentEnable("Ipv6AutoconfigTest", LOG_LEVEL_INFO);

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
  macAddresses[9] = Mac48Address("00:00:00:00:00:0A"); // Using 'A' for consistency

  Ipv6Prefix prefix1("2001:1::/64");
  Ipv6Prefix prefix2("2002:1:1::/64");

  std::vector<Ipv6Prefix> prefixes;
  prefixes.push_back(prefix1);
  prefixes.push_back(prefix2);

  for (const auto& currentPrefix : prefixes)
  {
    NS_LOG_INFO("--- Generating IPv6 addresses for prefix: " << currentPrefix << " ---");
    for (int i = 0; i < 10; ++i)
    {
      Ipv6Address autoconfiguredAddress = Ipv6Address::CreateAutoconfiguredAddress(currentPrefix, macAddresses[i]);
      NS_LOG_INFO("  MAC: " << macAddresses[i] << " -> IPv6: " << autoconfiguredAddress);
    }
  }

  return 0;
}