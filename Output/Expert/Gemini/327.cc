#include <iostream>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
  LogComponentEnable ("OlsrRoutingProtocol", LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create (6);

  WifiHelper wifi;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  Ssid ssid = Ssid ("ns-3-adhoc");
  mac.SetType ("ns3::AdhocWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.Install (nodes);

  OlsrHelper olsr;
  InternetStackHelper internet;
  internet.SetRoutingHelper (olsr);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  UdpServerHelper server (9);
  ApplicationContainer apps = server.Install (nodes.Get (5));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  UdpClientHelper client (interfaces.GetAddress (5), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (100));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client.Install (nodes.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));

  AnimationInterface anim ("manet.xml");
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
  {
    anim.SetNodeDescription (nodes.Get (i), "Node " + std::to_string(i));
  }

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}