#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Simulation parameters
    uint32_t numNodes = 2;
    double simTime = 10.0; // seconds
    uint16_t port = 9;
    uint32_t packetSize = 1024; // bytes
    uint32_t numPackets = 5;
    double interval = 1.0; // seconds

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Configure Wi-Fi (802.11) for ad hoc mode
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());
    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility: RandomWalk2dMobility within bounds
    MobilityHelper mobility;
    mobility.SetMobilityModel (
        "ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue (Rectangle (0.0, 100.0, 0.0, 100.0)),
        "Distance", DoubleValue (10.0),
        "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]")
    );
    mobility.SetPositionAllocator (
        "ns3::GridPositionAllocator",
        "MinX", DoubleValue (10.0),
        "MinY", DoubleValue (10.0),
        "DeltaX", DoubleValue (50.0),
        "DeltaY", DoubleValue (50.0),
        "GridWidth", UintegerValue (2),
        "LayoutType", StringValue ("RowFirst")
    );
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo Server on node 1
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // UDP Echo Client on node 0
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // Enable Flow Monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // Enable animation
    AnimationInterface anim("adhoc2nodes.xml");
    anim.SetMaxPktsPerTraceFile(1000000);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Flow Monitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;
    uint32_t totalLostPackets = 0;
    double totalDelay = 0.0;
    double totalThroughput = 0.0;
    uint32_t flowCount = 0;

    std::cout << "========== Flow Monitor Statistics ==========" << std::endl;
    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);

        std::cout << "Flow ID: " << flow.first
                  << " Src Addr: " << t.sourceAddress
                  << " Dst Addr: " << t.destinationAddress << std::endl;

        std::cout << " Tx Packets: " << flow.second.txPackets
                  << " Rx Packets: " << flow.second.rxPackets
                  << " Lost Packets: " << flow.second.lostPackets << std::endl;

        if (flow.second.rxPackets > 0)
        {
            double throughput = flow.second.rxBytes * 8.0 / (simTime - 2.0) / 1024.0; // kbps
            double e2eDelay = (flow.second.delaySum.GetSeconds() / flow.second.rxPackets);
            double deliveryRatio = (100.0 * flow.second.rxPackets / flow.second.txPackets);

            std::cout << " Delivery Ratio: " << deliveryRatio << "%"
                      << " End-to-End Delay: " << e2eDelay << " s"
                      << " Throughput: " << throughput << " kbps" << std::endl;

            totalThroughput += throughput;
            totalDelay += e2eDelay * flow.second.rxPackets;
            totalRxPackets += flow.second.rxPackets;
            flowCount++;
        }
        totalTxPackets += flow.second.txPackets;
        totalLostPackets += flow.second.lostPackets;

        std::cout << "--------------------------------------------" << std::endl;
    }
    std::cout << "========== Overall Statistics =============" << std::endl;
    double overallPdr = totalTxPackets ? (double)totalRxPackets / totalTxPackets * 100.0 : 0.0;
    double averageDelay = totalRxPackets ? totalDelay / totalRxPackets : 0.0;
    double averageThroughput = flowCount ? totalThroughput / flowCount : 0.0;

    std::cout << " Total Sent Packets: " << totalTxPackets << std::endl;
    std::cout << " Total Received Packets: " << totalRxPackets << std::endl;
    std::cout << " Total Lost Packets: " << totalLostPackets << std::endl;
    std::cout << " Packet Delivery Ratio: " << overallPdr << "%" << std::endl;
    std::cout << " Average End-to-End Delay: " << averageDelay << " s" << std::endl;
    std::cout << " Average Throughput: " << averageThroughput << " kbps" << std::endl;
    std::cout << "===========================================" << std::endl;

    Simulator::Destroy();
    return 0;
}