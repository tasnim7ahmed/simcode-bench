#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi80211axSimulation");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nSta = 1;
  double simTime = 10;
  std::string payloadSize = "1472";
  std::string dataRate = "OfdmRate9";
  double distance = 10.0;
  bool rtsCtsEnabled = false;
  bool extendedBA = false;
  bool ulOfdma = false;
  bool dlMuMimo = false;
  std::string phyStandard = "802.11ax";
  std::string channelWidth = "80";
  std::string guardInterval = "0.8us";
  std::string frequencyBand = "5"; // 2, 5, or 6
  bool useSpectrumPhy = false;
  std::string transportProtocol = "TCP"; // TCP or UDP
  uint32_t port = 9;
  uint32_t targetThroughputKbps = 1000; //Target throughput in Kbps

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("nSta", "Number of stations", nSta);
  cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("dataRate", "Data rate", dataRate);
  cmd.AddValue ("distance", "Distance between AP and station (m)", distance);
  cmd.AddValue ("rtsCts", "Enable RTS/CTS", rtsCtsEnabled);
  cmd.AddValue ("extendedBA", "Enable Extended Block Ack", extendedBA);
  cmd.AddValue ("ulOfdma", "Enable UL OFDMA", ulOfdma);
  cmd.AddValue ("dlMuMimo", "Enable DL MU-MIMO", dlMuMimo);
  cmd.AddValue ("phyStandard", "802.11 PHY standard", phyStandard);
  cmd.AddValue ("channelWidth", "Channel width (MHz)", channelWidth);
  cmd.AddValue ("guardInterval", "Guard Interval (0.8us, 1.6us, or 3.2us)", guardInterval);
  cmd.AddValue ("frequencyBand", "Frequency band (2, 5, or 6 GHz)", frequencyBand);
  cmd.AddValue ("useSpectrumPhy", "Use Spectrum PHY model", useSpectrumPhy);
  cmd.AddValue ("transportProtocol", "Transport protocol (TCP or UDP)", transportProtocol);
  cmd.AddValue ("targetThroughputKbps", "Target throughput in Kbps", targetThroughputKbps);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("Wifi80211axSimulation", LOG_LEVEL_ALL);
    }

  NodeContainer staNodes;
  staNodes.Create (nSta);

  NodeContainer apNode;
  apNode.Create (1);

  YansWifiChannelHelper channel;
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();

  if (useSpectrumPhy)
    {
      channel = YansWifiChannelHelper::MakeFrequency (StringValue (frequencyBand + "GHz"));
      phy = YansWifiPhyHelper::MakeSpectrumPhy ();
    }
  else
    {
      channel = YansWifiChannelHelper::Default ();
    }

  YansWifiPropagationLossModel propagationLossModel = YansWifiPropagationLossModel ();
  YansWifiPropagationDelayModel propagationDelayModel = YansWifiPropagationDelayModel ();
  channel.AddPropagationLoss ("ns3::DistancePropagationLossModel");
  channel.AddPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");

  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211ax);

  WifiMacHelper mac;

  Ssid ssid = Ssid ("ns3-80211ax");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, staNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconGeneration", BooleanValue (true));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, apNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (distance),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (nSta),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (staNodes);

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (0.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (1),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.Install (apNode);

  InternetStackHelper stack;
  stack.Install (apNode);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeIface;
  staNodeIface = address.Assign (staDevices);
  Ipv4InterfaceContainer apNodeIface;
  apNodeIface = address.Assign (apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t sinkPort = port;
  Address sinkAddress (InetSocketAddress (apNodeIface.GetAddress (0), sinkPort));

  if(transportProtocol == "TCP"){
    TcpEchoClientHelper echoClient (apNodeIface.GetAddress(0), sinkPort);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (4294967295));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.001))); // Adjust for desired throughput
    echoClient.SetAttribute ("PacketSize", UintegerValue (std::stoi(payloadSize)));
    ApplicationContainer clientApps = echoClient.Install (staNodes);

    TcpEchoServerHelper echoServer (sinkPort);
    ApplicationContainer serverApps = echoServer.Install (apNode);
    serverApps.Start (Seconds (0.0));
    serverApps.Stop (Seconds (simTime + 1));
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (simTime));
  } else {
    UdpClientHelper client (apNodeIface.GetAddress(0), sinkPort);
    client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
    client.SetAttribute ("Interval", TimeValue (Seconds (0.001))); // Adjust for desired throughput
    client.SetAttribute ("PacketSize", UintegerValue (std::stoi(payloadSize)));
    ApplicationContainer clientApps = client.Install (staNodes);

    UdpServerHelper echoServer (sinkPort);
    ApplicationContainer serverApps = echoServer.Install (apNode);
    serverApps.Start (Seconds (0.0));
    serverApps.Stop (Seconds (simTime + 1));
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (simTime));
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simTime + 1));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort << "\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000/ 1000  << " Mbps\n";
    }

  monitor->SerializeToXmlFile ("wifi-80211ax.flowmon", false, true);

  Simulator::Destroy ();

  return 0;
}