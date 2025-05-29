#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTcpExample");

int main(int argc, char *argv[])
{
    uint32_t nClients = 5;
    double simTime = 15.0;
    uint16_t port = 50000;

    // Enable TCP logging
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes: 1 server, nClients clients
    NodeContainer serverNode;
    serverNode.Create(1);
    NodeContainer clientNodes;
    clientNodes.Create(nClients);

    // For position assignment for NetAnim
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(50,50,0)); // server in the center
    double angleStep = 2 * M_PI / nClients;
    double radius = 40;
    for (uint32_t i=0; i<nClients; ++i) {
        double angle = i * angleStep;
        double x = 50 + radius * std::cos(angle);
        double y = 50 + radius * std::sin(angle);
        positionAlloc->Add(Vector(x, y, 0));
    }

    // Point-to-point helper setup
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Containers and Address helpers
    InternetStackHelper stack;
    stack.Install(serverNode);
    stack.Install(clientNodes);

    Ipv4AddressHelper address;
    std::vector<NetDeviceContainer> deviceContainers;
    std::vector<Ipv4InterfaceContainer> interfaceContainers;

    // Each client connects to server over dedicated p2p link, addresses 10.1.i.0/24
    for(uint32_t i = 0; i<nClients; ++i) {
        NodeContainer pair(serverNode.Get(0), clientNodes.Get(i));
        NetDeviceContainer devices = p2p.Install(pair);
        deviceContainers.push_back(devices);

        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        interfaceContainers.push_back(interfaces);
    }

    // Install PacketSink on server, one per incoming connection
    ApplicationContainer sinkApps;
    for(uint32_t i=0; i<nClients; ++i) {
        Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny(), port + i));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(serverNode.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simTime));
        sinkApps.Add(sinkApp);
    }

    // Install BulkSend applications on clients
    ApplicationContainer clientApps;
    for(uint32_t i=0; i<nClients; ++i) {
        BulkSendHelper bulkSender("ns3::TcpSocketFactory",
            InetSocketAddress(interfaceContainers[i].GetAddress(0), port + i));
        bulkSender.SetAttribute("MaxBytes", UintegerValue(5*1024*1024)); // 5MB per client
        ApplicationContainer app = bulkSender.Install(clientNodes.Get(i));
        app.Start(Seconds(1.0 + 0.2*i));
        app.Stop(Seconds(simTime));
        clientApps.Add(app);
    }

    // NetAnim setup
    AnimationInterface anim("star-topology-animation.xml");
    for (uint32_t i=0; i<1+nClients; ++i)
        anim.SetConstantPosition((i==0)?serverNode.Get(0):clientNodes.Get(i-1),
                                positionAlloc->GetNext()->x, positionAlloc->GetNext()->y);

    // FlowMonitor
    FlowMonitorHelper flowMonHelper;
    Ptr<FlowMonitor> flowMon = flowMonHelper.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    flowMon->CheckForLostPackets();

    // Metrics computation
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMon->GetFlowStats();
    for (auto& flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationPort >= port && t.destinationPort < port + nClients) {
            double txBytes = flow.second.txBytes * 8.0 / 1e6; // Bits to Mbit
            double throughput = txBytes / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds());
            double latency = 0.0;
            if (flow.second.rxPackets > 0)
                latency = flow.second.delaySum.GetSeconds() / flow.second.rxPackets;
            std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "Throughput: " << throughput << " Mbps\n";
            std::cout << "Mean Latency: " << latency*1000 << " ms\n";
            std::cout << "Packet Loss: " << (flow.second.txPackets - flow.second.rxPackets) << " packets\n";
        }
    }

    Simulator::Destroy();
    return 0;
}