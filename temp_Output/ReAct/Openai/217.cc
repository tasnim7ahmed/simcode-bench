#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetAodvSimulation");

int main (int argc, char *argv[])
{
    // Simulation parameters
    uint32_t numNodes = 5;
    double simTime = 30.0; // seconds
    double areaWidth = 200.0;
    double areaHeight = 200.0;
    double nodeSpeed = 10.0; // m/s

    CommandLine cmd;
    cmd.Parse (argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create (numNodes);

    // Configure Wifi (802.11b Adhoc)
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

    // Mobility: RandomWaypointMobilityModel within areaWidth x areaHeight
    MobilityHelper mobility;
    mobility.SetMobilityModel (
        "ns3::RandomWaypointMobilityModel",
        "Speed", StringValue ("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
        "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
        "PositionAllocator", PointerValue (
            CreateObjectWithAttributes<RandomRectanglePositionAllocator> (
                "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=200.0]"),
                "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=200.0]"))
        )
    );
    mobility.SetPositionAllocator (
        "ns3::GridPositionAllocator",
        "MinX", DoubleValue (0.0),
        "MinY", DoubleValue (0.0),
        "DeltaX", DoubleValue (40.0),
        "DeltaY", DoubleValue (40.0),
        "GridWidth", UintegerValue (5),
        "LayoutType", StringValue ("RowFirst")
    );
    mobility.Install (nodes);

    // Enable mobility tracing
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> mobilityStream = ascii.CreateFileStream ("manet-mobility.tr");
    mobility.EnableAsciiAll (mobilityStream);

    // Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper (aodv);
    internet.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // UDP Application: OnOffApplication on node 0 -> node 4
    uint16_t port = 4000;
    OnOffHelper onoff ("ns3::UdpSocketFactory", 
        Address (InetSocketAddress (interfaces.GetAddress (4), port)));
    onoff.SetConstantRate (DataRate ("1Mbps"));
    onoff.SetAttribute ("PacketSize", UintegerValue (512));
    onoff.SetAttribute ("StartTime", TimeValue (Seconds (2.0)));
    onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime - 2)));

    ApplicationContainer app = onoff.Install (nodes.Get (0));

    // Packet sink on node 4
    PacketSinkHelper sink ("ns3::UdpSocketFactory",
        Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
    ApplicationContainer sinkApp = sink.Install (nodes.Get (4));
    sinkApp.Start (Seconds (1.5));
    sinkApp.Stop (Seconds (simTime));

    // Enable Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    // Simulation run
    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    // FlowMonitor result analysis
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    uint64_t rxPackets = 0;
    uint64_t txPackets = 0;
    for (auto iter = stats.begin (); iter != stats.end (); ++iter)
    {
        txPackets += iter->second.txPackets;
        rxPackets += iter->second.rxPackets;
    }
    double pdr = (txPackets > 0) ? (double) rxPackets / (double) txPackets : 0.0;
    std::cout << "---------- Packet Delivery Ratio -----------" << std::endl;
    std::cout << "Transmitted packets: " << txPackets << std::endl;
    std::cout << "Received packets:    " << rxPackets << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdr * 100 << " %" << std::endl;

    Simulator::Destroy ();
    return 0;
}