#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 50000;

    // Install TCP server (PacketSink) on Node 0
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Install TCP client (OnOffApplication) on Node 1 targeting server at Node 0
    AddressValue serverAddress(InetSocketAddress(interfaces.GetAddress(0), port));
    OnOffHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("DataRate", StringValue("8192bps")); // 1024 bytes / 1s = 8192 bits/sec
    clientHelper.SetAttribute("MaxBytes", UintegerValue(1024*10)); // 10 packets
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.999]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.001]"));

    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}