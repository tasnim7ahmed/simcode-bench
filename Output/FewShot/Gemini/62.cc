#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPacingSimulation");

int main(int argc, char *argv[]) {
    bool enablePacing = true;
    double simulationTime = 10.0;
    std::string bottleneckDataRate = "10Mbps";
    std::string bottleneckDelay = "10ms";
    uint32_t initialCwnd = 10;

    CommandLine cmd;
    cmd.AddValue("enablePacing", "Enable TCP pacing (default: true)", enablePacing);
    cmd.AddValue("simulationTime", "Simulation time in seconds (default: 10.0)", simulationTime);
    cmd.AddValue("bottleneckDataRate", "Bottleneck data rate (default: 10Mbps)", bottleneckDataRate);
    cmd.AddValue("bottleneckDelay", "Bottleneck delay (default: 10ms)", bottleneckDelay);
    cmd.AddValue("initialCwnd", "Initial congestion window (default: 10)", initialCwnd);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(6);

    PointToPointHelper p2pRouter;
    p2pRouter.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pRouter.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices01 = p2pRouter.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices12 = p2pRouter.Install(nodes.Get(1), nodes.Get(2));

    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue(bottleneckDataRate));
    p2pBottleneck.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    NetDeviceContainer devices23 = p2pBottleneck.Install(nodes.Get(2), nodes.Get(3));

    PointToPointHelper p2pRouter2;
    p2pRouter2.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pRouter2.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices34 = p2pRouter2.Install(nodes.Get(3), nodes.Get(4));
    NetDeviceContainer devices35 = p2pRouter2.Install(nodes.Get(3), nodes.Get(5));

    InternetStackHelper stack;
    stack.Install(nodes);

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

    uint16_t port = 5000;
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces35.GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer sourceApps = source.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(simulationTime));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(5));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    Ptr<Node> senderNode = nodes.Get(0);
    Ptr<Application> senderApp = sourceApps.Get(0);
    Ptr<BulkSendApplication> bulkSendApp = DynamicCast<BulkSendApplication>(senderApp);
    Ptr<Socket> socket = bulkSendApp->GetSocket();
    Ptr<TcpSocketBase> tcpSocket = DynamicCast<TcpSocketBase>(socket);

    if (enablePacing) {
        tcpSocket->SetPacingRate(DataRate(bottleneckDataRate));
    }
    tcpSocket->SetCwnd(initialCwnd);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));

    AsciiTraceHelper ascii;
    p2pBottleneck.EnableAsciiAll(ascii.CreateFileStream("tcp-pacing.tr"));
    p2pBottleneck.EnablePcapAll("tcp-pacing");

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    monitor->SerializeToXmlFile("tcp-pacing.flowmon", true, true);

    Simulator::Destroy();

    return 0;
}