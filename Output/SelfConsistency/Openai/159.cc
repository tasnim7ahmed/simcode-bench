#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 0 = Star Center, 1-3 = Star Peripherals, 4-6 = Bus Nodes
    NodeContainer starCenter;
    starCenter.Create(1); // Node 0

    NodeContainer starPeripherals;
    starPeripherals.Create(3); // Nodes 1-3

    NodeContainer busNodes;
    busNodes.Create(3); // Nodes 4-6

    // Aggregate all nodes for InternetStack
    NodeContainer allNodes;
    allNodes.Add(starCenter);
    allNodes.Add(starPeripherals);
    allNodes.Add(busNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(allNodes);

    // ----------------------
    // Create point-to-point links for Star Topology (0-1, 0-2, 0-3)
    PointToPointHelper p2pStar;
    p2pStar.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pStar.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer devices_star01 = p2pStar.Install(starCenter.Get(0), starPeripherals.Get(0)); // 0-1
    NetDeviceContainer devices_star02 = p2pStar.Install(starCenter.Get(0), starPeripherals.Get(1)); // 0-2
    NetDeviceContainer devices_star03 = p2pStar.Install(starCenter.Get(0), starPeripherals.Get(2)); // 0-3

    // Create point-to-point links for Bus Topology (4-5-6 as 4-5 and 5-6)
    PointToPointHelper p2pBus;
    p2pBus.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pBus.SetChannelAttribute("Delay", StringValue("4ms"));
    NetDeviceContainer devices_bus45 = p2pBus.Install(busNodes.Get(0), busNodes.Get(1)); // 4-5
    NetDeviceContainer devices_bus56 = p2pBus.Install(busNodes.Get(1), busNodes.Get(2)); // 5-6

    // Connect bus to central node (star center) by linking Node 4 <-> Node 0
    // (This forms the hybrid topology)
    PointToPointHelper p2pHybrid;
    p2pHybrid.SetDeviceAttribute("DataRate", StringValue("8Mbps"));
    p2pHybrid.SetChannelAttribute("Delay", StringValue("6ms"));
    NetDeviceContainer devices_hybrid = p2pHybrid.Install(busNodes.Get(0), starCenter.Get(0)); // 4-0

    // ----------------------
    // Assign IP addresses
    Ipv4AddressHelper address;

    // Star topology addresses
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces_star01 = address.Assign(devices_star01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces_star02 = address.Assign(devices_star02);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces_star03 = address.Assign(devices_star03);

    // Bus topology addresses
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces_bus45 = address.Assign(devices_bus45);

    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces_bus56 = address.Assign(devices_bus56);

    // Hybrid link between bus and star
    address.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces_hybrid = address.Assign(devices_hybrid);

    // ----------------------
    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ----------------------
    // UDP Echo Server on Node 0 (Central Node)
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(starCenter.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    // UDP Echo Client on Node 4 (busNodes.Get(0)) and Node 5 (busNodes.Get(1))
    UdpEchoClientHelper echoClient(interfaces_star01.GetAddress(0), echoPort); // To Node 0's IP on 10.1.1.1 interface
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));

    ApplicationContainer clientApps1 = echoClient.Install(busNodes.Get(0)); // Node 4
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(19.0));

    ApplicationContainer clientApps2 = echoClient.Install(busNodes.Get(1)); // Node 5
    clientApps2.Start(Seconds(3.0));
    clientApps2.Stop(Seconds(19.0));

    // ----------------------
    // Install FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Print per-flow statistics for debugging and summary
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << iter->second.lostPackets << "\n";
        std::cout << "  Packet Loss Ratio: " 
                  << (iter->second.txPackets > 0 ? 
                      (double)iter->second.lostPackets / iter->second.txPackets : 0.0) << "\n";
        std::cout << "  DelaySum: " << iter->second.delaySum.GetSeconds() << " s\n";
        std::cout << "  Average Delay: " 
                  << (iter->second.rxPackets > 0 ? 
                      iter->second.delaySum.GetSeconds() / iter->second.rxPackets : 0.0) << " s\n";
    }

    Simulator::Destroy();
    return 0;
}