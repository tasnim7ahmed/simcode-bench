#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  // Enable logging
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create Nodes: AP and two STAs
  NodeContainer apNode;
  apNode.Create(1);
  NodeContainer staNodes;
  staNodes.Create(2);

  // Configure Wireless Channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  // Configure Wifi Helper
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

  // Configure Wifi MAC
  NqosWifiMacHelper mac;

  // Configure the AP
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(MilliSeconds(100)));

  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  // Configure the STAs
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

  // Install mobility model
  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue("Time"),
                             "Time", StringValue("1s"),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]")); // 1 m/s

  mobility.Install(staNodes);

  // Make AP stationary
  MobilityHelper staticMobility;
  staticMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  staticMobility.Install(apNode);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(apNode);
  internet.Install(staNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  // Setup UDP Echo Server on the AP
  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApp = echoServer.Install(apNode.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Setup UDP Echo Clients on the STAs
  UdpEchoClientHelper echoClient1(apInterface.GetAddress(0), port);
  echoClient1.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp1 = echoClient1.Install(staNodes.Get(0));
  clientApp1.Start(Seconds(2.0));
  clientApp1.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient2(apInterface.GetAddress(0), port);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp2 = echoClient2.Install(staNodes.Get(1));
  clientApp2.Start(Seconds(2.0));
  clientApp2.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Run the simulation
  Simulator::Run(Seconds(10.0));
  Simulator::Destroy();

  return 0;
}