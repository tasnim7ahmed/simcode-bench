#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("P2PTcpThreeNode");

static uint32_t txPackets[3] = {0, 0, 0};
static uint32_t rxPackets[3] = {0, 0, 0};

void TxTrace(uint32_t nodeId) {
    txPackets[nodeId]++;
}

void RxTrace(uint32_t nodeId, Ptr<const Packet> packet, const Address &address) {
    rxPackets[nodeId]++;
}

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create 3 nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Create two point-to-point links: n0 <-> n1 <-> n2
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0d1 = p2p.Install(n0n1);
    NetDeviceContainer d1d2 = p2p.Install(n1n2);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

    // Enable routing on all nodes
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up TCP traffic from n0 to n2
    uint16_t port = 50000;

    // Install PacketSink on n2 (acts as the TCP server)
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(12.0));
    
    // Use BulkSendApplication on n0 (acts as TCP client)
    BulkSendHelper source("ns3::TcpSocketFactory",
        InetSocketAddress(i1i2.GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(0)); // Send unlimited data
    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(1.0));
    sourceApp.Stop(Seconds(11.0));

    // Packet tracing and statistics
    // Transmit trace on each NetDevice
    d0d1.Get(0)->TraceConnectWithoutContext("PhyTxEnd", MakeBoundCallback([](uint32_t nodeId, Ptr<const Packet>){TxTrace(nodeId);}, 0));
    d0d1.Get(1)->TraceConnectWithoutContext("PhyTxEnd", MakeBoundCallback([](uint32_t nodeId, Ptr<const Packet>){TxTrace(nodeId);}, 1));
    d1d2.Get(0)->TraceConnectWithoutContext("PhyTxEnd", MakeBoundCallback([](uint32_t nodeId, Ptr<const Packet>){TxTrace(nodeId);}, 1));
    d1d2.Get(1)->TraceConnectWithoutContext("PhyTxEnd", MakeBoundCallback([](uint32_t nodeId, Ptr<const Packet>){TxTrace(nodeId);}, 2));

    // Receive trace at IP layer for each node
    Config::ConnectWithoutContext("/NodeList/0/$ns3::Ipv4L3Protocol/Rx", MakeBoundCallback(&RxTrace, 0));
    Config::ConnectWithoutContext("/NodeList/1/$ns3::Ipv4L3Protocol/Rx", MakeBoundCallback(&RxTrace, 1));
    Config::ConnectWithoutContext("/NodeList/2/$ns3::Ipv4L3Protocol/Rx", MakeBoundCallback(&RxTrace, 2));

    // Enable pcap tracing
    p2p.EnablePcapAll("tcp-3node");

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();

    // Display packet stats
    std::cout << "Node-wise packet transmission and reception stats:\n";
    for (uint32_t i = 0; i < 3; ++i) {
        std::cout << "Node " << i << ": "
                  << "Tx = " << txPackets[i]
                  << ", Rx = " << rxPackets[i] << std::endl;
    }

    Simulator::Destroy();
    return 0;
}