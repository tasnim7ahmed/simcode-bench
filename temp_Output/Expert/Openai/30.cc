#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi80211nTosSimulation");

double
CalculateThroughput(Ptr<FlowMonitor> flowMonitor)
{
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
  uint64_t totalRxBytes = 0;
  double timeLastRx = 0.0, timeFirstTx = 0.0;

  for (auto const &it : stats)
    {
      totalRxBytes += it.second.rxBytes;
      if (it.second.timeLastRxPacket.GetSeconds() > timeLastRx)
        timeLastRx = it.second.timeLastRxPacket.GetSeconds();
      if (it.second.timeFirstTxPacket.GetSeconds() < timeFirstTx || timeFirstTx == 0.0)
        timeFirstTx = it.second.timeFirstTxPacket.GetSeconds();
    }
  if (timeLastRx <= timeFirstTx)
    return 0.0;
  double throughput = (totalRxBytes * 8.0) / (timeLastRx - timeFirstTx); // bits/sec
  return throughput / 1e6; // Mbps
}

int 
main(int argc, char *argv[])
{
  uint32_t nStations = 3;
  uint32_t mcs = 0;
  uint32_t channelWidth = 20;
  bool shortGuardInterval = false;
  double distance = 10.0;
  bool enableRtsCts = false;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue("nStations", "Number of Wi-Fi stations", nStations);
  cmd.AddValue("mcs", "HT MCS (0-7)", mcs);
  cmd.AddValue("channelWidth", "Channel width (20 or 40 MHz)", channelWidth);
  cmd.AddValue("shortGuardInterval", "Enable short guard interval", shortGuardInterval);
  cmd.AddValue("distance", "Distance (meters) between AP and any STA", distance);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
  cmd.Parse(argc, argv);

  if (mcs > 7) mcs = 7;
  if (channelWidth != 20 && channelWidth != 40) channelWidth = 20;

  NodeContainer wifiStaNodes, wifiApNode;
  wifiStaNodes.Create(nStations);
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());
  phy.Set("ChannelWidth", UintegerValue(channelWidth));
  phy.Set("ShortGuardEnabled", BooleanValue(shortGuardInterval));

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                              "DataMode", StringValue("HtMcs" + std::to_string(mcs)),
                              "ControlMode", StringValue("HtMcs0"),
                              "RtsCtsThreshold", UintegerValue(enableRtsCts ? 0 : 2347));

  HtWifiMacHelper mac = HtWifiMacHelper::Default();

  Ssid ssid = Ssid("wifi-80211n-tos");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevs = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevs = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 1.0));
  for (uint32_t i = 0; i < nStations; ++i)
    {
      positionAlloc->Add(Vector(distance, i * 2.0, 1.0));
    }
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);
  mobility.Install(wifiStaNodes);

  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevs);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevs);

  uint16_t port = 5000;
  ApplicationContainer serverApps, clientApps;
  std::vector<uint8_t> tosValues = {0x00, 0x20, 0x40, 0x80}; // default, DSCP AF21, AF41, CS1

  uint32_t nTos = std::min((uint32_t)tosValues.size(), nStations);

  for (uint32_t i = 0; i < nStations; ++i)
    {
      UdpServerHelper server(port + i);
      serverApps.Add(server.Install(wifiStaNodes.Get(i)));

      UdpClientHelper client(staInterfaces.GetAddress(i), port + i);
      client.SetAttribute("MaxPackets", UintegerValue(0));
      client.SetAttribute("Interval", TimeValue(MilliSeconds(0.5)));
      client.SetAttribute("PacketSize", UintegerValue(1472));
      client.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
      client.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));
      client.SetAttribute("Tos", UintegerValue(tosValues[i % nTos]));
      clientApps.Add(client.Install(wifiApNode.Get(0)));
    }

  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simulationTime + 1.0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(simulationTime));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime + 1.0));
  Simulator::Run();

  double throughput = CalculateThroughput(monitor);
  std::cout << "Aggregated UDP throughput: " << throughput << " Mbps" << std::endl;

  Simulator::Destroy();

  return 0;
}