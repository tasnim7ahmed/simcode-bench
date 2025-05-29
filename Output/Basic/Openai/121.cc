#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DisasterRecoveryNet");

int main (int argc, char *argv[])
{
    uint32_t numMedicalUnits = 3;
    uint32_t numTeams = 3;
    uint32_t respondersPerTeam = 5;
    uint32_t totalResponders = numTeams * respondersPerTeam;
    double simTime = 60.0;

    CommandLine cmd;
    cmd.Parse (argc, argv);

    // Node containers
    NodeContainer commandNode;
    commandNode.Create (1);
    NodeContainer medicalUnits;
    medicalUnits.Create (numMedicalUnits);
    std::vector<NodeContainer> teams;
    for (uint32_t t = 0; t < numTeams; ++t)
    {
        NodeContainer team;
        team.Create (respondersPerTeam);
        teams.push_back (team);
    }
    // Aggregate all nodes for device/install purposes
    NodeContainer allNodes;
    allNodes.Add (commandNode);
    allNodes.Add (medicalUnits);
    for (auto& team : teams)
        allNodes.Add (team);

    // WiFi configuration
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel (channel.Create ());
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue ("DsssRate11Mbps"),
                                  "RtsCtsThreshold", UintegerValue (0));
    WifiMacHelper mac;
    mac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (phy, mac, allNodes);

    // Mobility: stationary for command & medical, random for teams
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
    posAlloc->Add (Vector (50.0, 50.0, 0.0));
    for (uint32_t i = 0; i < numMedicalUnits; ++i)
        posAlloc->Add (Vector (20.0 + i*40, 80.0, 0.0));

    mobility.SetPositionAllocator (posAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    NodeContainer staticNodes;
    staticNodes.Add (commandNode);
    staticNodes.Add (medicalUnits);
    mobility.Install (staticNodes);

    for (uint32_t t = 0; t < numTeams; ++t)
    {
        MobilityHelper mob;
        mob.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue ("ns3::UniformRandomVariable[Min=10|Max=90]"),
                                  "Y", StringValue ("ns3::UniformRandomVariable[Min=10|Max=90]"));
        mob.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=4.0]"),
                              "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                              "PositionAllocator", PointerValue (mob.GetPositionAllocator ()));
        mob.Install (teams[t]);
    }

    // Internet stack & IP
    InternetStackHelper stack;
    stack.Install (allNodes);
    Ipv4AddressHelper ip;
    ip.SetBase ("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ip.Assign (devices);

    // UDP Echo Servers (one per team, on team[0])
    std::vector<uint16_t> serverPorts (numTeams, 9000);
    std::vector<ApplicationContainer> serverApps (numTeams);

    for (uint32_t t = 0; t < numTeams; ++t)
    {
        UdpEchoServerHelper echoServer (serverPorts[t]);
        serverApps[t] = echoServer.Install (teams[t].Get (0));
        serverApps[t].Start (Seconds (1.0));
        serverApps[t].Stop (Seconds (simTime));
    }

    // UDP Echo Clients (team[1..n] send to their own team[0])
    ApplicationContainer clientApps;
    for (uint32_t t = 0; t < numTeams; ++t)
    {
        Ipv4Address serverAddr = interfaces.GetAddress (1 + numMedicalUnits + t * respondersPerTeam);
        for (uint32_t n = 1; n < respondersPerTeam; ++n)
        {
            UdpEchoClientHelper echoClient (serverAddr, serverPorts[t]);
            echoClient.SetAttribute ("MaxPackets", UintegerValue (simTime/2));
            echoClient.SetAttribute ("Interval", TimeValue (Seconds (2.0)));
            echoClient.SetAttribute ("PacketSize", UintegerValue (64));
            ApplicationContainer clientApp = echoClient.Install (teams[t].Get (n));
            clientApp.Start (Seconds (2.0 + n * 0.1));
            clientApp.Stop (Seconds (simTime));
            clientApps.Add (clientApp);
        }
    }

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    // NetAnim
    AnimationInterface anim ("disaster-recovery-network.xml");
    anim.SetConstantPosition (commandNode.Get (0), 50, 50);
    for (uint32_t i = 0; i < numMedicalUnits; ++i)
        anim.SetConstantPosition (medicalUnits.Get (i), 20.0 + i*40, 80.0);
    // assign different colors: commandNode - red, medical - blue, responders - green
    anim.UpdateNodeColor (commandNode.Get (0)->GetId(), 255, 0, 0);
    for (uint32_t i = 0; i < numMedicalUnits; ++i)
        anim.UpdateNodeColor (medicalUnits.Get (i)->GetId(), 0, 0, 255);
    for (uint32_t t = 0; t < numTeams; ++t)
        for (uint32_t n = 0; n < respondersPerTeam; ++n)
            anim.UpdateNodeColor (teams[t].Get (n)->GetId(), 0, 200, 0);

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    // FlowMonitor results
    monitor->CheckForLostPackets ();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    uint32_t rxPackets = 0;
    uint32_t txPackets = 0;
    double delaySum = 0;
    double throughputSum = 0;
    for (const auto& flow : stats)
    {
        txPackets += flow.second.txPackets;
        rxPackets += flow.second.rxPackets;
        delaySum += flow.second.delaySum.GetSeconds ();
        if (flow.second.rxBytes > 0 && flow.second.timeLastRxPacket.GetSeconds () > flow.second.timeFirstTxPacket.GetSeconds ())
        {
            double throughput = flow.second.rxBytes * 8.0 /
                (flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds ()) / 1024.0;
            throughputSum += throughput;
        }
    }

    double pdr = txPackets ? (100.0 * rxPackets / txPackets) : 0.0;
    double avgDelay = rxPackets ? (delaySum / rxPackets) : 0.0;
    double avgThroughput = throughputSum / stats.size ();

    std::cout << "======= Simulation Results =======" << std::endl;
    std::cout << "Packet Delivery Ratio (%): " << pdr << std::endl;
    std::cout << "Average End-to-End Delay (s): " << avgDelay << std::endl;
    std::cout << "Average Throughput (kbps): " << avgThroughput << std::endl;

    Simulator::Destroy ();
    return 0;
}