#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AodvExample");

int main (int argc, char *argv[])
{
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel (LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (4);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  mac.SetType (WifiMacHelper::ADHOC,
               "Ssid", Ssid ("ns-3-adhoc"));

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingProtocol (aodv);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Set mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                               "MinX", DoubleValue (0.0),
                               "MinY", DoubleValue (0.0),
                               "DeltaX", DoubleValue (5.0),
                               "DeltaY", DoubleValue (10.0),
                               "GridWidth", UintegerValue (3),
                               "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
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

  // Create an OnOff application to send UDP datagrams from node 0 to node 3
  OnOffHelper onOffHelper ("ns3::UdpSocketFactory", Address (InetSocketAddress (interfaces.GetAddress (3), 5000)));
  onOffHelper.SetConstantRate (DataRate ("500kbps"));
  ApplicationContainer onOffApps = onOffHelper.Install (nodes.Get (0));
  onOffApps.Start (Seconds (3.0));
  onOffApps.Stop (Seconds (10.0));

  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 5000));
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (3));
  sinkApps.Start (Seconds (3.0));
  sinkApps.Stop (Seconds (10.0));


  if (tracing == true)
    {
      phy.EnablePcapAll ("aodv-example");
    }

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}