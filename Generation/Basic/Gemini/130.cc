#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/nat-module.h"
#include "ns3/point-to-point-layout-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NatSimulation");

int main (int argc, char *argv[])
{
  LogComponentEnable ("NatSimulation", LOG_LEVEL_INFO);
  LogComponentEnable ("Ipv4Nat", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_ALL); // To see routing table lookups
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);


  // 1. Create Nodes
  NodeContainer privateNodes;
  privateNodes.Create (2); // HostA, HostB
  Ptr<Node> natRouter = CreateObject<Node> ();
  NodeContainer publicNodes;
  publicNodes.Create (1); // External Server

  // Set node names for easier debugging (optional)
  Names::Add ("HostA", privateNodes.Get (0));
  Names::Add ("HostB", privateNodes.Get (1));
  Names::Add ("NatRouter", natRouter);
  Names::Add ("ExternalServer", publicNodes.Get (0));

  // 2. Install Internet Stack
  InternetStackHelper stack;
  stack.Install (privateNodes);
  stack.Install (natRouter);
  stack.Install (publicNodes);

  // 3. Create Point-to-Point Links and Devices
  PointToPointHelper p2pHelper;
  p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2pHelper.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Private Network: Star topology with NAT Router as center
  // Two hosts connected to the NAT router, acting as a small private network segment
  PointToPointStarHelper privateStar (privateNodes.GetN () + 1, p2pHelper); 
  privateStar.SetCentralNode (natRouter);
  for (uint32_t i = 0; i < privateNodes.GetN (); ++i)
  {
      privateStar.SetSpokeNode (i, privateNodes.Get (i));
  }
  // The InstallStack is implicitly done by SetCentralNode/SetSpokeNode, but devices are not assigned IPs yet.

  // Link: NAT Router (public side) to External Server
  NetDeviceContainer publicDevicesRouterServer;
  publicDevicesRouterServer = p2pHelper.Install (NodeContainer (natRouter, publicNodes.Get (0)));

  // 4. Assign IP Addresses
  Ipv4AddressHelper privateIpv4;
  privateIpv4.SetBase ("10.0.0.0", "255.255.255.0"); // Private network subnet
  // Assign IPs to the devices created by privateStar helper.
  // The order is typically: spoke_device_0, spoke_device_1, ..., central_device_0, central_device_1, ...
  // So, 10.0.0.1 -> HostA's device, 10.0.0.2 -> HostB's device, 10.0.0.3 -> Router's device to HostA, 10.0.0.4 -> Router's device to HostB.
  Ipv4InterfaceContainer privateIfs = privateIpv4.Assign (privateStar.GetNetDeviceContainer ());

  Ipv4AddressHelper publicIpv4;
  publicIpv4.SetBase ("20.0.0.0", "255.255.255.0"); // Public network subnet
  Ipv4InterfaceContainer publicIfsRouter = publicIpv4.Assign (publicDevicesRouterServer.Get (0)); // Router's public IP (e.g., 20.0.0.1)
  Ipv4InterfaceContainer publicIfsServer = publicIpv4.Assign (publicDevicesRouterServer.Get (1)); // Server's IP (e.g., 20.0.0.2)

  // 5. Configure NAT
  Ptr<Ipv4> natRouterIpv4 = natRouter->GetObject<Ipv4>();
  
  // Get interface indexes on the NAT router
  // The privateStarHelper creates two interfaces on the router (one for HostA, one for HostB).
  // We mark one of them as "private" for the NAT, it will apply to traffic going through it.
  // privateStar.GetHubNetDevice(0) returns the first device of the central node (router).
  int32_t privateIfIndex = natRouterIpv4->GetInterfaceForDevice(privateStar.GetHubNetDevice (0)); 
  int32_t publicIfIndex = natRouterIpv4->GetInterfaceForDevice(publicDevicesRouterServer.Get (0)); 

  NS_ASSERT (privateIfIndex >= 0 && publicIfIndex >= 0);

  Ipv4NatHelper natHelper;
  // Mark the interfaces as "inside" and "outside" for the NAT configuration.
  // The default NAT behavior for "Router" mode applies SNAT for outbound traffic
  // and corresponding DNAT for inbound established connections.
  natRouterIpv4->SetAttribute ("NatPrivate", UintegerValue (privateIfIndex));
  natRouterIpv4->SetAttribute ("NatPublic", UintegerValue (publicIfIndex));

  // Install NAT on the router node
  natHelper.Install (natRouter);

  // 6. Configure Applications
  // Server Application
  UdpEchoServerHelper echoServer (9); // Port 9 (discard)
  ApplicationContainer serverApps = echoServer.Install (publicNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Client Applications (from private hosts to public server)
  // HostA client
  UdpEchoClientHelper echoClientA (publicIfsServer.GetAddress (0), 9); // Target server's public IP
  echoClientA.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClientA.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClientA.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientAppsA = echoClientA.Install (privateNodes.Get (0));
  clientAppsA.Start (Seconds (2.0));
  clientAppsA.Stop (Seconds (10.0));

  // HostB client
  UdpEchoClientHelper echoClientB (publicIfsServer.GetAddress (0), 9); // Target server's public IP
  echoClientB.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClientB.SetAttribute ("Interval", TimeValue (Seconds (1.5))); // Offset start time
  echoClientB.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientAppsB = echoClientB.Install (privateNodes.Get (1));
  clientAppsB.Start (Seconds (3.0));
  clientAppsB.Stop (Seconds (10.0));

  // 7. Populate Routing Tables
  // This is crucial after setting up NAT interfaces to ensure proper packet forwarding
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Add static routes for private hosts to use the NAT router as their default gateway
  // HostA's IP: privateIfs.GetAddress (0) -> 10.0.0.1
  // HostB's IP: privateIfs.GetAddress (1) -> 10.0.0.2
  // Router's IP to HostA: privateIfs.GetAddress (2) -> 10.0.0.3
  // Router's IP to HostB: privateIfs.GetAddress (3) -> 10.0.0.4

  Ptr<Ipv4StaticRouting> privateHostARouting = Ipv4GlobalRoutingHelper::Get
    <Ipv4StaticRouting> (privateNodes.Get (0)->GetObject<Ipv4> ());
  privateHostARouting->SetDefaultRoute (privateIfs.GetAddress (2), // Router's IP on the link to HostA
                                        privateNodes.Get (0)->GetObject<Ipv4> ()->GetInterfaceForDevice (privateStar.GetSpokeNetDevice (0))); // HostA's interface index

  Ptr<Ipv4StaticRouting> privateHostBRouting = Ipv4GlobalRoutingHelper::Get
    <Ipv4StaticRouting> (privateNodes.Get (1)->GetObject<Ipv4> ());
  privateHostBRouting->SetDefaultRoute (privateIfs.GetAddress (3), // Router's IP on the link to HostB
                                        privateNodes.Get (1)->GetObject<Ipv4> ()->GetInterfaceForDevice (privateStar.GetSpokeNetDevice (1))); // HostB's interface index

  // 8. Enable PCAP Tracing for Verification
  // Enable PCAP for specific devices to verify NAT functionality
  // Private hosts' traffic
  privateStar.EnablePcap ("nat-simulation-hostA", privateNodes.Get (0));
  privateStar.EnablePcap ("nat-simulation-hostB", privateNodes.Get (1));

  // Router's private and public interfaces
  privateStar.EnablePcap ("nat-simulation-router-private", natRouter); // Captures on router's private interfaces
  p2pHelper.EnablePcap ("nat-simulation-router-public", natRouter->GetDevice (publicIfIndex), true);

  // External Server's interface
  p2pHelper.EnablePcap ("nat-simulation-server", publicNodes.Get (0)->GetDevice (0), true);

  // Run Simulation
  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}