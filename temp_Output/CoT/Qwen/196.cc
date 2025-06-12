#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 1 AP + 2 STAs
    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    // Create channel and setup PHY & MAC
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;

    // Configure the AP
    mac.SetType("ns3::ApWifiMac");
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Configure the STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(Ssid("wifi-network")));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Setup mobility model (constant position for AP, random positions for STAs)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(1.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    // Setup UDP Echo server on the AP
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(wifiApNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP clients on the stations
    UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1 = echoClient.Install(wifiStaNodes.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(10.0));

    ApplicationContainer clientApps2 = echoClient.Install(wifiStaNodes.Get(1));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(10.0));

    // Enable pcap tracing
    phy.EnablePcap("wifi_ap", apDevice.Get(0));
    phy.EnablePcap("wifi_sta1", staDevices.Get(0));
    phy.EnablePcap("wifi_sta2", staDevices.Get(1));

    // Packet counters and stats
    Ptr<OutputStreamWrapper> packetStats = CreateFileStream("packet-stats.csv");
    *packetStats->GetStream() << "Station, PacketsSent, BytesReceived, Throughput" << std::endl;

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/Rx", MakeBoundCallback(&[](
        std::string context,
        Ptr<const Packet> packet) {
        static std::map<uint32_t, uint64_t> bytesReceived;
        static std::map<uint32_t, uint32_t> packetCount;
        static Time startTime = Seconds(0);
        static Time stopTime = Seconds(10.0);

        uint32_t nodeId;
        if (sscanf(context.c_str(), "/NodeList/%u/ApplicationList", &nodeId) == 1) {
            bytesReceived[nodeId] += packet->GetSize();
            packetCount[nodeId] += 1;
        }

        if (Simulator::Now() >= stopTime - Seconds(0.1)) {
            double duration = (stopTime - startTime).GetSeconds();
            double throughput = (bytesReceived[nodeId] * 8) / duration / 1e6; // Mbps
            *packetStats->GetStream() << "STA-" << nodeId << ", " << packetCount[nodeId]
                                      << ", " << bytesReceived[nodeId] << ", "
                                      << throughput << std::endl;
        }
    }));

    // Schedule simulation stop
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}