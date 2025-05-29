#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(5);

  NodeContainer rsu;
  rsu.Create(1);

  DsdvHelper dsdv;
  InternetStackHelper stack;
  stack.SetRoutingHelper(dsdv);
  stack.Install(nodes);
  stack.Install(rsu);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(NodeContainer::Create(nodes, rsu));

  WaypointMobilityModelHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 10.0, 0.0));
  positionAlloc->Add(Vector(20.0, 10.0, 0.0));
  positionAlloc->Add(Vector(40.0, 10.0, 0.0));
  positionAlloc->Add(Vector(60.0, 10.0, 0.0));
  positionAlloc->Add(Vector(80.0, 10.0, 0.0));

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<Node> node = nodes.Get(i);
    Ptr<ConstantPositionMobilityModel> loc = node->GetObject<ConstantPositionMobilityModel>();
    loc->SetPosition(positionAlloc->GetNext());
  }

  Ptr<ListPositionAllocator> rsuPositionAlloc = CreateObject<ListPositionAllocator>();
  rsuPositionAlloc->Add(Vector(50.0, 20.0, 0.0));
  Ptr<Node> rsuNode = rsu.Get(0);
  Ptr<ConstantPositionMobilityModel> rsuLoc = rsuNode->GetObject<ConstantPositionMobilityModel>();
  rsuLoc->SetPosition(rsuPositionAlloc->GetNext());

  UdpClientServerHelper cs(9);
  cs.SetAttribute("MaxPackets", UintegerValue(1000));
  cs.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  cs.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    cs.SetAttribute("RemoteAddress", AddressValue(interfaces.GetAddress(5)));
    clientApps.Add(cs.Install(nodes.Get(i)));
  }

  ApplicationContainer serverApps = cs.Install(rsu);

  clientApps.Start(Seconds(1.0));
  serverApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(30.0));
  serverApps.Stop(Seconds(30.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(30.0));

  AnimationInterface anim("vanet-dsdv.xml");

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}