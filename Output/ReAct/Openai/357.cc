#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 0 - STA, 1 - AP
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Configure Wi-Fi PHY and Channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi (802.11b/g/n/ac all possible, default N)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-wifi");

    // STA device
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiNodes.Get(0));

    // AP device
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiNodes.Get(1));

    // Set mobility: static positions
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));  // STA
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));  // AP
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Install TCP/IP stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IPv4 addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface, apInterface;
    staInterface = address.Assign(staDevice);
    apInterface = address.Assign(apDevice);

    // UDP Echo Server on AP (node 1)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(wifiNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client on STA (node 0)
    UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(wifiNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(9.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}