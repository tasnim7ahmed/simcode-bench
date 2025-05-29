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

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Set up point-to-point link attributes
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  // Install devices and channel
  NetDeviceContainer devices;
  devices = p2p.Install(nodes);

  // Install the Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Set up the TCP server (PacketSink) on node 1
  uint16_t port = 50000;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  // Set up the TCP client (BulkSend) on node 0
  BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
  bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer clientApp = bulkSendHelper.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}