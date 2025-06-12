/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpSimpleExample");

uint64_t bytesSent = 0;
uint64_t bytesReceived = 0;

void
TxCallback(Ptr<const Packet> packet)
{
  bytesSent += packet->GetSize();
}

void
RxCallback(Ptr<const Packet> packet, const Address &address)
{
  bytesReceived += packet->GetSize();
}

int
main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Configure point-to-point link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Install network devices
  NetDeviceContainer devices = pointToPoint.Install(nodes);

  // Install protocol stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install packet tracing
  pointToPoint.EnablePcapAll("tcp-simple");

  // Install a TCP server (PacketSink) on node 1
  uint16_t port = 8080;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(10.0));

  // Connect the RxCallback to PacketSink
  Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApp.Get(0));
  sink->TraceConnectWithoutContext("Rx", MakeCallback(&RxCallback));

  // Install a TCP client (BulkSendApplication) on node 0
  BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
  bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(1 << 20)); // Send up to 1MB
  ApplicationContainer clientApp = bulkSendHelper.Install(nodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(10.0));

  // Connect the TxCallback to BulkSendApplication
  Ptr<Application> app = clientApp.Get(0);
  Ptr<BulkSendApplication> bulkSendApp = DynamicCast<BulkSendApplication>(app);
  if (bulkSendApp)
    {
      bulkSendApp->TraceConnectWithoutContext("Tx", MakeCallback(&TxCallback));
    }

  // Run simulation
  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  std::cout << "Bytes sent:     " << bytesSent << std::endl;
  std::cout << "Bytes received: " << bytesReceived << std::endl;

  Simulator::Destroy();
  return 0;
}