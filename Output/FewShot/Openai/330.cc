#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/http-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    LogComponentEnable("HttpClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("HttpServerApplication", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up point-to-point link
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up HTTP server on Node 1
    uint16_t port = 80;
    HttpServerHelper httpServer(port);
    ApplicationContainer serverApp = httpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up HTTP client on Node 0
    HttpClientHelper httpClient(interfaces.GetAddress(1), port);
    httpClient.SetAttribute("RequestInterval", TimeValue(Seconds(1.0)));
    httpClient.SetAttribute("RequestSize", UintegerValue(512));
    httpClient.SetAttribute("MaxRequests", UintegerValue(8));
    ApplicationContainer clientApp = httpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}