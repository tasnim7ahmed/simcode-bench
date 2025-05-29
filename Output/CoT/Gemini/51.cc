#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiTcpAggregation");

int main (int argc, char *argv[])
{
  bool verbose = false;
  std::string phyMode ("HtMcs7");
  uint32_t payloadSize = 1472;
  std::string dataRate ("5Mbps");
  std::string tcpVariant ("TcpNewReno");
  double simulationTime = 10;
  bool pcapTracing = false;
  std::string throughputTraceFile = "wifi-tcp-aggregation.dat";

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if set.", verbose);
  cmd.AddValue ("phyMode", "Wifi phy mode", phyMode);
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("dataRate", "Data rate for TCP", dataRate);
  cmd.AddValue ("tcpVariant", "Transport protocol to use: TcpNewReno, "
                  "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno",
                  tcpVariant);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("pcap", "Enable PCAP tracing", pcapTracing);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  tcpVariant = std::string ("ns3::") + tcpVariant;

  TypeId tid = TypeId::LookupByName (tcpVariant);
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (tid));

  NodeContainer apNode;
  apNode.Create (1);

  NodeContainer staNode;
  staNode.Create (1);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhy.Set ("TxPowerStart", DoubleValue (10));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (10));

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid ("ns-3-ssid");
  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid),
                   "BeaconInterval", TimeValue (MilliSeconds (100)));

  NetDeviceContainer apDevice = wifi.Install (wifiPhy, wifiMac, apNode);

  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevice = wifi.Install (wifiPhy, wifiMac, staNode);

  if (pcapTracing)
    {
      wifiPhy.EnablePcap ("wifi-tcp-aggregation", apDevice);
      wifiPhy.EnablePcap ("wifi-tcp-aggregation", staDevice);
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
  mobility.Install (apNode);
  mobility.Install (staNode);

  InternetStackHelper internet;
  internet.Install (apNode);
  internet.Install (staNode);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (NetDeviceContainer::Create (apDevice, staDevice));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  Address sinkAddress (InetSocketAddress (i.GetAddress (1), port));

  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install (apNode.Get (0));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simulationTime + 1));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (staNode.Get (0), TcpSocketFactory::GetTypeId ());

  Ptr<BulkSendApplication> bulkSendApp = CreateObject<BulkSendApplication> ();
  bulkSendApp->SetSocket (ns3TcpSocket);
  bulkSendApp->SetPeer (sinkAddress);
  bulkSendApp->SetSendSize (payloadSize);
  bulkSendApp->SetMaxBytes (0);
  bulkSendApp->SetAttribute ("DataRate", StringValue (dataRate));
  staNode.Get (0)->AddApplication (bulkSendApp);
  bulkSendApp->SetStartTime (Seconds (1.0));
  bulkSendApp->SetStopTime (Seconds (simulationTime));

  Simulator::Stop (Seconds (simulationTime + 1));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  Simulator::Schedule (Seconds (simulationTime + 0.5), [&](){
        monitor->CheckForLostPackets ();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
        double totalThroughput = 0.0;
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
          {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
            if (t.destinationAddress == i.GetAddress (1))
              {
                totalThroughput += i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000;
              }
          }
        std::cout << "Total Throughput: " << totalThroughput << " Mbps" << std::endl;

  });

  Simulator::Run ();

  monitor->SerializeToXmlFile("wifi-tcp-aggregation.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}