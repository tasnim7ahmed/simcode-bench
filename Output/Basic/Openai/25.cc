#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAxVariedMcs");

class WifiAxSim
{
public:
  WifiAxSim();
  void Configure(int argc, char **argv);
  void Run();
  void Report();

private:
  void SetWifiParams(uint8_t mcs, uint32_t channelWidth, WifiPhyStandard standard, bool shortGuard, bool enableRtsCts, bool enableBlockAck, bool enableUlOfdma, bool enable8080, std::string ackSequence, std::string phyMode, bool useSpectrumPhy, uint32_t band);
  void InstallApplications(bool udp, uint32_t payloadSize, double offeredThroughput, bool dl, uint16_t port);

  uint32_t nStations;
  double   distance;
  uint32_t mcs;
  uint32_t phyBand; // 2: 2.4GHz, 5: 5GHz, 6: 6GHz
  std::string channelWidthStr;
  std::vector<uint32_t> channelWidthVec;
  bool    shortGuard;
  bool    enableRtsCts;
  bool    enableBlockAck;
  bool    enableUlOfdma;
  bool    enableUdp;
  bool    enable8080;
  std::string ackSequence;
  std::string phyModel;
  bool    useSpectrumPhy;
  uint32_t  payloadSize;
  double    offeredThroughput;
  double    simTime;
  uint32_t  numMcs;
  NodeContainer wifiStaNodes;
  NodeContainer wifiApNode;
  NetDeviceContainer staDevices;
  NetDeviceContainer apDevice;
  Ipv4InterfaceContainer staInterfaces;
  Ipv4InterfaceContainer apInterface;
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> monitor;
  std::string phyStandardStr;
};

WifiAxSim::WifiAxSim()
{
  // Defaults
  nStations     = 2;
  distance      = 10.0;
  mcs           = 9; // 0..11 for HE
  phyBand       = 5; // 2, 5, or 6
  channelWidthStr = "20";
  channelWidthVec = {20};
  shortGuard    = false;
  enableRtsCts  = false;
  enableBlockAck= true;
  enableUlOfdma = false;
  enableUdp     = true;
  enable8080    = false;
  ackSequence   = "ht";   // ht/he/vht
  phyModel      = "yans";// yans or spectrum
  useSpectrumPhy = false;
  payloadSize   = 1200;
  offeredThroughput = 100e6; // 100 Mbps
  simTime       = 10.0;
  numMcs        = 12;
  phyStandardStr= "802.11ax-5GHz";
}

void
WifiAxSim::Configure(int argc, char **argv)
{
  CommandLine cmd;
  cmd.AddValue("nStations", "Number of Wi-Fi stations", nStations);
  cmd.AddValue("distance", "Distance of each STA from AP (meters)", distance);
  cmd.AddValue("mcs", "Starting MCS value (0..11 for 11ax)", mcs);
  cmd.AddValue("numMcs", "Number of MCS values to sweep", numMcs);
  cmd.AddValue("phyBand", "Wi-Fi band: 2 (2.4GHz), 5 (5GHz), 6 (6GHz)", phyBand);
  cmd.AddValue("channelWidth", "Comma separated channel widths (MHz): 20, 40, 80, 160 or 8080", channelWidthStr);
  cmd.AddValue("guardInterval", "Use Short Guard Interval (SGI)", shortGuard);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
  cmd.AddValue("enableBlockAck", "Enable extended Block Ack", enableBlockAck);
  cmd.AddValue("enableUlOfdma", "Enable UL OFDMA", enableUlOfdma);
  cmd.AddValue("enableUdp", "Use UDP; otherwise use TCP", enableUdp);
  cmd.AddValue("payloadSize", "Packet payload size in bytes", payloadSize);
  cmd.AddValue("offeredThroughput", "Per-flow offered throughput (bps)", offeredThroughput);
  cmd.AddValue("simTime", "Simulation time (s)", simTime);
  cmd.AddValue("phyModel", "Wi-Fi Phy model: yans or spectrum", phyModel);
  cmd.AddValue("enable8080", "Enable 80+80 MHz channels", enable8080);
  cmd.AddValue("ackSequence", "Ack sequence: ht/vht/he", ackSequence);
  cmd.AddValue("phyStandard", "Phy standard: 802.11ax-2.4GHz/5GHz/6GHz", phyStandardStr);
  cmd.Parse(argc, argv);

  // Parse channel widths
  channelWidthVec.clear();
  std::stringstream ss(channelWidthStr);
  std::string item;
  while (getline(ss, item, ',')) {
    uint32_t v = std::stoi(item);
    if (v==8080) { enable8080 = true; channelWidthVec.push_back(160); }
    else channelWidthVec.push_back(v);
  }

  // Phy model
  useSpectrumPhy = (phyModel == "spectrum");

  // Standard
  if (phyStandardStr == "802.11ax-2.4GHz") phyBand = 2;
  else if (phyStandardStr == "802.11ax-6GHz") phyBand = 6;
  else phyBand = 5;
}

void
WifiAxSim::SetWifiParams(uint8_t mcs, uint32_t channelWidth, WifiPhyStandard standard, bool shortGuard, bool enableRtsCts, bool enableBlockAck, bool enableUlOfdma, bool enable8080, std::string ackSequence, std::string phyMode, bool useSpectrumPhy, uint32_t band)
{
  WifiHelper wifi;
  if (useSpectrumPhy)
    wifi.SetStandard(standard);
  else
    wifi.SetStandard(standard);

  YansWifiPhyHelper phy;
  SpectrumWifiPhyHelper spectrumPhy;
  Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel>();

  if (useSpectrumPhy)
    spectrumPhy = SpectrumWifiPhyHelper::Default();
  else
    phy = YansWifiPhyHelper::Default();

  if (useSpectrumPhy)
    spectrumPhy.SetChannel(CreateObject<YansWifiChannel>());
  else
    phy.SetChannel(channel);

  wifi.SetRemoteStationManager("ns3::HeWifiManager",
                              "MaxSupportedTxSpatialStreams", UintegerValue(1),
                              "MaxSupportedRxSpatialStreams", UintegerValue(1),
                              "RtsCtsThreshold", UintegerValue(enableRtsCts? 0 : 9999),
                              "SupportedMcsSet", StringValue(""));

  // Use HeWifiMac and HeWifiPhy
  WifiMacHelper mac;
  Ssid ssid = Ssid("wifi-ax-ssid");

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false),
              "QosSupported", BooleanValue(true));

  if (enableUlOfdma) {
    // UL OFDMA settings can be controlled in the HeWifiMac parameters.
    mac.Set("OFDMA", BooleanValue(true));
  }

  NetDeviceContainer staDevs, apDevs;

  if (useSpectrumPhy)
  {
    spectrumPhy.Set("ChannelWidth", UintegerValue(channelWidth));
    spectrumPhy.Set("GuardInterval", TimeValue(shortGuard ? NanoSeconds(800) : NanoSeconds(1600)));
    spectrumPhy.Set("Frequency", UintegerValue(band==2? 2412 : band==6? 5955 : 5180));
    staDevs = wifi.Install(spectrumPhy, mac, wifiStaNodes);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid),
                "EnableBeaconJitter", BooleanValue(false),
                "QosSupported", BooleanValue(true));
    apDevs = wifi.Install(spectrumPhy, mac, wifiApNode);
  }
  else
  {
    phy.Set("ChannelWidth", UintegerValue(channelWidth));
    phy.Set("ShortGuardEnabled", BooleanValue(shortGuard));
    phy.Set("Frequency", UintegerValue(band==2? 2412 : band==6? 5955 : 5180));
    staDevs = wifi.Install(phy, mac, wifiStaNodes);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid),
                "EnableBeaconJitter", BooleanValue(false),
                "QosSupported", BooleanValue(true));
    apDevs = wifi.Install(phy, mac, wifiApNode);
  }

  staDevices = staDevs;
  apDevice   = apDevs;
}

void
WifiAxSim::InstallApplications(bool udp, uint32_t payloadSize, double offeredThroughput, bool dl, uint16_t port)
{
  ApplicationContainer serverApps, clientApps;
  if (udp)
  {
    UdpServerHelper server(port);
    for (uint32_t i=0; i< (dl ? nStations : 1); ++i)
      serverApps.Add(server.Install(dl? wifiStaNodes.Get(i): wifiApNode.Get(0)));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime-1.0));

    UdpClientHelper client(dl? staInterfaces.GetAddress(i) : apInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("Interval", TimeValue(Seconds(double(payloadSize*8) / offeredThroughput)));
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));

    for (uint32_t i=0; i< (dl ? 1 : nStations); ++i) {
      auto target = dl ? apInterface.GetAddress(0) : staInterfaces.GetAddress(i);
      UdpClientHelper client(target, port);
      client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
      client.SetAttribute("Interval", TimeValue(Seconds(double(payloadSize*8) / offeredThroughput)));
      client.SetAttribute("PacketSize", UintegerValue(payloadSize));
      clientApps.Add(client.Install(dl ? wifiApNode.Get(0) : wifiStaNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime-1.0));
  }
  else
  {
    uint16_t tcpPort = port;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);

    for (uint32_t i=0; i< (dl ? nStations : 1); ++i)
      serverApps.Add(sinkHelper.Install(dl? wifiStaNodes.Get(i): wifiApNode.Get(0)));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime-1.0));

    for (uint32_t i=0; i< (dl ? 1 : nStations); ++i) {
      auto target = dl ? staInterfaces.GetAddress(i) : apInterface.GetAddress(0);
      OnOffHelper onoff("ns3::TcpSocketFactory", Address(InetSocketAddress(target, tcpPort)));
      onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
      onoff.SetAttribute("DataRate", DataRateValue(DataRate(uint64_t(offeredThroughput))));
      onoff.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
      onoff.SetAttribute("StopTime", TimeValue(Seconds(simTime-1.0)));
      clientApps.Add(onoff.Install(dl ? wifiApNode.Get(0) : wifiStaNodes.Get(i)));
    }
  }
}

void
WifiAxSim::Run()
{
  wifiStaNodes.Create(nStations);
  wifiApNode.Create(1);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 1.5));
  for (uint32_t i=0; i < nStations; ++i)
    positionAlloc->Add(Vector(distance * (i+1), 0.0, 1.5));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);
  mobility.Install(wifiStaNodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  NetDeviceContainer allDevices;
  for (auto chWidth : channelWidthVec)
  {
    for (uint8_t testMcs = mcs; testMcs < mcs + numMcs; ++testMcs)
    {
      allDevices = NodeContainer();
      SetWifiParams(testMcs, chWidth, phyBand==2? WIFI_STANDARD_80211ax_2_4GHZ : phyBand==6? WIFI_STANDARD_80211ax_6GHZ : WIFI_STANDARD_80211ax_5GHZ, shortGuard, enableRtsCts, enableBlockAck, enableUlOfdma, enable8080, ackSequence, phyModel, useSpectrumPhy, phyBand);

      Ipv4InterfaceContainer ifSta, ifAp;
      ifSta = address.Assign(staDevices);
      address.NewNetwork();
      ifAp  = address.Assign(apDevice);

      staInterfaces = ifSta;
      apInterface   = ifAp;

      uint16_t port = 50000+chWidth+testMcs;
      InstallApplications(enableUdp, payloadSize, offeredThroughput, true, port);

      Ipv4GlobalRoutingHelper::PopulateRoutingTables();

      monitor = flowmonHelper.InstallAll();

      Simulator::Stop(Seconds(simTime));
      Simulator::Run();
      Simulator::Destroy();

      Report();
    }
  }
}

void
WifiAxSim::Report()
{
  monitor->CheckForLostPackets();
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (const auto &flow : stats) {
    auto st = flow.second;
    double throughput = st.rxBytes * 8.0 / (st.timeLastRxPacket.GetSeconds() - st.timeFirstTxPacket.GetSeconds()) / 1000000.0;
    std::cout << "FlowID: " << flow.first
      << " RX: " << st.rxBytes << " bytes, Throughput: "
      << throughput << " Mbps, Lost: " << st.lostPackets << std::endl;
  }
}

int main(int argc, char *argv[])
{
  WifiAxSim sim;
  sim.Configure(argc, argv);
  sim.Run();
  return 0;
}