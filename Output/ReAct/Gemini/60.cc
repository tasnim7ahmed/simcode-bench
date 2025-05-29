#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/address.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/queue.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpLargeTransfer");

static void
CwndTracer (std::string context, uint32_t cwnd)
{
  std::cout << Simulator::Now ().GetSeconds () << " " << cwnd << std::endl;
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("TcpLargeTransfer", LOG_LEVEL_INFO);

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
  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (nodes.Get (2));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get (0), tid);
  ns3TcpSocket->Bind ();
  ns3TcpSocket->Connect (InetSocketAddress (interfaces12.GetAddress (1), port));
  ns3TcpSocket->SetRecvPktInfo (true);

  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&CwndTracer));

  Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper> ("tcp-large-transfer.tr", std::ios::out);
  pointToPoint.EnableAsciiAll (stream);

  pointToPoint.EnablePcapAll ("tcp-large-transfer");

  Simulator::ScheduleWithContext (ns3TcpSocket->GetNode ()->GetId (), Seconds (2.0), [ns3TcpSocket]() {
    uint32_t bytesToSend = 2000000;
    uint32_t packetSize = 1024;
    uint32_t numPackets = bytesToSend / packetSize;
    for (uint32_t i = 0; i < numPackets; ++i) {
      Ptr<Packet> packet = Create<Packet> (packetSize);
      ns3TcpSocket->Send (packet);
    }
  });

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  Simulator::Destroy ();

  return 0;
}