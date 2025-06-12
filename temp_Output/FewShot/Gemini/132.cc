#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/ipv4-global-routing.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  // Enable logging
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable("Ipv4RoutingTable", LOG_LEVEL_ALL);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Create point-to-point link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer devices = pointToPoint.Install(nodes);

  // Install internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // Enable global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create a sink (server) node
  NodeContainer sinkNode;
  sinkNode.Create(1);
  internet.Install(sinkNode);

  // Create a point-to-point link to the sink
  PointToPointHelper sinkLink;
  sinkLink.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  sinkLink.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer sinkDevices;
  sinkDevices = sinkLink.Install(sinkNode.Get(0), nodes.Get(0)); //Connect sink to node 0
  Ipv4InterfaceContainer sinkInterfaces;
  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  sinkInterfaces = ipv4.Assign(sinkDevices);

  //Server Application
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApp = echoServer.Install(sinkNode.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Client Application
  UdpEchoClientHelper echoClient(sinkInterfaces.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  echoClient.SetAttribute("PacketSize", UintegerValue(100);
  ApplicationContainer clientApp = echoClient.Install(nodes.Get(1));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  // Simulate link failure
  Simulator::Schedule(Seconds(5.0), [&]() {
    devices.Get(0)->GetObject<PointToPointNetDevice>()->SetDataRate(DataRate("0bps"));
  });

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}