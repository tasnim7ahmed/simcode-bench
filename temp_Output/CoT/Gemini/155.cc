#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iostream>
#include <iomanip>

using namespace ns3;

int main(int argc, char *argv[]) {

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(5);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[4];
    for (int i = 0; i < 4; ++i) {
        NodeContainer linkNodes;
        linkNodes.Add(nodes.Get(0));
        linkNodes.Add(nodes.Get(i + 1));
        devices[i] = pointToPoint.Install(linkNodes);
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces[4];
    for (int i = 0; i < 4; ++i) {
        interfaces[i] = address.Assign(devices[i]);
        address.NewNetwork();
    }

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    uint16_t port = 9;
    UdpEchoClientHelper echoClient(interfaces[0].GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory",
                                 InetSocketAddress(interfaces[1].GetAddress(1), 5000));
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer bulkSendApp = bulkSendHelper.Install(nodes.Get(0));
    bulkSendApp.Start(Seconds(3.0));
    bulkSendApp.Stop(Seconds(10.0));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                     InetSocketAddress(Ipv4Address::GetAny(), 5000));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(3.0));
    sinkApp.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(11.0));

    std::ofstream ascii;
    ascii.open("star_topology.csv");
    ascii << "Source,Destination,PacketSize,TransmissionTime,ReceptionTime" << std::endl;

    Simulator::Schedule(Seconds(10.5), [&]() {
        monitor->CheckForLostPackets();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
            ascii << t.sourceAddress << "," << t.destinationAddress << "," << i->second.txBytes << ","
                  << i->second.timeFirstTxPacket.GetSeconds() << ","
                  << i->second.timeLastRxPacket.GetSeconds() << std::endl;
        }
        ascii.close();
    });

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}