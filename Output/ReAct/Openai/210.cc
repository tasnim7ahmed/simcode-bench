#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/mesh-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopoTcpUdp");

// TCP Star Topology Params
static const uint32_t nStarClients = 4; // Number of TCP clients

// Mesh Topology Params
static const uint32_t nMeshNodes = 6; // Number of mesh nodes

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // 1. Create Nodes
    NodeContainer starServer;
    starServer.Create(1);
    NodeContainer starClients;
    starClients.Create(nStarClients);

    NodeContainer meshNodes;
    meshNodes.Create(nMeshNodes);

    // 2. Install Internet Stack
    InternetStackHelper internet;
    internet.Install(starServer);
    internet.Install(starClients);
    internet.Install(meshNodes);

    // 3. Star Topology (TCP)
    NodeContainer starAllNodes;
    starAllNodes.Add(starServer.Get(0));
    for (uint32_t i = 0; i < nStarClients; ++i)
        starAllNodes.Add(starClients.Get(i));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("20Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer starDevices[nStarClients];
    Ipv4InterfaceContainer starInterfaces[nStarClients];

    Ipv4AddressHelper ipv4;
    for (uint32_t i = 0; i < nStarClients; ++i)
    {
        NodeContainer link(starClients.Get(i), starServer.Get(0));
        starDevices[i] = p2p.Install(link);

        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        starInterfaces[i] = ipv4.Assign(starDevices[i]);
    }

    // 4. Mesh Topology (UDP)
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    mesh.SetNumberOfInterfaces(1);

    NetDeviceContainer meshDevices = mesh.Install(wifiPhy, meshNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(25.0),
                                 "DeltaY", DoubleValue(25.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);

    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer meshInterfaces = ipv4.Assign(meshDevices);

    // 5. TCP Applications (BulkSend -> PacketSink)
    uint16_t tcpPort = 50000;
    ApplicationContainer tcpServerApps;
    ApplicationContainer tcpClientApps;
    for (uint32_t i = 0; i < nStarClients; ++i)
    {
        // TCP Server (sink) on starServer
        Address sinkAddress(InetSocketAddress(starInterfaces[i].GetAddress(1), tcpPort + i));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
        ApplicationContainer sinkApp = packetSinkHelper.Install(starServer.Get(0));
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(20.0));
        tcpServerApps.Add(sinkApp);

        // TCP Client
        BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
        bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
        ApplicationContainer clientApp = bulkSend.Install(starClients.Get(i));
        clientApp.Start(Seconds(2.0 + i));
        clientApp.Stop(Seconds(19.0));
        tcpClientApps.Add(clientApp);
    }

    // 6. UDP Applications (UDPClient -> UDPServer) in Mesh, multiple pairs
    uint16_t udpPortBase = 60000;
    ApplicationContainer udpServerApps;
    ApplicationContainer udpClientApps;
    for (uint32_t i = 0; i < nMeshNodes; ++i)
    {
        // Install UDPServer on each node
        UdpServerHelper udpServer(udpPortBase + i);
        ApplicationContainer serverApp = udpServer.Install(meshNodes.Get(i));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(20.0));
        udpServerApps.Add(serverApp);
    }
    for (uint32_t i = 0; i < nMeshNodes; ++i)
    {
        // Each node sends UDP traffic to the next node (ad-hoc, wrap around)
        uint32_t dstIdx = (i + 1) % nMeshNodes;
        UdpClientHelper udpClient(meshInterfaces.GetAddress(dstIdx), udpPortBase + dstIdx);
        udpClient.SetAttribute("MaxPackets", UintegerValue(3200));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(5)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = udpClient.Install(meshNodes.Get(i));
        clientApp.Start(Seconds(3.0 + i));
        clientApp.Stop(Seconds(19.0));
        udpClientApps.Add(clientApp);
    }

    // 7. NetAnim Setup
    AnimationInterface anim("hybrid-topo-tcp-udp.xml");
    // Star nodes: clients blue, server red
    for (uint32_t i = 0; i < nStarClients; ++i)
        anim.UpdateNodeColor(starClients.Get(i)->GetId(), 0, 0, 255); // Blue
    anim.UpdateNodeColor(starServer.Get(0)->GetId(), 255, 0, 0);     // Red
    // Mesh nodes: green
    for (uint32_t i = 0; i < nMeshNodes; ++i)
        anim.UpdateNodeColor(meshNodes.Get(i)->GetId(), 0, 128, 0); // Green

    // 8. FlowMonitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    // 9. Run Simulation
    Simulator::Stop(Seconds(21.0));
    Simulator::Run();

    // 10. Report results
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    std::cout << "FlowID  SrcAddr:Port   DstAddr:Port  Protocol  TxPackets  RxPackets  Lost  TxBytes  RxBytes  Throughput(Mbps)  MeanDelay(ms)" << std::endl;

    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::string proto = (t.protocol == 6 ? "TCP" : (t.protocol == 17 ? "UDP" : "???"));

        double throughputMbps = (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1e6;
        double mean_delay_ms = 0.0;
        if (flow.second.rxPackets > 0)
            mean_delay_ms = 1e3 * flow.second.delaySum.GetSeconds() / flow.second.rxPackets;

        std::cout << flow.first << "  "
                  << t.sourceAddress << ":" << t.sourcePort << "  "
                  << t.destinationAddress << ":" << t.destinationPort << "  "
                  << proto << "  "
                  << flow.second.txPackets << "  "
                  << flow.second.rxPackets << "  "
                  << (flow.second.lostPackets) << "  "
                  << flow.second.txBytes << "  "
                  << flow.second.rxBytes << "  "
                  << throughputMbps << "  "
                  << mean_delay_ms << std::endl;
    }

    Simulator::Destroy();
    return 0;
}