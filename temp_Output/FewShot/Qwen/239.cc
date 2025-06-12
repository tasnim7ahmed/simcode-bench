#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create three nodes: client, router, server
    NodeContainer nodes;
    nodes.Create(3);

    // Point-to-point helper for links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link client to router
    NetDeviceContainer clientRouterDevices = p2p.Install(nodes.Get(0), nodes.Get(1));

    // Link router to server
    NetDeviceContainer routerServerDevices = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    // Client-router subnet
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer clientInterfaces = address.Assign(clientRouterDevices);

    // Router-server subnet
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer serverInterfaces = address.Assign(routerServerDevices);

    // Set up TCP receiver (server)
    uint16_t port = 50000;
    Address serverAddress(InetSocketAddress(serverInterfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP sender (client)
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", serverAddress);
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    onOffHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Continuous transmission

    ApplicationContainer clientApp = onOffHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable logging for TCP
    LogComponentEnable("TcpL4Protocol", LOG_LEVEL_INFO);

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}