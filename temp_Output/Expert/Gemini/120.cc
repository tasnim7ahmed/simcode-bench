#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-echo-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.AddValue("tracing", "Enable PCAP tracing", false);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::AodvRoutingProtocol::HelloInterval", TimeValue(Seconds(1)));
    Config::SetDefault("ns3::AodvRoutingProtocol::AllowedHelloLoss", UintegerValue(3));

    NodeContainer nodes;
    nodes.Create(10);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", EnumValue(GridPositionAllocator::ROW_FIRST));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 0, 50, 50)));
    mobility.Install(nodes);

    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(nodes);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(4.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    for (uint32_t i = 1; i < 10; ++i) {
        UdpEchoClientHelper client(interfaces.GetAddress((i + 1) % 10), 9);
        client.SetAttribute("MaxPackets", UintegerValue(5));
        client.SetAttribute("Interval", TimeValue(Seconds(4.0)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientAppsNode = client.Install(nodes.Get(i));
        clientAppsNode.Start(Seconds(2.0 + (i * 0.1)));
        clientAppsNode.Stop(Seconds(20.0));
    }

    if (cmd.LookupByName("tracing")->GetBoolean()) {
        Packet::EnablePcapAll("aodv");
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("aodv.xml");
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        anim.UpdateNodeDescription(nodes.Get(i), "Node " + std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i), 0, 255, 0);
    }

    Simulator::Stop(Seconds(21.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4RoutingProtocol> proto = nodes.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol();
    if (proto) {
        AodvRoutingProtocol* aodvProto = dynamic_cast<AodvRoutingProtocol*>(proto->GetObject<AodvRoutingProtocol>());
        if (aodvProto) {
            aodvProto->PrintRoutingTable();
        }
    }

    FlowMonitor::StatisticsContainer statistics = monitor->GetFlowContainer().GetStatistics();
    double totalThroughput = 0.0;
    for (auto it = statistics.begin(); it != statistics.end(); ++it) {
        double throughput = it->second.txBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000000;
        totalThroughput += throughput;
        std::cout << "Flow ID: " << it->first << std::endl;
        std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
        std::cout << "  Packet Loss Ratio: " << (double)(it->second.lostPackets) / it->second.txPackets << std::endl;
        std::cout << "  Delay Sum: " << it->second.delaySum << std::endl;
        std::cout << "  Throughput: " << throughput << " Mbps" << std::endl;
    }
    std::cout << "Total Throughput: " << totalThroughput << " Mbps" << std::endl;
    monitor->SerializeToXmlFile("aodv.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}