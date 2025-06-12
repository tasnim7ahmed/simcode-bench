#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/http-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HttpWiredNetworkSimulation");

int main(int argc, char *argv[]) {
    // Enable logging for HTTP client and server
    LogComponentEnable("HttpClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("HttpServerApplication", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup PointToPoint link between the nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install HTTP Server on Node 1 (IP will be 10.1.1.2)
    HttpServerHelper serverHelper(80); // port 80
    ApplicationContainer serverApp = serverHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Install HTTP Client on Node 0
    HttpClientHelper clientHelper(interfaces.GetAddress(1), 80); // connect to server's IP and port
    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable packet tracing
    AsciiTraceHelper asciiTraceHelper;
    pointToPoint.EnableAsciiAll(asciiTraceHelper.CreateFileStream("http-wired-network.tr"));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}