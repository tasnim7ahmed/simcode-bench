#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/wifi-phy-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/applications-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Set simulation parameters
    uint32_t nSta = 3;
    double simTime = 10.0;
    uint16_t port = 4000;
    uint32_t packetSize = 512;
    Time interPacketInterval = MilliSeconds(10);

    // Create nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nSta);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // WiFi 802.11g Setup
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // STA configuration
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // AP configuration
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;

    // STA mobility: RandomWalk2d within 50x50 area
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0.0, 50.0, 0.0, 50.0)));
    mobility.Install(wifiStaNodes);

    // AP mobility: static at center
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces, apInterface;
    staInterfaces = address.Assign(staDevices);
    apInterface = address.Assign(apDevice);

    // UDP Server (on AP)
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // UDP Clients (on STAs)
    for (uint32_t i = 0; i < nSta; ++i)
    {
        UdpClientHelper udpClient(apInterface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000000));
        udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        udpClient.SetAttribute("Interval", TimeValue(interPacketInterval));

        ApplicationContainer clientApps = udpClient.Install(wifiStaNodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(simTime));
    }

    // Enable PCAP tracing on AP
    phy.EnablePcap("wifi-ap", apDevice.Get(0));

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}