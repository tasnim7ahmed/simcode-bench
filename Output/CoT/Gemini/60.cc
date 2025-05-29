#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/simulator.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

uint32_t bytesReceived;
uint32_t bytesTotal = 2000000;

static void
CwndTracer (std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
  std::cout << Simulator::Now ().GetSeconds () << " " << context << " " << oldCwnd << " " << newCwnd << std::endl;
}

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer devices01 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices12 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign (devices01);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign (devices12);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (2));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (10.0));

  TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get (0), tid);
  ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndTracer));

  InetSocketAddress remoteAddress (interfaces12.GetAddress (1), port);
  ns3TcpSocket->Connect (remoteAddress);

  Simulator::Schedule (Seconds (1.1), [ns3TcpSocket]() {
      ns3TcpSocket->Send (Create<Packet> (bytesTotal));
  });

  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("tcp-large-transfer.tr"));

  pointToPoint.EnablePcapAll ("tcp-large-transfer");

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}