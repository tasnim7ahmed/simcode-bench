#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // Enable logging for debugging purposes (optional)
  // LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);

  // Set TCP variant to NewReno
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType",
                      TypeIdValue (TcpNewReno::GetTypeId ()));

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Set up point-to-point link attributes
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install network devices and channel
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Install TCP server (PacketSink) on Node 1 (the receiver)
  uint16_t port = 50000;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  // Install TCP client (BulkSendApplication) on Node 0 (the sender)
  BulkSendHelper bulkSender ("ns3::TcpSocketFactory", sinkAddress);
  bulkSender.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited
  ApplicationContainer sourceApps = bulkSender.Install (nodes.Get (0));
  sourceApps.Start (Seconds (0.0));
  sourceApps.Stop (Seconds (10.0));

  // Install FlowMonitor on all nodes
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll ();

  // Run the simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  // Serialize FlowMonitor statistics to XML
  flowMonitor->SerializeToXmlFile ("tcp-newreno-flowmon.xml", true, true);

  Simulator::Destroy ();
  return 0;
}