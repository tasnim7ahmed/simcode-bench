#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologySimulation");

int main(int argc, char *argv[]) {
    // Default configuration values
    uint32_t numClients = 5;
    Time simulationTime = Seconds(10.0);
    uint32_t packetSize = 1024; // bytes
    DataRate dataRate("1Mbps");
    uint32_t mtu = 1500;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numClients", "Number of client nodes", numClients);
    cmd.AddValue("simulationTime", "Total simulation time", simulationTime);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("dataRate", "Data rate of point-to-point links", dataRate);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Creating topology.");

    NodeContainer clients;
    clients.Create(numClients);

    NodeContainer server;
    server.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(dataRate));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer serverDevices;
    NetDeviceContainer clientDevices;

    InternetStackHelper stack;
    stack.Install(server);
    stack.Install(clients);

    for (uint32_t i = 0; i < numClients; ++i) {
        NetDeviceContainer devices = p2p.Install(NodeContainer(clients.Get(i), server.Get(0)));
        clientDevices.Add(devices.Get(0));
        serverDevices.Add(devices.Get(1));
    }

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer serverInterfaces;
    Ipv4InterfaceContainer clientInterfaces;

    for (uint32_t i = 0; i < numClients; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(clientDevices.Get(i), serverDevices.Get(i)));
        clientInterfaces.Add(interfaces.Get(0));
        serverInterfaces.Add(interfaces.Get(1));
    }

    uint16_t sinkPort = 8080;

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(server.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(simulationTime);

    OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address());
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("500kbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numClients; ++i) {
        onOffHelper.SetAttribute("Remote", AddressValue(InetSocketAddress(serverInterfaces.GetAddress(i), sinkPort)));
        clientApps.Add(onOffHelper.Install(clients.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(simulationTime - Seconds(1.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("star-topology.xml");
    anim.SetConstantPosition(server.Get(0), 0, 0);
    double angleStep = 360.0 / numClients;
    for (uint32_t i = 0; i < numClients; ++i) {
        double angleRad = i * angleStep * M_PI / 180.0;
        double x = 10.0 * cos(angleRad);
        double y = 10.0 * sin(angleRad);
        anim.SetConstantPosition(clients.Get(i), x, y);
    }

    NS_LOG_INFO("Running simulation.");
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Analyzing flows.");
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalThroughput = 0;
    double totalLatency = 0;
    uint32_t flowCount = 0;
    uint64_t totalLostPackets = 0;

    for (auto &[flowId, flowStats] : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
        if (t.destinationPort == sinkPort) {
            NS_LOG_INFO("Flow ID: " << flowId << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress);
            NS_LOG_INFO("  Tx Packets = " << flowStats.txPackets);
            NS_LOG_INFO("  Rx Packets = " << flowStats.rxPackets);
            NS_LOG_INFO("  Lost Packets = " << flowStats.lostPackets);
            NS_LOG_INFO("  Throughput: " << flowStats.rxBytes * 8.0 / (flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps");
            NS_LOG_INFO("  Mean Delay: " << flowStats.delaySum.GetSeconds() / flowStats.rxPackets << " s");

            totalThroughput += flowStats.rxBytes * 8.0 / (flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds()) / 1e6;
            totalLatency += flowStats.delaySum.GetSeconds() / flowStats.rxPackets;
            totalLostPackets += flowStats.lostPackets;
            flowCount++;
        }
    }

    if (flowCount > 0) {
        NS_LOG_INFO("Average Throughput: " << totalThroughput / flowCount << " Mbps");
        NS_LOG_INFO("Average Latency: " << totalLatency / flowCount << " s");
        NS_LOG_INFO("Total Lost Packets: " << totalLostPackets);
    }

    return 0;
}