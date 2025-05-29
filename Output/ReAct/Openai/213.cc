#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

static uint64_t totalBytesSent = 0;
static uint64_t totalBytesReceived = 0;

void
TxCallback (Ptr<const Packet> packet)
{
  totalBytesSent += packet->GetSize ();
}

void
RxCallback (Ptr<const Packet> packet, const Address &address)
{
  totalBytesReceived += packet->GetSize ();
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // Create Nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Create Point-to-Point Channel
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP Addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Install Packet Sink (TCP Server) on Node 1
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  // Install TCP Client (BulkSendApplication) on Node 0
  BulkSendHelper bulkSender ("ns3::TcpSocketFactory", sinkAddress);
  bulkSender.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer clientApps = bulkSender.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  // Enable packet tracing
  p2p.EnablePcapAll ("tcp-p2p");

  // Connect trace sources
  devices.Get (0)->GetObject<PointToPointNetDevice> ()->TraceConnectWithoutContext ("PhyTxEnd", MakeCallback (&TxCallback));
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApps.Get (0));
  sink->TraceConnectWithoutContext ("Rx", MakeCallback (&RxCallback));

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  std::cout << "Total Bytes Sent: " << totalBytesSent << std::endl;
  std::cout << "Total Bytes Received: " << totalBytesReceived << std::endl;

  Simulator::Destroy ();
  return 0;
}