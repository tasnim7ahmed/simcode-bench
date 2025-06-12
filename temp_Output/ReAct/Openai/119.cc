#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Simulation parameters
    uint32_t numNodes = 6;
    double simTime = 15.0;
    double nodeStep = 50.0; // meters
    double areaWidth = 200.0, areaHeight = 150.0;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Configure wireless devices: IEEE 802.11 ad hoc
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility: Grid layout with RandomWalk2d in specified bounds
    MobilityHelper mobility;
    mobility.SetPositionAllocator (
        "ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(nodeStep),
        "DeltaY", DoubleValue(nodeStep),
        "GridWidth", UintegerValue(3),
        "LayoutType", StringValue("RowFirst")
    );
    mobility.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(0.0, areaWidth, 0.0, areaHeight)),
        "Distance", DoubleValue(20.0),
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]")
    );
    mobility.Install(nodes);

    // Internet stack and IP addressing
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces = ipv4.Assign(devices);

    // UDP Echo server: start on every node
    uint16_t echoPort = 9000;
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        UdpEchoServerHelper echoServer(echoPort);
        ApplicationContainer sa = echoServer.Install(nodes.Get(i));
        sa.Start(Seconds(1.0));
        sa.Stop(Seconds(simTime));
        serverApps.Add(sa);
    }

    // UDP Echo client: ring, each node sends to next
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        uint32_t next = (i+1)%numNodes;
        UdpEchoClientHelper echoClient(ifaces.GetAddress(next), echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(20));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
        echoClient.SetAttribute("PacketSize", UintegerValue(64));
        ApplicationContainer ca = echoClient.Install(nodes.Get(i));
        ca.Start(Seconds(2.0));
        ca.Stop(Seconds(simTime));
        clientApps.Add(ca);
    }

    // Enable Flow Monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    // NetAnim output
    AnimationInterface anim("adhoc_ring.anim.xml");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Collect statistics
    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;
    uint64_t totalTxBytes = 0;
    uint64_t totalRxBytes = 0;
    double avgDelay = 0.0;
    uint32_t flowCount = 0;

    std::cout << "Flow statistics:\n";
    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow ID: " << flow.first << "   "
                  << t.sourceAddress << " --> " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << flow.second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << flow.second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << flow.second.lostPackets << std::endl;
        std::cout << "  Throughput: ";
        if (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds() > 0)
        {
            double throughput = (flow.second.rxBytes * 8.0) /
                (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1024.0;
            std::cout << throughput << " Kbps" << std::endl;
        }
        else
        {
            std::cout << "N/A" << std::endl;
        }
        double delay = (flow.second.rxPackets > 0) ? (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) : 0.0;
        std::cout << "  Avg. End-to-End Delay: " << delay*1000 << " ms" << std::endl;

        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalTxBytes += flow.second.txBytes;
        totalRxBytes += flow.second.rxBytes;
        avgDelay += delay * flow.second.rxPackets;
        flowCount++;
    }

    double pdr = (totalTxPackets > 0) ? ((double)totalRxPackets / totalTxPackets) * 100.0 : 0.0;
    double avgEndToEndDelay = (totalRxPackets > 0) ? (avgDelay / totalRxPackets) : 0.0;
    double throughput = (Simulator::Now().GetSeconds() > 0) ? (totalRxBytes * 8.0 / Simulator::Now().GetSeconds() / 1024.0) : 0.0;

    std::cout << "\n=== Overall Statistics ===" << std::endl;
    std::cout << "Packet Delivery Ratio (PDR): " << pdr << " %" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgEndToEndDelay * 1000 << " ms" << std::endl;
    std::cout << "Throughput: " << throughput << " Kbps" << std::endl;

    Simulator::Destroy();
    return 0;
}