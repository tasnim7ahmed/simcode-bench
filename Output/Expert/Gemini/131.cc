#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DistanceVectorCountToInfinity");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ("DistanceVectorCountToInfinity", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devicesAB = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devicesBC = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAB = address.Assign (devicesAB);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesBC = address.Assign (devicesBC);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Break the link between B and C
  Simulator::Schedule (Seconds (5.0), [&](){
    devicesBC.Get (0)->GetChannel ()->Dispose ();
    devicesBC.Get (1)->GetChannel ()->Dispose ();
    NS_LOG_INFO ("Link between B and C broken.");
  });

  // Log routing tables at different times
  Simulator::Schedule (Seconds (3.0), [&](){
    NS_LOG_INFO ("Routing table at Node A (before link down): " << Ipv4GlobalRoutingHelper::PrintRoutingTableAt (nodes.Get (0), Seconds (3.0)));
    NS_LOG_INFO ("Routing table at Node C (before link down): " << Ipv4GlobalRoutingHelper::PrintRoutingTableAt (nodes.Get (2), Seconds (3.0)));
  });

  Simulator::Schedule (Seconds (7.0), [&](){
    NS_LOG_INFO ("Routing table at Node A (after link down): " << Ipv4GlobalRoutingHelper::PrintRoutingTableAt (nodes.Get (0), Seconds (7.0)));
    NS_LOG_INFO ("Routing table at Node C (after link down): " << Ipv4GlobalRoutingHelper::PrintRoutingTableAt (nodes.Get (2), Seconds (7.0)));
  });

  Simulator::Schedule (Seconds (15.0), [&](){
    NS_LOG_INFO ("Routing table at Node A (long after link down): " << Ipv4GlobalRoutingHelper::PrintRoutingTableAt (nodes.Get (0), Seconds (15.0)));
    NS_LOG_INFO ("Routing table at Node C (long after link down): " << Ipv4GlobalRoutingHelper::PrintRoutingTableAt (nodes.Get (2), Seconds (15.0)));
  });

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}