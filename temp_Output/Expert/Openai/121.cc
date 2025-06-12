#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Parameters
    uint32_t nTeams = 3;
    uint32_t respondersPerTeam = 5;
    uint32_t nMedicalUnits = 3;
    uint32_t nCentralCommand = 1;

    uint32_t totalCommandNodes = nCentralCommand;
    uint32_t totalMedicalNodes = nMedicalUnits;
    uint32_t totalTeamNodes = nTeams * respondersPerTeam;

    double simulationTime = 60.0; // seconds

    // Create nodes
    NodeContainer commandNodes;
    commandNodes.Create(totalCommandNodes);

    NodeContainer medicalNodes;
    medicalNodes.Create(totalMedicalNodes);

    std::vector<NodeContainer> teamNodes(nTeams);
    for (uint32_t i = 0; i < nTeams; ++i)
    {
        teamNodes[i].Create(respondersPerTeam);
    }

    // Aggregate all nodes
    NodeContainer allNodes;
    allNodes.Add(commandNodes);
    allNodes.Add(medicalNodes);
    for (uint32_t i = 0; i < nTeams; ++i)
    {
        allNodes.Add(teamNodes[i]);
    }

    // Configure WiFi Adhoc
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    // Enable RTS/CTS
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));

    // Install NetDevices
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, allNodes);

    // Mobility
    MobilityHelper mobility;

    // Central Command Position
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(50, 50, 0)); // Central command
    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(commandNodes);

    // Medical Units Positions
    Ptr<ListPositionAllocator> medAlloc = CreateObject<ListPositionAllocator>();
    medAlloc->Add(Vector(100, 30, 0));
    medAlloc->Add(Vector(120, 80, 0));
    medAlloc->Add(Vector(60, 110, 0));
    mobility.SetPositionAllocator(medAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(medicalNodes);

    // Teams - Random Mobility
    for (uint32_t i = 0; i < nTeams; ++i)
    {
        MobilityHelper teamMob;
        int offsetX = 30 + 60 * i;
        int offsetY = 200 + 40 * i;
        teamMob.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue(std::to_string(offsetX) + ".0|" + std::to_string(offsetX+30) + ".0"),
                                    "Y", StringValue(std::to_string(offsetY) + ".0|" + std::to_string(offsetY+30) + ".0"));
        teamMob.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                 "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=3.0]"),
                                 "Pause", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                                 "PositionAllocator", PointerValue(teamMob.GetPositionAllocator()));
        teamMob.Install(teamNodes[i]);
    }

    // Internet stack
    InternetStackHelper internet;
    internet.Install(allNodes);

    // IP Addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Application setup: UDP Echo Server per team
    uint16_t echoPortBase = 9000;
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < nTeams; ++i)
    {
        // Designate first node in the team as server
        UdpEchoServerHelper echoServer(echoPortBase + i);
        serverApps.Add(echoServer.Install(teamNodes[i].Get(0)));
    }
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    // Client apps
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nTeams; ++i)
    {
        // All other nodes in the same team act as clients to their server
        for (uint32_t j = 1; j < respondersPerTeam; ++j)
        {
            Ipv4Address serverAddr = interfaces.Get(nCentralCommand + nMedicalUnits + i * respondersPerTeam); // server addr
            UdpEchoClientHelper echoClient(serverAddr, echoPortBase + i);
            echoClient.SetAttribute("MaxPackets", UintegerValue(30));
            echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
            echoClient.SetAttribute("PacketSize", UintegerValue(64));
            clientApps.Add(echoClient.Install(teamNodes[i].Get(j)));
        }
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    // Flow Monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // Animation
    AnimationInterface anim("disaster_recovery.xml");

    // Legend: Central command in red, medical in blue, teams in green
    anim.SetConstantPosition(commandNodes.Get(0), 50, 50);
    anim.UpdateNodeColor(commandNodes.Get(0), 255, 0, 0);

    for (uint32_t i = 0; i < nMedicalUnits; ++i)
    {
        anim.SetConstantPosition(medicalNodes.Get(i),
                                 (i == 0) ? 100 : (i == 1 ? 120 : 60),
                                 (i == 0) ? 30 : (i == 1 ? 80 : 110));
        anim.UpdateNodeColor(medicalNodes.Get(i), 0, 0, 255);
    }

    for (uint32_t i = 0; i < nTeams; ++i)
    {
        for (uint32_t j = 0; j < respondersPerTeam; ++j)
        {
            uint32_t nodeId = nCentralCommand + nMedicalUnits + i * respondersPerTeam + j;
            anim.UpdateNodeColor(allNodes.Get(nodeId), 0, 255, 0);
        }
    }

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Flow Monitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;
    uint64_t totalDelay = 0;
    double totalThroughput = 0.0;
    uint32_t matchedFlows = 0;

    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationPort >= echoPortBase && t.destinationPort < echoPortBase + nTeams)
        {
            totalTxPackets += flow.second.txPackets;
            totalRxPackets += flow.second.rxPackets;
            totalDelay += flow.second.delaySum.GetNanoSeconds();
            double thr = flow.second.rxBytes * 8.0 / (simulationTime - 2.0) / 1000.0; // Kbps, after client start
            totalThroughput += thr;
            matchedFlows++;
        }
    }

    double pdr = (totalTxPackets > 0) ? (double)totalRxPackets / totalTxPackets : 0.0;
    double avgDelay = (totalRxPackets > 0) ? (double)totalDelay / totalRxPackets / 1e6 : 0.0; // ms

    std::cout << "Disaster Recovery Network Statistics:\n";
    std::cout << "--------------------------------------\n";
    std::cout << "Total UDP Echo Flows: " << matchedFlows << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdr * 100 << " %\n";
    std::cout << "Average End-to-End Delay: " << avgDelay << " ms\n";
    std::cout << "Average Throughput: " << (matchedFlows > 0 ? totalThroughput / matchedFlows : 0.0) << " Kbps\n";
    std::cout << "--------------------------------------\n";

    Simulator::Destroy();

    return 0;
}