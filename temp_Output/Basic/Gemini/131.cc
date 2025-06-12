#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/dv-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DistanceVectorCountToInfinity");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devicesAB = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devicesBC = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper internet;
  DvRoutingHelper dvRouting;
  internet.SetRoutingHelper (dvRouting);
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAB = address.Assign (devicesAB);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesBC = address.Assign (devicesBC);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Simulate a packet transmission from A to C before the link failure.
  UdpEchoClientHelper echoClient (interfacesBC.GetAddress (1), 9);
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1)));
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (1));
  clientApps.Stop (Seconds (3));

  // Break the link between B and C at time 5 seconds.
  Simulator::Schedule (Seconds (5.0), [&](){
    Config::Set ("/ChannelList/*/$ns3::PointToPointChannel/Down", BooleanValue (true));
    NS_LOG_INFO ("Link between B and C is down.");
  });

  // Schedule another packet transmission from A to C after link failure.
  UdpEchoClientHelper echoClient2 (interfacesBC.GetAddress (1), 9);
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1)));
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (1));
  ApplicationContainer clientApps2 = echoClient2.Install (nodes.Get (0));
  clientApps2.Start (Seconds (6));
  clientApps2.Stop (Seconds (8));

  // Print routing tables at certain times to observe the "count to infinity"
  Simulator::Schedule (Seconds (4), [&](){
    std::cout << "Routing table at Node A (before link failure):" << std::endl;
    dvRouting.PrintRoutingTableAt (nodes.Get (0), Seconds (0));
    std::cout << "Routing table at Node B (before link failure):" << std::endl;
    dvRouting.PrintRoutingTableAt (nodes.Get (1), Seconds (0));
    std::cout << "Routing table at Node C (before link failure):" << std::endl;
    dvRouting.PrintRoutingTableAt (nodes.Get (2), Seconds (0));
  });

    Simulator::Schedule (Seconds (7), [&](){
    std::cout << "Routing table at Node A (after link failure):" << std::endl;
    dvRouting.PrintRoutingTableAt (nodes.Get (0), Seconds (0));
    std::cout << "Routing table at Node B (after link failure):" << std::endl;
    dvRouting.PrintRoutingTableAt (nodes.Get (1), Seconds (0));
    std::cout << "Routing table at Node C (after link failure):" << std::endl;
    dvRouting.PrintRoutingTableAt (nodes.Get (2), Seconds (0));
  });

   Simulator::Schedule (Seconds (10), [&](){
    std::cout << "Routing table at Node A (after some time):" << std::endl;
    dvRouting.PrintRoutingTableAt (nodes.Get (0), Seconds (0));
    std::cout << "Routing table at Node B (after some time):" << std::endl;
    dvRouting.PrintRoutingTableAt (nodes.Get (1), Seconds (0));
    std::cout << "Routing table at Node C (after some time):" << std::endl;
    dvRouting.PrintRoutingTableAt (nodes.Get (2), Seconds (0));
  });

  Simulator::Stop (Seconds (12));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}