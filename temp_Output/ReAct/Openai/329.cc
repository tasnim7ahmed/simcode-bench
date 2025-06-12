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
    nodes.Create(3);

    // Create two point-to-point links: 0<->2, 1<->2
    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0d2 = p2p.Install(n0n2);
    NetDeviceContainer d1d2 = p2p.Install(n1n2);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

    // Install TCP server (PacketSink) on Node 2, port 9
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(i0i2.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                      InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(2));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // TCP client on Node 0, connect to Node 2's 10.1.1.2
    OnOffHelper clientHelper0("ns3::TcpSocketFactory", InetSocketAddress(i0i2.GetAddress(1), port));
    clientHelper0.SetAttribute("DataRate", StringValue("500Kbps"));
    clientHelper0.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper0.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    clientHelper0.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer clientApps0 = clientHelper0.Install(nodes.Get(0));

    // TCP client on Node 1, connect to Node 2's 10.1.2.2
    OnOffHelper clientHelper1("ns3::TcpSocketFactory", InetSocketAddress(i1i2.GetAddress(1), port));
    clientHelper1.SetAttribute("DataRate", StringValue("500Kbps"));
    clientHelper1.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper1.SetAttribute("StartTime", TimeValue(Seconds(3.0)));
    clientHelper1.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer clientApps1 = clientHelper1.Install(nodes.Get(1));

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}