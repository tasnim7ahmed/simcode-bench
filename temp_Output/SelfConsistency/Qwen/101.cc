#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/global-route-manager.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FourNodeSimulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create four nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Define links: n0-n2 and n1-n2 with 5 Mbps, 2 ms; n2-n3 with 1.5 Mbps, 10 ms
    PointToPointHelper p2p_n0n2;
    p2p_n0n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_n0n2.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p_n1n2;
    p2p_n1n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_n1n2.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p_n2n3;
    p2p_n2n3.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p_n2n3.SetChannelAttribute("Delay", StringValue("10ms"));

    // Install internet stacks on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Create devices and channels
    NetDeviceContainer dev_n0n2 = p2p_n0n2.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer dev_n1n2 = p2p_n1n2.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer dev_n2n3 = p2p_n2n3.Install(nodes.Get(2), nodes.Get(3));

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n0n2 = address.Assign(dev_n0n2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n1n2 = address.Assign(dev_n1n2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n2n3 = address.Assign(dev_n2n3);

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // CBR/UDP traffic from n0 to n3
    uint16_t cbrPort = 9;
    Address sinkAddress(InetSocketAddress(if_n2n3.GetAddress(1), cbrPort)); // n3's address
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), cbrPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(3));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(2.0));

    OnOffHelper onOffHelper("ns3::UdpSocketFactory", sinkAddress);
    onOffHelper.SetConstantRate(DataRate("448kbps"), 210);
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer srcApps = onOffHelper.Install(nodes.Get(0));
    srcApps.Start(Seconds(0.5));
    srcApps.Stop(Seconds(2.0));

    // CBR/UDP traffic from n3 to n1
    Address sinkAddress2(InetSocketAddress(if_n1n2.GetAddress(1), cbrPort)); // n1's address
    PacketSinkHelper packetSinkHelper2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), cbrPort));
    ApplicationContainer sinkApps2 = packetSinkHelper2.Install(nodes.Get(1));
    sinkApps2.Start(Seconds(0.0));
    sinkApps2.Stop(Seconds(2.0));

    OnOffHelper onOffHelper2("ns3::UdpSocketFactory", sinkAddress2);
    onOffHelper2.SetConstantRate(DataRate("448kbps"), 210);
    onOffHelper2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer srcApps2 = onOffHelper2.Install(nodes.Get(3));
    srcApps2.Start(Seconds(0.5));
    srcApps2.Stop(Seconds(2.0));

    // FTP/TCP flow from n0 to n3 (only between 1.2s and 1.35s)
    uint16_t tcpPort = 50000;
    Address tcpSinkAddress(InetSocketAddress(if_n2n3.GetAddress(1), tcpPort));

    PacketSinkHelper tcpPacketSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer tcpSinkApps = tcpPacketSinkHelper.Install(nodes.Get(3));
    tcpSinkApps.Start(Seconds(1.0));
    tcpSinkApps.Stop(Seconds(2.0));

    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", tcpSinkAddress);
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited

    ApplicationContainer tcpSrcApps = bulkSendHelper.Install(nodes.Get(0));
    tcpSrcApps.Start(Seconds(1.2));
    tcpSrcApps.Stop(Seconds(1.35));

    // Enable tracing
    AsciiTraceHelper asciiTraceHelper;
    p2p_n0n2.EnableAsciiAll(asciiTraceHelper.CreateFileStream("simple-global-routing.tr"));
    p2p_n1n2.EnableAsciiAll(asciiTraceHelper.CreateFileStream("simple-global-routing.tr"));
    p2p_n2n3.EnableAsciiAll(asciiTraceHelper.CreateFileStream("simple-global-routing.tr"));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}