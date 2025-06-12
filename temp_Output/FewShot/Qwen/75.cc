#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging if needed (optional)
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);

    // Create 7 nodes
    NodeContainer nodes;
    nodes.Create(7);

    // Point-to-point links: n0-n2 and n1-n2
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dev_n0n2 = p2p.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer dev_n1n2 = p2p.Install(nodes.Get(1), nodes.Get(2));

    // CSMA/CD bus for n2, n3, n4, n5
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0.01)));

    NetDeviceContainer dev_csma;
    dev_csma.Add(dev_n0n2.Get(1)); // From n2 side of previous link
    NodeContainer csmaNodes = NodeContainer(nodes.Get(2), nodes.Get(3), nodes.Get(4), nodes.Get(5));
    NetDeviceContainer tempCsmaDevs = csma.Install(csmaNodes);
    dev_csma.Add(tempCsmaDevs);

    // Point-to-point link between n5 and n6
    NetDeviceContainer dev_n5n6 = p2p.Install(nodes.Get(5), nodes.Get(6));

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces_n0n2 = address.Assign(dev_n0n2);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces_n1n2 = address.Assign(dev_n1n2);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces_n5n6 = address.Assign(dev_n5n6);

    // Assign IP addresses to CSMA devices
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces_csma = address.Assign(tempCsmaDevs);

    // Set up CBR UDP traffic from n0 to n6
    uint16_t cbrPort = 9; // well-known port

    // Create a packet sink on n6 to receive the traffic
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          InetSocketAddress(Ipv4Address::GetAny(), cbrPort));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(6));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Create the CBR client on n0
    OnOffHelper source("ns3::UdpSocketFactory",
                       InetSocketAddress(interfaces_n5n6.GetAddress(1), cbrPort));
    source.SetAttribute("DataRate", DataRateValue(DataRate("448Kb/s")));
    source.SetAttribute("PacketSize", UintegerValue(210));
    source.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    source.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(1.0));
    sourceApp.Stop(Seconds(10.0));

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("mixed_topology.tr");

    pointToPoint.EnableAsciiAll(stream);
    csma.EnableAsciiAll(stream);
    pointToPoint.EnablePcapAll("mixed_topology");
    csma.EnablePcapAll("mixed_topology");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}