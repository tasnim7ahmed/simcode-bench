#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  // Create nodes
  NodeContainer nodes;
  nodes.Create(3);

  // Configure point-to-point links
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Install NetDevices
  NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

  // Create UDP client application on node 0
  UdpClientHelper client(interfaces12.GetAddress(1), 9); // Send to node 2
  client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  // Create UDP server application on node 2
  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(nodes.Get(2));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  // Enable PCAP tracing
  pointToPoint.EnablePcapAll("line_topology");

  // Run the simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}