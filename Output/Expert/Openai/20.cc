#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/wifi-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/icmpv4-l4-protocol.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set simulation seed for reproducibility
    SeedManager::SetSeed(12345);

    // Configure block ack attributes
    Config::SetDefault("ns3::QosWifiMac::BlockAckThreshold", UintegerValue(2));           // Minimum frames before BlockAck requested
    Config::SetDefault("ns3::QosWifiMac::BlockAckInactivityTimeout", UintegerValue(100)); // Timeout in TUs (default = 100)
    Config::SetDefault("ns3::WifiRemoteStationManager::BlockAckType", StringValue("COMPRESSED"));

    // Create 2 STAs and 1 AP
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Set up Wi-Fi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-80211n-ba");

    // One Qos STA will transmit
    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "QosSupported", BooleanValue(true));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    NetDeviceContainer apDevice;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "QosSupported", BooleanValue(true));
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // STA 1
    positionAlloc->Add(Vector(0.0, 5.0, 0.0));   // STA 2
    positionAlloc->Add(Vector(10.0, 0.0, 0.0));  // AP
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // IP assignment
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces, apInterface;
    staInterfaces = address.Assign(staDevices);
    apInterface = address.Assign(apDevice);

    // Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Traffic: UDP client on STA 0 → AP UDP echo server
    uint16_t udpPort = 4000;

    UdpEchoServerHelper udpServer(udpPort);
    ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper udpClient(apInterface.GetAddress(0), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(40));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(9.0));

    // ICMP application on AP: Reply with ICMP to each UDP packet (simulate using OnOff).
    // We'll use a simple Ping between AP and STA 0.
    V4PingHelper pingHelper(staInterfaces.GetAddress(0));
    pingHelper.SetAttribute("Verbose", BooleanValue(false));
    pingHelper.SetAttribute("Interval", TimeValue(Seconds(0.2)));
    pingHelper.SetAttribute("Size", UintegerValue(64));
    ApplicationContainer pingApp = pingHelper.Install(wifiApNode.Get(0));
    pingApp.Start(Seconds(2.1));
    pingApp.Stop(Seconds(9.0));

    // PCAP tracing on AP
    phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("80211n-blockack-ap", apDevice.Get(0), true);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}