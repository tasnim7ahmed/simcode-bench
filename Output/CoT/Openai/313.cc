#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(2);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(0.0),
                               "MinY", DoubleValue(0.0),
                               "DeltaX", DoubleValue(5.0),
                               "DeltaY", DoubleValue(0.0),
                               "GridWidth", UintegerValue(1),
                               "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                            "Distance", DoubleValue(10.0),
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
  mobility.Install(wifiStaNodes);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNodes);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

  // UDP server on STA 0
  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(wifiStaNodes.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // UDP client on STA 1
  UdpClientHelper client(staInterfaces.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(1));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}