#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvSimulation");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  bool enableFlowMonitor = false;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("enablePcap", "Enable PCAP Tracing", enablePcap);
  cmd.AddValue ("enableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel (LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (2), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("400"),
                                 "Y", StringValue ("400"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=30.0]"));
  mobility.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-100, 100, -100, 100)),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=10]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  mobility.Install (nodes);

  AnimationInterface anim ("aodv-adhoc.xml");
  anim.SetConstantPosition (nodes.Get (0), 10, 10);
  anim.SetConstantPosition (nodes.Get (1), 50, 50);
  anim.SetConstantPosition (nodes.Get (2), 90, 90);

  if (enablePcap)
  {
    phy.EnablePcapAll ("aodv-adhoc");
  }

  FlowMonitorHelper flowmon;
  if (enableFlowMonitor)
  {
    flowmon.InstallAll ();
  }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  if (enableFlowMonitor)
  {
    flowmon.SerializeToXmlFile ("aodv-adhoc.flowmon", false, true);
  }

  Simulator::Destroy ();
  return 0;
}