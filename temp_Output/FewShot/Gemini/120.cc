#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool enablePcapTracing = true;
    bool printRoutingTables = false;
    double routingTablePrintInterval = 5.0;

    CommandLine cmd;
    cmd.AddValue("enablePcapTracing", "Enable PCAP tracing", enablePcapTracing);
    cmd.AddValue("printRoutingTables", "Print routing tables periodically", printRoutingTables);
    cmd.AddValue("routingTablePrintInterval", "Routing table print interval (seconds)", routingTablePrintInterval);
    cmd.Parse(argc, argv);

    uint32_t numNodes = 10;
    double simulationTime = 60.0;
    double areaSize = 100.0;

    NodeContainer nodes;
    nodes.Create(numNodes);

    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, areaSize, 0, areaSize)));
    mobility.Install(nodes);

    // UDP Echo Server on each node
    uint16_t port = 9;
    ApplicationContainer servers;
    for (uint32_t i = 0; i < numNodes; ++i) {
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer server = echoServer.Install(nodes.Get(i));
        server.Start(Seconds(1.0));
        server.Stop(Seconds(simulationTime));
        servers.Add(server);
    }

    // UDP Echo Client sending to the next node in a circular manner
    ApplicationContainer clients;
    for (uint32_t i = 0; i < numNodes; ++i) {
        Ipv4Address destAddr = interfaces.GetAddress((i + 1) % numNodes);
        UdpEchoClientHelper echoClient(destAddr, port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(4.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer client = echoClient.Install(nodes.Get(i));
        client.Start(Seconds(2.0));
        client.Stop(Seconds(simulationTime));
        clients.Add(client);
    }

    if (enablePcapTracing) {
        PointToPointHelper p2p;
        p2p.EnablePcapAll("aodv_adhoc");
    }

    if (printRoutingTables) {
        Simulator::Schedule(Seconds(routingTablePrintInterval), &Ipv4::PrintRoutingTableAllAt, Seconds(routingTablePrintInterval));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("aodv_adhoc_animation.xml");

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

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

    Simulator::Destroy();

    return 0;
}