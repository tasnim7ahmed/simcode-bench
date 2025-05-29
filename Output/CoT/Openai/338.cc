#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Simulation parameters
    uint32_t numSta = 2;
    double simTime = 20.0; // seconds

    // Create nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(numSta);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configure PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("ChannelNumber", UintegerValue(42)); // 5 GHz band

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211n");

    // Wifi STA devices
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Wifi AP device
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;

    // STA - RandomWalk2dMobilityModel
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)),
                              "Distance", DoubleValue(10.0),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"));
    mobility.Install(wifiStaNodes);

    // AP - stationary at (50,50)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    Ptr<MobilityModel> apMm = wifiApNode.Get(0)->GetObject<MobilityModel>();
    apMm->SetPosition(Vector(50.0, 50.0, 0.0));

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // UDP server on AP, port 9
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // UDP client on each STA towards AP
    for (uint32_t i = 0; i < numSta; ++i)
    {
        UdpClientHelper udpClient(apInterface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(4294967295u));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simTime));
    }

    // UDP client on AP towards each STA
    for (uint32_t i = 0; i < numSta; ++i)
    {
        UdpClientHelper udpClient(staInterfaces.GetAddress(i), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(4294967295u));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(wifiApNode.Get(0));
        clientApp.Start(Seconds(1.0 + i * 0.1));
        clientApp.Stop(Seconds(simTime));
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}