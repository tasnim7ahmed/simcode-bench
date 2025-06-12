#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool verbose = false;
  uint32_t nSta = 1;
  double simulationTime = 10;
  std::string phyMode = "OfdmRate6Mbps";
  double distance = 10.0;
  uint16_t port = 9;
  bool enableRtsCts = false;
  bool enable8080 = false;
  bool enableExtendedBA = false;
  bool enableUlOfdma = false;
  bool useSpectrum = false;
  uint32_t packetSize = 1472;
  std::string protocol = "Udp";
  std::string mcs = "VhtMcs5";
  uint32_t targetThroughputKbps = 1000;
  uint32_t channelWidth = 80;
  double guardInterval = 0.8;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if set.", verbose);
  cmd.AddValue("nSta", "Number of wifi stations", nSta);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue("distance", "Distance STA AP", distance);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
  cmd.AddValue("enable8080", "Enable 80+80MHz channels", enable8080);
  cmd.AddValue("enableExtendedBA", "Enable extended Block Ack", enableExtendedBA);
  cmd.AddValue("enableUlOfdma", "Enable UL OFDMA", enableUlOfdma);
  cmd.AddValue("useSpectrum", "Use spectrum model", useSpectrum);
  cmd.AddValue("packetSize", "Size of application packet sent", packetSize);
  cmd.AddValue("protocol", "Transport protocol to use: Udp or Tcp", protocol);
  cmd.AddValue("mcs", "Modulation and Coding Scheme", mcs);
  cmd.AddValue("targetThroughputKbps", "Target throughput in Kbps", targetThroughputKbps);
  cmd.AddValue("channelWidth", "Channel Width in MHz", channelWidth);
  cmd.AddValue("guardInterval", "Guard Interval in us", guardInterval);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer staNodes;
  staNodes.Create(nSta);
  NodeContainer apNode;
  apNode.Create(1);

  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_STANDARD_80211ax);

  YansWifiChannelHelper channel;
  YansWifiPhyHelper phy = useSpectrum ? YansWifiPhyHelper::Default() : YansWifiPhyHelper::Default();

  if (useSpectrum) {
      SpectrumWifiPhyHelper spectrumPhy = SpectrumWifiPhyHelper::Default();
      channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelay");
      channel.AddPropagationLoss("ns3::FriisPropagationLossModel", "Frequency", DoubleValue(5.18e9));
      spectrumPhy.SetChannel(channel.Create());
      phy = spectrumPhy;
  } else {
      channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelay");
      channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
      phy.SetChannel(channel.Create());
  }

  wifiHelper.SetRemoteStationManager("ns3::HeWifiMacQueueSize",
                                     "Cwmin", UintegerValue(0),
                                     "Cwmax", UintegerValue(0));

  WifiMacHelper macHelper;
  Ssid ssid = Ssid("ns-3-ssid");

  macHelper.SetType("ns3::HeApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BE_QosControlAckPolicy", BooleanValue(true),
                    "EnableUlOfdma", BooleanValue(enableUlOfdma));

  NetDeviceContainer apDevice;
  apDevice = wifiHelper.Install(phy, macHelper, apNode);

  macHelper.SetType("ns3::HeStaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BE_QosControlAckPolicy", BooleanValue(true),
                    "EnableUlOfdma", BooleanValue(enableUlOfdma));

  NetDeviceContainer staDevices;
  staDevices = wifiHelper.Install(phy, macHelper, staNodes);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(nSta),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodes);

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(distance + 5),
                                "DeltaX", DoubleValue(0.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(1),
                                "LayoutType", StringValue("RowFirst"));
  mobility.Install(apNode);

  InternetStackHelper stack;
  stack.Install(apNode);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint32_t packetRate = targetThroughputKbps * 1000 / 8 / packetSize;
  std::string dataRate = std::to_string(packetRate) + "p/s";

  ApplicationContainer clientApps;
  ApplicationContainer serverApps;

  if (protocol == "Udp") {
      UdpServerHelper echoServer(port);
      serverApps = echoServer.Install(apNode);
      serverApps.Start(Seconds(0.0));
      serverApps.Stop(Seconds(simulationTime + 1));

      UdpClientHelper echoClient(apInterface.GetAddress(0), port);
      echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
      echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0 / packetRate)));
      echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
      clientApps = echoClient.Install(staNodes);
      clientApps.Start(Seconds(1.0));
      clientApps.Stop(Seconds(simulationTime));
  } else if (protocol == "Tcp") {
      PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
      serverApps = sinkHelper.Install(apNode);
      serverApps.Start(Seconds(0.0));
      serverApps.Stop(Seconds(simulationTime + 1));

      BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
      bulkSendHelper.SetAttribute("SendSize", UintegerValue(packetSize));
      clientApps = bulkSendHelper.Install(staNodes);
      clientApps.Start(Seconds(1.0));
      clientApps.Stop(Seconds(simulationTime));
  } else {
      std::cerr << "Invalid protocol: " << protocol << std::endl;
      return 1;
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime + 1));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort << "\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps\n";
  }

  monitor->SerializeToXmlFile("wifi-80211ax.flowmon", true, true);

  Simulator::Destroy();

  return 0;
}