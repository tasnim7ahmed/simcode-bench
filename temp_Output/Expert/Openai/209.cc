#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("StarTopologyTcpExample");

int
main (int argc, char *argv[])
{
    uint32_t numClients = 5;
    double simTime = 10.0; // seconds
    std::string dataRate = "5Mbps";
    std::string delay = "2ms";
    uint32_t packetSize = 1024; // bytes
    uint32_t numPackets = 100;

    CommandLine cmd;
    cmd.Parse (argc, argv);

    NodeContainer serverNode;
    serverNode.Create(1);

    NodeContainer clientNodes;
    clientNodes.Create(numClients);

    NodeContainer allNodes;
    allNodes.Add(serverNode);
    allNodes.Add(clientNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(allNodes);

    // Create point-to-point links between server and each client
    std::vector<NodeContainer> clientToServerLinks;
    std::vector<NetDeviceContainer> netDevices;
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue (dataRate));
    p2p.SetChannelAttribute ("Delay", StringValue (delay));

    for (uint32_t i = 0; i < numClients; ++i)
    {
        NodeContainer link (clientNodes.Get(i), serverNode.Get(0));
        clientToServerLinks.push_back(link);
        netDevices.push_back(p2p.Install(link));
    }

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces;

    for (uint32_t i = 0; i < numClients; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces.push_back(address.Assign(netDevices[i]));
    }

    // Server Application
    uint16_t port = 9000;
    Address serverAddress (InetSocketAddress (interfaces[0].GetAddress(1), port));

    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory",
                                       InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer serverApps = packetSinkHelper.Install (serverNode.Get(0));
    serverApps.Start (Seconds (0.0));
    serverApps.Stop (Seconds (simTime + 1));

    // Client Applications (all connect to server)
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numClients; ++i)
    {
        OnOffHelper client ("ns3::TcpSocketFactory",
                            Address (InetSocketAddress (interfaces[i].GetAddress (1), port)));
        client.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
        client.SetAttribute ("PacketSize", UintegerValue (packetSize));
        client.SetAttribute ("MaxBytes", UintegerValue (packetSize * numPackets));
        client.SetAttribute ("StartTime", TimeValue (Seconds (1.0 + 0.2 * i)));
        client.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));
        clientApps.Add (client.Install (clientNodes.Get(i)));
    }

    // Enable Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // NetAnim setup
    AnimationInterface anim ("star-topology-tcp.xml");
    anim.SetConstantPosition (serverNode.Get(0), 50, 50);
    double angleStep = 2*M_PI/numClients;
    double radius = 30;
    for (uint32_t i = 0; i < numClients; ++i)
    {
        double angle = i * angleStep;
        double x = 50 + radius * std::cos(angle);
        double y = 50 + radius * std::sin(angle);
        anim.SetConstantPosition (clientNodes.Get(i), x, y);
    }

    // Flow Monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds (simTime + 2));
    Simulator::Run();

    // Analyze results
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

    double sumThroughput = 0.0;
    double sumDelay = 0.0;
    uint32_t totalPacketsRx = 0, totalPacketsTx = 0, totalLost = 0, flowCount = 0;

    for (auto const& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationPort != port)
            continue;

        double throughput = flow.second.rxBytes * 8.0 / (simTime * 1000000.0); // Mbps
        sumThroughput += throughput;
        totalPacketsRx += flow.second.rxPackets;
        totalPacketsTx += flow.second.txPackets;
        totalLost += flow.second.lostPackets;
        if (flow.second.rxPackets > 0)
            sumDelay += (flow.second.delaySum.GetSeconds() / flow.second.rxPackets);
        ++flowCount;
    }

    if (flowCount > 0 && totalPacketsRx > 0)
    {
        std::cout << "Average throughput: " << sumThroughput / flowCount << " Mbps\n";
        std::cout << "Average latency: " << sumDelay / flowCount << " s\n";
        std::cout << "Total packet loss: " << totalLost << "\n";
        std::cout << "Packet loss ratio: " << 100.0*totalLost/totalPacketsTx << " %\n";
    }
    else
    {
        std::cout << "No data collected.\n";
    }

    Simulator::Destroy();
    return 0;
}