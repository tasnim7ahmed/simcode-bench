#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

void
PrintQueueDiscStats (Ptr<QueueDisc> queue)
{
    QueueDisc::Stats st = queue->GetStats();
    std::cout << Simulator::Now ().GetSeconds ()
              << "s RED QueueDisc: PacketsIn=" << st.nPackets
              << ", BytesIn=" << st.nBytes
              << ", Drops=" << st.nDroppedPackets
              << ", UnforcedDrops=" << st.nUnforcedDrops
              << ", ForcedDrops=" << st.nForcedDrops
              << std::endl;
}

void
PrintNetDeviceQueueStats (Ptr<NetDevice> dev)
{
    Ptr<PointToPointNetDevice> p2p = DynamicCast<PointToPointNetDevice> (dev);
    if (p2p)
    {
        Ptr<Queue<Packet>> txQueue = p2p->GetQueue ();
        std::cout << Simulator::Now ().GetSeconds ()
                  << "s NetDevice TXQueue: Packets="
                  << txQueue->GetNPackets ()
                  << ", Bytes=" << txQueue->GetNBytes ()
                  << std::endl;
    }
}

void
SojournTracer (Ptr<const QueueDiscItem> item)
{
    Time sojourn = Simulator::Now () - item->GetTimeStamp ();
    std::cout << Simulator::Now ().GetSeconds ()
              << "s Packet sojourn time in QueueDisc: "
              << sojourn.GetMilliSeconds ()
              << " ms" << std::endl;
}

int
main (int argc, char *argv[])
{
    // Logging (optional, feel free to adjust)
    LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

    // 1. Nodes and PointToPoint link
    NodeContainer nodes;
    nodes.Create (2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("20Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("5ms"));
    p2p.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("100p"));
    NetDeviceContainer devices = p2p.Install (nodes);

    // 2. Install Internet
    InternetStackHelper stack;
    stack.Install (nodes);

    // 3. IP addressing
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // 4. TrafficControl with RED QueueDisc
    TrafficControlHelper tch;
    uint32_t handle = tch.SetRootQueueDisc("ns3::RedQueueDisc", 
                                             "MaxSize", StringValue("50p"),
                                             "MinTh", DoubleValue (10),
                                             "MaxTh", DoubleValue (30),
                                             "LinkBandwidth", StringValue ("20Mbps"),
                                             "LinkDelay", StringValue ("5ms"));
    QueueDiscContainer disc = tch.Install(devices.Get(0)); // install only on sender

    // Connect trace for sojourn time
    disc.Get(0)->TraceConnectWithoutContext ("SojournTime", MakeCallback (&SojournTracer));
    // Schedule periodic queue stats printing
    for (double t = 0; t <= 10.0; t += 1.0)
    {
        Simulator::Schedule (Seconds (t), &PrintQueueDiscStats, disc.Get(0));
        Simulator::Schedule (Seconds (t), &PrintNetDeviceQueueStats, devices.Get(0));
    }

    // 5. TCP BulkSendApp on sender, PacketSink on receiver
    uint16_t port = 50000;
    Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (1));
    sinkApp.Start (Seconds (0.0));
    sinkApp.Stop (Seconds (12.0));

    BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
    source.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited
    source.SetAttribute ("SendSize", UintegerValue (1024));
    ApplicationContainer sourceApp = source.Install (nodes.Get (0));
    sourceApp.Start (Seconds (1.0));
    sourceApp.Stop (Seconds (11.0));

    // 6. FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop (Seconds (12.0));
    Simulator::Run ();

    // 7. Flow Monitor stats
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    for (auto it = stats.begin (); it != stats.end (); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (it->first);
        std::cout << "Flow " << it->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << it->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << it->second.rxPackets << "\n";
        std::cout << "  Throughput: "
                  << it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds () - it->second.timeFirstTxPacket.GetSeconds ()) / 1e6
                  << " Mbps\n";
        std::cout << "  Mean Delay: "
                  << (it->second.rxPackets > 0 ? (it->second.delaySum.GetSeconds() / it->second.rxPackets) : 0)
                  << " s\n";
        std::cout << "  Mean Jitter: "
                  << (it->second.rxPackets > 0 ? (it->second.jitterSum.GetSeconds() / it->second.rxPackets) : 0)
                  << " s\n";
    }

    Simulator::Destroy ();
    return 0;
}