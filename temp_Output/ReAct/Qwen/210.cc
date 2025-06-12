#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologySimulation");

int main(int argc, char *argv[]) {
    uint32_t tcpClients = 5;
    uint32_t meshNodes = 6;
    double simulationTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tcpClients", "Number of TCP clients in star topology", tcpClients);
    cmd.AddValue("meshNodes", "Number of UDP nodes in mesh topology", meshNodes);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer tcpStarNodes;
    tcpStarNodes.Create(tcpClients + 1); // +1 for server

    NodeContainer meshNodesContainer;
    meshNodesContainer.Create(meshNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> tcpInterfaces;
    Ipv4InterfaceContainer serverInterface;

    // Star Topology: Clients to Server
    for (uint32_t i = 0; i < tcpClients; ++i) {
        NetDeviceContainer devices = p2p.Install(NodeContainer(tcpStarNodes.Get(i), tcpStarNodes.Get(tcpClients)));
        address.SetBase("10.1." + std::to_string(i) + ".0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        tcpInterfaces.push_back(interfaces);
        if (i == 0) {
            serverInterface = interfaces;
        }
    }

    // Mesh Topology: Fully connected UDP nodes
    std::vector<std::vector<Ipv4InterfaceContainer>> meshInterfaces(meshNodes);
    for (uint32_t i = 0; i < meshNodes; ++i) {
        for (uint32_t j = i + 1; j < meshNodes; ++j) {
            NetDeviceContainer meshDevices = p2p.Install(NodeContainer(meshNodesContainer.Get(i), meshNodesContainer.Get(j)));
            address.SetBase("10.2." + std::to_string(i) + "." + std::to_string(j) + ".0", "255.255.255.0");
            Ipv4InterfaceContainer meshIf = address.Assign(meshDevices);
            meshInterfaces[i].Add(meshIf.Get(0));
            meshInterfaces[j].Add(meshIf.Get(1));
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // TCP Applications: Clients send to server
    uint16_t port = 5000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(tcpStarNodes.Get(tcpClients)); // server node
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(serverInterface.GetAddress(1), port));
    onoff.SetConstantRate(DataRate("1Mbps"), 512);
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < tcpClients; ++i) {
        clientApps.Add(onoff.Install(tcpStarNodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime - 0.1));

    // UDP Applications: Mesh nodes communicate with each other
    port = 6000;
    for (uint32_t i = 0; i < meshNodes; ++i) {
        Address sinkAddr(InetSocketAddress(Ipv4Address::GetAny(), port + i));
        PacketSinkHelper udpSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + i));
        ApplicationContainer meshSinkApp = udpSink.Install(meshNodesContainer.Get(i));
        meshSinkApp.Start(Seconds(0.0));
        meshSinkApp.Stop(Seconds(simulationTime));

        for (uint32_t j = 0; j < meshNodes; ++j) {
            if (i != j) {
                OnOffHelper meshOnOff("ns3::UdpSocketFactory", InetSocketAddress(meshInterfaces[j].GetAddress(0), port + j));
                meshOnOff.SetConstantRate(DataRate("500kbps"), 1024);
                ApplicationContainer meshClientApp = meshOnOff.Install(meshNodesContainer.Get(i));
                meshClientApp.Start(Seconds(2.0));
                meshClientApp.Stop(Seconds(simulationTime - 0.1));
            }
        }
    }

    // Flow Monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Mobility for visualization
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.InstallAll();

    // NetAnim visualization
    AnimationInterface anim("hybrid_topology.xml");
    anim.EnablePacketMetadata(true);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::string proto = (t.protocol == 6) ? "TCP" : "UDP";
        std::cout << "Flow ID: " << iter->first << " (" << proto << " "
                  << t.sourceAddress << ":" << t.sourcePort << " -> "
                  << t.destinationAddress << ":" << t.destinationPort << ")"
                  << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
        std::cout << "  Throughput: " << (iter->second.rxBytes * 8.0) / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps" << std::endl;
        std::cout << "  Mean Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}