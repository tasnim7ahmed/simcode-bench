#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/log.h"
#include "ns3/mac48-address.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6AutoconfigTest");

static std::string AutoconfigureIpv6Address (const std::string& prefix, const Mac48Address& mac)
{
  uint8_t eui[8];
  uint8_t macBytes[6];
  mac.CopyTo (macBytes);

  // Convert the MAC-48 to EUI-64
  eui[0] = macBytes[0] ^ 0x02; // Flip U/L bit
  eui[1] = macBytes[1];
  eui[2] = macBytes[2];
  eui[3] = 0xff;
  eui[4] = 0xfe;
  eui[5] = macBytes[3];
  eui[6] = macBytes[4];
  eui[7] = macBytes[5];

  char eui64str[30];
  snprintf(eui64str, sizeof(eui64str), "%02x%02x:%02xff:fe%02x:%02x%02x", eui[0], eui[1], eui[2], eui[5], eui[6], eui[7]);

  // Format: <prefix>:<eui64>
  char addrStr[80];
  snprintf(addrStr, sizeof(addrStr), "%s%02x%02x:%02xff:fe%02x:%02x%02x",
           prefix.c_str(),
           eui[0], eui[1],
           eui[2],
           eui[5],
           eui[6], eui[7]);

  // Insert a colon after the prefix if not already present
  std::string pfx = prefix;
  if (pfx.back() != ':')
    pfx += ":";

  char finalStr[80];
  snprintf(finalStr, sizeof(finalStr),
           "%s%02x%02x:%02xff:fe%02x:%02x%02x",
           pfx.c_str(),
           eui[0], eui[1],
           eui[2],
           eui[5],
           eui[6], eui[7]);

  return std::string(finalStr);
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("Ipv6AutoconfigTest", LOG_LEVEL_INFO);

  // Define 10 specific MAC addresses
  Mac48Address macs[10] = {
    Mac48Address ("00:11:22:33:44:55"),
    Mac48Address ("00:aa:bb:cc:dd:ee"),
    Mac48Address ("12:34:56:78:9a:bc"),
    Mac48Address ("de:ad:be:ef:00:01"),
    Mac48Address ("02:00:00:00:00:01"),
    Mac48Address ("0a:0b:0c:0d:0e:0f"),
    Mac48Address ("1a:2b:3c:4d:5e:6f"),
    Mac48Address ("ff:ee:dd:cc:bb:aa"),
    Mac48Address ("01:23:45:67:89:ab"),
    Mac48Address ("10:20:30:40:50:60")
  };

  std::string prefixes[2] = { "2001:1::", "2002:1:1::" };

  for (int p = 0; p < 2; ++p)
    {
      NS_LOG_INFO ("Prefix: " << prefixes[p]);
      for (int i = 0; i < 10; ++i)
        {
          std::string addr = AutoconfigureIpv6Address (prefixes[p], macs[i]);
          NS_LOG_INFO ("  MAC: " << macs[i] << " -> IPv6: " << addr);
        }
    }

  Simulator::Stop (Seconds (0.01));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}