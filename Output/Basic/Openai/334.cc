#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(2); // wifiNodes.Get(0): STA, wifiNodes.Get(1): AP

    // Wifi PHY and MAC
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");

    NetDeviceContainer devices;

    // STA
    mac.SetType("ns3::StaWifiMac",
        "Ssid", SsidValue(ssid),
        "ActiveProbing", BooleanValue(false));
    devices.Add(wifi.Install(phy, mac, wifiNodes.Get(0)));

    // AP
    mac.SetType("ns3::ApWifiMac",
        "Ssid", SsidValue(ssid));
    devices.Add(wifi.Install(phy, mac, wifiNodes.Get(1)));

    // Mobility
    MobilityHelper mobility;

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // STA: initial position
    positionAlloc->Add(Vector(50.0, 50.0, 0.0)); // AP: center of area

    mobility.SetPositionAllocator(positionAlloc);

    // STA: RandomWalk2dMobilityModel
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)));
    mobility.Install(wifiNodes.Get(0));

    // AP: ConstantPositionMobilityModel
    MobilityHelper apMobility;
    apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    apMobility.Install(wifiNodes.Get(1));

    // Internet
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Server (AP) on port 9
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(wifiNodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(15.0));

    // UDP Client (STA) -> sends 10 packets of 1024 bytes, 1s interval
    UdpClientHelper udpClient(interfaces.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = udpClient.Install(wifiNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(15.0));

    Simulator::Stop(Seconds(16.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}