#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

int main (int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse (argc, argv);

    uint32_t numNodes = 4;
    double simTime = 15.0; // seconds

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Setup WiFi (802.11b) in ad hoc mode
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, wifiMac, nodes);

    // Configure Mobility: Grid + Random movement within a rectangle
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(50.0),
                                 "DeltaY", DoubleValue(50.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                             "Distance", DoubleValue(10.0),
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=3.0]"),
                             "Time", TimeValue(Seconds(2.0)));

    mobility.Install(nodes);

    // Install Internet stack and assign IP addresses
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9999;
    ApplicationContainer serverApps, clientApps;

    // Install UDP Echo Server on each node
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApp = echoServer.Install(nodes.Get(i));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(simTime));
        serverApps.Add(serverApp);
    }

    // Each node sends UDP Echo requests to the next node in ring topology
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        uint32_t next = (i + 1) % numNodes;
        UdpEchoClientHelper echoClient(interfaces.GetAddress(next), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(20));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
        echoClient.SetAttribute("PacketSize", UintegerValue(64));
        ApplicationContainer clientApp = echoClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(simTime));
        clientApps.Add(clientApp);
    }

    // Install FlowMonitor
    Ptr<FlowMonitor> flowmon;
    FlowMonitorHelper flowmonHelper;
    flowmon = flowmonHelper.InstallAll();

    // Enable Animation
    AnimationInterface anim ("adhoc-udp-echo.xml");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Output flow statistics and performance metrics
    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;
    uint32_t totalLostPackets = 0;
    double totalDelay = 0.0;
    double totalRxBytes = 0.0;
    double totalThroughput = 0.0;
    uint32_t flowCount = 0;

    std::cout << "Flow statistics:\n";
    for (const auto& it : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it.first);
        std::cout << "Flow ID: " << it.first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << it.second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << it.second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << it.second.lostPackets << std::endl;
        if (it.second.rxPackets > 0)
        {
            double throughput = it.second.rxBytes * 8.0 / (it.second.timeLastRxPacket.GetSeconds() - it.second.timeFirstTxPacket.GetSeconds()) / 1024;
            double delay = it.second.delaySum.GetSeconds() / it.second.rxPackets;
            std::cout << "  Throughput: " << throughput << " Kbps" << std::endl;
            std::cout << "  Mean Delay: " << delay << " s" << std::endl;
            totalDelay += it.second.delaySum.GetSeconds();
            totalRxBytes += it.second.rxBytes;
            totalThroughput += throughput;
        }
        flowCount++;
        totalTxPackets += it.second.txPackets;
        totalRxPackets += it.second.rxPackets;
        totalLostPackets += it.second.lostPackets;
    }

    double pdr = (totalRxPackets * 100.0) / (totalTxPackets > 0 ? totalTxPackets : 1);
    double avgDelay = (totalRxPackets > 0) ? totalDelay / totalRxPackets : 0.0;
    double avgThroughput = (flowCount > 0) ? totalThroughput / flowCount : 0.0;

    std::cout << "\nSummary Performance Metrics:" << std::endl;
    std::cout << "  Packet Delivery Ratio: " << pdr << " %" << std::endl;
    std::cout << "  Average End-to-End Delay: " << avgDelay << " s" << std::endl;
    std::cout << "  Average Throughput: " << avgThroughput << " Kbps" << std::endl;

    Simulator::Destroy();
    return 0;
}