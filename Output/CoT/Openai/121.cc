#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DisasterRecoveryWifiAdhoc");

int main (int argc, char *argv[])
{
    // Simulation parameters
    uint32_t nTeams = 3;
    uint32_t teamSize = 5;
    uint32_t nMedical = 3;
    uint32_t nCommand = 1;
    uint32_t nResponders = nTeams * teamSize;
    double simTime = 60.0;
    double packetInterval = 2.0;
    uint32_t echoPort = 9;

    // Total nodes: 1 command + 3 medical + 15 responders
    uint32_t totalNodes = nCommand + nMedical + nResponders;

    NodeContainer commandNode;
    commandNode.Create (nCommand);

    NodeContainer medicalNodes;
    medicalNodes.Create (nMedical);

    std::vector<NodeContainer> teamNodes (nTeams);
    for (uint32_t i = 0; i < nTeams; ++i)
    {
        teamNodes[i].Create (teamSize);
    }

    // Combine all nodes for the wifi setup
    NodeContainer allNodes;
    allNodes.Add (commandNode);
    allNodes.Add (medicalNodes);
    for (uint32_t i = 0; i < nTeams; ++i)
        allNodes.Add (teamNodes[i]);

    // Wifi channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue ("DsssRate11Mbps"),
                                 "ControlMode", StringValue ("DsssRate1Mbps"),
                                 "RtsCtsThreshold", UintegerValue (0)); // Enable RTS/CTS for all

    Ssid ssid = Ssid ("disaster-recovery");
    mac.SetType ("ns3::AdhocWifiMac", "Ssid", SsidValue (ssid));

    NetDeviceContainer devices = wifi.Install (phy, mac, allNodes);

    // Mobility
    MobilityHelper mobility;
    // Stationary for command and medical
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector (10.0, 50.0, 0.0)); // Command
    positionAlloc->Add (Vector (30.0, 70.0, 0.0)); // Medical 1
    positionAlloc->Add (Vector (30.0, 50.0, 0.0)); // Medical 2
    positionAlloc->Add (Vector (30.0, 30.0, 0.0)); // Medical 3

    uint32_t idx = 0;
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();
    // Allocate initial positions for teams (randomized in area)
    for (uint32_t team = 0; team < nTeams; ++team)
    {
        for (uint32_t m = 0; m < teamSize; ++m)
        {
            double x = 70.0 + 50.0 * team + rand->GetValue (0,20);
            double y = 20.0 + 40.0 * m;
            positionAlloc->Add (Vector (x, y, 0.0));
        }
    }

    mobility.SetPositionAllocator (positionAlloc);

    // Mobility models
    // Command and medical: constant position
    Ptr<UniformRandomVariable> srand = CreateObject<UniformRandomVariable>();
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    NodeContainer staticNodes;
    staticNodes.Add (commandNode);
    staticNodes.Add (medicalNodes);
    mobility.Install (staticNodes);

    // Each team: RandomWaypointMobilityModel
    for (uint32_t team = 0; team < nTeams; ++team)
    {
        MobilityHelper mobTeam;
        std::ostringstream area;
        double minX = 60.0 + 50.0 * team, maxX = minX + 40.0;
        double minY = 10.0, maxY = 110.0;
        mobTeam.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                     "X", StringValue ("ns3::UniformRandomVariable[Min=" + std::to_string(minX) + "|Max=" + std::to_string(maxX) + "]"),
                                     "Y", StringValue ("ns3::UniformRandomVariable[Min=" + std::to_string(minY) + "|Max=" + std::to_string(maxY) + "]"));
        mobTeam.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue ("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
                                  "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                                  "PositionAllocator", PointerValue (mobTeam.GetPositionAllocator ()));
        mobTeam.Install (teamNodes[team]);
    }

    // Internet stack
    InternetStackHelper internet;
    internet.Install (allNodes);

    // IP addressing
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer ifaces = ipv4.Assign (devices);

    // Application - UDP Echo server/client per team
    ApplicationContainer serverApps, clientApps;
    for (uint32_t team = 0; team < nTeams; ++team)
    {
        // Choose first node in each team as the echo server
        Ptr<Node> serverNode = teamNodes[team].Get (0);
        Ipv4Address serverAddr = ifaces.GetAddress (nCommand + nMedical + team * teamSize);
        UdpEchoServerHelper echoServer (echoPort + team);

        ApplicationContainer serverApp = echoServer.Install (serverNode);
        serverApp.Start (Seconds (1.0));
        serverApp.Stop (Seconds (simTime));

        serverApps.Add (serverApp);

        // All other team nodes send UDP echo to their own team server
        for (uint32_t m = 1; m < teamSize; ++m)
        {
            Ptr<Node> clientNode = teamNodes[team].Get (m);
            UdpEchoClientHelper echoClient (serverAddr, echoPort + team);
            echoClient.SetAttribute ("MaxPackets", UintegerValue (30));
            echoClient.SetAttribute ("Interval", TimeValue (Seconds (packetInterval)));
            echoClient.SetAttribute ("PacketSize", UintegerValue (64));
            ApplicationContainer clientApp = echoClient.Install (clientNode);
            clientApp.Start (Seconds (2.0));
            clientApp.Stop (Seconds (simTime));
            clientApps.Add (clientApp);
        }
    }

    // Flow Monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    // Animation
    AnimationInterface anim ("disaster-recovery.xml");
    anim.SetConstantPosition (commandNode.Get (0), 10.0, 50.0);
    for (uint32_t i = 0; i < nMedical; ++i)
        anim.SetConstantPosition (medicalNodes.Get(i), 30.0, 30.0 + 20.0*i);

    // Assign node descriptions
    anim.UpdateNodeDescription (commandNode.Get(0), "Command");
    for (uint32_t i = 0; i < nMedical; ++i)
        anim.UpdateNodeDescription (medicalNodes.Get(i), "Medical-" + std::to_string (i+1));
    for (uint32_t t = 0; t < nTeams; ++t)
    {
        for (uint32_t m = 0; m < teamSize; ++m)
        {
            Ptr<Node> n = teamNodes[t].Get (m);
            anim.UpdateNodeDescription (n, "T" + std::to_string (t+1) + "R" + std::to_string (m+1));
        }
    }

    // Logging
    // Uncomment for application and wifi logs
    //LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    //LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    Simulator::Stop (Seconds (simTime + 2));
    Simulator::Run ();

    // Flow monitor statistics
    flowmon->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    uint32_t rxPackets = 0, txPackets = 0;
    double delaySum = 0.0, rxBytes = 0.0;
    double firstTxTime = 1e6, lastRxTime = 0;
    for (auto &flow : stats)
    {
        txPackets += flow.second.txPackets;
        rxPackets += flow.second.rxPackets;
        delaySum += flow.second.delaySum.GetSeconds ();
        rxBytes += flow.second.rxBytes;
        if (flow.second.timeFirstTxPacket.GetSeconds () < firstTxTime)
            firstTxTime = flow.second.timeFirstTxPacket.GetSeconds ();
        if (flow.second.timeLastRxPacket.GetSeconds () > lastRxTime)
            lastRxTime = flow.second.timeLastRxPacket.GetSeconds ();
    }

    double pdr = (txPackets > 0) ? (double)rxPackets / txPackets : 0;
    double avgDelay = (rxPackets > 0) ? delaySum / rxPackets : 0;
    double throughput = ((rxBytes * 8.0) / (lastRxTime - firstTxTime)) / 1e6; // Mbps

    std::cout << "======= Disaster Recovery WiFi Ad hoc Simulation Results =======" << std::endl;
    std::cout << "Total sent packets: " << txPackets << std::endl;
    std::cout << "Total received packets: " << rxPackets << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdr * 100 << " %" << std::endl;
    std::cout << "Average end-to-end delay: " << avgDelay * 1000 << " ms" << std::endl;
    std::cout << "Overall throughput: " << throughput << " Mbps" << std::endl;

    Simulator::Destroy ();
    return 0;
}