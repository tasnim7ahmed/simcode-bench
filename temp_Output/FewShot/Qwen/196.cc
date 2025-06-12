#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging if needed (optional)
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 3 nodes: one AP and two STAs
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode = NodeContainer(wifiStaNodes.Get(0));
    wifiStaNodes.Remove(wifiApNode);

    NodeContainer allNodes = NodeContainer(wifiApNode, wifiStaNodes);

    // Create channel and configure PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Setup MAC and install WiFi devices
    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);

    // Set remote station manager
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    // Install AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(Ssid("ns-3-ssid")),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Install STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(Ssid("ns-3-ssid")),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Set up UDP traffic from stations to AP
    uint16_t port = 9; // Discard port

    // Server on AP
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Clients on STAs
    Time interPacketInterval = Seconds(0.1);
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 50;

    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
        echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
        echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer clientApp = echoClient.Install(wifiStaNodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Enable pcap tracing
    phy.EnablePcap("wifi_ap", apDevice.Get(0));
    phy.EnablePcap("wifi_sta0", staDevices.Get(0));
    phy.EnablePcap("wifi_sta1", staDevices.Get(1));

    // Setup packet counters and throughput calculation
    Ptr<PacketSink> sinkAp;
    {
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sinkHelper.Install(wifiApNode.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));
        sinkAp = DynamicCast<PacketSink>(sinkApp.Get(0));
    }

    std::vector<Ptr<PacketSink>> sinksSta;
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + 1000 + i));
        ApplicationContainer sinkApp = sinkHelper.Install(wifiStaNodes.Get(i));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));
        sinksSta.push_back(DynamicCast<PacketSink>(sinkApp.Get(0)));
    }

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Output statistics
    std::cout << "=== Throughput Statistics ===" << std::endl;

    double totalTxPackets = 0;
    double totalRxBytes = 0;

    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        uint64_t txPackets = DynamicCast<UdpEchoClient>(clientApp.Get(i))->GetSent();
        uint64_t rxBytes = sinksSta[i]->GetTotalRx();
        double throughput = (rxBytes * 8) / (10.0 * 1000000); // Mbps
        std::cout << "STA " << i << ": TX packets=" << txPackets
                  << ", RX bytes=" << rxBytes
                  << ", Throughput=" << throughput << " Mbps" << std::endl;
        totalTxPackets += txPackets;
        totalRxBytes += rxBytes;
    }

    uint64_t apRxBytes = sinkAp->GetTotalRx();
    double totalThroughput = (totalRxBytes * 8) / (10.0 * 1000000); // Mbps
    std::cout << "AP: Total RX bytes=" << apRxBytes << std::endl;
    std::cout << "Total Throughput: " << totalThroughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}