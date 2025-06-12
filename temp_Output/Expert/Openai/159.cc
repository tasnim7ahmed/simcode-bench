#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Create nodes
    NodeContainer starCenter;
    starCenter.Create(1); // Node 0

    NodeContainer starPeripherals;
    starPeripherals.Create(3); // Nodes 1,2,3

    NodeContainer busNodes;
    busNodes.Create(3); // Nodes 4,5,6

    // Combine all nodes for InternetStack installation
    NodeContainer allNodes;
    allNodes.Add(starCenter);
    allNodes.Add(starPeripherals);
    allNodes.Add(busNodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Point-to-Point helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // STAR TOPOLOGY: Center node 0 <-> peripheral nodes 1,2,3
    NetDeviceContainer starDevices[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        NodeContainer pair(starCenter.Get(0), starPeripherals.Get(i));
        starDevices[i] = p2p.Install(pair);
    }

    // BUS TOPOLOGY: Node 4 <-> Node 5 <-> Node 6 (sequential links)
    NetDeviceContainer busDev1 = p2p.Install(NodeContainer(busNodes.Get(0), busNodes.Get(1))); // 4 <-> 5
    NetDeviceContainer busDev2 = p2p.Install(NodeContainer(busNodes.Get(1), busNodes.Get(2))); // 5 <-> 6

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer starIfs[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        starIfs[i] = address.Assign(starDevices[i]);
    }

    address.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer busIf1 = address.Assign(busDev1);

    address.SetBase("10.2.2.0", "255.255.255.0");
    Ipv4InterfaceContainer busIf2 = address.Assign(busDev2);

    // UDP Echo server on Node 0 (star center)
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(starCenter.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    // UDP Echo clients on Node 4 and 5 (bus nodes)
    // Both clients send to Node 0 (use first star interface's address on Node 0)
    Address serverAddress = starIfs[0].GetAddress(0);

    UdpEchoClientHelper echoClient(serverAddress, echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1 = echoClient.Install(busNodes.Get(0)); // Node 4
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(19.0));

    ApplicationContainer clientApps2 = echoClient.Install(busNodes.Get(1)); // Node 5
    clientApps2.Start(Seconds(2.5));
    clientApps2.Stop(Seconds(19.5));

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << iter->second.lostPackets << "\n";
        if (iter->second.rxPackets > 0)
        {
            std::cout << "  Mean delay: " << (iter->second.delaySum.GetSeconds() / iter->second.rxPackets) << " s\n";
        }
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1024 << " Kbps\n";
    }

    Simulator::Destroy();
    return 0;
}