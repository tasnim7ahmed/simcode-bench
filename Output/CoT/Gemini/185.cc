#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer apNodes;
  apNodes.Create(3);

  NodeContainer staNodes;
  staNodes.Create(6);

  NodeContainer serverNode;
  serverNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

  Ssid ssid1 = Ssid("ns3-ssid-1");
  Ssid ssid2 = Ssid("ns3-ssid-2");
  Ssid ssid3 = Ssid("ns3-ssid-3");

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid1),
              "BeaconInterval", TimeValue(Seconds(0.075)),
              "QosSupported", BooleanValue(true),
              "EnableBeaconJitter", BooleanValue(false));

  NetDeviceContainer apDevice1 = wifi.Install(phy, mac, apNodes.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid2),
              "BeaconInterval", TimeValue(Seconds(0.075)),
              "QosSupported", BooleanValue(true),
              "EnableBeaconJitter", BooleanValue(false));

  NetDeviceContainer apDevice2 = wifi.Install(phy, mac, apNodes.Get(1));

   mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid3),
              "BeaconInterval", TimeValue(Seconds(0.075)),
              "QosSupported", BooleanValue(true),
              "EnableBeaconJitter", BooleanValue(false));

  NetDeviceContainer apDevice3 = wifi.Install(phy, mac, apNodes.Get(2));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid1),
              "QosSupported", BooleanValue(true),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevice1 = wifi.Install(phy, mac, staNodes.Get(0));
  NetDeviceContainer staDevice2 = wifi.Install(phy, mac, staNodes.Get(1));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid2),
              "QosSupported", BooleanValue(true),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice3 = wifi.Install(phy, mac, staNodes.Get(2));
  NetDeviceContainer staDevice4 = wifi.Install(phy, mac, staNodes.Get(3));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid3),
              "QosSupported", BooleanValue(true),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice5 = wifi.Install(phy, mac, staNodes.Get(4));
  NetDeviceContainer staDevice6 = wifi.Install(phy, mac, staNodes.Get(5));

  InternetStackHelper internet;
  internet.Install(apNodes);
  internet.Install(staNodes);
  internet.Install(serverNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface1 = address.Assign(apDevice1);
  Ipv4InterfaceContainer staInterface1 = address.Assign(staDevice1);
  Ipv4InterfaceContainer staInterface2 = address.Assign(staDevice2);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface2 = address.Assign(apDevice2);
  Ipv4InterfaceContainer staInterface3 = address.Assign(staDevice3);
  Ipv4InterfaceContainer staInterface4 = address.Assign(staDevice4);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface3 = address.Assign(apDevice3);
  Ipv4InterfaceContainer staInterface5 = address.Assign(staDevice5);
  Ipv4InterfaceContainer staInterface6 = address.Assign(staDevice6);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer serverInterface = address.Assign(serverNode);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("50.0"),
                                "Y", StringValue("50.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=20.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Mode", StringValue("Time"),
                            "Time", StringValue("1s"),
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                            "Bounds", StringValue("50 50 100 100"));

  mobility.Install(staNodes);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);
  mobility.Install(serverNode);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(serverNode.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient1(serverInterface.GetAddress(0), 9);
  echoClient1.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps1 = echoClient1.Install(staNodes.Get(0));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(serverInterface.GetAddress(0), 9);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps2 = echoClient2.Install(staNodes.Get(3));
  clientApps2.Start(Seconds(3.0));
  clientApps2.Stop(Seconds(10.0));


  AnimationInterface anim("wifi-mobility.xml");
  anim.SetConstantPosition(serverNode.Get(0), 10, 10);
  anim.SetConstantPosition(apNodes.Get(0), 60, 60);
  anim.SetConstantPosition(apNodes.Get(1), 80, 60);
  anim.SetConstantPosition(apNodes.Get(2), 70, 80);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}