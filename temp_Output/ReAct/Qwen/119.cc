#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdHocWirelessNetwork");

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (6);

  // Setup Wi-Fi
  WifiMacHelper wifiMac;
  WifiPhyHelper wifiPhy;
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager ("ns3::ArfWifiManager");

  wifiMac.SetType ("ns3::AdhocWifiMac");
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  NetDeviceContainer devices = wifi.Install (phy, wifiMac, nodes);

  // Mobility model: grid layout with random movement in a rectangle
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (20.0),
                                 "DeltaY", DoubleValue (20.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)));
  mobility.Install (nodes);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IPv4 addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Setup UDP Echo Server on all nodes
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Setup UDP Echo Clients in ring topology
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      uint32_t j = (i + 1) % nodes.GetN (); // Ring connection
      UdpEchoClientHelper echoClient (interfaces.GetAddress (j, 0), 9);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (20));
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
      echoClient.SetAttribute ("PacketSize", UintegerValue (512));

      ApplicationContainer clientApp = echoClient.Install (nodes.Get (i));
      clientApp.Start (Seconds (2.0));
      clientApp.Stop (Seconds (10.0));
    }

  // Enable flow monitor
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll ();

  // Set up animation output
  AnimationInterface anim ("adhoc_network.xml");
  anim.EnablePacketMetadata (true);

  // Simulation setup
  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();

  // Output flow statistics
  flowmon->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); ++iter)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
      std::cout << "Flow ID: " << iter->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
      std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
      std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
      std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
      std::cout << "  Throughput: " << (iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds () - iter->second.timeFirstTxPacket.GetSeconds ()) ) / 1024 << " Kbps" << std::endl;
      std::cout << "  Mean Delay: " << iter->second.delaySum.GetSeconds () / iter->second.rxPackets << " s" << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}