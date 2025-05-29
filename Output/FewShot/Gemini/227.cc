#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer uavNodes;
  uavNodes.Create(3);

  NodeContainer gcsNode;
  gcsNode.Create(1);

  MobilityHelper mobilityUAV;
  mobilityUAV.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                   "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                   "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                   "Z", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=20.0]"));
  mobilityUAV.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityUAV.Install(uavNodes);

  MobilityHelper mobilityGCS;
  mobilityGCS.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                   "X", StringValue("ns3::ConstantRandomVariable[Constant=50.0]"),
                                   "Y", StringValue("ns3::ConstantRandomVariable[Constant=50.0]"),
                                   "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));

  mobilityGCS.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityGCS.Install(gcsNode);

  WifiHelper wifi;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  NetDeviceContainer uavDevices;
  NetDeviceContainer gcsDevices;

  Ssid ssid = Ssid("uav-network");
  mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));
  uavDevices = wifi.Install(phy, mac, uavNodes);
  gcsDevices = wifi.Install(phy, mac, gcsNode);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingProtocol("ns3::AodvHelper", "HelloInterval", TimeValue(Seconds(1)),
                            "AllowedHelloLoss", UintegerValue(2),
                            "NetDiameter", UintegerValue(15));
  stack.Install(uavNodes);
  stack.Install(gcsNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer uavInterfaces = address.Assign(uavDevices);
  Ipv4InterfaceContainer gcsInterfaces = address.Assign(gcsDevices);

  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(gcsNode.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(60.0));

  UdpEchoClientHelper echoClient(gcsInterfaces.GetAddress(0), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000000));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < uavNodes.GetN(); ++i) {
    clientApps.Add(echoClient.Install(uavNodes.Get(i)));
  }
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(60.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  AnimationInterface anim("uav-aodv.xml");
  anim.SetConstantPosition(gcsNode.Get(0), 50.0, 50.0);

  Simulator::Stop(Seconds(60.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}