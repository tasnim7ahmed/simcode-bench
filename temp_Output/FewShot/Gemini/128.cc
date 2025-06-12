#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Enable logging for OSPF and other relevant components
    LogComponentEnable("OlsrProtocol", LOG_LEVEL_ALL);
    LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_ALL);
    LogComponentEnable("Packet::Header", LOG_LEVEL_ALL);
    LogComponentEnable("Packet::Payload", LOG_LEVEL_ALL);

    // Create nodes (4 routers)
    NodeContainer routers;
    routers.Create(4);

    // Configure point-to-point links between routers
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer routerDevices[4];
    routerDevices[0] = pointToPoint.Install(routers.Get(0), routers.Get(1));
    routerDevices[1] = pointToPoint.Install(routers.Get(1), routers.Get(2));
    routerDevices[2] = pointToPoint.Install(routers.Get(2), routers.Get(3));
    routerDevices[3] = pointToPoint.Install(routers.Get(0), routers.Get(3));

    // Install Internet stack on all nodes
    InternetStackHelper internet;
    OlsrHelper olsr;
    internet.SetRoutingHelper(olsr);
    internet.Install(routers);

    // Assign IP addresses to the interfaces
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer routerInterfaces[4];
    address.SetBase("10.1.1.0", "255.255.255.0");
    routerInterfaces[0] = address.Assign(routerDevices[0]);
    address.SetBase("10.1.2.0", "255.255.255.0");
    routerInterfaces[1] = address.Assign(routerDevices[1]);
    address.SetBase("10.1.3.0", "255.255.255.0");
    routerInterfaces[2] = address.Assign(routerDevices[2]);
    address.SetBase("10.1.4.0", "255.255.255.0");
    routerInterfaces[3] = address.Assign(routerDevices[3]);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create traffic: node 0 sends packets to node 3
    uint16_t port = 9;
    UdpEchoClientHelper echoClient(routerInterfaces[2].GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(routers.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(12.0));

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(routers.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(13.0));

    // Add pcap tracing to links to observe LSA packets
    pointToPoint.EnablePcapAll("linkstate", false);

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(15.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto i = stats.begin(); i != stats.end(); ++i) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    }

    Simulator::Destroy();

    return 0;
}