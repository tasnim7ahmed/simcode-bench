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
  LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Set up point-to-point link attributes
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  // Install network devices
  NetDeviceContainer devices;
  devices = p2p.Install(nodes);

  // Install the Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install PacketSink (TCP server) on Node 1
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  // Install BulkSendHelper (TCP client) on Node 0
  BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory",
                                 InetSocketAddress(interfaces.GetAddress(1), port));
  bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer clientApps = bulkSendHelper.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Enable pcap tracing
  p2p.EnablePcapAll("p2p-tcp-bulk-send");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}