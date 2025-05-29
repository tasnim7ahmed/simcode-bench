/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiApStaThroughputExample");

void
CalculateThroughput(Ptr<FlowMonitor> monitor)
{
    static uint64_t lastTotalRx = 0;
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    uint64_t totalRx = 0;
    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        totalRx += iter->second.rxBytes;
    }
    double throughput = (totalRx - lastTotalRx) * 8.0 / 1e6; // Mbit/s per second
    std::cout << Simulator::Now().GetSeconds() << "s: Throughput: " << throughput << " Mbit/s" << std::endl;
    lastTotalRx = totalRx;
    Simulator::Schedule(Seconds(1.0), &CalculateThroughput, monitor);
}

int
main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("WifiApStaThroughputExample", LOG_LEVEL_INFO);

    uint32_t nSta = 3;
    double simulationTime = 10.0; // seconds

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nSta);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configure physical and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Wifi MAC and standard
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("ErpOfdmRate54Mbps"));

    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-wifi-ap");

    // Configure STA devices
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Configure AP device
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(5.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Applications: set up UDP traffic from STA1 to STA2, STA2 to STA3, STA3 to STA1

    uint16_t port = 8000;
    ApplicationContainer serverApps, clientApps;

    for (uint32_t i = 0; i < nSta; ++i)
    {
        // Each STA gets a UDP server
        UdpServerHelper server(port);
        serverApps.Add(server.Install(wifiStaNodes.Get(i)));
    }

    // Client on STA0 -> Server on STA1
    UdpClientHelper client1(staInterfaces.GetAddress(1), port);
    client1.SetAttribute("MaxPackets", UintegerValue(320));
    client1.SetAttribute("Interval", TimeValue(MilliSeconds(25)));
    client1.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(client1.Install(wifiStaNodes.Get(0)));

    // Client on STA1 -> Server on STA2
    UdpClientHelper client2(staInterfaces.GetAddress(2), port);
    client2.SetAttribute("MaxPackets", UintegerValue(320));
    client2.SetAttribute("Interval", TimeValue(MilliSeconds(25)));
    client2.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(client2.Install(wifiStaNodes.Get(1)));

    // Client on STA2 -> Server on STA0
    UdpClientHelper client3(staInterfaces.GetAddress(0), port);
    client3.SetAttribute("MaxPackets", UintegerValue(320));
    client3.SetAttribute("Interval", TimeValue(MilliSeconds(25)));
    client3.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(client3.Install(wifiStaNodes.Get(2)));

    serverApps.Start(Seconds(1.0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));
    serverApps.Stop(Seconds(simulationTime + 1.0));

    // Enable pcap tracing
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("wifi-ap", apDevice.Get(0));
    for (uint32_t i = 0; i < nSta; ++i)
    {
        phy.EnablePcap("wifi-sta-" + std::to_string(i), staDevices.Get(i));
    }

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Schedule throughput calculation
    Simulator::Schedule(Seconds(1.0), &CalculateThroughput, monitor);

    // Run simulation
    Simulator::Stop(Seconds(simulationTime + 2.0));
    Simulator::Run();

    // Collect and display statistics
    monitor->CheckForLostPackets();
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalThroughput = 0.0;
    uint64_t totalRxPackets = 0, totalTxPackets = 0, totalLostPackets = 0;

    std::cout << "Flow statistics:\n";
    for (auto const &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = flowmon.GetClassifier()->FindFlow(flow.first);
        std::cout << "FlowID: " << flow.first << "\n";
        std::cout << "Src Addr: " << t.sourceAddress << " ----> Dst Addr: " << t.destinationAddress << "\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        double throughput = flow.second.rxBytes * 8.0 / (simulationTime - 1.0) / 1e6; // Mbit/s
        std::cout << "  Throughput: " << throughput << " Mbit/s\n\n";

        totalThroughput += throughput;
        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalLostPackets += flow.second.lostPackets;
    }

    std::cout << "=== Aggregate Statistics ===\n";
    std::cout << "Aggregate Throughput: " << totalThroughput << " Mbit/s\n";
    std::cout << "Total Tx Packets: " << totalTxPackets << "\n";
    std::cout << "Total Rx Packets: " << totalRxPackets << "\n";
    std::cout << "Total Lost Packets: " << totalLostPackets << "\n";
    if (totalTxPackets > 0)
    {
        double packetLossRate = 100.0 * totalLostPackets / totalTxPackets;
        std::cout << "Packet Loss Rate: " << packetLossRate << " %\n";
    }

    Simulator::Destroy();
    return 0;
}