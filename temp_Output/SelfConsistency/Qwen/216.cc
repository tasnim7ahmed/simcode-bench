#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("P2PThreeNodeTcpSimulation");

int main(int argc, char *argv[])
{
    // Enable logging for the simulation
    LogComponentEnable("P2PThreeNodeTcpSimulation", LOG_LEVEL_INFO);

    // Create 3 nodes: Node 0 (client), Node 1 (router), Node 2 (server)
    NodeContainer nodes;
    nodes.Create(3);

    // Set up Point-to-Point links between Node 0 <-> Node 1 and Node 1 <-> Node 2
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link Node 0 to Node 1
    NetDeviceContainer devices0_1 = p2p.Install(nodes.Get(0), nodes.Get(1));

    // Link Node 1 to Node 2
    NetDeviceContainer devices1_2 = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces0_1 = address.Assign(devices0_1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1_2 = address.Assign(devices1_2);

    // Set up TCP server on Node 2
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces1_2.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Set up TCP client on Node 0
    OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Infinite data

    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing for all devices
    p2p.EnablePcapAll("p2p-three-node-tcp");

    // Set up FlowMonitor to collect throughput and packet loss statistics
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    // Run simulation
    Simulator::Run();

    // Output flow statistics
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    for (auto iter : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter.first);
        NS_LOG_INFO("Flow ID: " << iter.first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
        NS_LOG_INFO("  Tx Packets = " << iter.second.txPackets);
        NS_LOG_INFO("  Rx Packets = " << iter.second.rxPackets);
        NS_LOG_INFO("  Lost Packets = " << iter.second.lostPackets);
        NS_LOG_INFO("  Throughput: " << iter.second.rxBytes * 8.0 / (iter.second.timeLastRxPacket.GetSeconds() - iter.second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps");
    }

    Simulator::Destroy();

    return 0;
}