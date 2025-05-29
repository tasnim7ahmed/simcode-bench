#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: Node 0 = AP, Node 1 = STA
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Configure Wi-Fi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set Wi-Fi standard and helpers
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    // Configure STA device on Node 1
    NetDeviceContainer staDevice;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    staDevice = wifi.Install(phy, mac, wifiNodes.Get(1));

    // Configure AP device on Node 0
    NetDeviceContainer apDevice;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    apDevice = wifi.Install(phy, mac, wifiNodes.Get(0));

    // Mobility: place nodes statically
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // IP address allocation
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    // UDP server (Access Point, Node 0)
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiNodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // UDP client on station (Node 1) sending to AP
    UdpClientHelper client(apInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(10000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.05))); // 20 pkt/s
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(wifiNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}