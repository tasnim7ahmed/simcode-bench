#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install TCP/IP stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up application: TCP BulkSend from n0 to n1
    uint16_t port = 9;
    BulkSendHelper source("ns3::TcpSocketFactory",
                          InetSocketAddress(interfaces.GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer sourceApps = source.Install(nodes.Get(0));
    sourceApps.Start(Seconds(0.0));
    sourceApps.Stop(Seconds(10.0));

    // Set up Sink on n1
    PacketSinkHelper sink("ns3::TcpSocketFactory",
                          InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // Install FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Throughput calculation
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationAddress == interfaces.GetAddress(1))
        {
            double throughput = flow.second.rxBytes * 8.0 / (10.0 * 1e6); // Mbps
            std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Bytes:   " << flow.second.txBytes << "\n";
            std::cout << "  Rx Bytes:   " << flow.second.rxBytes << "\n";
            std::cout << "  Throughput: " << throughput << " Mbps\n";
        }
    }

    Simulator::Destroy();
    return 0;
}