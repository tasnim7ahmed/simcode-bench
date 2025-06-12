#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/mac48-address.h"
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6AutoconfigFromMacExample");

// Helper function to generate the EUI-64 identifier from a MAC address
Ipv6Address GenerateIpv6Autoconfig (const std::string &prefix, const Mac48Address &mac)
{
  uint8_t buf[8];
  uint8_t macbuf[6];
  mac.CopyTo (macbuf);

  // Convert MAC to EUI-64 (RFC 4291)
  buf[0] = macbuf[0] ^ 0x02; // invert the U/L bit
  buf[1] = macbuf[1];
  buf[2] = macbuf[2];
  buf[3] = 0xff;
  buf[4] = 0xfe;
  buf[5] = macbuf[3];
  buf[6] = macbuf[4];
  buf[7] = macbuf[5];

  // Create IPv6 address string
  std::ostringstream oss;
  oss << prefix;
  if (prefix.back() != ':')
    {
      oss << ":";
    }
  // Append EUI-64 as hex
  for (int i = 0; i < 8; i += 2)
    {
      oss << std::hex << std::setfill('0') << std::setw(2) << int(buf[i]);
      oss << std::hex << std::setfill('0') << std::setw(2) << int(buf[i+1]);
      if (i < 6)
        oss << ":";
    }
  return Ipv6Address (oss.str().c_str());
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("Ipv6AutoconfigFromMacExample", LOG_LEVEL_INFO);

  std::vector<std::string> macStrs = {
    "00:00:00:00:00:01",
    "00:00:00:00:00:02",
    "00:00:00:00:00:03",
    "00:00:00:00:00:04",
    "00:00:00:00:00:05",
    "00:00:00:00:00:06",
    "00:00:00:00:00:07",
    "00:00:00:00:00:08",
    "00:00:00:00:00:09",
    "00:00:00:00:00:0A"
  };
  std::vector<Mac48Address> macs;
  for (const auto& s : macStrs)
    {
      macs.push_back (Mac48Address (s.c_str ()));
    }

  std::vector<std::string> prefixes = { "2001:1::", "2002:1:1::" };

  for (const auto& prefix : prefixes)
    {
      NS_LOG_INFO ("Prefix: " << prefix);
      for (size_t i = 0; i < macs.size (); ++i)
        {
          Ipv6Address addr = GenerateIpv6Autoconfig (prefix, macs[i]);
          NS_LOG_INFO ("  MAC: " << macStrs[i] << " -> IPv6: " << addr);
        }
    }
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}