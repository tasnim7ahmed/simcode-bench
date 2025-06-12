#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Simulation parameters
    uint32_t tcpClients = 4;
    uint32_t meshNodes = 5;
    double simulationTime = 10.0;

    // Enable logging for applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("TcpSocketBase", LOG_LEVEL_INFO);

    // Create nodes: TCP clients + TCP server + Mesh nodes
    NodeContainer tcpClientNodes;
    tcpClientNodes.Create(tcpClients);

    Ptr<Node> tcpServerNode = CreateObject<Node>();
    NodeContainer meshNodesContainer;
    meshNodesContainer.Create(meshNodes);

    NodeContainer allNodes;
    allNodes.Add(tcpClientNodes);
    allNodes.Add(tcpServerNode);
    allNodes.Add(meshNodesContainer);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Point-to-point helper for connecting TCP clients to server
    PointToPointHelper p2pTcp;
    p2pTcp.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pTcp.SetChannelAttribute("Delay", StringValue("5ms"));

    // Connect each TCP client to the server
    NetDeviceContainer tcpDevices;
    Ipv4InterfaceContainer tcpInterfaces;
    for (uint32_t i = 0; i < tcpClients; ++i) {
        NodeContainer clientServer(tcpClientNodes.Get(i), tcpServerNode);
        NetDeviceContainer devices = p2pTcp.Install(clientServer);
        Ipv4AddressHelper address;
        std::string baseIp = "10.0." + std::to_string(i + 1) + ".0";
        address.SetBase(baseIp.c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        tcpInterfaces.Add(interfaces);
    }

    // Create a mesh topology among mesh nodes using point-to-point links
    PointToPointHelper p2pMesh;
    p2pMesh.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pMesh.SetChannelAttribute("Delay", StringValue("10ms"));

    // Full mesh connection (each node connected to every other node)
    NetDeviceContainer meshDevices[meshNodes][meshNodes];
    Ipv4InterfaceContainer meshInterfaces[meshNodes][meshNodes];

    for (uint32_t i = 0; i < meshNodes; ++i) {
        for (uint32_t j = i + 1; j < meshNodes; ++j) {
            NodeContainer pair(meshNodesContainer.Get(i), meshNodesContainer.Get(j));
            NetDeviceContainer devs = p2pMesh.Install(pair);
            Ipv4AddressHelper address;
            std::string baseIp = "192.168." + std::to_string(i + 1) + "." + std::to_string(j + 1) + ".0";
            address.SetBase(baseIp.c_str(), "255.255.255.0");
            Ipv4InterfaceContainer ifaces = address.Assign(devs);
            meshInterfaces[i][j] = ifaces;
            meshInterfaces[j][i] = ifaces; // Symmetric assignment
        }
    }

    // Assign IP addresses to mesh nodes manually or via routing protocol (simplified here)

    // Set up TCP server (central node in star topology)
    uint16_t tcpPort = 5000;
    Address tcpServerAddress = InetSocketAddress(Ipv4Address::GetAny(), tcpPort);
    PacketSinkHelper packetSinkTcp("ns3::TcpSocketFactory", tcpServerAddress);
    ApplicationContainer sinkAppTcp = packetSinkTcp.Install(tcpServerNode);
    sinkAppTcp.Start(Seconds(0.0));
    sinkAppTcp.Stop(Seconds(simulationTime));

    // Set up TCP clients
    for (uint32_t i = 0; i < tcpClients; ++i) {
        BulkSendHelper source("ns3::TcpSocketFactory", 
                              InetSocketAddress(tcpInterfaces.GetAddress(1, i), tcpPort));
        source.SetAttribute("MaxBytes", UintegerValue(0)); // continuous sending
        ApplicationContainer clientApp = source.Install(tcpClientNodes.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simulationTime));
    }

    // Set up UDP Echo Servers on mesh nodes
    uint16_t udpPort = 9;
    UdpEchoServerHelper echoServer(udpPort);
    ApplicationContainer serverApps = echoServer.Install(meshNodesContainer);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // Set up UDP Echo Clients between mesh nodes
    for (uint32_t i = 0; i < meshNodes; ++i) {
        for (uint32_t j = 0; j < meshNodes; ++j) {
            if (i != j) {
                Ipv4Address destAddr = meshInterfaces[i][j].GetAddress(1); // assuming index 1 is destination
                UdpEchoClientHelper echoClient(destAddr, udpPort);
                echoClient.SetAttribute("MaxPackets", UintegerValue(10));
                echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
                echoClient.SetAttribute("PacketSize", UintegerValue(512));
                ApplicationContainer clientApp = echoClient.Install(meshNodesContainer.Get(i));
                clientApp.Start(Seconds(2.0));
                clientApp.Stop(Seconds(simulationTime));
            }
        }
    }

    // Setup FlowMonitor to measure performance metrics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.Install(allNodes);

    // Setup NetAnim for visualization
    AnimationInterface anim("hybrid-topology.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("routing-table-writer.xml", Seconds(0), Seconds(simulationTime), Seconds(0.5));

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Output flow statistics
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(monitor->GetClassifier().Peek())->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
        std::cout << "  Throughput: " << (iter->second.rxBytes * 8.0) / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps" << std::endl;
        std::cout << "  Average Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}