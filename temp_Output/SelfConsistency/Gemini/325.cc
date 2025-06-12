#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-socket-base.h"

#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpRttSimulation");

static void
CWndChange(std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
  std::cout << Simulator::Now().GetSeconds() << " " << context << " OldCwnd " << oldCwnd
            << " NewCwnd " << newCwnd << std::endl;
}

static void
RxDrop(std::string context, Ptr<const Packet> p)
{
  std::cout << Simulator::Now().GetSeconds() << " " << context << " RxDrop at node "
            << Simulator::GetContext() << std::endl;
}

static void
RttTracer(std::string context, Time oldval, Time newval)
{
  std::cout << Simulator::Now().GetSeconds() << " " << context << " RTT " << newval << std::endl;
}

int
main(int argc, char* argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 50000;

  // Server
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  // Client
  Ptr<Node> appSource = nodes.Get(0);
  Ptr<Ipv4> ipv4 = appSource->GetObject<Ipv4>();
  Ipv4Address serverAddress = interfaces.GetAddress(1);

  V4TcpClientHelper clientHelper(serverAddress, port);
  clientHelper.SetAttribute("MaxPackets", UintegerValue(10000));
  clientHelper.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Tracing
  Config::ConnectWithoutContext(
      "/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
      MakeCallback(&CWndChange));
  Config::ConnectWithoutContext(
      "/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/Drop", MakeCallback(&RxDrop));

  // RTT tracing
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream =
      asciiTraceHelper.CreateFileStream("tcp-rtt-simulation.txt");
  Config::ConnectWithoutContext(
      "/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/RttEst", MakeBoundCallback(&RttTracer));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}