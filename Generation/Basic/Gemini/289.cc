#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// NS_LOG_COMPONENT_DEFINE ("P2PTcpSimulation"); // Not needed if no logs are explicitly enabled

int main (int argc, char *argv[])
{
    // Configure TCP variant (e.g., NewReno)
    // By default, ns-3.41 uses TcpNewReno or TcpCubic. Explicitly setting for clarity.
    ns3::Config::SetDefault ("ns3::TcpL4Protocol::SocketType", ns3::StringValue ("ns3::TcpNewReno"));
    
    // Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create (2); // Node 0 and Node 1

    // Create Point-to-Point helper and set link attributes
    ns3::PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute ("DataRate", ns3::StringValue ("10Mbps"));
    p2pHelper.SetChannelAttribute ("Delay", ns3::StringValue ("2ms"));

    // Install devices on nodes
    ns3::NetDeviceContainer devices = p2pHelper.Install (nodes);

    // Install the Internet stack on nodes
    ns3::InternetStackHelper stack;
    stack.Install (nodes);

    // Assign IP addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // --- TCP Server (Node 1) ---
    uint16_t port = 5000;
    ns3::PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory",
                                           ns3::InetSocketAddress (ns3::Ipv4Address::GetAny (), port));
    ns3::ApplicationContainer serverApps = packetSinkHelper.Install (nodes.Get (1)); // Node 1 is the server
    serverApps.Start (ns3::Seconds (0.0));
    serverApps.Stop (ns3::Seconds (10.0));

    // --- TCP Client (Node 0) ---
    ns3::OnOffHelper onoff ("ns3::TcpSocketFactory", ns3::Address ()); // Address will be set later
    onoff.SetAttribute ("OnTime", ns3::StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute ("OffTime", ns3::StringValue ("ns3::ConstantRandomVariable[Constant=0]")); // Always on
    onoff.SetAttribute ("PacketSize", ns3::UintegerValue (1024)); // 1024-byte packets
    onoff.SetAttribute ("DataRate", ns3::StringValue ("5Mbps")); // 5Mbps application rate

    // Set the remote address for the client (Node 1's IP and server port)
    ns3::Address serverAddress (ns3::InetSocketAddress (interfaces.GetAddress (1), port));
    onoff.SetAttribute ("Remote", ns3::AddressValue (serverAddress));

    ns3::ApplicationContainer clientApps = onoff.Install (nodes.Get (0)); // Node 0 is the client
    clientApps.Start (ns3::Seconds (1.0)); // Start client after server is ready
    clientApps.Stop (ns3::Seconds (10.0));

    // Enable PCAP tracing on all devices in the Point-to-Point network
    p2pHelper.EnablePcapAll ("p2p-tcp");

    // Set simulation stop time
    ns3::Simulator::Stop (ns3::Seconds (10.0));

    // Run the simulation
    ns3::Simulator::Run ();
    ns3::Simulator::Destroy ();

    return 0;
}