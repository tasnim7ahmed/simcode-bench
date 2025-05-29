#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    uint32_t numSta = 3;
    double simulationTime = 10.0;

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(numSta);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Wi-Fi channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Wi-Fi MAC and helper
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi-ssid");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;

    // Access Point is stationary
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    // Stations have random walk
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wifiStaNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // UDP Server on the Access Point
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    // UDP Client on STA 0
    UdpClientHelper client(apInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simulationTime));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}