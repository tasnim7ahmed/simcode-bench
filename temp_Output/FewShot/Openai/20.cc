#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging (optional, e.g., for UDP, ICMP, etc.)
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv4L4Protocol", LOG_LEVEL_INFO);

    // Create nodes: 2 stations and 1 AP
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configure channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi with 802.11n and enable Compressed Block Ack
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);

    // MAC layer helpers
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211n-blockack");

    // Install devices on stations (QoS = true)
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "QosSupported", BooleanValue(true));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Install device on AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "QosSupported", BooleanValue(true));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Enable Compressed Block Ack on all devices, set threshold and timeout
    // These are advanced attributes of the WifiMac
    for (uint32_t i = 0; i < staDevices.GetN(); ++i) {
        Ptr<NetDevice> dev = staDevices.Get(i);
        Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(dev);
        Ptr<WifiMac> mac = wifiDev->GetMac();
        mac->SetAttribute("BE_MaxAmpduSize", UintegerValue(65535)); // Enable A-MPDU
        mac->SetAttribute("BE_MaxAmsduSize", UintegerValue(7935));  // Enable A-MSDU
        mac->SetAttribute("BlockAckThreshold", UintegerValue(2)); // Request BA after 2 MPDUs
        mac->SetAttribute("BlockAckInactivityTimeout", UintegerValue(30)); // 30 TUs
    }
    Ptr<NetDevice> apDev = apDevice.Get(0);
    Ptr<WifiNetDevice> wifiApDev = DynamicCast<WifiNetDevice>(apDev);
    Ptr<WifiMac> apMac = wifiApDev->GetMac();
    apMac->SetAttribute("BE_MaxAmpduSize", UintegerValue(65535));
    apMac->SetAttribute("BE_MaxAmsduSize", UintegerValue(7935));
    apMac->SetAttribute("BlockAckThreshold", UintegerValue(2));
    apMac->SetAttribute("BlockAckInactivityTimeout", UintegerValue(30));

    // Set up mobility for all nodes
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));    // Sta 0
    positionAlloc->Add(Vector(1.0, 0.0, 0.0));    // Sta 1
    positionAlloc->Add(Vector(0.5, 5.0, 0.0));    // AP
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Install Internet stack and routing
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install UDP application: station 0 --> AP
    uint16_t udpPort = 4000;
    uint32_t packetSize = 1024;
    uint32_t maxPackets = 20;
    double pktInterval = 0.02; // 20 ms
    UdpClientHelper udpClient(apInterface.GetAddress(0), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(pktInterval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = udpClient.Install(wifiStaNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(5.0));

    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(wifiApNode.Get(0));
    serverApps.Start(Seconds(0.5));
    serverApps.Stop(Seconds(5.5));

    // Install ICMP echo (ping) server on AP and client on Sta 0, triggered after UDP traffic
    uint16_t icmpPort = 5000;
    V4PingHelper ping(apInterface.GetAddress(0));
    ping.SetAttribute("Verbose", BooleanValue(true));
    ping.SetAttribute("PacketSize", UintegerValue(64));
    ping.SetAttribute("Interval", TimeValue(Seconds(0.2)));
    ping.SetAttribute("Count", UintegerValue(5));
    ApplicationContainer pingApps = ping.Install(wifiStaNodes.Get(0));
    pingApps.Start(Seconds(5.1));
    pingApps.Stop(Seconds(7.0));

    // Enable pcap tracing on the AP device
    phy.EnablePcap("blockack-ap", apDevice.Get(0), true);

    // Run the simulation
    Simulator::Stop(Seconds(8.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}