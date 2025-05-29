#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BulkSendPcapNanosecExample");

int 
main (int argc, char *argv[])
{
  bool enablePcap = true;
  bool nanosecTimestamps = false;

  CommandLine cmd;
  cmd.AddValue ("pcap", "Enable/disable PCAP tracing", enablePcap);
  cmd.AddValue ("nanosec", "Enable nanosecond timestamps in PCAP trace", nanosecTimestamps);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("500Kbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("5ms"));

  NetDeviceContainer devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // BulkSendApplication on n0
  uint16_t port = 9;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory", sinkAddress);
  bulkSendHelper.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer senderApp = bulkSendHelper.Install (nodes.Get (0));
  senderApp.Start (Seconds (1.0));
  senderApp.Stop (Seconds (10.0));

  // PacketSinkApplication on n1
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  if (enablePcap)
    {
      pointToPoint.EnablePcap ("tcp-pcap-nanosec-example", devices, false, nanosecTimestamps);
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}