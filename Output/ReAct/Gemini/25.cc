#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/channel-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/tcp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi80211axSimulation");

int main(int argc, char *argv[]) {
  bool verbose = false;
  uint32_t nWifi = 1;
  double distance = 20.0;
  std::string mcs = "MCS11";
  std::string channelWidth = "HT20";
  bool enableRtsCts = false;
  bool enable80plus80 = false;
  bool enableExtBa = false;
  bool enableUlOfdma = false;
  std::string guardInterval = "Long";
  std::string frequencyBand = "5GHz";
  bool useSpectrumPhy = false;
  uint32_t packetSize = 1472;
  std::string transportProtocol = "UDP";
  double simulationTime = 10.0;
  double targetThroughputMbps = 10.0;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if possible", verbose);
  cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue("distance", "Distance between AP and STA devices", distance);
  cmd.AddValue("mcs", "Modulation and Coding Scheme", mcs);
  cmd.AddValue("channelWidth", "Channel width (HT20, HT40, HT80, HT160)", channelWidth);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
  cmd.AddValue("enable80plus80", "Enable 80+80 MHz channels", enable80plus80);
  cmd.AddValue("enableExtBa", "Enable extended block ack", enableExtBa);
  cmd.AddValue("enableUlOfdma", "Enable UL OFDMA", enableUlOfdma);
  cmd.AddValue("guardInterval", "Guard interval (Long, Short)", guardInterval);
  cmd.AddValue("frequencyBand", "Frequency band (2.4GHz, 5GHz, 6GHz)", frequencyBand);
  cmd.AddValue("useSpectrumPhy", "Use Spectrum Phy", useSpectrumPhy);
  cmd.AddValue("packetSize", "Payload size of UDP packets", packetSize);
  cmd.AddValue("transportProtocol", "Transport protocol (UDP, TCP)", transportProtocol);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("targetThroughputMbps", "Target throughput in Mbps", targetThroughputMbps);

  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("Wifi80211axSimulation", LOG_LEVEL_ALL);
  }

  NodeContainer apNode;
  apNode.Create(1);

  NodeContainer staNodes;
  staNodes.Create(nWifi);

  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211ax);

  YansWifiChannelHelper channelHelper;
  YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();

  if (useSpectrumPhy) {
      phyHelper = SpectrumWifiPhyHelper::Default();
      Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel>();
      Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
      channel->SetPropagationDelayModel(delayModel);
      Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel>();
      channel->SetPropagationLossModel(lossModel);
      channelHelper.SetChannel(channel);
  } else {
      channelHelper = YansWifiChannelHelper::Default();
      channelHelper.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
      channelHelper.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
  }

  phyHelper.SetChannel(channelHelper.Create());

  WifiMacHelper macHelper;
  Ssid ssid = Ssid("ns3-80211ax");

  Wifi80211axHelper wifi80211axHelper;

  NetDeviceContainer staDevices;
  NetDeviceContainer apDevices;

  macHelper.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

  staDevices = wifiHelper.Install(phyHelper, macHelper, staNodes);

  macHelper.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconGeneration", BooleanValue(true));

  apDevices = wifiHelper.Install(phyHelper, macHelper, apNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(nWifi),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

  mobility.Install(staNodes);

  mobility.SetPositionAllocator("ns3::ConstantPositionAllocator",
                                "X", DoubleValue(0.0),
                                "Y", DoubleValue(0.0),
                                "Z", DoubleValue(0.0));

  mobility.Install(apNode);

  InternetStackHelper stack;
  stack.Install(apNode);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer staNodeInterface;
  Ipv4InterfaceContainer apNodeInterface;
  staNodeInterface = address.Assign(staDevices);
  apNodeInterface = address.Assign(apDevices);
  Ipv4InterfaceContainer allInterfaces = apNodeInterface;
  allInterfaces.Add(staNodeInterface);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  ApplicationContainer clientApps;
  ApplicationContainer serverApps;

  if (transportProtocol == "UDP") {
    UdpServerHelper echoServer(port);
    serverApps.Add(echoServer.Install(apNode.Get(0)));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    UdpClientHelper echoClient(apNodeInterface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.00001)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    for (uint32_t i = 0; i < nWifi; ++i) {
      clientApps.Add(echoClient.Install(staNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

  } else if (transportProtocol == "TCP") {

    TcpServerHelper echoServer(port);
    serverApps = echoServer.Install(apNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    TcpClientHelper echoClient(apNodeInterface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.00001)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    for (uint32_t i = 0; i < nWifi; ++i) {
      clientApps.Add(echoClient.Install(staNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));
  }

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(simulationTime + 1));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalThroughput = 0.0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    if (t.destinationAddress == apNodeInterface.GetAddress(0))
     {
        totalThroughput += i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
    }

  }

  std::cout << "Total throughput: " << totalThroughput << " Mbps" << std::endl;

  Simulator::Destroy();
  return 0;
}