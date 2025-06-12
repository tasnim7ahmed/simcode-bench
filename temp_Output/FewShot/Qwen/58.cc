#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-bbr.h"

using namespace ns3;

static void
CwndTracer(Ptr<OutputStreamWrapper> stream, uint32_t oldval, uint32_t newval)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newval << std::endl;
}

static void
QueueTracer(Ptr<OutputStreamWrapper> stream, uint32_t oldval, uint32_t newval)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newval << std::endl;
}

int main(int argc, char *argv[])
{
    // Set default TCP congestion control algorithm to BBR
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpBbr::GetTypeId()));

    // Create nodes
    NodeContainer sender;
    NodeContainer router1;
    NodeContainer router2;
    NodeContainer receiver;

    sender.Create(1);
    router1.Create(1);
    router2.Create(1);
    receiver.Create(1);

    NodeContainer net1(sender, router1);
    NodeContainer net2(router1, router2);
    NodeContainer net3(router2, receiver);

    // Point-to-point helpers
    PointToPointHelper p2p_sender_router1;
    p2p_sender_router1.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
    p2p_sender_router1.SetChannelAttribute("Delay", StringValue("5ms"));

    PointToPointHelper p2p_router1_router2;
    p2p_router1_router2.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p_router1_router2.SetChannelAttribute("Delay", StringValue("10ms"));

    PointToPointHelper p2p_router2_receiver;
    p2p_router2_receiver.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
    p2p_router2_receiver.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer dev_sender_router1 = p2p_sender_router1.Install(net1);
    NetDeviceContainer dev_router1_router2 = p2p_router1_router2.Install(net2);
    NetDeviceContainer dev_router2_receiver = p2p_router2_receiver.Install(net3);

    // Install Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper ip;
    ip.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer if_sender_router1 = ip.Assign(dev_sender_router1);

    ip.NewNetwork();
    Ipv4InterfaceContainer if_router1_router2 = ip.Assign(dev_router1_router2);

    ip.NewNetwork();
    Ipv4InterfaceContainer if_router2_receiver = ip.Assign(dev_router2_receiver);

    // Manually set up routing tables
    Ipv4StaticRoutingHelper routingHelper;

    Ptr<Ipv4> ipv4Sender = sender.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routingSender = routingHelper.GetStaticRouting(ipv4Sender);
    routingSender->AddHostRouteTo(Ipv4Address("10.0.2.2"), Ipv4Address("10.0.0.2"), 1);

    Ptr<Ipv4> ipv4Router1 = router1.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routingRouter1 = routingHelper.GetStaticRouting(ipv4Router1);
    routingRouter1->AddHostRouteTo(Ipv4Address("10.0.2.2"), Ipv4Address("10.0.1.2"), 2);

    Ptr<Ipv4> ipv4Router2 = router2.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routingRouter2 = routingHelper.GetStaticRouting(ipv4Router2);
    routingRouter2->AddHostRouteTo(Ipv4Address("10.0.0.1"), Ipv4Address("10.0.1.1"), 1);
    routingRouter2->AddHostRouteTo(Ipv4Address("10.0.2.2"), Ipv4Address("10.0.2.2"), 2);

    Ptr<Ipv4> ipv4Receiver = receiver.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routingReceiver = routingHelper.GetStaticRouting(ipv4Receiver);
    routingReceiver->AddHostRouteTo(Ipv4Address("10.0.0.1"), Ipv4Address("10.0.2.1"), 1);

    // Install applications
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(if_router2_receiver.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(receiver.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(100.0));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(sender.Get(0), TcpSocketFactory::GetTypeId());

    // Configure OnOff application
    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("Remote", AddressValue(sinkAddress));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1000Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1448));

    ApplicationContainer clientApp = clientHelper.Install(sender.Get(0));
    clientApp.Start(Seconds(0.0));
    clientApp.Stop(Seconds(100.0));

    // Enable PCAP tracing on all interfaces
    p2p_sender_router1.EnablePcapAll("bbr-sender-router1");
    p2p_router1_router2.EnablePcapAll("bbr-router1-router2");
    p2p_router2_receiver.EnablePcapAll("bbr-router2-receiver");

    // Tracing congestion window
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> cwndStream = asciiTraceHelper.CreateFileStream("bbr-cwnd-trace.tr");
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeBoundCallback(&CwndTracer, cwndStream));

    // Tracing queue size on bottleneck link (router1 <-> router2)
    Ptr<PointToPointNetDevice> deviceR1 = DynamicCast<PointToPointNetDevice>(dev_router1_router2.Get(0));
    Ptr<PointToPointNetDevice> deviceR2 = DynamicCast<PointToPointNetDevice>(dev_router1_router2.Get(1));
    if (deviceR1 && deviceR1->GetQueue())
    {
        Ptr<OutputStreamWrapper> queueStream = asciiTraceHelper.CreateFileStream("bbr-queue-size-trace.tr");
        deviceR1->GetQueue()->TraceConnectWithoutContext("BytesInQueue", MakeBoundCallback(&QueueTracer, queueStream));
    }

    // Throughput tracing at the sender
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowMonitorHelper;
    flowMonitor = flowMonitorHelper.InstallAll();

    Simulator::Stop(Seconds(100.0));
    Simulator::Run();

    // Output throughput traces
    flowMonitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        FlowIdTag flowId;
        bool available;
        double throughput = iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000000; // Mbps
        std::ofstream out("bbr-throughput-trace.tr", std::ios::app);
        out << iter->second.timeLastRxPacket.GetSeconds() << " " << throughput << std::endl;
        out.close();
    }

    Simulator::Destroy();
    return 0;
}