#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SpatialReuse80211axExample");

void LogSpatialReuseEvent(std::string context, uint32_t staId, bool obssPdState, double rssi, double threshold)
{
  static std::ofstream outfile("spatial_reuse_events.txt", std::ios_base::app);
  outfile << Simulator::Now().GetSeconds()
          << " STA" << staId
          << " context: " << context
          << ", OBSS-PD enabled: " << obssPdState
          << ", RSSI: " << rssi
          << ", Threshold: " << threshold
          << std::endl;
}

int main(int argc, char *argv[])
{
  double d1 = 5.0;      // Distance STA1 <-> AP1
  double d2 = 5.0;      // Distance STA2 <-> AP2
  double d3 = 15.0;     // AP1 <-> AP2
  double txPower = 16;  // dBm
  double ccaEd = -62;   // CCA-ED threshold dBm
  double obssPd = -72;  // OBSS-PD threshold dBm
  bool enableObssPd = true;
  std::string channelWidth = "20MHz";
  double appInterval = 0.001; // seconds
  uint32_t packetSize = 1200; // bytes
  double simTime = 10.0; // seconds

  CommandLine cmd;
  cmd.AddValue("d1", "Distance STA1 to AP1 (meters)", d1);
  cmd.AddValue("d2", "Distance STA2 to AP2 (meters)", d2);
  cmd.AddValue("d3", "Distance AP1 to AP2 (meters)", d3);
  cmd.AddValue("txPower", "Transmit power in dBm", txPower);
  cmd.AddValue("ccaEd", "CCA-ED threshold in dBm", ccaEd);
  cmd.AddValue("obssPd", "OBSS-PD threshold in dBm", obssPd);
  cmd.AddValue("enableObssPd", "Enable OBSS-PD spatial reuse", enableObssPd);
  cmd.AddValue("channelWidth", "Wi-Fi channel width (20MHz, 40MHz, 80MHz)", channelWidth);
  cmd.AddValue("appInterval", "Traffic generator inter-packet interval (s)", appInterval);
  cmd.AddValue("packetSize", "Application packet size (bytes)", packetSize);
  cmd.AddValue("simTime", "Simulation time (s)", simTime);
  cmd.Parse(argc, argv);

  uint16_t _chWidth = 20;
  if (channelWidth == "40MHz") _chWidth = 40;
  else if (channelWidth == "80MHz") _chWidth = 80;
  else _chWidth = 20;

  // Nodes: 2 APs, 2 STAs
  NodeContainer aps, stas;
  aps.Create(2);
  stas.Create(2);

  // Channel and PHY setup
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ax);
  WifiMacHelper mac;

  // 8 spatial streams per AP is standard for 802.11ax, MCS max for easy demo
  phy.Set("ChannelWidth", UintegerValue(_chWidth));
  phy.Set("TxPowerStart", DoubleValue(txPower));
  phy.Set("TxPowerEnd", DoubleValue(txPower));
  phy.Set("CcaEdThreshold", DoubleValue(ccaEd));

  HtConfiguration ht;
  HeConfiguration he;
  he.SetChannelWidth(_chWidth);

  // Enable/disable OBSS-PD spatial reuse
  HeConfiguration::SpatialReuseInfo srInfo;
  srInfo.srEnabled = enableObssPd;
  srInfo.obssPdLevel = obssPd;
  he.SetSpatialReuseInfo(srInfo);

  WifiPhyHelper::Set("ShortGuardEnabled", BooleanValue(true));

  // AP1 setup
  Ssid ssid1 = Ssid("BSS-1");
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
  NetDeviceContainer apDev1 = wifi.Install(phy, mac, aps.Get(0));
  he.EnableHeForDevice(apDev1.Get(0));
  // AP2 setup
  Ssid ssid2 = Ssid("BSS-2");
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
  NetDeviceContainer apDev2 = wifi.Install(phy, mac, aps.Get(1));
  he.EnableHeForDevice(apDev2.Get(0));

  // STA1 (to AP1)
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDev1 = wifi.Install(phy, mac, stas.Get(0));
  he.EnableHeForDevice(staDev1.Get(0));
  // STA2 (to AP2)
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDev2 = wifi.Install(phy, mac, stas.Get(1));
  he.EnableHeForDevice(staDev2.Get(0));

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
  // Place AP1 at (0,0), STA1 at (d1,0), AP2 at (d3,0), STA2 at (d3+d2,0)
  posAlloc->Add(Vector(0.0, 0.0, 1.0));           // AP1
  posAlloc->Add(Vector(d3, 0.0, 1.0));            // AP2
  posAlloc->Add(Vector(d1, 0.0, 1.0));            // STA1
  posAlloc->Add(Vector(d3 + d2, 0.0, 1.0));       // STA2
  mobility.SetPositionAllocator(posAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  NodeContainer allNodes;
  allNodes.Add(aps);
  allNodes.Add(stas);
  mobility.Install(allNodes);

  // Internet
  InternetStackHelper stack;
  stack.Install(allNodes);

  Ipv4AddressHelper ip;
  ip.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apIf1 = ip.Assign(apDev1);
  Ipv4InterfaceContainer staIf1 = ip.Assign(staDev1);
  ip.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer apIf2 = ip.Assign(apDev2);
  Ipv4InterfaceContainer staIf2 = ip.Assign(staDev2);

  // Applications: STA1->AP1, STA2->AP2 (UDP traffic)
  uint16_t port1 = 6661, port2 = 6662;

  // AP1 receives from STA1
  UdpServerHelper server1(port1);
  ApplicationContainer apps1 = server1.Install(aps.Get(0));
  apps1.Start(Seconds(0.0));
  apps1.Stop(Seconds(simTime + 1));
  UdpClientHelper client1(apIf1.GetAddress(0), port1);
  client1.SetAttribute("MaxPackets", UintegerValue(0));
  client1.SetAttribute("Interval", TimeValue(Seconds(appInterval)));
  client1.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer cApp1 = client1.Install(stas.Get(0));
  cApp1.Start(Seconds(1.0));
  cApp1.Stop(Seconds(simTime));

  // AP2 receives from STA2
  UdpServerHelper server2(port2);
  ApplicationContainer apps2 = server2.Install(aps.Get(1));
  apps2.Start(Seconds(0.0));
  apps2.Stop(Seconds(simTime + 1));
  UdpClientHelper client2(apIf2.GetAddress(0), port2);
  client2.SetAttribute("MaxPackets", UintegerValue(0));
  client2.SetAttribute("Interval", TimeValue(Seconds(appInterval)));
  client2.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer cApp2 = client2.Install(stas.Get(1));
  cApp2.Start(Seconds(1.0));
  cApp2.Stop(Seconds(simTime));

  // Connect spatial reuse events: For demonstration, monitor CCA busy status at STAs
  // ns-3 does not have direct OBSS-PD signal, so use PHY state change as proxy
  // We'll log when PHY senses medium and whether OBSS-PD is enabled
  Config::Connect("/NodeList/2/DeviceList/0/Phy/State/State",
                  MakeBoundCallback([](std::string ctx, Time start, Time duration, WifiPhy::State oldState, WifiPhy::State newState){
                      LogSpatialReuseEvent(ctx, 1, true, 0.0, 0.0);
                  }, "STA1"));
  Config::Connect("/NodeList/3/DeviceList/0/Phy/State/State",
                  MakeBoundCallback([](std::string ctx, Time start, Time duration, WifiPhy::State oldState, WifiPhy::State newState){
                      LogSpatialReuseEvent(ctx, 2, true, 0.0, 0.0);
                  }, "STA2"));
  // In a real spatial reuse setup, one would use WifiPhy or Ocb callbacks/data to log OBSS-PD actual usage.

  // Flow monitor for throughput reporting
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simTime + 1));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  double rxBytesBss1 = 0, rxBytesBss2 = 0;
  for (auto& flow: stats)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
    // Check which BSS: compare destination IP address
    if (t.destinationAddress == apIf1.GetAddress(0))
      rxBytesBss1 += flow.second.rxBytes;
    else if (t.destinationAddress == apIf2.GetAddress(0))
      rxBytesBss2 += flow.second.rxBytes;
  }

  double throughput1 = rxBytesBss1 * 8.0 / simTime / 1e6; // Mbps
  double throughput2 = rxBytesBss2 * 8.0 / simTime / 1e6; // Mbps

  std::cout << "OBSS-PD enabled: " << (enableObssPd ? "true" : "false") << std::endl;
  std::cout << "Throughput BSS1 (AP1): " << throughput1 << " Mbps" << std::endl;
  std::cout << "Throughput BSS2 (AP2): " << throughput2 << " Mbps" << std::endl;

  Simulator::Destroy();
  return 0;
}