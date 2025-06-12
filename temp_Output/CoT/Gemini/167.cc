#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/packet-sink-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpPointToPoint");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ("TcpPointToPoint", LOG_LEVEL_INFO);

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
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (interfaces.GetAddress (0), port));
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (0));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  Address serverAddress (InetSocketAddress (interfaces.GetAddress (0), port));
  TcpClientHelper clientHelper (serverAddress);
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (1000000));
  clientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.0001)));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = clientHelper.Install (nodes.Get (1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  pointToPoint.EnablePcapAll ("tcp-point-to-point");

  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("tcp-point-to-point.tr"));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}