#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-net-device.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/on-off-application.h"
#include "ns3/udp-echo-client.h"
#include "ns3/udp-echo-server.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(3);

  WifiHelper wifi;
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
  NetDeviceContainer devices = wifi80211p.Install(wifiPhy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(50.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(nodes);

  Ptr<ConstantVelocityMobilityModel> mob0 = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  mob0->SetVelocity(Vector(10, 0, 0));

  Ptr<ConstantVelocityMobilityModel> mob1 = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
  mob1->SetVelocity(Vector(15, 0, 0));

  Ptr<ConstantVelocityMobilityModel> mob2 = nodes.Get(2)->GetObject<ConstantVelocityMobilityModel>();
  mob2->SetVelocity(Vector(5, 0, 0));

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(3));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}