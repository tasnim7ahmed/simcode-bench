#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/mac48-address.h"

#include <iostream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AutoconfiguredIpv6Addresses");

int main() {
  LogComponentEnable("AutoconfiguredIpv6Addresses", LOG_LEVEL_INFO);

  // Array of MAC addresses
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

  // IPv6 prefixes
  Ipv6Prefix prefix1 = Ipv6Prefix("2001:1::/64");
  Ipv6Prefix prefix2 = Ipv6Prefix("2002:1:1::/64");

  // Generate and log IPv6 addresses for prefix1
  NS_LOG_INFO("Prefix: " << prefix1);
  for (int i = 0; i < 10; ++i) {
    Ipv6Address address = Ipv6Address::Autoconfigure (prefix1, macAddresses[i]);
    NS_LOG_INFO("MAC: " << macAddresses[i] << ", IPv6: " << address);
  }

  // Generate and log IPv6 addresses for prefix2
  NS_LOG_INFO("Prefix: " << prefix2);
  for (int i = 0; i < 10; ++i) {
    Ipv6Address address = Ipv6Address::Autoconfigure (prefix2, macAddresses[i]);
    NS_LOG_INFO("MAC: " << macAddresses[i] << ", IPv6: " << address);
  }

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}