#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for the UDP server and client applications if needed.
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create nodes: one STA and one AP
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configure Wi-Fi channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);

    WifiMacHelper mac;

    // SSID
    Ssid ssid = Ssid("ns3-80211n");

    // Install STA device
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    // Install AP device
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // STA position
    positionAlloc->Add(Vector(5.0, 0.0, 0.0)); // AP position
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    // IP Addressing
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface, apInterface;
    staInterface = address.Assign(staDevice);
    apInterface = address.Assign(apDevice);

    // UDP Server (AP)
    uint16_t udpServerPort = 9;
    UdpServerHelper udpServer(udpServerPort);
    ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client (STA)
    uint32_t packetSize = 1024;
    uint32_t numPackets = 50;
    Time interPacketInterval = MilliSeconds(200);

    UdpClientHelper udpClient(apInterface.GetAddress(0), udpServerPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    udpClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = udpClient.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Enable pcap tracing
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("wifi-udp-ap", apDevice.Get(0));
    phy.EnablePcap("wifi-udp-sta", staDevice.Get(0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}