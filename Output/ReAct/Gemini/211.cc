#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocWifiExample");

int main (int argc, char *argv[])
{
  bool enableMonitor = false;
  std::string animFile = "adhoc.xml";

  CommandLine cmd;
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableMonitor);
  cmd.AddValue ("animFile", "File Name for Animation Output", animFile);
  cmd.Parse (argc, argv);

  LogComponent::SetLevel (LogComponent::GetComponent ("UdpClient"), LOG_LEVEL_INFO);
  LogComponent::SetLevel (LogComponent::GetComponent ("UdpServer"), LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (5);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  mac.SetType (WifiMacHelper::ADHOC,
               "Ssid", Ssid ("ns-3-adhoc"));

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("0.0"),
                                 "Y", StringValue ("0.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpServerHelper server (9);
  ApplicationContainer apps = server.Install (nodes.Get (4));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  UdpClientHelper client (interfaces.GetAddress (4), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Time ("0.01")));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client.Install (nodes.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  FlowMonitorHelper flowmon;
  if (enableMonitor)
    {
      flowmon.InstallAll ();
    }

  AnimationInterface anim ("adhoc.xml");
  anim.SetConstantPosition (nodes.Get (0), 10.0, 10.0);
  anim.SetConstantPosition (nodes.Get (1), 20.0, 20.0);
  anim.SetConstantPosition (nodes.Get (2), 30.0, 30.0);
  anim.SetConstantPosition (nodes.Get (3), 40.0, 40.0);
  anim.SetConstantPosition (nodes.Get (4), 50.0, 50.0);

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  if (enableMonitor)
    {
      Ptr<FlowMonitor> monitor = flowmon.GetMonitor ();
      monitor->CheckForLostPackets ();
      std::ofstream filestream;
      filestream.open ("AdhocWifi.flowmon", std::ios::out);
      monitor->SerializeToXmlFile ("AdhocWifi.flowmon", true, true);
    }

  Simulator::Destroy ();
  return 0;
}