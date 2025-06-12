#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DualApWifiSimulation");

uint64_t totalBytesSent[8] = {0}; // Assuming max 8 stations; adjust as needed

void
TxCallback(Ptr<const Packet> packet, const Address &)
{
  uint32_t nodeId = Simulator::GetContext();
  if (nodeId < 8) // adjust if more stations
    totalBytesSent[nodeId] += packet->GetSize();
}

int
main(int argc, char *argv[])
{
  uint32_t numStaPerAp = 3;
  uint32_t packetSize = 1024;
  DataRate dataRate("2Mbps");
  double simulationTime = 10.0;
  
  CommandLine cmd;
  cmd.AddValue("numStaPerAp", "Number of stations per AP", numStaPerAp);
  cmd.Parse(argc, argv);

  uint32_t totalStas = 2 * numStaPerAp;
  NodeContainer wifiStaNodes1, wifiStaNodes2, apNodes;
  wifiStaNodes1.Create(numStaPerAp); // Group 1
  wifiStaNodes2.Create(numStaPerAp); // Group 2
  apNodes.Create(2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);
  WifiMacHelper mac;
  Ssid ssid1 = Ssid("ssid-ap1");
  Ssid ssid2 = Ssid("ssid-ap2");

  NetDeviceContainer staDevices1, staDevices2, apDevices;

  // Group 1
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid1),
              "ActiveProbing", BooleanValue(false));
  staDevices1 = wifi.Install(phy, mac, wifiStaNodes1);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid1));
  apDevices.Add(wifi.Install(phy, mac, apNodes.Get(0)));

  // Group 2
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid2),
              "ActiveProbing", BooleanValue(false));
  staDevices2 = wifi.Install(phy, mac, wifiStaNodes2);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid2));
  apDevices.Add(wifi.Install(phy, mac, apNodes.Get(1)));

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  positionAlloc->Add(Vector(20.0, 0.0, 0.0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  Ptr<ListPositionAllocator> staPosAlloc1 = CreateObject<ListPositionAllocator>();
  for (uint32_t i = 0; i < numStaPerAp; ++i)
    staPosAlloc1->Add(Vector(0.0 + 2.0*i, 10.0, 0.0));
  mobility.SetPositionAllocator(staPosAlloc1);
  mobility.Install(wifiStaNodes1);

  Ptr<ListPositionAllocator> staPosAlloc2 = CreateObject<ListPositionAllocator>();
  for (uint32_t i = 0; i < numStaPerAp; ++i)
    staPosAlloc2->Add(Vector(20.0 + 2.0*i, 10.0, 0.0));
  mobility.SetPositionAllocator(staPosAlloc2);
  mobility.Install(wifiStaNodes2);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(wifiStaNodes1);
  stack.Install(wifiStaNodes2);
  stack.Install(apNodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer staIfs1, staIfs2, apIfs;

  address.SetBase("10.1.1.0", "255.255.255.0");
  staIfs1 = address.Assign(staDevices1);
  apIfs.Add(address.Assign(apDevices.Get(0)));

  address.SetBase("10.1.2.0", "255.255.255.0");
  staIfs2 = address.Assign(staDevices2);
  apIfs.Add(address.Assign(apDevices.Get(1)));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // UDP Server (at APs)
  uint16_t port = 9;
  UdpServerHelper serverHelper(port);
  ApplicationContainer serverApps;
  serverApps.Add(serverHelper.Install(apNodes.Get(0)));
  serverApps.Add(serverHelper.Install(apNodes.Get(1)));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simulationTime + 1));

  // UDP Clients (at STAs -> to their AP's IP)
  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < numStaPerAp; ++i)
  {
    UdpClientHelper client1(apIfs.GetAddress(0), port);
    client1.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client1.SetAttribute("Interval", TimeValue(MicroSeconds(uint32_t((double)packetSize*8*1e6/dataRate.GetBitRate()))));
    client1.SetAttribute("PacketSize", UintegerValue(packetSize));

    auto clientApp = client1.Install(wifiStaNodes1.Get(i));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime));
    clientApps.Add(clientApp);

    UdpClientHelper client2(apIfs.GetAddress(1), port);
    client2.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client2.SetAttribute("Interval", TimeValue(MicroSeconds(uint32_t((double)packetSize*8*1e6/dataRate.GetBitRate()))));
    client2.SetAttribute("PacketSize", UintegerValue(packetSize));

    clientApp = client2.Install(wifiStaNodes2.Get(i));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime));
    clientApps.Add(clientApp);
  }

  // Tracing
  phy.EnablePcapAll("wifi-dual-ap", false);
  AsciiTraceHelper ascii;
  phy.EnableAsciiAll(ascii.CreateFileStream("wifi-dual-ap.tr"));

  // Per-station packet TX tracing (at application layer)
  for (uint32_t i = 0; i < numStaPerAp; ++i)
  {
    Ptr<Application> app = clientApps.Get(i); // Group 1
    app->TraceConnectWithoutContext("Tx", MakeCallback(&TxCallback));
    app = clientApps.Get(i + numStaPerAp);    // Group 2
    app->TraceConnectWithoutContext("Tx", MakeCallback(&TxCallback));
  }

  Simulator::Stop(Seconds(simulationTime+1));
  Simulator::Run();

  // Report statistics
  std::cout << "Total Data Sent by Each Station (bytes):" << std::endl;
  for (uint32_t i = 0; i < numStaPerAp; ++i)
  {
    std::cout << "STA" << i << " (AP1): " << totalBytesSent[i] << std::endl;
    std::cout << "STA" << (i + numStaPerAp) << " (AP2): " << totalBytesSent[i + numStaPerAp] << std::endl;
  }

  Simulator::Destroy();
  return 0;
}