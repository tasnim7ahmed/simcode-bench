#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpRoutedExample");

int main() {
    LogComponentEnable("TcpRoutedExample", LOG_LEVEL_INFO);

    // Create nodes (Client, Router, Server)
    NodeContainer clientRouter, routerServer;
    clientRouter.Create(2); // Client and Router
    routerServer.Add(clientRouter.Get(1)); // Reuse Router node
    routerServer.Create(1); // Server

    // Setup Point-to-Point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer clientRouterDevices = p2p.Install(clientRouter);
    NetDeviceContainer routerServerDevices = p2p.Install(routerServer);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(clientRouter);
    internet.Install(routerServer.Get(1)); // Server

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer clientRouterInterfaces = address.Assign(clientRouterDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer routerServerInterfaces = address.Assign(routerServerDevices);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup TCP Server (Server Node)
    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(routerServerInterfaces.GetAddress(1), port));

    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = sinkHelper.Install(routerServer.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Setup TCP Client (Client Node)
    BulkSendHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
    clientHelper.SetAttribute("MaxBytes", UintegerValue(1024)); // Send 1024 bytes

    ApplicationContainer clientApp = clientHelper.Install(clientRouter.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

