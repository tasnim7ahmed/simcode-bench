#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // TCP Server (PacketSink) on node 1
    uint16_t port = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));

    // TCP Client (OnOffApplication) on node 0
    OnOffHelper client("ns3::TcpSocketFactory", sinkAddress);
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("DataRate", StringValue("8kbps")); // Will be throttled by OnTime/OffTime
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(12.0)); // Give enough time for last packet

    // Schedule stopping after 10 packets (1 second interval)
    uint32_t numPackets = 10;
    Ptr<Application> app = clientApps.Get(0);
    for (uint32_t i = 1; i <= numPackets; ++i) {
        Simulator::Schedule(Seconds(2.0 + i - 1), &OnOffApplication::StartApplication, DynamicCast<OnOffApplication>(app));
        Simulator::Schedule(Seconds(2.0 + i - 1 + 0.001), &OnOffApplication::StopApplication, DynamicCast<OnOffApplication>(app));
    }
    // Stop simulation at 10s
    Simulator::Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}