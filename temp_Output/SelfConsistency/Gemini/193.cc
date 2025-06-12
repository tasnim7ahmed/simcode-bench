#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpExample");

int main (int argc, char *argv[])
{
  bool tracing = true;
  uint64_t dataRate = 5000000;
  uint32_t packetSize = 1024;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("dataRate", "Application data rate", dataRate);
  cmd.AddValue ("packetSize", "Size of packets generated", packetSize);

  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LogLevel::LOG_PREFIX_TIME);
  LogComponent::SetPrintLineContext (true);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
  pointToPoint.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  BulkSendHelper source ("ns3::TcpSocketFactory", Address (sinkAddress));
  source.SetAttribute ("MaxBytes", UintegerValue (0));
  source.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (0.0));
  sourceApps.Stop (Seconds (10.0));

  if (tracing == true)
    {
      pointToPoint.EnablePcapAll ("tcp-example");
    }

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  uint64_t totalBytesReceived = DynamicCast<PacketSink> (sinkApps.Get (0))->GetTotalRx ();
  std::cout << "Total Bytes Received: " << totalBytesReceived << std::endl;

  Simulator::Destroy ();
  return 0;
}