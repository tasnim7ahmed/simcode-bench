#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpWifiAggregation");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  std::string tcpCongestionControl = "TcpNewReno";
  std::string appDataRate = "5Mbps";
  uint32_t payloadSize = 1448;
  std::string phyRate = "HtMcs7";
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("tcpCongestionControl", "TCP congestion control algorithm", tcpCongestionControl);
  cmd.AddValue ("appDataRate", "Application data rate", appDataRate);
  cmd.AddValue ("payloadSize", "Payload size", payloadSize);
  cmd.AddValue ("phyRate", "Physical layer bitrate", phyRate);
  cmd.AddValue ("simulationTime", "Simulation time", simulationTime);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue (tcpCongestionControl));

  NodeContainer nodes;
  nodes.Create (2);

  NodeContainer apNode = NodeContainer (nodes.Get (0));
  NodeContainer staNode = NodeContainer (nodes.Get (1));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconInterval", TimeValue (Seconds (0.05)));

  NetDeviceContainer apDevice = wifi.Install (phy, mac, apNode);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevice = wifi.Install (phy, mac, staNode);

  if (enablePcap)
  {
      phy.EnablePcap ("TcpWifiAggregation", apDevice);
      phy.EnablePcap ("TcpWifiAggregation", staDevice);
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

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNode);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign (staDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  Address sinkAddress (InetSocketAddress (apInterface.GetAddress (0), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (apNode);
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (simulationTime + 1));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (staNode.Get (0), TcpSocketFactory::GetTypeId ());

  BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), port));
  bulkSendHelper.SetAttribute ("SendSize", UintegerValue (payloadSize));
  bulkSendHelper.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApps = bulkSendHelper.Install (staNode);
  sourceApps.Start (Seconds (2.0));
  sourceApps.Stop (Seconds (simulationTime + 1));

  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/Aggregation", BooleanValue (true));

  phy.Set ("ShortGuardEnabled", BooleanValue (true));
  wifi.SetRemoteStationManager ("ns3::HtWifiMac", "DataMode", StringValue (phyRate), "ControlMode", StringValue (phyRate));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 2));

  Simulator::Schedule (Seconds (simulationTime + 1.5), [&](){
        monitor->CheckForLostPackets ();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
        {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
            if (t.sourceAddress == staInterface.GetAddress (0) && t.destinationAddress == apInterface.GetAddress (0))
            {
                std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
                std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
                std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
                std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024  << " Mbps\n";
            }
        }
  });

  Simulator::Run ();

  monitor->SerializeToXmlFile("TcpWifiAggregation.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}