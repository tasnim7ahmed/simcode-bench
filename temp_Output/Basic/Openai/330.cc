#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t httpPort = 80;

    // Install HTTP server (ns3::HttpServer) on node 1
    HttpServerHelper httpServer(httpPort);
    ApplicationContainer serverApp = httpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Install HTTP client (ns3::HttpClient) on node 0
    HttpClientHelper httpClient(interfaces.GetAddress(1), httpPort);
    // Generate regular HTTP GET requests with 1s interval
    httpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    httpClient.SetAttribute("RequestType", StringValue("GET"));
    httpClient.SetAttribute("MaxRequests", UintegerValue(100));
    ApplicationContainer clientApp = httpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}