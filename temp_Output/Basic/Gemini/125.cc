#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/rip-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipStar");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("Rip", AttributeValue (BooleanValue (true)));
  LogComponent::SetAttribute ("RipRoutingProtocol", AttributeValue (BooleanValue (true)));

  NodeContainer mainRouter;
  mainRouter.Create (1);

  NodeContainer subnetRouters;
  subnetRouters.Create (4);

  NodeContainer allNodes;
  allNodes.Add (mainRouter);
  allNodes.Add (subnetRouters);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer mainRouterDevices[4];
  NetDeviceContainer subnetRouterDevices[4];

  for (int i = 0; i < 4; ++i)
    {
      NetDeviceContainer devices = p2p.Install (mainRouter.Get (0), subnetRouters.Get (i));
      mainRouterDevices[i] = NetDeviceContainer (devices.Get (0));
      subnetRouterDevices[i] = NetDeviceContainer (devices.Get (1));
    }

  InternetStackHelper internet;
  internet.Install (allNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer mainRouterInterfaces[4];
  Ipv4InterfaceContainer subnetRouterInterfaces[4];

  for (int i = 0; i < 4; ++i)
    {
      Ipv4InterfaceContainer interfaces = ipv4.Assign (NetDeviceContainer (mainRouterDevices[i], subnetRouterDevices[i]));
      mainRouterInterfaces[i] = Ipv4InterfaceContainer (interfaces.Get (0));
      subnetRouterInterfaces[i] = Ipv4InterfaceContainer (interfaces.Get (1));
      ipv4.NewNetwork ();
    }

  RipHelper ripRouting;
  ripRouting.ExcludeInterface (mainRouter.Get (0), mainRouterInterfaces[0].Get (0));
  ripRouting.ExcludeInterface (mainRouter.Get (0), mainRouterInterfaces[1].Get (0));
  ripRouting.ExcludeInterface (mainRouter.Get (0), mainRouterInterfaces[2].Get (0));
  ripRouting.ExcludeInterface (mainRouter.Get (0), mainRouterInterfaces[3].Get (0));

  Address sinkAddress1 (InetSocketAddress (subnetRouterInterfaces[0].GetAddress (0), 8080));
  PacketSinkHelper packetSinkHelper1 ("ns3::TcpSocketFactory", sinkAddress1);
  ApplicationContainer sinkApps1 = packetSinkHelper1.Install (subnetRouters.Get (0));
  sinkApps1.Start (Seconds (1.0));
  sinkApps1.Stop (Seconds (10.0));

  Address sinkAddress2 (InetSocketAddress (subnetRouterInterfaces[1].GetAddress (0), 8080));
  PacketSinkHelper packetSinkHelper2 ("ns3::TcpSocketFactory", sinkAddress2);
  ApplicationContainer sinkApps2 = packetSinkHelper2.Install (subnetRouters.Get (1));
  sinkApps2.Start (Seconds (1.0));
  sinkApps2.Stop (Seconds (10.0));

  Address sinkAddress3 (InetSocketAddress (subnetRouterInterfaces[2].GetAddress (0), 8080));
  PacketSinkHelper packetSinkHelper3 ("ns3::TcpSocketFactory", sinkAddress3);
  ApplicationContainer sinkApps3 = packetSinkHelper3.Install (subnetRouters.Get (2));
  sinkApps3.Start (Seconds (1.0));
  sinkApps3.Stop (Seconds (10.0));

  Address sinkAddress4 (InetSocketAddress (subnetRouterInterfaces[3].GetAddress (0), 8080));
  PacketSinkHelper packetSinkHelper4 ("ns3::TcpSocketFactory", sinkAddress4);
  ApplicationContainer sinkApps4 = packetSinkHelper4.Install (subnetRouters.Get (3));
  sinkApps4.Start (Seconds (1.0));
  sinkApps4.Stop (Seconds (10.0));


  V4PingHelper ping (subnetRouterInterfaces[0].GetAddress (0));
  ping.SetAttribute ("Verbose", BooleanValue (true));
  ApplicationContainer apps = ping.Install (subnetRouters.Get (1));
  apps.Start (Seconds (2));
  apps.Stop (Seconds (9));

  V4PingHelper ping2 (subnetRouterInterfaces[1].GetAddress (0));
  ping2.SetAttribute ("Verbose", BooleanValue (true));
  ApplicationContainer apps2 = ping2.Install (subnetRouters.Get (2));
  apps2.Start (Seconds (2));
  apps2.Stop (Seconds (9));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  AnimationInterface anim ("rip-star.xml");
  anim.SetConstantPosition (mainRouter.Get (0), 10, 10);
  anim.SetConstantPosition (subnetRouters.Get (0), 20, 20);
  anim.SetConstantPosition (subnetRouters.Get (1), 30, 20);
  anim.SetConstantPosition (subnetRouters.Get (2), 20, 30);
  anim.SetConstantPosition (subnetRouters.Get (3), 30, 30);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}