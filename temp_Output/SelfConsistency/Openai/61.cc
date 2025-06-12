#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iostream>

using namespace ns3;

// Directory paths
static std::string resultsDir = "results/";
static std::string pcapDir = resultsDir + "pcap/";

// Helper function to ensure output directories exist
void EnsureResultsDirectories () {
    system(("mkdir -p " + resultsDir).c_str());
    system(("mkdir -p " + pcapDir).c_str());
}

// Congestion window trace
static void
CwndChangeTrace(std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
    static std::ofstream cwndOut(resultsDir + "cwndTraces");
    cwndOut << Simulator::Now().GetSeconds() << " " << newCwnd << std::endl;
}

// Queue length trace
static void
QueueLengthTrace(uint32_t oldValue, uint32_t newValue)
{
    static std::ofstream queueLengthOut(resultsDir + "queue-size.dat", std::ios_base::app);
    queueLengthOut << Simulator::Now().GetSeconds() << " " << newValue << std::endl;
}

// Queue drop trace
static void
QueueDropTrace(std::string context, Ptr<const Packet> p)
{
    static std::ofstream queueDropOut(resultsDir + "queueTraces", std::ios_base::app);
    queueDropOut << Simulator::Now().GetSeconds() << " Packet dropped" << std::endl;
}

// Record configuration details
void WriteConfig ()
{
    std::ofstream config(resultsDir + "config.txt");
    config << "Topology: n0 -- n1 -- n2 -- n3" << std::endl;
    config << "Links:" << std::endl;
    config << "  n0-n1: 10 Mbps, 1 ms" << std::endl;
    config << "  n1-n2: 1 Mbps, 10 ms" << std::endl;
    config << "  n2-n3: 10 Mbps, 1 ms" << std::endl;
    config << "Application: BulkSend TCP from n0 to n3" << std::endl;
    config << "Traces: " << std::endl;
    config << "  Congestion window traces: " << resultsDir << "cwndTraces" << std::endl;
    config << "  Queue size: " << resultsDir << "queue-size.dat" << std::endl;
    config << "  PCAP: " << pcapDir << std::endl;
    config << "  Queue drops: " << resultsDir << "queueTraces" << std::endl;
    config << "  Queue stats: " << resultsDir << "queueStats.txt" << std::endl;
}

int
main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);
    LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
    EnsureResultsDirectories ();
    WriteConfig ();

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create (4);

    // Create links
    PointToPointHelper p2p_0_1;
    p2p_0_1.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    p2p_0_1.SetChannelAttribute ("Delay", StringValue ("1ms"));

    PointToPointHelper p2p_1_2;
    p2p_1_2.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
    p2p_1_2.SetChannelAttribute ("Delay", StringValue ("10ms"));

    PointToPointHelper p2p_2_3;
    p2p_2_3.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    p2p_2_3.SetChannelAttribute ("Delay", StringValue ("1ms"));

    // Install links
    NetDeviceContainer dev0_1 = p2p_0_1.Install (nodes.Get (0), nodes.Get (1));
    NetDeviceContainer dev1_2 = p2p_1_2.Install (nodes.Get (1), nodes.Get (2));
    NetDeviceContainer dev2_3 = p2p_2_3.Install (nodes.Get (2), nodes.Get (3));

    // Traffic control, to get a reference to queues (FIFO by default)
    TrafficControlHelper tch0_1, tch1_2, tch2_3;
    tch0_1.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
    tch1_2.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
    tch2_3.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");

    tch0_1.Install(dev0_1);
    tch1_2.Install(dev1_2);
    tch2_3.Install(dev2_3);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install (nodes);

    // Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if0_1 = address.Assign (dev0_1);

    address.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if1_2 = address.Assign (dev1_2);

    address.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if2_3 = address.Assign (dev2_3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    // BulkSend application from n0 to n3
    uint16_t port = 50000;

    // Sink on n3
    Address sinkAddress (InetSocketAddress (if2_3.GetAddress (1), port));
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (3));
    sinkApp.Start (Seconds (0.0));
    sinkApp.Stop (Seconds (10.0));

    // BulkSend application on n0
    BulkSendHelper source ("ns3::TcpSocketFactory", sinkAddress);
    source.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited
    source.SetAttribute ("SendSize", UintegerValue (1024));
    ApplicationContainer sourceApp = source.Install (nodes.Get (0));
    sourceApp.Start (Seconds (0.1));
    sourceApp.Stop (Seconds (10.0));

    // Enable PCAP
    p2p_0_1.EnablePcapAll (pcapDir + "n0-n1");
    p2p_1_2.EnablePcapAll (pcapDir + "n1-n2");
    p2p_2_3.EnablePcapAll (pcapDir + "n2-n3");

    // Track congestion window
    Ptr<Socket> tcpSocket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());
    tcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndChangeTrace));

    // Connect to the actual socket used by BulkSend
    Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&CwndChangeTrace));

    // Queue length and drop traces (track n1-n2 bottleneck queue on n1 device)
    Ptr<NetDevice> nd = dev1_2.Get(0); // n1's device towards n2
    Ptr<PointToPointNetDevice> ptpnd = nd->GetObject<PointToPointNetDevice>();
    Ptr<Queue<Packet>> queue = ptpnd->GetQueue();

    if (queue)
    {
        queue->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&QueueLengthTrace));
        queue->TraceConnectWithoutContext ("Drop", MakeCallback (&QueueDropTrace));
    } 
    // For queue disc (PfifoFastQueueDisc on n1-n2)
    Config::ConnectWithoutContext ("/NodeList/1/TrafficControlLayer/RootQueueDiscList/0/PacketsInQueue", MakeCallback (&QueueLengthTrace));
    Config::ConnectWithoutContext ("/NodeList/1/TrafficControlLayer/RootQueueDiscList/0/Drop", MakeCallback (&QueueDropTrace));

    // Schedule queue statistics dump at simulation end
    Simulator::Schedule (Seconds (10.0), [queue](){
        std::ofstream queueStats(resultsDir + "queueStats.txt");
        if (queue)
        {
            queueStats << "Packets in queue at end: " << queue->GetNPackets() << std::endl;
            queueStats << "Bytes in queue at end: " << queue->GetNBytes() << std::endl;
        }
        queueStats.close();
    });

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}