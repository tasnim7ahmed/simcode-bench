#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologyTcpUdp");

int main(int argc, char *argv[])
{
    uint32_t nStarClients = 4;
    uint32_t nMeshNodes = 5;
    double simulationTime = 20.0;
    bool enableTracing = true;

    CommandLine cmd;
    cmd.AddValue("nStarClients", "Number of TCP clients in the star", nStarClients);
    cmd.AddValue("nMeshNodes", "Number of UDP mesh nodes", nMeshNodes);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("enableTracing", "Enable NetAnim tracing", enableTracing);
    cmd.Parse(argc, argv);

    // 1. Create nodes
    NodeContainer starClients;
    starClients.Create(nStarClients);
    Ptr<Node> starServer = CreateObject<Node>();
    NodeContainer meshNodes;
    meshNodes.Create(nMeshNodes);

    // 2. Star topology: point-to-point links
    NodeContainer allStarNodes;
    allStarNodes.Add(starServer);
    allStarNodes.Add(starClients);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("20Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer starDevices;
    for (uint32_t i = 0; i < nStarClients; ++i)
    {
        NodeContainer link;
        link.Add(starClients.Get(i));
        link.Add(starServer);

        NetDeviceContainer linkDevs = p2p.Install(link);
        starDevices.Add(linkDevs.Get(0));
        starDevices.Add(linkDevs.Get(1));
    }

    // 3. Mesh topology
    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    MeshHelper::InterfaceIdentifiers meshIfaces;
    NetDeviceContainer meshDevices = mesh.Install(wifiPhy, meshNodes);

    // 4. Internet stack
    InternetStackHelper stack;
    stack.Install(allStarNodes);
    stack.Install(meshNodes);

    // 5. Assign IP addresses
    Ipv4AddressHelper address;

    // Star
    std::vector<Ipv4InterfaceContainer> starIfaces(nStarClients);
    for (uint32_t i = 0; i < nStarClients; ++i)
    {
        address.SetBase(Ipv4Address("10.1." + std::to_string(i + 1) + ".0").c_str(), "255.255.255.0");
        NodeContainer link;
        link.Add(starClients.Get(i));
        link.Add(starServer);
        NetDeviceContainer linkDevs;
        linkDevs.Add(starDevices.Get(2 * i));
        linkDevs.Add(starDevices.Get(2 * i + 1));
        starIfaces[i] = address.Assign(linkDevs);
    }

    // Mesh
    address.SetBase("10.2.0.0", "255.255.255.0");
    Ipv4InterfaceContainer meshIfacesAddr = address.Assign(meshDevices);

    // 6. Mobility
    // Star: fixed
    MobilityHelper mobilityStar;
    Ptr<ListPositionAllocator> starPos = CreateObject<ListPositionAllocator>();
    starPos->Add(Vector(50, 50, 0)); // server center
    double radius = 30.0;
    for (uint32_t i = 0; i < nStarClients; ++i)
    {
        double angle = i * 2 * M_PI / nStarClients;
        starPos->Add(Vector(50 + radius * std::cos(angle), 50 + radius * std::sin(angle), 0));
    }
    mobilityStar.SetPositionAllocator(starPos);
    mobilityStar.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityStar.Install(allStarNodes);

    // Mesh: random rectangular
    MobilityHelper mobilityMesh;
    mobilityMesh.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(150.0),
                                      "MinY", DoubleValue(30.0),
                                      "DeltaX", DoubleValue(20.0),
                                      "DeltaY", DoubleValue(30.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));
    mobilityMesh.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(120, 220, 0, 100)));
    mobilityMesh.Install(meshNodes);

    // 7. Applications
    // TCP - BulkSend (star clients) to a PacketSink at the server
    // Use port 9000 + i for each client for differentiation
    uint16_t tcpBasePort = 9000;
    ApplicationContainer tcpSourceApps, tcpSinkApps;
    for (uint32_t i = 0; i < nStarClients; ++i)
    {
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), tcpBasePort + i));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = packetSinkHelper.Install(starServer);
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simulationTime));
        tcpSinkApps.Add(sinkApp);

        BulkSendHelper bulkSend("ns3::TcpSocketFactory",
                                InetSocketAddress(starIfaces[i].GetAddress(1), tcpBasePort + i));
        bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
        ApplicationContainer clientApp = bulkSend.Install(starClients.Get(i));
        clientApp.Start(Seconds(1.0 + 0.2 * i));
        clientApp.Stop(Seconds(simulationTime));
        tcpSourceApps.Add(clientApp);
    }

    // UDP - OnOff applications between all mesh node pairs (no self-traffic)
    uint16_t udpPort = 8000;
    ApplicationContainer udpApps;
    for (uint32_t src = 0; src < nMeshNodes; ++src)
    {
        for (uint32_t dst = 0; dst < nMeshNodes; ++dst)
        {
            if (src == dst) continue;
            OnOffHelper onoff("ns3::UdpSocketFactory", 
                InetSocketAddress(meshIfacesAddr.GetAddress(dst), udpPort + dst));
            onoff.SetAttribute("DataRate", StringValue("1Mbps"));
            onoff.SetAttribute("PacketSize", UintegerValue(1024));
            onoff.SetAttribute("StartTime", TimeValue(Seconds(2.0 + 0.1 * (src + dst))));
            onoff.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));

            ApplicationContainer app = onoff.Install(meshNodes.Get(src));
            udpApps.Add(app);

            // Install sink on each mesh node
            PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", 
                InetSocketAddress(Ipv4Address::GetAny(), udpPort + dst));
            ApplicationContainer sinkApp = sinkHelper.Install(meshNodes.Get(dst));
            sinkApp.Start(Seconds(1.5));
            sinkApp.Stop(Seconds(simulationTime));
        }
    }

    // 8. FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // 9. NetAnim
    if (enableTracing)
    {
        AnimationInterface anim("hybrid-topology-tcp-udp.xml");

        // Star nodes: 0 - server, 1..n - clients
        anim.SetNodeDescription(starServer, "StarServer");
        for (uint32_t i = 0; i < nStarClients; ++i)
        {
            anim.SetNodeDescription(starClients.Get(i), "StarClient-" + std::to_string(i));
            anim.UpdateNodeColor(starClients.Get(i), 0, 255, 255); // cyan
        }
        anim.UpdateNodeColor(starServer, 255, 0, 0); // red

        // Mesh nodes
        for (uint32_t i = 0; i < nMeshNodes; ++i)
        {
            anim.SetNodeDescription(meshNodes.Get(i), "MeshNode-" + std::to_string(i));
            anim.UpdateNodeColor(meshNodes.Get(i), 0, 255, 0); // green
        }
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // 10. Performance measurements
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress
                  << ", proto=" << unsigned(t.protocol) << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << (flow.second.txPackets - flow.second.rxPackets) << "\n";
        std::cout << "  Throughput: " 
                  << (flow.second.rxBytes * 8.0) / (simulationTime * 1000.0) << " kbps\n";
        if (flow.second.rxPackets > 0)
        {
            std::cout << "  Mean delay: " << (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) << " s\n";
        }
        else
        {
            std::cout << "  Mean delay: N/A\n";
        }
    }

    Simulator::Destroy();
    return 0;
}