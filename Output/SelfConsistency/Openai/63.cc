#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpPcapNanosecExample");

int
main (int argc, char *argv[])
{
  bool enableTracing = true;
  bool enableNanoSec = false;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable packet tracing", enableTracing);
  cmd.AddValue ("nanosec", "Enable nanosecond timestamps in pcap", enableNanoSec);
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Configure point-to-point link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("500Kbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("5ms"));

  // Set pcap nanosecond timestamps if requested
  if (enableNanoSec)
    {
      pointToPoint.SetPcapDataLinkType (PointToPointHelper::DLT_PPP);
      Config::SetDefault ("ns3::PcapFileWrapper::NanosecMode", BooleanValue (true));
    }

  NetDeviceContainer devices = pointToPoint.Install (nodes);

  // Install internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Install PacketSinkApplication on n1
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  // Install BulkSendApplication on n0
  BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory", sinkAddress);
  // SetMaxBytes to zero means unlimited data
  bulkSendHelper.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApp = bulkSendHelper.Install (nodes.Get (0));
  sourceApp.Start (Seconds (1.0));
  sourceApp.Stop (Seconds (10.0));

  // Enable pcap tracing if requested
  if (enableTracing)
    {
      pointToPoint.EnablePcap ("tcp-pcap-nanosec-example", devices, true /*promiscuous*/);
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}