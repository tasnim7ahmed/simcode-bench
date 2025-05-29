#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Log server and client
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Setup physical and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Wifi 802.11g configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-ssid");

    // STA devices
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // AP device
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Set mobility - STA random mobility in 50x50
    MobilityHelper mobility;
    Ptr<PositionAllocator> positionAlloc = CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                             "Distance", DoubleValue(5.0));
    mobility.Install(wifiStaNodes);

    // AP is stationary
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(25.0),
                                 "MinY", DoubleValue(25.0),
                                 "DeltaX", DoubleValue(0.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(1),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(wifiApNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // UDP Server on AP (port 4000)
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Clients on each STA
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        UdpClientHelper udpClient(apInterface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(10000));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Enable PCAP tracing on the AP
    phy.EnablePcap("wifi-ap", apDevice.Get(0), true, true);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}