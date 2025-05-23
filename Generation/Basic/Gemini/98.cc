#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/drop-tail-queue.h"

int main(int argc, char *argv[])
{
    // Set up default parameters
    Config::SetDefault("ns3::DropTailQueue::MaxSize", StringValue("100p"));
    Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("10Mbps"));
    Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("1ms"));
    Config::SetDefault("ns3::CsmaNetDevice::DataRate", StringValue("100Mbps"));
    Config::SetDefault("ns3::CsmaChannel::Delay", StringValue("6560ns")); // Standard Ethernet delay

    // Disable checksums for simulation simplicity/speed
    Config::SetDefault("ns3::Ipv4L3Protocol::CalculateChecksum", BooleanValue(false));
    Config::SetDefault("ns3::TcpL4Protocol::CalculateChecksum", BooleanValue(false));
    Config::SetDefault("ns3::UdpL4Protocol::CalculateChecksum", BooleanValue(false));

    // Create 7 nodes: N0, N1, N2, N3, N4, N5, N6
    NodeContainer nodes;
    nodes.Create(7);

    // Create P2P links
    PointToPointHelper p2pHelper;
    NetDeviceContainer p2pDevices01 = p2pHelper.Install(nodes.Get(0), nodes.Get(1)); // N0-N1
    NetDeviceContainer p2pDevices12 = p2pHelper.Install(nodes.Get(1), nodes.Get(2)); // N1-N2
    NetDeviceContainer p2pDevices25 = p2pHelper.Install(nodes.Get(2), nodes.Get(5)); // N2-N5 (alternative path)
    NetDeviceContainer p2pDevices54 = p2pHelper.Install(nodes.Get(5), nodes.Get(4)); // N5-N4 (alternative path)
    NetDeviceContainer p2pDevices46 = p2pHelper.Install(nodes.Get(4), nodes.Get(6)); // N4-N6

    // Create CSMA link (N2, N3, N4 form a single CSMA segment)
    CsmaHelper csmaHelper;
    NodeContainer csmaNodes;
    csmaNodes.Add(nodes.Get(2));
    csmaNodes.Add(nodes.Get(3));
    csmaNodes.Add(nodes.Get(4));
    NetDeviceContainer csmaDevices = csmaHelper.Install(csmaNodes);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP Addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.0.0", "255.255.255.252");
    Ipv4InterfaceContainer iface01 = ipv4.Assign(p2pDevices01); // N0-N1

    ipv4.SetBase("10.0.1.0", "255.255.255.252");
    Ipv4InterfaceContainer iface12 = ipv4.Assign(p2pDevices12); // N1-N2

    ipv4.SetBase("10.0.2.0", "255.255.255.0"); // CSMA link, /24
    Ipv4InterfaceContainer ifaceCsma = ipv4.Assign(csmaDevices); // N2, N3, N4 on CSMA

    ipv4.SetBase("10.0.3.0", "255.255.255.252");
    Ipv4InterfaceContainer iface25 = ipv4.Assign(p2pDevices25); // N2-N5

    ipv4.SetBase("10.0.4.0", "255.255.255.252");
    Ipv4InterfaceContainer iface54 = ipv4.Assign(p2pDevices54); // N5-N4

    ipv4.SetBase("10.0.5.0", "255.255.255.252");
    Ipv4InterfaceContainer iface46 = ipv4.Assign(p2pDevices46); // N4-N6

    // Global Routing: Populate initial routing tables and enable auto-recomputation
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    Ipv4GlobalRoutingHelper::EnableAutoRecompute();

    // Application: UDP traffic from Node 1 to Node 6
    // Node 6 is the sink (server)
    UdpServerHelper server(9); // Port 9 (discard)
    ApplicationContainer serverApps = server.Install(nodes.Get(6));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(40.0));

    // Node 1 is the source (client)
    uint32_t payloadSize = 1024; // Bytes
    Time interPacketInterval = MilliSeconds(100); // 10 packets/sec
    // Destination is N6's IP address on the N4-N6 link
    UdpClientHelper client(iface46.GetAddress(1), server.GetPort()); 
    client.SetAttribute("MaxPackets", UintegerValue(10000)); // Sufficiently large
    client.SetAttribute("Interval", TimeValue(interPacketInterval));
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));

    // Flow 1: N1 to N6, starts at 1s, stops at 25s
    ApplicationContainer clientApp1 = client.Install(nodes.Get(1));
    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(25.0));

    // Flow 2: N1 to N6, starts at 11s, stops at 30s
    ApplicationContainer clientApp2 = client.Install(nodes.Get(1));
    clientApp2.Start(Seconds(11.0));
    clientApp2.Stop(Seconds(30.0));

    // Interface Down/Up Events
    // Identify N2's interface on the CSMA link (which is csmaDevices.Get(0))
    Ptr<Ipv4> ipv4N2 = nodes.Get(2)->GetObject<Ipv4>();
    int32_t csmaIfIndexN2 = ipv4N2->GetInterfaceForDevice(csmaDevices.Get(0));

    // Bring N2's CSMA interface down at 5 seconds
    Simulator::Schedule(Seconds(5.0), &Ipv4::SetDown, ipv4N2, csmaIfIndexN2);

    // Bring N2's CSMA interface up at 15 seconds
    Simulator::Schedule(Seconds(15.0), &Ipv4::SetUp, ipv4N2, csmaIfIndexN2);

    // Tracing to output files
    AsciiTraceHelper ascii;
    p2pHelper.EnableAsciiAll(ascii.CreateFileStream("p2p-traffic.tr"));
    csmaHelper.EnableAsciiAll(ascii.CreateFileStream("csma-traffic.tr"));
    
    // Optional: Enable PCAP tracing for debugging
    // p2pHelper.EnablePcapAll("p2p", false);
    // csmaHelper.EnablePcapAll("csma", false);

    // Set simulation end time
    Simulator::Stop(Seconds(35.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}