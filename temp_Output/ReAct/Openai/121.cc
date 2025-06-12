#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DisasterRecoveryNetwork");

int main (int argc, char *argv[])
{
    uint32_t nMedicalUnits = 3;
    uint32_t nTeams = 3;
    uint32_t nTeamNodes = 5;
    double simTime = 60.0;

    CommandLine cmd;
    cmd.Parse (argc, argv);

    // Create nodes
    NodeContainer commandNode;
    commandNode.Create (1);

    NodeContainer medicalNodes;
    medicalNodes.Create (nMedicalUnits);

    std::vector<NodeContainer> teamNodes (nTeams);
    for (uint32_t t = 0; t < nTeams; ++t)
        teamNodes[t].Create (nTeamNodes);

    // Aggregate all nodes for devices/channels
    NodeContainer allNodes;
    allNodes.Add (commandNode);
    allNodes.Add (medicalNodes);
    for (uint32_t t = 0; t < nTeams; ++t)
        allNodes.Add (teamNodes[t]);

    // Configure WiFi (ad hoc, RTS/CTS enabled)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", 
                                  "DataMode", StringValue ("DsssRate11Mbps"),
                                  "ControlMode", StringValue ("DsssRate1Mbps"),
                                  "RtsCtsThreshold", UintegerValue (0)); // always use RTS/CTS

    WifiMacHelper mac;
    mac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (phy, mac, allNodes);

    // Mobility: command and medical units stationary, teams mobile
    MobilityHelper mobilityStatic;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
    posAlloc->Add (Vector (10, 30, 0)); // Central Command
    for (uint32_t m = 0; m < nMedicalUnits; ++m)
        posAlloc->Add (Vector (30 + 20*m, 30, 0)); // Medical units

    mobilityStatic.SetPositionAllocator (posAlloc);
    mobilityStatic.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    NodeContainer staticNodes;
    staticNodes.Add (commandNode);
    staticNodes.Add (medicalNodes);
    mobilityStatic.Install (staticNodes);

    // Teams: random mobility
    for (uint32_t t = 0; t < nTeams; ++t)
    {
        MobilityHelper mob;
        mob.SetPositionAllocator (
            "ns3::RandomRectanglePositionAllocator",
            "X", StringValue ("ns3::UniformRandomVariable[Min=50,Max=200]"),
            "Y", StringValue ("ns3::UniformRandomVariable[Min=10,Max=130]")
        );
        mob.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
            "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
            "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
            "PositionAllocator", PointerValue (mob.GetPositionAllocator())
        );
        mob.Install (teamNodes[t]);
    }

    // Internet stack
    InternetStackHelper internet;
    internet.Install (allNodes);

    // IP addressing
    Ipv4AddressHelper address;
    address.SetBase ("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    uint32_t idx = 0;
    std::vector<Ipv4Address> teamAddresses (nTeams);
    std::vector<Ipv4Address> firstTeamNodeAddress (nTeams);
    // IPs for command and medical (do not use in apps)
    idx += 1 + nMedicalUnits;
    // For each team, store address of first node (to run echo server)
    for (uint32_t t = 0; t < nTeams; ++t)
    {
        firstTeamNodeAddress[t] = interfaces.GetAddress (idx);
        idx += nTeamNodes;
    }

    // UDP echo servers (port 9000+t) on first node of each team
    ApplicationContainer echoServers;
    for (uint32_t t = 0; t < nTeams; ++t)
    {
        UdpEchoServerHelper echoServer (9000 + t);
        echoServers.Add (echoServer.Install (teamNodes[t].Get (0)));
    }
    echoServers.Start (Seconds (1.0));
    echoServers.Stop (Seconds (simTime-1));

    // UDP echo clients: install on every team node except the server node itself
    ApplicationContainer echoClients;
    for (uint32_t t = 0; t < nTeams; ++t)
    {
        for (uint32_t n = 0; n < nTeamNodes; ++n)
        {
            if (n == 0) continue; // 0 is the server node
            UdpEchoClientHelper echoClient (firstTeamNodeAddress[t], 9000 + t);
            echoClient.SetAttribute ("MaxPackets", UintegerValue (simTime/2));
            echoClient.SetAttribute ("Interval", TimeValue (Seconds (2.0)));
            echoClient.SetAttribute ("PacketSize", UintegerValue (64));
            echoClients.Add (echoClient.Install (teamNodes[t].Get (n)));
        }
    }
    echoClients.Start (Seconds (2.0));
    echoClients.Stop (Seconds (simTime-1));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    // Animation
    AnimationInterface anim ("disaster-recovery-network.xml");
    anim.SetConstantPosition (commandNode.Get (0), 10, 30);
    for (uint32_t m = 0; m < nMedicalUnits; ++m)
        anim.SetConstantPosition (medicalNodes.Get (m), 30 + 20*m, 30);

    // Run simulation
    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    // Flow monitor statistics
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;
    uint32_t totalLostPackets = 0;
    double sumDelay = 0.0;
    double sumThroughput = 0.0;
    uint32_t flowsMeasured = 0;

    std::cout << "Flow statistics:\n";
    for (auto iter = stats.begin (); iter != stats.end (); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
        std::cout << "Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << iter->second.lostPackets << "\n";
        std::cout << "  Throughput: " 
                  << (iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds () - iter->second.timeFirstTxPacket.GetSeconds ()))/1024
                  << " Kbps\n";
        if (iter->second.rxPackets > 0)
        {
            std::cout << "  Mean Delay: "
                      << iter->second.delaySum.GetSeconds () / iter->second.rxPackets << " s\n";
        }
        else
        {
            std::cout << "  Mean Delay: NA\n";
        }
        // Accumulate statistics
        totalTxPackets += iter->second.txPackets;
        totalRxPackets += iter->second.rxPackets;
        totalLostPackets += iter->second.lostPackets;
        if (iter->second.rxPackets > 0)
        {
            sumDelay += iter->second.delaySum.GetSeconds ();
            sumThroughput += (iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds () - iter->second.timeFirstTxPacket.GetSeconds ()));
            ++flowsMeasured;
        }
        std::cout << "---------------------------------------------\n";
    }
    std::cout << "============= Aggregate Results =============\n";
    std::cout << "Packet Delivery Ratio: ";
    if (totalTxPackets > 0)
        std::cout << 100.0 * totalRxPackets / totalTxPackets << " %\n";
    else
        std::cout << "NA\n";
    std::cout << "Average End-to-End Delay: ";
    if (totalRxPackets > 0)
        std::cout << sumDelay / totalRxPackets << " s\n";
    else
        std::cout << "NA\n";
    std::cout << "Average Throughput (all flows): ";
    if (flowsMeasured > 0)
        std::cout << (sumThroughput / flowsMeasured)/1024 << " Kbps\n";
    else
        std::cout << "NA\n";

    Simulator::Destroy ();
    return 0;
}