#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    // Enable logging (optional, useful for debugging)
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("WifiMac", LOG_LEVEL_INFO);

    // Create nodes for AP and STA
    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer staNode;
    staNode.Create(1);

    // Configure Mobility Models (Static Positions)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP position
    positionAlloc->Add(Vector(5.0, 0.0, 0.0)); // STA position (5 meters away)
    mobility.SetPositionAllocator(positionAlloc);

    mobility.Install(apNode);
    mobility.Install(staNode);

    // Configure Wi-Fi devices
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ); // Using 802.11n on 2.4GHz band

    YansWifiPhyHelper phy;
    phy.SetChannel(YansWifiChannelHelper::Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("my-wifi-ssid");

    // Configure AP MAC
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    // Configure STA MAC
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false)); // STA passively associates
    NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNode);

    // Assign IP Addresses (192.168.1.0/24)
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevice);

    // Configure UDP Echo Server on AP
    UdpEchoServerHelper echoServer(9); // Port 9
    ApplicationContainer serverApps = echoServer.Install(apNode.Get(0));
    serverApps.Start(Seconds(1.0));  // Server starts at 1 second
    serverApps.Stop(Seconds(10.0)); // Server stops at 10 seconds

    // Configure UDP Echo Client on STA
    UdpEchoClientHelper echoClient(apInterfaces.GetAddress(0), 9); // Target AP's IP and server port
    echoClient.SetAttribute("MaxPackets", UintegerValue(100)); // Send 100 packets
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1))); // Send every 0.1 seconds
    echoClient.SetAttribute("PacketSize", UintegerValue(1024)); // Packet size 1024 bytes
    ApplicationContainer clientApps = echoClient.Install(staNode.Get(0));
    clientApps.Start(Seconds(2.0)); // Client starts at 2 seconds
    clientApps.Stop(Seconds(9.0));  // Client stops at 9 seconds

    // Set simulation stop time
    Simulator::Stop(Seconds(11.0)); // Run for 11 seconds (slightly after all apps stop)

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}