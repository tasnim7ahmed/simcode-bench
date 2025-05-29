#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocGridOlsr");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1000;
  uint32_t numPackets = 1;
  double interval = 1.0;
  double simulationTime = 10.0;
  std::string phyMode ("DsssRate1Mbps");
  double distance = 10.0;
  uint32_t gridSize = 5;
  uint32_t sourceNode = gridSize * gridSize - 1;
  uint32_t sinkNode = 0;
  bool enablePcap = false;
  bool enableAsciiTrace = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("packetSize", "Size of packet sent in bytes", packetSize);
  cmd.AddValue ("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue ("interval", "Interval between packets in seconds", interval);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("phyMode", "Wifi phy mode", phyMode);
  cmd.AddValue ("distance", "Grid distance in meters", distance);
  cmd.AddValue ("gridSize", "Grid size (number of rows and columns)", gridSize);
  cmd.AddValue ("sourceNode", "Source node number", sourceNode);
  cmd.AddValue ("sinkNode", "Sink node number", sinkNode);
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("enableAsciiTrace", "Enable ASCII tracing", enableAsciiTrace);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (gridSize * gridSize);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("RxGain", DoubleValue (-10) );
  wifiPhy.Set ("TxGain", DoubleValue (0) );
  wifiPhy.Set ("CcaEdThreshold", DoubleValue (-85));
  wifiPhy.Set ("RxNoiseFigure", DoubleValue (7) );
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-90) );
  wifiPhy.Set ("EdThreshold", DoubleValue (-85) );

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelay");
  wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel", "Rss", DoubleValue (-95));
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (distance),
                                 "GridWidth", UintegerValue (gridSize),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  OlsrHelper olsr;
  InternetStackHelper internet;
  internet.SetRoutingHelper (olsr);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  UdpServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (sinkNode));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simulationTime));

  UdpClientHelper echoClient (i.GetAddress (sinkNode), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (sourceNode));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simulationTime));

  if (enablePcap)
    {
      wifiPhy.EnablePcapAll ("adhoc-olsr");
    }

  if (enableAsciiTrace)
    {
      AsciiTraceHelper ascii;
      wifiPhy.EnableAsciiAll (ascii.CreateFileStream ("adhoc-olsr.tr"));
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("adhoc-olsr.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}