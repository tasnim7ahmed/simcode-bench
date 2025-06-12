#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/olsr-module.h"
#include "ns3/uan-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer vehicles;
  vehicles.Create(3);

  WifiHelper wifi;
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
  NetDeviceContainer devices = wifi80211p.Install(wifiPhy, mac, vehicles);

  InternetStackHelper internet;
  internet.Install(vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(50.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(vehicles.Get(0));
  vehicles.Get(0)->GetObject<MobilityModel>()->SetVelocity(Vector(20, 0, 0));

  mobility.Install(vehicles.Get(1));
  vehicles.Get(1)->GetObject<MobilityModel>()->SetVelocity(Vector(15, 0, 0));

  mobility.Install(vehicles.Get(2));
  vehicles.Get(2)->GetObject<MobilityModel>()->SetVelocity(Vector(25, 0, 0));

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApp = echoServer.Install(vehicles.Get(2));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp = echoClient.Install(vehicles.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}