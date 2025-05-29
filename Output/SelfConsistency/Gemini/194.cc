#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiExample");

int main(int argc, char *argv[]) {
  bool tracing = true;
  uint32_t numStaPerAp = 5;
  std::string phyMode("OfdmRate6Mbps");
  double simulationTime = 10; //seconds

  CommandLine cmd;
  cmd.AddValue("numStaPerAp", "Number of stations per AP", numStaPerAp);
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse(argc, argv);

  NodeContainer apNodes;
  apNodes.Create(2);

  NodeContainer staNodes;
  staNodes.Create(2 * numStaPerAp);

  // Configure Wi-Fi PHY
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  // Add a mac and disable rate control
  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

  // Configure AP
  Ssid ssid1 = Ssid("ns3-wifi-ap1");
  wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
  NetDeviceContainer apDevice1 = wifi.Install(wifiPhy, wifiMac, apNodes.Get(0));

  Ssid ssid2 = Ssid("ns3-wifi-ap2");
  wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
  NetDeviceContainer apDevice2 = wifi.Install(wifiPhy, wifiMac, apNodes.Get(1));


  // Configure STA for AP1
  wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice1 = wifi.Install(wifiPhy, wifiMac, staNodes.Get(0), staNodes.Get(numStaPerAp - 1));

  // Configure STA for AP2
  wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice2 = wifi.Install(wifiPhy, wifiMac, staNodes.Get(numStaPerAp), staNodes.Get(2 * numStaPerAp - 1));


  // Install Mobility Model
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(numStaPerAp),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodes);

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(20.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(1),
                                "LayoutType", StringValue("RowFirst"));
  mobility.Install(apNodes);

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install(apNodes);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface1 = address.Assign(apDevice1);
  Ipv4InterfaceContainer staInterface1 = address.Assign(staDevice1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface2 = address.Assign(apDevice2);
  Ipv4InterfaceContainer staInterface2 = address.Assign(staDevice2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create Applications
  uint16_t port = 9; // Discard port (RFC 863)

  // Create UDP server on AP1 and AP2
  UdpServerHelper server1(port);
  ApplicationContainer serverApps1 = server1.Install(apNodes.Get(0));
  serverApps1.Start(Seconds(1.0));
  serverApps1.Stop(Seconds(simulationTime + 1));

  UdpServerHelper server2(port);
  ApplicationContainer serverApps2 = server2.Install(apNodes.Get(1));
  serverApps2.Start(Seconds(1.0));
  serverApps2.Stop(Seconds(simulationTime + 1));


  // Create UDP client on all STAs
  UdpClientHelper client1(apInterface1.GetAddress(0), port);
  client1.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client1.SetAttribute("Interval", TimeValue(Time("0.00001"))); // packets/sec
  client1.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps1 = client1.Install(staNodes.Get(0), staNodes.Get(numStaPerAp - 1));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(simulationTime));

  UdpClientHelper client2(apInterface2.GetAddress(0), port);
  client2.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client2.SetAttribute("Interval", TimeValue(Time("0.00001"))); // packets/sec
  client2.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps2 = client2.Install(staNodes.Get(numStaPerAp), staNodes.Get(2 * numStaPerAp - 1));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(simulationTime));


  // Tracing
  if (tracing) {
    wifiPhy.EnablePcapAll("wifi-example");
    AsciiTraceHelper ascii;
    wifiPhy.EnableAsciiAll(ascii.CreateFileStream("wifi-example.tr"));
  }

  Simulator::Stop(Seconds(simulationTime + 2));

  Simulator::Run();

  uint64_t totalBytesSent[2 * numStaPerAp];
  for (uint32_t i = 0; i < 2 * numStaPerAp; ++i) {
    totalBytesSent[i] = DynamicCast<UdpClient>(clientApps1.Get(i))->GetTotalBytesSent();
    std::cout << "STA " << i << " Total Bytes Sent: " << totalBytesSent[i] << std::endl;
  }

  Simulator::Destroy();

  return 0;
}