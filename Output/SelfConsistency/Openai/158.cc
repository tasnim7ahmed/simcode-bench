#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO); // For debugging

    // Set default values
    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(1024));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("5Mbps"));

    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // 2. Install point-to-point device and channel
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // 3. Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // 4. Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 5. Set up TCP server (PacketSink) on node 1
    uint16_t tcpPort = 8080;
    Address bindAddress(InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", bindAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1)); // node 1 as server
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // 6. Set up TCP client (OnOffApplication) on node 0
    AddressValue remoteAddress(InetSocketAddress(interfaces.GetAddress(1), tcpPort));
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address());
    onOffHelper.SetAttribute("Remote", remoteAddress);
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    onOffHelper.SetAttribute("DataRate", StringValue("5Mbps"));
    onOffHelper.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    onOffHelper.SetAttribute("StopTime", TimeValue(Seconds(10.0)));

    ApplicationContainer clientApp = onOffHelper.Install(nodes.Get(0));

    // 7. Enable FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    // 8. Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // 9. Print flow statistics
    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    std::cout << "Flow statistics:\n";
    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow ID: " << flow.first
                  << " Src Addr: " << t.sourceAddress
                  << " Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << flow.second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << flow.second.rxPackets << std::endl;
        std::cout << "  Throughput: ";
        if (flow.second.timeLastRxPacket.GetSeconds() > 0)
        {
            double throughput = flow.second.rxBytes * 8.0 /
                                (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) /
                                1e6; // Mbps
            std::cout << throughput << " Mbps" << std::endl;
        }
        else
        {
            std::cout << "0 Mbps" << std::endl;
        }
    }

    // 10. Cleanup
    Simulator::Destroy();
    return 0;
}