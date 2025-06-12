#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue-disc-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

void
CwndTracer(std::string filename, uint32_t oldCwnd, uint32_t newCwnd)
{
    static std::ofstream cwndStream;
    static bool first = true;
    if (first)
    {
        cwndStream.open(filename, std::ios_base::out);
        first = false;
    }
    cwndStream << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

void
ThroughputTracer(std::string filename, Ptr<Application> app, Ptr<Socket> socket)
{
    static std::ofstream thrStream;
    static bool first = true;
    if (first)
    {
        thrStream.open(filename, std::ios_base::out);
        first = false;
    }
    static uint64_t lastTotalTx = 0;
    static Time lastTime = Seconds(0);
    uint64_t currTx = socket->GetTxAvailable();
    Time now = Simulator::Now();
    double throughput = 0.0;
    if (now > lastTime)
    {
        throughput = ((currTx - lastTotalTx) * 8.0) / (now.GetSeconds() - lastTime.GetSeconds()) / 1e6;
        thrStream << now.GetSeconds() << "\t" << throughput << std::endl;
        lastTotalTx = currTx;
        lastTime = now;
    }
    Simulator::Schedule(Seconds(0.1), &ThroughputTracer, filename, app, socket);
}

void
QueueTracer(std::string filename, Ptr<QueueDisc> queue)
{
    static std::ofstream qStream;
    static bool first = true;
    if (first)
    {
        qStream.open(filename, std::ios_base::out);
        first = false;
    }
    qStream << Simulator::Now().GetSeconds() << "\t" << queue->GetCurrentSize().GetValue() << std::endl;
    Simulator::Schedule(Seconds(0.01), &QueueTracer, filename, queue);
}

void
ForceBbrProbeRtt(Ptr<Node> sender)
{
    // Enter PROBE_RTT and reduce to 4 segments using socket parameters
    Ptr<Ipv4L3Protocol> ipv4 = sender->GetObject<Ipv4L3Protocol>();
    for (uint32_t i = 0; i < sender->GetNApplications(); ++i)
    {
        Ptr<Application> app = sender->GetApplication(i);
        Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication>(app);
        if (bulk)
        {
            Ptr<Socket> socket = bulk->GetSocket();
            if (socket)
            {
                Ptr<TcpSocketBase> tcpSocket = DynamicCast<TcpSocketBase>(socket);
                if (tcpSocket)
                {
                    tcpSocket->ForceProbeRtt();
                    tcpSocket->SetAttribute("CongestionWindow", UintegerValue(4 * tcpSocket->GetSegSize()));
                }
            }
        }
    }
    Simulator::Schedule(Seconds(10.0), &ForceBbrProbeRtt, sender);
}

int main(int argc, char *argv[])
{
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpBbr")));

    NodeContainer nodes;
    nodes.Create(4); // 0: sender, 1: R1, 2: R2, 3: receiver

    // Point-to-point: sender <-> R1
    PointToPointHelper p2p1;
    p2p1.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
    p2p1.SetChannelAttribute("Delay", StringValue("5ms"));
    NetDeviceContainer dev01 = p2p1.Install(nodes.Get(0), nodes.Get(1));

    // Point-to-point: R1 <-> R2 (bottleneck)
    PointToPointHelper p2p2;
    p2p2.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p2.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer dev12 = p2p2.Install(nodes.Get(1), nodes.Get(2));

    // Point-to-point: R2 <-> receiver
    PointToPointHelper p2p3;
    p2p3.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
    p2p3.SetChannelAttribute("Delay", StringValue("5ms"));
    NetDeviceContainer dev23 = p2p3.Install(nodes.Get(2), nodes.Get(3));

    // Enable pcap tracing
    p2p1.EnablePcapAll("sender-r1");
    p2p2.EnablePcapAll("r1-r2");
    p2p3.EnablePcapAll("r2-receiver");

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper addr;
    addr.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iface01 = addr.Assign(dev01);

    addr.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iface12 = addr.Assign(dev12);

    addr.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer iface23 = addr.Assign(dev23);

    // Set up queue disc for bottleneck (R1->R2)
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
    Ptr<QueueDisc> qdisc = tch.Install(dev12.Get(0)).Get(0);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install BulkSend on sender (node 0), socket = TCP BBR.
    uint16_t port = 5000;
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(iface23.GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer srcApp = source.Install(nodes.Get(0));
    srcApp.Start(Seconds(1.0));
    srcApp.Stop(Seconds(100.0));

    // Install packet sink on receiver (node 3)
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(100.0));

    // Congestion window trace
    Ptr<Socket> tcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    tcpSocket->Bind();
    tcpSocket->Connect(InetSocketAddress(iface23.GetAddress(1), port));
    tcpSocket->SetAttribute("CongestionControlAlgorithm", StringValue("Bbr"));

    Config::ConnectWithoutContext(
        "/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
        MakeBoundCallback(&CwndTracer, "cwnd_traces.txt"));

    // Throughput trace for sender
    Ptr<Application> bulkApp = srcApp.Get(0);
    Simulator::Schedule(Seconds(1.0), &ThroughputTracer, "throughput_traces.txt", bulkApp, tcpSocket);

    // Queue size tracing for bottleneck link
    Simulator::Schedule(Seconds(0.0), &QueueTracer, "queue_traces.txt", qdisc);

    // Force PROBE_RTT every 10 seconds
    Simulator::Schedule(Seconds(10.0), &ForceBbrProbeRtt, nodes.Get(0));

    Simulator::Stop(Seconds(100.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}