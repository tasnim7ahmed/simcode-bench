#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/command-line.h"
#include "ns3/config-store.h"
#include "ns3/tcp-socket-base.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpWifiMpduAggregation");

void
CalculateThroughput (FlowMonitorHelper *flowmon, Time start, Time end)
{
  flowmon->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon->GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats ();
  double totalBytes = 0.0;
  Time totalDuration = end - start;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      totalBytes += i->second.txBytes;
    }

  double throughput = (totalBytes * 8.0) / totalDuration.GetSeconds () / 1000000;
  std::cout << Simulator::Now ().GetSeconds () << "s: Throughput: " << throughput << " Mbps" << std::endl;

  Simulator::Schedule (MilliSeconds (100), &CalculateThroughput, flowmon, Simulator::Now (), end);
}


int
main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t payloadSize = 1448;
  std::string dataRate ("5Mbps");
  std::string phyRate ("HtMcs7");
  std::string tcpVariant ("TcpNewReno");
  double simulationTime = 10;
  bool pcapTracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("dataRate", "Data rate for TCP socket", dataRate);
  cmd.AddValue ("phyRate", "Physical layer bitrate", phyRate);
  cmd.AddValue ("tcpVariant", "Transport protocol to use: TcpNewReno, "
                  "TcpIllinois, TcpVegas, Tcp запад", tcpVariant);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("pcap", "Enable PCAP tracing", pcapTracing);
  cmd.Parse (argc, argv);

  // Configure TCP variant
  if (tcpVariant.compare ("TcpNewReno") == 0)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));
    }
  else if (tcpVariant.compare ("TcpIllinois") == 0)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpIllinois::GetTypeId ()));
    }
  else if (tcpVariant.compare ("TcpVegas") == 0)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpVegas::GetTypeId ()));
    }
  else
    {
      std::cerr << "Unsupported TCP protocol " << tcpVariant << std::endl;
      exit (1);
    }

  NodeContainer apStaNodes;
  apStaNodes.Create (2);

  Node *apNode = apStaNodes.Get (0);
  Node *staNode = apStaNodes.Get (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();

  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconInterval", TimeValue (MilliSeconds (100)));

  NetDeviceContainer apDevice = wifi.Install (phy, mac, apNode);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevice = wifi.Install (phy, mac, staNode);

  if (pcapTracing)
    {
      phy.EnablePcapAll ("tcp-wifi-mpdu-aggregation");
    }

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apStaNodes);

  InternetStackHelper stack;
  stack.Install (apStaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apStaInterfaces = address.Assign (apStaNodes);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (apNode);
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime + 1));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (staNode, TcpSocketFactory::GetTypeId ());
  ns3TcpSocket->SetAttribute ("CongestionControl", StringValue (tcpVariant));

  BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory",
                                 InetSocketAddress (apStaInterfaces.GetAddress (0), port));
  bulkSendHelper.SetAttribute ("SendSize", UintegerValue (payloadSize));
  bulkSendHelper.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApp = bulkSendHelper.Install (staNode);
  sourceApp.Start (Seconds (1.0));
  sourceApp.Stop (Seconds (simulationTime));

  Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Mac/MpduAggregationEnabled", BooleanValue (true));
  Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Mac/MaxAmsduSize", UintegerValue (7935));
  Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Mac/EdcaQueues/QosQueue/*/*/MaxSize", StringValue ("65535p"));

  // Set Physical Layer bitrate
  Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue (16));
  Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue (16));
  Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Phy/ShortGuardEnabled", BooleanValue (true));
  Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (20));
  Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Phy/GuardInterval", TimeValue (NanoSeconds (400)));
  Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Phy/TxSpatialStreams", UintegerValue (1));
  Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Phy/HtConfiguration/ShortGuardIntervalSupported", BooleanValue (true));
  Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Phy/HtConfiguration/MaxAmsduSize", UintegerValue (7935));
  Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Phy/HtConfiguration/MpduAggregationEnabled", BooleanValue (true));
  Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Phy/HtConfiguration/SupportedMcsSet", StringValue (phyRate));

  // Set TCP data rate
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));
  Config::Set ("/NodeList/*/$ns3::TcpL4Protocol/SocketType", StringValue ("ns3::TcpNewReno"));

  Simulator::Stop (Seconds (simulationTime + 1));

  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll ();
  Simulator::Schedule (Seconds (1.1), &CalculateThroughput, &flowmonHelper, Seconds (1.1), Seconds (simulationTime));

  Simulator::Run ();

  flowmon->SerializeToXmlFile ("tcp-wifi-mpdu-aggregation.flowmon", false, false);

  Simulator::Destroy ();

  return 0;
}