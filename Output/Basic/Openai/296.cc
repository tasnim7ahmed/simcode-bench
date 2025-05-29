#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
  // Set simulation time
  double simulationTime = 10.0;

  // Create nodes: 1 AP, 3 STAs
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(3);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  // Wifi channel and physical layer
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  // Wifi MAC and helpers
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
  wifi.SetRemoteStationManager("ns3::AarfWifiManager");
  WifiMacHelper mac;

  Ssid ssid = Ssid("ns3-wifi-ssid");

  // STAs
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  // AP
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

  // Mobility for STAs: RandomWalk2d
  MobilityHelper mobilitySta;
  mobilitySta.SetPositionAllocator("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue(0.0),
                                   "MinY", DoubleValue(0.0),
                                   "DeltaX", DoubleValue(5.0),
                                   "DeltaY", DoubleValue(5.0),
                                   "GridWidth", UintegerValue(3),
                                   "LayoutType", StringValue("RowFirst"));
  mobilitySta.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                               "Distance", DoubleValue(5.0));
  mobilitySta.Install(wifiStaNodes);

  // Mobility for AP: ConstantPosition
  MobilityHelper mobilityAp;
  mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityAp.Install(wifiApNode);

  // Internet
  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

  // UDP server on AP
  uint16_t port = 50000;
  UdpServerHelper udpServer(port);
  ApplicationContainer serverApps = udpServer.Install(wifiApNode.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simulationTime));

  // UDP client on one STA (the first)
  UdpClientHelper udpClient(apInterface.GetAddress(0), port);
  udpClient.SetAttribute("MaxPackets", UintegerValue(3200));
  udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(3)));
  udpClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = udpClient.Install(wifiStaNodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(simulationTime));

  // Run simulation
  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}