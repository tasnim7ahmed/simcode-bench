#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPointToPointSimulation");

int main(int argc, char *argv[]) {
    // Log levels
    LogComponentEnable("TcpPointToPointSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup TCP server on Node 1 (port 9)
    uint16_t port = 9;
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = sink.Install(nodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Setup TCP client on Node 0
    OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    client.SetAttribute("DataRate", DataRateValue(DataRate("1Kbps"))); // Not limiting bandwidth
    client.SetAttribute("PacketSize", UintegerValue(512));
    client.SetAttribute("MaxBytes", UintegerValue(512 * 10));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(0.0));
    clientApp.Stop(Seconds(10.0));

    // Enable packet capture
    pointToPoint.EnablePcapAll("tcp-point-to-point");

    // Setup FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.Install(nodes);

    // Run simulation
    Simulator::Run();

    // Print FlowMonitor statistics
    monitor->CheckForLostPackets();
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        if (it->first == 0) continue; // skip header
        NS_LOG_INFO("Flow ID: " << it->first);
        NS_LOG_INFO("  Tx Packets: " << it->second.txPackets);
        NS_LOG_INFO("  Rx Packets: " << it->second.rxPackets);
        NS_LOG_INFO("  Lost Packets: " << it->second.lostPackets);
        NS_LOG_INFO("  Throughput: " << it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps");
    }

    Simulator::Destroy();
    return 0;
}