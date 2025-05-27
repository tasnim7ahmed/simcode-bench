#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/mac48-address.h"
#include <iostream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AutoconfigIpv6");

int main (int argc, char *argv[])
{
  LogComponentEnable ("AutoconfigIpv6", LOG_LEVEL_INFO);

  Mac48Address macAddrs[10];
  macAddrs[0] = Mac48Address::Allocate ();
  macAddrs[1] = Mac48Address ("00:00:00:00:00:01");
  macAddrs[2] = Mac48Address ("00:00:00:00:00:02");
  macAddrs[3] = Mac48Address ("00:00:00:00:00:03");
  macAddrs[4] = Mac48Address ("00:00:00:00:00:04");
  macAddrs[5] = Mac48Address ("00:00:00:00:00:05");
  macAddrs[6] = Mac48Address ("00:00:00:00:00:06");
  macAddrs[7] = Mac48Address ("00:00:00:00:00:07");
  macAddrs[8] = Mac48Address ("00:00:00:00:00:08");
  macAddrs[9] = Mac48Address ("00:00:00:00:00:09");

  Ipv6Prefix prefix1 ("2001:1::");
  Ipv6Prefix prefix2 ("2002:1:1::");

  NS_LOG_INFO ("Prefix: " << prefix1);
  for (int i = 0; i < 10; ++i)
    {
      Ipv6Address addr = Ipv6Address::MakeAutoconfiguredAddress (prefix1, macAddrs[i]);
      NS_LOG_INFO ("  MAC: " << macAddrs[i] << ", IPv6: " << addr);
    }

  NS_LOG_INFO ("Prefix: " << prefix2);
  for (int i = 0; i < 10; ++i)
    {
      Ipv6Address addr = Ipv6Address::MakeAutoconfiguredAddress (prefix2, macAddrs[i]);
      NS_LOG_INFO ("  MAC: " << macAddrs[i] << ", IPv6: " << addr);
    }

  return 0;
}