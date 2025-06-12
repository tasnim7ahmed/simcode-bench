#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-global-routing-helper.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LanSimulation");

int main (int argc, char *argv[])
{
  bool enableIpv6 = false;
  bool enableLogging = false;

  CommandLine cmd;
  cmd.AddValue ("enableIpv6", "Enable IPv6 addressing", enableIpv6);
  cmd.AddValue ("enableLogging", "Enable simulation logging", enableLogging);
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

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  Ipv6AddressHelper address6;
  Ipv4InterfaceContainer interfaces;
  Ipv6InterfaceContainer interfaces6;

  if (!enableIpv6)
    {
      address.SetBase ("10.1.1.0", "255.255.255.0");
      interfaces = address.Assign (devices);
    }
  else
    {
      address6.SetBase (Ipv6Address("2001:db8::"), Ipv6Prefix (64));
      interfaces6 = address6.Assign (devices);
    }

  uint16_t port = 9;

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper client (enableIpv6 ? interfaces6.GetAddress (1, 1) : interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (320));
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (50)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (!enableIpv6)
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