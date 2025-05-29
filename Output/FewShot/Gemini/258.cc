#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-net-device.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(3);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  QosWaveMacHelper mac = QosWaveMacHelper::Default();
  Wifi80211pHelper wifi80211p;
  NetDeviceContainer devices = wifi80211p.Install(phy, mac, nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(20.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(nodes);

  Ptr<ConstantVelocityMobilityModel> cvmm0 =
      nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  cvmm0->SetVelocity(Vector(5, 0, 0)); // m/s

  Ptr<ConstantVelocityMobilityModel> cvmm1 =
      nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
  cvmm1->SetVelocity(Vector(10, 0, 0)); // m/s

  Ptr<ConstantVelocityMobilityModel> cvmm2 =
      nodes.Get(2)->GetObject<ConstantVelocityMobilityModel>();
  cvmm2->SetVelocity(Vector(15, 0, 0)); // m/s

  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(2), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(3));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  AnimationInterface anim("vanet_simulation.xml");
  anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
  anim.SetConstantPosition(nodes.Get(1), 20.0, 0.0);
  anim.SetConstantPosition(nodes.Get(2), 40.0, 0.0);

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}