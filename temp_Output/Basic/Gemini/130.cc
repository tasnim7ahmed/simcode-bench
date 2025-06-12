#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-nat.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NatSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("UdpClient", "Interval", StringValue ("1s"));

  NodeContainer privateNetworkNodes;
  privateNetworkNodes.Create (2);
  NodeContainer natRouter;
  natRouter.Create (1);
  NodeContainer publicNetworkServer;
  publicNetworkServer.Create (1);

  PointToPointHelper privateNetworkLink;
  privateNetworkLink.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  privateNetworkLink.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer privateDevices = privateNetworkLink.Install (privateNetworkNodes.Get (0), natRouter.Get (0));
  NetDeviceContainer privateDevices2 = privateNetworkLink.Install (privateNetworkNodes.Get (1), natRouter.Get (0));

  PointToPointHelper publicNetworkLink;
  publicNetworkLink.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  publicNetworkLink.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer publicDevices = publicNetworkLink.Install (natRouter.Get (0), publicNetworkServer.Get (0));

  InternetStackHelper stack;
  stack.Install (privateNetworkNodes);
  stack.Install (natRouter);
  stack.Install (publicNetworkServer);

  Ipv4AddressHelper privateNetworkAddress;
  privateNetworkAddress.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer privateInterfaces = privateNetworkAddress.Assign (privateDevices);
  Ipv4InterfaceContainer privateInterfaces2 = privateNetworkAddress.Assign (privateDevices2);

  Ipv4AddressHelper publicNetworkAddress;
  publicNetworkAddress.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer publicInterfaces = publicNetworkAddress.Assign (publicDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ipv4NatHelper natHelper;
  natHelper.SetInternalAddress ("10.1.1.0", "255.255.255.0");
  Ipv4Nat nat = natHelper.Install (natRouter.Get (0)).Get (0);

  UdpClientServerHelper clientServer (10000);
  clientServer.SetAttribute ("MaxPackets", UintegerValue (10));
  clientServer.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  clientServer.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps1 = clientServer.Install (privateNetworkNodes.Get (0), publicInterfaces.GetAddress (1), 10000);
  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop (Seconds (10.0));

  ApplicationContainer clientApps2 = clientServer.Install (privateNetworkNodes.Get (1), publicInterfaces.GetAddress (1), 10000);
  clientApps2.Start (Seconds (3.0));
  clientApps2.Stop (Seconds (11.0));

  ApplicationContainer serverApps = clientServer.Install (publicNetworkServer.Get (0), publicInterfaces.GetAddress (1), 10000);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (12.0));

  Simulator::Stop (Seconds (13.0));

  AnimationInterface anim ("nat-simulation.xml");
  anim.SetNodePosition (privateNetworkNodes.Get (0), 1, 1);
  anim.SetNodePosition (privateNetworkNodes.Get (1), 1, 3);
  anim.SetNodePosition (natRouter.Get (0), 3, 2);
  anim.SetNodePosition (publicNetworkServer.Get (0), 5, 2);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}