#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPacingExample");

int main(int argc, char* argv[]) {
    bool enablePacing = true;
    double simulationTime = 10.0;
    std::string dataRate = "5Mbps";
    std::string delay = "2ms";
    uint32_t packetSize = 1448;

    CommandLine cmd;
    cmd.AddValue("enablePacing", "Enable TCP pacing", enablePacing);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("dataRate", "Data rate of bottleneck link", dataRate);
    cmd.AddValue("delay", "Delay of bottleneck link", delay);
    cmd.AddValue("packetSize", "Size of TCP packets", packetSize);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));

    NodeContainer nodes;
    nodes.Create(6);

    InternetStackHelper stack;
    stack.Install(nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));

    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue(delay));
    NetDeviceContainer devices23 = p2p.Install(nodes.Get(2), nodes.Get(3));

    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer devices34 = p2p.Install(nodes.Get(3), nodes.Get(4));
    NetDeviceContainer devices35 = p2p.Install(nodes.Get(3), nodes.Get(5));

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces34 = address.Assign(devices34);

    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces35 = address.Assign(devices35);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces3.GetAddress(0), port));
    source.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApps = source.Install(nodes.Get(2));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(simulationTime - 1.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(3));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    if (enablePacing) {
        Config::Set("/NodeList/*/ApplicationList/*/$ns3::BulkSendApplication/Socket/EnablePacing", BooleanValue(true));
        Config::Set("/NodeList/*/ApplicationList/*/$ns3::BulkSendApplication/Socket/InitialCwnd", UintegerValue(10));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.sourceAddress == interfaces2.GetAddress(0) && t.destinationAddress == interfaces3.GetAddress(0)) {
            std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
            std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
            std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
            std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
            std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
            std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";
        }
    }

    monitor->SerializeToXmlFile("tcp-pacing.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}