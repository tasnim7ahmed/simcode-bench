#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetOlsrExample");

void
PrintRoutingTable (Ptr<Node> node, Time printTime)
{
  Ipv4StaticRoutingHelper staticRouting;
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  Ipv4RoutingTableEntry route;
  Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper> (&std::cout);
  NS_LOG_UNCOND ("Routing table of node " << node->GetId () << " at " << Simulator::Now ().GetSeconds () << "s:");
  node->GetObject<Ipv4> ()->GetRoutingProtocol ()->PrintRoutingTable (stream);
}

void
SchedulePrintRoutingTables (NodeContainer nodes, Time interval, Time stopTime)
{
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Simulator::Schedule (Seconds (0.1) + Seconds (i*0.1), &PrintRoutingTable, nodes.Get (i), Seconds (0));
    }
  Time next = Simulator::Now () + interval;
  if (next < stopTime)
    {
      Simulator::Schedule (interval, &SchedulePrintRoutingTables, nodes, interval, stopTime);
    }
}

int
main (int argc, char *argv[])
{
  Time simTime = Seconds (10.0);

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (6);

  // Wifi Setup
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Mobility 
  MobilityHelper mobility;
  ObjectFactory posFactory;
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"),
                             "PositionAllocator", StringValue ("ns3::RandomRectanglePositionAllocator[MinX=0.0|MinY=0.0|MaxX=100.0|MaxY=100.0]"));
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "MaxX", DoubleValue (100.0),
                                "MaxY", DoubleValue (100.0));
  mobility.Install (nodes);

  // Internet Stack with OLSR
  OlsrHelper olsr;
  Ipv4StaticRoutingHelper staticRouting;
  Ipv4ListRoutingHelper list;
  list.Add (staticRouting, 0);
  list.Add (olsr, 10);

  InternetStackHelper internet;
  internet.SetRoutingHelper (list);
  internet.Install (nodes);

  // IP Address Assignment
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // UDP Application (OnOff -> node 0 to node 5)
  uint16_t port = 4000;
  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (5), port));
  onoff.SetAttribute ("PacketSize", UintegerValue (512));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("1Mbps")));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (9.0)));

  ApplicationContainer app = onoff.Install (nodes.Get (0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (nodes.Get (5));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (simTime);

  // Print routing tables periodically
  SchedulePrintRoutingTables (nodes, Seconds (2.0), simTime);

  // Optional: NetAnim for visualization (can be commented out if not needed)
  // AnimationInterface anim("manet-olsr.xml");

  Simulator::Stop (simTime);
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}