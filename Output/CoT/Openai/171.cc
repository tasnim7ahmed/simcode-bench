#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <vector>
#include <ctime>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvPdrDelay");

int main (int argc, char *argv[])
{
    uint32_t numNodes = 10;
    double simTime = 30.0;
    double areaSize = 500.0;

    CommandLine cmd(__FILE__);
    cmd.Parse (argc, argv);

    // Seed for RNG
    RngSeedManager::SetSeed (time(0));

    // Create nodes
    NodeContainer nodes;
    nodes.Create (numNodes);

    // Set up wifi devices & channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue ("DsssRate11Mbps"),
                                 "ControlMode", StringValue ("DsssRate11Mbps"));

    WifiMacHelper mac;
    mac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

    // Set up mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                               "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=10.0]"),
                               "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                               "PositionAllocator", PointerValue (
                                    CreateObjectWithAttributes<RandomRectanglePositionAllocator> (
                                       "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                       "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"))));
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "DeltaX", DoubleValue (30.0),
                                  "DeltaY", DoubleValue (30.0),
                                  "GridWidth", UintegerValue (5),
                                  "LayoutType", StringValue ("RowFirst"));
    mobility.Install (nodes);

    // Install Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper (aodv);
    internet.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // Enable pcap tracing
    phy.EnablePcapAll ("adhoc-aodv");

    // Set up UDP traffic: install one UDP server on each node, and a client on a random other node
    // (so 10 flows)
    uint16_t portBase = 4000;
    Ptr<UniformRandomVariable> startTimeVar = CreateObject<UniformRandomVariable> ();
    startTimeVar->SetAttribute ("Min", DoubleValue (1.0));
    startTimeVar->SetAttribute ("Max", DoubleValue (simTime/2.0));

    std::vector<ApplicationContainer> serverApps(numNodes);
    std::vector<ApplicationContainer> clientApps(numNodes);
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        // UDP server
        UdpServerHelper server (portBase + i);
        serverApps[i] = server.Install (nodes.Get (i));
        serverApps[i].Start (Seconds (0.0));
        serverApps[i].Stop (Seconds (simTime));

        // Randomly select a node other than self as source
        uint32_t src;
        do {
            src = rand() % numNodes;
        } while (src == i);

        // UDP client
        UdpClientHelper client (interfaces.GetAddress (i), portBase + i);
        client.SetAttribute ("MaxPackets", UintegerValue (10000));
        client.SetAttribute ("Interval", TimeValue (MilliSeconds (100)));
        client.SetAttribute ("PacketSize", UintegerValue (512));
        double startTime = startTimeVar->GetValue ();
        clientApps[i] = client.Install (nodes.Get (src));
        clientApps[i].Start (Seconds (startTime));
        clientApps[i].Stop (Seconds (simTime));
    }

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    double sumRx = 0.0;
    double sumTx = 0.0;
    double sumDelay = 0.0;
    uint32_t receivedPackets = 0;

    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

    for (auto it = stats.begin (); it != stats.end (); ++it)
    {
        sumTx += it->second.txPackets;
        sumRx += it->second.rxPackets;
        receivedPackets += it->second.rxPackets;
        sumDelay += it->second.delaySum.GetSeconds ();
    }

    double pdr = (sumRx > 0 ? (sumRx / sumTx * 100.0) : 0.0);
    double avgDelay = (receivedPackets > 0 ? (sumDelay / receivedPackets) : 0.0);

    std::cout << "Simulation Results:" << std::endl;
    std::cout << "  Packet Delivery Ratio (PDR): " << pdr << " %" << std::endl;
    std::cout << "  Average End-to-End Delay: " << avgDelay << " s" << std::endl;

    Simulator::Destroy ();
    return 0;
}