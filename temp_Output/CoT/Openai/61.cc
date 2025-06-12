#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/queue-disc-module.h"
#include <fstream>
#include <sys/stat.h>

using namespace ns3;

std::ofstream g_cwndStream;
std::ofstream g_queueSizeStream;
std::ofstream g_queueDropStream;
uint32_t g_previousQueueSize = 0;
uint32_t g_totalDrops = 0;

void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

void
QueueSizeTrace(std::string context, uint32_t oldVal, uint32_t newVal)
{
    if (g_queueSizeStream.is_open())
    {
        g_queueSizeStream << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
    }
    g_previousQueueSize = newVal;
}

void
QueueDropTrace(Ptr<const QueueDiscItem> item)
{
    if (g_queueDropStream.is_open())
    {
        g_queueDropStream << Simulator::Now().GetSeconds() << "\t" << item->GetPacket()->GetUid() << std::endl;
    }
    ++g_totalDrops;
}

int main(int argc, char *argv[])
{
    std::string resultsDir = "results";
    std::string cwndTracesDir = resultsDir + "/cwndTraces";
    std::string pcapDir = resultsDir + "/pcap";
    std::string queueTracesDir = resultsDir + "/queueTraces";

    // Create directories
    mkdir(resultsDir.c_str(), 0777);
    mkdir(cwndTracesDir.c_str(), 0777);
    mkdir(pcapDir.c_str(), 0777);
    mkdir(queueTracesDir.c_str(), 0777);

    // Write configuration
    std::ofstream configFile((resultsDir + "/config.txt").c_str());
    configFile << "Network topology: n0 <-> n1 <-> n2 <-> n3\n";
    configFile << "Links:\n";
    configFile << "  n0-n1: 10 Mbps, 1 ms delay\n";
    configFile << "  n1-n2: 1 Mbps, 10 ms delay\n";
    configFile << "  n2-n3: 10 Mbps, 1 ms delay\n";
    configFile << "TCP: BulkSendApplication from n0 to n3\n";
    configFile.close();

    NodeContainer nodes;
    nodes.Create(4);

    // Links
    PointToPointHelper p2p_0_1, p2p_1_2, p2p_2_3;
    p2p_0_1.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p_0_1.SetChannelAttribute("Delay", StringValue("1ms"));
    p2p_0_1.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(50));

    p2p_1_2.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p_1_2.SetChannelAttribute("Delay", StringValue("10ms"));
    p2p_1_2.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(50));

    p2p_2_3.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p_2_3.SetChannelAttribute("Delay", StringValue("1ms"));
    p2p_2_3.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(50));

    // NetDevices
    NetDeviceContainer d0d1 = p2p_0_1.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d1d2 = p2p_1_2.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d2d3 = p2p_2_3.Install(nodes.Get(2), nodes.Get(3));

    // Install stack and assign IPs
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = address.Assign(d2d3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install queue disc (for monitoring queue stats on the bottleneck link)
    TrafficControlHelper tch;
    tch.SetRootQueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue ("50p"));
    Ptr<QueueDisc> queueDisc = tch.Install(d1d2.Get(0)).Get(0);

    // BulkSend (TCP) from n0 to n3
    uint16_t port = 50000;
    Address sinkAddress(InetSocketAddress(i2i3.GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
    bulkSender.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer srcApp = bulkSender.Install(nodes.Get(0));
    srcApp.Start(Seconds(1.0));
    srcApp.Stop(Seconds(20.0));

    Simulator::Schedule(Seconds(1.0), &Ipv4GlobalRoutingHelper::RecomputeRoutingTables);

    // PCAP tracing
    p2p_0_1.EnablePcapAll((pcapDir+"/n0n1").c_str(), false, false);
    p2p_1_2.EnablePcapAll((pcapDir+"/n1n2").c_str(), false, false);
    p2p_2_3.EnablePcapAll((pcapDir+"/n2n3").c_str(), false, false);

    // Congestion window tracing for TCP on n0
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    Ptr<Socket> tcpSocket = Socket::CreateSocket(nodes.Get(0), tid);
    std::string cwndFile = cwndTracesDir + "/cwnd-n0.dat";
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> cwndStream = ascii.CreateFileStream(cwndFile);
    tcpSocket->TraceConnectWithoutContext("CongestionWindow",
      MakeBoundCallback(&CwndChange, cwndStream));
    // But BulkSendApp creates its own socket. So, hook on generic path:
    Config::Connect(
        "/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
        MakeBoundCallback(&CwndChange, cwndStream));

    // Queue length tracing
    std::string queueSizeFile = resultsDir + "/queue-size.dat";
    g_queueSizeStream.open(queueSizeFile.c_str());
    if (g_queueSizeStream.is_open())
    {
        queueDisc->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&QueueSizeTrace));
    }

    // Queue drop tracing
    std::string queueDropFile = queueTracesDir + "/queue-drop.dat";
    g_queueDropStream.open(queueDropFile.c_str());
    if (g_queueDropStream.is_open())
    {
        queueDisc->TraceConnectWithoutContext("Drop", MakeCallback(&QueueDropTrace));
    }

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Write queue stats
    std::ofstream queueStatsOut((resultsDir + "/queueStats.txt").c_str());
    queueStatsOut << "Total drops on bottleneck queue (n1-n2): " << g_totalDrops << "\n";
    queueStatsOut << "Final queue size (packets): " << g_previousQueueSize << "\n";
    queueStatsOut.close();

    // Cleanup
    if (g_queueSizeStream.is_open()) g_queueSizeStream.close();
    if (g_queueDropStream.is_open()) g_queueDropStream.close();

    Simulator::Destroy();
    return 0;
}