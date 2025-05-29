#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/queue-disc.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBbrProbeRttSimulation");

std::ofstream cwndStream;
std::ofstream throughputStream;
std::ofstream queueStream;

Ptr<QueueDisc> bottleneckQueueDisc;
Ptr<const TcpSocketBase> senderSocket;
Ptr<OutputStreamWrapper> cwndStreamWrapper;

// For sender throughput measurement
uint64_t lastTxBytes = 0;
double lastTime = 0.0;

void
CwndTracer(uint32_t oldCwnd, uint32_t newCwnd)
{
    if (cwndStream.is_open())
    {
        cwndStream << Simulator::Now().GetSeconds() << " " << newCwnd << std::endl;
    }
}

void
ThroughputTracer()
{
    double now = Simulator::Now().GetSeconds();
    if (senderSocket && throughputStream.is_open())
    {
        uint64_t txBytes = senderSocket->GetTxBytes();
        double throughput = (txBytes - lastTxBytes) * 8.0 / (now - lastTime) / 1e6; // Mbps
        throughputStream << now << " " << throughput << std::endl;
        lastTxBytes = txBytes;
        lastTime = now;
    }
    Simulator::Schedule(Seconds(0.1), &ThroughputTracer);
}

void
QueueTracer()
{
    if (bottleneckQueueDisc && queueStream.is_open())
    {
        uint32_t qSize = bottleneckQueueDisc->GetCurrentSize().GetValue();
        queueStream << Simulator::Now().GetSeconds() << " " << qSize << std::endl;
    }
    Simulator::Schedule(Seconds(0.01), &QueueTracer);
}

void
EnterProbeRtt(Ptr<TcpSocket> sock)
{
    // BBR's ProbeRTT: temporarilly reduce cwnd to 4 segments
    sock->SetAttribute("CWnd", UintegerValue(4 * sock->GetSegSize()));
    // Leave ProbeRTT after 200 ms (typical ProbeRTT time)
    Simulator::Schedule(Seconds(0.2), [sock]() {
        sock->SetAttribute("CWnd", UintegerValue(sock->GetInitialCwnd() * sock->GetSegSize()));
    });
}

void
ScheduleProbeRtt(Ptr<TcpSocket> sock)
{
    Simulator::Schedule(Seconds(10.0), [sock]() {
        EnterProbeRtt(sock);
        ScheduleProbeRtt(sock);
    });
}

int
main(int argc, char *argv[])
{
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpBbr"));

    NodeContainer nodes;
    nodes.Create(4); // 0: sender, 1: R1, 2: R2, 3: receiver

    // Sender <-> R1
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    // R1 <-> R2 (bottleneck)
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    // R2 <-> Receiver
    NodeContainer n2n3 = NodeContainer(nodes.Get(2), nodes.Get(3));

    PointToPointHelper p2pHigh;
    p2pHigh.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
    p2pHigh.SetChannelAttribute("Delay", StringValue("5ms"));

    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pBottleneck.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer d0d1 = p2pHigh.Install(n0n1);
    NetDeviceContainer d1d2 = p2pBottleneck.Install(n1n2);
    NetDeviceContainer d2d3 = p2pHigh.Install(n2n3);

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
    // Only bottleneck gets queue disc
    QueueDiscContainer qdiscs = tch.Install(d1d2);

    bottleneckQueueDisc = qdiscs.Get(0);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0d1);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4.Assign(d2d3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable pcap
    p2pHigh.EnablePcapAll("tcp-bbr-nonbottleneck");
    p2pBottleneck.EnablePcapAll("tcp-bbr-bottleneck");

    uint16_t port = 50000;
    Address sinkAddress(InetSocketAddress(i2i3.GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(100.0));

    // Sender socket/app
    Ptr<TcpSocket> tcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    tcpSocket->SetAttribute("CongestionControlAlgorithm", StringValue("BBR"));
    senderSocket = DynamicCast<const TcpSocketBase>(tcpSocket);

    // BulkSend to generate continuous TCP traffic
    BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
    bulkSender.SetAttribute("MaxBytes", UintegerValue(0));
    bulkSender.SetAttribute("SendSize", UintegerValue(1448));
    ApplicationContainer app = bulkSender.Install(nodes.Get(0));
    app.Start(Seconds(0.1));
    app.Stop(Seconds(100.0));

    // Open trace files
    cwndStream.open("cwnd.tr");
    throughputStream.open("throughput.tr");
    queueStream.open("queue.tr");

    // Set up traces
    tcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndTracer));

    lastTxBytes = 0;
    lastTime = 0.0;
    Simulator::Schedule(Seconds(0.1), &ThroughputTracer);
    Simulator::Schedule(Seconds(0.01), &QueueTracer);

    // Schedule BBR PROBE_RTT phase every 10 seconds
    Simulator::Schedule(Seconds(10.0), [tcpSocket]() {
        EnterProbeRtt(tcpSocket);
        ScheduleProbeRtt(tcpSocket);
    });

    Simulator::Stop(Seconds(100.0));
    Simulator::Run();

    cwndStream.close();
    throughputStream.close();
    queueStream.close();

    Simulator::Destroy();
    return 0;
}