#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaUdp");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool useIpv6 = false;
  bool enableLogging = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("useIpv6", "Use IPv6 addressing if true", useIpv6);
  cmd.AddValue ("enableLogging", "Enable logging to the console", enableLogging);
  cmd.Parse (argc, argv);

  if (enableLogging)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv6AddressHelper ipv6;
  ipv6.SetBase ("2001:db8::", Ipv6Prefix (64));

  Ipv4InterfaceContainer ipv4Interfaces;
  Ipv6InterfaceContainer ipv6Interfaces;
  if (useIpv6)
    {
      ipv6Interfaces = ipv6.Assign (devices);
    }
  else
    {
      ipv4Interfaces = ipv4.Assign (devices);
    }

  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (useIpv6 ? ipv6Interfaces.GetAddress (1, 1) : ipv4Interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (320));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (50)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));

  if (enableLogging) {
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll (ascii.CreateFileStream ("csma-udp.tr"));
    csma.EnablePcapAll ("csma-udp", false);
  }

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}