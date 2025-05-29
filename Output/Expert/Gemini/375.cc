#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  NodeContainer apNode = NodeContainer(nodes.Get(0));
  NodeContainer staNode = NodeContainer(nodes.Get(1));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);

  Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
  mobility.SetMobilityModel("ns3::RandomRectanglePositionAllocator",
                             "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                             "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
  mobility.Install(staNode);

  Ptr<RandomRectanglePositionAllocator> rrp = DynamicCast<RandomRectanglePositionAllocator>(mobility.GetPositionAllocator());
  rrp->SetX(var);
  rrp->SetY(var);

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 0, 50, 50)));
  mobility.Install(staNode);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign(apDevice);
  interfaces = address.Assign(staDevice);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(apNode.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(staNode.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}