#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes: client, router, server
    NodeContainer nodes;
    nodes.Create(3);

    // Create P2P links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // NetDevice containers for each subnet
    NetDeviceContainer devClientRouter = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devRouterServer = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceClientRouter = address.Assign(devClientRouter);

    address.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceRouterServer = address.Assign(devRouterServer);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up TCP server on node 2 (server)
    uint16_t port = 50000;
    Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(2));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP client on node 0 (client) - BulkSendApplication
    BulkSendHelper bulkSender("ns3::TcpSocketFactory",
        InetSocketAddress(ifaceRouterServer.GetAddress(1), port));
    bulkSender.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited
    ApplicationContainer clientApp = bulkSender.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Enable ASCII tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("tcp-p2p-router.tr"));

    // Enable PCAP tracing
    p2p.EnablePcapAll("tcp-p2p-router", false);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}