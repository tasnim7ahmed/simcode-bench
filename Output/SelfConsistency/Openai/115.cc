/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Network topology:
 *
 *   Node 0 <-----> Node 1 (Ad Hoc Wireless, IEEE 802.11)
 *
 * Node 0: UDP Echo Server
 * Node 1: UDP Echo Client
 *
 * Both nodes move within bounds using RandomWalk2dMobilityModel.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiUdpEcho");

int main(int argc, char *argv[])
{
    // Simulation parameters
    double simTime = 20.0; // seconds
    uint32_t packetSize = 1024; // bytes
    uint32_t numPackets = 50;
    double interval = 0.5; // seconds between packets

    // Set logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure Wi-Fi (802.11) in ad hoc mode
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices;
    devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Set mobility: RandomWalk2dMobilityModel within bounds
    MobilityHelper mobility;
    mobility.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)),
        "Distance", DoubleValue(10.0),
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
        "Direction", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=6.283185307]")
    );
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(10.0),
                                 "MinY", DoubleValue(10.0),
                                 "DeltaX", DoubleValue(80.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst")
    );
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IPv4 addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP Echo Server (Node 0)
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // UDP Echo Client (Node 1 -> Node 0)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // Enable Flow Monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // Enable animation
    AnimationInterface anim("adhoc-wifi-udp-echo.xml");
    anim.SetConstantPosition(nodes.Get(0), 20.0, 50.0);
    anim.SetConstantPosition(nodes.Get(1), 80.0, 50.0);

    // Run simulation
    Simulator::Stop(Seconds(simTime + 1));
    Simulator::Run();

    // Flow Monitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalTxBytes = 0;
    double totalRxBytes = 0;
    double totalDelay = 0.0;
    double totalDuration = 0.0;

    for (const auto& flowPair : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowPair.first);
        NS_LOG_UNCOND("Flow ID: " << flowPair.first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress);
        NS_LOG_UNCOND("Tx Packets = " << flowPair.second.txPackets);
        NS_LOG_UNCOND("Rx Packets = " << flowPair.second.rxPackets);
        NS_LOG_UNCOND("Lost Packets = " << flowPair.second.lostPackets);
        NS_LOG_UNCOND("Throughput: " << (flowPair.second.rxBytes * 8.0 / (flowPair.second.timeLastRxPacket.GetSeconds() - flowPair.second.timeFirstTxPacket.GetSeconds()) / 1e3) << " Kbps");
        if (flowPair.second.rxPackets > 0)
        {
            NS_LOG_UNCOND("Average End-to-End Delay: " << (flowPair.second.delaySum.GetSeconds() / flowPair.second.rxPackets) << " s");
        }
        NS_LOG_UNCOND("-----------------------------------------------------");

        totalTxPackets += flowPair.second.txPackets;
        totalRxPackets += flowPair.second.rxPackets;
        totalTxBytes += flowPair.second.txBytes;
        totalRxBytes += flowPair.second.rxBytes;
        totalDelay += flowPair.second.delaySum.GetSeconds();
        if (flowPair.second.timeLastRxPacket > flowPair.second.timeFirstTxPacket)
            totalDuration += (flowPair.second.timeLastRxPacket - flowPair.second.timeFirstTxPacket).GetSeconds();
    }

    double pdr = (totalRxPackets / totalTxPackets) * 100.0;
    double avgDelay = (totalRxPackets > 0) ? (totalDelay / totalRxPackets) : 0;
    double avgThroughput = (totalDuration > 0) ? (totalRxBytes * 8.0 / totalDuration / 1e3) : 0;

    std::cout << std::endl;
    std::cout << "================ Simulation Results ================" << std::endl;
    std::cout << "Total Tx Packets        : " << totalTxPackets << std::endl;
    std::cout << "Total Rx Packets        : " << totalRxPackets << std::endl;
    std::cout << "Packet Delivery Ratio   : " << pdr << " %" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;
    std::cout << "Average Throughput      : " << avgThroughput << " Kbps" << std::endl;
    std::cout << "===================================================" << std::endl;

    Simulator::Destroy();
    return 0;
}