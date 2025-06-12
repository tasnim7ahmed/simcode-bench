#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RoutingExample");

int main(int argc, char* argv[]) {
    bool verbose = false;
    uint32_t maxBytes = 0;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if true", verbose);
    cmd.AddValue("maxBytes", "Total number of bytes for application to send", maxBytes);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices02;
    devices02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));

    NetDeviceContainer devices12;
    devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("100ms"));

    NetDeviceContainer devices13;
    devices13 = pointToPoint.Install(nodes.Get(1), nodes.Get(3));

    PointToPointHelper pointToPoint23;
    pointToPoint23.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    pointToPoint23.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices23;
    devices23 = pointToPoint23.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces13 = address.Assign(devices13);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set link cost for n1-n3 link to a high value to encourage routing through n2
    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4StaticRouting> routingTable1 = staticRouting.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    routingTable1->SetRouteToNetwork(Ipv4Address("10.1.3.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.3.2"), 20); // Metric 20

    uint16_t port = 9; // well-known echo port number
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(interfaces12.GetAddress(1), port);
    client.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    ApplicationContainer clientApps = client.Install(nodes.Get(3));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable tracing
    pointToPoint.EnablePcapAll("routing_example");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}