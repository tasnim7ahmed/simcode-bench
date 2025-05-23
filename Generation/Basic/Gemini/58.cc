#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <fstream>

using namespace ns3;

Ptr<OutputStreamWrapper> g_cwndStream;
Ptr<OutputStreamWrapper> g_throughputStream;
Ptr<OutputStreamWrapper> g_queueStream;

void CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newCwnd << std::endl;
}

void QueueSizeChange (Ptr<OutputStreamWrapper> stream, Ptr<Queue> queue)
{
    *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << queue->GetNBytes () << std::endl;
}

void CalculateThroughput (Ptr<PacketSinkApplication> sink, Ptr<OutputStreamWrapper> stream)
{
    static double lastTotalRxBytes = 0;
    static double lastTime = 0;

    double currentTime = Simulator::Now ().GetSeconds ();
    double totalRxBytes = sink->GetTotalRx ();
    double throughput = (totalRxBytes - lastTotalRxBytes) * 8 / (currentTime - lastTime) / 1000000.0; // Mbps

    if (currentTime > lastTime)
    {
        *stream->GetStream () << currentTime << "\t" << throughput << std::endl;
    }

    lastTotalRxBytes = totalRxBytes;
    lastTime = currentTime;

    Simulator::Schedule (Seconds (1.0), &CalculateThroughput, sink, stream);
}

int main (int argc, char *argv[])
{
    Config::SetDefault ("ns3::TcpL4Protocol::CongestionControlAlgorithm", StringValue ("ns3::TcpBbr"));
    Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (131072));
    Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (131072));

    Config::SetDefault ("ns3::BbrCongestionControl::ProbeRttInterval", TimeValue (Seconds (10.0)));
    Config::SetDefault ("ns3::BbrCongestionControl::MinCwndInProbeRtt", UintegerValue (4));

    NodeContainer nodes;
    nodes.Create (4);
    Ptr<Node> sender = nodes.Get (0);
    Ptr<Node> r1 = nodes.Get (1);
    Ptr<Node> r2 = nodes.Get (2);
    Ptr<Node> receiver = nodes.Get (3);

    PointToPointHelper p2pHelper;
    Ipv4AddressHelper ipv4;

    p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("1000Mbps"));
    p2pHelper.SetChannelAttribute ("Delay", StringValue ("5ms"));
    NetDeviceContainer senderR1Devices = p2pHelper.Install (sender, r1);
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer senderR1Interfaces = ipv4.Assign (senderR1Devices);

    p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    p2pHelper.SetChannelAttribute ("Delay", StringValue ("10ms"));
    NetDeviceContainer r1r2Devices = p2pHelper.Install (r1, r2);
    ipv4.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer r1r2Interfaces = ipv4.Assign (r1r2Devices);

    p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("1000Mbps"));
    p2pHelper.SetChannelAttribute ("Delay", StringValue ("5ms"));
    NetDeviceContainer r2ReceiverDevices = p2pHelper.Install (r2, receiver);
    ipv4.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer r2ReceiverInterfaces = ipv4.Assign (r2ReceiverDevices);

    InternetStackHelper stack;
    stack.Install (nodes);

    uint16_t port = 50000;
    
    Address sinkAddress (InetSocketAddress (r2ReceiverInterfaces.GetAddress (1), port));
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::Any, port));
    ApplicationContainer sinkApps = packetSinkHelper.Install (receiver);
    sinkApps.Start (Seconds (0.0));
    sinkApps.Stop (Seconds (100.0));
    Ptr<PacketSinkApplication> sink = StaticCast<PacketSinkApplication> (sinkApps.Get (0));

    OnOffHelper onoffHelper ("ns3::TcpSocketFactory", Address ());
    onoffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoffHelper.SetAttribute ("DataRate", StringValue ("100Mbps"));
    onoffHelper.SetAttribute ("PacketSize", UintegerValue (1500));
    
    Address senderAddress (InetSocketAddress (sinkAddress.GetAs<InetSocketAddress> ().GetIpv4 (), port));
    onoffHelper.SetAttribute ("Remote", AddressValue (senderAddress));
    ApplicationContainer clientApps = onoffHelper.Install (sender);
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (100.0));

    p2pHelper.EnablePcapAll ("bbr-bottleneck-sim");

    AsciiTraceHelper asciiTraceHelper;
    g_cwndStream = asciiTraceHelper.CreateFileStream ("bbr-cwnd.tr");
    
    std::string tracePath = "/NodeList/" + std::to_string (sender->GetId ()) + "/$ApplicationList/*/$ns3::OnOffApplication/Socket/CongestionWindow";
    Config::Connect (tracePath, MakeBoundCallback (&CwndChange, g_cwndStream));

    g_throughputStream = asciiTraceHelper.CreateFileStream ("bbr-throughput.tr");
    Simulator::Schedule (Seconds (0.0), &CalculateThroughput, sink, g_throughputStream);

    g_queueStream = asciiTraceHelper.CreateFileStream ("bbr-queue.tr");
    Ptr<PointToPointNetDevice> r1r2Device = DynamicCast<PointToPointNetDevice> (r1r2Devices.Get (0));
    if (r1r2Device)
    {
        Ptr<Queue> queue = r1r2Device->GetQueue ();
        if (queue)
        {
            queue->TraceConnectWithoutContext ("BytesInQueue", MakeBoundCallback (&QueueSizeChange, g_queueStream, queue));
        }
    }
    
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    Simulator::Stop (Seconds (100.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}