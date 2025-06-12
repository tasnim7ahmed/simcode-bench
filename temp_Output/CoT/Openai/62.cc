#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/tcp-l4-protocol.h"
#include "ns3/tcp-header.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPacingExample");

std::ofstream cwndStream;
std::ofstream pacingRateStream;
std::ofstream txRxTimesStream;

void
CwndTracer(uint32_t oldCwnd, uint32_t newCwnd)
{
    Simulator::ScheduleNow(&Simulator::ScheduleNow);
    double time = Simulator::Now().GetSeconds();
    cwndStream << std::fixed << std::setprecision(6) << time << "," << newCwnd << std::endl;
}

void
PacingRateTracer(Ptr<const TcpSocketBase> socket)
{
    double time = Simulator::Now().GetSeconds();
    DataRate pacingRate;
    if (socket->GetPacing())
    {
        pacingRate = socket->GetPacingRate();
    }
    else
    {
        pacingRate = DataRate("0bps");
    }
    pacingRateStream << std::fixed << std::setprecision(6) << time << "," << pacingRate.GetBitRate() << std::endl;
    Simulator::Schedule(MilliSeconds(5), &PacingRateTracer, socket);
}

void
TxTracer(Ptr<const Packet> pkt, const TcpHeader& hdr, Ptr<const TcpSocketBase> socket)
{
    double time = Simulator::Now().GetSeconds();
    txRxTimesStream << std::fixed << std::setprecision(6) << time
                    << ",TX," << pkt->GetSize() << "," << hdr.GetSequenceNumber().GetValue() << std::endl;
}

void
RxTracer(Ptr<const Packet> pkt, const TcpHeader& hdr, Ptr<const TcpSocketBase> socket)
{
    double time = Simulator::Now().GetSeconds();
    txRxTimesStream << std::fixed << std::setprecision(6) << time
                    << ",RX," << pkt->GetSize() << "," << hdr.GetSequenceNumber().GetValue() << std::endl;
}

void
TraceTcp(Ptr<Socket> socket)
{
    Ptr<TcpSocketBase> tcpSocket = DynamicCast<TcpSocketBase>(socket);
    if (tcpSocket)
    {
        tcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndTracer));
        tcpSocket->TraceConnectWithoutContext("Tx", MakeBoundCallback(&TxTracer, tcpSocket));
        tcpSocket->TraceConnectWithoutContext("Rx", MakeBoundCallback(&RxTracer, tcpSocket));
        Simulator::Schedule(MilliSeconds(1), &PacingRateTracer, tcpSocket);
    }
}

int
main(int argc, char* argv[])
{
    bool pacing = true;
    double simulationTime = 10.0;
    std::string tcpType = "ns3::TcpCubic";
    uint32_t initialCwnd = 10;
    std::string dataRate_n0n1 = "100Mbps";
    std::string dataRate_n1n2 = "50Mbps";
    std::string dataRate_n2n3 = "10Mbps"; // bottleneck
    std::string dataRate_n3n4 = "50Mbps";
    std::string dataRate_n4n5 = "100Mbps";
    std::string delay_n0n1 = "2ms";
    std::string delay_n1n2 = "5ms";
    std::string delay_n2n3 = "30ms";
    std::string delay_n3n4 = "5ms";
    std::string delay_n4n5 = "2ms";
    uint32_t packetSize = 1448;

    CommandLine cmd;
    cmd.AddValue("pacing", "Enable TCP pacing", pacing);
    cmd.AddValue("simulationTime", "Simulation time (seconds)", simulationTime);
    cmd.AddValue("tcpType", "TCP variant", tcpType);
    cmd.AddValue("initialCwnd", "Initial congestion window (in segments)", initialCwnd);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(tcpType));
    Config::SetDefault("ns3::TcpSocketBase::InitialCwnd", UintegerValue(initialCwnd));
    Config::SetDefault("ns3::TcpSocketBase::Pacing", BooleanValue(pacing));

    NodeContainer nodes;
    nodes.Create(6);

    PointToPointHelper p2p_n0n1;
    p2p_n0n1.SetDeviceAttribute("DataRate", StringValue(dataRate_n0n1));
    p2p_n0n1.SetChannelAttribute("Delay", StringValue(delay_n0n1));

    PointToPointHelper p2p_n1n2;
    p2p_n1n2.SetDeviceAttribute("DataRate", StringValue(dataRate_n1n2));
    p2p_n1n2.SetChannelAttribute("Delay", StringValue(delay_n1n2));

    PointToPointHelper p2p_n2n3;
    p2p_n2n3.SetDeviceAttribute("DataRate", StringValue(dataRate_n2n3));
    p2p_n2n3.SetChannelAttribute("Delay", StringValue(delay_n2n3));

    PointToPointHelper p2p_n3n4;
    p2p_n3n4.SetDeviceAttribute("DataRate", StringValue(dataRate_n3n4));
    p2p_n3n4.SetChannelAttribute("Delay", StringValue(delay_n3n4));

    PointToPointHelper p2p_n4n5;
    p2p_n4n5.SetDeviceAttribute("DataRate", StringValue(dataRate_n4n5));
    p2p_n4n5.SetChannelAttribute("Delay", StringValue(delay_n4n5));

    NetDeviceContainer d0n1 = p2p_n0n1.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d1n2 = p2p_n1n2.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d2n3 = p2p_n2n3.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer d3n4 = p2p_n3n4.Install(nodes.Get(3), nodes.Get(4));
    NetDeviceContainer d4n5 = p2p_n4n5.Install(nodes.Get(4), nodes.Get(5));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> ifaces;
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    ifaces.push_back(ipv4.Assign(d0n1));
    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    ifaces.push_back(ipv4.Assign(d1n2));
    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    ifaces.push_back(ipv4.Assign(d2n3));
    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    ifaces.push_back(ipv4.Assign(d3n4));
    ipv4.SetBase("10.0.5.0", "255.255.255.0");
    ifaces.push_back(ipv4.Assign(d4n5));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;

    // Sink on n3
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime + 1));

    // TCP traffic generator on n2 targeting n3
    Ptr<Socket> tcpSocket = Socket::CreateSocket(nodes.Get(2), TcpSocketFactory::GetTypeId());
    tcpSocket->SetAttribute("SegmentSize", UintegerValue(packetSize));
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    Address serverAddress = InetSocketAddress(ifaces[2].GetAddress(1), port);

    Ptr<BulkSendApplication> app = CreateObject<BulkSendApplication>();
    app->SetAttribute("Socket", PointerValue(tcpSocket));
    app->SetAttribute("Remote", AddressValue(serverAddress));
    app->SetAttribute("MaxBytes", UintegerValue(0));
    nodes.Get(2)->AddApplication(app);
    app->SetStartTime(Seconds(0.1));
    app->SetStopTime(Seconds(simulationTime));

    // Prepare output streams for tracing
    cwndStream.open("cwnd.csv");
    pacingRateStream.open("pacing_rate.csv");
    txRxTimesStream.open("tx_rx_times.csv");
    cwndStream << "Time,Cwnd\n";
    pacingRateStream << "Time,PacingRate_bps\n";
    txRxTimesStream << "Time,Event,Bytes,SeqNo\n";

    // Attach traces to TCP socket
    Simulator::Schedule(Seconds(0.09), &TraceTcp, tcpSocket);

    // FlowMonitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    // Print flow stats
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    std::cout << "\nFlow statistics:\n";
    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "FlowID: " << flow.first << " [" << t.sourceAddress << ":" << t.sourcePort
                  << " --> " << t.destinationAddress << ":" << t.destinationPort << "]\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Packet Loss Ratio: "
                  << ((flow.second.txPackets - flow.second.rxPackets) * 100.0 / flow.second.txPackets) << " %\n";
        std::cout << "  Throughput: " << (flow.second.rxBytes * 8.0 / (simulationTime * 1e6)) << " Mbps\n";
        std::cout << "  Mean Delay: " << (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) << " s\n";
    }

    cwndStream.close();
    pacingRateStream.close();
    txRxTimesStream.close();

    Simulator::Destroy();
    return 0;
}