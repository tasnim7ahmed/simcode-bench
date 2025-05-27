#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/oscillation-detection.h"
#include "ns3/ipv4-ospf-routing.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if set to true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("OspfExample", LOG_LEVEL_INFO);
      LogComponentEnable ("ns3::OspfRouting", LOG_LEVEL_ALL);
      LogComponentEnable ("ns3::OspfInterface", LOG_LEVEL_ALL);
      LogComponentEnable ("ns3::OspfArea", LOG_LEVEL_ALL);
      LogComponentEnable ("ns3::TcpL4Protocol", LOG_LEVEL_ALL);
      LogComponentEnableAll (LOG_PREFIX_FUNC);
      LogComponentEnableAll (LOG_PREFIX_NODE);
      LogComponentEnableAll (LOG_PREFIX_TIME);
    }

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices[4];
  devices[0] = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  devices[1] = pointToPoint.Install (nodes.Get (1), nodes.Get (2));
  devices[2] = pointToPoint.Install (nodes.Get (2), nodes.Get (3));
  devices[3] = pointToPoint.Install (nodes.Get (3), nodes.Get (0));

  InternetStackHelper internet;
  OspfHelper ospfRoutingHelper;
  internet.SetRoutingHelper (ospfRoutingHelper);
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces[4];
  interfaces[0] = address.Assign (devices[0]);
  address.NewNetwork ();
  interfaces[1] = address.Assign (devices[1]);
  address.NewNetwork ();
  interfaces[2] = address.Assign (devices[2]);
  address.NewNetwork ();
  interfaces[3] = address.Assign (devices[3]);

  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces[2].GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (tracing)
    {
      pointToPoint.EnablePcapAll ("ospf-example");
    }

  Simulator::Stop (Seconds (11.0));

  Simulator::Run ();

  Ipv4 ospf;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("ospf.routes", std::ios::out);
  ospf = nodes.Get (0)->GetObject<Ipv4> ();
  ospf.GetRoutingProtocol ()->PrintRoutingTableAllAt (Seconds (11.0), routingStream);

  ospf = nodes.Get (1)->GetObject<Ipv4> ();
  ospf.GetRoutingProtocol ()->PrintRoutingTableAllAt (Seconds (11.0), routingStream);

  ospf = nodes.Get (2)->GetObject<Ipv4> ();
  ospf.GetRoutingProtocol ()->PrintRoutingTableAllAt (Seconds (11.0), routingStream);

  ospf = nodes.Get (3)->GetObject<Ipv4> ();
  ospf.GetRoutingProtocol ()->PrintRoutingTableAllAt (Seconds (11.0), routingStream);

  Simulator::Destroy ();
  return 0;
}