#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkTopologySimulation");

int main(int argc, char *argv[]) {
    bool tracing = true;
    uint16_t metric_n1_n3 = 10; // configurable metric for link n1-n3

    CommandLine cmd(__FILE__);
    cmd.AddValue("metric_n1_n3", "Metric for the link between n1 and n3", metric_n1_n3);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(4); // n0, n1, n2, n3

    NS_LOG_INFO("Creating channels.");

    PointToPointHelper p2p_low_delay;
    p2p_low_delay.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_low_delay.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p_high_delay;
    p2p_high_delay.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p_high_delay.SetChannelAttribute("Delay", StringValue("100ms"));

    PointToPointHelper p2p_alt_path;
    p2p_alt_path.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p_alt_path.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer dev_n0_n2 = p2p_low_delay.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer dev_n1_n2 = p2p_low_delay.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer dev_n1_n3 = p2p_high_delay.Install(nodes.Get(1), nodes.Get(3));
    NetDeviceContainer dev_n2_n3 = p2p_alt_path.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n0_n2 = address.Assign(dev_n0_n2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n1_n2 = address.Assign(dev_n1_n2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n1_n3 = address.Assign(dev_n1_n3);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n2_n3 = address.Assign(dev_n2_n3);

    Ipv4StaticRoutingHelper routingHelper;

    Ptr<Ipv4> ipv4_n1 = nodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routing_n1 = routingHelper.GetStaticRouting(ipv4_n1);

    // Route from n1 to n3 via n2 with lower delay path as default (metric 1)
    routing_n1->AddNetworkRouteTo(Ipv4Address("10.1.3.0"), Ipv4Mask("255.255.255.0"), 3, 1);
    // Add alternate route through n2 with higher cost (metric set by parameter)
    routing_n1->AddNetworkRouteTo(Ipv4Address("10.1.4.0"), Ipv4Mask("255.255.255.0"), 2, metric_n1_n3);

    NS_LOG_INFO("Creating applications.");

    uint32_t packetSize = 1024;
    Time interPacketInterval = MilliSeconds(100);
    uint32_t maxPacketCount = 300;
    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(if_n1_n3.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(3));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    if (tracing) {
        AsciiTraceHelper ascii;
        p2p_low_delay.EnableAsciiAll(ascii.CreateFileStream("network-topology-simulation.tr"));
        p2p_high_delay.EnableAsciiAll(ascii.CreateFileStream("network-topology-simulation.tr"));
        p2p_alt_path.EnableAsciiAll(ascii.CreateFileStream("network-topology-simulation.tr"));

        p2p_low_delay.EnablePcapAll("network-topology-simulation");
        p2p_high_delay.EnablePcapAll("network-topology-simulation");
        p2p_alt_path.EnablePcapAll("network-topology-simulation");
    }

    NS_LOG_INFO("Running simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");
    return 0;
}