#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    // Node 0 <-> Node 2
    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));
    // Node 1 <-> Node 2
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0d2 = pointToPoint.Install(n0n2);
    NetDeviceContainer d1d2 = pointToPoint.Install(n1n2);

    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

    uint16_t port = 9;

    // Install TCP server (PacketSink) on Node 2
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Install TCP clients on Node 0 and Node 1
    // Client on Node 0 sends to Node 2 via 10.1.1.2
    OnOffHelper clientHelper0("ns3::TcpSocketFactory",
                              Address(InetSocketAddress(i0i2.GetAddress(1), port)));
    clientHelper0.SetAttribute("DataRate", StringValue("500Kbps"));
    clientHelper0.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper0.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    clientHelper0.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    clientHelper0.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer clientApp0 = clientHelper0.Install(nodes.Get(0));

    // Client on Node 1 sends to Node 2 via 10.1.2.2
    OnOffHelper clientHelper1("ns3::TcpSocketFactory",
                              Address(InetSocketAddress(i1i2.GetAddress(1), port)));
    clientHelper1.SetAttribute("DataRate", StringValue("500Kbps"));
    clientHelper1.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper1.SetAttribute("StartTime", TimeValue(Seconds(3.0)));
    clientHelper1.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    clientHelper1.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer clientApp1 = clientHelper1.Install(nodes.Get(1));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}