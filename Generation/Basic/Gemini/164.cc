#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ospf-helper.h"
#include "ns3/netanim-module.h"

int main(int argc, char* argv[])
{
    // Enable logging for OSPF related components and applications
    ns3::LogComponentEnable("OspfHelper", ns3::LOG_LEVEL_DEBUG);
    ns3::LogComponentEnable("OspfRoutingProtocol", ns3::LOG_LEVEL_DEBUG);
    ns3::LogComponentEnable("Ipv4L3Protocol", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);

    // Create 6 nodes for the topology
    ns3::NodeContainer nodes;
    nodes.Create(6);

    // Set up point-to-point helper for link attributes
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("2ms")); // Small delay to observe routing

    // Install Internet Stack with OSPF routing helper on all nodes
    // By default, OSPF will be configured for all interfaces added subsequently and placed into Area 0.0.0.0
    ns3::OspfHelper ospf;
    ns3::InternetStackHelper internetStack;
    internetStack.SetRoutingHelper(ospf);
    internetStack.Install(nodes);

    // Assign IP addresses helper
    ns3::Ipv4AddressHelper ipv4;

    // Create links between nodes and assign IP addresses
    ns3::NetDeviceContainer devices;
    ns3::Ipv4InterfaceContainer interfaces;

    // Link N0 <-> N1 (10.0.0.0/24)
    devices = p2p.Install(nodes.Get(0), nodes.Get(1));
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    interfaces = ipv4.Assign(devices);

    // Link N0 <-> N2 (10.0.1.0/24)
    devices = p2p.Install(nodes.Get(0), nodes.Get(2));
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    interfaces = ipv4.Assign(devices);

    // Link N1 <-> N3 (10.0.2.0/24)
    devices = p2p.Install(nodes.Get(1), nodes.Get(3));
    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    interfaces = ipv4.Assign(devices);

    // Link N2 <-> N4 (10.0.3.0/24)
    devices = p2p.Install(nodes.Get(2), nodes.Get(4));
    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    interfaces = ipv4.Assign(devices);

    // Link N3 <-> N5 (10.0.4.0/24)
    devices = p2p.Install(nodes.Get(3), nodes.Get(5));
    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    interfaces = ipv4.Assign(devices);
    // N5's IP on this link is 10.0.4.2

    // Link N4 <-> N5 (10.0.5.0/24)
    devices = p2p.Install(nodes.Get(4), nodes.Get(5));
    ipv4.SetBase("10.0.5.0", "255.255.255.0");
    interfaces = ipv4.Assign(devices);
    // N5's IP on this link is 10.0.5.2

    // Set up UDP Echo Server on Node 5 (N5)
    // N5 has two interfaces: 10.0.4.2 (from N3) and 10.0.5.2 (from N4).
    // The server will bind to port 9 on all its interfaces.
    ns3::UdpEchoServerHelper echoServer(9);
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(5));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // Set up UDP Echo Client on Node 0 (N0)
    // Client will send packets to N5's IP address on the 10.0.4.0 network (10.0.4.2)
    // N5's first non-loopback interface (index 1) corresponds to the link N3-N5 (10.0.4.x)
    ns3::Ptr<ns3::Ipv4> ipv4N5 = nodes.Get(5)->GetObject<ns3::Ipv4>();
    ns3::Ipv4Address serverAddress = ipv4N5->GetAddress(1, 0).GetLocal(); // Get IP of N5's interface connected to N3 (10.0.4.2)

    ns3::UdpEchoClientHelper echoClient(serverAddress, 9);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(5));
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024));
    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(2.0)); // Start client after OSPF has converged
    clientApps.Stop(ns3::Seconds(10.0));

    // Configure NetAnim for visualization
    ns3::NetAnimHelper anim;
    anim.EnableNetAnim("ospf_netanim.xml");
    anim.EnablePacketTracking();      // Visualize packet flow
    anim.EnableIpv4RouteTracking();   // Visualize routing table changes

    // Set node descriptions for NetAnim
    anim.SetNodeDescription(nodes.Get(0), "N0");
    anim.SetNodeDescription(nodes.Get(1), "N1");
    anim.SetNodeDescription(nodes.Get(2), "N2");
    anim.SetNodeDescription(nodes.Get(3), "N3");
    anim.SetNodeDescription(nodes.Get(4), "N4");
    anim.SetNodeDescription(nodes.Get(5), "N5");

    // Set node positions for a clear diamond/hourglass topology in NetAnim
    // N0 (top), N1, N2 (middle-top), N3, N4 (middle-bottom), N5 (bottom)
    anim.SetNodeX(nodes.Get(0), 50.0); anim.SetNodeY(nodes.Get(0), 100.0);
    anim.SetNodeX(nodes.Get(1), 25.0); anim.SetNodeY(nodes.Get(1), 75.0);
    anim.SetNodeX(nodes.Get(2), 75.0); anim.SetNodeY(nodes.Get(2), 75.0);
    anim.SetNodeX(nodes.Get(3), 0.0);  anim.SetNodeY(nodes.Get(3), 50.0);
    anim.SetNodeX(nodes.Get(4), 100.0); anim.SetNodeY(nodes.Get(4), 50.0);
    anim.SetNodeX(nodes.Get(5), 50.0); anim.SetNodeY(nodes.Get(5), 25.0);

    // Set simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(11.0));

    // Run the simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy(); // Clean up simulation resources

    return 0;
}