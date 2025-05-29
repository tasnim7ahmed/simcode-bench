#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  // Create nodes: STA (sender) and AP (receiver)
  NodeContainer staNodes;
  staNodes.Create(1);
  NodeContainer apNode;
  apNode.Create(1);

  // Configure wireless channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  // Configure Wi-Fi MAC layer
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

  NqosWifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(Seconds(1.0)));

  NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

  // Configure mobility for STA (random walk) and AP (stationary)
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
  mobility.Install(staNodes);

  MobilityHelper staticMobility;
  staticMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  staticMobility.Install(apNode);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(staNodes);
  stack.Install(apNode);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);

  // Configure UDP echo client (STA)
  uint16_t port = 9;
  UdpEchoClientHelper echoClient(apInterfaces.GetAddress(0), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1))); // Send every 0.1 seconds
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  // Install and start UDP echo client application
  ApplicationContainer clientApps = echoClient.Install(staNodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  // Configure UDP echo server (AP)
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(apNode.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Run the simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}