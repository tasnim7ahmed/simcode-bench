#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi80211acSimulation");

int main(int argc, char *argv[]) {
  bool useRtsCts = false;
  std::string trafficType = "UDP";
  int mcs = 5;
  int channelWidth = 80;
  std::string guardInterval = "short";
  double distance = 10.0;

  CommandLine cmd;
  cmd.AddValue("useRtsCts", "Enable RTS/CTS (0 or 1)", useRtsCts);
  cmd.AddValue("trafficType", "Traffic type (UDP or TCP)", trafficType);
  cmd.AddValue("mcs", "MCS value (0-9)", mcs);
  cmd.AddValue("channelWidth", "Channel width (20, 40, 80, 160)", channelWidth);
  cmd.AddValue("guardInterval", "Guard interval (short or long)", guardInterval);
  cmd.AddValue("distance", "Distance between STA and AP (meters)", distance);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);
  NodeContainer staNodes = NodeContainer(nodes.Get(0));
  NodeContainer apNodes = NodeContainer(nodes.Get(1));

  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211ac);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns3-80211ac");
  wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifiHelper.Install(wifiPhy, wifiMac, staNodes);

  wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconGeneration", BooleanValue(true));
  NetDeviceContainer apDevices = wifiHelper.Install(wifiPhy, wifiMac, apNodes);

  if (useRtsCts) {
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/ এডdHe এডedRts", BooleanValue(true));
  }

  std::stringstream ss;
  ss << "HtMcs" << mcs;
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/NonErpDataRate", StringValue(ss.str()));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/ErpDataRate", StringValue(ss.str()));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/ControlDataRate", StringValue("OfdmRate6Mbps"));

  std::stringstream channelWidthString;
    channelWidthString << channelWidth << "MHz";
    Config::Set ("/ChannelList/0/Frequency", UintegerValue (5180));
    Config::Set ("/ChannelList/0/ChannelWidth", UintegerValue (channelWidth));

  if (guardInterval == "short") {
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/GuardInterval", TimeValue(NanoSeconds(400)));
  } else {
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/GuardInterval", TimeValue(NanoSeconds(800)));
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0), "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(10.0), "GridWidth", UintegerValue(3), "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodes);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);

  Ptr<Node> apNode = apNodes.Get(0);
  Ptr<MobilityModel> apMobility = apNode->GetObject<MobilityModel>();
  apMobility->SetPosition(Vector(distance, 0.0, 0.0));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(staDevices, apDevices));

  uint16_t port = 50000;
  uint32_t packetSize = 1472;
  DataRate dataRate = DataRate("50Mbps");
  Time startTime = Seconds(1.0);
  Time stopTime = Seconds(10.0);

  if (trafficType == "UDP") {
      UdpClientHelper clientHelper(interfaces.GetAddress(1), port);
      clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
      clientHelper.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
      clientHelper.SetAttribute("Interval", TimeValue(Time(1.0/dataRate.GetBitRate()*packetSize*8)));
      ApplicationContainer clientApps = clientHelper.Install(staNodes.Get(0));
      clientApps.Start(startTime);
      clientApps.Stop(stopTime);

      UdpServerHelper serverHelper(port);
      ApplicationContainer serverApps = serverHelper.Install(apNodes.Get(0));
      serverApps.Start(startTime);
      serverApps.Stop(stopTime);
  } else if (trafficType == "TCP") {
        PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
        ApplicationContainer sinkApps = sinkHelper.Install (apNodes.Get(0));
        sinkApps.Start (startTime);
        sinkApps.Stop (stopTime);

        OnOffHelper onOffHelper ("ns3::TcpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
        onOffHelper.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
        onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
        onOffHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
        onOffHelper.SetAttribute ("DataRate", DataRateValue (dataRate));
        ApplicationContainer onOffApps = onOffHelper.Install (staNodes.Get(0));
        onOffApps.Start (startTime);
        onOffApps.Stop (stopTime);
  } else {
      std::cerr << "Invalid traffic type. Choose UDP or TCP." << std::endl;
      return 1;
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(stopTime + Seconds(1));

  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalGoodput = 0.0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    double goodput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Goodput: " << goodput << " Mbps\n";
    totalGoodput += goodput;
  }
  std::cout << "Total Goodput: " << totalGoodput << " Mbps" << std::endl;

  Simulator::Destroy();
  return 0;
}