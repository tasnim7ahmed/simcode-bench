#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocGridSimulation");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application containers to log if set to true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (4);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (2),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::RandomBoxMobilityModel",
                             "Bounds", BoxValue (Box (0, 0, 0, 20, 20, 10)));
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      uint32_t nextNodeIndex = (i + 1) % nodes.GetN ();
      UdpEchoClientHelper echoClient (interfaces.GetAddress (nextNodeIndex), 9);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (20));
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
      echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
      ApplicationContainer clientApps = echoClient.Install (nodes.Get (i));
      clientApps.Start (Seconds (2.0 + i * 0.1));
      clientApps.Stop (Seconds (10.0));
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (11.0));

  AnimationInterface anim ("adhoc-grid.anim");

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4GlobalRouting> globalRouting = Ipv4GlobalRouting::GetInstance ();
  globalRouting->PrintRoutingTableAllAt (Seconds (11), std::cout);

  FlowMonitor::StatisticsContainer statistics = monitor->GetFlowStatistics ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = statistics.begin (); i != statistics.end (); ++i)
    {
      std::cout << std::endl << "*** Flow id " << i->first << " ***" << std::endl;
      std::cout << "  Tx Packets: " << i->second.txPackets << std::endl;
      std::cout << "  Rx Packets: " << i->second.rxPackets << std::endl;
      std::cout << "  Lost Packets: " << i->second.lostPackets << std::endl;
      std::cout << "  Throughput: " << i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024  << " Mbps" << std::endl;
      std::cout << "  End-to-End Delay: " << i->second.delaySum.GetSeconds () / i->second.rxPackets << " s" << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}