#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DisasterRecoveryNetwork");

int main(int argc, char *argv[])
{
    uint32_t nMedicalUnits = 3;
    uint32_t nTeams = 3;
    uint32_t nRespondersPerTeam = 5;
    uint32_t totalTeamNodes = nTeams * nRespondersPerTeam;
    uint32_t totalNodes = 1 + nMedicalUnits + totalTeamNodes; // command + medical + teams

    // Logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Node Containers
    Ptr<Node> commandNode = CreateObject<Node>();
    NodeContainer medicalNodes;
    medicalNodes.Create(nMedicalUnits);
    NodeContainer teamNodes;
    teamNodes.Create(totalTeamNodes);

    // Combine into one container for easy wifi/mobility/internet install
    NodeContainer allNodes;
    allNodes.Add(commandNode);
    allNodes.Add(medicalNodes);
    allNodes.Add(teamNodes);

    // WiFi Configuration
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper chan = YansWifiChannelHelper::Default();
    phy.SetChannel(chan.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    // Enable RTS/CTS for all packets
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));

    NetDeviceContainer devices = wifi.Install(phy, mac, allNodes);

    // Mobility
    MobilityHelper mobility;
    // Assign positions: command center at (50,50), medicals at (100,50), (100,100), (50,100)
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(50.0, 50.0, 0.0)); // Command

    posAlloc->Add(Vector(100.0, 50.0, 0.0));
    posAlloc->Add(Vector(100.0, 100.0, 0.0));
    posAlloc->Add(Vector(50.0, 100.0, 0.0));

    // Team nodes—initial random placement in areas
    for (uint32_t i = 0; i < nTeams; ++i)
    {
        double xOffset = 200.0 + 70.0 * i;
        double yOffset = 50.0 + 70.0 * i;
        for (uint32_t j = 0; j < nRespondersPerTeam; ++j)
        {
            double x = xOffset + 30.0 * (j % 2);
            double y = yOffset + 30.0 * (j / 2);
            posAlloc->Add(Vector(x, y, 0.0));
        }
    }

    mobility.SetPositionAllocator(posAlloc);

    // Set constant position for command and medical
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(commandNode);
    mobility.Install(medicalNodes);

    // Random mobility for team nodes
    MobilityHelper mobTeams;
    mobTeams.SetPositionAllocator(posAlloc);
    mobTeams.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                              "PositionAllocator", PointerValue(posAlloc));
    mobTeams.Install(teamNodes);

    // Internet Stack
    InternetStackHelper internet;
    internet.Install(allNodes);

    // IP addressing: assign all nodes from 10.1.0.0/16
    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up applications: UDP echo servers—one per team
    uint16_t basePort = 8000;
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    for (uint32_t team = 0; team < nTeams; ++team)
    {
        uint32_t serverIdx = 1 + nMedicalUnits + team * nRespondersPerTeam;
        Ipv4Address serverAddr = interfaces.GetAddress(serverIdx);
        uint16_t port = basePort + team;

        UdpEchoServerHelper echoServer(port);
        ApplicationContainer server = echoServer.Install(allNodes.Get(serverIdx));
        server.Start(Seconds(1.0));
        server.Stop(Seconds(40.0));
        serverApps.Add(server);

        // For each responder in the team, set up a client to the server (except for the server itself)
        for (uint32_t j = 0; j < nRespondersPerTeam; ++j)
        {
            uint32_t nodeIdx = 1 + nMedicalUnits + team * nRespondersPerTeam + j;
            if (nodeIdx == serverIdx) continue; // Do not install client on server

            UdpEchoClientHelper echoClient(serverAddr, port);
            echoClient.SetAttribute("MaxPackets", UintegerValue(0)); // Unlimited; control with stop time
            echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
            echoClient.SetAttribute("PacketSize", UintegerValue(128));

            ApplicationContainer client = echoClient.Install(allNodes.Get(nodeIdx));
            client.Start(Seconds(2.0 + j * 0.15));
            client.Stop(Seconds(40.0));
            clientApps.Add(client);
        }
    }

    // Install Flow Monitor
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    // Animation Interface
    AnimationInterface anim("disaster_recovery_network.xml");

    // Label the nodes for clarity in animation
    anim.UpdateNodeDescription(commandNode, "Command");
    anim.UpdateNodeColor(commandNode, 255, 0, 0); // red

    for (uint32_t i = 0; i < nMedicalUnits; ++i)
    {
        anim.UpdateNodeDescription(medicalNodes.Get(i), "Medical " + std::to_string(i));
        anim.UpdateNodeColor(medicalNodes.Get(i), 0, 0, 255); // blue
    }

    for (uint32_t i = 0; i < totalTeamNodes; ++i)
    {
        uint32_t team = i / nRespondersPerTeam;
        anim.UpdateNodeDescription(teamNodes.Get(i), "Team" + std::to_string(team) + "_Node" + std::to_string(i % nRespondersPerTeam));
        anim.UpdateNodeColor(teamNodes.Get(i), 0, 180 + 25 * team, 0); // different greens
    }

    // Run simulation
    Simulator::Stop(Seconds(42.0));
    Simulator::Run();

    // Flow Monitor Statistics
    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalTxPackets = 0.0;
    double totalRxPackets = 0.0;
    double totalDelay = 0.0;
    double totalRxBytes = 0.0;
    double simulationTime = 40.0; // seconds

    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        totalTxPackets += iter->second.txPackets;
        totalRxPackets += iter->second.rxPackets;
        totalDelay += iter->second.delaySum.GetSeconds();
        totalRxBytes += iter->second.rxBytes;
    }

    double pdr = (totalTxPackets > 0) ? (totalRxPackets / totalTxPackets) * 100.0 : 0.0;
    double avgDelay = (totalRxPackets > 0) ? (totalDelay / totalRxPackets) : 0.0;
    double throughput = (totalRxBytes * 8.0) / (simulationTime * 1e6); // Mbps

    std::cout << "===== Flow Monitor Results =====" << std::endl;
    std::cout << "Total Packets Sent:    " << totalTxPackets << std::endl;
    std::cout << "Total Packets Received:" << totalRxPackets << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdr << " %" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay * 1000.0 << " ms" << std::endl;
    std::cout << "Total Throughput: " << throughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}