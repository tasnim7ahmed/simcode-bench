#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Log for TCP apps
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // -----------------------------
    // 1. Create Nodes
    // Wired nodes: 0,1,2,3 (node 1 is the GW)
    NodeContainer wiredNodes;
    wiredNodes.Create(4);

    // Wireless nodes: 4,5,6
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);

    // Gateway node
    Ptr<Node> wifiApNode = wiredNodes.Get(1);

    // -----------------------------
    // 2. Point-to-Point Chain Topology (wiredNodes: 0-1-2-3)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("20Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices01 = p2p.Install(wiredNodes.Get(0), wiredNodes.Get(1));
    NetDeviceContainer p2pDevices12 = p2p.Install(wiredNodes.Get(1), wiredNodes.Get(2));
    NetDeviceContainer p2pDevices23 = p2p.Install(wiredNodes.Get(2), wiredNodes.Get(3));

    // -----------------------------
    // 3. Wi-Fi network setup (AP = wiredNodes[1])
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-hybrid-wifi");

    // Station MACs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // AP MAC
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // -----------------------------
    // 4. Mobility for WiFi nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(10.0),
                                 "DeltaY", DoubleValue(10.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // -----------------------------
    // 5. Install the Internet stack
    InternetStackHelper internet;
    internet.Install(wiredNodes);
    internet.Install(wifiStaNodes);

    // -----------------------------
    // 6. Assign IP addresses
    Ipv4AddressHelper address;

    // P2P links
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(p2pDevices01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address.Assign(p2pDevices12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces23 = address.Assign(p2pDevices23);

    // WiFi network (AP + STAs)
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface  = address.Assign(apDevice);

    // -----------------------------
    // 7. Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // -----------------------------
    // 8. TCP Applications
    // All wireless nodes send TCP BulkSend to random wired node (except GW, e.g., nodes 0,2,3)
    uint16_t sinkPort = 50000;
    ApplicationContainer sinkApps;
    ApplicationContainer sourceApps;

    // Create a TCP sink on each wired node (excluding GW, e.g., 0,2,3)
    std::vector<Ipv4Address> sinkAddresses;
    sinkAddresses.push_back(interfaces01.GetAddress(0)); // node 0
    sinkAddresses.push_back(interfaces12.GetAddress(1)); // node 2
    sinkAddresses.push_back(interfaces23.GetAddress(1)); // node 3

    for (uint32_t i = 0; i < sinkAddresses.size(); ++i)
    {
        Address sinkAddress(InetSocketAddress(sinkAddresses[i], sinkPort));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
        sinkApps.Add(packetSinkHelper.Install(wiredNodes.Get(i == 0 ? 0 : (i == 1 ? 2 : 3))));
    }
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(20.0));

    // Each wireless node sends to one sink (round-robin assignment)
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        Address sinkAddress(InetSocketAddress(sinkAddresses[i % sinkAddresses.size()], sinkPort));
        BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
        source.SetAttribute("MaxBytes", UintegerValue(0));
        source.SetAttribute("SendSize", UintegerValue(1024));
        sourceApps.Add(source.Install(wifiStaNodes.Get(i)));
    }
    sourceApps.Start(Seconds(2.0));
    sourceApps.Stop(Seconds(20.0));

    // -----------------------------
    // 9. Enable pcap tracing for all NetDevices
    p2p.EnablePcapAll("hybrid-p2p");
    phy.EnablePcap("hybrid-wifi-ap", apDevice.Get(0));
    for (uint32_t i = 0; i < staDevices.GetN(); ++i)
        phy.EnablePcap("hybrid-wifi-sta", staDevices.Get(i));

    // -----------------------------
    // 10. Install FlowMonitor
    Ptr<FlowMonitor> flowmon;
    FlowMonitorHelper flowmonHelper;
    flowmon = flowmonHelper.InstallAll();

    // -----------------------------
    // 11. Run simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // -----------------------------
    // 12. Average throughput calculation
    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = flowmon->GetFlowStats();

    double totalBytes = 0;
    double minStartTime = 1e9;
    double maxEndTime = 0;

    for (const auto& flowPair : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowPair.first);
        if (t.destinationPort == sinkPort)
        {
            totalBytes += flowPair.second.rxBytes;
            if (flowPair.second.timeFirstRxPacket.GetSeconds() < minStartTime)
            {
                minStartTime = flowPair.second.timeFirstRxPacket.GetSeconds();
            }
            if (flowPair.second.timeLastRxPacket.GetSeconds() > maxEndTime)
            {
                maxEndTime = flowPair.second.timeLastRxPacket.GetSeconds();
            }
        }
    }

    double throughputMbps = 0.0;
    if (maxEndTime > minStartTime)
        throughputMbps = (totalBytes * 8.0 / 1e6) / (maxEndTime - minStartTime);

    std::cout << "\n==== TCP Flow Statistics ====" << std::endl;
    std::cout << "Total Bytes Received: " << totalBytes << std::endl;
    std::cout << "Average Throughput: " << throughputMbps << " Mbps" << std::endl;
    std::cout << "Measurement Interval: " << minStartTime << "s - " << maxEndTime << "s" << std::endl;

    Simulator::Destroy();
    return 0;
}