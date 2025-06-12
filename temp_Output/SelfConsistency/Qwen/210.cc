#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridNetworkSimulation");

int main(int argc, char *argv[])
{
    // Default configuration values
    uint32_t numTcpClients = 5;
    uint32_t numUdpNodes = 6;
    double simulationTime = 10.0; // seconds

    // Command line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("numTcpClients", "Number of TCP clients in star topology", numTcpClients);
    cmd.AddValue("numUdpNodes", "Number of UDP nodes in mesh topology", numUdpNodes);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes for TCP star topology
    NodeContainer tcpStarNodes;
    tcpStarNodes.Create(numTcpClients + 1); // +1 for the server
    Ptr<Node> tcpServerNode = tcpStarNodes.Get(0);

    // Create nodes for UDP mesh topology
    NodeContainer udpMeshNodes;
    udpMeshNodes.Create(numUdpNodes);

    // Combine all nodes for global internet install
    NodeContainer allNodes;
    allNodes.Add(tcpStarNodes);
    allNodes.Add(udpMeshNodes);

    // Setup point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Build star topology (TCP)
    NetDeviceContainer tcpStarDevices;
    Ipv4InterfaceContainer tcpStarInterfaces;
    InternetStackHelper stack;
    stack.Install(allNodes);

    for (uint32_t i = 1; i < tcpStarNodes.GetN(); ++i)
    {
        PointToPointHelper p2pStar;
        p2pStar.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2pStar.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = p2pStar.Install(NodeContainer(tcpStarNodes.Get(0), tcpStarNodes.Get(i)));
        tcpStarDevices.Add(devices);

        Ipv4InterfaceContainer interfaces = InternetStackHelper::AssignIpv4Addresses(devices, Ipv4AddressHelper("10.1.0.0", "255.255.255.0"));
        tcpStarInterfaces.Add(interfaces);
    }

    // Build mesh topology (UDP)
    std::vector<NetDeviceContainer> udpMeshDevices;
    std::vector<Ipv4InterfaceContainer> udpMeshInterfaces;

    for (uint32_t i = 0; i < udpMeshNodes.GetN(); ++i)
    {
        for (uint32_t j = i + 1; j < udpMeshNodes.GetN(); ++j)
        {
            PointToPointHelper p2pMesh;
            p2pMesh.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
            p2pMesh.SetChannelAttribute("Delay", StringValue("2ms"));

            NetDeviceContainer devices = p2pMesh.Install(NodeContainer(udpMeshNodes.Get(i), udpMeshNodes.Get(j)));
            udpMeshDevices.push_back(devices);

            std::ostringstream subnet;
            subnet << "10.2." << (i * 10 + j) << ".0";
            Ipv4AddressHelper address;
            address.SetBase(subnet.str().c_str(), "255.255.255.0");
            Ipv4InterfaceContainer interfaces = address.Assign(devices);
            udpMeshInterfaces.push_back(interfaces);
        }
    }

    // Assign IP addresses to all devices
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install applications
    uint16_t port = 9;

    // TCP Applications: Star topology with BulkSend (client) and PacketSink (server)
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer tcpSinkApps = packetSinkHelper.Install(tcpServerNode);
    tcpSinkApps.Start(Seconds(0.0));
    tcpSinkApps.Stop(Seconds(simulationTime));

    for (uint32_t i = 1; i < tcpStarNodes.GetN(); ++i)
    {
        BulkSendHelper bulkSend("ns3::TcpSocketFactory", InetSocketAddress(tcpStarInterfaces.GetAddress(0, 0), port));
        bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
        ApplicationContainer sourceApps = bulkSend.Install(tcpStarNodes.Get(i));
        sourceApps.Start(Seconds(1.0));
        sourceApps.Stop(Seconds(simulationTime - 0.1));
    }

    // UDP Applications: Mesh topology with UdpEchoClient and Server
    for (uint32_t i = 0; i < udpMeshNodes.GetN(); ++i)
    {
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApps = echoServer.Install(udpMeshNodes.Get(i));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(simulationTime));
    }

    for (uint32_t i = 0; i < udpMeshNodes.GetN(); ++i)
    {
        for (uint32_t j = 0; j < udpMeshNodes.GetN(); ++j)
        {
            if (i != j)
            {
                Ipv4Address remoteAddr = udpMeshNodes.Get(j)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
                UdpEchoClientHelper echoClient(remoteAddr, port);
                echoClient.SetAttribute("MaxPackets", UintegerValue(20));
                echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
                echoClient.SetAttribute("PacketSize", UintegerValue(1024));

                ApplicationContainer clientApps = echoClient.Install(udpMeshNodes.Get(i));
                clientApps.Start(Seconds(2.0));
                clientApps.Stop(Seconds(simulationTime - 0.1));
            }
        }
    }

    // Setup FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Setup NetAnim
    AnimationInterface anim("hybrid-network.xml");
    anim.SetConstantPosition(tcpServerNode, 0, 0);
    for (uint32_t i = 1; i < tcpStarNodes.GetN(); ++i)
    {
        double angle = 2 * M_PI * (i - 1) / (tcpStarNodes.GetN() - 1);
        double x = 20 * cos(angle);
        double y = 20 * sin(angle);
        anim.SetConstantPosition(tcpStarNodes.Get(i), x, y);
    }

    for (uint32_t i = 0; i < udpMeshNodes.GetN(); ++i)
    {
        double angle = 2 * M_PI * i / udpMeshNodes.GetN();
        double x = 80 + 20 * cos(angle);
        double y = 20 * sin(angle);
        anim.SetConstantPosition(udpMeshNodes.Get(i), x, y);
    }

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Output FlowMonitor results
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / simulationTime / 1000 / 1000 << " Mbps\n";
        std::cout << "  Mean Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s\n";
    }

    Simulator::Destroy();
    return 0;
}