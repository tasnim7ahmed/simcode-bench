#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    // Create seven nodes
    NodeContainer nodes;
    nodes.Create(7);
    Ptr<Node> n0 = nodes.Get(0);
    Ptr<Node> n1 = nodes.Get(1);
    Ptr<Node> n2 = nodes.Get(2);
    Ptr<Node> n3 = nodes.Get(3);
    Ptr<Node> n4 = nodes.Get(4);
    Ptr<Node> n5 = nodes.Get(5);
    Ptr<Node> n6 = nodes.Get(6);

    // Create Point-to-Point helper and set attributes
    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pHelper.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install Point-to-Point links
    NetDeviceContainer p2pDev02 = p2pHelper.Install(n0, n2);
    NetDeviceContainer p2pDev12 = p2pHelper.Install(n1, n2);
    NetDeviceContainer p2pDev56 = p2pHelper.Install(n5, n6);

    // Create CSMA helper and set attributes
    CsmaHelper csmaHelper;
    csmaHelper.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csmaHelper.SetChannelAttribute("Delay", StringValue("6560ns"));

    // Install CSMA/CD bus for n2, n3, n4, n5
    NodeContainer csmaNodes;
    csmaNodes.Add(n2);
    csmaNodes.Add(n3);
    csmaNodes.Add(n4);
    csmaNodes.Add(n5);
    NetDeviceContainer csmaDevs = csmaHelper.Install(csmaNodes);

    // Install Internet Stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.0.0", "255.255.255.252"); // For n0-n2 link
    Ipv4InterfaceContainer p2pInt02 = ipv4.Assign(p2pDev02);

    ipv4.SetBase("10.0.0.4", "255.255.255.252"); // For n1-n2 link
    Ipv4InterfaceContainer p2pInt12 = ipv4.Assign(p2pDev12);

    ipv4.SetBase("10.0.1.0", "255.255.255.0"); // For CSMA bus
    Ipv4InterfaceContainer csmaInts = ipv4.Assign(csmaDevs);

    ipv4.SetBase("10.0.2.0", "255.255.255.252"); // For n5-n6 link
    Ipv4InterfaceContainer p2pInt56 = ipv4.Assign(p2pDev56);

    // Populate routing tables for global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup CBR UDP traffic from n0 to n6
    uint16_t port = 9; // Discard port
    const uint32_t packetSize = 210; // bytes
    const DataRate dataRate("448Kbps"); // 448 Kb/s

    // Calculate the interval between packets for the CBR source
    // Interval = (PacketSize_bits) / (DataRate_bits_per_second)
    Time packetInterval = Seconds(static_cast<double>(packetSize * 8) / dataRate.GetBitsPerSecond());

    // Setup UDP Server (sink) on n6
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(n6);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP Client (source) on n0
    // Destination is n6's IP address on the p2p link with n5 (p2pInt56.GetAddress(1))
    UdpClientHelper client(p2pInt56.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(0)); // Send indefinitely
    client.SetAttribute("Interval", TimeValue(packetInterval));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = client.Install(n0);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(9.0));

    // Enable tracing of queues and packet receptions
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("mixed-topology.tr");

    // Enable ASCII traces for all devices installed by the p2p and csma helpers
    p2pHelper.EnableAsciiAll(stream);
    csmaHelper.EnableAsciiAll(stream);

    // Enable Pcap tracing for all devices
    p2pHelper.EnablePcapAll("mixed-topology", false);
    csmaHelper.EnablePcapAll("mixed-topology", false);

    // Set simulation duration
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}