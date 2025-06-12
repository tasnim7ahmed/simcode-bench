#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for applications
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes: client, router, server
    NodeContainer nodes;
    nodes.Create(3);

    // Point-to-point links: client<->router, router<->server
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("3ms"));

    NetDeviceContainer devicesA = p2p.Install(nodes.Get(0), nodes.Get(1)); // client-router
    NetDeviceContainer devicesB = p2p.Install(nodes.Get(1), nodes.Get(2)); // router-server

    // Install the internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper addressA;
    addressA.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesA = addressA.Assign(devicesA);

    Ipv4AddressHelper addressB;
    addressB.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesB = addressB.Assign(devicesB);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install TCP server (PacketSink) on node 2
    uint16_t port = 50000;
    Address sinkAddress(InetSocketAddress(interfacesB.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Install TCP client (BulkSend) on node 0
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited send
    ApplicationContainer clientApp = bulkSendHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}