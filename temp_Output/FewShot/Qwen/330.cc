#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/http-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for HTTP applications if needed
    LogComponentEnable("HttpClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("HttpServerApplication", LOG_LEVEL_INFO);

    // Create two nodes (Client and Server)
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up the HTTP Server on node 1
    uint16_t port = 80;  // HTTP port number
    HttpServerHelper httpServer(port);
    ApplicationContainer serverApp = httpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the HTTP Client on node 0
    HttpClientHelper httpClient(interfaces.GetAddress(1), port);
    ApplicationContainer clientApp = httpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}