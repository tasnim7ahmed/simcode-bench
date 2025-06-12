#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging (optional)
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer spokeNodes;
    spokeNodes.Create(4);

    NodeContainer allNodes;
    allNodes.Add(hubNode);
    allNodes.Add(spokeNodes);

    // Configure point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer hubDevices;
    NetDeviceContainer spokeDevices;

    for (uint32_t i = 0; i < spokeNodes.GetN(); ++i) {
        NetDeviceContainer link = pointToPoint.Install(hubNode.Get(0), spokeNodes.Get(i));
        hubDevices.Add(link.Get(0));
        spokeDevices.Add(link.Get(1));
    }

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer hubInterfaces = address.Assign(hubDevices);
    Ipv4InterfaceContainer spokeInterfaces;

    for (uint32_t i = 0; i < spokeDevices.GetN(); ++i) {
        spokeInterfaces.Add(address.AssignOne(spokeDevices.Get(i)));
        address.NewNetwork();
    }
    
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install UDP echo server on the hub node
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(hubNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Install UDP echo clients on the spoke nodes
    UdpEchoClientHelper echoClient(hubInterfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < spokeNodes.GetN(); ++i) {
        ApplicationContainer clientApp = echoClient.Install(spokeNodes.Get(i));
        clientApp.Start(Seconds(2.0 + i * 0.1));
        clientApp.Stop(Seconds(10.0));
        clientApps.Add(clientApp);
    }

    // Enable PCAP tracing
    pointToPoint.EnablePcapAll("star");

    // Install FlowMonitor
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Analyze results
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Packet Delivery Ratio: " << (double)i->second.rxPackets / (double)i->second.txPackets << "\n";
        std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    monitor->SerializeToXmlFile("star.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}