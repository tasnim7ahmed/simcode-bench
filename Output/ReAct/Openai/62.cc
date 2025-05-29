#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <string>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPacingScenario");

std::ofstream cwndStream;
std::ofstream pacingRateStream;
std::ofstream txStream;
std::ofstream rxStream;

void
CwndTracer(uint32_t oldCwnd, uint32_t newCwnd)
{
    double time = Simulator::Now().GetSeconds();
    cwndStream << std::fixed << std::setprecision(6)
               << time << "\t" << newCwnd << std::endl;
}

void
PacingRateTracer(uint32_t sockId, double newRate)
{
    double time = Simulator::Now().GetSeconds();
    pacingRateStream << std::fixed << std::setprecision(6)
                     << time << "\t" << newRate << std::endl;
}

void
TxTracer(Ptr<const Packet> packet)
{
    double time = Simulator::Now().GetSeconds();
    txStream << std::fixed << std::setprecision(6) << time << "\t" << packet->GetSize() << std::endl;
}

void
RxTracer(Ptr<const Packet> packet, const Address &)
{
    double time = Simulator::Now().GetSeconds();
    rxStream << std::fixed << std::setprecision(6) << time << "\t" << packet->GetSize() << std::endl;
}

void
TraceSocket(Ptr<Socket> sock)
{
    sock->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndTracer));
#if 0 // TCP Pacing traces are protocol dependent; use attribute for simplicity
    // When available, attach to pacing rate changes
#endif
}

void
TraceTcp(Ptr<Node> node, Ptr<Socket> sock)
{
    // Trace congestion window
    sock->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndTracer));

    // Attempt to trace pacing rate (for TCPBbr, TcpPacing, Ns3TcpSocketBase supports PacingRate trace)
    Ptr<Object> obj = sock;
    if (obj->GetInstanceTypeId().HasTraceSource("PacingRate")) {
        sock->TraceConnectWithoutContext("PacingRate", MakeBoundCallback(&PacingRateTracer, sock->GetId()));
    }
}

void
SetupTracing(Ptr<Socket> socket, ApplicationContainer apps)
{
    TraceTcp(socket->GetNode(), socket);
    apps.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&TxTracer));
    apps.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&RxTracer));
}

int
main(int argc, char *argv[])
{
    // Command-line parameters
    bool enablePacing = true;
    double simTime = 10.0; // seconds
    std::string tcpVariant = "ns3::TcpBbr";
    uint32_t initCwnd = 10;
    std::string dataRate1 = "50Mbps"; // non-bottleneck
    std::string delay1 = "1ms";
    std::string dataRate2 = "10Mbps"; // bottleneck
    std::string delay2 = "20ms";
    uint32_t packetSize = 1448;
    uint32_t maxBytes = 0;

    CommandLine cmd;
    cmd.AddValue("EnablePacing", "Enable TCP pacing on sender sockets", enablePacing);
    cmd.AddValue("SimTime", "Simulation time (seconds)", simTime);
    cmd.AddValue("TcpVariant", "TCP variant (e.g., ns3::TcpBbr, ns3::TcpCubic, ns3::TcpPcc)", tcpVariant);
    cmd.AddValue("InitCwnd", "Initial congestion window (segments)", initCwnd);
    cmd.AddValue("PacketSize", "Application packet size (Bytes)", packetSize);
    cmd.AddValue("MaxBytes", "Maximum number of bytes to send (0 means unlimited)", maxBytes);
    cmd.Parse(argc, argv);

    // Output files for traces
    cwndStream.open("cwnd.tr");
    pacingRateStream.open("pacing_rate.tr");
    txStream.open("tx_times.tr");
    rxStream.open("rx_times.tr");

    // Create network topology
    NodeContainer nodes;
    nodes.Create(6);

    // Helper to keep record of NetDeviceContainers per link
    std::vector<NetDeviceContainer> devs(5);
    std::vector<PointToPointHelper> p2p(5);

    // Interconnect the nodes:
    // n0--n1--n2==n3--n4--n5
    // Link 0: n0-n1
    // Link 1: n1-n2
    // Link 2: n2-n3 <-- bottleneck
    // Link 3: n3-n4
    // Link 4: n4-n5

    p2p[0].SetDeviceAttribute("DataRate", StringValue(dataRate1));
    p2p[0].SetChannelAttribute("Delay", StringValue(delay1));
    devs[0] = p2p[0].Install(nodes.Get(0), nodes.Get(1));

    p2p[1].SetDeviceAttribute("DataRate", StringValue(dataRate1));
    p2p[1].SetChannelAttribute("Delay", StringValue(delay1));
    devs[1] = p2p[1].Install(nodes.Get(1), nodes.Get(2));

    p2p[2].SetDeviceAttribute("DataRate", StringValue(dataRate2)); // bottleneck!
    p2p[2].SetChannelAttribute("Delay", StringValue(delay2));
    devs[2] = p2p[2].Install(nodes.Get(2), nodes.Get(3));

    p2p[3].SetDeviceAttribute("DataRate", StringValue(dataRate1));
    p2p[3].SetChannelAttribute("Delay", StringValue(delay1));
    devs[3] = p2p[3].Install(nodes.Get(3), nodes.Get(4));

    p2p[4].SetDeviceAttribute("DataRate", StringValue(dataRate1));
    p2p[4].SetChannelAttribute("Delay", StringValue(delay1));
    devs[4] = p2p[4].Install(nodes.Get(4), nodes.Get(5));

    // Install protocol stacks
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses on each link
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces(5);

    address.SetBase("10.0.1.0", "255.255.255.0");
    interfaces[0] = address.Assign(devs[0]);
    address.SetBase("10.0.2.0", "255.255.255.0");
    interfaces[1] = address.Assign(devs[1]);
    address.SetBase("10.0.3.0", "255.255.255.0");
    interfaces[2] = address.Assign(devs[2]);
    address.SetBase("10.0.4.0", "255.255.255.0");
    interfaces[3] = address.Assign(devs[3]);
    address.SetBase("10.0.5.0", "255.255.255.0");
    interfaces[4] = address.Assign(devs[4]);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set TCP variant
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(tcpVariant));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(initCwnd));

    // Optionally enable TCP pacing
    if (enablePacing) {
        if (tcpVariant == "ns3::TcpBbr" || tcpVariant == "ns3::TcpPcc" || tcpVariant == "ns3::TcpPrr" || tcpVariant == "ns3::TcpDctcp" ||
            tcpVariant == "ns3::TcpPacing" || tcpVariant == "ns3::TcpAdaptiveReno" || tcpVariant == "ns3::TcpNewReno" ||
            tcpVariant == "ns3::TcpCubic") {
            Config::SetDefault("ns3::TcpSocketBase::Pacing", BooleanValue(true));
        }
    }

    // Create a TCP flow from n2 to n3 (sender: n2, receiver: n3)
    uint16_t port = 50000;
    // Install sink on n3
    Address sinkAddr(InetSocketAddress(interfaces[2].GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime + 1.0));

    // Install BulkSendApp (TCP source) on n2
    BulkSendHelper sourceHelper("ns3::TcpSocketFactory", sinkAddr);
    sourceHelper.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    sourceHelper.SetAttribute("SendSize", UintegerValue(packetSize));
    ApplicationContainer sourceApp = sourceHelper.Install(nodes.Get(2));
    sourceApp.Start(Seconds(0.5));
    sourceApp.Stop(Seconds(simTime + 1.0));

    // Attach tracing (find the socket)
    sourceApp.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&TxTracer));
    sinkApp.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&RxTracer));

    // Trace congestion window
    Ptr<Socket> tcpSocket = Socket::CreateSocket(nodes.Get(2), TcpSocketFactory::GetTypeId());
    tcpSocket->Bind();
    TraceTcp(nodes.Get(2), tcpSocket);

    // FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simTime + 1.0));
    Simulator::Run();

    // Print flow monitor statistics
    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = flowmon->GetFlowStats();
    std::cout << "------ Flow Monitor Statistics ------" << std::endl;
    for (const auto &flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow " << flow.first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes: " << flow.second.txBytes << "\n";
        std::cout << "  Rx Bytes: " << flow.second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        std::cout << "  Throughput: " << (flow.second.rxBytes * 8.0 / (simTime * 1000000.0)) << " Mbps\n";
        std::cout << "  Delay Sum: " << flow.second.delaySum.GetSeconds() << " s\n";
        std::cout << "  Jitter Sum: " << flow.second.jitterSum.GetSeconds() << " s\n";
        std::cout << "  Packets: " << flow.second.rxPackets << "\n";
    }
    std::cout << "-------------------------------------" << std::endl;

    cwndStream.close();
    pacingRateStream.close();
    txStream.close();
    rxStream.close();

    Simulator::Destroy();
    return 0;
}