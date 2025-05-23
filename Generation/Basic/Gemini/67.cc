#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/csma-module.h"
#include "ns3/stats-module.h"

#include <string>
#include <fstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellSimulation");

// Trace function for TCP congestion window
void
TraceCwnd(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    *stream << Simulator::Now().GetSeconds() << "," << newCwnd << std::endl;
}

// Trace function for QueueDisc packets in queue
void
TraceQueueSize(Ptr<OutputStreamWrapper> stream, uint32_t oldValue, uint32_t newValue)
{
    *stream << Simulator::Now().GetSeconds() << "," << newValue << std::endl;
}

void
experiment(std::string queueDiscType)
{
    NS_LOG_INFO("Running experiment with " << queueDiscType << " Queue Disc");

    double simTime = 10.0; // seconds

    // 1. Configure global parameters
    // Set TCP variant to Cubic
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpCubic::GetTypeId()));
    // Set default packet size and data rate for OnOff applications
    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(1400));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("50Mbps")); // Max rate, will be limited by bottleneck

    // 2. Node Creation
    NodeContainer senders;
    senders.Create(7);
    NodeContainer receivers;
    receivers.Create(1);
    NodeContainer routers;
    routers.Create(2);

    // Get specific router nodes and the receiver node for clarity
    Ptr<Node> routerA = routers.Get(0);
    Ptr<Node> routerB = routers.Get(1);
    Ptr<Node> receiverNode = receivers.Get(0);

    // 3. Internet Stack Installation
    InternetStackHelper stack;
    stack.Install(senders);
    stack.Install(receivers);
    stack.Install(routers);

    // 4. Device and IP Address Installation
    // Sender-RouterA LAN (CSMA)
    CsmaHelper senderCsma;
    senderCsma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    senderCsma.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer senderNetDevices = senderCsma.Install(NodeContainer(senders, routerA));
    Ipv4AddressHelper senderIp;
    senderIp.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer senderInterfaces = senderIp.Assign(senderNetDevices);

    // RouterB-Receiver LAN (CSMA)
    CsmaHelper receiverCsma;
    receiverCsma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    receiverCsma.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer receiverNetDevices = receiverCsma.Install(NodeContainer(routerB, receiverNode));
    Ipv4AddressHelper receiverIp;
    receiverIp.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer receiverInterfaces = receiverIp.Assign(receiverNetDevices);

    // RouterA-RouterB P2P Link (Bottleneck)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps")); // Bottleneck data rate
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));     // Bottleneck propagation delay
    NetDeviceContainer routerP2pDevices = p2p.Install(routers);
    Ipv4AddressHelper p2pIp;
    p2pIp.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces = p2pIp.Assign(routerP2pDevices);

    // 5. Queue Disc Installation on Bottleneck Link
    // The queue disc is installed on the egress interface of each router in the bottleneck link
    TrafficControlHelper tch;
    if (queueDiscType == "CoDel")
    {
        tch.SetRootQueueDisc("ns3::CoDelQueueDisc");
    }
    else if (queueDiscType == "Cobalt")
    {
        // Example Cobalt parameters. User can tune these.
        // Alpha controls the rate of increase of mark probability with queue length.
        // Beta controls the rate of decrease of mark probability with queue length.
        tch.SetRootQueueDisc("ns3::CobaltQueueDisc", "Alpha", DoubleValue(1.0), "Beta", DoubleValue(0.5));
    }
    else
    {
        NS_FATAL_ERROR("Unknown queue disc type: " << queueDiscType);
    }

    // Install queue discs on the devices connecting routers
    // routerP2pDevices.Get(0) is routerA's P2P device (connected to routerB)
    // routerP2pDevices.Get(1) is routerB's P2P device (connected to routerA)
    QueueDiscContainer routerAQueueDisc = tch.Install(routerP2pDevices.Get(0));
    QueueDiscContainer routerBQueueDisc = tch.Install(routerP2pDevices.Get(1)); 

    // 6. Applications
    // Get the IP address of the receiver node on its LAN interface
    Ptr<Ipv4> receiverIpv4 = receiverNode->GetObject<Ipv4>();
    // The receiver's IP address on the 10.1.2.0/24 network (index 1 for interface)
    Ipv4Address receiverAddr = receiverIpv4->GetAddress(1, 0).GetLocal(); 

    // TCP Traffic (7 senders to 1 receiver)
    uint16_t tcpPort = 9;
    PacketSinkHelper tcpSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::Any, tcpPort));
    ApplicationContainer tcpSinkApps = tcpSink.Install(receiverNode);
    tcpSinkApps.Start(Seconds(0.0));
    tcpSinkApps.Stop(Seconds(simTime));

    OnOffHelper tcpOnOff("ns3::TcpSocketFactory", Address()); 
    tcpOnOff.SetAttribute("Remote", AddressValue(InetSocketAddress(receiverAddr, tcpPort)));
    tcpOnOff.SetAttribute("PacketSize", UintegerValue(1400));
    tcpOnOff.SetAttribute("DataRate", StringValue("50Mbps")); // Attempt to saturate the bottleneck link

    ApplicationContainer tcpSrcApps;
    for (uint32_t i = 0; i < senders.GetN(); ++i)
    {
        tcpSrcApps.Add(tcpOnOff.Install(senders.Get(i)));
    }
    tcpSrcApps.Start(Seconds(1.0)); // Start after network setup
    tcpSrcApps.Stop(Seconds(simTime - 0.1)); // Stop slightly before simulation end

    // UDP Traffic (7 senders to 1 receiver)
    uint16_t udpPort = 10;
    PacketSinkHelper udpSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::Any, udpPort));
    ApplicationContainer udpSinkApps = udpSink.Install(receiverNode);
    udpSinkApps.Start(Seconds(0.0));
    udpSinkApps.Stop(Seconds(simTime));

    OnOffHelper udpOnOff("ns3::UdpSocketFactory", Address()); 
    udpOnOff.SetAttribute("Remote", AddressValue(InetSocketAddress(receiverAddr, udpPort)));
    udpOnOff.SetAttribute("PacketSize", UintegerValue(1400));
    udpOnOff.SetAttribute("DataRate", StringValue("5Mbps")); // Moderate UDP background traffic

    ApplicationContainer udpSrcApps;
    for (uint32_t i = 0; i < senders.GetN(); ++i)
    {
        udpSrcApps.Add(udpOnOff.Install(senders.Get(i)));
    }
    udpSrcApps.Start(Seconds(1.0));
    udpSrcApps.Stop(Seconds(simTime - 0.1));

    // 7. Tracing
    // TCP Congestion Window Trace for each TCP sender
    for (uint32_t i = 0; i < tcpSrcApps.GetN(); ++i)
    {
        Ptr<OnOffApplication> app = tcpSrcApps.Get(i)->GetObject<OnOffApplication>();
        if (app) {
            Ptr<Socket> socket = app->GetSocket();
            Ptr<TcpSocket> tcpSocket = DynamicCast<TcpSocket>(socket);
            if (tcpSocket)
            {
                std::string filename = "Cwnd_TcpSender" + std::to_string(senders.Get(i)->GetId()) + "_" + queueDiscType + ".csv";
                Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>(filename, std::ios::out);
                *stream << "Time,Cwnd" << std::endl; // Header
                tcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&TraceCwnd, stream));
            }
        }
    }

    // Queue Size Trace for routerA's bottleneck interface (packets in queue)
    std::string qSizeFilenameA = "QueueSize_RouterA_to_RouterB_" + queueDiscType + ".csv";
    Ptr<OutputStreamWrapper> qStreamA = Create<OutputStreamWrapper>(qSizeFilenameA, std::ios::out);
    *qStreamA << "Time,PacketsInQueue" << std::endl; // Header
    routerAQueueDisc.Get(0)->TraceConnectWithoutContext("PacketsInQueue", MakeBoundCallback(&TraceQueueSize, qStreamA));

    // Queue Size Trace for routerB's bottleneck interface (packets in queue, for symmetry)
    std::string qSizeFilenameB = "QueueSize_RouterB_to_RouterA_" + queueDiscType + ".csv";
    Ptr<OutputStreamWrapper> qStreamB = Create<OutputStreamWrapper>(qSizeFilenameB, std::ios::out);
    *qStreamB << "Time,PacketsInQueue" << std::endl; // Header
    routerBQueueDisc.Get(0)->TraceConnectWithoutContext("PacketsInQueue", MakeBoundCallback(&TraceQueueSize, qStreamB));

    // 8. Simulation Execution
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Finished experiment with " << queueDiscType << " Queue Disc");
}

int
main(int argc, char* argv[])
{
    // Enable logging for relevant components
    LogComponentEnable("DumbbellSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("TcpSocket", LOG_LEVEL_INFO); // For TCP flow details

    // Run the simulation with CoDel queue discipline
    experiment("CoDel");

    // Run the simulation with Cobalt queue discipline
    experiment("Cobalt");

    return 0;
}