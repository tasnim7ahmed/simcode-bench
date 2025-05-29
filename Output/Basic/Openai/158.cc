#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set time resolution
    Time::SetResolution(Time::NS);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up point-to-point link attributes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install network stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install TCP server on node 1 (node index 1)
    uint16_t serverPort = 8080;
    Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Install TCP client (OnOffApplication) on node 0 (node index 0)
    AddressValue remoteAddress(InetSocketAddress(interfaces.GetAddress(1), serverPort));
    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("Remote", remoteAddress);
    clientHelper.SetAttribute("DataRate", StringValue("5Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable FlowMonitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // FlowMonitor statistics
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

    std::cout << "Flow Statistics:" << std::endl;
    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow ID: " << flow.first << " "
                  << t.sourceAddress << " -> " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets:   " << flow.second.txPackets << std::endl;
        std::cout << "  Rx Packets:   " << flow.second.rxPackets << std::endl;
        std::cout << "  Tx Bytes:     " << flow.second.txBytes << std::endl;
        std::cout << "  Rx Bytes:     " << flow.second.rxBytes << std::endl;

        double timeDuration = flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds();
        double throughput = timeDuration > 0 ? ((flow.second.rxBytes * 8.0) / (timeDuration * 1000000.0)) : 0;
        std::cout << "  Throughput:   " << throughput << " Mbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}