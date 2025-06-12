#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridNetworkSimulation");

int main(int argc, char *argv[]) {
    uint32_t tcpClients = 5;
    uint32_t meshNodes = 6;
    double simulationTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tcpClients", "Number of TCP clients in star topology", tcpClients);
    cmd.AddValue("meshNodes", "Number of nodes in mesh topology", meshNodes);
    cmd.AddValue("simulationTime", "Duration of the simulation in seconds", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer tcpStarNodes;
    tcpStarNodes.Create(tcpClients + 1); // +1 for server

    NodeContainer meshNodesContainer;
    meshNodesContainer.Create(meshNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> tcpDevices;
    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(tcpStarNodes.Get(0)->GetDevice(0));
    address.NewNetwork();

    for (uint32_t i = 1; i <= tcpClients; ++i) {
        NetDeviceContainer devices = p2p.Install(NodeContainer(tcpStarNodes.Get(0), tcpStarNodes.Get(i)));
        tcpDevices.push_back(devices);
        address.Assign(devices);
        address.NewNetwork();
    }

    std::vector<NetDeviceContainer> meshDevices;
    for (uint32_t i = 0; i < meshNodes; ++i) {
        for (uint32_t j = i + 1; j < meshNodes; ++j) {
            NetDeviceContainer devices = p2p.Install(NodeContainer(meshNodesContainer.Get(i), meshNodesContainer.Get(j)));
            meshDevices.push_back(devices);
            address.Assign(devices);
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 5000;
    ApplicationContainer tcpApps;

    for (uint32_t i = 1; i <= tcpClients; ++i) {
        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
        source.SetAttribute("MaxBytes", UintegerValue(0));
        ApplicationContainer app = source.Install(tcpStarNodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simulationTime - 1.0));
        tcpApps.Add(app);
    }

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = sink.Install(tcpStarNodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));

    ApplicationContainer udpApps;
    for (uint32_t i = 0; i < meshNodes; ++i) {
        for (uint32_t j = 0; j < meshNodes; ++j) {
            if (i != j) {
                UdpEchoClientHelper echoClientHelper(meshNodesContainer.Get(j)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
                echoClientHelper.SetAttribute("MaxPackets", UintegerValue(20));
                echoClientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
                echoClientHelper.SetAttribute("PacketSize", UintegerValue(1024));
                ApplicationContainer clientApp = echoClientHelper.Install(meshNodesContainer.Get(i));
                clientApp.Start(Seconds(2.0));
                clientApp.Stop(Seconds(simulationTime - 1.0));
                udpApps.Add(clientApp);
            }
        }
    }

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer echoServerApp = echoServer.Install(meshNodesContainer);
    echoServerApp.Start(Seconds(0.0));
    echoServerApp.Stop(Seconds(simulationTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("hybrid-network.xml");
    anim.SetConstantPosition(tcpStarNodes.Get(0), 0, 0);
    for (uint32_t i = 1; i <= tcpClients; ++i) {
        anim.SetConstantPosition(tcpStarNodes.Get(i), 5 * cos(2 * M_PI * (i - 1) / tcpClients), 5 * sin(2 * M_PI * (i - 1) / tcpClients));
    }

    for (uint32_t i = 0; i < meshNodes; ++i) {
        double angle = 2 * M_PI * i / meshNodes;
        anim.SetConstantPosition(meshNodesContainer.Get(i), 15 + 5 * cos(angle), 5 * sin(angle));
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    std::cout << "\nTCP/UDP Performance Metrics:\n";
    for (auto &[flowId, flowStats] : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
        std::cout << "Flow " << flowId << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flowStats.txPackets << "\n";
        std::cout << "  Rx Packets: " << flowStats.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flowStats.lostPackets << "\n";
        std::cout << "  Throughput: " << flowStats.rxBytes * 8.0 / simulationTime / 1000 / 1000 << " Mbps\n";
        std::cout << "  Mean Delay: " << flowStats.delaySum.GetSeconds() / flowStats.rxPackets << " s\n";
        std::cout << "  Mean Jitter: " << flowStats.jitterSum.GetSeconds() / (flowStats.rxPackets > 1 ? flowStats.rxPackets - 1 : 1) << " s\n\n";
    }

    Simulator::Destroy();
    return 0;
}