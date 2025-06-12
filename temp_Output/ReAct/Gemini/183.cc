#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if set to true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer apNodes;
  apNodes.Create (2);

  NodeContainer staNodes;
  staNodes.Create (4);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid1 = Ssid ("ns-3-ssid-1");
  Ssid ssid2 = Ssid ("ns-3-ssid-2");

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid1),
               "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevices1 = wifi.Install (phy, mac, apNodes.Get (0));

    mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid2),
               "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevices2 = wifi.Install (phy, mac, apNodes.Get (1));

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid1),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices1 = wifi.Install (phy, mac, staNodes.Get (0), staNodes.Get (1));

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid2),
               "ActiveProbing", BooleanValue (false));

    NetDeviceContainer staDevices2 = wifi.Install (phy, mac, staNodes.Get (2), staNodes.Get (3));


  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (apNodes);
  mobility.Install (staNodes);


  InternetStackHelper internet;
  internet.Install (apNodes);
  internet.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces1 = address.Assign (apDevices1);
  Ipv4InterfaceContainer staInterfaces1 = address.Assign (staDevices1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces2 = address.Assign (apDevices2);
  Ipv4InterfaceContainer staInterfaces2 = address.Assign (staDevices2);

  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (staNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

    ApplicationContainer serverApps2 = server.Install (staNodes.Get (2));
  serverApps2.Start (Seconds (1.0));
  serverApps2.Stop (Seconds (10.0));

  UdpClientHelper client (staInterfaces1.GetAddress (0), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (staNodes.Get (1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  UdpClientHelper client2 (staInterfaces2.GetAddress (0), 9);
  client2.SetAttribute ("MaxPackets", UintegerValue (1000));
  client2.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
  client2.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps2 = client2.Install (staNodes.Get (3));
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (10.0));


  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  if (tracing)
    {
      phy.EnablePcapAll ("wifi-simple-adhoc");
    }

  AnimationInterface anim ("wifi-animation.xml");
  anim.SetConstantPosition(apNodes.Get(0), 10.0, 10.0);
  anim.SetConstantPosition(apNodes.Get(1), 30.0, 10.0);
  anim.SetConstantPosition(staNodes.Get(0), 10.0, 20.0);
  anim.SetConstantPosition(staNodes.Get(1), 15.0, 20.0);
  anim.SetConstantPosition(staNodes.Get(2), 30.0, 20.0);
  anim.SetConstantPosition(staNodes.Get(3), 35.0, 20.0);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (11.0));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.bytesSent << "\n";
      std::cout << "  Rx Bytes:   " << i->second.bytesReceived << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024/1024  << " Mbps\n";
    }

  Simulator::Destroy ();

  return 0;
}