#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-dv-routing-helper.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TwoNodeLoopDVRouting");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("TwoNodeLoopDVRouting", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    InternetStackHelper stack;
    DvRoutingHelper dvRouting;
    stack.SetRoutingHelper(dvRouting);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    Ipv4Address destination("192.168.1.1");
    uint32_t port = 9;

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(destination, port));
    onoff.SetConstantRate(DataRate("1kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = onoff.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(15.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = sink.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(15.0));

    Ipv4DvRoutingTableEntry entry = Ipv4DvRoutingTableEntry::CreateNetworkRouteTo(destination, Ipv4Mask("/32"), interfaces.GetAddress(1), 1);
    dvRouting.AddNetworkRouteTo(nodes.Get(0), entry);
    dvRouting.AddNetworkRouteTo(nodes.Get(1), entry);

    Simulator::Schedule(Seconds(5.0), &DvRoutingHelper::InvalidateRoute, &dvRouting, nodes.Get(0), destination);
    Simulator::Schedule(Seconds(5.0), &DvRoutingHelper::InvalidateRoute, &dvRouting, nodes.Get(1), destination);

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("two-node-loop.tr");
    p2p.EnableAsciiAll(stream);

    p2p.EnablePcapAll("two-node-loop");

    Simulator::Stop(Seconds(15.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}