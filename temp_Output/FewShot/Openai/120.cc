#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

void PrintRoutingTables (Ptr<Node> node, Ptr<Ipv4> ipv4, Ptr<OutputStreamWrapper> stream)
{
    Ptr<Ipv4RoutingProtocol> rp = ipv4->GetRoutingProtocol ();
    Ptr<aodv::RoutingProtocol> aodv = DynamicCast<aodv::RoutingProtocol> (rp);
    if (aodv)
    {
        *stream->GetStream () << "Routing table for node " << node->GetId () << " at time " << Simulator::Now ().GetSeconds () << "s\n";
        aodv->PrintRoutingTable (stream);
        *stream->GetStream () << "\n";
    }
}

void PrintAllRoutingTables (NodeContainer &nodes)
{
    std::ostringstream oss;
    oss << "routing-tables-at-" << Simulator::Now ().GetSeconds () << "s.txt";
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream (oss.str ());
    for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
        Ptr<Node> node = nodes.Get (i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
        PrintRoutingTables (node, ipv4, stream);
    }
}

int main (int argc, char *argv[])
{
    uint32_t nNodes = 10;
    double simTime = 60.0;
    double areaX = 100.0, areaY = 100.0;
    double gridSpacing = 30.0;
    double nodeSpeed = 5.0; // m/s
    double nodePause = 1.0; // s
    double routingTablePrintInterval = 10.0; // seconds

    CommandLine cmd;
    cmd.AddValue("simTime","Simulation time in seconds", simTime);
    cmd.Parse (argc, argv);

    NodeContainer nodes;
    nodes.Create (nNodes);

    // Set up WiFi Ad-Hoc devices
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    wifiPhy.Set ("TxPowerStart", DoubleValue(16.0206)); //20dBm
    wifiPhy.Set ("TxPowerEnd", DoubleValue(16.0206));

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

    // Install mobility (Grid + RandomWalk)
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (gridSpacing),
                                   "DeltaY", DoubleValue (gridSpacing),
                                   "GridWidth", UintegerValue (4),
                                   "LayoutType", StringValue ("RowFirst"));

    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", TimeValue (Seconds (2.0)),
                              "Speed", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=5.0]"),
                              "Bounds", RectangleValue (Rectangle (0.0, areaX, 0.0, areaY)));
    mobility.Install (nodes);

    // Install Internet stack (AODV)
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper (aodv);
    internet.Install (nodes);

    // Assign IPv4 addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // Applications: UDP echo servers on all nodes, clients send to next node (circular)
    uint16_t port = 9;
    ApplicationContainer serverApps, clientApps;
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        // Install UDP echo server on each node
        UdpEchoServerHelper echoServer (port);
        serverApps.Add (echoServer.Install (nodes.Get (i)));
    }
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (simTime));

    for (uint32_t i = 0; i < nNodes; ++i)
    {
        uint32_t next = (i+1)%nNodes;
        UdpEchoClientHelper echoClient (interfaces.GetAddress (next), port);
        echoClient.SetAttribute ("MaxPackets", UintegerValue (1000)); // Enough packets for duration
        echoClient.SetAttribute ("Interval", TimeValue (Seconds (4.0)));
        echoClient.SetAttribute ("PacketSize", UintegerValue (64));
        ApplicationContainer app = echoClient.Install (nodes.Get (i));
        app.Start (Seconds (2.0));
        app.Stop (Seconds (simTime));
        clientApps.Add (app);
    }

    // Enable pcap tracing
    wifiPhy.EnablePcapAll ("aodv-adhoc");

    // FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    // NetAnim
    AnimationInterface anim ("aodv-adhoc.xml");

    // Routing Table Print Schedule
    for (double t = 5.0; t < simTime; t += routingTablePrintInterval)
    {
        Simulator::Schedule (Seconds (t), &PrintAllRoutingTables, std::ref (nodes));
    }

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

    uint32_t sentPackets = 0, receivedPackets = 0;
    double avgDelay = 0.0, avgThroughput = 0.0;
    uint32_t flowCount = 0;
    for (auto const &flow : stats)
    {
        sentPackets += flow.second.txPackets;
        receivedPackets += flow.second.rxPackets;
        if (flow.second.rxPackets > 0)
        {
            avgDelay += flow.second.delaySum.GetSeconds () / flow.second.rxPackets;
            double throughput = (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds ()) / 1000.0; // kbps
            avgThroughput += throughput;
            ++flowCount;
        }
    }
    double pdr = sentPackets > 0 ? (double)receivedPackets / sentPackets * 100.0 : 0.0;
    avgDelay = flowCount > 0 ? avgDelay / flowCount : 0.0;
    avgThroughput = flowCount > 0 ? avgThroughput / flowCount : 0.0;

    std::cout << "====== Simulation Results ======\n";
    std::cout << "Packets Sent: " << sentPackets << "\n";
    std::cout << "Packets Received: " << receivedPackets << "\n";
    std::cout << "Packet Delivery Ratio: " << pdr << " %\n";
    std::cout << "Average End-to-End Delay: " << avgDelay << " s\n";
    std::cout << "Average Throughput: " << avgThroughput << " kbps\n";

    Simulator::Destroy ();
    return 0;
}