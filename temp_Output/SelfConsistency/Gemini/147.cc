#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BranchTopology");

int main(int argc, char *argv[]) {
  // Enable logging
  LogComponent::SetLevel( "UdpClient", LOG_LEVEL_INFO );
  LogComponent::SetLevel( "UdpServer", LOG_LEVEL_INFO );

  // Create nodes
  NodeContainer nodes;
  nodes.Create(3);

  // Create point-to-point helper
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Create net devices and channels
  NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);

  // Create UDP server on node 2
  UdpServerHelper server(9);
  ApplicationContainer serverApp = server.Install(nodes.Get(2));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Create UDP client on node 1
  UdpClientHelper client(interfaces02.GetAddress(1), 9); // Send to node 2
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp = client.Install(nodes.Get(1));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  // Enable PCAP tracing
  pointToPoint.EnablePcapAll("branch");

  // Enable global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Run simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}