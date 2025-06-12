#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer clients;
    clients.Create(5);

    NodeContainer server;
    server.Create(1);

    NodeContainer allNodes;
    allNodes.Add(clients);
    allNodes.Add(server);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer clientInterfaces[5];
    PointToPointNetDeviceContainer devices[5];
    for (int i = 0; i < 5; ++i) {
        NodeContainer link;
        link.Add(clients.Get(i));
        link.Add(server.Get(0));
        devices[i] = pointToPoint.Install(link);
        clientInterfaces[i] = address.Assign(devices[i]);
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(clientInterfaces[0].GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer sourceApps = source.Install(clients.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));

    for (int i = 1; i < 5; ++i) {
        BulkSendHelper source_i("ns3::TcpSocketFactory", InetSocketAddress(clientInterfaces[i].GetAddress(1), port + i));
        source_i.SetAttribute("MaxBytes", UintegerValue(1000000));
        ApplicationContainer sourceApps_i = source_i.Install(clients.Get(i));
        sourceApps_i.Start(Seconds(1.0));
        sourceApps_i.Stop(Seconds(10.0));
    }

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(server.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(11.0));

    for (int i = 1; i < 5; ++i) {
        PacketSinkHelper sink_i("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + i));
        ApplicationContainer sinkApps_i = sink_i.Install(server.Get(0));
        sinkApps_i.Start(Seconds(0.0));
        sinkApps_i.Stop(Seconds(11.0));
    }

    AnimationInterface anim("star-topology.xml");
    anim.SetConstantPosition(server.Get(0), 10.0, 10.0);
    anim.SetConstantPosition(clients.Get(0), 20.0, 20.0);
    anim.SetConstantPosition(clients.Get(1), 20.0, 30.0);
    anim.SetConstantPosition(clients.Get(2), 20.0, 40.0);
    anim.SetConstantPosition(clients.Get(3), 20.0, 50.0);
    anim.SetConstantPosition(clients.Get(4), 20.0, 60.0);

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(11.0));

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
        std::cout << "  Packet Loss Ratio: " << (double)i->second.lostPackets / (double)i->second.txPackets << std::endl;
        std::cout << "  Delay Sum: " << i->second.delaySum << std::endl;
    }

    Simulator::Destroy();

    return 0;
}