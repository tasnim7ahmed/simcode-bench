#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/tcp-header.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

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

    uint16_t port = 5000;
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    source.SetAttribute("SendSize", UintegerValue(1024));

    ApplicationContainer sourceApps = source.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // Enable pcap tracing
    pointToPoint.EnablePcapAll("tcp-newreno");

    // Enable NetAnim
    AnimationInterface anim("tcp-newreno.xml");
    anim.SetConstantPosition(nodes.Get(0), 1, 1);
    anim.SetConstantPosition(nodes.Get(1), 2, 2);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}