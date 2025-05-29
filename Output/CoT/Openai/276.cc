#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSimpleUDPGridExample");

int main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t nSta = 3;
  double simulationTime = 10.0;
  uint16_t serverPort = 9;

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(nSta);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-80211n-5ghz");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;

  // STA grid: row of 3 nodes, distance 5 meters apart, start at (0,0,0)
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);

  // AP at position (2.5, 10, 0)
  Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator>();
  apPositionAlloc->Add(Vector(2.5, 10.0, 0.0));
  mobility.SetPositionAllocator(apPositionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign(apDevice);

  // UDP Server on AP
  UdpServerHelper udpServer(serverPort);
  ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(simulationTime));

  // UDP Clients on each STA
  for (uint32_t i = 0; i < nSta; ++i)
    {
      UdpClientHelper udpClient(apInterface.GetAddress(0), serverPort);
      udpClient.SetAttribute("MaxPackets", UintegerValue(5));
      udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
      udpClient.SetAttribute("PacketSize", UintegerValue(512));
      ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(i));
      clientApp.Start(Seconds(1.0));
      clientApp.Stop(Seconds(simulationTime));
    }

  // Enable PCAP tracing
  phy.EnablePcap("wifi-ap", apDevice.Get(0));
  for (uint32_t i = 0; i < nSta; ++i)
    {
      phy.EnablePcap("wifi-sta" + std::to_string(i), staDevices.Get(i));
    }

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}