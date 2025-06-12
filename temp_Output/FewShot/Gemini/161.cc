#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // TCP Server and Client
    uint16_t tcpPort = 50000;
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), tcpPort));
    bulkSendHelper.SetAttribute("SendSize", UintegerValue(1024));
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(1000000));

    ApplicationContainer tcpClientApp = bulkSendHelper.Install(nodes.Get(0));
    tcpClientApp.Start(Seconds(1.0));
    tcpClientApp.Stop(Seconds(10.0));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer tcpServerApp = packetSinkHelper.Install(nodes.Get(1));
    tcpServerApp.Start(Seconds(0.0));
    tcpServerApp.Stop(Seconds(10.0));

    // UDP Server and Client
    uint16_t udpPort = 50001;
    UdpClientHelper udpClient(interfaces.GetAddress(1), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer udpClientApp = udpClient.Install(nodes.Get(0));
    udpClientApp.Start(Seconds(1.0));
    udpClientApp.Stop(Seconds(10.0));

    PacketSinkHelper udpServer("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    ApplicationContainer udpServerApp = udpServer.Install(nodes.Get(1));
    udpServerApp.Start(Seconds(0.0));
    udpServerApp.Stop(Seconds(10.0));


    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    // Enable tracing for NetAnim
    AnimationInterface anim("tcp-udp-comparison.xml");
    anim.SetConstantPosition(nodes.Get(0), 10, 10);
    anim.SetConstantPosition(nodes.Get(1), 100, 10);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
        std::cout << "  Packet Loss Ratio: " << (double)i->second.lostPackets / i->second.txPackets << "\n";
        std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
    }

    monitor->SerializeToXmlFile("tcp-udp-comparison.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}