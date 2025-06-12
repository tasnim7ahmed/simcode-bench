#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AodvExample");

int main (int argc, char *argv[])
{
  bool enablePcap = true;

  CommandLine cmd;
  cmd.AddValue ("EnablePcap", "Enable/disable PCAP tracing", enablePcap);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (4);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-adhoc");
  mac.SetType ("ns3::AdhocWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingProtocol ("ns3::AodvRoutingProtocol",
                            "AllowedHelloLoss", UintegerValue(2),
                            "HelloInterval", TimeValue(Seconds(1)),
                            "AodvRreqRetries", UintegerValue(2),
                            "ActiveRouteTimeout", TimeValue(Seconds(10))
                           );
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (nodes);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (3), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (enablePcap)
    {
      phy.EnablePcapAll ("aodv-example");
    }

  Simulator::Stop (Seconds (10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4GlobalRouting> globalRouting = Ipv4GlobalRouting::GetInstance();
  globalRouting->PrintRoutingTableAllAt(Seconds(9.0), std::cout);

  Ptr<AodvRoutingProtocol> aodvRouting = DynamicCast<AodvRoutingProtocol>(nodes.Get(0)->GetObject<Ipv4L3Protocol>()->GetRoutingProtocol());
  if (aodvRouting) {
     aodvRouting->PrintRoutingTable(std::cout);
  }
  aodvRouting = DynamicCast<AodvRoutingProtocol>(nodes.Get(1)->GetObject<Ipv4L3Protocol>()->GetRoutingProtocol());
   if (aodvRouting) {
     aodvRouting->PrintRoutingTable(std::cout);
  }
  aodvRouting = DynamicCast<AodvRoutingProtocol>(nodes.Get(2)->GetObject<Ipv4L3Protocol>()->GetRoutingProtocol());
   if (aodvRouting) {
     aodvRouting->PrintRoutingTable(std::cout);
  }
  aodvRouting = DynamicCast<AodvRoutingProtocol>(nodes.Get(3)->GetObject<Ipv4L3Protocol>()->GetRoutingProtocol());
   if (aodvRouting) {
     aodvRouting->PrintRoutingTable(std::cout);
  }

  Simulator::Destroy ();

  return 0;
}