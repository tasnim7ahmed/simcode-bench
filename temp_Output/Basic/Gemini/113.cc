#include <iostream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/mac48-address.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("IPv6Autoconfiguration");

int main (int argc, char *argv[])
{
  LogComponent::EnableIStream (std::cout, "IPv6Autoconfiguration");

  Mac48Address macAddresses[10];
  macAddresses[0] = Mac48Address::Allocate ();
  macAddresses[1] = Mac48Address::Allocate ();
  macAddresses[2] = Mac48Address::Allocate ();
  macAddresses[3] = Mac48Address::Allocate ();
  macAddresses[4] = Mac48Address::Allocate ();
  macAddresses[5] = Mac48Address::Allocate ();
  macAddresses[6] = Mac48Address::Allocate ();
  macAddresses[7] = Mac48Address::Allocate ();
  macAddresses[8] = Mac48Address::Allocate ();
  macAddresses[9] = Mac48Address::Allocate ();

  macAddresses[0] = Mac48Address ("00:00:00:00:00:01");
  macAddresses[1] = Mac48Address ("00:00:00:00:00:02");
  macAddresses[2] = Mac48Address ("00:00:00:00:00:03");
  macAddresses[3] = Mac48Address ("00:00:00:00:00:04");
  macAddresses[4] = Mac48Address ("00:00:00:00:00:05");
  macAddresses[5] = Mac48Address ("00:00:00:00:00:06");
  macAddresses[6] = Mac48Address ("00:00:00:00:00:07");
  macAddresses[7] = Mac48Address ("00:00:00:00:00:08");
  macAddresses[8] = Mac48Address ("00:00:00:00:00:09");
  macAddresses[9] = Mac48Address ("00:00:00:00:00:0a");

  Ipv6Prefix prefix1 ("2001:1::");
  Ipv6Prefix prefix2 ("2002:1:1::");

  NS_LOG_INFO ("Prefix 1: " << prefix1);
  for (int i = 0; i < 10; ++i)
    {
      Ipv6Address address = Ipv6Address::GetAutoconfiguredAddress (prefix1, macAddresses[i]);
      NS_LOG_INFO ("MAC Address " << macAddresses[i] << " -> IPv6 Address: " << address);
    }

  NS_LOG_INFO ("Prefix 2: " << prefix2);
  for (int i = 0; i < 10; ++i)
    {
      Ipv6Address address = Ipv6Address::GetAutoconfiguredAddress (prefix2, macAddresses[i]);
      NS_LOG_INFO ("MAC Address " << macAddresses[i] << " -> IPv6 Address: " << address);
    }

  return 0;
}