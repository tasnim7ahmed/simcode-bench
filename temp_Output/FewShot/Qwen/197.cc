#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-header.h"

using namespace ns3;

// Packet counters
uint32_t txPacketsNode1 = 0;
uint32_t rxPacketsNode3 = 0;
uint32_t forwardedPacketsRouter = 0;

void TxPacketLogger(Ptr<const Packet> packet) {
    txPacketsNode1++;
}

void RxPacketLogger(Ptr<const Packet> packet) {
    rxPacketsNode3++;
}

void ForwardPacketLogger(Ptr<const Packet> packet) {
    forwardedPacketsRouter++;
}

int main(int argc, char *argv[]) {
    // Enable logging for debugging if needed
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create three nodes: Node 0 (source), Node 1 (router), Node 2 (destination)
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link Node 0 <-> Node 1
    NetDeviceContainer devices0_1 = p2p.Install(nodes.Get(0), nodes.Get(1));
    // Link Node 1 <-> Node 2
    NetDeviceContainer devices1_2 = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces0_1 = address.Assign(devices0_1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1_2 = address.Assign(devices1_2);

    // Set up routing so Node 0 can reach Node 2 via Node 1 (router)
    Ipv4StaticRoutingHelper ipv4RoutingHelper;

    Ptr<Ipv4> ipv4_0 = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routing_0 = ipv4RoutingHelper.GetStaticRouting(ipv4_0);
    routing_0->AddNetworkRouteTo(Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"), 1); // via Node 1

    Ptr<Ipv4> ipv4_2 = nodes.Get(2)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routing_2 = ipv4RoutingHelper.GetStaticRouting(ipv4_2);
    routing_2->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), 1); // via Node 1

    // Install TCP sink (receiver) on Node 2
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces1_2.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Install TCP source (sender) on Node 0
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", sinkAddress);
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    onOffHelper.SetAttribute("MaxBytes", UintegerValue(0)); // continuous transmission

    ApplicationContainer clientApp = onOffHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable packet tracing
    Config::Connect("/NodeList/0/ApplicationList/0/Tx", MakeCallback(&TxPacketLogger));
    Config::Connect("/NodeList/2/ApplicationList/0/Rx", MakeCallback(&RxPacketLogger));
    Config::Connect("/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/MacRx", MakeCallback(&ForwardPacketLogger));

    // Enable pcap tracing for all devices
    p2p.EnablePcapAll("network_trace");

    // Run the simulation
    Simulator::Run();

    // Print final packet counts
    std::cout << "Node 0 (Source) transmitted packets: " << txPacketsNode1 << std::endl;
    std::cout << "Node 2 (Destination) received packets: " << rxPacketsNode3 << std::endl;
    std::cout << "Node 1 (Router) forwarded packets: " << forwardedPacketsRouter << std::endl;

    Simulator::Destroy();
    return 0;
}