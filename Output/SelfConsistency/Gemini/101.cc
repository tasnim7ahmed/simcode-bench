#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleGlobalRouting");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetAttribute("UdpClient", "PacketSize", UintegerValue(210));

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1p"));

    NetDeviceContainer devices02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices23 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);
    address.NewNetwork();

    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);
    address.NewNetwork();

    Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP application from n0 to n3
    UdpClientHelper client0(interfaces23.GetAddress(1), 9);
    client0.SetAttribute("Interval", TimeValue(MilliSeconds(0.375))); // Adjust interval for 448kbps
    client0.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    ApplicationContainer clientApps0 = client0.Install(nodes.Get(0));
    clientApps0.Start(Seconds(1.0));
    clientApps0.Stop(Seconds(10.0));

    // UDP application from n3 to n1
    UdpClientHelper client3(interfaces12.GetAddress(0), 9);
    client3.SetAttribute("Interval", TimeValue(MilliSeconds(0.375))); // Adjust interval for 448kbps
    client3.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    ApplicationContainer clientApps3 = client3.Install(nodes.Get(3));
    clientApps3.Start(Seconds(1.0));
    clientApps3.Stop(Seconds(10.0));

    UdpServerHelper server(9);
    ApplicationContainer serverApps0 = server.Install(nodes.Get(3));
    serverApps0.Start(Seconds(0.9));
    serverApps0.Stop(Seconds(10.0));
    ApplicationContainer serverApps1 = server.Install(nodes.Get(1));
    serverApps1.Start(Seconds(0.9));
    serverApps1.Stop(Seconds(10.0));


    // TCP application (FTP) from n0 to n3
    BulkSendHelper ftpClient("ns3::TcpSocketFactory", InetSocketAddress(interfaces23.GetAddress(1), 50000));
    ftpClient.SetAttribute("SendSize", UintegerValue(1024));
    ApplicationContainer ftpClientApps = ftpClient.Install(nodes.Get(0));
    ftpClientApps.Start(Seconds(1.2));
    ftpClientApps.Stop(Seconds(1.35));

    PacketSinkHelper ftpServer("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 50000));
    ApplicationContainer ftpServerApps = ftpServer.Install(nodes.Get(3));
    ftpServerApps.Start(Seconds(1.1));
    ftpServerApps.Stop(Seconds(1.4));

    // Tracing
    Simulator::TraceConfig("./simple-global-routing.tr");

    // FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}