#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 3 nodes: Node 0 (client), Node 1 (router), Node 2 (server)
    NodeContainer nodes;
    nodes.Create(3);

    // Define point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    // Install link between Node 0 and Node 1
    NetDeviceContainer devices0_1 = p2p.Install(nodes.Get(0), nodes.Get(1));

    // Install link between Node 1 and Node 2
    NetDeviceContainer devices1_2 = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet Stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces0_1 = address.Assign(devices0_1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1_2 = address.Assign(devices1_2);

    // Set up routing so that Node 0 can reach Node 2 through Node 1
    Ipv4StaticRoutingHelper ipv4RoutingHelper;

    Ptr<Ipv4> ipv4_0 = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> ipv4StaticRouting_0 = ipv4RoutingHelper.GetStaticRouting(ipv4_0);
    // Route to Node 2 via Node 1
    ipv4StaticRouting_0->AddNetworkRouteTo(Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"), 2); // Next hop interface index

    Ptr<Ipv4> ipv4_2 = nodes.Get(2)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> ipv4StaticRouting_2 = ipv4RoutingHelper.GetStaticRouting(ipv4_2);
    // Reverse route to Node 0 via Node 1
    ipv4StaticRouting_2->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), 1);

    // Install TCP Server on Node 2
    uint16_t port = 5000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Install TCP Client on Node 0
    AddressValue remoteAddress(InetSocketAddress(interfaces1_2.GetAddress(1), port));
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", remoteAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Send continuously until simulation ends
    ApplicationContainer clientApp = bulkSend.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    p2p.EnablePcapAll("tcp_p2p_simulation");

    // Setup FlowMonitor to collect throughput and packet loss statistics
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    // Run the simulation
    Simulator::Run();

    // Print FlowMonitor results
    flowMonitor->CheckForLostPackets();
    std::cout << "\n\nFlow Monitor Results:\n";
    flowMonitor->SerializeToXmlFile("tcp_p2p_simulation.flowmon", true, true);

    Simulator::Destroy();

    return 0;
}