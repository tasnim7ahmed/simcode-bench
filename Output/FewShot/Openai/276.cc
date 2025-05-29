#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for UDP applications
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create nodes: 3 STAs + 1 AP
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Set up Wi-Fi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set Wi-Fi standard to 802.11n 5GHz
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    WifiMacHelper mac;

    // Configure STA devices
    Ssid ssid = Ssid("ns3-80211n-5ghz");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Configure AP device
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Set up mobility (AP static)
    MobilityHelper mobilityAp;
    Ptr<ListPositionAllocator> apPosition = CreateObject<ListPositionAllocator>();
    apPosition->Add(Vector(0.0, 0.0, 0.0));
    mobilityAp.SetPositionAllocator(apPosition);
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(wifiApNode);

    // STAs in a grid layout
    MobilityHelper mobilitySta;
    mobilitySta.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(5.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(5.0),
        "DeltaY", DoubleValue(5.0),
        "GridWidth", UintegerValue(2),
        "LayoutType", StringValue("RowFirst"));
    mobilitySta.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilitySta.Install(wifiStaNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Set up UDP server on AP
    uint16_t serverPort = 9999;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Each STA sends 5 packets of 512 bytes, 1/s, to AP
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        UdpClientHelper udpClient(apInterface.GetAddress(0), serverPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(5));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}