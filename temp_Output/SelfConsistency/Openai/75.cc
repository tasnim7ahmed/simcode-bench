/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedPtPAndCsmaCdExample");

void
PacketReceiveCallback(Ptr<const Packet> packet, const Address & addr)
{
  static std::ofstream recvTrace;
  if (!recvTrace.is_open())
    {
      recvTrace.open("packet-receptions.tr", std::ios::out);
    }
  recvTrace << Simulator::Now().GetSeconds()
            << "s: Packet of size "
            << packet->GetSize()
            << " received at "
            << InetSocketAddress::ConvertFrom(addr).GetIpv4()
            << std::endl;
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("MixedPtPAndCsmaCdExample", LOG_LEVEL_INFO);

  // Create all nodes
  NodeContainer nodes;
  nodes.Create(7); // n0, n1, n2, n3, n4, n5, n6

  // Naming for readability
  Ptr<Node> n0 = nodes.Get(0);
  Ptr<Node> n1 = nodes.Get(1);
  Ptr<Node> n2 = nodes.Get(2);
  Ptr<Node> n3 = nodes.Get(3);
  Ptr<Node> n4 = nodes.Get(4);
  Ptr<Node> n5 = nodes.Get(5);
  Ptr<Node> n6 = nodes.Get(6);

  // Point-to-point links from n0 to n2, and n1 to n2
  NodeContainer p2p1 = NodeContainer(n0, n2);
  NodeContainer p2p2 = NodeContainer(n1, n2);

  // CSMA/CD: n2, n3, n4, n5
  NodeContainer csmaCdNodes = NodeContainer(n2, n3, n4, n5);

  // Point-to-point link from n5 to n6
  NodeContainer p2p3 = NodeContainer(n5, n6);

  // Set up point-to-point helpers
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer d_p2p1 = p2p.Install(p2p1);
  NetDeviceContainer d_p2p2 = p2p.Install(p2p2);

  NetDeviceContainer d_p2p3 = p2p.Install(p2p3);

  // Set up CSMA/CD helper
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
  csma.SetChannelAttribute("Delay", StringValue("1ms"));

  NetDeviceContainer d_csma = csma.Install(csmaCdNodes);

  // Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;

  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i_p2p1 = ipv4.Assign(d_p2p1);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i_p2p2 = ipv4.Assign(d_p2p2);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i_csma = ipv4.Assign(d_csma);

  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i_p2p3 = ipv4.Assign(d_p2p3);

  // Enable Queue Tracing for all NetDevices
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll(ascii.CreateFileStream("queues.tr"));
  csma.EnableAsciiAll(ascii.CreateFileStream("queues-csma.tr"));

  // Set up UDP traffic: n0 --> n6
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress(i_p2p3.GetAddress(1), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(n6);
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  // Trace packet receptions at sink
  Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(0));
  sink->TraceConnectWithoutContext("Rx", MakeCallback(
    [](Ptr<const Packet> packet, const Address &addr){
      static std::ofstream recvTrace("packet-receptions.tr", std::ios_base::app);
      recvTrace << Simulator::Now().GetSeconds()
        << "s: Packet of size "
        << packet->GetSize()
        << " received" << std::endl;
    }
  ));

  // Configure OnOff application for CBR traffic
  OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
  onoff.SetAttribute("PacketSize", UintegerValue(210));
  onoff.SetAttribute("DataRate", StringValue("448Kbps"));
  onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
  onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
  onoff.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited

  ApplicationContainer clientApps = onoff.Install(n0);

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}