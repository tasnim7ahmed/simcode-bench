#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/core-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaSimulation");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetPrintLevel(LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(5); // 4 client nodes + 1 server node

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(csmaDevices);

    // Create a server application on node 4
    uint16_t port = 50000;
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(4), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(4));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));

    // Create client applications on nodes 0-3, sending to server on node 4
    BulkSendHelper sourceClientHelper("ns3::TcpSocketFactory", serverAddress);
    sourceClientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send unlimited data

    ApplicationContainer clientApps;
    for (int i = 0; i < 4; ++i) {
        clientApps.Add(sourceClientHelper.Install(nodes.Get(i)));
        clientApps.Start(Seconds(2.0 + i * 0.1)); // Stagger start times slightly
        clientApps.Stop(Seconds(10.0));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable tracing for PCAP files
    csma.EnablePcap("csma-simulation", csmaDevices, false);

    // Enable NetAnim
    AnimationInterface anim("csma-animation.xml");
    anim.SetConstantPosition(nodes.Get(4), 10, 10);  // Server position
    anim.SetConstantPosition(nodes.Get(0), 20, 20);
    anim.SetConstantPosition(nodes.Get(1), 30, 20);
    anim.SetConstantPosition(nodes.Get(2), 40, 20);
    anim.SetConstantPosition(nodes.Get(3), 50, 20);

    Simulator::Stop(Seconds(11.0));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
        std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";

    }

    monitor->SerializeToXmlFile("csma-simulation.flowmon", true, true);

    Simulator::Destroy();

    return 0;
}