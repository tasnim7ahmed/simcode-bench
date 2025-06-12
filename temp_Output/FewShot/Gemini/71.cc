#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3); // A, B, C

    NodeContainer A = NodeContainer(nodes.Get(0));
    NodeContainer B = NodeContainer(nodes.Get(1));
    NodeContainer C = NodeContainer(nodes.Get(2));

    // Point-to-point links A-B and B-C
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB = pointToPoint.Install(A, B);
    NetDeviceContainer devicesBC = pointToPoint.Install(B, C);

    // CSMA devices for A and C
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));

    NetDeviceContainer devicesA_csma = csma.Install(A);
    NetDeviceContainer devicesC_csma = csma.Install(C);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    // A-B subnet
    address.SetBase("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

    // B-C subnet
    address.SetBase("10.1.2.0", "255.255.255.252");
    Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

    // A csma
    address.SetBase("172.16.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesA_csma = address.Assign(devicesA_csma);

    // C csma
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesC_csma = address.Assign(devicesC_csma);

    Ipv4Address a_addr = interfacesA_csma.GetAddress(0);
    Ipv4Address c_addr = interfacesC_csma.GetAddress(0);

    // Set /32 addresses for A and C
    interfacesA_csma.GetAddress(0);
    interfacesC_csma.GetAddress(0);
    Ipv4InterfaceContainer ifacesA;
    ifacesA.Add(interfacesA_csma.Get(0));

    Ipv4InterfaceContainer ifacesC;
    ifacesC.Add(interfacesC_csma.Get(0));


    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create UDP application: sender on A, sink on C
    uint16_t port = 9;
    UdpClientHelper client(c_addr, port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(A);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    UdpServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(C);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Enable Tracing
    pointToPoint.EnablePcapAll("router");
    csma.EnablePcapAll("router");

    // AnimationInterface anim("routing-animation.xml");
    // anim.SetConstantPosition(nodes.Get(0), 1, 1);
    // anim.SetConstantPosition(nodes.Get(1), 5, 1);
    // anim.SetConstantPosition(nodes.Get(2), 9, 1);

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.Seconds() - i->second.timeFirstTxPacket.Seconds()) / 1024 / 1024 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}