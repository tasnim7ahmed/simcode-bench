#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/flow-monitor-helper.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocWifiRandomMobility");

int main (int argc, char *argv[])
{
    uint32_t numNodes = 5;
    double simTime = 20.0; // seconds
    uint32_t packetSize = 1024; // bytes
    uint32_t numPackets = 10000;
    double interval = 0.01; // seconds
    std::string phyMode = "DsssRate11Mbps";
    std::string outputFileName = "adhoc-udp-stats.txt";

    CommandLine cmd;
    cmd.AddValue("outputFile", "Output stat file", outputFileName);
    cmd.Parse (argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create (numNodes);

    // Set up Wifi
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    wifiPhy.Set ("TxPowerStart", DoubleValue (16.0));
    wifiPhy.Set ("TxPowerEnd", DoubleValue (16.0));
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue (phyMode),
                                 "ControlMode", StringValue (phyMode));

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

    // Mobility: RandomWaypointMobilityModel
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                               "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                               "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                               "PositionAllocator", StringValue ("ns3::RandomRectanglePositionAllocator[MinX=0.0|MinY=0.0|MaxX=100.0|MaxY=100.0]"));
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (20.0),
                                   "DeltaY", DoubleValue (20.0),
                                   "GridWidth", UintegerValue (5),
                                   "LayoutType", StringValue ("RowFirst"));

    mobility.Install (nodes);

    // Internet Stack
    InternetStackHelper internet;
    internet.Install (nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // UDP: Node 0 sends to Node 1
    uint16_t port = 4000;
    UdpServerHelper server (port);
    ApplicationContainer serverApp = server.Install (nodes.Get (1));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (simTime));

    UdpClientHelper client (interfaces.GetAddress (1), port);
    client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
    client.SetAttribute ("Interval", TimeValue (Seconds (interval)));
    client.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer clientApp = client.Install (nodes.Get (0));
    clientApp.Start (Seconds (2.0));
    clientApp.Stop (Seconds (simTime));

    // Flow Monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll ();

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    // Gather stats and output
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

    uint64_t total_rx = 0;
    uint64_t total_tx = 0;
    uint64_t lost_packets = 0;
    double throughput = 0.0;

    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
        if ((t.sourceAddress == interfaces.GetAddress (0) && t.destinationAddress == interfaces.GetAddress (1)) || 
            (t.sourceAddress == interfaces.GetAddress (1) && t.destinationAddress == interfaces.GetAddress (0)))
        {
            total_tx += flow.second.txPackets;
            total_rx += flow.second.rxPackets;
            lost_packets += flow.second.lostPackets;
            throughput += flow.second.rxBytes * 8.0 / (simTime - 2.0) / 1e6; // Mbps, client starts at 2.0
        }
    }

    std::ofstream outFile;
    outFile.open (outputFileName.c_str ());
    outFile << "Total TX packets: " << total_tx << std::endl;
    outFile << "Total RX packets: " << total_rx << std::endl;
    outFile << "Lost packets: " << lost_packets << std::endl;
    outFile << "Throughput (Mbps): " << throughput << std::endl;
    outFile.close ();

    Simulator::Destroy ();
    return 0;
}