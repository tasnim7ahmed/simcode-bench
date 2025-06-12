#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(3);

  // PointToPoint helper
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Node 0 <-> Node 2
  NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer d0d2 = pointToPoint.Install(n0n2);

  // Node 1 <-> Node 2
  NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer d1d2 = pointToPoint.Install(n1n2);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;

  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

  // Set up the server (Node 2) on port 9
  uint16_t port = 9;
  Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(2));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(10.0));

  // Set up TCP client on Node 0, destined to Node 2 on port 9 (use interface address on link 0)
  OnOffHelper client0("ns3::TcpSocketFactory", InetSocketAddress(i0i2.GetAddress(1), port));
  client0.SetAttribute("DataRate", StringValue("500Kbps"));
  client0.SetAttribute("PacketSize", UintegerValue(1024));
  client0.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
  client0.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
  ApplicationContainer clientApp0 = client0.Install(nodes.Get(0));

  // Set up TCP client on Node 1, destined to Node 2 on port 9 (use interface address on link 1)
  OnOffHelper client1("ns3::TcpSocketFactory", InetSocketAddress(i1i2.GetAddress(1), port));
  client1.SetAttribute("DataRate", StringValue("500Kbps"));
  client1.SetAttribute("PacketSize", UintegerValue(1024));
  client1.SetAttribute("StartTime", TimeValue(Seconds(3.0)));
  client1.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
  ApplicationContainer clientApp1 = client1.Install(nodes.Get(1));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}