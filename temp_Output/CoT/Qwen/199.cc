#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkSimulation");

int main(int argc, char *argv[]) {
    uint32_t packetSize = 1024;
    double simulationTime = 10.0;
    std::string throughputOutputFile = "throughput.csv";

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1024));

    NodeContainer routers;
    routers.Create(3);

    NodeContainer csmaNodes;
    csmaNodes.Create(3);

    NodeContainer p2pR1R2 = NodeContainer(routers.Get(0), routers.Get(1));
    NodeContainer p2pR2R3 = NodeContainer(routers.Get(1), routers.Get(2));
    NodeContainer csmaRouter = NodeContainer(routers.Get(1), csmaNodes.Get(0));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pR1R2Devices = p2p.Install(p2pR1R2);
    NetDeviceContainer p2pR2R3Devices = p2p.Install(p2pR2R3);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0.01)));

    NetDeviceContainer csmaDevices = csma.Install(csmaRouter);
    NetDeviceContainer csmaLanDevices = csma.Install(csmaNodes);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pR1R2Interfaces = address.Assign(p2pR1R2Devices);

    address.NewNetwork();
    Ipv4InterfaceContainer p2pR2R3Interfaces = address.Assign(p2pR2R3Devices);

    address.NewNetwork();
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    address.NewNetwork();
    Ipv4InterfaceContainer csmaLanInterfaces = address.Assign(csmaLanDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(csmaInterfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(csmaNodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    OnOffHelper onOff("ns3::TcpSocketFactory", sinkAddress);
    onOff.SetConstantRate(DataRate("1Mbps"));
    onOff.SetPacketSize(packetSize);

    ApplicationContainer clientApps = onOff.Install(csmaNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("network-simulation.tr"));
    p2p.EnablePcapAll("network-simulation");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();

    std::ofstream outFile(throughputOutputFile);
    outFile << "FlowID,Source,Destination,PacketsSent,PacketsReceived,Throughput" << std::endl;

    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(monitor->GetClassifier().Peek())->FindFlow(iter->first);
        std::stringstream ssSource, ssDest;
        ssSource << t.sourceAddress << ":" << t.sourcePort;
        ssDest << t.destinationAddress << ":" << t.destinationPort;
        double throughput = iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000;
        outFile << iter->first << ","
                << ssSource.str() << ","
                << ssDest.str() << ","
                << iter->second.txPackets << ","
                << iter->second.rxPackets << ","
                << throughput << std::endl;
    }

    outFile.close();
    Simulator::Destroy();

    return 0;
}