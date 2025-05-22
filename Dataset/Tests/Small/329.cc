#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HttpSimpleExample");

int main(int argc, char *argv[])
{
    // Enable log components
    LogComponentEnable("HttpSimpleExample", LOG_LEVEL_INFO);

    // Create nodes for the network
    NodeContainer nodes;
    nodes.Create(2); // Two nodes: Client and Server

    // Set up point-to-point link between nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes.Get(0), nodes.Get(1)); // Connection between node 0 and 1

    // Install internet stack (TCP/IP stack)
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up the HTTP server application on Node 1 (server)
    uint16_t port = 80; // Standard HTTP port
    OnOffHelper httpServerHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    httpServerHelper.SetConstantRate(DataRate("500kb/s"));
    ApplicationContainer serverApp = httpServerHelper.Install(nodes.Get(1)); // Install on Node 1 (Server)
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the HTTP client application on Node 0 (client)
    HttpRequestHelper httpClientHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    ApplicationContainer clientApp = httpClientHelper.Install(nodes.Get(0)); // Install on Node 0 (Client)
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
