#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    // 1. Create nodes
    ns3::NodeContainer nodes;
    nodes.Create(2); // One AP, one STA

    ns3::NodeContainer apNode = ns3::NodeContainer(nodes.Get(0));
    ns3::NodeContainer staNode = ns3::NodeContainer(nodes.Get(1));

    // 2. Configure Wi-Fi
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211n); // Set 802.11n standard

    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(ns3::YansWifiChannelHelper::Create());

    ns3::WifiMacHelper mac;
    ns3::Ssid ssid = ns3::Ssid("my-wifi-ssid");

    // AP MAC
    mac.SetType("ns3::ApWifiMac", "Ssid", ns3::SsidValue(ssid));
    ns3::NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    // STA MAC
    mac.SetType("ns3::StaWifiMac", "Ssid", ns3::SsidValue(ssid));
    ns3::NetDeviceContainer staDevices = wifi.Install(phy, mac, staNode);

    // 3. Mobility Model
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(apNode);
    apNode.GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0));

    mobility.Install(staNode);
    staNode.GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(1.0, 0.0, 0.0));

    // 4. Install Internet Stack
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP Addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    ns3::Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    ns3::Ipv4Address apIpAddress = apInterfaces.GetAddress(0);

    // 6. UDP Applications
    // Server (AP)
    uint16_t port = 9; // Echo server port
    ns3::UdpEchoServerHelper echoServer(port);
    ns3::ApplicationContainer serverApps = echoServer.Install(apNode);
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // Client (STA)
    ns3::UdpEchoClientHelper echoClient(apIpAddress, port);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(50));
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::MilliSeconds(200)));
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024));
    ns3::ApplicationContainer clientApps = echoClient.Install(staNode);
    clientApps.Start(ns3::Seconds(1.0)); // Start after server
    clientApps.Stop(ns3::Seconds(10.0));

    // 7. Simulation Time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // 8. Run Simulation
    ns3::Simulator::Run();

    // 9. Cleanup
    ns3::Simulator::Destroy();

    return 0;
}