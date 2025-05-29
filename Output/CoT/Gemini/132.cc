#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"
#include "ns3/ipv4-global-routing.h"
#include "ns3/dv-routing-protocol.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DistanceVectorTwoNodeLoop");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetCategory ("DistanceVectorTwoNodeLoop");
  LogComponent::SetLevel (LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper internet;
  internet.SetRoutingHelper (DvRoutingHelper ());
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1, 0), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (10.0));

  Ptr<Node> appSource = nodes.Get (0);
  UdpClientHelper clientHelper (interfaces.GetAddress (1, 0), sinkPort);
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (32000));
  clientHelper.SetAttribute ("Interval", TimeValue (Time ("1ms")));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = clientHelper.Install (appSource);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (5.0));

  Simulator::Schedule (Seconds (6.0), [&] () {
      Ptr<DvRoutingProtocol> dvRoutingA = DynamicCast<DvRoutingProtocol>(nodes.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
      Ptr<DvRoutingProtocol> dvRoutingB = DynamicCast<DvRoutingProtocol>(nodes.Get(1)->GetObject<Ipv4>()->GetRoutingProtocol());

      dvRoutingA->InvalidateRouteTo(interfaces.GetAddress(1,0));
      dvRoutingB->InvalidateRouteTo(interfaces.GetAddress(0,0));
  });

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}