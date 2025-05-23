#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ripv2-routing-module.h"

// Using namespace ns3 is good practice for ns-3 programs.
using namespace ns3;

// NS_LOG_COMPONENT_DEFINE is used to enable logging for this specific program.
// Set environment variable NS_LOG=CountToInfinitySimulation=info to see logs.
NS_LOG_COMPONENT_DEFINE ("CountToInfinitySimulation");

/**
 * \brief Helper function to print the routing table for a given node.
 * \param node The node whose routing table is to be printed.
 * \param time The current simulation time for context.
 */
static void PrintRoutingTable (Ptr<Node> node, Time time)
{
    // Get the Ipv4 object from the node.
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    // Dynamically cast the Ipv4RoutingProtocol to Ripv2RoutingProtocol.
    // This allows access to RIP-specific methods like PrintRoutingTable.
    Ptr<Ripv2RoutingProtocol> rip = DynamicCast<Ripv2RoutingProtocol>(ipv4->GetRoutingProtocol());

    if (rip)
    {
        NS_LOG_INFO("--- Routing Table for Node " << node->GetId() << " at " << time << "s ---");
        // Print the routing table to the console.
        rip->PrintRoutingTable(Create<OutputStreamWrapper>(&std::cout, std::ios::out));
        NS_LOG_INFO("-------------------------------------------------");
    }
}

int main (int argc, char *argv[])
{
    // Configure RIPv2 properties. By default, ns-3 RIPv2 includes features
    // like triggered updates and split horizon with poison reverse, which
    // prevent the "count to infinity" problem. These default settings are
    // kept to demonstrate how modern RIP prevents the issue.
    Config::SetDefault ("ns3::Ripv2RoutingProtocol::AdvertiseRouteStop", BooleanValue (true));
    Config::SetDefault ("ns3::Ripv2RoutingProtocol::TriggeredUpdates", BooleanValue (true));
    Config::SetDefault ("ns3::Ripv2RoutingProtocol::UpdateInterval", TimeValue (Seconds (5)));
    Config::SetDefault ("ns3::Ripv2RoutingProtocol::Timeout", TimeValue (Seconds (180)));
    Config::SetDefault ("ns3::Ripv2RoutingProtocol::GarbageCollection", TimeValue (Seconds (120)));

    // Enable logging for RIP protocol messages and this simulation component.
    LogComponentEnable ("Ripv2RoutingProtocol", LOG_LEVEL_INFO);
    LogComponentEnable ("CountToInfinitySimulation", LOG_LEVEL_INFO);
    // LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_DEBUG); // Uncomment for more detailed IP layer logs

    // Create three nodes: A (Node 0), B (Node 1), C (Node 2)
    NodeContainer nodes;
    nodes.Create (3);

    // Install the Internet stack (including IPv4 and IP routing) on all nodes.
    InternetStackHelper internet;
    internet.Install (nodes);

    // Set up PointToPointHelper for network connections.
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    // Connect Node A (0) to Node B (1)
    NetDeviceContainer devicesAB;
    devicesAB = p2p.Install (nodes.Get (0), nodes.Get (1));

    // Connect Node B (1) to Node C (2). This is the link that will be broken.
    NetDeviceContainer devicesBC;
    devicesBC = p2p.Install (nodes.Get (1), nodes.Get (2));

    // Assign IP addresses to the interfaces.
    Ipv4AddressHelper ipv4;

    ipv4.SetBase ("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = ipv4.Assign (devicesAB); // A's IP on 192.168.1.x, B's IP on 192.168.1.x

    ipv4.SetBase ("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesBC = ipv4.Assign (devicesBC); // B's IP on 192.168.2.x, C's IP on 192.168.2.x

    // Add a dummy destination network (10.0.0.0/24) behind Node C (Node 2).
    // This host/network will be the target for routing table entries.
    NodeContainer hostC;
    hostC.Create(1);
    internet.Install(hostC);

    NetDeviceContainer deviceCHost;
    deviceCHost = p2p.Install(nodes.Get(2), hostC.Get(0));

    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaceCHost = ipv4.Assign(deviceCHost);

    // Configure RIPv2 routing protocols on all routers.
    Ripv2RoutingHelper ripRouting;
    ripRouting.Install (nodes); // Installs RIP on all nodes and attaches interfaces.

    // Get Ipv4 objects for each node to configure RIP interface metrics.
    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nodes.Get(2)->GetObject<Ipv4>();

    // Get the RIP protocol instances for each node.
    Ptr<Ripv2RoutingProtocol> ripA = DynamicCast<Ripv2RoutingProtocol>(ipv4A->GetRoutingProtocol());
    Ptr<Ripv2RoutingProtocol> ripB = DynamicCast<Ripv2RoutingProtocol>(ipv4B->GetRoutingProtocol());
    Ptr<Ripv2RoutingProtocol> ripC = DynamicCast<Ripv2RoutingProtocol>(ipv4C->GetRoutingProtocol());

    // Set interface metrics for RIP. Default is 1. Exclude loopback (index 0).
    // Get correct interface indices by querying the device.
    int32_t indexA_B = ipv4A->GetInterfaceForDevice(devicesAB.Get(0));
    int32_t indexB_A = ipv4B->GetInterfaceForDevice(devicesAB.Get(1));
    int32_t indexB_C = ipv4B->GetInterfaceForDevice(devicesBC.Get(0));
    int32_t indexC_B = ipv4C->GetInterfaceForDevice(devicesBC.Get(1));
    int32_t indexC_Host = ipv4C->GetInterfaceForDevice(deviceCHost.Get(0));

    ripA->SetInterfaceMetric(indexA_B, 1);
    ripB->SetInterfaceMetric(indexB_A, 1);
    ripB->SetInterfaceMetric(indexB_C, 1);
    ripC->SetInterfaceMetric(indexC_B, 1);
    ripC->SetInterfaceMetric(indexC_Host, 1);

    // Exclude loopback interfaces from RIP advertisements.
    ripA->ExcludeInterface(0);
    ripB->ExcludeInterface(0);
    ripC->ExcludeInterface(0);
    
    // Schedule printing of routing tables to observe convergence.
    // Initial state after 5 seconds for RIP to converge.
    Simulator::Schedule (Seconds (5.0), &PrintRoutingTable, nodes.Get(0), Seconds (5.0));
    Simulator::Schedule (Seconds (5.0), &PrintRoutingTable, nodes.Get(1), Seconds (5.0));
    Simulator::Schedule (Seconds (5.0), &PrintRoutingTable, nodes.Get(2), Seconds (5.0));
    
    // Schedule the link break between Node B (1) and Node C (2) at 10 seconds.
    // Both sides of the point-to-point link need to be brought down.
    Simulator::Schedule (Seconds (10.0), &Ipv4::SetDown, ipv4B, interfaceIdxB_C);
    Simulator::Schedule (Seconds (10.0), &Ipv4::SetDown, ipv4C, interfaceIdxC_B);
    NS_LOG_INFO ("*** Link B-C broken at 10.0s ***");

    // Schedule printing of routing tables after the link break, to observe changes.
    // 5 seconds after link break (at 15 seconds)
    Simulator::Schedule (Seconds (15.0), &PrintRoutingTable, nodes.Get(0), Seconds (15.0));
    Simulator::Schedule (Seconds (15.0), &PrintRoutingTable, nodes.Get(1), Seconds (15.0));
    Simulator::Schedule (Seconds (15.0), &PrintRoutingTable, nodes.Get(2), Seconds (15.0));

    // Schedule final printing of routing tables after more time, to ensure full convergence
    // and observe if 'count to infinity' would manifest (which RIPv2 prevents).
    // 15 seconds after link break (at 25 seconds)
    Simulator::Schedule (Seconds (25.0), &PrintRoutingTable, nodes.Get(0), Seconds (25.0));
    Simulator::Schedule (Seconds (25.0), &PrintRoutingTable, nodes.Get(1), Seconds (25.0));
    Simulator::Schedule (Seconds (25.0), &PrintRoutingTable, nodes.Get(2), Seconds (25.0));

    // Enable PCAP tracing for all devices to capture network traffic.
    p2p.EnablePcapAll ("count-to-infinity");

    // Set simulation stop time.
    Simulator::Stop (Seconds (30.0));
    // Run the simulation.
    Simulator::Run ();
    // Clean up simulation resources.
    Simulator::Destroy ();

    return 0;
}