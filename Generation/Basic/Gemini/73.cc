#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

int main(int argc, char *argv[])
{
    // Enable logging for relevant components
    ns3::LogComponentEnable("OnOffApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("Ipv4GlobalRouting", ns3::LOG_LEVEL_INFO);

    // Create three nodes: Router A, Router B, Router C
    ns3::NodeContainer routers;
    routers.Create(3);
    ns3::Ptr<ns3::Node> routerA = routers.Get(0);
    ns3::Ptr<ns3::Node> routerB = routers.Get(1);
    ns3::Ptr<ns3::Node> routerC = routers.Get(2);

    // Configure Point-to-Point Helper
    ns3::PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue("10Mbps"));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    // Create NetDevices for A-B link
    ns3::NetDeviceContainer abDevices;
    abDevices = p2pHelper.Install(routerA, routerB);

    // Create NetDevices for B-C link
    ns3::NetDeviceContainer bcDevices;
    bcDevices = p2pHelper.Install(routerB, routerC);

    // Install Internet stack on all nodes
    ns3::InternetStackHelper stack;
    stack.Install(routers);

    // Assign IP addresses to A-B link (subnet x.x.x.0/30, e.g., 10.1.1.0/30)
    ns3::Ipv4AddressHelper ipv4ab;
    ipv4ab.SetBase("10.1.1.0", "255.255.255.252"); // 10.1.1.1 for A, 10.1.1.2 for B
    ns3::Ipv4InterfaceContainer abInterfaces = ipv4ab.Assign(abDevices);

    // Assign IP addresses to B-C link (subnet y.y.y.0/30, e.g., 10.1.2.0/30)
    ns3::Ipv4AddressHelper ipv4bc;
    ipv4bc.SetBase("10.1.2.0", "255.255.255.252"); // 10.1.2.1 for B, 10.1.2.2 for C
    ns3::Ipv4InterfaceContainer bcInterfaces = ipv4bc.Assign(bcDevices);

    // Populate routing tables using Ipv4GlobalRoutingHelper
    // This will automatically configure routes between all connected subnets.
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up OnOff UDP application on Router A
    // Router A's IP (a.a.a.a/32) is its interface IP on the A-B link: 10.1.1.1
    // Router C's IP (c.c.c.c/32) is its interface IP on the B-C link: 10.1.2.2
    uint16_t sinkPort = 9; // Standard discard port

    // Create the remote address for the OnOff application (Router C's IP)
    ns3::Address sinkAddress = ns3::InetSocketAddress(bcInterfaces.GetAddress(1), sinkPort); // 10.1.2.2

    // OnOff application configuration
    ns3::OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); // Always on
    onoff.SetAttribute("DataRate", ns3::StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1024 bytes per packet

    // Install OnOff application on Router A
    ns3::ApplicationContainer clientApps = onoff.Install(routerA);
    clientApps.Start(ns3::Seconds(1.0)); // Start sending at 1 second
    clientApps.Stop(ns3::Seconds(9.0));  // Stop sending at 9 seconds

    // Set up Packet Sink application on Router C
    ns3::PacketSinkHelper sink("ns3::UdpSocketFactory",
                                ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), sinkPort)); // Listen on any IP address for the specified port

    // Install Packet Sink application on Router C
    ns3::ApplicationContainer serverApps = sink.Install(routerC);
    serverApps.Start(ns3::Seconds(0.0)); // Start listening immediately
    serverApps.Stop(ns3::Seconds(10.0)); // Stop listening at 10 seconds

    // Configure packet tracing
    // Generates pcap files for each network device (e.g., three-router-network-0-0.pcap)
    p2pHelper.EnablePcapAll("three-router-network");

    // Run the simulation for 10 seconds
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}