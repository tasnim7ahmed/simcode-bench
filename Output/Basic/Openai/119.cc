#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/animation-interface.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numNodes = 6;
    double simTime = 20.0;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Configure WiFi - Ad hoc mode IEEE 802.11
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(30.0),
                                 "DeltaY", DoubleValue(30.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(-50, 100, -50, 100)),
                             "Distance", DoubleValue(10));
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IPv4 addresses
    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP echo servers and clients in a ring
    ApplicationContainer serverApps, clientApps;
    uint16_t portBase = 8000;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        // EchoServer on node i
        UdpEchoServerHelper echoServer(portBase + i);
        ApplicationContainer server = echoServer.Install(nodes.Get(i));
        server.Start(Seconds(0.0));
        server.Stop(Seconds(simTime));
        serverApps.Add(server);

        // EchoClient from node i to node ((i+1) % numNodes)
        uint32_t next = (i + 1) % numNodes;
        UdpEchoClientHelper echoClient(interfaces.GetAddress(next), portBase + next);
        echoClient.SetAttribute("MaxPackets", UintegerValue(20));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
        echoClient.SetAttribute("PacketSize", UintegerValue(64));

        ApplicationContainer client = echoClient.Install(nodes.Get(i));
        client.Start(Seconds(1.0));
        client.Stop(Seconds(simTime));
        clientApps.Add(client);
    }

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Animation
    AnimationInterface anim("adhoc-grid.xml");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Flow statistics
    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalTx = 0, totalRx = 0;
    double sumDelay = 0;
    double sumThroughput = 0;
    uint32_t flows = 0;

    std::cout << "Flow statistics:\n";
    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);

        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << ":" << t.sourcePort
                  << " -> " << t.destinationAddress << ":" << t.destinationPort << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";

        double pdr = flow.second.rxPackets * 100.0 / (double)flow.second.txPackets;
        std::cout << "  Packet Delivery Ratio: " << pdr << " %\n";

        if (flow.second.rxPackets > 0)
        {
            double meanDelay = (flow.second.delaySum.GetSeconds() / flow.second.rxPackets);
            double throughput = (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1024.0;
            std::cout << "  Mean End-to-End Delay: " << meanDelay * 1000 << " ms\n";
            std::cout << "  Throughput: " << throughput << " Kbps\n";
            sumDelay += meanDelay;
            sumThroughput += throughput;
        }
        else
        {
            std::cout << "  Mean End-to-End Delay: 0 ms\n";
            std::cout << "  Throughput: 0 Kbps\n";
        }

        totalTx += flow.second.txPackets;
        totalRx += flow.second.rxPackets;
        flows++;
    }

    if (flows > 0 && totalTx > 0)
    {
        std::cout << "\nAggregate Results:\n";
        std::cout << "  Overall Packet Delivery Ratio: " << (totalRx * 100.0 / totalTx) << " %\n";
        std::cout << "  Average End-to-End Delay: " << (sumDelay * 1000 / flows) << " ms\n";
        std::cout << "  Average Throughput: " << (sumThroughput / flows) << " Kbps\n";
    }

    Simulator::Destroy();
    return 0;
}