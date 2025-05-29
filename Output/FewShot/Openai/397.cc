#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);

    // Create nodes: AP (sender) and STA (receiver)
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Wi-Fi PHY and channel helpers
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Wi-Fi MAC configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    WifiMacHelper mac;

    // AP configuration (node 0)
    Ssid ssid = Ssid("ns3-ssid");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiNodes.Get(0));

    // STA configuration (node 1)
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiNodes.Get(1));

    NetDeviceContainer devices;
    devices.Add(apDevice.Get(0));
    devices.Add(staDevice.Get(0));

    // Mobility: both nodes are static
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP
    positionAlloc->Add(Vector(5.0, 0.0, 0.0)); // STA
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP server (receiver) on STA (node 1)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(wifiNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(25.0));

    // UDP client (sender) on AP (node 0) sending to STA
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(wifiNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(25.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}