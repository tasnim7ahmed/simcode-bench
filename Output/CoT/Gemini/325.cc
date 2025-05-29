#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-socket.h"

using namespace ns3;

static void
CWndChange (Ptr<Socket> socket)
{
  Time now = Simulator::Now ();
  uint32_t cwnd = socket->GetCongestionWindow ();
  std::cout << now.GetSeconds () << " " << cwnd << std::endl;
}

static void
RxDrop (Ptr<const Packet> p)
{
  std::cout << "RxDrop at " << Simulator::Now ().GetSeconds () << std::endl;
}

static void
RttEstimate (Ptr<const Packet> p, const Address& addr)
{
  Time rtt = Simulator::Now () - p->GetCreationTime ();
  std::cout << "RTT at " << Simulator::Now ().GetSeconds () << " = " << rtt.GetSeconds() << " seconds" << std::endl;
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100000));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
  Ptr<Socket> socket = Socket::CreateSocket (nodes.Get (0), tid);
  socket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CWndChange));

  devices.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));
  devices.Get (0)->TraceConnect ("PhyTxEnd", MakeCallback (&RttEstimate));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}