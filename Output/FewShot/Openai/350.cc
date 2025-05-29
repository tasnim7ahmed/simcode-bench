#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Point-to-point configuration
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // TCP server (PacketSink) on Node 0
    uint16_t port = 50000;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(0), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // TCP client (OnOffApplication) on Node 1
    OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("DataRate", StringValue("5Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(1024 * 10));

    // Each packet is 1024 bytes, 10 packets total. We'll schedule based on interval.
    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}