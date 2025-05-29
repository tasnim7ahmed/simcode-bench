#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTimingParametersExample");

double g_rxBytes = 0.0;

void RxPacket(Ptr<const Packet> packet, const Address &from)
{
  g_rxBytes += packet->GetSize();
}

int main(int argc, char *argv[])
{
  double slotTime = 9.0;      // microseconds, default for 802.11n 2.4GHz
  double sifs = 10.0;         // microseconds
  double pifs = 25.0;         // microseconds

  CommandLine cmd;
  cmd.AddValue("slotTime", "Wi-Fi slot time in microseconds", slotTime);
  cmd.AddValue("sifs", "Short InterFrame Space (SIFS) in microseconds", sifs);
  cmd.AddValue("pifs", "Point Coordination InterFrame Space (PIFS) in microseconds", pifs);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNode, wifiApNode;
  wifiStaNode.Create(1);
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

  WifiMacHelper mac;

  Ssid ssid = Ssid("ns3-wifi");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

  // Set timing parameters
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Slot", TimeValue(MicroSeconds(slotTime)));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Sifs", TimeValue(MicroSeconds(sifs)));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Pifs", TimeValue(MicroSeconds(pifs)));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(0.0),
                               "MinY", DoubleValue(0.0),
                               "DeltaX", DoubleValue(5.0),
                               "DeltaY", DoubleValue(0.0),
                               "GridWidth", UintegerValue(2),
                               "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNode);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNode);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staIf = address.Assign(staDevice);
  Ipv4InterfaceContainer apIf = address.Assign(apDevice);

  // UDP server on STA
  uint16_t port = 4000;
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(wifiStaNode.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // UDP client on AP
  uint32_t packetSize = 1024;
  uint32_t nPacketsPerSecond = 800;
  UdpClientHelper client(staIf.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0 / nPacketsPerSecond)));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApp = client.Install(wifiApNode.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  // Throughput calculation
  Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback(&RxPacket));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  double throughput = (g_rxBytes * 8.0) / (8.0 - 2.0) / 1000000.0; // Mbit/s over 8-2=6s (active time)
  std::cout << std::fixed << std::setprecision(4)
            << "Throughput: " << throughput << " Mbit/s" << std::endl;
  Simulator::Destroy();
  return 0;
}