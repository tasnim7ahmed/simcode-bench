#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AodvExample");

int main (int argc, char *argv[])
{
  bool enablePcap = true;

  CommandLine cmd;
  cmd.AddValue ("EnablePcap", "Enable pcap tracing", enablePcap);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ( "AodvRoutingProtocol", LOG_LEVEL_INFO );

  NodeContainer nodes;
  nodes.Create (5);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  mac.SetType (WIFI_MAC_ADHOC);
  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("50.0"),
                                 "Y", StringValue ("50.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 0, 100, 100)));
  mobility.Install (nodes);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (4));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (i.GetAddress (4), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (enablePcap)
  {
    phy.EnablePcapAll ("aodv");
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (10.0));

  AnimationInterface anim ("aodv.xml");
  anim.SetMaxPktsPerTraceFile(5000000);

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4RoutingTableGroup> routingTableGroup = internet.GetRoutingTables ();
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
  {
    Ptr<Ipv4RoutingTable> routingTable = routingTableGroup->GetRoutingTable (i);
    routingTable->PrintRoutingTable ();
  }


  FlowMonitorHelper::WriteStatistics ("aodv.flowmon", monitor->GetFlowStats ());

  Simulator::Destroy ();

  return 0;
}