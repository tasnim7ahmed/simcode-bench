#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for UDP applications if desired
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    // Configure Wi-Fi channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set Wi-Fi standard to 802.11b
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    // Configure MAC: use AdhocWifiMac
    WifiMacHelper mac;
    Ssid ssid = Ssid("simple-wifi");
    mac.SetType("ns3::AdhocWifiMac",
        "Ssid", SsidValue(ssid));

    // Setup Wifi devices
    NetDeviceContainer devices = wifi.Install(phy, mac, wifiStaNodes);

    // Install mobility model (constant position)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    // Install the internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install UDP server on Node 1
    uint16_t port = 8000;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiStaNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Install UDP client on Node 0
    uint32_t packetSize = 1024;
    double rate_kbps = 500.0;
    double interval = (packetSize * 8) / (rate_kbps * 1000.0); // seconds/packet

    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(0)); // unlimited
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}