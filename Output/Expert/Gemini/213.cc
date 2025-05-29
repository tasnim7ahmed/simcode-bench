#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/tcp-socket-base.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 50000;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  uint32_t maxBytes = 0;
  BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
  source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (10.0));

  pointToPoint.EnablePcapAll ("tcp-example");

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}