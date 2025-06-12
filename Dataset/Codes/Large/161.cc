#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpVsUdpComparison");

int main(int argc, char *argv[])
{
    // Set simulation time
    double simTime = 10.0;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create point-to-point channel
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install network devices
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Create a UDP server on node 1
    uint16_t udpPort = 9;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    // Create a UDP client on node 0
    UdpClientHelper udpClient(interfaces.GetAddress(1), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.05)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simTime));

    // Create a TCP server (Packet Sink) on node 1
    uint16_t tcpPort = 8080;
    PacketSinkHelper tcpSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer sinkApp = tcpSink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(simTime));

    // Create a TCP client on node 0
    OnOffHelper tcpClient("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), tcpPort));
    tcpClient.SetAttribute("DataRate", StringValue("10Mbps"));
    tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
    tcpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    tcpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientAppTcp = tcpClient.Install(nodes.Get(0));
    clientAppTcp.Start(Seconds(3.0));
    clientAppTcp.Stop(Seconds(simTime));

    // Set up FlowMonitor
    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    // NetAnim configuration
    AnimationInterface anim("tcp_vs_udp.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 20.0);
    anim.SetConstantPosition(nodes.Get(1), 30.0, 20.0);

    // Run the simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Flow monitor statistics
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        std::cout << "Flow from " << t.sourceAddress << " to " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
        std::cout << "  Throughput: " << it->second.rxBytes * 8.0 / simTime / 1000 / 1000 << " Mbps" << std::endl;
        std::cout << "  Packet Delivery Ratio: " << (double)it->second.rxPackets / it->second.txPackets * 100 << "%" << std::endl;
        std::cout << "  Delay: " << it->second.delaySum.GetSeconds() << " seconds" << std::endl;
    }

    // Cleanup
    Simulator::Destroy();
    return 0;
}

