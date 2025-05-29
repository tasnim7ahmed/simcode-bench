#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/rip-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  // Enable logging
  LogComponentEnable("Rip", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(3);

  // Name the nodes for clarity
  Names::Add("NodeA", nodes.Get(0));
  Names::Add("NodeB", nodes.Get(1));
  Names::Add("NodeC", nodes.Get(2));

  // Create point-to-point links
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devicesAB = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devicesBC = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer devicesCA = pointToPoint.Install(nodes.Get(2), nodes.Get(0));

  // Install Internet stack on all nodes
  InternetStackHelper internet;
  RipHelper ripRouting;
  internet.SetRoutingHelper(ripRouting);
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);
  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);
  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesCA = address.Assign(devicesCA);

  // Start RIP routing protocol
  ripRouting.ExcludeInterface(nodes.Get(0), 2);
  ripRouting.ExcludeInterface(nodes.Get(1), 2);
  ripRouting.ExcludeInterface(nodes.Get(2), 2);

  // Create sink application on node C
  uint16_t port = 9;
  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sink.Install(nodes.Get(2));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(20.0));

  // Create OnOff application on node A
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfacesBC.GetAddress(1), port));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));
  onoff.SetAttribute("DataRate", StringValue("500kbps"));
  ApplicationContainer onoffApp = onoff.Install(nodes.Get(0));
  onoffApp.Start(Seconds(2.0));
  onoffApp.Stop(Seconds(20.0));

  // Break the link between B and C at time 10s
  Simulator::Schedule(Seconds(10.0), [&]() {
    std::cout << "Breaking link between B and C" << std::endl;
    devicesBC.Get(0)->Dispose();
    devicesBC.Get(1)->Dispose();
  });

  // Run the simulation
  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}