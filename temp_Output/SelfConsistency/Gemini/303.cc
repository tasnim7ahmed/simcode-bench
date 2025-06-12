#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpClientServer");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("TcpClientServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create point-to-point channel
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install net devices
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Create TCP server application
    uint16_t port = 8080;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Create TCP client application
    Ptr<Node> appSource = nodes.Get(0);
    Address remoteAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", remoteAddress);
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(500000));
    ApplicationContainer sourceApp = bulkSendHelper.Install(appSource);
    sourceApp.Start(Seconds(2.0));
    sourceApp.Stop(Seconds(10.0));

    // Enable flow monitor
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Analyze results
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        NS_LOG_INFO("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
        NS_LOG_INFO("  Tx Bytes:   " << i->second.txBytes);
        NS_LOG_INFO("  Rx Bytes:   " << i->second.rxBytes);
        NS_LOG_INFO("  Lost Packets: " << i->second.lostPackets);
    }

    monitor->SerializeToXmlFile("tcp-client-server.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}