#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: STA and AP
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Set up Wi-Fi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure 802.11n (2.4GHz)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211n");

    // Configure STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    // Configure AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility: Position STA and AP
    MobilityHelper mobility;

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // STA
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));   // AP

    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    // Internet stack and IP assignment
    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer staInterface;
    Ipv4InterfaceContainer apInterface;

    staInterface = address.Assign(staDevice);
    address.NewNetwork(); // Not strictly necessary, as only one net
    apInterface = address.Assign(apDevice);

    // Set up UDP server on AP
    uint16_t udpServerPort = 5000;
    UdpServerHelper udpServer(udpServerPort);
    ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on STA
    uint32_t maxPacketCount = 50;
    Time interPacketInterval = Seconds(0.2); // 200 ms
    uint32_t packetSize = 1024;

    UdpClientHelper udpClient(apInterface.GetAddress(0), udpServerPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    udpClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = udpClient.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(1.0)); // Start after wifi association
    clientApp.Stop(Seconds(10.0));

    // Enable pcap tracing if desired
    // phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    // phy.EnablePcap("wifi-udp", apDevice.Get(0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}