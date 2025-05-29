#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Logging for debugging (optional)
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create three nodes: two STAs and one AP
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Wi-Fi Physical layer and Channel configuration
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Wi-Fi Helper
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    // Set up MAC for station
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Set up MAC for access point
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility: AP is stationary at (5,5,0)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(5.0),
                                 "MinY", DoubleValue(5.0),
                                 "DeltaX", DoubleValue(0.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(1),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    // STA: Randomly move within (0,0)-(10,10)
    MobilityHelper staMobility;
    staMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                     "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"),
                                     "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"));
    staMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue(Rectangle(0, 10, 0, 10)),
                                 "Distance", DoubleValue(2.0),
                                 "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    staMobility.Install(wifiStaNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    // Assign IPv4 addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // UDP server on STA 0 (port 9)
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiStaNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP client on STA 1 targets STA 0
    UdpClientHelper udpClient(staInterfaces.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}