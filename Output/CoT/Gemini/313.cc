#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer staNodes;
  staNodes.Create(2);

  NodeContainer apNode;
  apNode.Create(1);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, staNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(Seconds(1.0)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, apNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("50.0"),
                                "Y", StringValue("50.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=30.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue("Time"),
                             "Time", StringValue("1s"),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue("50x50"));
  mobility.Install(staNodes);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);

  Ptr<ConstantPositionMobilityModel> apMobility = apNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
  apMobility->SetPosition(Vector(50.0, 50.0, 0.0));

  InternetStackHelper internet;
  internet.Install(staNodes);
  internet.Install(apNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterfaces;
  apInterfaces = address.Assign(apDevices);

  UdpEchoServerHelper echoServer(9);

  ApplicationContainer serverApps = echoServer.Install(staNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper echoClient(staInterfaces.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(staNodes.Get(1));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}