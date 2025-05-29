#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Logging (optional: can be commented for cleaner output)
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Simulation parameters
    uint32_t numNodes = 2;
    double simTime = 20.0; // seconds
    std::string phyMode = "DsssRate11Mbps";
    double txPower = 16.0; // dBm
    uint32_t packetSize = 1024; // bytes
    uint32_t numPackets = 20;
    double interval = 1.0; // s
    std::string animFile = "adhoc-wifi.xml";

    NodeContainer nodes;
    nodes.Create(numNodes);

    // Wifi configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue(phyMode),
                                "ControlMode", StringValue(phyMode));

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.Set("TxPowerStart", DoubleValue(txPower));
    wifiPhy.Set("TxPowerEnd", DoubleValue(txPower));
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("TxGain", DoubleValue(0));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-96.0)); // dBm
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-99.0)); // dBm

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility configuration
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
        "Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=10.0]"),
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
        "PositionAllocator", PointerValue(mobility.GetPositionAllocator()));
    mobility.Install(nodes);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // IP address assignment
    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo Applications (bidirectional)
    uint16_t portA = 9;
    uint16_t portB = 10;

    UdpEchoServerHelper echoServerA(portA);
    UdpEchoServerHelper echoServerB(portB);

    ApplicationContainer serverAppsA = echoServerA.Install(nodes.Get(0)); // Server on Node 0
    serverAppsA.Start(Seconds(1.0));
    serverAppsA.Stop(Seconds(simTime));

    ApplicationContainer serverAppsB = echoServerB.Install(nodes.Get(1)); // Server on Node 1
    serverAppsB.Start(Seconds(1.0));
    serverAppsB.Stop(Seconds(simTime));

    UdpEchoClientHelper echoClientA(interfaces.GetAddress(0), portA);
    echoClientA.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClientA.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClientA.SetAttribute("PacketSize", UintegerValue(packetSize));

    UdpEchoClientHelper echoClientB(interfaces.GetAddress(1), portB);
    echoClientB.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClientB.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClientB.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientAppsA = echoClientB.Install(nodes.Get(0)); // Node 0 sends to Node 1
    clientAppsA.Start(Seconds(2.0));
    clientAppsA.Stop(Seconds(simTime - 1));

    ApplicationContainer clientAppsB = echoClientA.Install(nodes.Get(1)); // Node 1 sends to Node 0
    clientAppsB.Start(Seconds(2.0));
    clientAppsB.Stop(Seconds(simTime - 1));

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // NetAnim
    AnimationInterface anim(animFile);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Flow Monitor Statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    uint32_t totalSent = 0;
    uint32_t totalReceived = 0;
    uint32_t totalLost = 0;
    double totalDelay = 0.0;
    uint64_t totalRxBytes = 0;
    double firstTxTime = simTime, lastRxTime = 0;
    uint32_t activeFlows = 0;

    for (auto const& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow ID: " << flow.first << " (" << t.sourceAddress
                  << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        std::cout << "  Tx Bytes: " << flow.second.txBytes << "\n";
        std::cout << "  Rx Bytes: " << flow.second.rxBytes << "\n";
        std::cout << "  Throughput: "
                  << (flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds())) / 1000
                  << " Kbps\n";
        if (flow.second.rxPackets > 0)
        {
            std::cout << "  Mean Delay: "
                      << (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) * 1000 << " ms\n";
        }
        else
        {
            std::cout << "  Mean Delay: 0 ms\n";
        }
        std::cout << std::endl;

        totalSent += flow.second.txPackets;
        totalReceived += flow.second.rxPackets;
        totalLost += flow.second.lostPackets;
        totalDelay += flow.second.delaySum.GetSeconds();
        totalRxBytes += flow.second.rxBytes;
        if (flow.second.timeFirstTxPacket.GetSeconds() < firstTxTime)
            firstTxTime = flow.second.timeFirstTxPacket.GetSeconds();
        if (flow.second.timeLastRxPacket.GetSeconds() > lastRxTime)
            lastRxTime = flow.second.timeLastRxPacket.GetSeconds();
        if (flow.second.rxPackets > 0) activeFlows++;
    }

    double deliveryRatio = (totalSent > 0) ? (double)totalReceived / totalSent : 0.0;
    double lostRatio = (totalSent > 0) ? (double)totalLost / totalSent : 0.0;
    double meanDelay = (totalReceived > 0) ? totalDelay / totalReceived : 0.0;
    double throughput = (lastRxTime > firstTxTime) ?
        (totalRxBytes * 8.0 / (lastRxTime - firstTxTime)) / 1000 : 0.0;

    std::cout << "------- Aggregate Results -------" << std::endl;
    std::cout << "Total Tx Packets: " << totalSent << std::endl;
    std::cout << "Total Rx Packets: " << totalReceived << std::endl;
    std::cout << "Total Lost Packets: " << totalLost << std::endl;
    std::cout << "Packet Delivery Ratio: " << (deliveryRatio * 100) << " %" << std::endl;
    std::cout << "Packet Loss Ratio: " << (lostRatio * 100) << " %" << std::endl;
    std::cout << "Average End-to-End Delay: " << (meanDelay * 1000) << " ms" << std::endl;
    std::cout << "Aggregate Throughput: " << throughput << " Kbps" << std::endl;

    Simulator::Destroy();
    return 0;
}