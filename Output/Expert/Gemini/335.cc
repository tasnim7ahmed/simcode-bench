#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer apNode;
  apNode.Create(1);

  NodeContainer staNode;
  staNode.Create(1);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
  mobility.Install(staNode);

  MobilityHelper apMobility;
  apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  apMobility.Install(apNode);

  InternetStackHelper internet;
  internet.Install(apNode);
  internet.Install(staNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

  uint16_t port = 9;
  Address serverAddress = InetSocketAddress(apInterface.GetAddress(0), port);

  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer sinkApp = sinkHelper.Install(apNode);
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(11.0));

  OnOffHelper onOffHelper("ns3::TcpSocketFactory", serverAddress);
  onOffHelper.SetConstantRate(DataRate("10Mbps"));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer clientApp = onOffHelper.Install(staNode);
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(12.0));

  Simulator::Stop(Seconds(12.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}