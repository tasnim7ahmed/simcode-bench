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

int main(int argc, char *argv[]) {
  bool verbose = false;
  uint32_t packetSize = 1472;
  std::string dataRate = "50Mbps";
  std::string channelWidth = "80+80";
  uint32_t mcs = 5;
  std::string guardInterval = "short";
  double distance = 10.0;
  bool rtsCtsEnabled = false;
  std::string transportProtocol = "UDP";

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("packetSize", "Size of packets generated", packetSize);
  cmd.AddValue("dataRate", "Data rate for applications", dataRate);
  cmd.AddValue("channelWidth", "Channel width (20, 40, 80, 160, 80+80)", channelWidth);
  cmd.AddValue("mcs", "Modulation and coding scheme (0-9)", mcs);
  cmd.AddValue("guardInterval", "Guard interval (short, long)", guardInterval);
  cmd.AddValue("distance", "Distance between STA and AP (meters)", distance);
  cmd.AddValue("rtsCtsEnabled", "Enable RTS/CTS", rtsCtsEnabled);
  cmd.AddValue("transportProtocol", "Transport protocol (UDP, TCP)", transportProtocol);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer apNode;
  apNode.Create(1);
  NodeContainer staNode;
  staNode.Create(1);

  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211ac);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns3-wifi");

  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevice = wifiHelper.Install(wifiPhy, wifiMac, staNode);

  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid),
                  "BeaconGeneration", BooleanValue(true));

  NetDeviceContainer apDevice = wifiHelper.Install(wifiPhy, wifiMac, apNode);

  if (channelWidth == "20") {
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("VhtMcs" + std::to_string(mcs)),
                                       "ControlMode", StringValue("VhtMcs0"));
  } else if (channelWidth == "40") {
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("VhtMcs" + std::to_string(mcs)),
                                       "ControlMode", StringValue("VhtMcs0"));
  } else if (channelWidth == "80") {
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("VhtMcs" + std::to_string(mcs)),
                                       "ControlMode", StringValue("VhtMcs0"));
  } else if (channelWidth == "160") {
      wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                         "DataMode", StringValue("VhtMcs" + std::to_string(mcs)),
                                         "ControlMode", StringValue("VhtMcs0"));
  } else if (channelWidth == "80+80") {
      wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                         "DataMode", StringValue("VhtMcs" + std::to_string(mcs)),
                                         "ControlMode", StringValue("VhtMcs0"));
  }
  else {
      std::cerr << "Invalid Channel Width, defaulting to 80 MHz" << std::endl;
      wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                         "DataMode", StringValue("VhtMcs" + std::to_string(mcs)),
                                         "ControlMode", StringValue("VhtMcs0"));
  }

  if (rtsCtsEnabled) {
      wifiPhy.EnableRtsCts(true, 2347);
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNode);
  mobility.Install(apNode);

  InternetStackHelper stack;
  stack.Install(apNode);
  stack.Install(staNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

  uint16_t port = 50000;
  ApplicationContainer sinkApp;
  ApplicationContainer clientApp;

  if (transportProtocol == "UDP") {
    UdpServerHelper echoServer(port);
    sinkApp = echoServer.Install(apNode.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    UdpClientHelper echoClient(apInterface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
    echoClient.SetAttribute("Interval", TimeValue(Time("0.00002")));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    clientApp = echoClient.Install(staNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));
  } else if (transportProtocol == "TCP") {
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    sinkApp = packetSinkHelper.Install(apNode.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
    bulkSendHelper.SetAttribute("SendSize", UintegerValue(packetSize));
    clientApp = bulkSendHelper.Install(staNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));
  }
  else {
        std::cerr << "Invalid transport protocol selected." << std::endl;
        return 1;
  }


  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(11.0));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      if (t.sourceAddress == staInterface.GetAddress(0))
        {
          std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
          std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
          std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
          std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
        }
    }

  Simulator::Destroy();

  return 0;
}