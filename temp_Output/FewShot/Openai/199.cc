#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Log for debugging
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create routers
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> r3 = CreateObject<Node>();

    NodeContainer routers;
    routers.Add(r1);
    routers.Add(r2);
    routers.Add(r3);

    // Create LAN nodes: one sender node + R2 as gateway
    NodeContainer lanNodes;
    lanNodes.Create(1); // Sender node
    lanNodes.Add(r2);   // Gateway to router

    // One receiver node, connected to R3
    NodeContainer recNode;
    recNode.Create(1);  // Receiver node

    // Point-to-Point link: R1 <-> R2
    NodeContainer r1r2;
    r1r2.Add(r1);
    r1r2.Add(r2);

    // Point-to-Point link: R2 <-> R3
    NodeContainer r2r3;
    r2r3.Add(r2);
    r2r3.Add(r3);

    // Point-to-Point link: R3 <-> Receiver
    NodeContainer r3rec;
    r3rec.Add(r3);
    r3rec.Add(recNode.Get(0));

    // Configure P2P links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d_r1r2 = p2p.Install(r1r2);
    NetDeviceContainer d_r2r3 = p2p.Install(r2r3);
    NetDeviceContainer d_r3rec = p2p.Install(r3rec);

    // Configure CSMA (LAN)
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer d_lan = csma.Install(lanNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(NodeContainer(r1, r2, r3, recNode.Get(0), lanNodes.Get(0)));

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer i_r1r2, i_r2r3, i_r3rec, i_lan;

    // Subnet 10.1.1.0/24: R1 <-> R2
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    i_r1r2 = ipv4.Assign(d_r1r2);

    // Subnet 10.1.2.0/24: R2 <-> R3
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    i_r2r3 = ipv4.Assign(d_r2r3);

    // Subnet 10.1.3.0/24: R3 <-> Receiver
    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    i_r3rec = ipv4.Assign(d_r3rec);

    // Subnet 10.1.4.0/24: LAN
    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    i_lan = ipv4.Assign(d_lan);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // TCP sink on Receiver
    uint16_t sinkPort = 50000;
    Address sinkAddress(InetSocketAddress(i_r3rec.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(recNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    // TCP traffic generator on LAN node
    OnOffHelper client("ns3::TcpSocketFactory", sinkAddress);
    client.SetAttribute("DataRate", StringValue("5Mbps"));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer clientApp = client.Install(lanNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(20.0));

    // Throughput & packet statistics via FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(21.0));
    Simulator::Run();

    // Output file for statistics
    std::ofstream outFile("output.txt");
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (const auto &flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        outFile << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        outFile << "  Tx Packets: " << flow.second.txPackets << "\n";
        outFile << "  Rx Packets: " << flow.second.rxPackets << "\n";
        outFile << "  Throughput: " << (flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000) << " Mbps\n";
    }
    outFile.close();

    Simulator::Destroy();
    return 0;
}