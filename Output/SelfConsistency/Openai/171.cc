#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <ctime>
#include <cstdlib>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AodvAdhocSimulation");

int main (int argc, char *argv[])
{
    // Seed RNG with current time for traffic start randomness
    RngSeedManager::SetSeed (time (0));
    RngSeedManager::SetRun (std::rand () % 10000);

    uint32_t numNodes = 10;
    double simTime = 30.0;
    double areaSize = 500.0;

    CommandLine cmd;
    cmd.Parse (argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create (numNodes);

    // Set up WiFi (Adhoc)
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiHelper wifiHelper;
    wifiHelper.SetStandard (WIFI_STANDARD_80211b);
    wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                        "DataMode", StringValue ("DsssRate11Mbps"),
                                        "ControlMode", StringValue ("DsssRate11Mbps"));

    NetDeviceContainer devices = wifiHelper.Install (wifiPhy, wifiMac, nodes);

    // Set up mobility - Random Waypoint Mobility Model
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                   "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                   "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
    mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                               "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=10.0]"),
                               "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                               "PositionAllocator", PointerValue (mobility.GetPositionAllocator ()));
    mobility.Install (nodes);

    // Install Internet Stack
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper (aodv);
    stack.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Enable pcap tracing
    wifiPhy.EnablePcapAll ("aodv-adhoc");

    // Setup UDP traffic: randomly pick pairs
    uint16_t port = 4000;
    uint32_t numPairs = numNodes / 2;

    ApplicationContainer serverApps, clientApps;

    Ptr<UniformRandomVariable> startRv = CreateObject<UniformRandomVariable> ();
    startRv->SetAttribute ("Min", DoubleValue (1.0));
    startRv->SetAttribute ("Max", DoubleValue (5.0));

    for (uint32_t i = 0; i < numPairs; ++i)
    {
        // pick sender and receiver (avoid self-communication)
        uint32_t sender = i;
        uint32_t receiver = (i + numPairs) % numNodes;

        // UDP Server
        UdpServerHelper server (port);
        ApplicationContainer serverApp = server.Install (nodes.Get (receiver));
        serverApp.Start (Seconds (0.0));
        serverApp.Stop (Seconds (simTime));
        serverApps.Add (serverApp);

        // UDP Client
        UdpClientHelper client (interfaces.GetAddress (receiver), port);
        client.SetAttribute ("MaxPackets", UintegerValue (10000));
        client.SetAttribute ("Interval", TimeValue (Seconds (0.1))); // 10 packets/sec
        client.SetAttribute ("PacketSize", UintegerValue (512));
        double startTime = startRv->GetValue ();
        ApplicationContainer clientApp = client.Install (nodes.Get (sender));
        clientApp.Start (Seconds (startTime));
        clientApp.Stop (Seconds (simTime));
        clientApps.Add (clientApp);
    }

    // Install FlowMonitor
    Ptr<FlowMonitor> flowmon;
    FlowMonitorHelper flowmonHelper;
    flowmon = flowmonHelper.InstallAll ();

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    // Analyze FlowMonitor statistics
    flowmon->CheckForLostPackets ();

    uint64_t totalTxPackets = 0;
    uint64_t totalRxPackets = 0;
    double   totalDelay = 0.0;
    uint64_t deliveredFlows = 0;

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats ();

    for (auto const &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);

        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;

        if (flow.second.rxPackets > 0)
        {
            totalDelay += flow.second.delaySum.GetSeconds ();
            ++deliveredFlows;
        }
    }

    double pdr = (totalTxPackets > 0) ? (100.0 * totalRxPackets / totalTxPackets) : 0.0;
    double avgE2EDelay = (totalRxPackets > 0) ? (totalDelay / totalRxPackets) : 0.0;

    std::cout << "Simulation Results:" << std::endl;
    std::cout << "  Total Packets Sent       : " << totalTxPackets << std::endl;
    std::cout << "  Total Packets Received   : " << totalRxPackets << std::endl;
    std::cout << "  Packet Delivery Ratio    : " << pdr << " %" << std::endl;
    std::cout << "  Average End-to-End Delay : " << avgE2EDelay << " s" << std::endl;

    Simulator::Destroy ();
    return 0;
}