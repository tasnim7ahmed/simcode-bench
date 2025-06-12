#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPacingSimulation");

// Global output streams
std::ofstream cwndStream("cwnd-trace.dat");
std::ofstream pacingStream("pacing-rate-trace.dat");
std::ofstream txStream("tcp-tx-trace.dat");
std::ofstream rxStream("tcp-rx-trace.dat");

// Congestion window tracing
void CwndTracer (std::string context, uint32_t oldCwnd, uint32_t newCwnd) {
    double now = Simulator::Now().GetSeconds();
    cwndStream << std::fixed << std::setprecision(6) << now << " " << newCwnd << std::endl;
}

// Pacing rate tracing
void PacingTracer (std::string context, DataRate oldRate, DataRate newRate) {
    double now = Simulator::Now().GetSeconds();
    pacingStream << std::fixed << std::setprecision(6) << now << " " << newRate.GetBitRate() << std::endl;
}

// Packet transmission tracing
void TxTracer(Ptr<const Packet> packet, const TcpHeader &header, Ptr<const TcpSocketBase> socket) {
    double now = Simulator::Now().GetSeconds();
    txStream << std::fixed << std::setprecision(6) << now << " " << header.GetSequenceNumber().GetValue() << std::endl;
}

// Packet reception tracing
void RxTracer(Ptr<const Packet> packet, const TcpHeader &header, Ptr<const TcpSocketBase> socket) {
    double now = Simulator::Now().GetSeconds();
    rxStream << std::fixed << std::setprecision(6) << now << " " << header.GetSequenceNumber().GetValue() << std::endl;
}

int main(int argc, char *argv[]) {
    // Command-line parameters
    double simTime = 10.0;
    bool tcpPacing = true;
    uint32_t initialCwnd = 2;
    std::string tcpVariant = "TcpCubic";
    std::string dataRate1 = "100Mbps";
    std::string delay1 = "1ms";
    std::string dataRate2 = "10Mbps"; // bottleneck
    std::string delay2 = "10ms"; // bottleneck
    uint32_t maxBytes = 0; // unlimited by default

    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("tcpPacing", "Enable TCP pacing", tcpPacing);
    cmd.AddValue("initialCwnd", "Initial congestion window (segments)", initialCwnd);
    cmd.AddValue("tcpVariant", "TCP variant (TcpCubic, TcpBbr, etc)", tcpVariant);
    cmd.AddValue("maxBytes", "Maximum number of bytes the client can send (0=unlimited)", maxBytes);
    cmd.Parse(argc, argv);

    // Set TCP variant
    if (tcpVariant == "TcpCubic") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpCubic::GetTypeId()));
    } else if (tcpVariant == "TcpBbr") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpBbr::GetTypeId()));
    } else if (tcpVariant == "TcpNewReno") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    } else {
        NS_ABORT_MSG("Unsupported TCP variant");
    }

    // Initial congestion window
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(initialCwnd));

    // TCP pacing
    Config::SetDefault("ns3::TcpSocketBase::Pacing", BooleanValue(tcpPacing));

    // Node topology:
    // n0 -- n1 -- n2 == n3 -- n4 -- n5
    // Bottleneck is n2 <==> n3

    NodeContainer nodes;
    nodes.Create(6);

    // Link helpers
    PointToPointHelper p2p_fast, p2p_bottleneck, p2p_slow;
    p2p_fast.SetDeviceAttribute("DataRate", StringValue(dataRate1));
    p2p_fast.SetChannelAttribute("Delay", StringValue(delay1));
    p2p_bottleneck.SetDeviceAttribute("DataRate", StringValue(dataRate2));
    p2p_bottleneck.SetChannelAttribute("Delay", StringValue(delay2));
    p2p_slow.SetDeviceAttribute("DataRate", StringValue(dataRate1));
    p2p_slow.SetChannelAttribute("Delay", StringValue(delay1));

    NetDeviceContainer d01 = p2p_fast.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d12 = p2p_fast.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d23 = p2p_bottleneck.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer d34 = p2p_slow.Install(nodes.Get(3), nodes.Get(4));
    NetDeviceContainer d45 = p2p_slow.Install(nodes.Get(4), nodes.Get(5));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IPs
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0"); Ipv4InterfaceContainer i01 = address.Assign(d01);
    address.SetBase("10.1.2.0", "255.255.255.0"); Ipv4InterfaceContainer i12 = address.Assign(d12);
    address.SetBase("10.1.3.0", "255.255.255.0"); Ipv4InterfaceContainer i23 = address.Assign(d23);
    address.SetBase("10.1.4.0", "255.255.255.0"); Ipv4InterfaceContainer i34 = address.Assign(d34);
    address.SetBase("10.1.5.0", "255.255.255.0"); Ipv4InterfaceContainer i45 = address.Assign(d45);

    // Populate routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // TCP Source: n2, TCP Sink: n3 (crossing bottleneck)
    uint16_t port = 5001;
    Address sinkAddress(InetSocketAddress(i23.GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime));

    // TCP source
    Ptr<Socket> sourceSocket = Socket::CreateSocket(nodes.Get(2), TcpSocketFactory::GetTypeId());
    // App config
    Ptr<BulkSendApplication> app =
        CreateObject<BulkSendApplication>();
    app->SetAttribute("Socket", PointerValue(sourceSocket));
    app->SetAttribute("Remote", AddressValue(sinkAddress));
    app->SetAttribute("MaxBytes", UintegerValue(maxBytes));
    nodes.Get(2)->AddApplication(app);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(simTime));

    // Attach tracing on the source socket for cwnd, pacing, tx
    std::string srcPrefix = "/NodeList/2/$ns3::TcpL4Protocol/SocketList/0/";

    Config::ConnectWithoutContext("/NodeList/2/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow", MakeCallback(&CwndTracer));
    Config::ConnectWithoutContext("/NodeList/2/$ns3::TcpL4Protocol/SocketList/*/PacingRate", MakeCallback(&PacingTracer));
    sourceSocket->TraceConnectWithoutContext("Tx", MakeCallback(&TxTracer));

    // Attach RX trace on sink
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    sink->TraceConnectWithoutContext("RxWithAddresses", MakeCallback(
        [](Ptr<const Packet> packet, const Address &from, const Address &to) {
            double now = Simulator::Now().GetSeconds();
            rxStream << std::fixed << std::setprecision(6) << now << " " << packet->GetSize() << std::endl;
        }
    ));

    // Run FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Output flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    std::cout << "Flow statistics:" << std::endl;
    for (const auto &flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow " << flow.first << " (" 
                  << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << flow.second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << flow.second.lostPackets << std::endl;
        std::cout << "  Throughput: "
                  << flow.second.rxBytes * 8.0 / (simTime - 1.0) / 1e6  // Mbit/s (accounting for 1s wait)
                  << " Mbps" << std::endl;
        std::cout << "  Mean Delay: "
                  << (flow.second.delaySum.GetSeconds() / (double)flow.second.rxPackets)
                  << " s" << std::endl;
    }
    monitor->SerializeToXmlFile("tcp-pacing-flowmon.xml", true, true);

    // Close output files
    cwndStream.close();
    pacingStream.close();
    txStream.close();
    rxStream.close();

    Simulator::Destroy();
    return 0;
}