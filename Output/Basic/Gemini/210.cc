#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridNetworkSimulation");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetAllPrintLevel(LOG_LEVEL_ERROR);

    // Star Topology (TCP)
    uint32_t nStarClients = 3;

    NodeContainer starNodes;
    starNodes.Create(nStarClients + 1); // n clients + 1 server

    PointToPointHelper p2pStar;
    p2pStar.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pStar.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer starDevices;
    for (uint32_t i = 0; i < nStarClients; ++i) {
        NetDeviceContainer link = p2pStar.Install(starNodes.Get(i), starNodes.Get(nStarClients));
        starDevices.Add(link.Get(0));
        starDevices.Add(link.Get(1));
    }

    // Mesh Topology (UDP)
    uint32_t nMeshNodes = 4;

    NodeContainer meshNodes;
    meshNodes.Create(nMeshNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer meshDevices = wifi.Install(phy, mac, meshNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("50.0"),
                                  "Y", StringValue("50.0"),
                                  "Rho", StringValue("10"),
                                  "Theta", StringValue("360"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);


    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(starNodes);
    internet.Install(meshNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer starInterfaces = ipv4.Assign(starDevices);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer meshInterfaces = ipv4.Assign(meshDevices);

    // TCP Applications (Star)
    uint16_t port = 50000;
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(starInterfaces.GetAddress(nStarClients), port));
    source.SetAttribute("SendSize", UintegerValue(1024));

    ApplicationContainer sourceApps;
    for (uint32_t i = 0; i < nStarClients; ++i) {
        sourceApps.Add(source.Install(starNodes.Get(i)));
    }
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(starNodes.Get(nStarClients));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // UDP Applications (Mesh)
    uint16_t udpPort = 9;

    for (uint32_t i = 0; i < nMeshNodes; ++i) {
        for (uint32_t j = 0; j < nMeshNodes; ++j) {
            if (i != j) {
                UdpClientHelper client(meshInterfaces.GetAddress(j), udpPort);
                client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
                client.SetAttribute("MaxPackets", UintegerValue(1000));
                ApplicationContainer clientApps = client.Install(meshNodes.Get(i));
                clientApps.Start(Seconds(2.0));
                clientApps.Stop(Seconds(9.0));
            }
        }

        UdpServerHelper server(udpPort);
        ApplicationContainer serverApps = server.Install(meshNodes.Get(i));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));
    }

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable global static routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Animation
    AnimationInterface anim("hybrid-network.xml");
    anim.SetConstantPosition(starNodes.Get(nStarClients), 10.0, 50.0);
    for (uint32_t i = 0; i < nStarClients; ++i) {
        anim.SetConstantPosition(starNodes.Get(i), 10.0, 70.0 + i * 10);
    }
    for (uint32_t i = 0; i < nMeshNodes; ++i) {
        anim.SetConstantPosition(meshNodes.Get(i), 60.0 + i * 15, 50.0);
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Delay Sum:  " << i->second.delaySum << "\n";
    }

    monitor->SerializeToXmlFile("hybrid-network.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}