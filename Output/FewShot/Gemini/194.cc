#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiExample");

int main(int argc, char *argv[]) {
  bool verbose = false;
  bool tracing = false;
  uint32_t nWifi = 3;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer apNodes;
  apNodes.Create(2);
  NodeContainer staNodes;
  staNodes.Create(nWifi);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid1 = Ssid("ns-3-ssid-1");
  Ssid ssid2 = Ssid("ns-3-ssid-2");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid1),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevs1 = wifi.Install(phy, mac, staNodes.Get(0));
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid2),
              "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevs2 = wifi.Install(phy, mac, staNodes.Get(1));
    NetDeviceContainer staDevs3 = wifi.Install(phy, mac, staNodes.Get(2));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid1),
              "BeaconInterval", TimeValue(MilliSeconds(100)),
              "QosSupported", BooleanValue(true));

  NetDeviceContainer apDevs1 = wifi.Install(phy, mac, apNodes.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid2),
              "BeaconInterval", TimeValue(MilliSeconds(100)),
              "QosSupported", BooleanValue(true));

  NetDeviceContainer apDevs2 = wifi.Install(phy, mac, apNodes.Get(1));

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

  mobility.Install(apNodes);
  mobility.Install(staNodes);

  InternetStackHelper stack;
  stack.Install(apNodes);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevs1);
  Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevs1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevs2);
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevs2);
    Ipv4InterfaceContainer staInterfaces3 = address.Assign(staDevs3);

  UdpServerHelper server1(9);
  ApplicationContainer serverApps1 = server1.Install(apNodes.Get(0));
  serverApps1.Start(Seconds(1.0));
  serverApps1.Stop(Seconds(simulationTime));

  UdpServerHelper server2(9);
  ApplicationContainer serverApps2 = server2.Install(apNodes.Get(1));
  serverApps2.Start(Seconds(1.0));
  serverApps2.Stop(Seconds(simulationTime));

  UdpClientHelper client1(apInterfaces1.GetAddress(0), 9);
  client1.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client1.SetAttribute("Interval", TimeValue(Time("0.00001")));
  client1.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps1 = client1.Install(staNodes.Get(0));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(simulationTime));

  UdpClientHelper client2(apInterfaces2.GetAddress(0), 9);
    client2.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client2.SetAttribute("Interval", TimeValue(Time("0.00001")));
    client2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = client2.Install(staNodes.Get(1));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(simulationTime));

    UdpClientHelper client3(apInterfaces2.GetAddress(0), 9);
    client3.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client3.SetAttribute("Interval", TimeValue(Time("0.00001")));
    client3.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps3 = client3.Install(staNodes.Get(2));
    clientApps3.Start(Seconds(2.0));
    clientApps3.Stop(Seconds(simulationTime));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  if (tracing) {
      phy.EnablePcapAll("wifi-example");
      AsciiTraceHelper ascii;
      phy.EnableAsciiAll(ascii.CreateFileStream("wifi-example.tr"));
  }

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  uint64_t totalBytesSentSta1 = DynamicCast<UdpClient>(clientApps1.Get(0)->GetObject<Application>())->m_totalBytes;
  uint64_t totalBytesSentSta2 = DynamicCast<UdpClient>(clientApps2.Get(0)->GetObject<Application>())->m_totalBytes;
  uint64_t totalBytesSentSta3 = DynamicCast<UdpClient>(clientApps3.Get(0)->GetObject<Application>())->m_totalBytes;

  std::cout << "Total Bytes Sent by STA 1: " << totalBytesSentSta1 << std::endl;
  std::cout << "Total Bytes Sent by STA 2: " << totalBytesSentSta2 << std::endl;
  std::cout << "Total Bytes Sent by STA 3: " << totalBytesSentSta3 << std::endl;

  Simulator::Destroy();
  return 0;
}