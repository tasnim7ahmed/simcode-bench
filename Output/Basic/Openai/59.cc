#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  uint64_t maxBytes = 0;
  bool enableTracing = false;

  CommandLine cmd;
  cmd.AddValue ("maxBytes", "Total number of bytes for BulkSendApplication to send (0 means unlimited)", maxBytes);
  cmd.AddValue ("tracing", "Enable ascii and pcap tracing", enableTracing);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("500Kbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("5ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 9;

  BulkSendHelper bulkSender ("ns3::TcpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
  bulkSender.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  ApplicationContainer sourceApps = bulkSender.Install (nodes.Get (0));
  sourceApps.Start (Seconds (0.0));
  sourceApps.Stop (Seconds (10.0));

  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sinkHelper.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  if (enableTracing)
    {
      AsciiTraceHelper ascii;
      pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("tcp-bulk-send.tr"));
      pointToPoint.EnablePcapAll ("tcp-bulk-send", false);
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApps.Get (0));
  std::cout << "Total bytes received by sink: " << sink->GetTotalRx () << std::endl;

  Simulator::Destroy ();
  return 0;
}