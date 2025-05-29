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
  bool rtsCtsEnabled = false;
  std::string transportProtocol = "Udp";
  std::string guardInterval = "Long";
  uint32_t mcs = 0;
  uint32_t channelWidth = 80;
  double distance = 1.0;
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue("rtsCts", "Enable or disable RTS/CTS (0 or 1)", rtsCtsEnabled);
  cmd.AddValue("transportProtocol", "Transport protocol (Udp or Tcp)", transportProtocol);
  cmd.AddValue("guardInterval", "Guard interval (Short or Long)", guardInterval);
  cmd.AddValue("mcs", "MCS rate (0-9)", mcs);
  cmd.AddValue("channelWidth", "Channel width (20, 40, 80, 160)", channelWidth);
  cmd.AddValue("distance", "Distance between nodes", distance);
  cmd.AddValue("verbose", "Enable verbose output", verbose);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer apNode;
  apNode.Create(1);

  NodeContainer staNode;
  staNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211ac);

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(MilliSeconds(100)));

  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

  if (rtsCtsEnabled) {
    Config::Set("/NodeList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", StringValue("0"));
  }

  Config::Set("/NodeList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(channelWidth));

  std::string rate;
  if (channelWidth == 160) {
      rate = "VhtSigMcs" + std::to_string(mcs) + "DoubleGuardInterval";
  } else {
      if (guardInterval == "Short") {
          rate = "VhtSigMcs" + std::to_string(mcs);
      } else {
          rate = "VhtSigMcs" + std::to_string(mcs) + "LongGuardInterval";
      }
  }

  Config::Set("/NodeList/*/$ns3::WifiNetDevice/Phy/TxSpatialStreams", UintegerValue(1));
  Config::Set("/NodeList/*/$ns3::WifiNetDevice/Phy/TxAntennas", UintegerValue(1));
  Config::Set("/NodeList/*/$ns3::WifiNetDevice/Phy/RxSpatialStreams", UintegerValue(1));
  Config::Set("/NodeList/*/$ns3::WifiNetDevice/Phy/RxAntennas", UintegerValue(1));

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue(rate),
                               "ControlMode", StringValue(rate));
  
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(distance),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.Install(staNode);

  InternetStackHelper stack;
  stack.Install(apNode);
  stack.Install(staNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(apDevice);
  address.Assign(staDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 50000;
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper packetSinkHelper(transportProtocol == "Tcp" ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(apNode.Get(0));

  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  Address remoteAddress(InetSocketAddress(interfaces.GetAddress(1), port));

  ApplicationContainer clientApp;
  if (transportProtocol == "Udp") {
    UdpClientHelper client(remoteAddress);
    client.SetAttribute("Interval", TimeValue(MicroSeconds(1)));
    client.SetAttribute("MaxPackets", UintegerValue(0));
    clientApp = client.Install(staNode.Get(0));
  } else {
    OnOffHelper client("ns3::TcpSocketFactory", remoteAddress);
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("DataRate", DataRateValue(DataRate("50Mbps")));
    clientApp = client.Install(staNode.Get(0));
  }

  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));
  
  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double goodput = 0.0;

  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    if (t.destinationAddress == interfaces.GetAddress(1)) {
        goodput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
        break;
    }
  }

  std::cout << "Goodput: " << goodput << " Mbps" << std::endl;

  Simulator::Destroy();
  return 0;
}