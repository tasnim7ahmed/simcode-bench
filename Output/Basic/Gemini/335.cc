#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer apNode;
  apNode.Create(1);

  NodeContainer staNode;
  staNode.Create(1);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;

  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds",
                             RectangleValue(Rectangle(0, 100, 0, 100)));
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

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(apNode.Get(0));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(11.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(staNode.Get(0), TcpSocketFactory::GetTypeId());

  Ptr<Application> app = CreateObject<BulkSendApplication>(
      InetSocketAddress(apInterface.GetAddress(0), port), ns3TcpSocket, 1024, 10);
  app->SetStartTime(Seconds(2.0));
  app->SetStopTime(Seconds(12.0));
  staNode.Get(0)->AddApplication(app);

  Simulator::Stop(Seconds(12.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}