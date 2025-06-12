#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBulkSendExample");

int 
main (int argc, char *argv[])
{
  uint64_t maxBytes = 0;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue("maxBytes", "Total number of bytes for application to send (0 = unlimited)", maxBytes);
  cmd.AddValue("tracing", "Enable ASCII and pcap tracing", tracing);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("500Kbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("5ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 9;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  Address remoteAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  BulkSendHelper bulkSender ("ns3::TcpSocketFactory", remoteAddress);
  bulkSender.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  ApplicationContainer senderApp = bulkSender.Install (nodes.Get (0));
  senderApp.Start (Seconds (1.0));
  senderApp.Stop (Seconds (10.0));

  if (tracing)
    {
      AsciiTraceHelper ascii;
      p2p.EnableAsciiAll (ascii.CreateFileStream ("tcp-bulk-send.tr"));
      p2p.EnablePcapAll ("tcp-bulk-send", false);
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
  std::cout << "Total Bytes Received: " << sink->GetTotalRx () << std::endl;

  Simulator::Destroy ();
  return 0;
}