#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/random-walk-2d-mobility-model.h"

#include <iostream>
#include <iomanip> // For std::fixed and std::setprecision

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiSimulation");

int main(int argc, char *argv[])
{
    LogComponentEnable("AdhocWifiSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    double simulationTime = 20.0; // seconds
    uint32_t numNodes = 2;
    uint32_t payloadSize = 1024; // bytes
    uint32_t maxPackets = 10;
    double packetInterval = 1.0; // seconds

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("payloadSize", "Size of UDP payload in bytes", payloadSize);
    cmd.AddValue("maxPackets", "Maximum number of packets for UDP client to send", maxPackets);
    cmd.AddValue("packetInterval", "Time interval between packets for UDP client", packetInterval);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

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
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    mobility.Install(nodes);

    WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211a);

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLoss("ns3::FriisPropagationLossModel");
    wifiChannel.SetDistanceAttenuationModel("ns3::ConstantSpeedPropagationDelayModel");
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Wifi80211a_Adhoc();

    NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(wifiDevices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime - 1.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime - 0.5));

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    NetAnimHelper netAnim;
    netAnim.Set("PlaybackDelay", TimeValue(Seconds(0.1)));
    netAnim.EnableAnimation("adhoc-wifi-animation.xml");

    NS_LOG_INFO("Starting simulation for " << simulationTime << " seconds...");
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    flowMonitor->CheckFor(FlowMonitor::EveryPacket);

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalDelaySum = 0;
    uint64_t totalRxBytes = 0;
    uint32_t totalPacketsLost = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        if (t.sourceAddress == interfaces.GetAddress(1) && t.destinationAddress == interfaces.GetAddress(0)) // Client to Server flow
        {
            NS_LOG_INFO("Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
            NS_LOG_INFO("  Tx Packets: " << i->second.txPackets);
            NS_LOG_INFO("  Rx Packets: " << i->second.rxPackets);
            NS_LOG_INFO("  Lost Packets: " << i->second.lostPackets);
            NS_LOG_INFO("  Tx Bytes: " << i->second.txBytes);
            NS_LOG_INFO("  Rx Bytes: " << i->second.rxBytes);
            NS_LOG_INFO("  Mean Delay: " << (i->second.rxPackets > 0 ? (i->second.delaySum.GetSeconds() / i->second.rxPackets) : 0) << " s");
            NS_LOG_INFO("  Throughput: " << (i->second.timeLastRxPacket.IsPositive() ? (i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000.0) : 0) << " Mbps");

            totalTxPackets += i->second.txPackets;
            totalRxPackets += i->second.rxPackets;
            totalPacketsLost += i->second.lostPackets;
            totalDelaySum += i->second.delaySum.GetSeconds();
            totalRxBytes += i->second.rxBytes;
        }
    }

    double pdr = (totalTxPackets > 0) ? (totalRxPackets * 100.0 / totalTxPackets) : 0;
    double avgEndToEndDelay = (totalRxPackets > 0) ? (totalDelaySum / totalRxPackets) : 0;
    double avgThroughputMbps = (totalRxBytes * 8.0 / simulationTime) / 1000000.0;

    NS_LOG_UNCOND("--- Simulation Results ---");
    NS_LOG_UNCOND("Packet Delivery Ratio: " << std::fixed << std::setprecision(2) << pdr << "%");
    NS_LOG_UNCOND("Average End-to-End Delay: " << std::fixed << std::setprecision(6) << avgEndToEndDelay << " seconds");
    NS_LOG_UNCOND("Average Throughput: " << std::fixed << std::setprecision(2) << avgThroughputMbps << " Mbps");
    NS_LOG_UNCOND("Total Packets Transmitted: " << (uint32_t)totalTxPackets);
    NS_LOG_UNCOND("Total Packets Received: " << (uint32_t)totalRxPackets);
    NS_LOG_UNCOND("Total Packets Lost: " << (uint32_t)totalPacketsLost);

    flowMonitor->SerializeToXmlFile("adhoc-flowmonitor.xml", true, true);

    Simulator::Destroy();
    NS_LOG_INFO("Simulation finished.");

    return 0;
}