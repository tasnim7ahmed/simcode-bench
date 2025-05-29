#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = true;
  uint32_t packetSize = 1024;
  std::string dataRate = "5Mbps";
  std::string phyRate = "ErpOfdmRate54Mbps";
  uint32_t numPackets = 1000;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("packetSize", "Size of packet sent", packetSize);
  cmd.AddValue ("dataRate", "Data rate for UDP traffic", dataRate);
  cmd.AddValue ("phyRate", "Physical layer rate", phyRate);
  cmd.AddValue ("numPackets", "Number of packets generated", numPackets);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer staNodes;
  staNodes.Create (2);

  NodeContainer apNode;
  apNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();

  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, staNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, apNode);

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

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode);

  InternetStackHelper stack;
  stack.Install (apNode);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (apNode.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper client1 (apInterfaces.GetAddress (0), 9);
  client1.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  client1.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client1.SetAttribute ("PacketSize", UintegerValue (packetSize));
  client1.SetAttribute ("DataRate", StringValue (dataRate));

  ApplicationContainer clientApps1 = client1.Install (staNodes.Get (0));
  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop (Seconds (10.0));

  UdpClientHelper client2 (apInterfaces.GetAddress (0), 9);
  client2.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  client2.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client2.SetAttribute ("PacketSize", UintegerValue (packetSize));
  client2.SetAttribute ("DataRate", StringValue (dataRate));

  ApplicationContainer clientApps2 = client2.Install (staNodes.Get (1));
  clientApps2.Start (Seconds (3.0));
  clientApps2.Stop (Seconds (10.0));


  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  if (tracing == true)
    {
      phy.EnablePcap ("wifi-example", apDevices);
      phy.EnablePcap ("wifi-example", staDevices);
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (10.0));

  AnimationInterface anim ("wifi-animation.xml");
  anim.SetConstantPosition (apNode.Get (0), 10.0, 10.0);
  anim.SetConstantPosition (staNodes.Get (0), 1.0, 2.0);
  anim.SetConstantPosition (staNodes.Get (1), 1.0, 18.0);

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps\n";
    }

  Simulator::Destroy ();
  return 0;
}