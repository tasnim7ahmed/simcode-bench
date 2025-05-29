#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaUdpExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool useIpv6 = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("ipv6", "Set to true to enable IPv6", useIpv6);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (2)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  Ipv6AddressHelper address6;
  Ipv4InterfaceContainer interfaces;
  Ipv6InterfaceContainer interfaces6;

  if (useIpv6)
    {
      address6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
      interfaces6 = address6.Assign (devices);
    }
  else
    {
      address.SetBase ("10.1.1.0", "255.255.255.0");
      interfaces = address.Assign (devices);
    }

  uint16_t port = 9;

  UdpServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper echoClient (useIpv6 ? interfaces6.GetAddress (1,1) : interfaces.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (320));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (50)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (!useIpv6)
    {
      Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    }
  else
    {
      Ipv6GlobalRoutingHelper::PopulateRoutingTables ();
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}