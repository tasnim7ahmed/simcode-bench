#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer meshNodes;
  meshNodes.Create(3);

  MeshHelper mesh;
  mesh.SetStandard(WIFI_PHY_STANDARD_80211_S);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(meshNodes);

  mesh.Install(meshNodes);

  InternetStackHelper internet;
  internet.Install(meshNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(meshNodes);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(meshNodes.Get(2));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApps = echoClient.Install(meshNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  AnimationInterface anim("mesh-animation.xml");
  anim.SetConstantPosition(meshNodes.Get(0), 0.0, 0.0);
  anim.SetConstantPosition(meshNodes.Get(1), 5.0, 0.0);
  anim.SetConstantPosition(meshNodes.Get(2), 10.0, 0.0);

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}