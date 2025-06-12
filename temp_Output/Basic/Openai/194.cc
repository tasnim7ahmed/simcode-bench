#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DualApWifiSim");

struct StaStats
{
  uint32_t sent;
};

void TxTrace(Ptr<StaStats> stats, Ptr<const Packet> packet)
{
  stats->sent += packet->GetSize();
}

int main(int argc, char *argv[])
{
  uint32_t nStasPerAp = 3;
  double simTime = 10.0;
  std::string dataRate = "5Mbps";
  uint16_t udpPort = 4000;
  uint32_t packetSize = 1024;

  CommandLine cmd;
  cmd.AddValue("nStasPerAp", "Number of stations per AP", nStasPerAp);
  cmd.AddValue("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue("dataRate", "Application data rate", dataRate);
  cmd.AddValue("packetSize", "UDP packet size", packetSize);
  cmd.Parse(argc, argv);

  // Create nodes: 2 APs and stations
  NodeContainer apNodes, staNodesA, staNodesB;
  apNodes.Create(2);
  staNodesA.Create(nStasPerAp);
  staNodesB.Create(nStasPerAp);

  // WiFi PHY and MAC
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);

  WifiMacHelper mac;
  Ssid ssid1 = Ssid("wifi-ap-1");
  Ssid ssid2 = Ssid("wifi-ap-2");

  NetDeviceContainer apDevices, staDevicesA, staDevicesB;

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid1),
              "ActiveProbing", BooleanValue(false));
  staDevicesA = wifi.Install(phy, mac, staNodesA);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid2),
              "ActiveProbing", BooleanValue(false));
  staDevicesB = wifi.Install(phy, mac, staNodesB);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid1));
  apDevices.Add(wifi.Install(phy, mac, apNodes.Get(0)));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid2));
  apDevices.Add(wifi.Install(phy, mac, apNodes.Get(1)));

  // Mobility
  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                             "MinX", DoubleValue(0.0),
                             "MinY", DoubleValue(0.0),
                             "DeltaX", DoubleValue(5.0),
                             "DeltaY", DoubleValue(5.0),
                             "GridWidth", UintegerValue(nStasPerAp),
                             "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodesA);
  Ptr<MobilityModel> apMobA = CreateObject<ConstantPositionMobilityModel>();
  apNodes.Get(0)->AggregateObject(apMobA);
  apMobA->SetPosition(Vector(10.0, -5.0, 0.0));

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                             "MinX", DoubleValue(100.0),
                             "MinY", DoubleValue(0.0),
                             "DeltaX", DoubleValue(5.0),
                             "DeltaY", DoubleValue(5.0),
                             "GridWidth", UintegerValue(nStasPerAp),
                             "LayoutType", StringValue("RowFirst"));
  mobility.Install(staNodesB);
  Ptr<MobilityModel> apMobB = CreateObject<ConstantPositionMobilityModel>();
  apNodes.Get(1)->AggregateObject(apMobB);
  apMobB->SetPosition(Vector(110.0, -5.0, 0.0));

  // Internet stack
  InternetStackHelper stack;
  stack.Install(apNodes);
  stack.Install(staNodesA);
  stack.Install(staNodesB);

  Ipv4AddressHelper ip;
  ip.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staIfsA = ip.Assign(staDevicesA);
  Ipv4InterfaceContainer apIfA = ip.Assign(NetDeviceContainer(apDevices.Get(0)));

  ip.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer staIfsB = ip.Assign(staDevicesB);
  Ipv4InterfaceContainer apIfB = ip.Assign(NetDeviceContainer(apDevices.Get(1)));

  // Applications: UDP Server on each AP, clients on each STA
  uint16_t basePort = udpPort;
  ApplicationContainer serverApps, clientApps;

  // For statistics
  std::vector<Ptr<StaStats>> staStatsVec(2 * nStasPerAp);

  // AP 1
  UdpServerHelper server1(udpPort);
  serverApps.Add(server1.Install(apNodes.Get(0)));

  for (uint32_t i = 0; i < nStasPerAp; ++i)
    {
      UdpClientHelper client(staIfsA.GetAddress(i == 0 ? 0 : 0), udpPort);
      client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
      client.SetAttribute("Interval", TimeValue(Seconds(double(packetSize * 8) / std::stod(dataRate.substr(0, dataRate.find("M"))) / 1e6)));
      client.SetAttribute("PacketSize", UintegerValue(packetSize));
      clientApps.Add(client.Install(staNodesA.Get(i)));

      ptrdiff_t idx = i; // index for stats vector
      staStatsVec[idx] = Create<StaStats>();
      std::ostringstream oss;
      oss << "/NodeList/" << staNodesA.Get(i)->GetId() << "/ApplicationList/0/$ns3::UdpClient/Tx";
      Config::ConnectWithoutContext(oss.str(), MakeBoundCallback(&TxTrace, staStatsVec[idx]));
    }

  // AP 2
  ++udpPort;
  UdpServerHelper server2(udpPort);
  serverApps.Add(server2.Install(apNodes.Get(1)));

  for (uint32_t i = 0; i < nStasPerAp; ++i)
    {
      UdpClientHelper client(apIfB.GetAddress(0), udpPort);
      client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
      client.SetAttribute("Interval", TimeValue(Seconds(double(packetSize * 8) / std::stod(dataRate.substr(0, dataRate.find("M"))) / 1e6)));
      client.SetAttribute("PacketSize", UintegerValue(packetSize));
      clientApps.Add(client.Install(staNodesB.Get(i)));

      ptrdiff_t idx = nStasPerAp + i; // index for stats vector
      staStatsVec[idx] = Create<StaStats>();
      std::ostringstream oss;
      oss << "/NodeList/" << staNodesB.Get(i)->GetId() << "/ApplicationList/0/$ns3::UdpClient/Tx";
      Config::ConnectWithoutContext(oss.str(), MakeBoundCallback(&TxTrace, staStatsVec[idx]));
    }

  serverApps.Start(Seconds(0.0));
  clientApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simTime + 1));
  clientApps.Stop(Seconds(simTime + 1));

  // Tracing
  phy.EnablePcapAll("dual-ap-wifi", true);
  AsciiTraceHelper ascii;
  phy.EnableAsciiAll(ascii.CreateFileStream("dual-ap-wifi.tr"));

  Simulator::Stop(Seconds(simTime + 2));
  Simulator::Run();

  for (uint32_t i = 0; i < nStasPerAp; ++i)
    {
      std::cout << "STA-A" << i << " sent: " << staStatsVec[i]->sent << " bytes" << std::endl;
    }
  for (uint32_t i = 0; i < nStasPerAp; ++i)
    {
      std::cout << "STA-B" << i << " sent: " << staStatsVec[nStasPerAp + i]->sent << " bytes" << std::endl;
    }

  Simulator::Destroy();
  return 0;
}