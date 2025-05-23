#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/http-module.h"

// NS_LOG_COMPONENT_DEFINE("SimpleWiredHttp"); // Not needed as per output requirements

int main(int argc, char* argv[])
{
    // Configure default attributes for PointToPoint links
    // Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
    // Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));

    NodeContainer nodes;
    nodes.Create(2); // Node 0: Client, Node 1: Server

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

    // Node 1 is the HTTP server
    // Server listens on port 80
    // Server starts at 1 second and stops at 10 seconds
    HttpServerHelper serverHelper(80);
    ApplicationContainer serverApp = serverHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Node 0 is the HTTP client
    // Client connects to Node 1 (server) on port 80
    // Client starts generating traffic at 2 seconds and stops at 10 seconds
    Ipv4Address serverAddress = interfaces.GetAddress(1);
    HttpClientHelper clientHelper(serverAddress, 80);
    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Optional: Enable tracing
    // pointToPoint.EnablePcapAll("simple-wired-http");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}