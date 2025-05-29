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
  LogComponent::SetFirstLevel (LOG_LEVEL_INFO);

  NodeContainer routers;
  routers.Create (3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devicesAB, devicesBC, devicesCA;
  devicesAB = p2p.Install (routers.Get (0), routers.Get (1));
  devicesBC = p2p.Install (routers.Get (1), routers.Get (2));
  devicesCA = p2p.Install (routers.Get (0), routers.Get (2));

  InternetStackHelper internet;
  internet.Install (routers);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAB = ipv4.Assign (devicesAB);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesBC = ipv4.Assign (devicesBC);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesCA = ipv4.Assign (devicesCA);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Schedule (Seconds (1.0), [](){
    NS_LOG_INFO ("Initial Routing Tables:");
    Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt (Seconds (1.0));
  });

  Simulator::Schedule (Seconds (5.0), [&](){
      NS_LOG_INFO ("Breaking link between B and C");
      devicesBC.Get (0)->GetObject<PointToPointNetDevice> ()->GetChannel ()->Dispose ();
      devicesBC.Get (1)->GetObject<PointToPointNetDevice> ()->GetChannel ()->Dispose ();
  });

  Simulator::Schedule (Seconds (6.0), [](){
      NS_LOG_INFO ("Routing Tables after breaking link:");
      Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt (Seconds (6.0));
  });

  Simulator::Schedule (Seconds (10.0), [](){
      NS_LOG_INFO ("Routing Tables after some time:");
      Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt (Seconds (10.0));
  });

  Simulator::Stop (Seconds (15.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}