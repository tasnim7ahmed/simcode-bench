#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/net-device.h"
#include "ns3/queue.h"
#include <iostream>
#include <fstream>

using namespace ns3;

static void
CwndTracer (std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
  std::cout << Simulator::Now ().GetSeconds () << " " << newCwnd << std::endl;
}


int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));
  pointToPoint.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (20));

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

  uint16_t port = 50000;
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (interfaces12.GetAddress (1), port));
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (2));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));


  TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get (0), tid);
  ns3TcpSocket->Bind ();
  ns3TcpSocket->Connect (InetSocketAddress (interfaces12.GetAddress (1), port));


  uint32_t bytesTotal = 2000000;
  uint32_t packetSize = 1024;
  uint32_t numPackets = bytesTotal / packetSize;

  Simulator::ScheduleWithContext (ns3TcpSocket->GetNode ()->GetId (), Seconds (2.0), [ns3TcpSocket, numPackets, packetSize]() {
    for (uint32_t i = 0; i < numPackets; ++i)
      {
        Ptr<Packet> packet = Create<Packet> (packetSize);
        ns3TcpSocket->Send (packet);
      }
  });

  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("tcp-large-transfer.tr"));

  pointToPoint.EnablePcapAll ("tcp-large-transfer");

  Config::ConnectWithoutContext ("/NodeList/0/ApplicationList/0/$ns3::TcpSocketBase/CongestionWindow", MakeCallback (&CwndTracer));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}