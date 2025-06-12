#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AodvAdhocRandomMobility");

int main(int argc, char *argv[])
{
    uint32_t numNodes = 10;
    double simTime = 30.0;
    double areaSize = 500.0;
    uint16_t port = 9000;

    CommandLine cmd;
    cmd.Parse (argc, argv);

    // Nodes & Mobility
    NodeContainer nodes;
    nodes.Create (numNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
        "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
        "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
    mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
        "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
        "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
        "PositionAllocator", PointerValue (mobility.GetPositionAllocator()));
    mobility.Install (nodes);

    // Wi-Fi (ad-hoc)
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiMacHelper mac;
    mac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

    // Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper (aodv);
    stack.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Install applications: create random pairs of sender/receiver, random start times
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
    ApplicationContainer sinkApps, clientApps;
    std::vector<bool> usedAsReceiver(numNodes, false);
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        uint32_t receiver;
        do {
            receiver = uv->GetInteger (0, numNodes - 1);
        } while (receiver == i || usedAsReceiver[receiver]);
        usedAsReceiver[receiver] = true;

        uint16_t thisPort = port + i;

        // Sink
        UdpServerHelper server (thisPort);
        ApplicationContainer sink = server.Install (nodes.Get(receiver));
        sink.Get(0)->SetStartTime (Seconds (0.0));
        sink.Get(0)->SetStopTime (Seconds (simTime));
        sinkApps.Add (sink);

        // Random start time between 1-5 s
        double startTime = uv->GetValue (1.0, 5.0);

        // Client
        UdpClientHelper client (interfaces.GetAddress(receiver), thisPort);
        client.SetAttribute ("MaxPackets", UintegerValue (1000));
        client.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
        client.SetAttribute ("PacketSize", UintegerValue (512));
        ApplicationContainer src = client.Install (nodes.Get(i));
        src.Get(0)->SetStartTime (Seconds (startTime));
        src.Get(0)->SetStopTime (Seconds (simTime));
        clientApps.Add (src);
    }

    // Enable pcap tracing on all node's wifi devices
    phy.EnablePcap ("aodv-adhoc", devices);

    // FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    // Statistics: PDR and average delay
    uint64_t totalTxPackets = 0;
    uint64_t totalRxPackets = 0;
    double totalDelay = 0.0;

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalDelay += flow.second.delaySum.GetSeconds();
    }
    double pdr = totalTxPackets ? (double)totalRxPackets / totalTxPackets * 100.0 : 0.0;
    double avgDelay = totalRxPackets ? totalDelay / totalRxPackets : 0.0;

    std::cout << "Simulation Results:\n";
    std::cout << "  Packet Delivery Ratio: " << pdr << " %\n";
    std::cout << "  Average End-to-End Delay: " << avgDelay << " s\n";

    Simulator::Destroy ();
    return 0;
}