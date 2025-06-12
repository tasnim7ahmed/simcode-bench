#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

void
ThroughputMonitor (Ptr<FlowMonitor> flowMonitor, Ptr<Ipv4FlowClassifier> classifier)
{
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    for (auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationPort == 5000)
        {
            double time = Simulator::Now().GetSeconds();
            double throughput = 0;
            if (flow.second.timeLastRxPacket.GetSeconds() > 0)
            {
                throughput = (flow.second.rxBytes * 8.0) /
                             (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1e6; // Mbps
            }
            std::cout << "Time: " << time << "s "
                      << "Throughput: " << throughput << " Mbps" << std::endl;
        }
    }
    Simulator::Schedule (Seconds(1.0), &ThroughputMonitor, flowMonitor, classifier);
}

int
main (int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create (2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer devices = pointToPoint.Install (nodes);

    pointToPoint.EnablePcapAll("p2p-tcp");
    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("p2p-tcp.tr"));

    InternetStackHelper stack;
    stack.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    uint16_t port = 5000;
    Address serverAddress (InetSocketAddress (interfaces.GetAddress (0), port));

    // TCP Server (Node A)
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer serverApp = packetSinkHelper.Install (nodes.Get(0));
    serverApp.Start (Seconds(0.0));
    serverApp.Stop (Seconds(20.0));

    // TCP Client (Node B)
    OnOffHelper clientHelper ("ns3::TcpSocketFactory", serverAddress);
    clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("4Mbps")));
    clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
    clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApp = clientHelper.Install (nodes.Get(1));
    clientApp.Start (Seconds(1.0));
    clientApp.Stop (Seconds(19.0));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());

    Simulator::Schedule (Seconds(2.0), &ThroughputMonitor, monitor, classifier);

    Simulator::Stop (Seconds (20.0));
    Simulator::Run ();

    monitor->SerializeToXmlFile("p2p-tcp-flowmon.xml", true, true);
    Simulator::Destroy ();
    return 0;
}