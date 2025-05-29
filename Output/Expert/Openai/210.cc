#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t nStarClients = 4;
    uint32_t nMeshNodes = 6;
    uint32_t tcpPort = 8080;
    uint32_t udpPort = 9090;
    double simTime = 20.0; // seconds

    // Enable logs as needed
    // LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

    // Star topology (TCP)
    NodeContainer starServer;
    starServer.Create(1);
    NodeContainer starClients;
    starClients.Create(nStarClients);

    // PointToPoint setup for Star
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer starDevices;
    NetDeviceContainer serverDevices;
    NetDeviceContainer clientDevices;

    for (uint32_t i = 0; i < nStarClients; ++i)
    {
        NodeContainer pair(starClients.Get(i), starServer.Get(0));
        NetDeviceContainer link = p2p.Install(pair);
        clientDevices.Add(link.Get(0));
        serverDevices.Add(link.Get(1));
        starDevices.Add(link);
    }

    // Mesh topology (UDP)
    NodeContainer meshNodes;
    meshNodes.Create(nMeshNodes);

    MeshHelper mesh = MeshHelper::Default ();
    mesh.SetStackInstaller ("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType ("RandomStart", TimeValue (Seconds(0.1)));
    mesh.SetNumberOfInterfaces (1);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    NetDeviceContainer meshDevices = mesh.Install (wifiPhy, meshNodes);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(starClients);
    internet.Install(starServer);
    internet.Install(meshNodes);

    // Assign IP addresses for star
    Ipv4AddressHelper starIp;
    std::vector<Ipv4InterfaceContainer> starIfs;
    for (uint32_t i = 0; i < nStarClients; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        starIp.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        NetDeviceContainer devs;
        devs.Add(clientDevices.Get(i));
        devs.Add(serverDevices.Get(i));
        starIfs.push_back(starIp.Assign(devs));
        starIp.NewNetwork();
    }

    // Assign IP addresses for mesh
    Ipv4AddressHelper meshIp;
    meshIp.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer meshIfs = meshIp.Assign(meshDevices);

    // Mobility model
    MobilityHelper mobilityStar;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(30.0, 50.0, 0.0)); // server
    for (uint32_t j = 0; j < nStarClients; ++j)
    {
        double angle = 2 * M_PI * j / nStarClients;
        double x = 30.0 + 20.0 * std::cos(angle);
        double y = 50.0 + 20.0 * std::sin(angle);
        posAlloc->Add(Vector(x, y, 0.0));
    }
    mobilityStar.SetPositionAllocator(posAlloc);
    mobilityStar.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    NodeContainer allStar;
    allStar.Add(starServer);
    allStar.Add(starClients);
    mobilityStar.Install(allStar);

    MobilityHelper mobilityMesh;
    mobilityMesh.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(80.0),
                                      "MinY", DoubleValue(10.0),
                                      "DeltaX", DoubleValue(20.0),
                                      "DeltaY", DoubleValue(20.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));
    mobilityMesh.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(60,140,0,80)));
    mobilityMesh.Install(meshNodes);

    // Star: Install TCP applications
    Address sinkAddress (InetSocketAddress(starIfs[0].GetAddress(1), tcpPort));
    PacketSinkHelper tcpSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer tcpSinkApp = tcpSinkHelper.Install(starServer.Get(0));
    tcpSinkApp.Start (Seconds(0.0));
    tcpSinkApp.Stop (Seconds(simTime));

    for (uint32_t i = 0; i < nStarClients; ++i)
    {
        OnOffHelper tcpSource("ns3::TcpSocketFactory", Address());
        AddressValue remoteAddress(InetSocketAddress(starIfs[i].GetAddress(1), tcpPort));
        tcpSource.SetAttribute("Remote", remoteAddress);
        tcpSource.SetAttribute("DataRate", StringValue("5Mbps"));
        tcpSource.SetAttribute("PacketSize", UintegerValue(1024));
        tcpSource.SetAttribute("StartTime", TimeValue(Seconds(1.0 + i)));
        tcpSource.SetAttribute("StopTime", TimeValue(Seconds(simTime)));
        tcpSource.Install(starClients.Get(i));
    }

    // Mesh: Install UDP applications (all-to-all except loopback)
    ApplicationContainer udpSinks, udpApps;
    for (uint32_t i = 0; i < nMeshNodes; ++i)
    {
        // UDP sink on each node
        PacketSinkHelper udpSinkHelper("ns3::UdpSocketFactory",
            InetSocketAddress(meshIfs.GetAddress(i), udpPort));
        udpSinks.Add(udpSinkHelper.Install(meshNodes.Get(i)));
    }
    udpSinks.Start(Seconds(0.0));
    udpSinks.Stop(Seconds(simTime));

    for (uint32_t i = 0; i < nMeshNodes; ++i)
    {
        for (uint32_t j = 0; j < nMeshNodes; ++j)
        {
            if (i == j)
                continue;
            OnOffHelper udpSource("ns3::UdpSocketFactory",
                                  InetSocketAddress(meshIfs.GetAddress(j), udpPort));
            udpSource.SetAttribute("DataRate", StringValue("2Mbps"));
            udpSource.SetAttribute("PacketSize", UintegerValue(512));
            udpSource.SetAttribute("StartTime", TimeValue(Seconds(2.0 + i * nMeshNodes + j)));
            udpSource.SetAttribute("StopTime", TimeValue(Seconds(simTime)));
            udpApps.Add(udpSource.Install(meshNodes.Get(i)));
        }
    }

    // Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // NetAnim setup
    AnimationInterface anim("hybrid-topology.xml");
    // Node description and constant coloring
    anim.SetConstantPosition(starServer.Get(0), 30.0, 50.0);
    for (uint32_t i = 0; i < nStarClients; ++i)
    {
        anim.SetConstantPosition(starClients.Get(i), 
            posAlloc->GetNext()->GetPosition().x, 
            posAlloc->GetNext()->GetPosition().y);
        anim.UpdateNodeDescription(starClients.Get(i), "StarClient"+std::to_string(i+1));
        anim.UpdateNodeColor(starClients.Get(i), 0, 255, 0);
    }
    anim.UpdateNodeDescription(starServer.Get(0), "StarServer");
    anim.UpdateNodeColor(starServer.Get(0), 255, 0, 0);

    for (uint32_t i = 0; i < nMeshNodes; ++i)
    {
        anim.UpdateNodeDescription(meshNodes.Get(i), "MeshNode"+std::to_string(i+1));
        anim.UpdateNodeColor(meshNodes.Get(i), 0, 0, 255);
    }

    // FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Performance metrics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Protocol: " << (unsigned)t.protocol << std::endl;
        std::cout << "  Tx Packets: " << flow.second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << flow.second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << flow.second.lostPackets << std::endl;
        std::cout << "  Throughput: "
                  << (flow.second.rxBytes * 8.0 / (simTime - 2.0) / 1e6)
                  << " Mbps\n";
        if (flow.second.rxPackets > 0)
        {
            std::cout << "  Mean delay: " 
                      << (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) 
                      << " s\n";
        }
        else
        {
            std::cout << "  Mean delay: NaN\n";
        }
    }

    anim.StopAnimation();

    Simulator::Destroy();
    return 0;
}