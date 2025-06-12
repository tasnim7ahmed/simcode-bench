#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/rip.h"
#include "ns3/ipv4-global-routing.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipStarTopology");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application containers to log if set to true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  Time::SetResolution (Time::NS);
  LogComponentEnable ("Rip", LOG_LEVEL_ALL);

  NodeContainer routers;
  routers.Create (5);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer routerDevices[4];
  for(int i = 0; i < 4; ++i)
  {
    routerDevices[i] = pointToPoint.Install (routers.Get(0), routers.Get(i+1));
  }

  InternetStackHelper internet;
  internet.Install (routers);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer routerInterfaces[4];
  for(int i = 0; i < 4; ++i)
  {
    routerInterfaces[i] = address.Assign (routerDevices[i]);
    address.NewNetwork ();
  }

  Rip::Ipv4RoutingHelper ripRouting;
  GlobalRoutingHelper g;

  for (uint32_t i = 0; i < routers.GetN (); i++)
  {
    Ptr<Node> router = routers.Get (i);
    ripRouting.Install (router);
  }

  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (routers.Get (4));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (routerInterfaces[3].GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (routers.Get (1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  if (tracing)
    {
      pointToPoint.EnablePcapAll ("rip-star");
    }

  AnimationInterface anim ("rip-star.xml");
  anim.SetConstantPosition (routers.Get(0), 10, 10);
  anim.SetConstantPosition (routers.Get(1), 20, 20);
  anim.SetConstantPosition (routers.Get(2), 30, 20);
  anim.SetConstantPosition (routers.Get(3), 20, 30);
  anim.SetConstantPosition (routers.Get(4), 30, 30);

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}