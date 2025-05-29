#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  // Enable logging
  LogComponentEnable("DistanceVectorRouting", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(3);

  // Create point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devicesAB = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devicesBC = p2p.Install(nodes.Get(1), nodes.Get(2));

  // Install Internet stack
  InternetStackHelper internet;
  internet.SetRoutingHelper(DistanceVectorRoutingHelper());
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

  // Populate routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create traffic source and sink (Node A to Node C)
  uint16_t port = 9; // Discard port
  UdpClientHelper client(interfacesBC.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(10));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(2));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(11.0));

  // Run the simulation
  Simulator::Stop(Seconds(12.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}