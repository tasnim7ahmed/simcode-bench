#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
  // Set simulation time
  double simTime = 12.0; // seconds

  // Create 2 nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Configure point-to-point link
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devices = p2p.Install(nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install TCP server (on node 1, Computer 2)
  uint16_t port = 9;
  Address bindAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", bindAddress);
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(simTime));

  // Install TCP client (on node 0, Computer 1)
  Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
  clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
  clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(11.0));

  // Limit number of packets to 10. Use MaxBytes attribute.
  uint32_t numPackets = 10;
  uint32_t packetSize = 1024;
  clientApp.Get(0)->SetAttribute("MaxBytes", UintegerValue(numPackets * packetSize));

  // Enable logging (optional, can be omitted if not desired)
  // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}