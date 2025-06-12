#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/icmpv6-echo.h"
#include "ns3/drop-tail-queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WsnPing6");

int main (int argc, char *argv[])
{
  LogComponent::EnableIlog (LOG_LEVEL_ALL);
  LogComponent::EnableIlog (LOG_PREFIX_TIME);
  LogComponent::EnableIlog (LOG_PREFIX_NODE);
  LogComponent::EnableIlog (LOG_PREFIX_LEVEL);

  bool verbose = true;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log ifconfig interfaces.", verbose);
  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponent::Enable ("Icmpv6L4Protocol", LOG_LEVEL_ALL);
      LogComponent::Enable ("Icmpv6EchoClient", LOG_LEVEL_ALL);
      LogComponent::Enable ("Icmpv6EchoServer", LOG_LEVEL_ALL);
    }

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_802154);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  wifi.Install (phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (nodes);

  Ptr<Icmpv6EchoServer> echoServer = CreateObject<Icmpv6EchoServer> ();
  nodes.Get (1)->AddApplication (echoServer);
  echoServer->SetStartTime (Seconds (1.0));
  echoServer->SetStopTime (Seconds (10.0));

  Ptr<Icmpv6EchoClient> echoClient = CreateObject<Icmpv6EchoClient> ();
  echoClient->SetRemote (interfaces.GetAddress (1, 1));
  echoClient->SetStartTime (Seconds (2.0));
  echoClient->SetStopTime (Seconds (9.0));
  nodes.Get (0)->AddApplication (echoClient);

  // Tracing
  phy.EnablePcapAll ("wsn-ping6");

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}