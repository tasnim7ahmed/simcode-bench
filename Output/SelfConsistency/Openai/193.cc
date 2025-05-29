#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PointToPointTcpBulkTransfer");

static uint64_t g_totalBytesReceived = 0;

void
RxCallback (Ptr<const Packet> packet, const Address &addr)
{
  g_totalBytesReceived += packet->GetSize ();
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("PointToPointTcpBulkTransfer", LOG_LEVEL_INFO);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Configure point-to-point channel
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install network devices
  NetDeviceContainer devices = pointToPoint.Install (nodes);

  // Install the Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Enable PCAP tracing
  pointToPoint.EnablePcapAll ("point-to-point-tcp");

  // Install a TCP receiver (PacketSink) on node 1
  uint16_t sinkPort = 50000;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  // Connect trace source to monitor RX
  Ptr<Application> app = sinkApp.Get (0);
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (app);
  sink->TraceConnectWithoutContext ("Rx", MakeCallback (&RxCallback));

  // Install BulkSendApplication on node 0 to send as much as possible
  BulkSendHelper bulkSender ("ns3::TcpSocketFactory", sinkAddress);
  bulkSender.SetAttribute ("MaxBytes", UintegerValue (0)); // Unlimited
  ApplicationContainer senderApp = bulkSender.Install (nodes.Get (0));
  senderApp.Start (Seconds (1.0));
  senderApp.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  // Report the total received bytes
  std::cout << "Total Bytes Received: " << g_totalBytesReceived << std::endl;

  Simulator::Destroy ();
  return 0;
}