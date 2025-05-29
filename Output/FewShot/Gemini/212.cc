#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_INFO);

    // Create Nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Mobility Model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install AODV protocol
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingProtocol("ns3::AodvRoutingProtocol", "AllowedHelloLoss", UintegerValue(2));
    stack.Install(nodes);

    // Assign IPv4 Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(nodes);

    // UDP Traffic
    uint16_t port = 9;
    UdpClientHelper client(interfaces.GetAddress(4), port);
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    UdpServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Enable PCAP Tracing
    PointToPointHelper p2p;
    p2p.EnablePcapAll("aodv", false);

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Animation Interface
    AnimationInterface anim("aodv.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 10.0);
    anim.SetConstantPosition(nodes.Get(1), 20.0, 20.0);
    anim.SetConstantPosition(nodes.Get(2), 30.0, 30.0);
    anim.SetConstantPosition(nodes.Get(3), 40.0, 40.0);
    anim.SetConstantPosition(nodes.Get(4), 50.0, 50.0);

    // Run Simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Print Flow Monitor Statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
        std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";
    }

    Simulator::Destroy();

    return 0;
}