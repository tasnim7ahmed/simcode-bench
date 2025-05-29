#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("StarTcpNetwork");

int
main (int argc, char *argv[])
{
    uint32_t nClients = 5;
    double simulationTime = 10.0; // seconds
    std::string dataRate = "10Mbps";
    std::string delay = "2ms";
    uint32_t packetSize = 1024;
    uint32_t numPackets = 100;
    double appStart = 1.0;
    double appStop = 9.0;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer serverNode;
    serverNode.Create(1);
    NodeContainer clientNodes;
    clientNodes.Create(nClients);

    NodeContainer allNodes;
    allNodes.Add(serverNode);
    allNodes.Add(clientNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue(delay));

    std::vector<NetDeviceContainer> clientDevicesList;
    NetDeviceContainer serverDevices;

    for (uint32_t i = 0; i < nClients; ++i)
    {
        NodeContainer pair(serverNode.Get(0), clientNodes.Get(i));
        NetDeviceContainer link = p2p.Install(pair);
        serverDevices.Add(link.Get(0));
        clientDevicesList.push_back(NetDeviceContainer());
        clientDevicesList[i].Add(link.Get(1));
    }

    InternetStackHelper internet;
    internet.Install(allNodes);

    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> clientInterfaces;
    Ipv4InterfaceContainer serverInterfaces;

    for (uint32_t i = 0; i < nClients; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        ipv4.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        NetDeviceContainer netDevices;
        netDevices.Add(serverDevices.Get(i));
        netDevices.Add(clientDevicesList[i].Get(0));
        Ipv4InterfaceContainer interfaces = ipv4.Assign(netDevices);
        serverInterfaces.Add(interfaces.Get(0));
        clientInterfaces.push_back(Ipv4InterfaceContainer());
        clientInterfaces[i].Add(interfaces.Get(1));
        ipv4.NewNetwork();
    }

    // Install applications
    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(serverInterfaces.GetAddress(0), port));
    // TCP Server (PacketSink)
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    ApplicationContainer sinkApp = packetSinkHelper.Install(serverNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    // TCP Clients (BulkSend)
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nClients; ++i)
    {
        BulkSendHelper bulkSend("ns3::TcpSocketFactory", serverAddress);
        bulkSend.SetAttribute("MaxBytes", UintegerValue(packetSize * numPackets));
        bulkSend.SetAttribute("SendSize", UintegerValue(packetSize));
        ApplicationContainer app = bulkSend.Install(clientNodes.Get(i));
        app.Start(Seconds(appStart + i * 0.1));
        app.Stop(Seconds(appStop));
        clientApps.Add(app);
    }

    // Flow Monitor for statistics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // NetAnim
    AnimationInterface anim("star-topology.xml");
    anim.SetConstantPosition(serverNode.Get(0), 50, 50);
    for (uint32_t i = 0; i < nClients; ++i)
    {
        double angle = 2 * M_PI * i / nClients;
        double x = 50 + 30 * std::cos(angle);
        double y = 50 + 30 * std::sin(angle);
        anim.SetConstantPosition(clientNodes.Get(i), x, y);
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Calculate throughput, latency, and packet loss
    double total_throughput = 0.0; // bit/s
    double total_delay = 0.0; // ms
    uint32_t total_rxPackets = 0, total_txPackets = 0, lostPackets = 0;
    uint32_t flowsWithPackets = 0;

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (auto const& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationPort != port)
            continue;

        double throughput = (flow.second.rxBytes * 8.0) / (simulationTime - appStart); // bit/s
        total_throughput += throughput;

        if (flow.second.rxPackets > 0)
        {
            double meanDelay = (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) * 1000; // ms
            total_delay += meanDelay;
            flowsWithPackets++;
        }
        total_rxPackets += flow.second.rxPackets;
        total_txPackets += flow.second.txPackets;
        lostPackets += flow.second.lostPackets;
    }

    std::cout << "========== Simulation Results ==========" << std::endl;
    std::cout << "Number of clients: " << nClients << std::endl;
    std::cout << "Aggregate throughput: " << total_throughput / 1e6 << " Mbps" << std::endl;
    if (flowsWithPackets > 0)
        std::cout << "Average latency: " << total_delay / flowsWithPackets << " ms" << std::endl;
    else
        std::cout << "Average latency: n/a" << std::endl;
    std::cout << "Total packets sent: " << total_txPackets << std::endl;
    std::cout << "Total packets received: " << total_rxPackets << std::endl;
    std::cout << "Total packet loss: " << lostPackets << std::endl;
    std::cout << "Packet loss ratio: " << (total_txPackets > 0 ? (100.0 * lostPackets / total_txPackets) : 0.0) << " %" << std::endl;

    Simulator::Destroy();
    return 0;
}