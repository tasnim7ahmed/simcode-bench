#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-list-routing-helper.h"
#include "ns3/ipv6-global-routing-helper.h" // For PrintRoutingTable (debug only, not strictly needed for final output)
#include "ns3/icmpv6-header.h"
#include "ns3/ipv6-l3-protocol.h"

using namespace ns3;

// Global Ptr for STA1's static routing to access it in callbacks
Ptr<Ipv6StaticRouting> g_sta1StaticRouting;

// Function to print IPv6 routing table of a node
void PrintIpv6RoutingTable (Ptr<Node> node)
{
    Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
    if (!ipv6)
    {
        NS_LOG_WARN ("Node " << node->GetId() << " does not have an IPv6 protocol installed.");
        return;
    }

    Ptr<Ipv6RoutingProtocol> ipv6rp = ipv6->GetRoutingProtocol();
    if (!ipv6rp)
    {
        NS_LOG_WARN ("Node " << node->GetId() << " does not have an IPv6 routing protocol installed.");
        return;
    }

    Ptr<Ipv6ListRouting> listRouting = DynamicCast<Ipv6ListRouting>(ipv6rp);
    if (!listRouting)
    {
        NS_LOG_WARN ("Node " << node->GetId() << " does not have an IPv6ListRouting protocol installed.");
        return;
    }

    // Attempt to get the static routing protocol.
    // If other protocols (e.g., global routing) are added, this might need adjustment.
    Ptr<Ipv6StaticRouting> staticRouting = 0;
    for (uint32_t i = 0; i < listRouting->GetNProtocols(); ++i)
    {
        staticRouting = DynamicCast<Ipv6StaticRouting>(listRouting->GetRoutingProtocol(i));
        if (staticRouting)
        {
            break;
        }
    }
    
    if (!staticRouting)
    {
        NS_LOG_WARN ("Node " << node->GetId() << " does not have an IPv6StaticRouting protocol in its list.");
        return;
    }

    std::cout << "Node " << node->GetId() << " IPv6 Routing Table at " << Simulator::Now().GetSeconds() << "s:" << std::endl;
    staticRouting->PrintRoutingTable (std::cout);
    std::cout << "---------------------------------------" << std::endl;
}

// Callback to handle received ICMPv6 Redirection messages
void RxRedirect (Ptr<const Packet> p, const Ipv6Header& header, const Icmpv6Header& icmpv6Header)
{
    if (icmpv6Header.GetType() == 137) // ICMPv6 Type 137 is Redirection
    {
        NS_LOG_INFO ("Time " << Simulator::Now().GetSeconds() << "s: STA1 received ICMPv6 Redirection from " << header.GetSourceAddress() << ".");
        // Schedule a print of STA1's routing table shortly after to see the update
        Simulator::Schedule (Seconds (0.01), &PrintIpv6RoutingTable, NodeList::GetNode (0)); // STA1 is node 0
    }
}


int main (int argc, char *argv[])
{
  // Enable logging for relevant components
  LogComponentEnable("Icmpv6RedirectSimulation", LOG_LEVEL_INFO);
  LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);
  LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_DEBUG); // Crucial for seeing redirection events
  LogComponentEnable("Ipv6StaticRouting", LOG_LEVEL_INFO);

  CommandLine cmd;
  cmd.Parse (argc, argv);

  // 1. Create nodes
  NodeContainer sta1Node, r1Node, r2Node, sta2Node;
  sta1Node.Create (1);
  r1Node.Create (1);
  r2Node.Create (1);
  sta2Node.Create (1);

  // 2. Install Internet Stack (IPv6) on all nodes
  InternetStackHelper internetv6StackHelper;
  internetv6StackHelper.SetIpv6Router (true); // R1 and R2 are routers
  internetv6StackHelper.Install (r1Node);
  internetv6StackHelper.Install (r2Node);
  internetv6StackHelper.SetIpv6Router (false); // STA1 and STA2 are hosts
  internetv6StackHelper.Install (sta1Node);
  internetv6StackHelper.Install (sta2Node);

  // 3. Create Network Devices and Channels
  // Link 1: Shared CSMA segment for STA1, R1, R2
  CsmaHelper csmaHelper;
  csmaHelper.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csmaHelper.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer link1Devices;
  link1Devices.Add (sta1Node.Get (0));
  link1Devices.Add (r1Node.Get (0));
  link1Devices.Add (r2Node.Get (0));
  link1Devices = csmaHelper.Install (link1Devices);

  // Link 2: Point-to-point link between R2 and STA2
  PointToPointHelper p2pHelper;
  p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pHelper.SetChannelAttribute ("Delay", StringValue ("10ms"));
  
  // Note: R2 already has an interface from Link 1. This will create a second interface for R2.
  NetDeviceContainer link2Devices = p2pHelper.Install (r2Node.Get (0), sta2Node.Get (0));


  // 4. Assign IPv6 Addresses
  Ipv6AddressHelper ipv6;

  // Link 1: 2001:db8:1::/64
  ipv6.SetBase (Ipv6Address ("2001:db8:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer link1Interfaces = ipv6.Assign (link1Devices);
  Ipv6Address sta1_link1_ip = link1Interfaces.GetAddress (0); // STA1's IP on Link 1
  Ipv6Address r1_link1_ip = link1Interfaces.GetAddress (1);   // R1's IP on Link 1
  Ipv6Address r2_link1_ip = link1Interfaces.GetAddress (2);   // R2's IP on Link 1

  // Link 2: 2001:db8:2::/64
  ipv6.SetBase (Ipv6Address ("2001:db8:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer link2Interfaces = ipv6.Assign (link2Devices);
  Ipv6Address r2_link2_ip = link2Interfaces.GetAddress (0);   // R2's IP on Link 2
  Ipv6Address sta2_link2_ip = link2Interfaces.GetAddress (1); // STA2's IP on Link 2

  NS_LOG_INFO("STA1 IP: " << sta1_link1_ip);
  NS_LOG_INFO("R1 (Link1) IP: " << r1_link1_ip);
  NS_LOG_INFO("R2 (Link1) IP: " << r2_link1_ip);
  NS_LOG_INFO("R2 (Link2) IP: " << r2_link2_ip);
  NS_LOG_INFO("STA2 IP: " << sta2_link2_ip);


  // 5. Routing Configuration
  Ipv6StaticRoutingHelper ipv6StaticRoutingHelper;

  // Get static routing protocols for nodes that need explicit routes
  Ptr<Ipv6ListRouting> sta1Routing = DynamicCast<Ipv6ListRouting> (sta1Node.Get (0)->GetObject<Ipv6RoutingProtocol>());
  g_sta1StaticRouting = ipv6StaticRoutingHelper.Get (sta1Routing); // Store global ptr for STA1

  Ptr<Ipv6ListRouting> r1Routing = DynamicCast<Ipv6ListRouting> (r1Node.Get (0)->GetObject<Ipv6RoutingProtocol>());
  Ptr<Ipv6StaticRouting> r1StaticRouting = ipv6StaticRoutingHelper.Get (r1Routing);

  Ptr<Ipv6ListRouting> sta2Routing = DynamicCast<Ipv6ListRouting> (sta2Node.Get (0)->GetObject<Ipv6RoutingProtocol>());
  Ptr<Ipv6StaticRouting> sta2StaticRouting = ipv6StaticRoutingHelper.Get (sta2Routing);

  // STA1 default route to R1
  // STA1 has only one interface (index 0) connected to Link 1
  g_sta1StaticRouting->AddDefaultRoute (r1_link1_ip, sta1Node.Get(0)->GetDevice(0)->GetIfIndex());

  // R1 static route to STA2's subnet via R2 (R1 has one interface (index 0) connected to Link 1)
  r1StaticRouting->AddHostRouteTo (sta2_link2_ip, r2_link1_ip, r1Node.Get(0)->GetDevice(0)->GetIfIndex());

  // STA2 default route to R2 (STA2 has one interface (index 0) connected to Link 2)
  sta2StaticRouting->AddDefaultRoute (r2_link2_ip, sta2Node.Get(0)->GetDevice(0)->GetIfIndex());

  // R2 is a router and directly connected to both subnets, so no static routes are strictly needed on R2 for this scenario.

  // 6. Trace setup
  AsciiTraceHelper ascii;
  // Enable tracing on both CSMA and PointToPoint links to the same file
  csmaHelper.EnableAsciiAll (ascii.CreateFileStream ("icmpv6-redirect.tr"));
  p2pHelper.EnableAsciiAll (ascii.CreateFileStream ("icmpv6-redirect.tr"));

  // 7. Configure Ping6 Application
  Ping6Helper ping6;
  ping6.SetRemote (sta2_link2_ip); // STA2's IP on Link 2
  ping6.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  ping6.SetAttribute ("Count", UintegerValue (3)); // Send 3 pings to observe before/after redirection

  ApplicationContainer pingApps = ping6.Install (sta1Node.Get (0)); // STA1 sends pings
  pingApps.Start (Seconds (1.0));
  pingApps.Stop (Seconds (10.0));

  // Connect trace source for ICMPv6 redirect on STA1
  Ptr<Ipv6L3Protocol> sta1Ipv6L3 = sta1Node.Get (0)->GetObject<Ipv6L3Protocol> ();
  sta1Ipv6L3->TraceConnectWithoutContext ("RxIcmpv6Redirect", MakeCallback (&RxRedirect));

  // Print initial routing tables
  Simulator::Schedule (Seconds (0.5), &PrintIpv6RoutingTable, sta1Node.Get (0));
  Simulator::Schedule (Seconds (0.5), &PrintIpv6RoutingTable, r1Node.Get (0));
  Simulator::Schedule (Seconds (0.5), &PrintIpv6RoutingTable, r2Node.Get (0));
  Simulator::Schedule (Seconds (0.5), &PrintIpv6RoutingTable, sta2Node.Get (0));

  // Simulate
  Simulator::Stop (Seconds (12.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}