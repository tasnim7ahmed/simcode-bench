#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedNetworkTopology");

int main(int argc, char* argv[])
{
    // 1. Set up simulation defaults
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("MixedNetworkTopology", LOG_LEVEL_INFO);

    // 2. Create nodes
    NodeContainer nodes;
    nodes.Create(7); // n0, n1, n2, n3, n4, n5, n6

    Ptr<Node> n0 = nodes.Get(0);
    Ptr<Node> n1 = nodes.Get(1);
    Ptr<Node> n2 = nodes.Get(2);
    Ptr<Node> n3 = nodes.Get(3);
    Ptr<Node> n4 = nodes.Get(4);
    Ptr<Node> n5 = nodes.Get(5);
    Ptr<Node> n6 = nodes.Get(6);

    // 3. Create Point-to-Point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2p_n0_n2_devs = p2p.Install(n0, n2);
    NetDeviceContainer p2p_n1_n2_devs = p2p.Install(n1, n2);
    NetDeviceContainer p2p_n5_n6_devs = p2p.Install(n5, n6);

    // 4. Create CSMA segment
    CsmaHelper csma;
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500)); // Default Ethernet MTU
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("6560ns")); // Typical Ethernet delay for 100m, 100Mbps

    NodeContainer csmaNodes;
    csmaNodes.Add(n2);
    csmaNodes.Add(n3);
    csmaNodes.Add(n4);
    csmaNodes.Add(n5);

    NetDeviceContainer csma_devs = csma.Install(csmaNodes);

    // 5. Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // 6. Assign IP Addresses
    Ipv4AddressHelper ipv4;

    // P2P n0-n2 link (10.1.1.0/24)
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2p_n0_n2_interfaces = ipv4.Assign(p2p_n0_n2_devs);

    // P2P n1-n2 link (10.1.2.0/24)
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer p2p_n1_n2_interfaces = ipv4.Assign(p2p_n1_n2_devs);

    // CSMA segment n2-n3-n4-n5 (10.1.3.0/24)
    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer csma_interfaces = ipv4.Assign(csma_devs);

    // P2P n5-n6 link (10.1.4.0/24)
    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer p2p_n5_n6_interfaces = ipv4.Assign(p2p_n5_n6_devs);

    // 7. Populate routing tables
    // Enable global routing for automatic routing table population across subnets
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 8. Setup Application (CBR/UDP from n0 to n6)
    // OnOff application on n0 (sender)
    uint16_t sinkPort = 9;
    
    // The sink (n6) is on the 10.1.4.0/24 network, its IP is 10.1.4.2
    // GetAddress(1) refers to the second device in the p2p_n5_n6_devs container, which is n6.
    Ipv4Address sinkAddress = p2p_n5_n6_interfaces.GetAddress(1);

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(sinkAddress, sinkPort));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=100.0]")); // Keep app "On" for a long duration
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));  // Never turn "Off"
    onoff.SetAttribute("PacketSize", UintegerValue(50)); // 50 bytes per packet
    onoff.SetAttribute("DataRate", StringValue("300bps")); // 300 bits per second

    ApplicationContainer onoffApps = onoff.Install(n0);
    onoffApps.Start(Seconds(1.0)); // Start sending at 1 second
    onoffApps.Stop(Seconds(9.0));  // Stop sending at 9 seconds

    // PacketSink on n6 (receiver)
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = sink.Install(n6);
    sinkApps.Start(Seconds(0.5));  // Start listening slightly before sender
    sinkApps.Stop(Seconds(10.0));  // Listen until the end of simulation

    // 9. Enable Tracing
    // Pcap tracing for all channels
    p2p.EnablePcapAll("mixed-network-p2p");
    csma.EnablePcapAll("mixed-network-csma");

    // Ascii trace for network queues and packet receptions
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("mixed-network-trace.tr");
    
    // Enable ascii tracing on all p2p devices
    p2p.EnableAsciiAll(stream);
    // Enable ascii tracing on all csma devices
    csma.EnableAsciiAll(stream);

    // 10. Run simulation
    Simulator::Stop(Seconds(10.0)); // Simulate for 10 seconds
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}