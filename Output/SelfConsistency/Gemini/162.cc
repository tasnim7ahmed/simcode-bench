#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/tcp-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PersistentTcpRing");

int main(int argc, char *argv[]) {
  // Enable logging for the simulation
  LogComponent::Enable("PersistentTcpRing", LOG_LEVEL_INFO);

  // Set simulation time
  Time::SetResolution(Time::NS);
  double simulationTime = 10.0;

  // Create nodes
  NodeContainer nodes;
  nodes.Create(4);

  // Create point-to-point links
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

  // Connect nodes in a ring topology
  NetDeviceContainer devices1 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices2 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer devices3 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
  NetDeviceContainer devices4 = pointToPoint.Install(nodes.Get(3), nodes.Get(0));

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);
  address.NewNetwork();
  Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);
  address.NewNetwork();
  Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);
  address.NewNetwork();
  Ipv4InterfaceContainer interfaces4 = address.Assign(devices4);

  // Configure global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Define TCP client and server ports
  uint16_t port = 50000;

  // Create TCP server application on node 3
  Address serverAddress(Ipv4Address::GetAny());
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
  packetSinkHelper.SetAttribute("Port", UintegerValue(port));
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(3));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(simulationTime));

  // Create TCP client application on node 0
  BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces4.GetAddress(1), port));
  bulkSendHelper.SetAttribute("SendSize", UintegerValue(1024)); // Set the send size in bytes
  bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));    // Send data indefinitely

  ApplicationContainer clientApp = bulkSendHelper.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(simulationTime));

  // Enable PCAP tracing
  pointToPoint.EnablePcapAll("persistent-tcp-ring");

  // Enable NetAnim visualization
  AnimationInterface anim("persistent-tcp-ring.xml");
  anim.SetConstantPosition(nodes.Get(0), 10, 10);
  anim.SetConstantPosition(nodes.Get(1), 30, 10);
  anim.SetConstantPosition(nodes.Get(2), 30, 30);
  anim.SetConstantPosition(nodes.Get(3), 10, 30);

  // Run the simulation
  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}