#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/http-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleHttpSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("SimpleHttpSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("HttpClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("HttpServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup PointToPoint link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up HTTP server on node 1 (server)
    HttpServerHelper serverHelper;
    ApplicationContainer serverApps = serverHelper.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up HTTP client on node 0 (client)
    HttpClientHelper clientHelper(interfaces.GetAddress(1), 80); // Server IP and port
    clientHelper.SetAttribute("MaxObjects", UintegerValue(10)); // Optional: limit number of objects
    ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable ASCII tracing
    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("simple-http-simulation.tr"));

    // Enable pcap tracing
    pointToPoint.EnablePcapAll("simple-http-simulation");

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}