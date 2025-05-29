#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiApStaThroughputExample");

void
PacketTrace(Ptr<const Packet> packet)
{
    std::cout << "Packet captured: " << packet->GetSize() << " bytes at " 
              << Simulator::Now().GetSeconds() << "s" << std::endl;
}

int main(int argc, char *argv[])
{
    uint32_t staCount = 3;
    double simulationTime = 10.0; // seconds

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(staCount);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Create wifi channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");

    // STA
    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // AP
    NetDeviceContainer apDevice;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Set mobility
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

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // Packet capture (pcap)
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("wifiApStaThroughput", apDevice, true);
    phy.EnablePcap("wifiApStaThroughput", staDevices, true);

    // Trace packets (Tx/Rx)
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxEnd",
                                 MakeCallback(&PacketTrace));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxEnd",
                                 MakeCallback(&PacketTrace));

    // Application: UDP traffic flow from STA1 -> STA2 and STA3 -> STA1
    uint16_t port = 9;
    uint32_t packetSize = 1024;
    double startTime = 1.0;
    double stopTime = 9.0;

    // UDP server on STA2
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiStaNodes.Get(1));
    serverApp.Start(Seconds(startTime));
    serverApp.Stop(Seconds(stopTime));

    // UDP client on STA1 sending to STA2
    UdpClientHelper client(staInterfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(32000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.05)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(0));
    clientApp.Start(Seconds(startTime));
    clientApp.Stop(Seconds(stopTime));

    // UDP server on STA1
    UdpServerHelper server2(port);
    ApplicationContainer serverApp2 = server2.Install(wifiStaNodes.Get(0));
    serverApp2.Start(Seconds(startTime));
    serverApp2.Stop(Seconds(stopTime));

    // UDP client on STA3 sending to STA1
    UdpClientHelper client2(staInterfaces.GetAddress(0), port);
    client2.SetAttribute("MaxPackets", UintegerValue(32000));
    client2.SetAttribute("Interval", TimeValue(Seconds(0.05)));
    client2.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp2 = client2.Install(wifiStaNodes.Get(2));
    clientApp2.Start(Seconds(startTime));
    clientApp2.Stop(Seconds(stopTime));

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Throughput and packet loss stats
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalThroughput = 0.0;
    uint64_t totalLost = 0, totalRx = 0, totalTx = 0;

    std::cout << "\nFlow statistics:\n";
    for (auto const& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets:  " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets:  " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets:" << flow.second.lostPackets << "\n";
        std::cout << "  Throughput:  " << (flow.second.rxBytes * 8.0 / (simulationTime - 2.0) / 1000 / 1000) << " Mbps\n";
        totalThroughput += (flow.second.rxBytes * 8.0 / (simulationTime - 2.0));
        totalLost += flow.second.lostPackets;
        totalRx   += flow.second.rxPackets;
        totalTx   += flow.second.txPackets;
    }
    std::cout << "\nAggregate Statistics:\n";
    std::cout << "  Throughput:   " << totalThroughput / 1000 / 1000 << " Mbps\n";
    std::cout << "  Packet Loss:  " << totalLost << "\n";
    std::cout << "  Tx Packets:   " << totalTx << "\n";
    std::cout << "  Rx Packets:   " << totalRx << "\n";
    if (totalTx > 0) {
        std::cout << "  Loss Ratio:   " << (double)totalLost / (double)totalTx * 100.0 << " %\n";
    }

    Simulator::Destroy();
    return 0;
}