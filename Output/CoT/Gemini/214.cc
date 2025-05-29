#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiExample");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  bool enableNetAnim = false;
  std::string animFile = "wifi-example.xml";

  CommandLine cmd;
  cmd.AddValue ("pcap", "Enable pcap tracing", enablePcap);
  cmd.AddValue ("netanim", "Enable NetAnim animation", enableNetAnim);
  cmd.AddValue ("animFile", "NetAnim animation file name", animFile);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ("UdpClient", LOG_LEVEL_INFO);

  NodeContainer apNode;
  apNode.Create (1);

  NodeContainer staNodes;
  staNodes.Create (3);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice = wifi.Install (phy, mac, apNode);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices = wifi.Install (phy, mac, staNodes);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                               "MinX", DoubleValue (0.0),
                               "MinY", DoubleValue (0.0),
                               "DeltaX", DoubleValue (5.0),
                               "DeltaY", DoubleValue (10.0),
                               "GridWidth", UintegerValue (3),
                               "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (staNodes);

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                               "MinX", DoubleValue (5.0),
                               "MinY", DoubleValue (5.0),
                               "DeltaX", DoubleValue (5.0),
                               "DeltaY", DoubleValue (10.0),
                               "GridWidth", UintegerValue (3),
                               "LayoutType", StringValue ("RowFirst"));
  mobility.Install (apNode);

  InternetStackHelper stack;
  stack.Install (apNode);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer apInterfaces = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (staNodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (staInterfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (staNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient2 (staInterfaces.GetAddress (1), 9);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient2.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps2 = echoClient2.Install (staNodes.Get (2));
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (10.0));


  if (enablePcap)
    {
      phy.EnablePcapAll ("wifi-example");
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  if (enableNetAnim)
    {
       AnimationInterface anim (animFile);
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000  << " Mbps\n";
    }

  Simulator::Destroy ();
  return 0;
}